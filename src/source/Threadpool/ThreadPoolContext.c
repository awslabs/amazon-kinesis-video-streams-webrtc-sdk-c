#define LOG_CLASS "ThreadPoolContext"
#include "../Include_i.h"

typedef struct {
    PThreadPoolContext pThreadPoolContext;
    startRoutine fn;
    PVOID customData;
} ThreadPoolContextTask, *PThreadPoolContextTask;

static PVOID trackedThreadpoolTask(PVOID customData)
{
    PThreadPoolContextTask pTask = (PThreadPoolContextTask) customData;
    PThreadPoolContext pThreadPoolContext = pTask->pThreadPoolContext;
    PVOID result = pTask->fn(pTask->customData);

    SAFE_MEMFREE(pTask);
    ATOMIC_DECREMENT(&pThreadPoolContext->outstandingTaskCount);

    return result;
}

// Function to get access to the Singleton instance
PThreadPoolContext getThreadContextInstance()
{
    static ThreadPoolContext t = {
        .pThreadpool = NULL, .isInitialized = FALSE, .shuttingDown = FALSE, .threadpoolContextLock = INVALID_MUTEX_VALUE, .outstandingTaskCount = 0};
    return &t;
}

PThreadPoolContext getReceiveThreadContextInstance()
{
    static ThreadPoolContext t = {
        .pThreadpool = NULL, .isInitialized = FALSE, .shuttingDown = FALSE, .threadpoolContextLock = INVALID_MUTEX_VALUE, .outstandingTaskCount = 0};
    return &t;
}

static STATUS createThreadPoolContextInternal(PThreadPoolContext pThreadPoolContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PCHAR pMinThreads, pMaxThreads;
    UINT32 minThreads, maxThreads;

    if (NULL == (pMinThreads = GETENV(WEBRTC_THREADPOOL_MIN_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMinThreads, NULL, 10, &minThreads)) {
        minThreads = THREADPOOL_MIN_THREADS;
    }
    if (NULL == (pMaxThreads = GETENV(WEBRTC_THREADPOOL_MAX_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMaxThreads, NULL, 10, &maxThreads)) {
        maxThreads = THREADPOOL_MAX_THREADS;
    }

    CHK_ERR(!IS_VALID_MUTEX_VALUE(pThreadPoolContext->threadpoolContextLock), STATUS_INVALID_OPERATION, "Mutex seems to have been created already");

    pThreadPoolContext->threadpoolContextLock = MUTEX_CREATE(FALSE);
    // Protecting this section to ensure we are not pushing threads / destroying the pool
    // when it is being created.
    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_WARN(!pThreadPoolContext->isInitialized, retStatus, "Threadpool already set up. Nothing to do");
    CHK_WARN(pThreadPoolContext->pThreadpool == NULL, STATUS_INVALID_OPERATION, "Threadpool object already allocated");
    CHK_STATUS(threadpoolCreate(&pThreadPoolContext->pThreadpool, minThreads, maxThreads));
    pThreadPoolContext->isInitialized = TRUE;
    pThreadPoolContext->shuttingDown = FALSE;
    ATOMIC_STORE(&pThreadPoolContext->outstandingTaskCount, 0);
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    return retStatus;
}

STATUS createThreadPoolContext()
{
    return createThreadPoolContextInternal(getThreadContextInstance());
}

STATUS createReceiveThreadPoolContext()
{
    return createThreadPoolContextInternal(getReceiveThreadContextInstance());
}

static STATUS threadpoolContextPushInternal(PThreadPoolContext pThreadPoolContext, startRoutine fn, PVOID customData, BOOL trackTask)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PThreadPoolContextTask pTask = NULL;

    // Protecting this section to ensure we are destroying the pool
    // when it is being used.
    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_ERR(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet");
    CHK_ERR(!pThreadPoolContext->shuttingDown, STATUS_INVALID_OPERATION, "Threadpool is shutting down");
    CHK_ERR(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Threadpool object is NULL");
    if (trackTask) {
        pTask = (PThreadPoolContextTask) MEMCALLOC(1, SIZEOF(ThreadPoolContextTask));
        CHK(pTask != NULL, STATUS_NOT_ENOUGH_MEMORY);
        pTask->pThreadPoolContext = pThreadPoolContext;
        pTask->fn = fn;
        pTask->customData = customData;
        ATOMIC_INCREMENT(&pThreadPoolContext->outstandingTaskCount);
        retStatus = threadpoolPush(pThreadPoolContext->pThreadpool, trackedThreadpoolTask, pTask);
    } else {
        retStatus = threadpoolPush(pThreadPoolContext->pThreadpool, fn, customData);
    }
    CHK_STATUS(retStatus);
CleanUp:
    if (STATUS_FAILED(retStatus) && pTask != NULL) {
        ATOMIC_DECREMENT(&pThreadPoolContext->outstandingTaskCount);
        SAFE_MEMFREE(pTask);
    }
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    return retStatus;
}

STATUS threadpoolContextPush(startRoutine fn, PVOID customData)
{
    return threadpoolContextPushInternal(getThreadContextInstance(), fn, customData, FALSE);
}

STATUS receiveThreadpoolContextPush(startRoutine fn, PVOID customData)
{
    return threadpoolContextPushInternal(getReceiveThreadContextInstance(), fn, customData, TRUE);
}

static STATUS destroyThreadPoolContextInternal(PThreadPoolContext pThreadPoolContext, BOOL awaitOutstandingTasks)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    // Ensure we do not destroy the pool if threads are still being pushed
    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_WARN(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet, nothing to destroy");
    CHK_WARN(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Destroying threadpool without setting up");
    pThreadPoolContext->shuttingDown = TRUE;
    MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    locked = FALSE;

    while (awaitOutstandingTasks && ATOMIC_LOAD(&pThreadPoolContext->outstandingTaskCount) > 0) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    threadpoolFree(pThreadPoolContext->pThreadpool);

    // All members of the static instance **MUST** be reset after destruction to allow for
    // the static object to be re-created after destruction (more relevant for unit tests)
    pThreadPoolContext->pThreadpool = NULL;
    pThreadPoolContext->isInitialized = FALSE;
    pThreadPoolContext->shuttingDown = FALSE;
    ATOMIC_STORE(&pThreadPoolContext->outstandingTaskCount, 0);
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    if (IS_VALID_MUTEX_VALUE(pThreadPoolContext->threadpoolContextLock)) {
        MUTEX_FREE(pThreadPoolContext->threadpoolContextLock);

        // Important to reset, specifically in case of unit tests where initKvsWebRtc() and
        // deinitKvsWebRtc() is invoked before and after every test suite
        pThreadPoolContext->threadpoolContextLock = INVALID_MUTEX_VALUE;
    }
    return retStatus;
}

STATUS destroyThreadPoolContext()
{
    return destroyThreadPoolContextInternal(getThreadContextInstance(), FALSE);
}

STATUS destroyReceiveThreadPoolContext()
{
    return destroyThreadPoolContextInternal(getReceiveThreadContextInstance(), TRUE);
}