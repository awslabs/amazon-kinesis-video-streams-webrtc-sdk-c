
#pragma once

#include "common_defs.h"
#include "semaphore.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Threadsafe Blocking Queue APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    volatile ATOMIC_BOOL terminate;
    volatile SIZE_T atLockCount;
    PStackQueue queue;
    MUTEX mutex;
    SEMAPHORE_HANDLE semaphore;
    CVAR terminationSignal;
} SafeBlockingQueue, *PSafeBlockingQueue;

/**
 * Create a new thread safe blocking queue
 *
 * @param - PSafeBlockingQueue* - OUT - Pointer to PSafeBlockingQueue to create.
 */
PUBLIC_API STATUS safeBlockingQueueCreate(PSafeBlockingQueue*);

/**
 * Frees and de-allocates the thread safe blocking queue
 *
 * @param - PSafeBlockingQueue - OUT - PSafeBlockingQueue to destroy.
 */
PUBLIC_API STATUS safeBlockingQueueFree(PSafeBlockingQueue);

/**
 * Clears and de-allocates all the items
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - BOOL - IN - Free objects stored in queue
 */
PUBLIC_API STATUS safeBlockingQueueClear(PSafeBlockingQueue, BOOL);

/**
 * Gets the number of items in the stack/queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PUINT32 - OUT - Pointer to integer to store count in
 */
PUBLIC_API STATUS safeBlockingQueueGetCount(PSafeBlockingQueue, PUINT32);

/**
 * Whether the thread safe blocking queue is empty
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PBOOL - OUT - Pointer to bool to store whether the queue is empty (true) or not (false)
 */
PUBLIC_API STATUS safeBlockingQueueIsEmpty(PSafeBlockingQueue, PBOOL);

/**
 * Enqueues an item in the queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - UINT64 - IN - casted pointer to object to enqueue
 */
PUBLIC_API STATUS safeBlockingQueueEnqueue(PSafeBlockingQueue, UINT64);

/**
 * Dequeues an item from the queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PUINT64 - OUT - casted pointer to object dequeued
 */
PUBLIC_API STATUS safeBlockingQueueDequeue(PSafeBlockingQueue, PUINT64);
