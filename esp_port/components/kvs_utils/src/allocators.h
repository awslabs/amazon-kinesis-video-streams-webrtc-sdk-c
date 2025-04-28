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
#ifndef __AWS_KVS_WEBRTC_ALLOCATORS_INCLUDE__
#define __AWS_KVS_WEBRTC_ALLOCATORS_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(_MSC_VER) && !defined(__MINGW64__) && !defined(__MINGW32__) && !defined(__MACH__)
// NOTE!!! For some reason memalign is not included for Linux builds in stdlib.h
#include <malloc.h>
#endif

#include "common_defs.h"

////////////////////////////////////////////////////
// Dumping memory functionality
////////////////////////////////////////////////////
VOID dumpMemoryHex(PVOID, UINT32);

////////////////////////////////////////////////////
// Check memory content
////////////////////////////////////////////////////
BOOL checkBufferValues(PVOID, BYTE, SIZE_T);

//
// Allocator function definitions
//
typedef PVOID (*memAlloc)(SIZE_T size);
typedef PVOID (*memAlignAlloc)(SIZE_T size, SIZE_T alignment);
typedef PVOID (*memCalloc)(SIZE_T num, SIZE_T size);
typedef PVOID (*memRealloc)(PVOID ptr, SIZE_T size);
typedef VOID (*memFree)(PVOID ptr);

typedef BOOL (*memChk)(PVOID ptr, BYTE val, SIZE_T size);

#if 0
//
// Default allocator functions
//
INLINE PVOID defaultMemAlloc(SIZE_T size);
INLINE PVOID defaultMemAlignAlloc(SIZE_T size, SIZE_T alignment);
INLINE PVOID defaultMemCalloc(SIZE_T num, SIZE_T size);
INLINE PVOID defaultMemRealloc(PVOID ptr, SIZE_T size);
INLINE VOID defaultMemFree(VOID* ptr);
#endif

//
// Global allocator function pointers
//
extern memAlloc globalMemAlloc;
extern memAlignAlloc globalMemAlignAlloc;
extern memCalloc globalMemCalloc;
extern memRealloc globalMemRealloc;
extern memFree globalMemFree;
extern memChk globalMemChk;

//
// Memory allocation and operations
//
#define MEMALLOC      globalMemAlloc
#define MEMALIGNALLOC globalMemAlignAlloc
#define MEMCALLOC     globalMemCalloc
#define MEMREALLOC    globalMemRealloc
#define MEMFREE       globalMemFree
#define MEMCMP        memcmp
#ifndef MEMCPY
#define MEMCPY memcpy
#endif
#define MEMSET memset
#ifndef MEMMOVE
#define MEMMOVE memmove
#endif
#define REALLOC realloc

//
// Whether the buffer contains the same char
//
#define MEMCHK globalMemChk

#ifndef SAFE_MEMFREE
#define SAFE_MEMFREE(p)                                                                                                                              \
    do {                                                                                                                                             \
        if (p) {                                                                                                                                     \
            MEMFREE(p);                                                                                                                              \
            (p) = NULL;                                                                                                                              \
        }                                                                                                                                            \
    } while (0)
#endif

void print_mem_stats(const char *tag);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_THREAD_INCLUDE__ */
