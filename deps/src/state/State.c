/**
 * State machine functionality
 */

#define LOG_CLASS "State"
#include "Include_i.h"

/**
 * Creates a new state machine
 */
STATUS createStateMachine(PStateMachineState pStates, UINT32 stateCount, UINT64 customData, GetCurrentTimeFunc getCurrentTimeFunc,
                          UINT64 getCurrentTimeFuncCustomData, PStateMachine* ppStateMachine)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachine = NULL;
    UINT32 allocationSize = 0;

    CHK(pStates != NULL && ppStateMachine != NULL && getCurrentTimeFunc != NULL, STATUS_NULL_ARG);
    CHK(stateCount > 0, STATUS_INVALID_ARG);

    // Allocate the main struct with an array of stream pointers at the end
    // NOTE: The calloc will Zero the fields
    allocationSize = SIZEOF(StateMachineImpl) + SIZEOF(StateMachineState) * stateCount;
    pStateMachine = (PStateMachineImpl) MEMCALLOC(1, allocationSize);
    CHK(pStateMachine != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Set the values
    pStateMachine->stateMachine.version = STATE_MACHINE_CURRENT_VERSION;
    pStateMachine->getCurrentTimeFunc = getCurrentTimeFunc;
    pStateMachine->getCurrentTimeFuncCustomData = getCurrentTimeFuncCustomData;
    pStateMachine->stateCount = stateCount;
    pStateMachine->customData = customData;

    // Set the states pointer and copy the globals
    pStateMachine->states = (PStateMachineState) (pStateMachine + 1);

    // Copy the states over
    MEMCPY(pStateMachine->states, pStates, SIZEOF(StateMachineState) * stateCount);

    // NOTE: Set the initial state as the first state.
    pStateMachine->context.pCurrentState = pStateMachine->states;

    // Assign the created object
    *ppStateMachine = (PStateMachine) pStateMachine;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStateMachine((PStateMachine) pStateMachine);
    }

    LEAVES();
    return retStatus;
}

STATUS createStateMachineWithName(PStateMachineState pStates, UINT32 stateCount, UINT64 customData, GetCurrentTimeFunc getCurrentTimeFunc,
                                  UINT64 getCurrentTimeFuncCustomData, PCHAR pStateMachineName, PStateMachine* ppStateMachine)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachineImpl;
    CHK_ERR(pStateMachineName != NULL, STATUS_NULL_ARG, "State machine name is NULL");
    CHK_ERR(!IS_EMPTY_STRING(pStateMachineName), STATUS_STATE_MACHINE_NAME_LEN_INVALID, "State machine name is empty");
    CHK(STRNLEN(pStateMachineName, MAX_STATE_MACHINE_NAME_LENGTH + 1) <= MAX_STATE_MACHINE_NAME_LENGTH, STATUS_STATE_MACHINE_NAME_LEN_INVALID);
    CHK_STATUS(createStateMachine(pStates, stateCount, customData, getCurrentTimeFunc, getCurrentTimeFuncCustomData, ppStateMachine));
    pStateMachineImpl = (PStateMachineImpl) *ppStateMachine;
    CHK(pStateMachineImpl != NULL, STATUS_NULL_ARG);
    STRNCPY(pStateMachineImpl->stateMachineName, pStateMachineName, MAX_STATE_MACHINE_NAME_LENGTH);
    pStateMachineImpl->stateMachineName[MAX_STATE_MACHINE_NAME_LENGTH] = '\0';
    *ppStateMachine = (PStateMachine) pStateMachineImpl;
CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * Frees the state machine object
 */
STATUS freeStateMachine(PStateMachine pStateMachine)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    // Call is idempotent
    CHK(pStateMachine != NULL, retStatus);

    // Release the object
    MEMFREE(pStateMachine);

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Gets a pointer to the state object given it's state
 */
STATUS getStateMachineState(PStateMachine pStateMachine, UINT64 state, PStateMachineState* ppState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pState = NULL;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;
    UINT32 i;

    CHK(pStateMachineImpl != NULL && ppState, STATUS_NULL_ARG);

    // Iterate over and find the first state
    for (i = 0; pState == NULL && i < pStateMachineImpl->stateCount; i++) {
        if (pStateMachineImpl->states[i].state == state) {
            pState = &pStateMachineImpl->states[i];
        }
    }

    // Check if found
    CHK(pState != NULL, STATUS_STATE_MACHINE_STATE_NOT_FOUND);

    // Assign the object which might be NULL if we didn't find any
    *ppState = pState;

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Gets a pointer to the current state object
 */
STATUS getStateMachineCurrentState(PStateMachine pStateMachine, PStateMachineState* ppState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;

    CHK(pStateMachineImpl != NULL && ppState, STATUS_NULL_ARG);

    *ppState = pStateMachineImpl->context.pCurrentState;

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Transition the state machine given its context
 */
STATUS stepStateMachine(PStateMachine pStateMachine)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pState = NULL;
    UINT64 nextState, time;
    UINT64 customData;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;
    UINT64 errorStateTransitionWaitTime = 0;

    CHK(pStateMachineImpl != NULL, STATUS_NULL_ARG);
    customData = pStateMachineImpl->customData;

    // Get the next state
    CHK(pStateMachineImpl->context.pCurrentState->getNextStateFn != NULL, STATUS_NULL_ARG);
    CHK_STATUS(pStateMachineImpl->context.pCurrentState->getNextStateFn(pStateMachineImpl->customData, &nextState));

    // Validate if the next state can accept the current state before transitioning
    CHK_STATUS(getStateMachineState(pStateMachine, nextState, &pState));

    CHK_STATUS(acceptStateMachineState((PStateMachine) pStateMachineImpl, pState->acceptStates));

    // Clear the iteration info if a different state and transition the state
    time = pStateMachineImpl->getCurrentTimeFunc(pStateMachineImpl->getCurrentTimeFuncCustomData);

    // This stateTransitionHookFunc will return state transition wait time if the state transition is happening for non 200 service response
    // For 200 service response, errorStateTransitionWaitTime will be 0.
    if (pStateMachineImpl->context.pCurrentState->stateTransitionHookFunc != NULL) {
        CHK_STATUS(pStateMachineImpl->context.pCurrentState->stateTransitionHookFunc(pStateMachineImpl->customData, &errorStateTransitionWaitTime));
    }

    // Check if we are changing the state
    if (pState->state != pStateMachineImpl->context.pCurrentState->state) {
        // Since we're transitioning to a different state from this state, reset the local state retry count to 0
        pStateMachineImpl->context.localStateRetryCount = 0;
    } else {
        // Increment the local state retry count.
        // Local state retry count determines the number of retries done within the same state.
        pStateMachineImpl->context.localStateRetryCount++;
    }

    if (IS_EMPTY_STRING(pStateMachineImpl->stateMachineName)) {
        DLOGV("State Machine - Current state: 0x%016" PRIx64 ", Next state: 0x%016" PRIx64 ", "
              "Current local state retry count [%u], Max local state retry count [%u], State transition wait time [%u] ms",
              pStateMachineImpl->context.pCurrentState->state, nextState, pStateMachineImpl->context.localStateRetryCount,
              pState->maxLocalStateRetryCount, errorStateTransitionWaitTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    } else {
        DLOGV("[%s] State Machine - Current state: 0x%016" PRIx64 ", Next state: 0x%016" PRIx64 ", "
              "Current local state retry count [%u], Max local state retry count [%u], State transition wait time [%u] ms",
              pStateMachineImpl->stateMachineName, pStateMachineImpl->context.pCurrentState->state, nextState,
              pStateMachineImpl->context.localStateRetryCount, pState->maxLocalStateRetryCount,
              errorStateTransitionWaitTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    // Check if we have tried enough times within the same state
    if (pState->maxLocalStateRetryCount != INFINITE_RETRY_COUNT_SENTINEL) {
        CHK(pStateMachineImpl->context.localStateRetryCount <= pState->maxLocalStateRetryCount, pState->status);
    }

    pStateMachineImpl->context.stateTransitionWaitTime = time + errorStateTransitionWaitTime;
    pStateMachineImpl->context.pCurrentState = pState;

    // Execute the state function if specified
    // The executeStateFn callback is expected to wait for stateTransitionWaitTime before executing the actual logic
    if (pStateMachineImpl->context.pCurrentState->executeStateFn != NULL) {
        CHK_STATUS(pStateMachineImpl->context.pCurrentState->executeStateFn(pStateMachineImpl->customData,
                                                                            pStateMachineImpl->context.stateTransitionWaitTime));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Checks whether the state machine state is accepted states
 */
STATUS acceptStateMachineState(PStateMachine pStateMachine, UINT64 requiredStates)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;

    CHK(pStateMachineImpl != NULL, STATUS_NULL_ARG);

    // Check the current state
    CHK((requiredStates & pStateMachineImpl->context.pCurrentState->state) == pStateMachineImpl->context.pCurrentState->state,
        STATUS_INVALID_STREAM_STATE);

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Force sets the state machine state
 */
STATUS setStateMachineCurrentState(PStateMachine pStateMachine, UINT64 state)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;
    PStateMachineState pState = NULL;

    CHK(pStateMachineImpl != NULL, STATUS_NULL_ARG);
    CHK_STATUS(getStateMachineState(pStateMachine, state, &pState));

    // Force set the state
    pStateMachineImpl->context.pCurrentState = pState;

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Resets the state machine retry count
 */
STATUS resetStateMachineRetryCount(PStateMachine pStateMachine)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;

    CHK(pStateMachineImpl != NULL, STATUS_NULL_ARG);

    // Reset the state
    pStateMachineImpl->context.localStateRetryCount = 0;
    pStateMachineImpl->context.stateTransitionWaitTime = 0;

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Calls the from function of the current state to determine if the state machine is ready to
 * move on to another state.
 */
STATUS checkForStateTransition(PStateMachine pStateMachine, PBOOL pTransitionReady)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pState = NULL;
    UINT64 nextState, time;
    UINT64 customData;
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;
    UINT64 errorStateTransitionWaitTime = 0;
    BOOL transitionReady = FALSE;
    UINT32 i;

    CHK(pStateMachineImpl != NULL && pTransitionReady != NULL, STATUS_NULL_ARG);
    customData = pStateMachineImpl->customData;

    // Get the next state
    CHK(pStateMachineImpl->context.pCurrentState->getNextStateFn != NULL, STATUS_NULL_ARG);
    CHK_STATUS(pStateMachineImpl->context.pCurrentState->getNextStateFn(pStateMachineImpl->customData, &nextState));

    // Iterate over and find the first state
    for (i = 0; pState == NULL && i < pStateMachineImpl->stateCount; i++) {
        if (pStateMachineImpl->states[i].state == nextState) {
            if (pStateMachineImpl->context.pCurrentState->state != nextState) {
                transitionReady = TRUE;
            }
            break;
        }
    }

    *pTransitionReady = transitionReady;

CleanUp:

    LEAVES();
    return retStatus;
}

// This function is useful for unit tests
PCHAR getStateMachineName(PStateMachine pStateMachine)
{
    PStateMachineImpl pStateMachineImpl = (PStateMachineImpl) pStateMachine;
    if (pStateMachineImpl == NULL) {
        DLOGW("State machine object not created. Cannot retrieve name");
        return NULL;
    } else {
        return pStateMachineImpl->stateMachineName;
    }
}