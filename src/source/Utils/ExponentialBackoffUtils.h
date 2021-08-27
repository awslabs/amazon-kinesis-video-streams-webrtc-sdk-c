/******************************************************
A generic utility for exponential back off waits
******************************************************/
#ifndef __KINESIS_VIDEO_EXPONENTIAL_BACKOFF_HELPER_INCLUDE__
#define __KINESIS_VIDEO_EXPONENTIAL_BACKOFF_HELPER_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

/************************************************************************
 Following #define values are default configuration values for
 exponential backoff retries.

 With these values, the wait times will look like following -
    ************************************
    * Retry Count *      Wait time     *
    * **********************************
    *     1       *    300ms + jitter  *
    *     2       *    600ms + jitter  *
    *     3       *   1200ms + jitter  *
    *     4       *   2400ms + jitter  *
    *     5       *   4800ms + jitter  *
    *     6       *   9600ms + jitter  *
    *     7       *  10000ms + jitter  *
    *     8       *  10000ms + jitter  *
    *     9       *  10000ms + jitter  *
    ************************************
 jitter = random number between [0, 300)ms.
************************************************************************/
/**
 * Factor for computing the exponential backoff wait time
 */
#define DEFAULT_RETRY_TIME_FACTOR_MILLISECONDS                  300

/**
 * Factor determining the curve of exponential wait time
 */
#define DEFAULT_EXPONENTIAL_FACTOR                              2

/**
 * Maximum exponential wait time. Once the exponential wait time
 * curve reaches this value, it stays at this value. This is
 * required to put a reasonable upper bound on wait time.
 */
#define DEFAULT_MAX_WAIT_TIME_MILLISECONDS                      15000

/**
 * Maximum time between two consecutive calls to exponentialBackoffBlockingWait
 * after which the retry count will be reset to initial state.
 */
#define DEFAULT_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS      25000

/**
 * Factor to get a random jitter. Jitter values [0, 300).
 */
#define DEFAULT_JITTER_FACTOR_MILLISECONDS                      300

typedef enum {
    BACKOFF_NOT_STARTED     = (UINT16) 0x01,
    BACKOFF_IN_PROGRESS     = (UINT16) 0x02,
    BACKOFF_TERMINATED      = (UINT16) 0x03
} ExponentialBackoffStatus;

typedef struct __ExponentialBackoffConfig {
    UINT32  maxRetryCount;
    UINT64  maxWaitTime;
    UINT32  retryFactorTime;
    UINT64  minTimeToResetRetryState;
    UINT32  jitterFactor;
} ExponentialBackoffConfig;
typedef ExponentialBackoffConfig* PExponentialBackoffConfig;

typedef struct __ExponentialBackoffState {
    PExponentialBackoffConfig pExponentialBackoffConfig;
    ExponentialBackoffStatus status;
    UINT32 currentRetryCount;
    UINT64 lastRetryWaitTime;
} ExponentialBackoffState;
typedef ExponentialBackoffState* PExponentialBackoffState;

/**************************************************************************************************
API usage:

 PExponentialBackoffState pExponentialBackoffState = NULL;
 CHK_STATUS(initializeExponentialBackoffStateWithDefaultConfig(&pExponentialBackoffState));

 while (...) {
     CHK_STATUS(exponentialBackoffBlockingWait(pExponentialBackoffState));
    // some business logic which includes service API call(s)
 }

 CHK_STATUS(freeExponentialBackoffState(&pExponentialBackoffState));

**************************************************************************************************/

/**
 * @brief Initializes exponential backoff state with default configuration
 * This should be called once before calling exponentialBackoffBlockingWait.
 *
 * NOT THREAD SAFE.
 */
PUBLIC_API STATUS initializeExponentialBackoffStateWithDefaultConfig(PExponentialBackoffState*);

/**
 * @brief Initializes exponential backoff state with provided configuration
 * This should be called once before calling exponentialBackoffBlockingWait.
 * If unsure about the configuration parameters, it is recommended to initialize
 * the state using initializeExponentialBackoffStateWithDefaultConfig API
 *
 * NOT THREAD SAFE.
 */
PUBLIC_API STATUS initializeExponentialBackoffState(PExponentialBackoffState*, PExponentialBackoffConfig);

/**
 * @brief
 * Computes next exponential backoff wait time and blocks the thread for that
 * much time
 *
 * NOT THREAD SAFE.
 */
PUBLIC_API STATUS exponentialBackoffBlockingWait(PExponentialBackoffState pExponentialBackoffState);

/**
 * @brief Frees ExponentialBackoffState and its corresponding ExponentialBackoffConfig struct
 *
 * NOT THREAD SAFE.
 */
PUBLIC_API VOID freeExponentialBackoffState(PExponentialBackoffState*);

#ifdef __cplusplus
}
#endif

#endif // __KINESIS_VIDEO_EXPONENTIAL_BACKOFF_HELPER_INCLUDE__
