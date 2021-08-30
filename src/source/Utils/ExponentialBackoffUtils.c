#include "ExponentialBackoffUtils.h"
#include "../Include_i.h"

STATUS getDefaultExponentialBackOffConfig(PExponentialBackoffConfig* ppPExponentialBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffConfig pExponentialBackoffConfig = NULL;

    CHK(ppPExponentialBackoffConfig != NULL, STATUS_NULL_ARG);

    pExponentialBackoffConfig = (PExponentialBackoffConfig) MEMALLOC(SIZEOF(ExponentialBackoffConfig));
    CHK(pExponentialBackoffConfig != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pExponentialBackoffConfig->maxRetryCount = INFINITE_RETRY_COUNT_SENTINEL;
    pExponentialBackoffConfig->maxWaitTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_MAX_WAIT_TIME_MILLISECONDS;
    pExponentialBackoffConfig->retryFactorTime = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_RETRY_TIME_FACTOR_MILLISECONDS;
    pExponentialBackoffConfig->minTimeToResetRetryState = HUNDREDS_OF_NANOS_IN_A_MILLISECOND * DEFAULT_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS;
    pExponentialBackoffConfig->jitterFactor = DEFAULT_JITTER_FACTOR_MILLISECONDS;

    CleanUp:
    (*ppPExponentialBackoffConfig) = pExponentialBackoffConfig;
    LEAVES();
    return retStatus;
}

STATUS resetRetryState(PExponentialBackoffState pExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pExponentialBackoffState != NULL, STATUS_NULL_ARG);

    pExponentialBackoffState->currentRetryCount = 0;
    pExponentialBackoffState->lastRetryWaitTime = 0;
    pExponentialBackoffState->status = BACKOFF_NOT_STARTED;
    DLOGI("Resetting Exponential Backoff State");

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS initializeExponentialBackoffStateWithDefaultConfig(PExponentialBackoffState* ppExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffState pExponentialBackoffState = NULL;
    PExponentialBackoffConfig pExponentialBackoffConfig = NULL;

    CHK(ppExponentialBackoffState != NULL, STATUS_NULL_ARG);

    pExponentialBackoffState = (PExponentialBackoffState) MEMALLOC(SIZEOF(ExponentialBackoffState));
    CHK(pExponentialBackoffState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(getDefaultExponentialBackOffConfig(&pExponentialBackoffConfig));

    pExponentialBackoffState->pExponentialBackoffConfig = pExponentialBackoffConfig;
    CHK_STATUS(resetRetryState(pExponentialBackoffState));

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppExponentialBackoffState) = pExponentialBackoffState;
    } else {
        if (pExponentialBackoffState != NULL) {
            SAFE_MEMFREE(pExponentialBackoffState);
            SAFE_MEMFREE(pExponentialBackoffState->pExponentialBackoffConfig);
        }
    }

    LEAVES();
    return retStatus;
}

STATUS initializeExponentialBackoffState(PExponentialBackoffState* ppBackoffState, PExponentialBackoffConfig pBackoffConfig) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PExponentialBackoffState pExponentialBackoffState = NULL;
    PExponentialBackoffConfig pExponentialBackoffConfig = NULL;

    CHK(ppBackoffState != NULL, STATUS_NULL_ARG);
    CHK(pBackoffConfig != NULL, STATUS_NULL_ARG);

    pExponentialBackoffState = (PExponentialBackoffState) MEMALLOC(SIZEOF(ExponentialBackoffState));
    CHK(pExponentialBackoffState != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pExponentialBackoffConfig = (PExponentialBackoffConfig) MEMALLOC(SIZEOF(ExponentialBackoffConfig));
    CHK(pExponentialBackoffConfig != NULL, STATUS_NOT_ENOUGH_MEMORY);

    *pExponentialBackoffConfig = *pBackoffConfig;

    pExponentialBackoffState->pExponentialBackoffConfig = pExponentialBackoffConfig;
    CHK_STATUS(resetRetryState(pExponentialBackoffState));

    CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        (*ppBackoffState) = pExponentialBackoffState;
    } else {
        SAFE_MEMFREE(pExponentialBackoffState);
        SAFE_MEMFREE(pExponentialBackoffConfig);
    }

    LEAVES();
    return retStatus;
}

UINT64 getRandomJitter(UINT32 jitterFactor) {
    return (RAND() % jitterFactor) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
}

UINT64 power(UINT32 base, UINT32 exponent) {
    UINT64 result = 1;
    while (exponent-- > 0) {
        result *= base;
    }
    return result;
}

UINT64 calculateWaitTime(PExponentialBackoffState pRetryState, PExponentialBackoffConfig pRetryConfig) {
    return power(DEFAULT_EXPONENTIAL_FACTOR, pRetryState->currentRetryCount) * pRetryConfig->retryFactorTime;
}

STATUS validateAndUpdateExponentialBackoffStatus(PExponentialBackoffState pExponentialBackoffState) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    switch (pExponentialBackoffState->status) {
        case BACKOFF_NOT_STARTED:
            pExponentialBackoffState->status = BACKOFF_IN_PROGRESS;
            DLOGI("Status changed from BACKOFF_NOT_STARTED to BACKOFF_IN_PROGRESS");
            break;
        case BACKOFF_IN_PROGRESS:
            // Do nothing
            DLOGI("Current status is BACKOFF_IN_PROGRESS");
            break;
        case BACKOFF_TERMINATED:
            DLOGI("Cannot execute exponentialBackoffBlockingWait. Current status is BACKOFF_TERMINATED");
            retStatus = STATUS_EXPONENTIAL_INVALID_STATE;
            break;
        default:
            DLOGI("Cannot execute exponentialBackoffBlockingWait. Unexpected state [%d]", pExponentialBackoffState->status);
            retStatus = STATUS_EXPONENTIAL_INVALID_STATE;
    }

    CleanUp:
        LEAVES();
        return retStatus;
}

STATUS exponentialBackoffBlockingWait(PExponentialBackoffState pRetryState) {
    ENTERS();
    PExponentialBackoffConfig pRetryConfig = NULL;
    UINT64 currentTime = 0, waitTime = 0, jitter = 0;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRetryState != NULL, STATUS_NULL_ARG);
    CHK(pRetryState->pExponentialBackoffConfig != NULL, STATUS_NULL_ARG);

    CHK_STATUS(validateAndUpdateExponentialBackoffStatus(pRetryState));

    pRetryConfig = pRetryState->pExponentialBackoffConfig;

    // If retries is exhausted, return error to the application
    if (pRetryConfig->maxRetryCount != INFINITE_RETRY_COUNT_SENTINEL) {
        CHK(!(pRetryConfig->maxRetryCount == pRetryState->currentRetryCount),
            STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED);
    }

    // check the last retry time. If the difference between last retry and current time is greater than
    // minTimeToResetRetryState, then reset the retry state
    //
    // Proceed if this is not the first retry
    currentTime = GETTIME();
    if (pRetryState->currentRetryCount != 0) {
        if (currentTime - pRetryState->lastRetryWaitTime > pRetryConfig->minTimeToResetRetryState) {
            CHK_STATUS(resetRetryState(pRetryState));
            pRetryState->status = BACKOFF_IN_PROGRESS;
        }
    }

    // Bound the exponential curve to maxWaitTime. Once we reach
    // maxWaitTime, then we always wait for maxWaitTime time
    // till the state is reset.
    if (pRetryState->lastRetryWaitTime == pRetryConfig->maxWaitTime) {
        waitTime = pRetryState->lastRetryWaitTime;
    } else {
        waitTime = calculateWaitTime(pRetryState, pRetryConfig);
        if (waitTime > pRetryConfig->maxWaitTime) {
            waitTime = pRetryConfig->maxWaitTime;
        }
    }

    jitter = getRandomJitter(pRetryConfig->jitterFactor);
    THREAD_SLEEP(waitTime + jitter);

    pRetryState->lastRetryWaitTime = currentTime + waitTime + jitter;
    pRetryState->currentRetryCount++;
    DLOGI("Number of retries done [%"PRIu32"]. Last retry wait time [%"PRIu64"]",
          pRetryState->currentRetryCount, pRetryState->lastRetryWaitTime);

CleanUp:
    if (retStatus == STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED) {
        DLOGI("Exhausted exponential retries");
        resetRetryState(pRetryState);
    }
    LEAVES();
    return retStatus;
}

VOID freeExponentialBackoffState(PExponentialBackoffState* ppExponentialBackoffState) {
    ENTERS();
    if (ppExponentialBackoffState == NULL) {
        return;
    }

    if (*ppExponentialBackoffState != NULL) {
        (*ppExponentialBackoffState)->status = BACKOFF_TERMINATED;
        SAFE_MEMFREE((*ppExponentialBackoffState)->pExponentialBackoffConfig);
        (*ppExponentialBackoffState)->pExponentialBackoffConfig = NULL;
    }
    SAFE_MEMFREE(*ppExponentialBackoffState);
    *ppExponentialBackoffState = NULL;
    LEAVES();
}