#include "ThreadSafeBlockingQueue.h"
#include "ThreadPool.h"

/**
 * Create a threadsafe blocking queue
 */
STATUS safeBlockingQueueCreate(PSafeBlockingQueue* ppSafeQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSafeBlockingQueue pSafeQueue = NULL;

    CHK(ppSafeQueue != NULL, STATUS_NULL_ARG);

    // Allocate the main structure
    pSafeQueue = (PSafeBlockingQueue) MEMCALLOC(1, SIZEOF(SafeBlockingQueue));
    CHK(pSafeQueue != NULL, STATUS_NOT_ENOUGH_MEMORY);

    ATOMIC_STORE_BOOL(&pSafeQueue->terminate, FALSE);
    ATOMIC_STORE(&pSafeQueue->atLockCount, 0);

    pSafeQueue->mutex = MUTEX_CREATE(FALSE);
    pSafeQueue->terminationSignal = CVAR_CREATE();
    CHK_STATUS(semaphoreEmptyCreate(KVS_MAX_BLOCKING_QUEUE_ENTRIES, &(pSafeQueue->semaphore)));
    CHK_STATUS(stackQueueCreate(&(pSafeQueue->queue)));

    *ppSafeQueue = pSafeQueue;

CleanUp:
    if (STATUS_FAILED(retStatus) && pSafeQueue != NULL) {
        SAFE_MEMFREE(pSafeQueue);
    }

    return retStatus;
}

/**
 * Frees and de-allocates the thread safe blocking queue
 */
STATUS safeBlockingQueueFree(PSafeBlockingQueue pSafeQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 count = 0;
    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);

    // set terminate flag, lock mutex -- this assures all other threads
    // are no longer directly interacting with the queue
    //
    // free semaphore, this will unblock any threads blocking on said semaphore,
    // and then exit from the terminate flag
    //
    // unlock mutex
    ATOMIC_STORE_BOOL(&pSafeQueue->terminate, TRUE);

    CHK_STATUS(semaphoreLock(pSafeQueue->semaphore));

    semaphoreFree(&(pSafeQueue->semaphore));

    MUTEX_LOCK(pSafeQueue->mutex);

    // wait for all threads to unlock the mutex
    if (ATOMIC_LOAD(&pSafeQueue->atLockCount) != 0) {
        CVAR_WAIT(pSafeQueue->terminationSignal, pSafeQueue->mutex, INFINITE_TIME_VALUE);
    }

    stackQueueFree(pSafeQueue->queue);
    MUTEX_UNLOCK(pSafeQueue->mutex);

    MUTEX_FREE(pSafeQueue->mutex);

    CVAR_FREE(pSafeQueue->terminationSignal);

    SAFE_MEMFREE(pSafeQueue);

CleanUp:

    return retStatus;
}

/**
 * Clears and de-allocates all the items
 */
STATUS safeBlockingQueueClear(PSafeBlockingQueue pSafeQueue, BOOL freeData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    // 0 timeout semaphore acquire, eventually getting a timeout, this is done to clear
    // the counting semaphore
    while ((semaphoreAcquire(pSafeQueue->semaphore, 0) == STATUS_SUCCESS) && !ATOMIC_LOAD_BOOL(&pSafeQueue->terminate))
        ;

    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    ATOMIC_INCREMENT(&pSafeQueue->atLockCount);
    MUTEX_LOCK(pSafeQueue->mutex);
    ATOMIC_DECREMENT(&pSafeQueue->atLockCount);
    locked = TRUE;

    // to avoid memory corruption the destructor waits for this signal if the atLockCount
    // isn't 0
    if (ATOMIC_LOAD_BOOL(&pSafeQueue->terminate)) {
        if (ATOMIC_LOAD(&pSafeQueue->atLockCount) == 0) {
            CHK_STATUS(CVAR_BROADCAST(pSafeQueue->terminationSignal));
        }
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    CHK_STATUS(stackQueueClear(pSafeQueue->queue, freeData));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Gets the number of items in the stack/queue
 */
STATUS safeBlockingQueueGetCount(PSafeBlockingQueue pSafeQueue, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    ATOMIC_INCREMENT(&pSafeQueue->atLockCount);
    MUTEX_LOCK(pSafeQueue->mutex);
    ATOMIC_DECREMENT(&pSafeQueue->atLockCount);
    locked = TRUE;

    // to avoid memory corruption the destructor waits for this signal if the atLockCount
    // isn't 0
    if (ATOMIC_LOAD_BOOL(&pSafeQueue->terminate)) {
        if (ATOMIC_LOAD(&pSafeQueue->atLockCount) == 0) {
            CHK_STATUS(CVAR_BROADCAST(pSafeQueue->terminationSignal));
        }
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    CHK_STATUS(stackQueueGetCount(pSafeQueue->queue, pCount));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Whether the thread safe blocking queue is empty
 */
STATUS safeBlockingQueueIsEmpty(PSafeBlockingQueue pSafeQueue, PBOOL pIsEmpty)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL && pIsEmpty != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    ATOMIC_INCREMENT(&pSafeQueue->atLockCount);
    MUTEX_LOCK(pSafeQueue->mutex);
    ATOMIC_DECREMENT(&pSafeQueue->atLockCount);
    locked = TRUE;

    // to avoid memory corruption the destructor waits for this signal if the atLockCount
    // isn't 0
    if (ATOMIC_LOAD_BOOL(&pSafeQueue->terminate)) {
        if (ATOMIC_LOAD(&pSafeQueue->atLockCount) == 0) {
            CHK_STATUS(CVAR_BROADCAST(pSafeQueue->terminationSignal));
        }
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    CHK_STATUS(stackQueueIsEmpty(pSafeQueue->queue, pIsEmpty));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Enqueues an item in the queue
 */
STATUS safeBlockingQueueEnqueue(PSafeBlockingQueue pSafeQueue, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    ATOMIC_INCREMENT(&pSafeQueue->atLockCount);
    MUTEX_LOCK(pSafeQueue->mutex);
    ATOMIC_DECREMENT(&pSafeQueue->atLockCount);
    locked = TRUE;

    // to avoid memory corruption the destructor waits for this signal if the atLockCount
    // isn't 0
    if (ATOMIC_LOAD_BOOL(&pSafeQueue->terminate)) {
        if (ATOMIC_LOAD(&pSafeQueue->atLockCount) == 0) {
            CHK_STATUS(CVAR_BROADCAST(pSafeQueue->terminationSignal));
        }
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    CHK_STATUS(stackQueueEnqueue(pSafeQueue->queue, item));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

    CHK_STATUS(semaphoreRelease(pSafeQueue->semaphore));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}

/**
 * Dequeues an item from the queue
 */
STATUS safeBlockingQueueDequeue(PSafeBlockingQueue pSafeQueue, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSafeQueue != NULL && pItem != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSafeQueue->terminate), STATUS_INVALID_OPERATION);

    CHK_STATUS(semaphoreAcquire(pSafeQueue->semaphore, INFINITE_TIME_VALUE));

    ATOMIC_INCREMENT(&pSafeQueue->atLockCount);
    MUTEX_LOCK(pSafeQueue->mutex);
    ATOMIC_DECREMENT(&pSafeQueue->atLockCount);
    locked = TRUE;

    // to avoid memory corruption the destructor waits for this signal if the atLockCount
    // isn't 0
    if (ATOMIC_LOAD_BOOL(&pSafeQueue->terminate)) {
        if (ATOMIC_LOAD(&pSafeQueue->atLockCount) == 0) {
            CHK_STATUS(CVAR_BROADCAST(pSafeQueue->terminationSignal));
        }
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    CHK_STATUS(stackQueueDequeue(pSafeQueue->queue, pItem));

    MUTEX_UNLOCK(pSafeQueue->mutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pSafeQueue->mutex);
    }

    return retStatus;
}
