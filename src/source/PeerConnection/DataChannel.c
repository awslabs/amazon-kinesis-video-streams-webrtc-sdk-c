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

    CHK_STATUS(sctpSessionWriteMessage(pSctpSession, pKvsDataChannel->channelId, isBinary, pMessage, pMessageLen));
    pKvsDataChannel->rtcDataChannelDiagnostics.messagesSent++;
    pKvsDataChannel->rtcDataChannelDiagnostics.bytesSent += pMessageLen;
CleanUp:

    return retStatus;
}

STATUS dataChannelOnMessage(PRtcDataChannel pRtcDataChannel, UINT64 customData, RtcOnMessage rtcOnMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PKvsDataChannel pKvsDataChannel = (PKvsDataChannel) pRtcDataChannel;

    CHK(pKvsDataChannel != NULL && rtcOnMessage != NULL, STATUS_NULL_ARG);

    pKvsDataChannel->onMessage = rtcOnMessage;
    pKvsDataChannel->onMessageCustomData = customData;

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
