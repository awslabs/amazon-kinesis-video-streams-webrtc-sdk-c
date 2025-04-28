/**
 * Main public include file
 */
#ifndef __CONTENT_STATE_INCLUDE__
#define __CONTENT_STATE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>

// IMPORTANT! Some of the headers are not tightly packed!
////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/mkvgen/Include.h>
#include "state_machine.h"

// Fix this later: Does nothing currently
static const ExponentialBackoffRetryStrategyConfig DEFAULT_STATE_MACHINE_EXPONENTIAL_BACKOFF_RETRY_CONFIGURATION = {
    /* Exponential wait times with this config will look like following -
        ************************************
        * Retry Count *      Wait time     *
        * **********************************
        *     1       *    100ms + jitter  *
        *     2       *    200ms + jitter  *
        *     3       *    400ms + jitter  *
        *     4       *    800ms + jitter  *
        *     5       *   1600ms + jitter  *
        *     6       *   3200ms + jitter  *
        *     7       *   6400ms + jitter  *
        *     8       *  10000ms + jitter  *
        *     9       *  10000ms + jitter  *
        *    10       *  10000ms + jitter  *
        ************************************
        jitter = random number between [0, wait time)
    */
    KVS_INFINITE_EXPONENTIAL_RETRIES,                       /* max retry count */
    10000,                                                  /* max retry wait time in milliseconds */
    100,                                                    /* factor determining exponential curve in milliseconds */
    DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, /* minimum time in milliseconds to reset retry state */
    FULL_JITTER,                                            /* use full jitter variant */
    0                                                       /* jitter value unused for full jitter variant */
};

#ifdef __cplusplus
}
#endif
#endif /* __CONTENT_STATE_INCLUDE__ */
