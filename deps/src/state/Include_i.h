/*******************************************
Main internal include file
*******************************************/
#ifndef __CONTENT_STATE_INCLUDE_I__
#define __CONTENT_STATE_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Project include files
////////////////////////////////////////////////////
#include "com/amazonaws/kinesis/video/state/Include.h"

////////////////////////////////////////////////////
// General defines and data structures
////////////////////////////////////////////////////
/**
 * Calculates the next service call retry time
 */
#define NEXT_SERVICE_CALL_RETRY_DELAY(r) (((UINT64) 1 << (r)) * SERVICE_CALL_RETRY_TIMEOUT)

/**
 * State Machine context
 */
typedef struct __StateMachineContext StateMachineContext;
struct __StateMachineContext {
    PStateMachineState pCurrentState;
    // Current retry count for a given state.
    // This is incremented only for retries happening within the same state. Once the
    // state transitions to a different state, localRetryCount is reset to 0.
    UINT32 localStateRetryCount;
    // Wait time before transitioning to next state.
    // Next state could be the same state OR a different state
    UINT64 stateTransitionWaitTime;
};
typedef struct __StateMachineContext* PStateMachineContext;

/**
 * State Machine definition
 */
typedef struct __StateMachineImpl StateMachineImpl;
struct __StateMachineImpl {
    // First member should be the public state machine
    StateMachine stateMachine;

    // Current time functionality
    GetCurrentTimeFunc getCurrentTimeFunc;

    // Custom data to be passed with the time function
    UINT64 getCurrentTimeFuncCustomData;

    // Custom data to be passed with the state callbacks
    UINT64 customData;

    // State machine context
    StateMachineContext context;

    // State machine state count
    UINT32 stateCount;

    CHAR stateMachineName[MAX_STATE_MACHINE_NAME_LENGTH + 1];

    // State machine states following the main structure
    PStateMachineState states;
};
typedef struct __StateMachineImpl* PStateMachineImpl;

////////////////////////////////////////////////////
// Internal functionality
////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif
#endif /* __CONTENT_STATE_INCLUDE_I__ */
