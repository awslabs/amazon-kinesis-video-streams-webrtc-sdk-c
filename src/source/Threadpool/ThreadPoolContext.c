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

    // The mutex is process-lifetime: created once, never freed. This eliminates
    // the TOCTOU race where threadpoolContextPush checks IS_VALID_MUTEX_VALUE
    // and then destroyThreadPoolContext frees the mutex before the lock.
    if (!IS_VALID_MUTEX_VALUE(pThreadPoolContext->threadpoolContextLock)) {
        pThreadPoolContext->threadpoolContextLock = MUTEX_CREATE(FALSE);
    }

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
    UINT32 threadCount = 0;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();

    // The mutex is process-lifetime (never freed), so if it is invalid,
    // createThreadPoolContext was never called in this process.
    CHK_ERR(IS_VALID_MUTEX_VALUE(pThreadPoolContext->threadpoolContextLock), STATUS_INVALID_OPERATION, "Threadpool context never initialized");

    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_ERR(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet");
    CHK_ERR(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Threadpool object is NULL");

    // Warn when the pool is near saturation, rate-limited to once per 30 s.
    // All task types (DTLS handshakes, signaling callbacks, STUN resolution)
    // share this pool. See docs/THREADPOOL.md for sizing guidance.
    if (STATUS_SUCCEEDED(threadpoolTotalThreadCount(pThreadPoolContext->pThreadpool, &threadCount))) {
        static UINT64 lastSaturationWarningTime = 0;
        PCHAR pMaxThreads;
        UINT32 maxThreads;
        UINT64 now;
        if (NULL == (pMaxThreads = GETENV(WEBRTC_THREADPOOL_MAX_THREADS_ENV_VAR)) ||
            STATUS_SUCCESS != STRTOUI32(pMaxThreads, NULL, 10, &maxThreads)) {
            maxThreads = THREADPOOL_MAX_THREADS;
        }
        if (threadCount >= maxThreads) {
            now = GETTIME();
            if (now >= lastSaturationWarningTime + 30 * HUNDREDS_OF_NANOS_IN_A_SECOND) {
                DLOGW("Thread pool at capacity (%u/%u). Tasks will queue. "
                      "Consider increasing AWS_KVS_WEBRTC_THREADPOOL_MAX_THREADS. "
                      "See docs/THREADPOOL.md for sizing guidance.",
                      threadCount, maxThreads);
                lastSaturationWarningTime = now;
            }
        }
    }

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

    CHK_ERR(IS_VALID_MUTEX_VALUE(pThreadPoolContext->threadpoolContextLock), STATUS_INVALID_OPERATION,
            "Threadpool context never initialized, nothing to destroy");

    MUTEX_LOCK(pThreadPoolContext->threadpoolContextLock);
    locked = TRUE;
    CHK_WARN(pThreadPoolContext->isInitialized, STATUS_INVALID_OPERATION, "Threadpool not initialized yet, nothing to destroy");
    CHK_WARN(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Destroying threadpool without setting up");
    threadpoolFree(pThreadPoolContext->pThreadpool);

    // Reset pool state but keep the mutex alive — it is process-lifetime so
    // concurrent threadpoolContextPush callers always have a valid lock.
    pThreadPoolContext->pThreadpool = NULL;
    pThreadPoolContext->isInitialized = FALSE;
CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pThreadPoolContext->threadpoolContextLock);
    }
    return retStatus;
};