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
#ifndef __AWS_KVS_WEBRTC_INSTRUMENTED_ALLOCATORS_INCLUDE__
#define __AWS_KVS_WEBRTC_INSTRUMENTED_ALLOCATORS_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
PVOID instrumentedAllocatorsMemAlloc(SIZE_T);
PVOID instrumentedAllocatorsMemAlignAlloc(SIZE_T, SIZE_T);
PVOID instrumentedAllocatorsMemCalloc(SIZE_T, SIZE_T);
PVOID instrumentedAllocatorsMemRealloc(PVOID, SIZE_T);
VOID instrumentedAllocatorsMemFree(PVOID);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrumented memory allocators functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the global allocators to the instrumented ones.
 *
 * @return - STATUS code of the execution
 */
STATUS setInstrumentedAllocators();

/**
 * No-op equivalent of the setInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(SET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
STATUS setInstrumentedAllocatorsNoop();

/**
 * Resets the global allocators to the original ones.
 *
 * NOTE: Any attempt to free allocations which were allocated after set call
 * past this API call will result in memory corruption.
 *
 * @return - STATUS code of the execution
 */
STATUS resetInstrumentedAllocators();

/**
 * No-op equivalent of the resetInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(RESET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
STATUS resetInstrumentedAllocatorsNoop();

/**
 * Returns the current total allocation size.
 *
 * @return - Total allocation size
 */
SIZE_T getInstrumentedTotalAllocationSize();

#ifdef INSTRUMENTED_ALLOCATORS
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocators()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocators()
#else
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocatorsNoop()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocatorsNoop()
#endif

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_INSTRUMENTED_ALLOCATORS_INCLUDE__ */
