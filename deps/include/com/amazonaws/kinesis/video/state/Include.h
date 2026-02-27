/**
 * Main public include file
 */
#ifndef __CONTENT_STATE_INCLUDE__
#define __CONTENT_STATE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>

// IMPORTANT! Some of the headers are not tightly packed!
////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/mkvgen/Include.h>

////////////////////////////////////////////////////
// Status return codes
////////////////////////////////////////////////////
#define STATUS_STATE_BASE                     0x52000000
#define STATUS_INVALID_STREAM_STATE           STATUS_STATE_BASE + 0x0000000e
#define STATUS_STATE_MACHINE_STATE_NOT_FOUND  STATUS_STATE_BASE + 0x00000056
#define STATUS_STATE_MACHINE_NAME_LEN_INVALID STATUS_STATE_BASE + 0x0000009a // 0x00000057 to 0x0000008f used with STATUS_CLIENT_BASE

////////////////////////////////////////////////////
// Main structure declarations
////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////

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
 * Calls the from function of the current state to determine if the state machine is ready to
 * move on to another state.
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 * @param 2 PBOOL - OUT - Returns TRUE if state machine is ready to transition, else false
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS checkForStateTransition(PStateMachine, PBOOL);

/**
 * Return state machine name set up
 * @param 1 PStateMachine - IN - State machine object to be deallocated
 * @return - Returns the name set for the state machine
 */
PUBLIC_API PCHAR getStateMachineName(PStateMachine);

static const ExponentialBackoffRetryStrategyConfig DEFAULT_STATE_MACHINE_EXPONENTIAL_BACKOFF_RETRY_CONFIGURATION = {
    /* Exponential wait times with this config will look like following -
        ************************************
        * Retry Count *      Wait time     *
        * **********************************
        *     1       *    100ms + jitter  *
        *     2       *    200ms + jitter  *
        *     3       *    400ms + jitter  *
        *     4       *    800ms + jitter  *
        *     5       *   1600ms + jitter  *
        *     6       *   3200ms + jitter  *
        *     7       *   6400ms + jitter  *
        *     8       *  10000ms + jitter  *
        *     9       *  10000ms + jitter  *
        *    10       *  10000ms + jitter  *
        ************************************
        jitter = random number between [0, wait time)
    */
    KVS_INFINITE_EXPONENTIAL_RETRIES,                       /* max retry count */
    10000,                                                  /* max retry wait time in milliseconds */
    100,                                                    /* factor determining exponential curve in milliseconds */
    DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, /* minimum time in milliseconds to reset retry state */
    FULL_JITTER,                                            /* use full jitter variant */
    0                                                       /* jitter value unused for full jitter variant */
};

#ifdef __cplusplus
}
#endif
#endif /* __CONTENT_STATE_INCLUDE__ */
