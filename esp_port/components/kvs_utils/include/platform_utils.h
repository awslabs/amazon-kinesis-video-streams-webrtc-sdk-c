/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "platform_esp32.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define CONN_LISTENER_THREAD_NAME "connListener"
#define WSS_CLIENT_THREAD_NAME    "wss_client" //!< the parameters of wss listener.
#define WSS_DISPATCH_THREAD_NAME  "wssDispatch" //!< the parameters of wss dispatcher
#define PEER_TIMER_NAME           "peerTimer"

#define CONN_LISTENER_THREAD_SIZE 16000
#if ENABLE_SIGNALLING_ONLY
#define WSS_CLIENT_THREAD_SIZE    8000
#define WSS_DISPATCH_THREAD_SIZE  4000 //!< this is passed to esp_work_queue
#else
#define WSS_CLIENT_THREAD_SIZE    10240
#define WSS_DISPATCH_THREAD_SIZE  10240
#endif
#define PEER_TIMER_SIZE           10240

// Tag for the logging
#ifndef LOG_CLASS
#define LOG_CLASS "platform-utils"
#endif

/**
 * EMA (Exponential Moving Average) alpha value and 1-alpha value - over appx 20 samples
 */
#define EMA_ALPHA_VALUE           ((DOUBLE) 0.05)
#define ONE_MINUS_EMA_ALPHA_VALUE ((DOUBLE) (1 - EMA_ALPHA_VALUE))

/**
 * Calculates the EMA (Exponential Moving Average) accumulator value
 *
 * a - Accumulator value
 * v - Next sample point
 *
 * @return the new Accumulator value
 */
#define EMA_ACCUMULATOR_GET_NEXT(a, v) (DOUBLE)(EMA_ALPHA_VALUE * (v) + ONE_MINUS_EMA_ALPHA_VALUE * (a))

// Max timestamp string length including null terminator
#define MAX_TIMESTAMP_STR_LEN 17

// (thread-0x7000076b3000)
#define MAX_THREAD_ID_STR_LEN 23


#ifdef ANDROID_BUILD
// Compiling with NDK
#include <android/log.h>
#define __LOG(p1, p2, p3, ...)    __android_log_print(p1, p2, p3, ##__VA_ARGS__)
#define __ASSERT(p1, p2, p3, ...) __android_log_assert(p1, p2, p3, ##__VA_ARGS__)
#else
// Compiling under non-NDK
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#define __ASSERT(p1, p2, p3, ...) assert(p1)
#define __LOG                     globalCustomLogPrintFn
#endif // ANDROID_BUILD

#define LOG_LEVEL_VERBOSE 1
#define LOG_LEVEL_DEBUG   2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_WARN    4
#define LOG_LEVEL_ERROR   5
#define LOG_LEVEL_FATAL   6
#define LOG_LEVEL_SILENT  7

// Adding this after LOG_LEVEL_SILENT to ensure we do not break backward compat
#define LOG_LEVEL_PROFILE 8

#define LOG_LEVEL_VERBOSE_STR (PCHAR) "VERBOSE"
#define LOG_LEVEL_DEBUG_STR   (PCHAR) "DEBUG"
#define LOG_LEVEL_BETA_STR    (PCHAR) "BETA"
#define LOG_LEVEL_INFO_STR    (PCHAR) "INFO"
#define LOG_LEVEL_WARN_STR    (PCHAR) "WARN"
#define LOG_LEVEL_ERROR_STR   (PCHAR) "ERROR"
#define LOG_LEVEL_FATAL_STR   (PCHAR) "FATAL"
#define LOG_LEVEL_SILENT_STR  (PCHAR) "SILENT"
#define LOG_LEVEL_PROFILE_STR (PCHAR) "PROFILE"

// Tag for the logging
#ifndef LOG_CLASS
#define LOG_CLASS "platform-utils"
#endif

#ifdef DETECTED_GIT_HASH
#define GIT_HASH SDK_VERSION
#else
#define GIT_HASH EMPTY_STRING
#endif

// Log print function definition
typedef VOID (*logPrintFunc)(UINT32, const PCHAR, const PCHAR, ...);

//
// Default logger function
//
PUBLIC_API VOID defaultLogPrint(UINT32 level, const PCHAR tag, const PCHAR fmt, ...);

extern logPrintFunc globalCustomLogPrintFn;

// Compiling under non-NDK
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#define __LOG globalCustomLogPrintFn

// LOG_LEVEL_VERBOSE_STR string length
#define MAX_LOG_LEVEL_STRLEN 7

// Extra logging macros
#ifndef DLOGE
#define DLOGE(fmt, ...) __LOG(LOG_LEVEL_ERROR, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGW
#define DLOGW(fmt, ...) __LOG(LOG_LEVEL_WARN, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGI
#define DLOGI(fmt, ...) __LOG(LOG_LEVEL_INFO, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGD
#define DLOGD(fmt, ...) __LOG(LOG_LEVEL_DEBUG, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGV
#define DLOGV(fmt, ...) __LOG(LOG_LEVEL_VERBOSE, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef DLOGP
#define DLOGP(fmt, ...) __LOG(LOG_LEVEL_PROFILE, (const PCHAR) LOG_CLASS, (const PCHAR) "%s(): " fmt, __FUNCTION__, ##__VA_ARGS__)
#endif

#ifndef ENTER
#define ENTER() DLOGV("Enter")
#endif
#ifndef LEAVE
#define LEAVE() DLOGV("Leave")
#endif
#ifndef ENTERS
#define ENTERS() DLOGS("Enter")
#endif
#ifndef LEAVES
#define LEAVES() DLOGS("Leave")
#endif

// Optionally log very verbose data (>200 msgs/second) about the streaming process
#ifndef DLOGS
#ifdef LOG_STREAMING
#define DLOGS DLOGV
#else
#define DLOGS(...)                                                                                                                                   \
    do {                                                                                                                                             \
    } while (0)
#endif
#endif

// Assertions
#ifndef CONDITION
#ifdef __GNUC__
#define CONDITION(cond) (__builtin_expect((cond) != 0, 0))
#else
#define CONDITION(cond) ((cond) == TRUE)
#endif
#endif
#ifndef LOG_ALWAYS_FATAL_IF
#define LOG_ALWAYS_FATAL_IF(cond, ...) ((CONDITION(cond)) ? ((void) __ASSERT(FALSE, (const PCHAR) LOG_CLASS, ##__VA_ARGS__)) : (void) 0)
#endif

#ifndef LOG_ALWAYS_FATAL
#define LOG_ALWAYS_FATAL(...) (((void) __ASSERT(FALSE, (const PCHAR) LOG_CLASS, ##__VA_ARGS__)))
#endif

#ifndef SANITIZED_FILE
#define SANITIZED_FILE (STRRCHR(__FILE__, '/') ? STRRCHR(__FILE__, '/') + 1 : __FILE__)
#endif
#ifndef CRASH
#define CRASH(fmt, ...) LOG_ALWAYS_FATAL("%s::%s: " fmt, (const PCHAR) LOG_CLASS, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef CHECK
#define CHECK(x) LOG_ALWAYS_FATAL_IF(!(x), "%s::%s: ASSERTION FAILED at %s:%d: " #x, (const PCHAR) LOG_CLASS, __FUNCTION__, SANITIZED_FILE, __LINE__)
#endif
#ifndef CHECK_EXT
#define CHECK_EXT(x, fmt, ...)                                                                                                                       \
    LOG_ALWAYS_FATAL_IF(!(x), "%s::%s: ASSERTION FAILED at %s:%d: " fmt, (const PCHAR) LOG_CLASS, __FUNCTION__, SANITIZED_FILE, __LINE__,            \
                        ##__VA_ARGS__)
#endif

#ifndef LOG_GIT_HASH
#define LOG_GIT_HASH() DLOGI("SDK version: %s", GIT_HASH)
#endif

#define DLOGD_LINE() DLOGD("%s(%d)", __func__, __LINE__)

////////////////////////////////////////////////////
// Callbacks definitions
////////////////////////////////////////////////////

////////////////////////////////////////////////////
// Main structure declarations
////////////////////////////////////////////////////

#define FRAME_CURRENT_VERSION      0

/**
 * Frame types enum
 */
typedef enum {
    /**
     * No flags are set
     */
    FRAME_FLAG_NONE = 0,

    /**
     * The frame is a key frame - I or IDR
     */
    FRAME_FLAG_KEY_FRAME = (1 << 0),

    /**
     * The frame is discardable - no other frames depend on it
     */
    FRAME_FLAG_DISCARDABLE_FRAME = (1 << 1),

    /**
     * The frame is invisible for rendering
     */
    FRAME_FLAG_INVISIBLE_FRAME = (1 << 2),

    /**
     * The frame is an explicit marker for the end-of-fragment
     */
    FRAME_FLAG_END_OF_FRAGMENT = (1 << 3),
} FRAME_FLAGS;

/**
 * The representation of the Frame
 */
typedef struct {
    UINT32 version;

    // Id of the frame
    UINT32 index;

    // Flags associated with the frame (ex. IFrame for frames)
    FRAME_FLAGS flags;

    // The decoding timestamp of the frame in 100ns precision
    UINT64 decodingTs;

    // The presentation timestamp of the frame in 100ns precision
    UINT64 presentationTs;

    // The duration of the frame in 100ns precision. Can be 0.
    UINT64 duration;

    // Size of the frame data in bytes
    UINT32 size;

    // The frame bits
    PBYTE frameData;

    // Id of the track this frame belong to
    UINT64 trackId;
} Frame, *PFrame;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/


#ifdef __cplusplus
}
#endif
