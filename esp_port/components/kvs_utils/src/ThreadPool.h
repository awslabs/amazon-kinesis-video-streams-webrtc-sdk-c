
#pragma once

#include "common_defs.h"
#include "ThreadSafeBlockingQueue.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Threadpool APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

// windows doesn't support INT32_MAX
#define KVS_MAX_BLOCKING_QUEUE_ENTRIES ((INT32) 1024)

typedef struct __Threadpool {
    volatile ATOMIC_BOOL terminate;
    // threads waiting for a task
    volatile SIZE_T availableThreads;

    // tracks task
    PSafeBlockingQueue taskQueue;

    // tracks threads created
    PStackQueue threadList;

    MUTEX listMutex;
    UINT32 minThreads;
    UINT32 maxThreads;
} Threadpool, *PThreadpool;

/**
 * Create a new threadpool
 *
 * @param - PThreadpool* - OUT - Pointer to PThreadpool to create
 * @param - UINT32 - IN - minimum threads the threadpool must maintain (cannot be 0)
 * @param - UINT32 - IN - maximum threads the threadpool is allowed to create
 *                       (cannot be 0, must be greater than minimum)
 */
PUBLIC_API STATUS threadpoolCreate(PThreadpool*, UINT32, UINT32);

/**
 * Destroy a threadpool
 *
 * @param - PThreadpool - IN - PThreadpool to destroy
 */
PUBLIC_API STATUS threadpoolFree(PThreadpool pThreadpool);

/**
 * Amount of threads currently tracked by this threadpool
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - PUINT32 - OUT - Pointer to integer to store the count
 */
PUBLIC_API STATUS threadpoolTotalThreadCount(PThreadpool pThreadpool, PUINT32 pCount);

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created.
 *          STATUS_FAILED/ if the threadpool is already at its
 *          predetermined max.
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - startRoutine - IN - function pointer to run in thread
 * @param - PVOID - IN - custom data to send to function pointer
 */
PUBLIC_API STATUS threadpoolTryAdd(PThreadpool, startRoutine, PVOID);

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created
 *          or if the task was added to the queue for the next thread.
 *          STATUS_FAILED/ if the threadpool queue is full.
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - startRoutine - IN - function pointer to run in thread
 * @param - PVOID - IN - custom data to send to function pointer
 */
PUBLIC_API STATUS threadpoolPush(PThreadpool, startRoutine, PVOID);
