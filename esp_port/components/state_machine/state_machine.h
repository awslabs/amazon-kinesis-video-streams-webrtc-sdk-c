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
#ifndef __AWS_KVS_WEBRTC_STATE_MACHINE__
#define __AWS_KVS_WEBRTC_STATE_MACHINE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "platform_utils.h"
#include "error.h"
#include "time_port.h"
/******************************************************************************
 * General defines and data structures
 ******************************************************************************/
/**
 * Service call retry timeout - 100ms
 */
#define SERVICE_CALL_RETRY_TIMEOUT (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Indicates infinite retries
 */
#define INFINITE_RETRY_COUNT_SENTINEL 0

/**
 * State machine current version
 */
#define STATE_MACHINE_CURRENT_VERSION 0

/**
 * Calculates the next service call retry time
 */
#define NEXT_SERVICE_CALL_RETRY_DELAY(r) (((UINT64) 1 << (r)) * SERVICE_CALL_RETRY_TIMEOUT)

/**
 * Maximum state machine name length
 */
#define MAX_STATE_MACHINE_NAME_LENGTH 32

/**
 * State transition function definitions
 *
 * @param 1 UINT64 - IN - Custom data passed in
 * @param 2 PUINT64 - OUT - Returned next state on success
 *
 * @return Status of the callback
 */
typedef STATUS (*GetNextStateFunc)(UINT64, PUINT64);

/**
 * State execution function definitions
 *
 * @param 1 - IN - Custom data passed in
 * @param 2 - IN - Delay time Time after which to execute the function
 *
 * @return Status of the callback
 */
typedef STATUS (*ExecuteStateFunc)(UINT64, UINT64);

/**
 * Function which gets called for each state before state machine
 * transitions to a different state. State machine transitioning to
 * the same state will not result in the transition hook being called
 *
 * @param 1 - IN - Custom data passed in
 * @param 2 - OUT - An opaque return value
 *
 * @return Status of the callback
 */
typedef STATUS (*StateTransitionHookFunc)(UINT64, PUINT64);

/**
 * State Machine state
 */
typedef struct __StateMachineState StateMachineState;
struct __StateMachineState {
    UINT64 state;
    UINT64 acceptStates;
    GetNextStateFunc getNextStateFn;
    ExecuteStateFunc executeStateFn;
    StateTransitionHookFunc stateTransitionHookFunc;
    UINT32 maxLocalStateRetryCount;
    STATUS status;
};
typedef struct __StateMachineState* PStateMachineState;

/**
 * State Machine definition
 */
typedef struct __StateMachine StateMachine;
struct __StateMachine {
    UINT32 version;
};
typedef struct __StateMachine* PStateMachine;

/**
 * Creates the state machine object
 *
 * @param 1 PStateMachineState - IN - List of state machine details on states, state transition functions and valid state transition list (mandatory)
 * @param 2 UINT32 - IN - stateCount - Number of entries in the PStateMachineState list (mandatory)
 * @param 3 UINT64 - IN - customData - application specific data that needs to be passed around (optional)
 * @param 4 GetCurrentTimeFunc - IN - custom get time function that the state machine should use to state transition wait time (optional)
 * @param 5 UINT64 - IN - getCurrentTimeFuncCustomData - custom data for getCurrentTimeFunc (optional)
 * @param 6 PStateMachine - OUT - Allocated state machine object
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createStateMachine(PStateMachineState, UINT32, UINT64, GetCurrentTimeFunc, UINT64, PStateMachine*);

/**
 * Creates a state machine with a state machine name string to identify the state machine. State machine name is mandatory
 * if using this API and cannot exceed 32 characters and cannot be an empty string
 *
 * @param 1 PStateMachineState - IN - List of state machine details on states, state transition functions and valid state transition list (mandatory)
 * @param 2 UINT32 - IN - stateCount - Number of entries in the PStateMachineState list (mandatory)
 * @param 3 UINT64 - IN - customData - application specific data that needs to be passed around (optional)
 * @param 4 GetCurrentTimeFunc - IN - custom get time function that the state machine should use to state transition wait time (optional)
 * @param 5 UINT64 - IN - getCurrentTimeFuncCustomData - custom data for getCurrentTimeFunc (optional)
 * @param 6 PCHAR - IN - pStateMachineName - name for the state machine. This will be used in the logs to uniquely identify a state machine (useful
 * when there are multiple state machines running simultaneously
 * @param 6 PStateMachine - OUT - Allocated state machine object
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createStateMachineWithName(PStateMachineState, UINT32, UINT64, GetCurrentTimeFunc, UINT64, PCHAR, PStateMachine*);

/**
 * Free the state machine object
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freeStateMachine(PStateMachine);

/**
 * Step to next valid state in a state machine.
 * @param 1 PStateMachine - IN - State machine object
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS stepStateMachine(PStateMachine);

/**
 * Checks whether the next state machine state is in the list of accepted states
 * @param 1 PStateMachine - IN - State machine object
 * @param 2 UINT64 - IN - requiredStates - ORed list of allowed states to transition to a particular state
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS acceptStateMachineState(PStateMachine, UINT64);

/**
 * Validate if the next state can accept the current state before transitioning
 * @param 1 PStateMachine - IN - State machine object
 * @param 2 UINT64 - IN - state - The next state to transition to
 * @param 3 PStateMachineState - OUT - Pointer to the state object given it's state
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS getStateMachineState(PStateMachine, UINT64, PStateMachineState*);

/**
 * Gets a pointer to the current state object
 * @param 1 PStateMachine - IN - State machine object
 * @param 3 PStateMachineState - OUT - Pointer to the state object for the current state
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS getStateMachineCurrentState(PStateMachine, PStateMachineState*);

/**
 * Force sets a pointer to the current state object
 * @param 1 PStateMachine - IN - State machine object
 * @param 3 UINT64 - IN - state - Set state machine to a particular state
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setStateMachineCurrentState(PStateMachine, UINT64);

/**
 * Resets the state machine retry count
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS resetStateMachineRetryCount(PStateMachine);

/**
 * Return state machine name set up
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 * @return - Returns the name set for the state machine
 */
PUBLIC_API PCHAR getStateMachineName(PStateMachine);

/**
 * Calls the from function of the current state to determine if the state machine is ready to
 * move on to another state.
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 * @param 2 PBOOL - OUT - Returns TRUE if state machine is ready to transition, else false
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS checkForStateTransition(PStateMachine, PBOOL);


#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_STATE_MACHINE__ */
