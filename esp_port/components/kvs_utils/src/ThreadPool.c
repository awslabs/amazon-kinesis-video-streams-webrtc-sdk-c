#include "ThreadPool.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// Threadpool functionality
//////////////////////////////////////////////////////////////////////////////////////////////

STATUS threadpoolInternalCreateThread(PThreadpool);
STATUS threadpoolInternalCanCreateThread(PThreadpool, PBOOL);
STATUS threadpoolInternalInactiveThreadCount(PThreadpool, PSIZE_T);

// per thread in threadpool
typedef struct __ThreadData {
    // Informs us the state of the threadpool object
    volatile ATOMIC_BOOL terminate;
    // The threadpool we belong to (may have been deleted)
    PThreadpool pThreadpool;
    // Must be locked before changing terminate, as a result this ensures us
    // that the threadpool as not been deleted while we hold this lock.
    MUTEX dataMutex;
} ThreadData, *PThreadData;

typedef struct TaskData {
    startRoutine function;
    PVOID customData;
} TaskData, *PTaskData;

typedef struct TerminationTask {
    MUTEX mutex;
    PSIZE_T pCount;
    SEMAPHORE_HANDLE semaphore;
} TerminationTask, *PTerminationTask;

PVOID threadpoolTermination(PVOID data)
{
    PTerminationTask task = (PTerminationTask) data;
    PSIZE_T pCount = NULL;
    if (task != NULL) {
        pCount = task->pCount;
        MUTEX_LOCK(task->mutex);
        semaphoreRelease(task->semaphore);
        (*pCount)++;
        MUTEX_UNLOCK(task->mutex);
    }
    return 0;
}

PVOID threadpoolActor(PVOID data)
{
    PThreadData pThreadData = (PThreadData) data;
    PThreadpool pThreadpool = NULL;
    PSafeBlockingQueue pQueue = NULL;
    PTaskData pTask = NULL;
    UINT32 count = 0;
    BOOL taskQueueEmpty = FALSE;
    UINT64 item = 0;
    BOOL finished = FALSE;

    if (pThreadData == NULL) {
        DLOGE("Threadpool actor unable to start, threaddata is NULL");
        return NULL;
    }

    // attempt to acquire thread mutex, if we cannot it means the threadpool has already been
    // destroyed. Quickly exit
    if (MUTEX_TRYLOCK(pThreadData->dataMutex)) {
        if (!ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
            pThreadpool = pThreadData->pThreadpool;

            if (pThreadpool == NULL) {
                DLOGE("Threadpool actor unable to start, threadpool is NULL");
                return NULL;
            }

            pQueue = pThreadpool->taskQueue;
        } else {
            finished = TRUE;
        }
        MUTEX_UNLOCK(pThreadData->dataMutex);
    } else {
        finished = TRUE;
    }

    // This actor will now wait for a task to be added to the queue, and then execute that task
    // when the task is complete it will check if the we're beyond our min threshold of threads
    // to determine whether it should exit or wait for another task.
    while (!finished) {
        // This lock exists to protect the atomic increment after the terminate check.
        // There is a data-race condition that can result in an increment after the Threadpool
        // has been deleted
        MUTEX_LOCK(pThreadData->dataMutex);

        // ThreadData is allocated separately from the Threadpool.
        // The Threadpool will set terminate to false before the threadpool is free.
        // This way the thread actors can avoid accessing the Threadpool after termination.
        if (!ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
            ATOMIC_INCREMENT(&pThreadData->pThreadpool->availableThreads);
            if (safeBlockingQueueDequeue(pQueue, &item) == STATUS_SUCCESS) {
                pTask = (PTaskData) item;
                ATOMIC_DECREMENT(&pThreadData->pThreadpool->availableThreads);
                MUTEX_UNLOCK(pThreadData->dataMutex);
                if (pTask != NULL) {
                    pTask->function(pTask->customData);
                    SAFE_MEMFREE(pTask);
                }
            } else {
                ATOMIC_DECREMENT(&pThreadData->pThreadpool->availableThreads);
                MUTEX_UNLOCK(pThreadData->dataMutex);
            }
        } else {
            finished = TRUE;
            MUTEX_UNLOCK(pThreadData->dataMutex);
            break;
        }

        MUTEX_LOCK(pThreadData->dataMutex);
        if (ATOMIC_LOAD_BOOL(&pThreadData->terminate)) {
            MUTEX_UNLOCK(pThreadData->dataMutex);
        } else {
            // Threadpool is active - lock its mutex
            MUTEX_LOCK(pThreadpool->listMutex);

            // if threadpool is in teardown, release this mutex and go to queue.
            // We don't want to be the one to remove this actor from the list in the event
            // of teardown.
            if (ATOMIC_LOAD_BOOL(&pThreadpool->terminate)) {
                MUTEX_UNLOCK(pThreadpool->listMutex);
                MUTEX_UNLOCK(pThreadData->dataMutex);
                continue;
            }

            // Check that there aren't any pending tasks.
            if (safeBlockingQueueIsEmpty(pQueue, &taskQueueEmpty) == STATUS_SUCCESS) {
                if (taskQueueEmpty) {
                    // Check if this thread is needed to maintain minimum thread count
                    // otherwise exit loop and remove it.
                    if (stackQueueGetCount(pThreadpool->threadList, &count) == STATUS_SUCCESS) {
                        if (count > pThreadpool->minThreads) {
                            finished = TRUE;
                            if (stackQueueRemoveItem(pThreadpool->threadList, (UINT64) pThreadData) != STATUS_SUCCESS) {
                                DLOGE("Failed to remove thread data from threadpool");
                            }
                        }
                    }
                }
            }
            MUTEX_UNLOCK(pThreadpool->listMutex);
            MUTEX_UNLOCK(pThreadData->dataMutex);
        }
    }
    // now that we've released the listMutex, we can do an actual MUTEX_LOCK to ensure the
    // threadpool has finished using pThreadData
    MUTEX_LOCK(pThreadData->dataMutex);
    MUTEX_UNLOCK(pThreadData->dataMutex);

    // we assume we've already been removed from the threadList
    MUTEX_FREE(pThreadData->dataMutex);
    SAFE_MEMFREE(pThreadData);
    return NULL;
}

/**
 * Create a new threadpool
 */
STATUS threadpoolCreate(PThreadpool* ppThreadpool, UINT32 minThreads, UINT32 maxThreads)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i = 0;
    PThreadpool pThreadpool = NULL;
    CHK(ppThreadpool != NULL, STATUS_NULL_ARG);
    CHK(minThreads <= maxThreads && minThreads > 0 && maxThreads > 0, STATUS_INVALID_ARG);

    pThreadpool = (PThreadpool) MEMCALLOC(1, SIZEOF(Threadpool));
    CHK(pThreadpool != NULL, STATUS_NOT_ENOUGH_MEMORY);

    ATOMIC_STORE_BOOL(&pThreadpool->terminate, FALSE);
    ATOMIC_STORE(&pThreadpool->availableThreads, 0);

    pThreadpool->listMutex = MUTEX_CREATE(FALSE);

    CHK_STATUS(safeBlockingQueueCreate(&pThreadpool->taskQueue));

    CHK_STATUS(stackQueueCreate(&pThreadpool->threadList));

    pThreadpool->minThreads = minThreads;
    pThreadpool->maxThreads = maxThreads;
    for (i = 0; i < minThreads; i++) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

    *ppThreadpool = pThreadpool;

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        threadpoolFree(pThreadpool);
    }
    return retStatus;
}

/**
 * Creates a thread, thread data, and detaches thread
 * Adds threadpool list
 */
STATUS threadpoolInternalCreateThread(PThreadpool pThreadpool)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadData data = NULL;
    BOOL locked = FALSE, dataCreated = FALSE, mutexCreated = FALSE;
    TID thread;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    data = (PThreadData) MEMCALLOC(1, SIZEOF(ThreadData));
    CHK(data != NULL, STATUS_NOT_ENOUGH_MEMORY);
    dataCreated = TRUE;

    data->dataMutex = MUTEX_CREATE(FALSE);
    mutexCreated = TRUE;
    data->pThreadpool = pThreadpool;
    ATOMIC_STORE_BOOL(&data->terminate, FALSE);

    CHK_STATUS(stackQueueEnqueue(pThreadpool->threadList, (UINT64) data));

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

    CHK_STATUS(THREAD_CREATE_EX_EXT(&thread, "threadpoolActor", 48 * 1024, TRUE, threadpoolActor, (PVOID) data));
    CHK_STATUS(THREAD_DETACH(thread));

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }

    // If logic changes such that it's possible successfully enqueue data but not create the thread
    // We may attempt a double free.  Right now it's fine.
    if (STATUS_FAILED(retStatus)) {
        if (mutexCreated) {
            MUTEX_FREE(data->dataMutex);
        }
        if (dataCreated) {
            SAFE_MEMFREE(data);
        }
    }

    return retStatus;
}

STATUS threadpoolInternalCreateTask(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTaskData pTask = NULL;
    BOOL allocated = FALSE;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    pTask = (PTaskData) MEMCALLOC(1, SIZEOF(TaskData));
    CHK(pTask != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pTask->function = function;
    pTask->customData = customData;

    allocated = TRUE;

    CHK_STATUS(safeBlockingQueueEnqueue(pThreadpool->taskQueue, (UINT64) pTask));

CleanUp:
    if (STATUS_FAILED(retStatus) && allocated) {
        SAFE_MEMFREE(pTask);
    }

    return retStatus;
}

STATUS threadpoolInternalCanCreateThread(PThreadpool pThreadpool, PBOOL pSpaceAvailable)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadData data = NULL;
    UINT32 count = 0;
    BOOL locked = FALSE;

    CHK(pThreadpool != NULL && pSpaceAvailable != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    CHK_STATUS(stackQueueGetCount(pThreadpool->threadList, &count));

    if (count < pThreadpool->maxThreads) {
        *pSpaceAvailable = TRUE;
    } else {
        *pSpaceAvailable = FALSE;
    }

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }

    return retStatus;
}

/**
 * Destroy a threadpool
 */
STATUS threadpoolFree(PThreadpool pThreadpool)
{
    STATUS retStatus = STATUS_SUCCESS;
    StackQueueIterator iterator;
    PThreadData item = NULL;
    UINT64 data;
    UINT32 threadCount, i = 0;
    BOOL finished = FALSE, taskQueueEmpty = FALSE, listMutexLocked = FALSE, tempMutexLocked = FALSE;
    SIZE_T sentTerminationTasks = 0, finishedTerminationTasks = 0;
    MUTEX tempMutex;
    SEMAPHORE_HANDLE tempSemaphore;
    TerminationTask terminateTask;
    PTaskData pTask = NULL;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    // Threads are not forced to finish their tasks. If the user has assigned
    // a thread with an infinite loop then this threadpool object cannot safely
    // terminate it

    //------Inform all threads that Threadpool has been terminated----------

    // set terminate flag of pool -- no new threads/items can be added now
    ATOMIC_STORE_BOOL(&pThreadpool->terminate, TRUE);

    // This is used to block threadpool actors on a task so that they release all
    // mutexes the destructor will need to acquire to teardown gracefully.
    tempMutex = MUTEX_CREATE(FALSE);
    MUTEX_LOCK(tempMutex);
    CHK_STATUS(semaphoreEmptyCreate(pThreadpool->maxThreads, &tempSemaphore));
    tempMutexLocked = TRUE;

    terminateTask.mutex = tempMutex;
    terminateTask.semaphore = tempSemaphore;
    terminateTask.pCount = &finishedTerminationTasks;

    CHK_STATUS(safeBlockingQueueIsEmpty(pThreadpool->taskQueue, &taskQueueEmpty));
    if (!taskQueueEmpty) {
        CHK_STATUS(safeBlockingQueueClear(pThreadpool->taskQueue, TRUE));
    }

    while (!finished) {
        // lock list mutex
        MUTEX_LOCK(pThreadpool->listMutex);
        listMutexLocked = TRUE;

        do {
            // iterate on list
            retStatus = stackQueueGetIterator(pThreadpool->threadList, &iterator);
            if (!IS_VALID_ITERATOR(iterator)) {
                finished = TRUE;
                break;
            }

            CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));
            item = (PThreadData) data;

            if (item == NULL) {
                DLOGW("NULL thread data present on threadpool.");
                if (stackQueueRemoveItem(pThreadpool->threadList, data) != STATUS_SUCCESS) {
                    DLOGE("Failed to remove NULL thread data from threadpool");
                }
                // We use trylock to avoid a potential deadlock with the actor. The destructor needs
                // to lock listMutex and then dataMutex, but the actor locks dataMutex and then listMutex.
            } else if (MUTEX_TRYLOCK(item->dataMutex)) {
                // set terminate flag of item
                ATOMIC_STORE_BOOL(&item->terminate, TRUE);

                // when we acquire the lock, remove the item from the list. Its thread will free it.
                if (stackQueueRemoveItem(pThreadpool->threadList, data) != STATUS_SUCCESS) {
                    DLOGE("Failed to remove thread data from threadpool");
                }
                MUTEX_UNLOCK(item->dataMutex);
            } else {
                // if the mutex is taken, give each thread a waiting task, unlock list mutex, sleep 10 ms and 'start over'
                //
                // The reasoning here is that the threadActors acquire their mutex during 2 operations:
                //
                // 1. While waiting on the queue, so we need to publish a sleep task to the queue to get the actors
                // to release the mutex.
                //
                // 2. to checkfor termination, after acquiring their mutex they need the list mutex to evaluate
                // the current count and determine if they should exit or wait on the taskQueue.
                //
                // Therefore if we currently have the list mutex, but cannot acquire the item mutex they
                // must be blocking on a mutex lock for the list. When we release it and sleep we allow the
                // other thread to finish their check operation and then unlock the mutex. Next they'll see
                // the termination flag we set earlier and will exit.
                //
                // When we unlock and sleep we give them
                CHK_STATUS(stackQueueGetCount(pThreadpool->threadList, &threadCount));

                for (i = 0; i < threadCount; i++) {
                    CHK_STATUS(threadpoolInternalCreateTask(pThreadpool, threadpoolTermination, &terminateTask));
                    sentTerminationTasks++;
                }
                break;
            }
        } while (1);

        MUTEX_UNLOCK(pThreadpool->listMutex);
        listMutexLocked = FALSE;
        if (!finished) {
            // the aforementioned sleep
            THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

    // in the event that we sent termination tasks, we now need to clean up all those
    // tasks so we can safely free the associated mutex.
    while (finishedTerminationTasks < sentTerminationTasks) {
        MUTEX_UNLOCK(tempMutex);
        tempMutexLocked = FALSE;

        // if there are still items in the queue, then we need to clear them
        CHK_STATUS(safeBlockingQueueGetCount(pThreadpool->taskQueue, &i));

        if (i > 0 && safeBlockingQueueDequeue(pThreadpool->taskQueue, &data) == STATUS_SUCCESS) {
            pTask = (PTaskData) data;
            if (pTask != NULL) {
                pTask->function(pTask->customData);
                SAFE_MEMFREE(pTask);
            }
        }
        semaphoreAcquire(tempSemaphore, INFINITE_TIME_VALUE);
        MUTEX_LOCK(tempMutex);
        tempMutexLocked = TRUE;
    }

CleanUp:

    if (tempMutexLocked) {
        MUTEX_UNLOCK(tempMutex);
    }

    if (listMutexLocked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }

    // now free all the memory
    if (tempMutexLocked) {
        MUTEX_FREE(tempMutex);
    }
    semaphoreFree(&tempSemaphore);
    MUTEX_FREE(pThreadpool->listMutex);
    stackQueueFree(pThreadpool->threadList);

    // this auto kicks out all blocking calls to it
    safeBlockingQueueFree(pThreadpool->taskQueue);
    SAFE_MEMFREE(pThreadpool);

    return retStatus;
}

/**
 * Amount of threads currently tracked by this threadpool
 */
STATUS threadpoolTotalThreadCount(PThreadpool pThreadpool, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pThreadpool != NULL && pCount != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    MUTEX_LOCK(pThreadpool->listMutex);
    locked = TRUE;

    CHK_STATUS(stackQueueGetCount(pThreadpool->threadList, pCount));

    MUTEX_UNLOCK(pThreadpool->listMutex);
    locked = FALSE;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadpool->listMutex);
    }
    return retStatus;
}

/**
 * Amount of threads available to accept a new task
 */
STATUS threadpoolInternalInactiveThreadCount(PThreadpool pThreadpool, PSIZE_T pCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    SIZE_T unblockedThreads = 0;
    UINT32 pendingTasks;

    CHK(pThreadpool != NULL && pCount != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pThreadpool->terminate), STATUS_INVALID_OPERATION);

    CHK_STATUS(safeBlockingQueueGetCount(pThreadpool->taskQueue, &pendingTasks));
    unblockedThreads = (SIZE_T) ATOMIC_LOAD(&pThreadpool->availableThreads);
    *pCount = unblockedThreads > (SIZE_T) pendingTasks ? (unblockedThreads - (SIZE_T) pendingTasks) : 0;

CleanUp:
    return retStatus;
}

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created.
 *          STATUS_FAILED/ if the threadpool is already at its
 *          predetermined max.
 */
STATUS threadpoolTryAdd(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL spaceAvailable = FALSE;
    SIZE_T count = 0;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK_STATUS(threadpoolInternalCanCreateThread(pThreadpool, &spaceAvailable));
    CHK_STATUS(threadpoolInternalInactiveThreadCount(pThreadpool, &count));
    // fail if there is not an available thread or if we're already maxed out on threads
    CHK(spaceAvailable || count > 0, STATUS_THREADPOOL_MAX_COUNT);

    CHK_STATUS(threadpoolInternalCreateTask(pThreadpool, function, customData));
    // only create a thread if there aren't any inactive threads.
    if (count <= 0) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

CleanUp:
    return retStatus;
}

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created
 *          or if the task was added to the queue for the next thread.
 *          STATUS_FAILED/ if the threadpool queue is full.
 */
STATUS threadpoolPush(PThreadpool pThreadpool, startRoutine function, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL spaceAvailable = FALSE;
    SIZE_T count = 0;
    CHK(pThreadpool != NULL, STATUS_NULL_ARG);

    CHK_STATUS(threadpoolInternalCanCreateThread(pThreadpool, &spaceAvailable));
    CHK_STATUS(threadpoolInternalInactiveThreadCount(pThreadpool, &count));

    // always queue task
    CHK_STATUS(threadpoolInternalCreateTask(pThreadpool, function, customData));

    // only create a thread if there are no available threads and not maxed
    if (count <= 0 && spaceAvailable) {
        CHK_STATUS(threadpoolInternalCreateThread(pThreadpool));
    }

CleanUp:
    return retStatus;
}
