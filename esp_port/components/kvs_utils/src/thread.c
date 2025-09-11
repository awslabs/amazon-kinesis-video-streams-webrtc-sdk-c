/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "thread.h"
#include "esp_pthread.h"

#if defined _WIN32 || defined _WIN64 || defined __CYGWIN__
/**
 * Thread wrapper for Windows
 */
typedef struct {
    // Stored routine
    startRoutine storedStartRoutine;

    // Original arguments
    PVOID storedArgs;
} WindowsThreadRoutineWrapper, *PWindowsThreadRoutineWrapper;

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

    UINT32 retVal = (UINT32)(UINT64) wrapper.storedStartRoutine(wrapper.storedArgs);

    return retVal;
}

PUBLIC_API STATUS defaultCreateThread(PTID pThreadId, startRoutine start, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    HANDLE threadHandle;
    PWindowsThreadRoutineWrapper pWrapper = NULL;

    CHK(pThreadId != NULL, STATUS_NULL_ARG);

    // Allocate temporary wrapper and store it
    pWrapper = (PWindowsThreadRoutineWrapper) MEMALLOC(SIZEOF(WindowsThreadRoutineWrapper));
    CHK(pWrapper != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pWrapper->storedArgs = args;
    pWrapper->storedStartRoutine = start;

    threadHandle = CreateThread(NULL, 0, startWrapperRoutine, pWrapper, 0, NULL);
    CHK(threadHandle != NULL, STATUS_CREATE_THREAD_FAILED);

    *pThreadId = (TID) threadHandle;

CleanUp:
    if (STATUS_FAILED(retStatus) && pWrapper != NULL) {
        MEMFREE(pWrapper);
    }

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

UINT32 totalNum = 0;
UINT32 successNum = 0;

PUBLIC_API STATUS defaultGetThreadName(TID thread, PCHAR name, UINT32 len)
{
    UINT32 retValue = STATUS_SUCCESS;

    if (NULL == name) {
        return STATUS_NULL_ARG;
    }

    if (len < MAX_THREAD_NAME) {
        return STATUS_INVALID_ARG;
    }

#if defined __APPLE__ && __MACH__ || __GLIBC__ && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 12
    retValue = pthread_getname_np((pthread_t) thread, name, len);
#elif defined ANDROID_BUILD
// This will return the current thread name on Android
#ifdef KVSPIC_HAVE_SYS_PRCTL_H
    retValue = prctl(PR_GET_NAME, (UINT64) name, 0, 0, 0);
#endif
#else
// TODO need to handle case when other platform use old verison GLIBC and don't support prctl
#ifdef KVSPIC_HAVE_SYS_PRCTL_H
    retValue = prctl(PR_GET_NAME, (UINT64) name, 0, 0, 0);
#endif
#endif

    return (0 == retValue) ? STATUS_SUCCESS : STATUS_INVALID_OPERATION;
}

PUBLIC_API TID defaultGetThreadId()
{
    // return (TID) pthread_self();
    return (TID) 1234; // some random number for now
}

STATUS defaultCreateThreadPriWithCaps(PTID pThreadId, PCHAR threadName, UINT32 threadSize, UINT32 caps, BOOL joinable, startRoutine start, UINT32 prio, PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    pthread_t threadId;
    INT32 result;
    pthread_attr_t* pAttr = NULL;
    pthread_attr_t attr;
    pAttr = &attr;
    CHK(pThreadId != NULL, STATUS_NULL_ARG);
    result = pthread_attr_init(pAttr);

#if CONSTRAINED_DEVICE
    pthread_attr_t attr;
    pAttr = &attr;
    result = pthread_attr_init(pAttr);
    CHK_ERR(result == 0, STATUS_THREAD_ATTR_INIT_FAILED, "pthread_attr_init failed with %d", result);
    result = pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE_ON_CONSTRAINED_DEVICE);
    CHK_ERR(result == 0, STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED, "pthread_attr_setstacksize failed with %d", result);
#endif

#if 1 // defined(KVS_PLAT_ESP_FREERTOS)
    // UINT32 totalSize = esp_get_free_heap_size();
    // UINT32 spiSize = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    // UINT32 internalSize = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    esp_pthread_cfg_t pthread_cfg = esp_pthread_get_default_config();
    esp_err_t esp_err = ESP_OK;
    // esp_err_t esp_err = esp_pthread_get_cfg(&pthread_cfg);
    // if (esp_err != ESP_OK) {
    //     DLOGW("get the esp pthread cfg failed.");
    // }

    if (threadSize == 0) {
        pthread_cfg.stack_size = DEFAULT_THREAD_SIZE;
    } else {
        pthread_cfg.stack_size = threadSize;
    }

    if (threadName == NULL) {
        pthread_cfg.thread_name = DEFAULT_THREAD_NAME;
    } else {
        pthread_cfg.thread_name = threadName;
    }

    if (caps != 0) {
        pthread_cfg.stack_alloc_caps = caps;
    }

    ESP_LOGI("Thread", "pthread_cfg.thread_name: %s", pthread_cfg.thread_name);
    ESP_LOGI("Thread", "pthread_cfg.stack_size: %d", (int) pthread_cfg.stack_size);
    ESP_LOGI("Thread", "pthread_cfg.stack_alloc_caps: %d", (int) pthread_cfg.stack_alloc_caps);

    // pthread_cfg.stack_alloc_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    esp_err = esp_pthread_set_cfg(&pthread_cfg);
    if (esp_err != ESP_OK) {
        DLOGW("set the esp pthread cfg failed.");
    }
    if (joinable == TRUE) {
        pthread_attr_setdetachstate(pAttr, PTHREAD_CREATE_JOINABLE);
    } else {
        pthread_attr_setdetachstate(pAttr, PTHREAD_CREATE_DETACHED);
    }
    ESP_LOGI("Thread", "pthread_attr_setdetachstate finished");
    if (threadSize == 0) {
        pthread_attr_setstacksize(pAttr, DEFAULT_THREAD_SIZE);
    } else {
        pthread_attr_setstacksize(pAttr, threadSize);
    }

    ESP_LOGI("Thread", "pthread_attr_setstacksize finished");

    esp_err = esp_pthread_set_cfg(&pthread_cfg);
    if (esp_err != ESP_OK) {
        DLOGW("set the esp pthread cfg failed.");
    }
#endif

    result = pthread_create(&threadId, pAttr, start, args);
    ESP_LOGI("Thread", "pthread_create finished, result: %d", (int) result);

#if defined(KVS_PLAT_ESP_FREERTOS)
    UINT32 curTotalSize = esp_get_free_heap_size();
    UINT32 curSpiSize = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    UINT32 curInternalSize = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    DLOGD("pthread:%s, size:%d requires ram, totalSize:%d, spiSize:%d, internalSize:%d", threadName, threadSize, totalSize - curTotalSize,
          spiSize - curSpiSize, internalSize - curInternalSize);
#endif
    switch (result) {
        case 0:
            // Successful case
            break;
        case ENOMEM:
            ESP_LOGE("Thread", "pthread_create failed with ENOMEM");
            CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
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
    successNum++;
CleanUp:

    CHK_LOG_ERR(retStatus);

    totalNum++;
    DLOGD("The number of threads: (%d/%d)", successNum, totalNum);
    if (pAttr != NULL) {
        result = pthread_attr_destroy(pAttr);
        if (result != 0) {
            DLOGW("pthread_attr_destroy failed with %u", result);
        }
    }

    return retStatus;
}


STATUS defaultCreateThreadPriExt(PTID pThreadId, PCHAR threadName, UINT32 threadSize, BOOL joinable, startRoutine start, UINT32 prio, PVOID args)
{
    return defaultCreateThreadPriWithCaps(pThreadId, threadName, threadSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, joinable, start, prio, args);
}

STATUS defaultCreateThreadPri(PTID pThreadId, PCHAR threadName, UINT32 threadSize, BOOL joinable, startRoutine start, UINT32 prio, PVOID args)
{
    return defaultCreateThreadPriWithCaps(pThreadId, threadName, threadSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, joinable, start, prio, args);
}

PUBLIC_API STATUS defaultCreateThreadEx(PTID pThreadId, PCHAR threadName, UINT32 threadSize, BOOL joinable, startRoutine start, PVOID args)
{
    return defaultCreateThreadPri(pThreadId, threadName, threadSize, joinable, start, CONFIG_PTHREAD_TASK_PRIO_DEFAULT, args);
}

PUBLIC_API STATUS defaultCreateThreadExExt(PTID pThreadId, PCHAR threadName, UINT32 threadSize, BOOL joinable, startRoutine start, PVOID args)
{
    return defaultCreateThreadPriExt(pThreadId, threadName, threadSize, joinable, start, CONFIG_PTHREAD_TASK_PRIO_DEFAULT, args);
}

PUBLIC_API STATUS defaultCreateThread(PTID pThreadId, startRoutine start, PVOID args)
{
    return defaultCreateThreadEx(pThreadId, NULL, 0, TRUE, start, args);
}

PUBLIC_API STATUS defaultCreateThreadExPri(PTID pThreadId, PCHAR threadName, UINT32 threadSize, BOOL joinable, startRoutine start, INT32 prio, PVOID args)
{
    return defaultCreateThreadPri(pThreadId, threadName, threadSize, joinable, start, prio, args);
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
#ifdef ANDROID_BUILD

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

PUBLIC_API VOID defaultExitThread(PVOID valuePtr)
{
    pthread_exit(valuePtr);
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
createThreadEx globalCreateThreadEx = defaultCreateThreadEx;
createThreadExExt globalCreateThreadExExt = defaultCreateThreadExExt;
createThreadExPri globalCreateThreadExPri = defaultCreateThreadExPri;
threadSleep globalThreadSleep = defaultThreadSleep;
threadSleepUntil globalThreadSleepUntil = defaultThreadSleepUntil;
joinThread globalJoinThread = defaultJoinThread;
cancelThread globalCancelThread = defaultCancelThread;
detachThread globalDetachThread = defaultDetachThread;
exitThread globalExitThread = defaultExitThread;
