#define LOG_CLASS "Pacer"

#include "../Include_i.h"

static PVOID pacerDrainThread(PVOID args);

STATUS createPacer(UINT64 initialBitrateBps, UINT32 maxQueueSize,
                   PacerSendFrameFn sendFrameFn, PPacer* ppPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacer pPacer = NULL;

    CHK(ppPacer != NULL && sendFrameFn != NULL, STATUS_NULL_ARG);
    CHK(initialBitrateBps >= PACER_MIN_BITRATE_BPS && initialBitrateBps <= PACER_MAX_BITRATE_BPS, STATUS_INVALID_ARG);

    pPacer = (PPacer) MEMCALLOC(1, SIZEOF(Pacer));
    CHK(pPacer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    ATOMIC_STORE_BOOL(&pPacer->running, FALSE);
    ATOMIC_STORE(&pPacer->targetBitrateBps, (SIZE_T) initialBitrateBps);

    pPacer->tokenLock = MUTEX_CREATE(FALSE);
    pPacer->availableTokens = 0;
    pPacer->lastRefillTime = 0;

    pPacer->queueLock = MUTEX_CREATE(FALSE);
    pPacer->queueCvar = CVAR_CREATE();
    pPacer->pHead = NULL;
    pPacer->pTail = NULL;
    pPacer->queueSize = 0;
    pPacer->maxQueueSize = (maxQueueSize > 0) ? maxQueueSize : PACER_DEFAULT_MAX_QUEUE_SIZE;

    pPacer->drainIntervalUs = PACER_DRAIN_INTERVAL_US;
    pPacer->drainThreadId = INVALID_TID_VALUE;

    ATOMIC_STORE(&pPacer->framesSent, 0);
    ATOMIC_STORE(&pPacer->framesDropped, 0);
    ATOMIC_STORE(&pPacer->bytesSent, 0);

    pPacer->sendFrameFn = sendFrameFn;

    *ppPacer = pPacer;

CleanUp:
    if (STATUS_FAILED(retStatus) && pPacer != NULL) {
        SAFE_MEMFREE(pPacer);
    }
    return retStatus;
}

STATUS freePacer(PPacer* ppPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacer pPacer = NULL;
    PPacerQueueNode pNode = NULL, pNext = NULL;

    CHK(ppPacer != NULL, STATUS_NULL_ARG);
    pPacer = *ppPacer;
    CHK(pPacer != NULL, retStatus);

    pacerStop(pPacer);

    // Free remaining queued frames
    pNode = pPacer->pHead;
    while (pNode != NULL) {
        pNext = pNode->pNext;
        SAFE_MEMFREE(pNode->pOwnedFrameData);
        SAFE_MEMFREE(pNode);
        pNode = pNext;
    }

    DLOGI("Pacer freed: sent=%zu, dropped=%zu, bytesSent=%zu",
          (SIZE_T) ATOMIC_LOAD(&pPacer->framesSent),
          (SIZE_T) ATOMIC_LOAD(&pPacer->framesDropped),
          (SIZE_T) ATOMIC_LOAD(&pPacer->bytesSent));

    if (IS_VALID_MUTEX_VALUE(pPacer->tokenLock)) {
        MUTEX_FREE(pPacer->tokenLock);
    }
    if (IS_VALID_CVAR_VALUE(pPacer->queueCvar)) {
        CVAR_FREE(pPacer->queueCvar);
    }
    if (IS_VALID_MUTEX_VALUE(pPacer->queueLock)) {
        MUTEX_FREE(pPacer->queueLock);
    }

    SAFE_MEMFREE(pPacer);
    *ppPacer = NULL;

CleanUp:
    return retStatus;
}

STATUS pacerStart(PPacer pPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pPacer != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pPacer->running), retStatus);

    ATOMIC_STORE_BOOL(&pPacer->running, TRUE);
    pPacer->lastRefillTime = GETTIME();
    pPacer->availableTokens = 0;

    CHK_STATUS(THREAD_CREATE(&pPacer->drainThreadId, pacerDrainThread, (PVOID) pPacer));

CleanUp:
    return retStatus;
}

STATUS pacerStop(PPacer pPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pPacer != NULL, STATUS_NULL_ARG);
    CHK(ATOMIC_LOAD_BOOL(&pPacer->running), retStatus);

    ATOMIC_STORE_BOOL(&pPacer->running, FALSE);

    MUTEX_LOCK(pPacer->queueLock);
    CVAR_SIGNAL(pPacer->queueCvar);
    MUTEX_UNLOCK(pPacer->queueLock);

    if (IS_VALID_TID_VALUE(pPacer->drainThreadId)) {
        THREAD_JOIN(pPacer->drainThreadId, NULL);
        pPacer->drainThreadId = INVALID_TID_VALUE;
    }

CleanUp:
    return retStatus;
}

STATUS pacerEnqueueFrame(PPacer pPacer, UINT64 sendCustomData, PFrame pFrame)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pCopy = NULL;

    CHK(pPacer != NULL && pFrame != NULL && pFrame->frameData != NULL, STATUS_NULL_ARG);
    CHK(pFrame->size > 0, STATUS_INVALID_ARG);

    pCopy = (PBYTE) MEMALLOC(pFrame->size);
    CHK(pCopy != NULL, STATUS_NOT_ENOUGH_MEMORY);
    MEMCPY(pCopy, pFrame->frameData, pFrame->size);

    // Temporarily point frame at the copy for the zero-copy path
    {
        Frame frameCopy = *pFrame;
        frameCopy.frameData = pCopy;
        retStatus = pacerEnqueueFrameZeroCopy(pPacer, sendCustomData, &frameCopy, pCopy);
    }

    // On success, ownership transferred; on failure, free the copy
    if (STATUS_FAILED(retStatus)) {
        SAFE_MEMFREE(pCopy);
    }

CleanUp:
    return retStatus;
}

STATUS pacerEnqueueFrameZeroCopy(PPacer pPacer, UINT64 sendCustomData, PFrame pFrame, PBYTE pFrameData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacerQueueNode pNode = NULL;

    CHK(pPacer != NULL && pFrame != NULL && pFrameData != NULL, STATUS_NULL_ARG);
    CHK(pFrame->size > 0 && pFrame->frameData == pFrameData, STATUS_INVALID_ARG);

    MUTEX_LOCK(pPacer->queueLock);

    if (pPacer->queueSize >= pPacer->maxQueueSize) {
        MUTEX_UNLOCK(pPacer->queueLock);
        DLOGW("Pacer queue full (%u frames), dropping frame pts=%llu", pPacer->queueSize, pFrame->presentationTs);
        ATOMIC_INCREMENT(&pPacer->framesDropped);
        // Caller still owns pFrameData on failure
        CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
    }

    pNode = (PPacerQueueNode) MEMCALLOC(1, SIZEOF(PacerQueueNode));
    if (pNode == NULL) {
        MUTEX_UNLOCK(pPacer->queueLock);
        CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
    }

    pNode->frame = *pFrame;
    pNode->pOwnedFrameData = pFrameData;  // take ownership
    pNode->frame.frameData = pNode->pOwnedFrameData;
    pNode->sendCustomData = sendCustomData;
    pNode->pNext = NULL;

    if (pPacer->pTail != NULL) {
        pPacer->pTail->pNext = pNode;
    } else {
        pPacer->pHead = pNode;
    }
    pPacer->pTail = pNode;
    pPacer->queueSize++;

    CVAR_SIGNAL(pPacer->queueCvar);
    MUTEX_UNLOCK(pPacer->queueLock);

CleanUp:
    return retStatus;
}

STATUS pacerSetBitrate(PPacer pPacer, UINT64 bitrateBps)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pPacer != NULL, STATUS_NULL_ARG);

    if (bitrateBps < PACER_MIN_BITRATE_BPS) {
        bitrateBps = PACER_MIN_BITRATE_BPS;
    } else if (bitrateBps > PACER_MAX_BITRATE_BPS) {
        bitrateBps = PACER_MAX_BITRATE_BPS;
    }

    ATOMIC_STORE(&pPacer->targetBitrateBps, (SIZE_T) bitrateBps);
    DLOGD("Pacer bitrate updated to %llu bps", (UINT64) bitrateBps);

CleanUp:
    return retStatus;
}

UINT64 pacerGetBitrate(PPacer pPacer)
{
    if (pPacer == NULL) {
        return 0;
    }
    return (UINT64) ATOMIC_LOAD(&pPacer->targetBitrateBps);
}

BOOL pacerIsRunning(PPacer pPacer)
{
    if (pPacer == NULL) {
        return FALSE;
    }
    return ATOMIC_LOAD_BOOL(&pPacer->running);
}

//-------------------------------------------------------------------
// Internal helpers
//-------------------------------------------------------------------
static STATUS pacerRefillTokens(PPacer pPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 now, elapsed, bitrate;
    INT64 newTokens, maxBurst;

    CHK(pPacer != NULL, STATUS_NULL_ARG);

    now = GETTIME();
    elapsed = now - pPacer->lastRefillTime;
    CHK(elapsed > 0, retStatus);

    bitrate = (UINT64) ATOMIC_LOAD(&pPacer->targetBitrateBps);

    // tokens (bytes) = bitrate(bps) * elapsed(100ns) / (8 * HUNDREDS_OF_NANOS_IN_A_SECOND)
    newTokens = (INT64) ((DOUBLE) bitrate * (DOUBLE) elapsed / (8.0 * (DOUBLE) HUNDREDS_OF_NANOS_IN_A_SECOND));

    MUTEX_LOCK(pPacer->tokenLock);
    pPacer->availableTokens += newTokens;

    // Cap to prevent unbounded burst after idle.  Allow ~2 drain intervals.
    maxBurst = (INT64) ((DOUBLE) bitrate * (DOUBLE) pPacer->drainIntervalUs * 2.0 / (8.0 * (DOUBLE) HUNDREDS_OF_NANOS_IN_A_SECOND));
    if (maxBurst < (INT64) DEFAULT_MTU_SIZE_BYTES) {
        maxBurst = (INT64) DEFAULT_MTU_SIZE_BYTES;
    }
    if (pPacer->availableTokens > maxBurst) {
        pPacer->availableTokens = maxBurst;
    }

    pPacer->lastRefillTime = now;
    MUTEX_UNLOCK(pPacer->tokenLock);

CleanUp:
    return retStatus;
}

static STATUS pacerDrainQueue(PPacer pPacer)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPacerQueueNode pNode = NULL;
    STATUS sendStatus;

    CHK(pPacer != NULL, STATUS_NULL_ARG);

    pacerRefillTokens(pPacer);

    while (ATOMIC_LOAD_BOOL(&pPacer->running)) {
        MUTEX_LOCK(pPacer->queueLock);
        pNode = pPacer->pHead;
        if (pNode == NULL) {
            MUTEX_UNLOCK(pPacer->queueLock);
            break;
        }

        // Check token budget against frame size
        MUTEX_LOCK(pPacer->tokenLock);
        if (pPacer->availableTokens < (INT64) pNode->frame.size) {
            MUTEX_UNLOCK(pPacer->tokenLock);
            MUTEX_UNLOCK(pPacer->queueLock);
            break;
        }
        pPacer->availableTokens -= (INT64) pNode->frame.size;
        MUTEX_UNLOCK(pPacer->tokenLock);

        // Dequeue
        pPacer->pHead = pNode->pNext;
        if (pPacer->pHead == NULL) {
            pPacer->pTail = NULL;
        }
        pPacer->queueSize--;
        MUTEX_UNLOCK(pPacer->queueLock);

        // Transmit via the registered callback
        sendStatus = pPacer->sendFrameFn(pNode->sendCustomData, &pNode->frame);
        if (STATUS_SUCCEEDED(sendStatus)) {
            ATOMIC_INCREMENT(&pPacer->framesSent);
            ATOMIC_ADD(&pPacer->bytesSent, (SIZE_T) pNode->frame.size);
        } else if (sendStatus != STATUS_SRTP_NOT_READY_YET) {
            ATOMIC_INCREMENT(&pPacer->framesDropped);
            DLOGV("Pacer send failed: 0x%08x", sendStatus);
        }

        SAFE_MEMFREE(pNode->pOwnedFrameData);
        SAFE_MEMFREE(pNode);
    }

CleanUp:
    return retStatus;
}

//-------------------------------------------------------------------
// Drain thread
//-------------------------------------------------------------------
static PVOID pacerDrainThread(PVOID args)
{
    PPacer pPacer = (PPacer) args;

    if (pPacer == NULL) {
        return NULL;
    }

    DLOGI("Pacer drain thread started, interval=%llu ms, bitrate=%llu bps",
          pPacer->drainIntervalUs / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          (UINT64) ATOMIC_LOAD(&pPacer->targetBitrateBps));

    while (ATOMIC_LOAD_BOOL(&pPacer->running)) {
        MUTEX_LOCK(pPacer->queueLock);
        if (pPacer->pHead == NULL) {
            CVAR_WAIT(pPacer->queueCvar, pPacer->queueLock, pPacer->drainIntervalUs);
        }
        MUTEX_UNLOCK(pPacer->queueLock);

        pacerDrainQueue(pPacer);
        THREAD_SLEEP(pPacer->drainIntervalUs);
    }

    // Final drain
    pacerDrainQueue(pPacer);

    DLOGI("Pacer drain thread exiting");
    return NULL;
}
