/**
 * DTLS-in-STUN (SPED, draft-uberti-tsvwg-warp section 4.2) controller
 *
 * Follows libwebrtc's DtlsStunPiggybackController, the implementation browsers interoperate with:
 * the peer supports SPED if its first authenticated check carries either attribute, a new DTLS flight
 * replaces the embedded one, and embedding ends once the local handshake is done and the peer has
 * acknowledged the last flight or sent application data.
 */
#define LOG_CLASS "DtlsInStun"
#include "../Include_i.h"

// MESSAGE-INTEGRITY and FINGERPRINT, which the STUN message gets after the SPED attributes
#define DTLS_IN_STUN_TRAILER_SIZE (STUN_ATTRIBUTE_HEADER_LEN + STUN_HMAC_VALUE_LEN + STUN_ATTRIBUTE_HEADER_LEN + STUN_ATTRIBUTE_FINGERPRINT_LEN)

static VOID clearPackets(PDtlsInStunPacket pPackets, PUINT32 pCount)
{
    UINT32 i;

    for (i = 0; i < *pCount; i++) {
        SAFE_MEMFREE(pPackets[i].pData);
    }
    *pCount = 0;
}

// Caller holds the lock
static VOID resetLocked(PDtlsInStun pDtlsInStun)
{
    clearPackets(pDtlsInStun->stash, &pDtlsInStun->stashCount);
    clearPackets(pDtlsInStun->flight, &pDtlsInStun->flightCount);
    pDtlsInStun->stashNext = 0;
    pDtlsInStun->embedEnded = FALSE;
    pDtlsInStun->ackCount = 0;
    MEMSET(pDtlsInStun->sentRequests, 0x00, SIZEOF(pDtlsInStun->sentRequests));
    pDtlsInStun->sentRequestNext = 0;
}

static BOOL isActiveState(DTLS_IN_STUN_STATE state)
{
    return state == DTLS_IN_STUN_STATE_TENTATIVE || state == DTLS_IN_STUN_STATE_CONFIRMED || state == DTLS_IN_STUN_STATE_PENDING;
}

static VOID changeStateLocked(PDtlsInStun pDtlsInStun, DTLS_IN_STUN_STATE state)
{
    if (pDtlsInStun->state != state) {
        DLOGD("DTLS-in-STUN state %u -> %u", pDtlsInStun->state, state);
        // Here, as completion comes with a STUN message or with the peer's application data
        if (state == DTLS_IN_STUN_STATE_COMPLETE) {
            DLOGI("DTLS-in-STUN complete, %u DTLS packets came embedded", pDtlsInStun->receivedPacketCount);
        }
        pDtlsInStun->state = state;
    }
    if (!isActiveState(state)) {
        resetLocked(pDtlsInStun);
    }
}

// Caller holds the lock
static VOID recordAckLocked(PDtlsInStun pDtlsInStun, UINT32 crc)
{
    UINT32 i;

    for (i = 0; i < pDtlsInStun->ackCount; i++) {
        if (pDtlsInStun->acks[i] == crc) {
            return;
        }
    }
    // Keep the newest ones
    if (pDtlsInStun->ackCount == STUN_DTLS_IN_STUN_ACK_MAX_SEND) {
        MEMMOVE(pDtlsInStun->acks, pDtlsInStun->acks + 1, (STUN_DTLS_IN_STUN_ACK_MAX_SEND - 1) * SIZEOF(UINT32));
        pDtlsInStun->ackCount--;
    }
    pDtlsInStun->acks[pDtlsInStun->ackCount++] = crc;
}

// Caller holds the lock
static VOID pruneStashLocked(PDtlsInStun pDtlsInStun, PUINT32 pAcks, UINT32 ackCount)
{
    UINT32 i, j, kept = 0, removedBeforeNext = 0;
    BOOL acked;

    for (i = 0; i < pDtlsInStun->stashCount; i++) {
        acked = FALSE;
        for (j = 0; j < ackCount && !acked; j++) {
            acked = pDtlsInStun->stash[i].crc == pAcks[j];
        }
        if (acked) {
            SAFE_MEMFREE(pDtlsInStun->stash[i].pData);
            if (i < pDtlsInStun->stashNext) {
                removedBeforeNext++;
            }
        } else {
            pDtlsInStun->stash[kept++] = pDtlsInStun->stash[i];
        }
    }
    pDtlsInStun->stashCount = kept;
    // Keep pointing at the same packet in the round-robin
    pDtlsInStun->stashNext -= removedBeforeNext;
    if (pDtlsInStun->stashNext >= pDtlsInStun->stashCount) {
        pDtlsInStun->stashNext = 0;
    }
}

// Caller holds the lock
static VOID commitFlightLocked(PDtlsInStun pDtlsInStun)
{
    if (pDtlsInStun->flightCount == 0) {
        return;
    }
    clearPackets(pDtlsInStun->stash, &pDtlsInStun->stashCount);
    MEMCPY(pDtlsInStun->stash, pDtlsInStun->flight, pDtlsInStun->flightCount * SIZEOF(DtlsInStunPacket));
    pDtlsInStun->stashCount = pDtlsInStun->flightCount;
    pDtlsInStun->stashNext = 0;
    MEMSET(pDtlsInStun->flight, 0x00, SIZEOF(pDtlsInStun->flight));
    pDtlsInStun->flightCount = 0;
}

static PDtlsInStunSentRequest findSentRequestLocked(PDtlsInStun pDtlsInStun, PBYTE transactionId)
{
    UINT32 i;

    for (i = 0; i < DTLS_IN_STUN_MAX_SENT_REQUESTS; i++) {
        if (MEMCMP(pDtlsInStun->sentRequests[i].transactionId, transactionId, STUN_TRANSACTION_ID_LEN) == 0) {
            return &pDtlsInStun->sentRequests[i];
        }
    }
    return NULL;
}

STATUS createDtlsInStun(PDtlsInStun* ppDtlsInStun)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsInStun pDtlsInStun = NULL;

    CHK(ppDtlsInStun != NULL, STATUS_NULL_ARG);
    CHK(NULL != (pDtlsInStun = (PDtlsInStun) MEMCALLOC(1, SIZEOF(DtlsInStun))), STATUS_NOT_ENOUGH_MEMORY);
    pDtlsInStun->lock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pDtlsInStun->lock), STATUS_INVALID_OPERATION);
    pDtlsInStun->state = DTLS_IN_STUN_STATE_OFF;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeDtlsInStun(&pDtlsInStun);
    }
    if (ppDtlsInStun != NULL) {
        *ppDtlsInStun = pDtlsInStun;
    }

    LEAVES();
    return retStatus;
}

STATUS freeDtlsInStun(PDtlsInStun* ppDtlsInStun)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDtlsInStun pDtlsInStun;

    CHK(ppDtlsInStun != NULL, STATUS_NULL_ARG);
    pDtlsInStun = *ppDtlsInStun;
    CHK(pDtlsInStun != NULL, retStatus);

    resetLocked(pDtlsInStun);
    if (IS_VALID_MUTEX_VALUE(pDtlsInStun->lock)) {
        MUTEX_FREE(pDtlsInStun->lock);
    }
    SAFE_MEMFREE(*ppDtlsInStun);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS dtlsInStunStart(PDtlsInStun pDtlsInStun)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    resetLocked(pDtlsInStun);
    pDtlsInStun->dtlsStarted = FALSE;
    changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_TENTATIVE);
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunStop(PDtlsInStun pDtlsInStun)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_OFF);
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

DTLS_IN_STUN_STATE dtlsInStunGetState(PDtlsInStun pDtlsInStun)
{
    DTLS_IN_STUN_STATE state;

    if (pDtlsInStun == NULL) {
        return DTLS_IN_STUN_STATE_OFF;
    }
    MUTEX_LOCK(pDtlsInStun->lock);
    state = pDtlsInStun->state;
    MUTEX_UNLOCK(pDtlsInStun->lock);
    return state;
}

BOOL dtlsInStunIsActive(PDtlsInStun pDtlsInStun)
{
    return isActiveState(dtlsInStunGetState(pDtlsInStun));
}

STATUS dtlsInStunSetDtlsStarted(PDtlsInStun pDtlsInStun)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    pDtlsInStun->dtlsStarted = TRUE;
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

BOOL dtlsInStunCapturePacket(PDtlsInStun pDtlsInStun, PBYTE pData, UINT32 length)
{
    BOOL captured = FALSE;
    PDtlsInStunPacket pPacket;

    if (pDtlsInStun == NULL || pData == NULL || !IS_DTLS_IN_STUN_PACKET(pData, length) || length > DTLS_IN_STUN_MAX_PACKET_SIZE) {
        return FALSE;
    }

    MUTEX_LOCK(pDtlsInStun->lock);
    if (isActiveState(pDtlsInStun->state) && !pDtlsInStun->embedEnded && pDtlsInStun->flightCount < DTLS_IN_STUN_MAX_STASHED_PACKETS) {
        pPacket = &pDtlsInStun->flight[pDtlsInStun->flightCount];
        if (NULL != (pPacket->pData = (PBYTE) MEMALLOC(length))) {
            MEMCPY(pPacket->pData, pData, length);
            pPacket->length = length;
            pPacket->crc = COMPUTE_CRC32(pData, length);
            pDtlsInStun->flightCount++;
            captured = TRUE;
        }
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

    return captured;
}

STATUS dtlsInStunEndFlight(PDtlsInStun pDtlsInStun, BOOL retransmission)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    if (retransmission) {
        clearPackets(pDtlsInStun->flight, &pDtlsInStun->flightCount);
    } else {
        commitFlightLocked(pDtlsInStun);
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunAppendAttributes(PDtlsInStun pDtlsInStun, PStunPacket pStunPacket, UINT32 maxSize, BOOL embed)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, embedded = FALSE;
    UINT32 size, crc = 0;
    PDtlsInStunPacket pPacket;
    PDtlsInStunSentRequest pSentRequest;

    CHK(pDtlsInStun != NULL && pStunPacket != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    locked = TRUE;
    CHK(isActiveState(pDtlsInStun->state), retStatus);

    size = STUN_HEADER_LEN + pStunPacket->header.messageLength + DTLS_IN_STUN_TRAILER_SIZE;

    // The ACKs go first, all of them if they fit. Not before the DTLS session can take packets: in a response the ACK
    // attribute also acknowledges what the request embedded, which was dropped then.
    if (pDtlsInStun->dtlsStarted) {
        if (size + STUN_ATTRIBUTE_HEADER_LEN + pDtlsInStun->ackCount * SIZEOF(UINT32) <= maxSize) {
            CHK_STATUS(appendStunDtlsInStunAckAttribute(pStunPacket, pDtlsInStun->acks, pDtlsInStun->ackCount));
        } else {
            CHK(size + STUN_ATTRIBUTE_HEADER_LEN <= maxSize, retStatus);
            CHK_STATUS(appendStunDtlsInStunAckAttribute(pStunPacket, NULL, 0));
        }
        size = STUN_HEADER_LEN + pStunPacket->header.messageLength + DTLS_IN_STUN_TRAILER_SIZE;
    }

    if (embed && !pDtlsInStun->embedEnded && pDtlsInStun->stashCount > 0) {
        pPacket = &pDtlsInStun->stash[pDtlsInStun->stashNext];
        if (size + STUN_ATTRIBUTE_HEADER_LEN + ROUND_UP(pPacket->length, 4) <= maxSize) {
            CHK_STATUS(appendStunDtlsInStunAttribute(pStunPacket, pPacket->pData, (UINT16) pPacket->length));
            embedded = TRUE;
            crc = pPacket->crc;
            pDtlsInStun->stashNext = (pDtlsInStun->stashNext + 1) % pDtlsInStun->stashCount;
        }
    }
    // Nothing embedded: an empty one announces SPED to peers that look for this attribute, and is the only announcement
    // while there is no ACK attribute
    if (!embedded && !pDtlsInStun->embedEnded &&
        ((pDtlsInStun->state == DTLS_IN_STUN_STATE_CONFIRMED && pDtlsInStun->stashCount == 0) || !pDtlsInStun->dtlsStarted) &&
        size + STUN_ATTRIBUTE_HEADER_LEN <= maxSize) {
        CHK_STATUS(appendStunDtlsInStunAttribute(pStunPacket, NULL, 0));
    }

    // A response to this request acknowledges what it embedded
    if (pStunPacket->header.stunMessageType == STUN_PACKET_TYPE_BINDING_REQUEST) {
        pSentRequest = &pDtlsInStun->sentRequests[pDtlsInStun->sentRequestNext];
        pDtlsInStun->sentRequestNext = (pDtlsInStun->sentRequestNext + 1) % DTLS_IN_STUN_MAX_SENT_REQUESTS;
        MEMCPY(pSentRequest->transactionId, pStunPacket->header.transactionId, STUN_TRANSACTION_ID_LEN);
        pSentRequest->embedded = embedded;
        pSentRequest->crc = crc;
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pDtlsInStun->lock);
    }

    return retStatus;
}

STATUS dtlsInStunOnStunMessage(PDtlsInStun pDtlsInStun, PStunPacket pStunPacket, PBYTE pDtlsPacket, PUINT32 pDtlsPacketLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, isResponse;
    PStunAttributeHeader pDataAttribute = NULL, pAckAttribute = NULL;
    PStunAttributeData pData;
    PDtlsInStunSentRequest pSentRequest = NULL;
    UINT32 acks[STUN_DTLS_IN_STUN_ACK_MAX_RECEIVE + 1], ackCount = 0;

    CHK(pDtlsInStun != NULL && pStunPacket != NULL && pDtlsPacket != NULL && pDtlsPacketLen != NULL, STATUS_NULL_ARG);
    *pDtlsPacketLen = 0;
    isResponse = pStunPacket->header.stunMessageType == STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS;

    MUTEX_LOCK(pDtlsInStun->lock);
    locked = TRUE;
    CHK(isActiveState(pDtlsInStun->state), retStatus);

    // NULL also for a request sentRequests no longer holds: its response still carries the peer's packet and ACKs
    if (isResponse) {
        pSentRequest = findSentRequestLocked(pDtlsInStun, pStunPacket->header.transactionId);
    }

    CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN, &pDataAttribute));
    CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_DTLS_IN_STUN_ACK, &pAckAttribute));
    // A malformed ACK attribute is not an absent one, which would mean a peer without SPED: ignore the message
    if (pAckAttribute != NULL && STATUS_FAILED(getStunDtlsInStunAcks(pAckAttribute, acks, &ackCount))) {
        DLOGW("Ignoring the DTLS-in-STUN attributes of a message with a malformed ACK attribute");
        CHK(FALSE, retStatus);
    }

    if (pDtlsInStun->state == DTLS_IN_STUN_STATE_TENTATIVE) {
        if (pDataAttribute == NULL && pAckAttribute == NULL) {
            // Only a response to a request known to carry the attributes shows the peer does not embed
            CHK(!isResponse || pSentRequest != NULL, retStatus);
            DLOGI("Peer does not embed DTLS in STUN, falling back to plain DTLS");
            // The stashed flight stays, to go out plain once ICE has a pair
            pDtlsInStun->state = DTLS_IN_STUN_STATE_OFF;
            clearPackets(pDtlsInStun->flight, &pDtlsInStun->flightCount);
            pDtlsInStun->ackCount = 0;
            CHK(FALSE, retStatus);
        }
        changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_CONFIRMED);
    }

    if (pAckAttribute != NULL) {
        if (pSentRequest != NULL && pSentRequest->embedded) {
            acks[ackCount++] = pSentRequest->crc;
        }
        pruneStashLocked(pDtlsInStun, acks, ackCount);
    }

    if (pDataAttribute != NULL && pDataAttribute->length > 0) {
        pData = (PStunAttributeData) pDataAttribute;
        if (!IS_DTLS_IN_STUN_PACKET(pData->data, pDataAttribute->length) || pDataAttribute->length > DTLS_IN_STUN_MAX_PACKET_SIZE) {
            DLOGW("Dropping embedded data that is not a DTLS packet");
        } else if (pDtlsInStun->dtlsStarted) {
            // Before the DTLS session runs it could not take the packet, so the peer must keep sending it
            recordAckLocked(pDtlsInStun, COMPUTE_CRC32(pData->data, pDataAttribute->length));
            MEMCPY(pDtlsPacket, pData->data, pDataAttribute->length);
            *pDtlsPacketLen = pDataAttribute->length;
            pDtlsInStun->receivedPacketCount++;
        }
    }

    if (pDtlsInStun->state == DTLS_IN_STUN_STATE_PENDING && pDtlsInStun->stashCount == 0) {
        changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_COMPLETE);
    }

CleanUp:

    if (pSentRequest != NULL) {
        MEMSET(pSentRequest, 0x00, SIZEOF(DtlsInStunSentRequest));
    }
    if (locked) {
        MUTEX_UNLOCK(pDtlsInStun->lock);
    }

    return retStatus;
}

STATUS dtlsInStunOnDtlsPacket(PDtlsInStun pDtlsInStun, PBYTE pData, UINT32 length)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL && pData != NULL, STATUS_NULL_ARG);
    CHK(IS_DTLS_IN_STUN_PACKET(pData, length), retStatus);
    MUTEX_LOCK(pDtlsInStun->lock);
    if (isActiveState(pDtlsInStun->state) && pDtlsInStun->dtlsStarted) {
        recordAckLocked(pDtlsInStun, COMPUTE_CRC32(pData, length));
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunOnHandshakeComplete(PDtlsInStun pDtlsInStun, BOOL isClient)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    if (isActiveState(pDtlsInStun->state)) {
        // The last flight is not followed by a retransmission timer, so nothing ended it yet
        commitFlightLocked(pDtlsInStun);
        if (isClient) {
            clearPackets(pDtlsInStun->stash, &pDtlsInStun->stashCount);
            pDtlsInStun->stashNext = 0;
        }
        changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_PENDING);
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunOnApplicationData(PDtlsInStun pDtlsInStun)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    if (pDtlsInStun->state == DTLS_IN_STUN_STATE_PENDING) {
        changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_COMPLETE);
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunOnDtlsFailed(PDtlsInStun pDtlsInStun)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    if (isActiveState(pDtlsInStun->state)) {
        DLOGI("DTLS failed, no more DTLS-in-STUN");
        changeStateLocked(pDtlsInStun, DTLS_IN_STUN_STATE_OFF);
    }
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}

STATUS dtlsInStunTakeStash(PDtlsInStun pDtlsInStun, PDtlsInStunPacket pPackets, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDtlsInStun != NULL && pPackets != NULL && pCount != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pDtlsInStun->lock);
    MEMCPY(pPackets, pDtlsInStun->stash, pDtlsInStun->stashCount * SIZEOF(DtlsInStunPacket));
    *pCount = pDtlsInStun->stashCount;
    MEMSET(pDtlsInStun->stash, 0x00, SIZEOF(pDtlsInStun->stash));
    pDtlsInStun->stashCount = 0;
    pDtlsInStun->stashNext = 0;
    // A flight the DTLS session is still writing goes out plain itself
    clearPackets(pDtlsInStun->flight, &pDtlsInStun->flightCount);
    pDtlsInStun->embedEnded = TRUE;
    MUTEX_UNLOCK(pDtlsInStun->lock);

CleanUp:
    return retStatus;
}
