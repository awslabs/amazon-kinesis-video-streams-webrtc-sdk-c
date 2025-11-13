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
#ifndef __AWS_KVS_WEBRTC_TIMER_QUEUE_INCLUDE__
#define __AWS_KVS_WEBRTC_TIMER_QUEUE_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "error.h"
#include "common_defs.h"
#include "time_port.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TimerQueue functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Minimum number of timers in the timer queue
 */
#define MIN_TIMER_QUEUE_TIMER_COUNT 1

/**
 * Default timer queue max timer count
 */
#define DEFAULT_TIMER_QUEUE_TIMER_COUNT 16

/**
 * Sentinel value to specify no periodic invocation
 */
#define TIMER_QUEUE_SINGLE_INVOCATION_PERIOD 0

/**
 * Shortest period value to schedule the call
 */
#define MIN_TIMER_QUEUE_PERIOD_DURATION (1 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Definition of the TimerQueue handle
 */
typedef UINT64 TIMER_QUEUE_HANDLE;
typedef TIMER_QUEUE_HANDLE* PTIMER_QUEUE_HANDLE;

/**
 * Timer queue callback
 *
 * IMPORTANT!!!
 * The callback should be 'prompt' - any lengthy or blocking operations should be executed
 * in their own execution unit - aka thread.
 *
 * NOTE: To terminate the scheduling of the calls return STATUS_TIMER_QUEUE_STOP_SCHEDULING
 * NOTE: Returning other non-STATUS_SUCCESS status will issue a warning but the timer will
 * still continue to be scheduled.
 *
 * @UINT32 - Timer ID that's fired
 * @UINT64 - Current time the scheduling is triggered
 * @UINT64 - Data that was passed to the timer function
 *
 */
typedef STATUS (*TimerCallbackFunc)(UINT32, UINT64, UINT64);

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_TIMER_QUEUE_HANDLE_VALUE
#define INVALID_TIMER_QUEUE_HANDLE_VALUE ((TIMER_QUEUE_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_TIMER_QUEUE_HANDLE
#define IS_VALID_TIMER_QUEUE_HANDLE(h) ((h) != INVALID_TIMER_QUEUE_HANDLE_VALUE)
#endif

/**
 * Startup and shutdown timeout value
 */
#define TIMER_QUEUE_SHUTDOWN_TIMEOUT (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Timer entry structure definition
 */
typedef struct __TimerEntry {
    UINT64 period;
    UINT64 invokeTime;
    UINT64 customData;
    TimerCallbackFunc timerCallbackFn;
} TimerEntry, *PTimerEntry;

/**
 * Internal timer queue definition
 */
typedef struct __TimerQueue {
    volatile TID executorTid;
    volatile ATOMIC_BOOL shutdown;
    volatile ATOMIC_BOOL terminated;
    volatile ATOMIC_BOOL started;
    UINT32 maxTimerCount;
    UINT32 activeTimerCount;
    UINT64 invokeTime;
    MUTEX executorLock;
    CVAR executorCvar;
    MUTEX startLock;
    CVAR startCvar;
    MUTEX exitLock;
    CVAR exitCvar;
    PTimerEntry pTimers;
} TimerQueue, *PTimerQueue;

// Public handle to and from object converters
#define TO_TIMER_QUEUE_HANDLE(p)   POINTER_TO_HANDLE(p)
#define FROM_TIMER_QUEUE_HANDLE(h) (IS_VALID_TIMER_QUEUE_HANDLE(h) ? (PTimerQueue) HANDLE_TO_POINTER(h) : NULL)

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * @brief Executor routine.
 *
 * @param[in] pArgs the argument of the timer queue.
 *
 * @return PVOID status of execution.
 */
PVOID timer_queue_executor(PVOID pArgs);

/**
 * @param - PTIMER_QUEUE_HANDLE - OUT - Timer queue handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCreate(PTIMER_QUEUE_HANDLE);

/**
 * @brief create the timer queue.
 *
 * @param[in, out] pHandle the handle of the timer queue.
 * @param[in] timerName the thread name of the timer queue.
 * @param[in] threadSize the thread size of the timer queue.
 *
 * @return STATUS status of execution.
 */
STATUS timerQueueCreateEx(PTIMER_QUEUE_HANDLE pHandle, PCHAR timerName, UINT32 threadSize);

/*
 * Frees the Timer queue object
 *
 * NOTE: The call is idempotent.
 *
 * @param - PTIMER_QUEUE_HANDLE - IN/OUT/OPT - Timer queue handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueFree(PTIMER_QUEUE_HANDLE);

/*
 * Add timer to the timer queue.
 *
 * NOTE: The timer period value of TIMER_QUEUE_SINGLE_INVOCATION_PERIOD will schedule the call only once
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - Start duration in 100ns at which to start the first time
 * @param - UINT64 - IN - Timer period value in 100ns to schedule the callback
 * @param - TimerCallbackFunc - IN - Callback to call for the timer
 * @param - UINT64 - IN - Timer callback function custom data
 * @param - PUINT32 - IN - Created timers ID
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueAddTimer(TIMER_QUEUE_HANDLE, UINT64, UINT64, TimerCallbackFunc, UINT64, PUINT32);

/*
 * Cancel the timer. customData is needed to handle case when user 1 add timer and then the timer
 * get cancelled because the callback returned STATUS_TIMER_QUEUE_STOP_SCHEDULING. Then user 2 add
 * another timer but then user 1 cancel timeId it first received. Without checking custom data user 2's timer
 * would be deleted by user 1.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT32 - IN - Timer id to cancel
 * @param - UINT64 - IN - provided customData. CustomData needs to match in order to successfully cancel.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelTimer(TIMER_QUEUE_HANDLE, UINT32, UINT64);

/*
 * Cancel all timers with customData
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - provided customData.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelTimersWithCustomData(TIMER_QUEUE_HANDLE, UINT64);

/*
 * Cancel all timers
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelAllTimers(TIMER_QUEUE_HANDLE);

/*
 * Get active timer count
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - PUINT32 - OUT - pointer to store active timer count
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueGetTimerCount(TIMER_QUEUE_HANDLE, PUINT32);

/*
 * Get timer ids with custom data
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - custom data to match
 * @param - PUINT32 - IN/OUT - size of the array passing in and will store the number of timer ids when the function returns
 * @param - PUINT32* - OUT/OPT - array that will store the timer ids. can be NULL.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueGetTimersWithCustomData(TIMER_QUEUE_HANDLE, UINT64, PUINT32, PUINT32);

/*
 * update timer id's period. Do nothing if timer not found.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - custom data to match
 * @param - UINT32 - IN - Timer id to update
 * @param - UINT32 - IN - new period
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueUpdateTimerPeriod(TIMER_QUEUE_HANDLE, UINT64, UINT32, UINT64);

/*
 * stop the timer. Once stopped timer can't be restarted. There will be no more timer callback invocation after
 * timerQueueShutdown returns.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueShutdown(TIMER_QUEUE_HANDLE);

/*
 * Kick timer id's timer to invoke immediately. Do nothing if timer not found.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT32 - IN - Timer id to update
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueKick(TIMER_QUEUE_HANDLE, UINT32);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_TIMER_QUEUE_INCLUDE__ */
