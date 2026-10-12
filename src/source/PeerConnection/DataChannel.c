#define LOG_CLASS "DataChannel"

#include "../Include_i.h"

STATUS connectLocalDataChannel()
{
    return STATUS_SUCCESS;
}

typedef struct {
    UINT16 id;
    BOOL taken;
} NegotiatedIdLookup;

// Is a negotiated id already claimed by a channel created before the association?
static STATUS findNegotiatedIdCallback(UINT64 customData, PHashEntry pHashEntry)
{
    NegotiatedIdLookup* pLookup = (NegotiatedIdLookup*) customData;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pHashEntry->value;

    if (pKvsDataChannel != NULL && pKvsDataChannel->rtcDataChannelInit.negotiated && pKvsDataChannel->rtcDataChannelInit.id.value == pLookup->id) {
        pLookup->taken = TRUE;
    }
    return STATUS_SUCCESS;
}

STATUS createDataChannel(PRtcPeerConnection pPeerConnection, PCHAR pDataChannelName, PRtcDataChannelInit pRtcDataChannelInit,
                         PRtcDataChannel* ppRtcDataChannel)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) pPeerConnection;
    UINT32 channelId = 0;
    BOOL negotiated = FALSE, exists = FALSE, locked = FALSE;
    PKvsDataChannel pKvsDataChannel = NULL;
    NegotiatedIdLookup lookup;

    CHK(pKvsPeerConnection != NULL && pDataChannelName != NULL && ppRtcDataChannel != NULL, STATUS_NULL_ARG);

    negotiated = (pRtcDataChannelInit != NULL && pRtcDataChannelInit->negotiated);
    // A negotiated channel (RFC 8832 section 5, "externally negotiated") carries no DCEP and needs an explicit stream id
    CHK(!negotiated || !NULLABLE_CHECK_EMPTY(pRtcDataChannelInit->id), STATUS_INVALID_ARG);
    // ...within the streams the association negotiates
    CHK(!negotiated || pRtcDataChannelInit->id.value < SCTP_MAX_STREAMS, STATUS_INVALID_ARG);

    // The SCTP thread uses the table (and allocateSctp swaps it) while the application creates channels
    MUTEX_LOCK(pKvsPeerConnection->dataChannelsLock);
    locked = TRUE;

    // In-band (DCEP) channels can only be created before the SCTP association exists, because the stream ids are
    // assigned and the DATA_CHANNEL_OPEN messages sent when the association is set up. Negotiated channels bring
    // their own id and may be created at any time.
    CHK(pKvsPeerConnection->pSctpSession == NULL || negotiated, STATUS_INVALID_OPERATION);

    CHK((pKvsDataChannel = (PKvsDataChannel) MEMCALLOC(1, SIZEOF(KvsDataChannel))) != NULL, STATUS_NOT_ENOUGH_MEMORY);
    STRNCPY(pKvsDataChannel->dataChannel.name, pDataChannelName, MAX_DATA_CHANNEL_NAME_LEN);
    pKvsDataChannel->pRtcPeerConnection = (PRtcPeerConnection) pKvsPeerConnection;
    if (pRtcDataChannelInit != NULL) {
        pKvsDataChannel->rtcDataChannelInit = *pRtcDataChannelInit;
    } else {
        // If nothing is set, set default to ordered mode
        pKvsDataChannel->rtcDataChannelInit.ordered = TRUE;
        NULLABLE_SET_EMPTY(pKvsDataChannel->rtcDataChannelInit.maxPacketLifeTime);
        NULLABLE_SET_EMPTY(pKvsDataChannel->rtcDataChannelInit.maxRetransmits);
        NULLABLE_SET_EMPTY(pKvsDataChannel->rtcDataChannelInit.id);
    }
    STRNCPY(pKvsDataChannel->rtcDataChannelDiagnostics.label, pKvsDataChannel->dataChannel.name, STRLEN(pKvsDataChannel->dataChannel.name));
    STRNCPY(pKvsDataChannel->rtcDataChannelDiagnostics.protocol, DATA_CHANNEL_PROTOCOL_STR,
            ARRAY_SIZE(pKvsDataChannel->rtcDataChannelDiagnostics.protocol));

    if (pKvsPeerConnection->pSctpSession != NULL) {
        // Negotiated channel on an established association: the stream is usable right away
        channelId = pRtcDataChannelInit->id.value;
        CHK_STATUS(hashTableContains(pKvsPeerConnection->pDataChannels, channelId, &exists));
        CHK(!exists, STATUS_INVALID_ARG);
        pKvsDataChannel->channelId = channelId;
        pKvsDataChannel->rtcDataChannelDiagnostics.state = RTC_DATA_CHANNEL_STATE_OPEN;
    } else {
        if (negotiated) {
            lookup.id = pRtcDataChannelInit->id.value;
            lookup.taken = FALSE;
            CHK_STATUS(hashTableIterateEntries(pKvsPeerConnection->pDataChannels, (UINT64) &lookup, findNegotiatedIdCallback));
            CHK(!lookup.taken, STATUS_INVALID_ARG);
        }
        // Before the association exists the table is keyed by creation order; allocateSctp re-keys it by stream id
        CHK_STATUS(hashTableGetCount(pKvsPeerConnection->pDataChannels, &channelId));
        pKvsDataChannel->rtcDataChannelDiagnostics.state = RTC_DATA_CHANNEL_STATE_CONNECTING;
    }
    pKvsDataChannel->rtcDataChannelDiagnostics.dataChannelIdentifier = channelId;
    pKvsDataChannel->dataChannel.id = channelId;
    CHK_STATUS(hashTablePut(pKvsPeerConnection->pDataChannels, channelId, (UINT64) pKvsDataChannel));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->dataChannelsLock);
    }
    if (STATUS_SUCCEEDED(retStatus)) {
        *ppRtcDataChannel = (PRtcDataChannel) pKvsDataChannel;
    } else {
        SAFE_MEMFREE(pKvsDataChannel);
    }

    LEAVES();
    return retStatus;
}

STATUS dataChannelSend(PRtcDataChannel pRtcDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSctpSession pSctpSession = NULL;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pRtcDataChannel;

    CHK(pKvsDataChannel != NULL && pMessage != NULL, STATUS_NULL_ARG);

    pSctpSession = ((PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection)->pSctpSession;

    CHK_STATUS(
        sctpSessionWriteMessage(pSctpSession, pKvsDataChannel->channelId, isBinary, pMessage, pMessageLen, &pKvsDataChannel->rtcDataChannelInit));
    pKvsDataChannel->rtcDataChannelDiagnostics.messagesSent++;
    pKvsDataChannel->rtcDataChannelDiagnostics.bytesSent += pMessageLen;
CleanUp:

    return retStatus;
}

// Hold a message while the channel has no onMessage, or while held messages are still being delivered,
// so that messages reach the application in the order they arrived. Caller holds dataChannelsLock.
BOOL dataChannelQueueMessageIfPending(PKvsDataChannel pKvsDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    PDataChannelPendingMessage pPending = NULL;

    if (pKvsDataChannel->onMessage != NULL && !pKvsDataChannel->deliveringPending && pKvsDataChannel->pPendingHead == NULL) {
        return FALSE;
    }

    if (pKvsDataChannel->pendingCount >= DATA_CHANNEL_MAX_PENDING_MESSAGES ||
        pKvsDataChannel->pendingBytes + pMessageLen > DATA_CHANNEL_MAX_PENDING_BYTES ||
        (pPending = (PDataChannelPendingMessage) MEMALLOC(SIZEOF(DataChannelPendingMessage) + pMessageLen)) == NULL) {
        DLOGW("Data channel %" PRIu32 " has no onMessage callback yet and its queue is full; message dropped", pKvsDataChannel->channelId);
        return TRUE;
    }

    pPending->pNext = NULL;
    pPending->isBinary = isBinary;
    pPending->length = pMessageLen;
    MEMCPY((PBYTE) (pPending + 1), pMessage, pMessageLen);
    if (pKvsDataChannel->pPendingTail == NULL) {
        pKvsDataChannel->pPendingHead = pPending;
    } else {
        pKvsDataChannel->pPendingTail->pNext = pPending;
    }
    pKvsDataChannel->pPendingTail = pPending;
    pKvsDataChannel->pendingCount++;
    pKvsDataChannel->pendingBytes += pMessageLen;

    return TRUE;
}

// Caller holds dataChannelsLock
VOID dataChannelFreePendingMessages(PKvsDataChannel pKvsDataChannel)
{
    PDataChannelPendingMessage pPending = pKvsDataChannel->pPendingHead, pNext = NULL;

    while (pPending != NULL) {
        pNext = pPending->pNext;
        MEMFREE(pPending);
        pPending = pNext;
    }
    pKvsDataChannel->pPendingHead = NULL;
    pKvsDataChannel->pPendingTail = NULL;
    pKvsDataChannel->pendingCount = 0;
    pKvsDataChannel->pendingBytes = 0;
}

STATUS freeDataChannelHashEntry(UINT64 customData, PHashEntry pHashEntry)
{
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pHashEntry->value;

    UNUSED_PARAM(customData);
    if (pKvsDataChannel != NULL) {
        dataChannelFreePendingMessages(pKvsDataChannel);
        MEMFREE(pKvsDataChannel);
    }

    return STATUS_SUCCESS;
}

// Hand held messages to onMessage, oldest first, outside the lock. Only one caller delivers at a time
// (deliveringPending), and messages arriving meanwhile queue behind the ones being delivered.
static VOID dataChannelDeliverPendingMessages(PKvsPeerConnection pKvsPeerConnection, PKvsDataChannel pKvsDataChannel)
{
    PDataChannelPendingMessage pPending = NULL;
    RtcOnMessage onMessage = NULL;
    UINT64 onMessageCustomData = 0;

    for (;;) {
        MUTEX_LOCK(pKvsPeerConnection->dataChannelsLock);
        pPending = pKvsDataChannel->pPendingHead;
        if (pPending == NULL) {
            pKvsDataChannel->deliveringPending = FALSE;
        } else {
            pKvsDataChannel->pPendingHead = pPending->pNext;
            if (pKvsDataChannel->pPendingHead == NULL) {
                pKvsDataChannel->pPendingTail = NULL;
            }
            pKvsDataChannel->pendingCount--;
            pKvsDataChannel->pendingBytes -= pPending->length;
        }
        onMessage = pKvsDataChannel->onMessage;
        onMessageCustomData = pKvsDataChannel->onMessageCustomData;
        MUTEX_UNLOCK(pKvsPeerConnection->dataChannelsLock);

        if (pPending == NULL) {
            break;
        }
        onMessage(onMessageCustomData, &pKvsDataChannel->dataChannel, pPending->isBinary, (PBYTE) (pPending + 1), pPending->length);
        MEMFREE(pPending);
    }
}

STATUS dataChannelOnMessage(PRtcDataChannel pRtcDataChannel, UINT64 customData, RtcOnMessage rtcOnMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pRtcDataChannel;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    BOOL deliver = FALSE;

    CHK(pKvsDataChannel != NULL && rtcOnMessage != NULL, STATUS_NULL_ARG);
    pKvsPeerConnection = (PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection;

    // The SCTP thread reads the callback and its custom data together, under the same lock
    MUTEX_LOCK(pKvsPeerConnection->dataChannelsLock);
    pKvsDataChannel->onMessage = rtcOnMessage;
    pKvsDataChannel->onMessageCustomData = customData;
    deliver = pKvsDataChannel->pPendingHead != NULL && !pKvsDataChannel->deliveringPending;
    if (deliver) {
        pKvsDataChannel->deliveringPending = TRUE;
    }
    MUTEX_UNLOCK(pKvsPeerConnection->dataChannelsLock);

    // Messages that arrived before the callback was set are delivered now, from this thread
    if (deliver) {
        dataChannelDeliverPendingMessages(pKvsPeerConnection, pKvsDataChannel);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dataChannelOnOpen(PRtcDataChannel pRtcDataChannel, UINT64 customData, RtcOnOpen rtcOnOpen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pRtcDataChannel;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    BOOL fire = FALSE;

    CHK(pKvsDataChannel != NULL && rtcOnOpen != NULL, STATUS_NULL_ARG);
    pKvsPeerConnection = (PKvsPeerConnection) pKvsDataChannel->pRtcPeerConnection;

    MUTEX_LOCK(pKvsPeerConnection->dataChannelsLock);
    pKvsDataChannel->onOpen = rtcOnOpen;
    pKvsDataChannel->onOpenCustomData = customData;
    // A channel that is already open (a negotiated one created on an established association, or a
    // remote one) gets its open event now: the callback is registered after the channel exists.
    fire = pKvsDataChannel->rtcDataChannelDiagnostics.state == RTC_DATA_CHANNEL_STATE_OPEN && !pKvsDataChannel->openFired;
    if (fire) {
        pKvsDataChannel->openFired = TRUE;
    }
    MUTEX_UNLOCK(pKvsPeerConnection->dataChannelsLock);
    if (fire) {
        rtcOnOpen(customData, &pKvsDataChannel->dataChannel);
    }

CleanUp:

    LEAVES();
    return retStatus;
}
