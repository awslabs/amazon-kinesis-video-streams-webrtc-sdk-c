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
#ifndef __AWS_KVS_WEBRTC_ATOMICS_INCLUDE__
#define __AWS_KVS_WEBRTC_ATOMICS_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
//
// Atomics functions
//
typedef SIZE_T (*atomicLoad)(volatile SIZE_T*);
typedef VOID (*atomicStore)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicExchange)(volatile SIZE_T*, SIZE_T);
typedef BOOL (*atomicCompareExchange)(volatile SIZE_T*, SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicIncrement)(volatile SIZE_T*);
typedef SIZE_T (*atomicDecrement)(volatile SIZE_T*);
typedef SIZE_T (*atomicAdd)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicSubtract)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicAnd)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicOr)(volatile SIZE_T*, SIZE_T);
typedef SIZE_T (*atomicXor)(volatile SIZE_T*, SIZE_T);
//
// Atomics
//
extern PUBLIC_API atomicLoad globalAtomicLoad;
extern PUBLIC_API atomicStore globalAtomicStore;
extern PUBLIC_API atomicExchange globalAtomicExchange;
extern PUBLIC_API atomicCompareExchange globalAtomicCompareExchange;
extern PUBLIC_API atomicIncrement globalAtomicIncrement;
extern PUBLIC_API atomicDecrement globalAtomicDecrement;
extern PUBLIC_API atomicAdd globalAtomicAdd;
extern PUBLIC_API atomicSubtract globalAtomicSubtract;
extern PUBLIC_API atomicAnd globalAtomicAnd;
extern PUBLIC_API atomicOr globalAtomicOr;
extern PUBLIC_API atomicXor globalAtomicXor;
//
// Basic Atomics functionality
//
#define ATOMIC_LOAD             globalAtomicLoad
#define ATOMIC_STORE            globalAtomicStore
#define ATOMIC_EXCHANGE         globalAtomicExchange
#define ATOMIC_COMPARE_EXCHANGE globalAtomicCompareExchange
#define ATOMIC_INCREMENT        globalAtomicIncrement
#define ATOMIC_DECREMENT        globalAtomicDecrement
#define ATOMIC_ADD              globalAtomicAdd
#define ATOMIC_SUBTRACT         globalAtomicSubtract
#define ATOMIC_AND              globalAtomicAnd
#define ATOMIC_OR               globalAtomicOr
#define ATOMIC_XOR              globalAtomicXor
//
// Helper atomics
//
typedef SIZE_T ATOMIC_BOOL;
#define ATOMIC_LOAD_BOOL             (BOOL) globalAtomicLoad
#define ATOMIC_STORE_BOOL(a, b)      ATOMIC_STORE((a), (SIZE_T)(b))
#define ATOMIC_EXCHANGE_BOOL         (BOOL) globalAtomicExchange
#define ATOMIC_COMPARE_EXCHANGE_BOOL (BOOL) globalAtomicCompareExchange
#define ATOMIC_AND_BOOL              (BOOL) globalAtomicAnd
#define ATOMIC_OR_BOOL               (BOOL) globalAtomicOr
#define ATOMIC_XOR_BOOL              (BOOL) globalAtomicXor
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_ATOMICS_INCLUDE__ */
