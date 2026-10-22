/**
 * Packet pacer: spreads outgoing video RTP packets over time instead of sending all of a frame's packets
 * back-to-back, so a keyframe doesn't overflow small uplink buffers or look like congestion to TWCC.
 */
#define LOG_CLASS "Pacer"
#include "../Include_i.h"

STATUS createPacer(PPacerConfig pConfig, PacerSendPacketFunc sendPacketFn, PacerPacketSentFunc packetSentFn, UINT64 customData, PPacer* ppPacer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PPacer pPacer = NULL;

    CHK(pConfig != NULL && sendPacketFn != NULL && ppPacer != NULL, STATUS_NULL_ARG);
    CHK(pConfig->pacingFactor > 0 && pConfig->maxQueueDelay > 0 && pConfig->maxQueuePackets > 0, STATUS_INVALID_ARG);

    pPacer = (PPacer) MEMCALLOC(1, SIZEOF(Pacer));
    CHK(pPacer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pPacer->lock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pPacer->lock), STATUS_INVALID_OPERATION);

    pPacer->config = *pConfig;
    pPacer->sendPacketFn = sendPacketFn;
    pPacer->packetSentFn = packetSentFn;
    pPacer->customData = customData;
    pPacer->pacingRateBps = pConfig->minPacingRateBps;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        freePacer(&pPacer);
    }

    if (ppPacer != NULL) {
        *ppPacer = pPacer;
    }

    LEAVES();
    return retStatus;
}

STATUS freePacer(PPacer* ppPacer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PPacer pPacer = NULL;
    PPacerPacket pCur = NULL, pNext = NULL;

    CHK(ppPacer != NULL, STATUS_NULL_ARG);
    pPacer = *ppPacer;
    CHK(pPacer != NULL, retStatus);

    if (pPacer->queuedPackets > 0) {
        DLOGD("Dropping %u packets still queued in the pacer", pPacer->queuedPackets);
    }

    for (pCur = pPacer->pHead; pCur != NULL; pCur = pNext) {
        pNext = pCur->pNext;
        SAFE_MEMFREE(pCur);
    }

    if (IS_VALID_MUTEX_VALUE(pPacer->lock)) {
        MUTEX_FREE(pPacer->lock);
    }

    SAFE_MEMFREE(*ppPacer);

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS pacerEnqueuePacket(PPacer pPacer, PBYTE pPacket, UINT32 packetLen, BOOL hasTwcc, UINT32 twccExtPayload, UINT32 payloadLength, UINT64 now)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacerPacket pEntry = NULL;
    BOOL locked = FALSE;

    CHK(pPacer != NULL && pPacket != NULL, STATUS_NULL_ARG);
    CHK(packetLen > 0, STATUS_INVALID_ARG);

    // Copy outside the lock so the timer thread isn't held up
    pEntry = (PPacerPacket) MEMALLOC(SIZEOF(PacerPacket) + packetLen);
    CHK(pEntry != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pEntry->pNext = NULL;
    pEntry->enqueueTime = now;
    pEntry->hasTwcc = hasTwcc;
    pEntry->twccExtPayload = twccExtPayload;
    pEntry->payloadLength = payloadLength;
    pEntry->packetLen = packetLen;
    MEMCPY((PBYTE) (pEntry + 1), pPacket, packetLen);

    MUTEX_LOCK(pPacer->lock);
    locked = TRUE;

    CHK_WARN(pPacer->queuedPackets < pPacer->config.maxQueuePackets, STATUS_PEERCONNECTION_PACER_QUEUE_FULL,
             "Pacer queue is full (%u packets), dropping packet", pPacer->queuedPackets);

    if (pPacer->pTail == NULL) {
        pPacer->pHead = pEntry;
    } else {
        pPacer->pTail->pNext = pEntry;
    }
    pPacer->pTail = pEntry;
    pPacer->queuedPackets++;
    pPacer->queuedBytes += packetLen;
    pEntry = NULL;

    if (!pPacer->rateWindowStarted) {
        pPacer->rateWindowStarted = TRUE;
        pPacer->rateWindowStart = now;
    }
    pPacer->rateWindowBytes += packetLen;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pPacer->lock);
    }

    SAFE_MEMFREE(pEntry);
    return retStatus;
}

STATUS pacerProcess(PPacer pPacer, UINT64 now)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacerPacket pSendHead = NULL, pSendTail = NULL, pCur = NULL, pNext = NULL;
    UINT64 elapsed, windowDuration, windowRateBps, oldestAge, timeLeft, requiredRateBps, pacingRateBps;
    INT64 maxBudgetBytes;
    BOOL locked = FALSE;
    STATUS sendStatus;

    CHK(pPacer != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pPacer->lock);
    locked = TRUE;

    if (!pPacer->processed) {
        elapsed = PACER_PROCESS_INTERVAL;
        pPacer->processed = TRUE;
    } else {
        elapsed = now > pPacer->lastProcessTime ? now - pPacer->lastProcessTime : 0;
    }
    pPacer->lastProcessTime = now;

    // Track the rate the encoder produces, smoothed over about a second
    if (pPacer->rateWindowStarted && now >= pPacer->rateWindowStart + PACER_RATE_WINDOW) {
        windowDuration = now - pPacer->rateWindowStart;
        windowRateBps = pPacer->rateWindowBytes * 8 * HUNDREDS_OF_NANOS_IN_A_SECOND / windowDuration;
        if (pPacer->mediaRateKnown) {
            pPacer->mediaRateBps =
                (UINT64) (PACER_RATE_SMOOTHING_FACTOR * windowRateBps + (1.0 - PACER_RATE_SMOOTHING_FACTOR) * pPacer->mediaRateBps);
        } else {
            pPacer->mediaRateBps = windowRateBps;
            pPacer->mediaRateKnown = TRUE;
        }
        pPacer->rateWindowStart = now;
        pPacer->rateWindowBytes = 0;
    }

    pacingRateBps = MAX((UINT64) (pPacer->config.pacingFactor * pPacer->mediaRateBps), pPacer->config.minPacingRateBps);

    // Speed up so the queue drains before its oldest packet has waited maxQueueDelay
    if (pPacer->pHead != NULL) {
        oldestAge = now > pPacer->pHead->enqueueTime ? now - pPacer->pHead->enqueueTime : 0;
        timeLeft = oldestAge < pPacer->config.maxQueueDelay ? pPacer->config.maxQueueDelay - oldestAge : 0;
        timeLeft = MAX(timeLeft, PACER_PROCESS_INTERVAL);
        requiredRateBps = pPacer->queuedBytes * 8 * HUNDREDS_OF_NANOS_IN_A_SECOND / timeLeft;
        pacingRateBps = MAX(pacingRateBps, requiredRateBps);
    }
    pPacer->pacingRateBps = pacingRateBps;

    pPacer->budgetBytes += (INT64) (pacingRateBps * elapsed / (8 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    maxBudgetBytes = (INT64) (pacingRateBps * PACER_MAX_BURST_TIME / (8 * HUNDREDS_OF_NANOS_IN_A_SECOND));
    pPacer->budgetBytes = MIN(pPacer->budgetBytes, maxBudgetBytes);

    // Send while the budget is positive and charge each packet afterwards, so a packet larger than the
    // remaining budget never blocks the queue
    while (pPacer->pHead != NULL && pPacer->budgetBytes > 0) {
        pCur = pPacer->pHead;
        pPacer->pHead = pCur->pNext;
        if (pPacer->pHead == NULL) {
            pPacer->pTail = NULL;
        }
        pPacer->queuedPackets--;
        pPacer->queuedBytes -= pCur->packetLen;
        pPacer->budgetBytes -= (INT64) pCur->packetLen;

        pCur->pNext = NULL;
        if (pSendTail == NULL) {
            pSendHead = pCur;
        } else {
            pSendTail->pNext = pCur;
        }
        pSendTail = pCur;
    }

    MUTEX_UNLOCK(pPacer->lock);
    locked = FALSE;

    // Send outside the lock so writeFrame can keep enqueueing
    for (pCur = pSendHead; pCur != NULL; pCur = pNext) {
        pNext = pCur->pNext;
        sendStatus = pPacer->sendPacketFn(pPacer->customData, (PBYTE) (pCur + 1), pCur->packetLen);
        if (STATUS_FAILED(sendStatus)) {
            DLOGV("Pacer failed to send a packet with 0x%08x", sendStatus);
        } else if (pCur->hasTwcc && pPacer->packetSentFn != NULL) {
            pPacer->packetSentFn(pPacer->customData, pCur->twccExtPayload, pCur->payloadLength, now);
        }
        SAFE_MEMFREE(pCur);
    }

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pPacer->lock);
    }

    return retStatus;
}
