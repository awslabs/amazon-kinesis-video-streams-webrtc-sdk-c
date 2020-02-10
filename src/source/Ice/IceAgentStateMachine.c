/**
 * Implementation of a ice agent states machine callbacks
 */
#define LOG_CLASS "IceAgentState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState ICE_AGENT_STATE_MACHINE_STATES[] = {
        {ICE_AGENT_STATE_NEW, ICE_AGENT_STATE_NONE | ICE_AGENT_STATE_NEW, fromNewIceAgentState, executeNewIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_NEW_STATE},
        {ICE_AGENT_STATE_GATHERING, ICE_AGENT_STATE_NEW | ICE_AGENT_STATE_GATHERING | ICE_AGENT_STATE_DISCONNECTED, fromGatheringIceAgentState, executeGatheringIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_GATHERING_STATE},
        {ICE_AGENT_STATE_CHECK_CONNECTION, ICE_AGENT_STATE_GATHERING | ICE_AGENT_STATE_CHECK_CONNECTION | ICE_AGENT_STATE_DISCONNECTED, fromCheckConnectionIceAgentState, executeCheckConnectionIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_CHECK_CONNECTION_STATE},
        {ICE_AGENT_STATE_CONNECTED, ICE_AGENT_STATE_CHECK_CONNECTION | ICE_AGENT_STATE_CONNECTED | ICE_AGENT_STATE_DISCONNECTED, fromConnectedIceAgentState, executeConnectedIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_CONNECTED_STATE},
        {ICE_AGENT_STATE_NOMINATING, ICE_AGENT_STATE_CONNECTED | ICE_AGENT_STATE_NOMINATING | ICE_AGENT_STATE_DISCONNECTED, fromNominatingIceAgentState, executeNominatingIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_NOMINATING_STATE},
        {ICE_AGENT_STATE_READY,  ICE_AGENT_STATE_CONNECTED | ICE_AGENT_STATE_NOMINATING | ICE_AGENT_STATE_READY | ICE_AGENT_STATE_DISCONNECTED, fromReadyIceAgentState, executeReadyIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_READY_STATE},
        {ICE_AGENT_STATE_DISCONNECTED, ICE_AGENT_STATE_GATHERING | ICE_AGENT_STATE_CHECK_CONNECTION | ICE_AGENT_STATE_CONNECTED | ICE_AGENT_STATE_NOMINATING | ICE_AGENT_STATE_READY | ICE_AGENT_STATE_DISCONNECTED, fromDisconnectedIceAgentState, executeDisconnectedIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_DISCONNECTED_STATE},
        {ICE_AGENT_STATE_FAILED, ICE_AGENT_STATE_GATHERING | ICE_AGENT_STATE_CHECK_CONNECTION | ICE_AGENT_STATE_CONNECTED | ICE_AGENT_STATE_NOMINATING | ICE_AGENT_STATE_READY | ICE_AGENT_STATE_DISCONNECTED | ICE_AGENT_STATE_FAILED, fromFailedIceAgentState, executeFailedIceAgentState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_ICE_INVALID_FAILED_STATE},
};

UINT32 ICE_AGENT_STATE_MACHINE_STATE_COUNT = ARRAY_SIZE(ICE_AGENT_STATE_MACHINE_STATES);

STATUS stepIceAgentStateMachine(PIceAgent pIceAgent)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    UINT64 oldState;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    oldState = pIceAgent->iceAgentState;

    CHK_STATUS(stepStateMachine(pIceAgent->pStateMachine));

    if (oldState != pIceAgent->iceAgentState) {
        if (pIceAgent->iceAgentCallbacks.connectionStateChangedFn != NULL) {
            DLOGD("Ice agent state changed from %s to %s.",
                  iceAgentStateToString(oldState),
                  iceAgentStateToString(pIceAgent->iceAgentState));
            pIceAgent->iceAgentCallbacks.connectionStateChangedFn(pIceAgent->customData, pIceAgent->iceAgentState);
        }
    } else {
        CHK_STATUS(resetStateMachineRetryCount(pIceAgent->pStateMachine));
    }

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS acceptIceAgentMachineState(PIceAgent pIceAgent, UINT64 state)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // Step the state machine
    CHK_STATUS(acceptStateMachineState(pIceAgent->pStateMachine, state));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    LEAVES();
    return retStatus;
}

/*
 * This function is supposed to be called from within IceAgentStateMachine callbacks. Assume holding IceAgent->lock
 */
STATUS iceAgentStateMachineCheckDisconnection(PIceAgent pIceAgent, PUINT64 pNextState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentTime;

    CHK(pIceAgent != NULL && pNextState != NULL, STATUS_NULL_ARG);

    currentTime = GETTIME();
    if (!pIceAgent->detectedDisconnection &&
        IS_VALID_TIMESTAMP(pIceAgent->lastDataReceivedTime) &&
        pIceAgent->lastDataReceivedTime + KVS_ICE_ENTER_STATE_DISCONNECTION_GRACE_PERIOD <= currentTime) {
        *pNextState = ICE_AGENT_STATE_DISCONNECTED;
    } else if (pIceAgent->detectedDisconnection) {

        if (IS_VALID_TIMESTAMP(pIceAgent->lastDataReceivedTime) &&
            pIceAgent->lastDataReceivedTime + KVS_ICE_ENTER_STATE_DISCONNECTION_GRACE_PERIOD > currentTime) {
            // recovered from disconnection
            DLOGD("recovered from disconnection");
            pIceAgent->detectedDisconnection = FALSE;
            if (pIceAgent->iceAgentCallbacks.connectionStateChangedFn != NULL) {
                pIceAgent->iceAgentCallbacks.connectionStateChangedFn(pIceAgent->customData, pIceAgent->iceAgentState);
            }
        } else if (currentTime >= pIceAgent->disconnectionGracePeriodEndTime) {
            CHK(FALSE, STATUS_ICE_FAILED_TO_RECOVER_FROM_DISCONNECTION);
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

PCHAR iceAgentStateToString(UINT64 state)
{
    PCHAR stateStr = NULL;
    switch (state) {
        case ICE_AGENT_STATE_NONE:
            stateStr = ICE_AGENT_STATE_NONE_STR;
            break;
        case ICE_AGENT_STATE_NEW:
            stateStr = ICE_AGENT_STATE_NEW_STR;
            break;
        case ICE_AGENT_STATE_GATHERING:
            stateStr = ICE_AGENT_STATE_GATHERING_STR;
            break;
        case ICE_AGENT_STATE_CHECK_CONNECTION:
            stateStr = ICE_AGENT_STATE_CHECK_CONNECTION_STR;
            break;
        case ICE_AGENT_STATE_CONNECTED:
            stateStr = ICE_AGENT_STATE_CONNECTED_STR;
            break;
        case ICE_AGENT_STATE_NOMINATING:
            stateStr = ICE_AGENT_STATE_NOMINATING_STR;
            break;
        case ICE_AGENT_STATE_READY:
            stateStr = ICE_AGENT_STATE_READY_STR;
            break;
        case ICE_AGENT_STATE_DISCONNECTED:
            stateStr = ICE_AGENT_STATE_DISCONNECTED_STR;
            break;
        case ICE_AGENT_STATE_FAILED:
            stateStr = ICE_AGENT_STATE_FAILED_STR;
            break;
    }

    return stateStr;
}

///////////////////////////////////////////////////////////////////////////
// State machine callback functions
///////////////////////////////////////////////////////////////////////////
STATUS fromNewIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state;

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    if (ATOMIC_LOAD_BOOL(&pIceAgent->agentStartGathering)) {
        // Transition to gathering state
        state = ICE_AGENT_STATE_GATHERING;
    } else {
        state = ICE_AGENT_STATE_NEW;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeNewIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // return early if we are already in ICE_AGENT_STATE_GATHERING
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_NEW, retStatus);

    pIceAgent->iceAgentState = ICE_AGENT_STATE_NEW;

    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  pIceAgent->kvsRtcConfiguration.iceConnectionCheckPollingInterval,
                                  iceAgentStateTransitionTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->iceAgentStateTimerCallback));
CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGatheringIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = ICE_AGENT_STATE_GATHERING;
    PDoubleListNode pNextNode = NULL, pCurNode = NULL;
    UINT64 data, currentTime = GETTIME();
    PIceCandidate pCandidate;
    UINT32 validLocalCandidateCount = 0, totalLocalCandidateCount = 0;
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // count number of valid candidates
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pCandidate = (PIceCandidate) data;

        if (pCandidate->state == ICE_CANDIDATE_STATE_VALID) {
            validLocalCandidateCount++;
        } else {
            CHK_STATUS(getIpAddrStr(&pCandidate->ipAddress,
                                    ipAddrStr,
                                    ARRAY_SIZE(ipAddrStr)));
            DLOGD("checking local candidate type %s, ip %s", iceAgentGetCandidateTypeStr(pCandidate->iceCandidateType), ipAddrStr);
        }
    }

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->localCandidates, &totalLocalCandidateCount));

    // return early and remain in ICE_AGENT_STATE_GATHERING if condition not met
    CHK((validLocalCandidateCount > 0 && validLocalCandidateCount == totalLocalCandidateCount) ||
        currentTime >= pIceAgent->stateEndTime, retStatus);

    // filter out invalid candidates, and compute priority for valid candidates
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pNextNode));
    while (pNextNode != NULL) {
        pCurNode = pNextNode;
        pNextNode = pNextNode->pNext;
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCandidate = (PIceCandidate) data;

        if (pCandidate->state != ICE_CANDIDATE_STATE_VALID) {
            doubleListRemoveNode(pIceAgent->localCandidates, pCurNode);
            MEMFREE(pCandidate);
        } else {
            validLocalCandidateCount++;
        }
    }

    CHK(validLocalCandidateCount > 0, STATUS_ICE_NO_LOCAL_CANDIDATE_AVAILABLE_AFTER_GATHERING_TIMEOUT);

    // proceed to next state since since we have at least one local candidate
    state = ICE_AGENT_STATE_CHECK_CONNECTION;

    // report NULL candidate to signal that candidate gathering is done.
    if (pIceAgent->iceAgentCallbacks.newLocalCandidateFn != NULL) {
        pIceAgent->iceAgentCallbacks.newLocalCandidateFn(pIceAgent->customData, NULL);
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeGatheringIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    if (pIceAgent->iceAgentState != ICE_AGENT_STATE_GATHERING) {
        CHK_STATUS(iceAgentGatheringStateSetup(pIceAgent));
        pIceAgent->iceAgentState = ICE_AGENT_STATE_GATHERING;
    }

    CHK_STATUS(iceAgentSendSrflxCandidateRequest(pIceAgent));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromCheckConnectionIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = ICE_AGENT_STATE_CHECK_CONNECTION; // original state
    BOOL connectedCandidatePairFound = FALSE;
    UINT64 currentTime = GETTIME();
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // connected pair found ? go to ICE_AGENT_STATE_CONNECTED : timeout ? go to error : remain in ICE_AGENT_STATE_CHECK_CONNECTION
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL && !connectedCandidatePairFound) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            connectedCandidatePairFound = TRUE;
            state = ICE_AGENT_STATE_CONNECTED;
        }
    }

    // return error if no connected pair found within timeout
    if (!connectedCandidatePairFound && currentTime >= pIceAgent->stateEndTime) {
        retStatus = STATUS_ICE_NO_CONNECTED_CANDIDATE_PAIR;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeCheckConnectionIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    if (pIceAgent->iceAgentState != ICE_AGENT_STATE_CHECK_CONNECTION) {
        CHK_STATUS(iceAgentCheckConnectionStateSetup(pIceAgent));
        pIceAgent->iceAgentState = ICE_AGENT_STATE_CHECK_CONNECTION;
    }

    if (!pIceAgent->agentStarted) {
        // extend endtime if no candidate pair is available or startIceAgent has not been called, in which case we would not
        // have the remote password.
        pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceConnectionCheckTimeout;
    } else {
        CHK_STATUS(iceAgentCheckCandidatePairConnection(pIceAgent));
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromConnectedIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = ICE_AGENT_STATE_CONNECTED; // original state

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // Go directly to nominating state from connected state.
    state = ICE_AGENT_STATE_NOMINATING;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeConnectedIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    CHK_STATUS(iceAgentConnectedStateSetup(pIceAgent));

    pIceAgent->iceAgentState = ICE_AGENT_STATE_CONNECTED;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromNominatingIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = ICE_AGENT_STATE_NOMINATING; // original state
    UINT64 currentTime = GETTIME();
    BOOL nominatedAndValidCandidatePairFound = FALSE;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // has a nominated and connected pair ? go to ICE_AGENT_STATE_READY : timeout ? go to failed state : remain in ICE_AGENT_STATE_NOMINATING

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->nominated && pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            nominatedAndValidCandidatePairFound = TRUE;
            break;
        }
    }

    if (nominatedAndValidCandidatePairFound) {
        state = ICE_AGENT_STATE_READY;
    } else if (currentTime >= pIceAgent->stateEndTime) {
        CHK(FALSE, STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR);
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeNominatingIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    if (pIceAgent->iceAgentState != ICE_AGENT_STATE_NOMINATING) {
        CHK_STATUS(iceAgentNominatingStateSetup(pIceAgent));
        pIceAgent->iceAgentState = ICE_AGENT_STATE_NOMINATING;
    }

    if (pIceAgent->isControlling) {
        CHK_STATUS(iceAgentSendCandidateNomination(pIceAgent));
    } else {
        // if not controlling, keep sending connection checks and wait for peer
        // to nominate a pair.
        CHK_STATUS(iceAgentCheckCandidatePairConnection(pIceAgent));
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromReadyIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = ICE_AGENT_STATE_READY; // original state

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    CHK_STATUS(iceAgentStateMachineCheckDisconnection(pIceAgent, &state));
    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeReadyIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    if (pIceAgent->iceAgentState != ICE_AGENT_STATE_READY) {
        CHK_STATUS(iceAgentReadyStateSetup(pIceAgent));
        pIceAgent->iceAgentState = ICE_AGENT_STATE_READY;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromDisconnectedIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    UINT64 state = pIceAgent->iceAgentState; // state before ICE_AGENT_STATE_DISCONNECTED

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        state = ICE_AGENT_STATE_FAILED;
        pIceAgent->iceAgentStatus = retStatus;
        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pState != NULL) {
        *pState = state;
    }

    LEAVES();
    return retStatus;
}

STATUS executeDisconnectedIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // not stopping timer task from previous state as we want it to continue and perhaps recover.
    // not setting pIceAgent->iceAgentState as it stores the previous state and we want to step back into it to continue retry
    DLOGD("Ice agent detected disconnection. current state %s", iceAgentStateToString(pIceAgent->iceAgentState));
    pIceAgent->detectedDisconnection = TRUE;

    // after detecting disconnection, store disconnectionGracePeriodEndTime and when it is reached and ice still hasnt recover
    // then go to failed state.
    pIceAgent->disconnectionGracePeriodEndTime = GETTIME() + KVS_ICE_ENTER_STATE_FAILED_GRACE_PERIOD;

    if (pIceAgent->iceAgentCallbacks.connectionStateChangedFn != NULL) {
        pIceAgent->iceAgentCallbacks.connectionStateChangedFn(pIceAgent->customData, ICE_AGENT_STATE_DISCONNECTED);
    }

    // step out of disconnection state to retry
    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    LEAVES();
    return retStatus;
}

STATUS fromFailedIceAgentState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // FAILED state is terminal.
    *pState = ICE_AGENT_STATE_FAILED;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeFailedIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    const PCHAR errMsgPrefix = (PCHAR) "IceAgent fatal error:";

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_FAILED, retStatus);

    pIceAgent->iceAgentState = ICE_AGENT_STATE_FAILED;

    // log some debug info about the failure once.
    switch (pIceAgent->iceAgentStatus) {
        case STATUS_ICE_NO_AVAILABLE_ICE_CANDIDATE_PAIR:
            DLOGE("%s No ice candidate pairs available to make connection.", errMsgPrefix);
            break;
        default:
            DLOGE("IceAgent failed with 0x%08x", pIceAgent->iceAgentStatus);
            break;
    }

CleanUp:

    LEAVES();
    return retStatus;
}
