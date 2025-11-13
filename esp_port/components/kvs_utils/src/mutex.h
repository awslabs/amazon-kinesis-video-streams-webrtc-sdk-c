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
#ifndef __AWS_KVS_WEBRTC_MUTEX_INCLUDE__
#define __AWS_KVS_WEBRTC_MUTEX_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include "platform_esp32.h"

// Forward declarations for Windows builds
#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
/* Forward declarations - these are defined in common_defs.h */
extern MUTEX globalCreateMutex(BOOL);
extern CVAR globalConditionVariableCreate(void);

// Definition of the static initializers
#define GLOBAL_MUTEX_INIT           globalCreateMutex(FALSE)
#define GLOBAL_MUTEX_INIT_RECURSIVE globalCreateMutex(TRUE)
#define GLOBAL_CVAR_INIT            globalConditionVariableCreate()

#else //!< #if defined _WIN32 || defined _WIN64 || defined __CYGWIN__

// NOTE!!! Some of the libraries don't have a definition of PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER

#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP                                                                                                       \
    {                                                                                                                                                \
        {                                                                                                                                            \
            PTHREAD_MUTEX_RECURSIVE                                                                                                                  \
        }                                                                                                                                            \
    }
#endif //!< #ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP

// On ESP32, pthread_mutex_t is a scalar type (uint32_t), so we can't use brace initialization
// ESP32 doesn't support static initialization of recursive mutexes - must use pthread_mutex_init
// This macro should not be used as a static initializer on ESP32
#define GLOBAL_MUTEX_INIT_RECURSIVE 0
#else //!< #ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#define GLOBAL_MUTEX_INIT_RECURSIVE PTHREAD_RECURSIVE_MUTEX_INITIALIZER
#endif //!< #ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER

#define GLOBAL_MUTEX_INIT PTHREAD_MUTEX_INITIALIZER
#define GLOBAL_CVAR_INIT  PTHREAD_COND_INITIALIZER

#endif //!< #if defined _WIN32 || defined _WIN64 || defined __CYGWIN__

// Max mutex name
#ifndef MAX_MUTEX_NAME
#define MAX_MUTEX_NAME 32
#endif

typedef BOOL (*waitLockMutex)(MUTEX, UINT64);
extern waitLockMutex globalWaitLockMutex;

//
// Condition variable functionality
//
#define CVAR_CREATE    globalConditionVariableCreate
#define CVAR_SIGNAL    globalConditionVariableSignal
#define CVAR_BROADCAST globalConditionVariableBroadcast
#define CVAR_WAIT      globalConditionVariableWait
#define CVAR_FREE      globalConditionVariableFree

//
// Static initializers
//
#define MUTEX_INIT           GLOBAL_MUTEX_INIT
#define MUTEX_INIT_RECURSIVE GLOBAL_MUTEX_INIT_RECURSIVE
#define CVAR_INIT            GLOBAL_CVAR_INIT

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_MUTEX_INCLUDE__ */
