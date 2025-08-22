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

#ifndef __AWS_KVS_WEBRTC_THREAD_INCLUDE__
#define __AWS_KVS_WEBRTC_THREAD_INCLUDE__

#include "common_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DEFAULT_THREAD_SIZE
#define DEFAULT_THREAD_SIZE (30 * 1024)
#define DEFAULT_THREAD_NAME "pthread"
#endif

// Max thread name buffer length - similar to Linux platforms
#ifndef MAX_THREAD_NAME
#define MAX_THREAD_NAME 16
#endif

typedef VOID (*exitThread)(PVOID);

//
// Thread related functionality
//
extern getTId globalGetThreadId;
extern getTName globalGetThreadName;

//
// Thread and Mutex related functionality
//
extern createThreadExExt globalCreateThreadExExt;
extern createThreadExPri globalCreateThreadExPri;
extern exitThread globalExitThread;

//
// Thread functionality
//
#define GETTID   globalGetThreadId
#define GETTNAME globalGetThreadName

//
// Thread functionality
//
#define THREAD_CREATE        globalCreateThread
#define THREAD_CREATE_EX     globalCreateThreadEx
#define THREAD_CREATE_EX_EXT globalCreateThreadExExt
#define THREAD_CREATE_EX_PRI globalCreateThreadExPri
#define THREAD_JOIN          globalJoinThread
#define THREAD_SLEEP         globalThreadSleep
#define THREAD_SLEEP_UNTIL   globalThreadSleepUntil
#define THREAD_CANCEL        globalCancelThread
#define THREAD_DETACH        globalDetachThread
#define THREAD_EXIT          globalExitThread

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_THREAD_INCLUDE__ */
