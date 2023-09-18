#define LOG_CLASS "ThreadPoolContext"
#include "../Include_i.h"

// Function to get access to the Singleton instance
PThreadpool getThreadpoolInstance()
{
    static Threadpool t;
    return &t;
}

STATUS webRtcCreateThreadPool()
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadpool pThreadpool = getThreadpoolInstance();
    PCHAR pMinThreads, pMaxThreads;
    UINT32 minThreads, maxThreads;
    if (NULL == (pMinThreads = GETENV(WEBRTC_THREADPOOL_MIN_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMinThreads, NULL, 10, &minThreads)) {
        minThreads = THREADPOOL_MIN_THREADS;
    }
    if (NULL == (pMaxThreads = GETENV(WEBRTC_THREADPOOL_MAX_THREADS_ENV_VAR)) || STATUS_SUCCESS != STRTOUI32(pMaxThreads, NULL, 10, &maxThreads)) {
        maxThreads = THREADPOOL_MAX_THREADS;
    }
    CHK_STATUS(threadpoolCreate(&pThreadpool, minThreads, maxThreads));
CleanUp:
    return retStatus;
}

STATUS webRtcThreadPoolPush(startRoutine fn, PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadpool pThreadpool = getThreadpoolInstance();
    CHK_ERR(pThreadpool != NULL, STATUS_NULL_ARG, "Threadpool not set up");
    CHK_STATUS(threadpoolPush(pThreadpool, fn, customData));
CleanUp:
    return retStatus;
}

STATUS webRtcDestroyThreadPool()
{
    STATUS retStatus = STATUS_SUCCESS;
    PThreadpool pThreadpool = getThreadpoolInstance();
    CHK_WARN(pThreadpool != NULL, STATUS_NULL_ARG, "Destroying threadpool without setting up");
    threadpoolFree(pThreadpool);
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
CleanUp:
    return retStatus;
};