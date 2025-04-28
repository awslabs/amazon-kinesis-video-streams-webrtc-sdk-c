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
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "mutex.h"
#include "timer_queue.h"
#include "thread.h"

/******************************************************************************
 * DEFINITION
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
// Internal Functions
STATUS priv_timer_queue_createInternal(UINT32, PTimerQueue*);
STATUS priv_timer_queue_createInternalEx(UINT32, PTimerQueue*, PCHAR, UINT32);
STATUS priv_timer_queue_freeInternal(PTimerQueue*);
STATUS priv_timer_queue_evaluateNextInvocation(PTimerQueue);

STATUS timerQueueCreateEx(PTIMER_QUEUE_HANDLE pHandle, PCHAR timerName, UINT32 threadSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = NULL;

    CHK(pHandle != NULL, STATUS_NULL_ARG);

    CHK_STATUS(priv_timer_queue_createInternalEx(DEFAULT_TIMER_QUEUE_TIMER_COUNT, &pTimerQueue, timerName, threadSize));

    *pHandle = TO_TIMER_QUEUE_HANDLE(pTimerQueue);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        priv_timer_queue_freeInternal(&pTimerQueue);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueCreate(PTIMER_QUEUE_HANDLE pHandle)
{
    return timerQueueCreateEx(pHandle, NULL, 0);
}

STATUS timerQueueFree(PTIMER_QUEUE_HANDLE pHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue;

    CHK(pHandle != NULL, STATUS_NULL_ARG);

    // Get the client handle
    pTimerQueue = FROM_TIMER_QUEUE_HANDLE(*pHandle);

    CHK_STATUS(priv_timer_queue_freeInternal(&pTimerQueue));

    // Set the handle pointer to invalid
    *pHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS timerQueueAddTimer(TIMER_QUEUE_HANDLE handle, UINT64 start, UINT64 period, TimerCallbackFunc timerCallbackFn, UINT64 customData,
                            PUINT32 pIndex)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;
    UINT32 i, retIndex = 0;
    PTimerEntry pTimerEntry = NULL;

    CHK(pTimerQueue != NULL && timerCallbackFn != NULL && pIndex != NULL, STATUS_NULL_ARG);
    CHK(period == TIMER_QUEUE_SINGLE_INVOCATION_PERIOD || period >= MIN_TIMER_QUEUE_PERIOD_DURATION, STATUS_INVALID_TIMER_PERIOD_VALUE);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    CHK_WARN(pTimerQueue->activeTimerCount < pTimerQueue->maxTimerCount, STATUS_MAX_TIMER_COUNT_REACHED, "reach the limit of timer");

    // Get an available index
    for (i = 0; i < pTimerQueue->maxTimerCount && pTimerEntry == NULL; i++) {
        if (pTimerQueue->pTimers[i].timerCallbackFn == NULL) {
            pTimerEntry = &pTimerQueue->pTimers[i];
            retIndex = i;
        }
    }

    // Increment the count and set the timer entries
    pTimerQueue->activeTimerCount++;

    pTimerEntry->timerCallbackFn = timerCallbackFn;
    pTimerEntry->customData = customData;
    pTimerEntry->invokeTime = GETTIME() + start;
    pTimerEntry->period = period;

    if (pTimerEntry->invokeTime < pTimerQueue->invokeTime) {
        // Need to update the scheduled invoke at this time
        pTimerQueue->invokeTime = pTimerEntry->invokeTime;

        // Signal the executor to wake up and re-evaluate
        CVAR_SIGNAL(pTimerQueue->executorCvar);
    }

    *pIndex = retIndex;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueCancelTimer(TIMER_QUEUE_HANDLE handle, UINT32 timerId, UINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;
    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);
    CHK(timerId < pTimerQueue->maxTimerCount, STATUS_INVALID_ARG);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    // Check if anything needs to be done
    CHK(pTimerQueue->activeTimerCount != 0 && pTimerQueue->pTimers[timerId].timerCallbackFn != NULL &&
            customData == pTimerQueue->pTimers[timerId].customData,
        retStatus);

    // Setting the callback to NULL to indicate empty timer
    pTimerQueue->pTimers[timerId].timerCallbackFn = NULL;

    // Decrement the count
    pTimerQueue->activeTimerCount--;

    // Check if the next invocation needs to change
    if (pTimerQueue->pTimers[timerId].invokeTime == pTimerQueue->invokeTime) {
        // Re-evaluate the new invocation
        CHK_STATUS(priv_timer_queue_evaluateNextInvocation(pTimerQueue));

        // Signal the executor to wake up and re-evaluate
        CVAR_SIGNAL(pTimerQueue->executorCvar);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueCancelTimersWithCustomData(TIMER_QUEUE_HANDLE handle, UINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;
    UINT32 timerId;

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    // cancel all timer with customData
    for (timerId = 0; timerId < pTimerQueue->maxTimerCount; timerId++) {
        if (pTimerQueue->pTimers[timerId].customData == customData && pTimerQueue->pTimers[timerId].timerCallbackFn != NULL) {
            CHK_STATUS(timerQueueCancelTimer(handle, timerId, customData));
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueCancelAllTimers(TIMER_QUEUE_HANDLE handle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;
    UINT32 timerId;

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    // cancel all timer
    for (timerId = 0; timerId < pTimerQueue->maxTimerCount; timerId++) {
        if (pTimerQueue->pTimers[timerId].timerCallbackFn != NULL) {
            CHK_STATUS(timerQueueCancelTimer(handle, timerId, pTimerQueue->pTimers[timerId].customData));
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueGetTimerCount(TIMER_QUEUE_HANDLE handle, PUINT32 pTimerCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;

    CHK(pTimerQueue != NULL && pTimerCount != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    *pTimerCount = pTimerQueue->activeTimerCount;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueGetTimersWithCustomData(TIMER_QUEUE_HANDLE handle, UINT64 customData, PUINT32 pTimerIdCount, PUINT32 pTimerIdsBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;
    UINT32 timerId, timerIdCount = 0;

    CHK(pTimerQueue != NULL && pTimerIdCount != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    // first pass to get the timer id count
    for (timerId = 0; timerId < pTimerQueue->maxTimerCount; timerId++) {
        if (pTimerQueue->pTimers[timerId].customData == customData && pTimerQueue->pTimers[timerId].timerCallbackFn != NULL) {
            timerIdCount++;
        }
    }

    if (pTimerIdsBuffer != NULL) {
        // pTimerIdCount should store the buffer size of buffer is not NULL.
        CHK(*pTimerIdCount >= timerIdCount, STATUS_BUFFER_TOO_SMALL);
    }

    *pTimerIdCount = timerIdCount;
    // return early if client wants just timer id count
    CHK(pTimerIdsBuffer != NULL, retStatus);

    // second pass to store the timer ids
    for (timerId = 0, timerIdCount = 0; timerId < pTimerQueue->maxTimerCount; timerId++) {
        if (pTimerQueue->pTimers[timerId].customData == customData && pTimerQueue->pTimers[timerId].timerCallbackFn != NULL) {
            pTimerIdsBuffer[timerIdCount] = timerId;
            timerIdCount++;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueUpdateTimerPeriod(TIMER_QUEUE_HANDLE handle, UINT64 customData, UINT32 timerId, UINT64 period)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);
    BOOL locked = FALSE;

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);
    CHK(timerId < pTimerQueue->maxTimerCount, STATUS_INVALID_ARG);
    // Check if anything needs to be done
    CHK(pTimerQueue->activeTimerCount != 0 && pTimerQueue->pTimers[timerId].timerCallbackFn != NULL &&
            customData == pTimerQueue->pTimers[timerId].customData,
        retStatus);
    CHK(period == TIMER_QUEUE_SINGLE_INVOCATION_PERIOD || period >= MIN_TIMER_QUEUE_PERIOD_DURATION, STATUS_INVALID_TIMER_PERIOD_VALUE);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;

    pTimerQueue->pTimers[timerId].period = period;
    // take effect immediately
    pTimerQueue->pTimers[timerId].invokeTime = GETTIME() + period;
    CHK_STATUS(priv_timer_queue_evaluateNextInvocation(pTimerQueue));
    CVAR_SIGNAL(pTimerQueue->executorCvar);

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    LEAVES();
    return retStatus;
}

STATUS timerQueueShutdown(TIMER_QUEUE_HANDLE handle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = FROM_TIMER_QUEUE_HANDLE(handle);

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pTimerQueue->shutdown, TRUE);
    CVAR_SIGNAL(pTimerQueue->executorCvar);

CleanUp:

    LEAVES();
    return retStatus;
}

/////////////////////////////////////////////////////////////////////////////////
// Internal operations
/////////////////////////////////////////////////////////////////////////////////
STATUS priv_timer_queue_createInternalEx(UINT32 maxTimers, PTimerQueue* ppTimerQueue, PCHAR timerName, UINT32 threadSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = NULL;
    UINT32 allocSize;
    BOOL locked = FALSE;
    TID threadId;

    CHK(ppTimerQueue != NULL, STATUS_NULL_ARG);
    CHK(maxTimers >= MIN_TIMER_QUEUE_TIMER_COUNT, STATUS_INVALID_TIMER_COUNT_VALUE);

    allocSize = SIZEOF(TimerQueue) + maxTimers * SIZEOF(TimerEntry);
    CHK(NULL != (pTimerQueue = (PTimerQueue) MEMCALLOC(1, allocSize)), STATUS_NOT_ENOUGH_MEMORY);
    pTimerQueue->activeTimerCount = 0;
    pTimerQueue->maxTimerCount = maxTimers;
    pTimerQueue->executorTid = INVALID_TID_VALUE;
    ATOMIC_STORE_BOOL(&pTimerQueue->terminated, FALSE);
    ATOMIC_STORE_BOOL(&pTimerQueue->started, FALSE);
    ATOMIC_STORE_BOOL(&pTimerQueue->shutdown, FALSE);
    pTimerQueue->invokeTime = MAX_UINT64;

    pTimerQueue->startLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pTimerQueue->startLock), STATUS_INVALID_OPERATION);
    pTimerQueue->startCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pTimerQueue->startCvar), STATUS_INVALID_OPERATION);

    pTimerQueue->exitLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pTimerQueue->exitLock), STATUS_INVALID_OPERATION);
    pTimerQueue->exitCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pTimerQueue->exitCvar), STATUS_INVALID_OPERATION);

    // executor lock has to be reentrant because timer callbacks can call other timer queue apis.
    pTimerQueue->executorLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pTimerQueue->executorLock), STATUS_INVALID_OPERATION);
    pTimerQueue->executorCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pTimerQueue->executorCvar), STATUS_INVALID_OPERATION);

    // Set the timer entry array past the end of the main allocation
    pTimerQueue->pTimers = (PTimerEntry)(pTimerQueue + 1);

    // Block threads start
    MUTEX_LOCK(pTimerQueue->startLock);
    locked = TRUE;

    // Create the executor thread
    CHK_STATUS(THREAD_CREATE_EX_EXT(&threadId, timerName, threadSize, FALSE, timer_queue_executor, (PVOID) pTimerQueue));
    // #define TIMER_QUEUE_TASK_PRIO    6
    // CHK_STATUS(THREAD_CREATE_EX_PRI(&threadId, timerName, threadSize, FALSE, timer_queue_executor, TIMER_QUEUE_TASK_PRIO, (PVOID) pTimerQueue));
    pTimerQueue->executorTid = threadId;

    while (!ATOMIC_LOAD_BOOL(&pTimerQueue->started)) {
        CHK_STATUS(CVAR_WAIT(pTimerQueue->startCvar, pTimerQueue->startLock, INFINITE_TIME_VALUE));
    }
    MUTEX_UNLOCK(pTimerQueue->startLock);
    locked = FALSE;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->startLock);
    }

    if (STATUS_FAILED(retStatus)) {
        priv_timer_queue_freeInternal(&pTimerQueue);
    }

    if (ppTimerQueue != NULL) {
        *ppTimerQueue = pTimerQueue;
    }

    LEAVES();
    return retStatus;
}

STATUS priv_timer_queue_createInternal(UINT32 maxTimers, PTimerQueue* ppTimerQueue)
{
    return priv_timer_queue_createInternalEx(maxTimers, ppTimerQueue, NULL, 0);
}

STATUS priv_timer_queue_freeInternal(PTimerQueue* ppTimerQueue)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue;
    BOOL iterate = TRUE, killThread = FALSE;

    CHK(ppTimerQueue != NULL, STATUS_NULL_ARG);

    pTimerQueue = *ppTimerQueue;
    CHK(pTimerQueue != NULL, retStatus);

    // Attempt to terminate the executor loop if we have fully constructed mutexes and cvars
    if (IS_VALID_CVAR_VALUE(pTimerQueue->executorCvar) && IS_VALID_CVAR_VALUE(pTimerQueue->exitCvar) && IS_VALID_CVAR_VALUE(pTimerQueue->startCvar) &&
        IS_VALID_MUTEX_VALUE(pTimerQueue->exitLock) && IS_VALID_MUTEX_VALUE(pTimerQueue->startLock) &&
        IS_VALID_MUTEX_VALUE(pTimerQueue->executorLock)) {
        // Terminate the executor thread
        ATOMIC_STORE_BOOL(&pTimerQueue->shutdown, TRUE);

        // Signal the executor to wake up and quit
        MUTEX_LOCK(pTimerQueue->executorLock);
        CVAR_SIGNAL(pTimerQueue->executorCvar);
        MUTEX_UNLOCK(pTimerQueue->executorLock);

        MUTEX_LOCK(pTimerQueue->exitLock);
        while (iterate && !ATOMIC_LOAD_BOOL(&pTimerQueue->terminated)) {
            retStatus = CVAR_WAIT(pTimerQueue->exitCvar, pTimerQueue->exitLock, TIMER_QUEUE_SHUTDOWN_TIMEOUT);

            if (STATUS_FAILED(retStatus)) {
                DLOGW("Awaiting for the executor to quit failed with 0x%08x", retStatus);

                // Terminate the loop and kill the thread
                iterate = FALSE;
                killThread = TRUE;
            }

            // Reset the return
            retStatus = STATUS_SUCCESS;
        }

        // Kill the thread if still available
        if (killThread) {
            DLOGW("Executor thread TID: 0x%" PRIx64 " didn't shutdown gracefully. Terminating...", pTimerQueue->executorTid);
            THREAD_CANCEL(pTimerQueue->executorTid);
        }

        MUTEX_UNLOCK(pTimerQueue->exitLock);
    }

    if (IS_VALID_MUTEX_VALUE(pTimerQueue->executorLock)) {
        MUTEX_FREE(pTimerQueue->executorLock);
    }

    if (IS_VALID_MUTEX_VALUE(pTimerQueue->exitLock)) {
        MUTEX_FREE(pTimerQueue->exitLock);
    }

    if (IS_VALID_MUTEX_VALUE(pTimerQueue->startLock)) {
        MUTEX_FREE(pTimerQueue->startLock);
    }

    if (IS_VALID_CVAR_VALUE(pTimerQueue->executorCvar)) {
        CVAR_FREE(pTimerQueue->executorCvar);
    }

    if (IS_VALID_CVAR_VALUE(pTimerQueue->exitCvar)) {
        CVAR_FREE(pTimerQueue->exitCvar);
    }

    if (IS_VALID_CVAR_VALUE(pTimerQueue->startCvar)) {
        CVAR_FREE(pTimerQueue->startCvar);
    }

    MEMFREE(pTimerQueue);

    *ppTimerQueue = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS priv_timer_queue_evaluateNextInvocation(PTimerQueue pTimerQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 invokeTime = MAX_UINT64;
    UINT32 i, index;

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);

    // IMPORTANT!!! This internal function is assumed to be running under the executor lock of the timer queue
    for (i = 0, index = 0; index < pTimerQueue->activeTimerCount && i < pTimerQueue->maxTimerCount; i++) {
        if (pTimerQueue->pTimers[i].timerCallbackFn != NULL) {
            index++;

            if (invokeTime > pTimerQueue->pTimers[i].invokeTime) {
                invokeTime = pTimerQueue->pTimers[i].invokeTime;
            }
        }
    }

    pTimerQueue->invokeTime = invokeTime;

CleanUp:

    return retStatus;
}

PVOID timer_queue_executor(PVOID pArgs)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTimerQueue pTimerQueue = (PTimerQueue) pArgs;
    UINT64 curTime;
    UINT32 i, index, removeCount;
    BOOL locked = FALSE;

    CHK(pTimerQueue != NULL, STATUS_NULL_ARG);

    // Enable the start sequence
    ATOMIC_STORE_BOOL(&pTimerQueue->started, TRUE);

    // Enable the create API to finish
    MUTEX_LOCK(pTimerQueue->startLock);
    CVAR_SIGNAL(pTimerQueue->startCvar);
    MUTEX_UNLOCK(pTimerQueue->startLock);

    MUTEX_LOCK(pTimerQueue->executorLock);
    locked = TRUE;
    // The main loop
    while (!ATOMIC_LOAD_BOOL(&pTimerQueue->shutdown)) {
        // Block/wait until signaled or timed out
        curTime = GETTIME();
        if (curTime < pTimerQueue->invokeTime) {
            CVAR_WAIT(pTimerQueue->executorCvar, pTimerQueue->executorLock, pTimerQueue->invokeTime - curTime);
        }

        // Check for the shutdown
        if (!ATOMIC_LOAD_BOOL(&pTimerQueue->shutdown)) {
            // Check the time against timers
            curTime = GETTIME();
            removeCount = 0;
            for (i = 0, index = 0; index < pTimerQueue->activeTimerCount && i < pTimerQueue->maxTimerCount; i++) {
                if (pTimerQueue->pTimers[i].timerCallbackFn != NULL) {
                    index++;

                    if (curTime >= pTimerQueue->pTimers[i].invokeTime) {
                        // Call the callback while locked. The executor lock is locked at this time upon cvar awakening
                        retStatus = pTimerQueue->pTimers[i].timerCallbackFn(i, curTime, pTimerQueue->pTimers[i].customData);

                        // Check for the terminal condition and for single invoke timers
                        if (retStatus == STATUS_TIMER_QUEUE_STOP_SCHEDULING ||
                            pTimerQueue->pTimers[i].period == TIMER_QUEUE_SINGLE_INVOCATION_PERIOD) {
                            // Reset the return
                            retStatus = STATUS_SUCCESS;

                            // Reset the time entry
                            pTimerQueue->pTimers[i].timerCallbackFn = NULL;

                            // Need to remove from active count
                            removeCount++;
                        }

                        // Warn the user on error
                        CHK_LOG_ERR(retStatus);

                        // Set the new invoke
                        pTimerQueue->pTimers[i].invokeTime = curTime + pTimerQueue->pTimers[i].period;
                    }
                }
            }

            // Decrement the active timer count
            pTimerQueue->activeTimerCount -= removeCount;

            // Re-evaluate again
            CHK_STATUS(priv_timer_queue_evaluateNextInvocation(pTimerQueue));
        }
    }

    MUTEX_UNLOCK(pTimerQueue->executorLock);
    locked = FALSE;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTimerQueue->executorLock);
    }

    CHK_LOG_ERR(retStatus);

    if (pTimerQueue != NULL) {
        MUTEX_LOCK(pTimerQueue->exitLock);
        // Indicate we have terminated
        ATOMIC_STORE_BOOL(&pTimerQueue->terminated, TRUE);

        CVAR_SIGNAL(pTimerQueue->exitCvar);
        MUTEX_UNLOCK(pTimerQueue->exitLock);
    }

    THREAD_EXIT(NULL);
    LEAVES();
    return (PVOID)(ULONG_PTR) retStatus;
}
