#define LOG_CLASS "ThreadPoolContext"
#include "../Include_i.h"

// Function to get access to the Singleton instance
PThreadPoolContext getThreadContextInstance()
{
    static ThreadPoolContext t;
    return &t;
}

STATUS createThreadPoolContext()
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();
    PCHAR pMinThreads, pMaxThreads;
    UINT32 minThreads, maxThreads;
    CHK_WARN(pThreadPoolContext->pThreadpool == NULL, retStatus, "Threadpool already set up. Nothing to do");
    if (NULL == (pMinThreads = GETENV(WEBRTC_THREADPOOL_MIN_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMinThreads, NULL, 10, &minThreads)) {
        minThreads = THREADPOOL_MIN_THREADS;
    }
    if (NULL == (pMaxThreads = GETENV(WEBRTC_THREADPOOL_MAX_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMaxThreads, NULL, 10, &maxThreads)) {
        maxThreads = THREADPOOL_MAX_THREADS;
    }
    CHK_STATUS(threadpoolCreate(&pThreadPoolContext->pThreadpool, minThreads, maxThreads));
CleanUp:
    return retStatus;
}

STATUS threadpoolContextPush(startRoutine fn, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();
    CHK_ERR(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Threadpool not set up");
    CHK_STATUS(threadpoolPush(pThreadPoolContext->pThreadpool, fn, customData));
CleanUp:
    return retStatus;
}

STATUS destroyThreadPoolContext()
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadPoolContext pThreadPoolContext = getThreadContextInstance();
    CHK_WARN(pThreadPoolContext->pThreadpool != NULL, STATUS_NULL_ARG, "Destroying threadpool without setting up");
    threadpoolFree(pThreadPoolContext->pThreadpool);
CleanUp:
    return retStatus;
};