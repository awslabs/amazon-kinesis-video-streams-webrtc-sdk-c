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
#ifndef __AWS_KVS_WEBRTC_TIME_INCLUDE__
#define __AWS_KVS_WEBRTC_TIME_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include <time.h>
#include "common_defs.h"
#include "platform_esp32.h"
#include "error.h"

#ifndef INLINE
#define INLINE       inline
#endif

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
// yyyy-mm-dd HH:MM:SS
#define MAX_TIMESTAMP_FORMAT_STR_LEN                             26
#define KVS_CONVERT_TIMESCALE(pts, from_timescale, to_timescale) (pts * to_timescale / from_timescale)

//
// NOTE: Timer precision is in 100ns intervals. This is used in heuristics and in time functionality
//
#define DEFAULT_TIME_UNIT_IN_NANOS         100
#define HUNDREDS_OF_NANOS_IN_A_MICROSECOND 10LL
#define HUNDREDS_OF_NANOS_IN_A_MILLISECOND (HUNDREDS_OF_NANOS_IN_A_MICROSECOND * 1000LL)
#define HUNDREDS_OF_NANOS_IN_A_SECOND      (HUNDREDS_OF_NANOS_IN_A_MILLISECOND * 1000LL)
#define HUNDREDS_OF_NANOS_IN_A_MINUTE      (HUNDREDS_OF_NANOS_IN_A_SECOND * 60LL)
#define HUNDREDS_OF_NANOS_IN_AN_HOUR       (HUNDREDS_OF_NANOS_IN_A_MINUTE * 60LL)
//
// Infinite time
//
#define INFINITE_TIME_VALUE MAX_UINT64
//
// Default time library functions
//
#define TIME_DIFF_UNIX_WINDOWS_TIME 116444736000000000ULL
/**
 * Gets the current time in 100ns from some timestamp.
 *
 * @param 1 UINT64 - Custom handle passed by the caller.
 *
 * @return Current time value in 100ns
 */
typedef UINT64 (*GetCurrentTimeFunc)(UINT64);
//
// Time library function definitions
//
typedef UINT64 (*getTime)();
//
// Thread related functionality
//
extern getTime globalGetTime;
extern getTime globalGetRealTime;

//
// Time functionality
//
#define MKTIME      mktime
#define GETTIME     globalGetTime
#define GETREALTIME globalGetRealTime
#define STRFTIME    strftime
#define GMTIME      gmtime

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * @UINT64  - IN - timestamp in 100ns to be converted to string
 * @PCHAR   - IN - timestamp format string
 * @PCHAR   - IN - buffer to hold the resulting string
 * @UINT32  - IN - buffer size
 * @PUINT32 - OUT - actual number of characters in the result string not including null terminator
 * @return  - STATUS code of the execution
 */
UINT32 generateTimestampStr(UINT64, PCHAR, PCHAR, UINT32, PUINT32);

UINT64 defaultGetTime();

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_TIME_INCLUDE__ */
