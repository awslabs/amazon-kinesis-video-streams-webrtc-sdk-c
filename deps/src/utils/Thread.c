#include "Include_i.h"

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__

//
// Thread library functions
//
PUBLIC_API TID defaultGetThreadId()
{
    return (TID) GetCurrentThread();
}

PUBLIC_API STATUS defaultGetThreadName(TID thread, PCHAR name, UINT32 len)
{
    UNUSED_PARAM(thread);
    UNUSED_PARAM(name);
    UNUSED_PARAM(len);
    return STATUS_SUCCESS;
}

PUBLIC_API DWORD WINAPI startWrapperRoutine(LPVOID data)
{
    // Get the data
    PWindowsThreadRoutineWrapper pWrapper = (PWindowsThreadRoutineWrapper) data;
    WindowsThreadRoutineWrapper wrapper;
    CHECK(NULL != pWrapper);

    // Struct-copy to store the heap allocated wrapper as the thread routine
    // can be cancelled which would lead to memory leak.
    wrapper = *pWrapper;

    // Free the heap allocated wrapper as we have a local stack copy
    MEMFREE(pWrapper);

    UINT32 retVal = (UINT32) (UINT64) wrapper.storedStartRoutine(wrapper.storedArgs);

    return retVal;
}

PUBLIC_API STATUS defaultCreateThreadWithParams(PTID pThreadId, PThreadParams pThreadParams, startRoutine start, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    HANDLE threadHandle;
    PWindowsThreadRoutineWrapper pWrapper = NULL;

    CHK(pThreadId != NULL && pThreadParams != NULL, STATUS_NULL_ARG);
    CHK(pThreadParams->version <= THREAD_PARAMS_CURRENT_VERSION, STATUS_INVALID_THREAD_PARAMS_VERSION);

    // Allocate temporary wrapper and store it
    pWrapper = (PWindowsThreadRoutineWrapper) MEMALLOC(SIZEOF(WindowsThreadRoutineWrapper));
    CHK(pWrapper != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pWrapper->storedArgs = args;
    pWrapper->storedStartRoutine = start;

    threadHandle = CreateThread(NULL, pThreadParams->stackSize, startWrapperRoutine, pWrapper, 0, NULL);
    CHK(threadHandle != NULL, STATUS_CREATE_THREAD_FAILED);

    *pThreadId = (TID) threadHandle;

CleanUp:
    if (STATUS_FAILED(retStatus) && pWrapper != NULL) {
        MEMFREE(pWrapper);
    }

    return retStatus;
}

PUBLIC_API STATUS defaultCreateThread(PTID pThreadId, startRoutine start, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    ThreadParams threadParams;
    threadParams.version = 0;

#if defined(KVS_DEFAULT_STACK_SIZE_BYTES)
    threadParams.stackSize = (SIZE_T) KVS_DEFAULT_STACK_SIZE_BYTES;
#else
    threadParams.stackSize = 0;
#endif
    CHK_STATUS(defaultCreateThreadWithParams(pThreadId, &threadParams, start, args));

CleanUp:

    return retStatus;
}

PUBLIC_API STATUS defaultJoinThread(TID threadId, PVOID* retVal)
{
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(retVal);

    CHK(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE) threadId, INFINITE), STATUS_JOIN_THREAD_FAILED);
    CloseHandle((HANDLE) threadId);

CleanUp:
    return retStatus;
}

PUBLIC_API VOID defaultThreadSleep(UINT64 time)
{
    // Time in milliseconds
    UINT64 remaining_time = time / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    // The loop will be run till the complete Sleep() time is reached
    while (remaining_time != 0) {
        // Covers the last case when there is residual time left and
        // when the value provided is less than or equal to MAX_UINT32
        if (remaining_time <= MAX_UINT32) {
            Sleep((UINT32) remaining_time);
            remaining_time = 0;
        }
        // Sleep maximum time supported by Sleep() repeatedly to cover large
        // sleep time cases
        else {
            Sleep(MAX_UINT32);
            remaining_time = remaining_time - (UINT64) MAX_UINT32;
        }
    }
}

PUBLIC_API STATUS defaultCancelThread(TID threadId)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(TerminateThread((HANDLE) threadId, 0), STATUS_CANCEL_THREAD_FAILED);

CleanUp:
    return retStatus;
}

#pragma warning(push)
#pragma warning(disable : 4102)
PUBLIC_API STATUS defaultDetachThread(TID threadId)
{
    STATUS retStatus = STATUS_SUCCESS;
    CloseHandle((HANDLE) threadId);

CleanUp:
    return retStatus;
}
#pragma warning(pop)

#else

PUBLIC_API STATUS defaultGetThreadName(TID thread, PCHAR name, UINT32 len)
{
    INT32 retValue;

    if (NULL == name) {
        return STATUS_NULL_ARG;
    }

    if (len < MAX_THREAD_NAME) {
        return STATUS_INVALID_ARG;
    }

#if defined __APPLE__ && __MACH__
    retValue = pthread_getname_np((pthread_t) thread, name, len);
#else
    // On Linux, pthread_getname_np requires _GNU_SOURCE to be defined before any system headers.
    // Use prctl(PR_GET_NAME) instead which works without _GNU_SOURCE.
    retValue = prctl(PR_GET_NAME, (UINT64) name, 0, 0, 0);
#endif

    return (0 == retValue) ? STATUS_SUCCESS : STATUS_INVALID_OPERATION;
}

PUBLIC_API TID defaultGetThreadId()
{
    return (TID) pthread_self();
}

PUBLIC_API STATUS defaultCreateThreadWithParams(PTID pThreadId, PThreadParams pThreadParams, startRoutine start, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    pthread_t threadId;
    INT32 result;
    SIZE_T stackSize;
    pthread_attr_t* pAttr = NULL;

    CHK(pThreadId != NULL && pThreadParams != NULL, STATUS_NULL_ARG); // TODO: Move to own validation function.
    CHK(pThreadParams->version <= THREAD_PARAMS_CURRENT_VERSION, STATUS_INVALID_THREAD_PARAMS_VERSION);

    stackSize = pThreadParams->stackSize;

    pthread_attr_t attr;
    if (stackSize != 0) {
        pAttr = &attr;
        result = pthread_attr_init(pAttr);
        CHK_ERR(result == 0, STATUS_THREAD_ATTR_INIT_FAILED, "pthread_attr_init failed with %d", result);
        result = pthread_attr_setstacksize(&attr, stackSize);
        CHK_ERR(result == 0, STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED, "pthread_attr_setstacksize failed with %d", result);
    }

    result = pthread_create(&threadId, pAttr, start, args);
    switch (result) {
        case 0:
            // Successful case
            break;
        case EAGAIN:
            CHK(FALSE, STATUS_THREAD_NOT_ENOUGH_RESOURCES);
        case EINVAL:
            CHK(FALSE, STATUS_THREAD_INVALID_ARG);
        case EPERM:
            CHK(FALSE, STATUS_THREAD_PERMISSIONS);
        default:
            // Generic error
            CHK(FALSE, STATUS_CREATE_THREAD_FAILED);
    }

    *pThreadId = (TID) threadId;

CleanUp:

    if (pAttr != NULL) {
        result = pthread_attr_destroy(pAttr);
        if (result != 0) {
            DLOGW("pthread_attr_destroy failed with %u", result);
        }
    }

    return retStatus;
}

PUBLIC_API STATUS defaultCreateThread(PTID pThreadId, startRoutine start, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    ThreadParams threadParams;
    threadParams.version = 0;

#if defined(KVS_DEFAULT_STACK_SIZE_BYTES) && defined(CONSTRAINED_DEVICE)
    DLOGW("KVS_DEFAULT_STACK_SIZE_BYTES and CONSTRAINED_DEVICE are both defined. KVS_DEFAULT_STACK_SIZE_BYTES will take priority.");
#endif

#if defined(KVS_DEFAULT_STACK_SIZE_BYTES)
    threadParams.stackSize = (SIZE_T) KVS_DEFAULT_STACK_SIZE_BYTES;
#elif defined(CONSTRAINED_DEVICE)
    threadParams.stackSize = THREAD_STACK_SIZE_ON_CONSTRAINED_DEVICE;
#else
    threadParams.stackSize = 0;
#endif

    CHK_STATUS(defaultCreateThreadWithParams(pThreadId, &threadParams, start, args));

CleanUp:

    return retStatus;
}

PUBLIC_API STATUS defaultJoinThread(TID threadId, PVOID* retVal)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 joinResult = pthread_join((pthread_t) threadId, retVal);

    switch (joinResult) {
        case 0:
            // Successful case
            break;
        case EDEADLK:
            CHK(FALSE, STATUS_THREAD_DEADLOCKED);
        case EINVAL:
            CHK(FALSE, STATUS_THREAD_INVALID_ARG);
        case ESRCH:
            CHK(FALSE, STATUS_THREAD_DOES_NOT_EXIST);
        default:
            // Generic error
            CHK(FALSE, STATUS_JOIN_THREAD_FAILED);
    }

CleanUp:
    return retStatus;
}

PUBLIC_API VOID defaultThreadSleep(UINT64 time)
{
    // Time in microseconds
    UINT64 remaining_time = time / HUNDREDS_OF_NANOS_IN_A_MICROSECOND;

    // The loop will be run till the complete usleep time is reached
    while (remaining_time != 0) {
        // Covers the last case when there is residual time left and
        // when the value provided is less than or equal to MAX_UINT32
        if (remaining_time <= MAX_UINT32) {
            usleep(remaining_time);
            remaining_time = 0;
        }

        // Sleep maximum time supported by usleep repeatedly to cover large
        // sleep time cases
        else {
            usleep(MAX_UINT32);
            remaining_time = remaining_time - (UINT64) MAX_UINT32;
        }
    }
}

/**
 * Android doesn't have a definition for pthread_cancel
 */
#ifdef __ANDROID__

PUBLIC_API STATUS defaultCancelThread(TID threadId)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 cancelResult = pthread_kill((pthread_t) threadId, 0);

    switch (cancelResult) {
        case 0:
            // Successful case
            break;
        case ESRCH:
            CHK(FALSE, STATUS_THREAD_DOES_NOT_EXIST);
        default:
            // Generic error
            CHK(FALSE, STATUS_CANCEL_THREAD_FAILED);
    }

CleanUp:
    return retStatus;
}

#else

PUBLIC_API STATUS defaultCancelThread(TID threadId)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 cancelResult = pthread_cancel((pthread_t) threadId);

    switch (cancelResult) {
        case 0:
            // Successful case
            break;
        case ESRCH:
            CHK(FALSE, STATUS_THREAD_DOES_NOT_EXIST);
        default:
            // Generic error
            CHK(FALSE, STATUS_CANCEL_THREAD_FAILED);
    }

CleanUp:
    return retStatus;
}

#endif

PUBLIC_API STATUS defaultDetachThread(TID threadId)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 detachResult = pthread_detach((pthread_t) threadId);

    switch (detachResult) {
        case 0:
            // Successful case
            break;
        case ESRCH:
            CHK(FALSE, STATUS_THREAD_DOES_NOT_EXIST);
        case EINVAL:
            CHK(FALSE, STATUS_THREAD_IS_NOT_JOINABLE);
        default:
            // Generic error
            CHK(FALSE, STATUS_DETACH_THREAD_FAILED);
    }

CleanUp:
    return retStatus;
}

#endif

PUBLIC_API VOID defaultThreadSleepUntil(UINT64 time)
{
    UINT64 curTime = GETTIME();

    if (time > curTime) {
        THREAD_SLEEP(time - curTime);
    }
}

getTId globalGetThreadId = defaultGetThreadId;
getTName globalGetThreadName = defaultGetThreadName;
createThread globalCreateThread = defaultCreateThread;
createThreadWithParams globalCreateThreadWithParams = defaultCreateThreadWithParams;
threadSleep globalThreadSleep = defaultThreadSleep;
threadSleepUntil globalThreadSleepUntil = defaultThreadSleepUntil;
joinThread globalJoinThread = defaultJoinThread;
cancelThread globalCancelThread = defaultCancelThread;
detachThread globalDetachThread = defaultDetachThread;
