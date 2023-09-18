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
    CHK_STATUS(threadpoolCreate(&pThreadPoolContext->pThreadpool, THREADPOOL_MIN_THREADS, THREADPOOL_MAX_THREADS));
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
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
CleanUp:
    return retStatus;
};