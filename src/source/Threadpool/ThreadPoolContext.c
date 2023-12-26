#define LOG_CLASS "ThreadPoolContext"
#include "../Include_i.h"

// Function to get access to the Singleton instance
PThreadPoolContext getThreadContextInstance()
{
    static ThreadPoolContext t = {.pThreadpool = NULL, .isInitialized = FALSE, .threadpoolContextLock = INVALID_MUTEX_VALUE};
    return &t;
}

STATUS createThreadPoolContext()
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PCHAR pMinThreads, pMaxThreads;
    UINT32 minThreads, maxThreads;

    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();

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
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    return retStatus;
}

STATUS threadpoolContextPush(startRoutine fn, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();

    // Protecting this section to ensure we are destroying the pool
    // when it is being used.
    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_ERR(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet");
    CHK_ERR(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Threadpool object is NULL");
    CHK_STATUS(threadpoolPush(pThreadPoolContext->pThreadpool, fn, customData));
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    return retStatus;
}

STATUS destroyThreadPoolContext()
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();

    // Ensure we do not destroy the pool if threads are still being pushed
    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_WARN(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet, nothing to destroy");
    CHK_WARN(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Destroying threadpool without setting up");
    threadpoolFree(pThreadPoolContext->pThreadpool);

    // All members of the static instance **MUST** be reset after destruction to allow for
    // the static object to be re-created after destruction (more relevant for unit tests)
    pThreadPoolContext->pThreadpool = NULL;
    pThreadPoolContext->isInitialized = FALSE;
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
};