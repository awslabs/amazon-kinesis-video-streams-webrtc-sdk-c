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
#ifndef __AWS_KVS_WEBRTC_SEMAPHORE_INCLUDE__
#define __AWS_KVS_WEBRTC_SEMAPHORE_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include "time_port.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Semaphore functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Semaphore shutdown timeout value
 */
#define SEMAPHORE_SHUTDOWN_TIMEOUT (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Definition of the Semaphore handle
 */
typedef UINT32 SEMAPHORE_HANDLE;
typedef SEMAPHORE_HANDLE* PSEMAPHORE_HANDLE;

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_SEMAPHORE_HANDLE_VALUE
#define INVALID_SEMAPHORE_HANDLE_VALUE ((SEMAPHORE_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_SEMAPHORE_HANDLE
#define IS_VALID_SEMAPHORE_HANDLE(h) ((h) != INVALID_SEMAPHORE_HANDLE_VALUE)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////
// Semaphore functionality
//////////////////////////////////////////////////////////////////////////////////////////////

// Shutdown spinlock will sleep this period and check
#define SEMAPHORE_SHUTDOWN_SPINLOCK_SLEEP_DURATION (5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Create a semaphore object
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreCreate(UINT32, PSEMAPHORE_HANDLE);

/**
 * Create a semaphore object that starts with 0 count
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreEmptyCreate(UINT32, PSEMAPHORE_HANDLE);

/*
 * Frees the semaphore object releasing all the awaiting threads.
 *
 * NOTE: The call is idempotent.
 *
 * @param - PSEMAPHORE_HANDLE - IN/OUT/OPT - Semaphore handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreFree(PSEMAPHORE_HANDLE);

/*
 * Acquires a semaphore. Will block for the specified amount of time before failing the acquisition.
 *
 * IMPORTANT NOTE: On failed acquire it will not increment the acquired count.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Time to wait to acquire in 100ns
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreAcquire(SEMAPHORE_HANDLE, UINT64);

/*
 * Releases a semaphore. Blocked threads will be released to acquire the available slot
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreRelease(SEMAPHORE_HANDLE);

/*
 * Locks the semaphore for any acquisitions.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreLock(SEMAPHORE_HANDLE);

/*
 * Unlocks the semaphore.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreUnlock(SEMAPHORE_HANDLE);

/*
 * Await for the semaphore to drain.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Timeout value to wait for
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreWaitUntilClear(SEMAPHORE_HANDLE, UINT64);

/*
 * Get the current value of the semaphore count
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - PINT32 - OUT - Value of count
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreGetCount(SEMAPHORE_HANDLE, PINT32);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_SEMAPHORE_INCLUDE__ */
