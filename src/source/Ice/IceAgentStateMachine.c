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
                                  iceAgentStateNewTimerCallback,
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
    UINT32 localCandidateCount, j;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pCandidate = NULL, pNewCandidate = NULL;
    TurnConnectionCallbacks turnConnectionCallbacks;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // return early if we are already in ICE_AGENT_STATE_GATHERING
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_GATHERING, retStatus);

    // stop timer task from previous state
    CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerCallback, (UINT64) pIceAgent));

    // skip gathering host candidate and srflx candidate if relay only
    if (pIceAgent->iceTransportPolicy != ICE_TRANSPORT_POLICY_RELAY) {
        CHK_STATUS(iceAgentGatherLocalCandidate(pIceAgent));

        CHK_STATUS(doubleListGetNodeCount(pIceAgent->localCandidates, &localCandidateCount));
        CHK(localCandidateCount != 0, STATUS_ICE_NO_LOCAL_HOST_CANDIDATE_AVAILABLE);

        CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pCandidate = (PIceCandidate) data;

            if (pCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_HOST) {
                CHK_STATUS(connectionListenerAddConnection(pIceAgent->pConnectionListener, pCandidate->pSocketConnection));

                for (j = 0; j < pIceAgent->iceServersCount; j++) {
                    if (!pIceAgent->iceServers[j].isTurn) {
                        CHK((pNewCandidate = (PIceCandidate) MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL,
                            STATUS_NOT_ENOUGH_MEMORY);

                        // copy over host candidate's address to open up a new socket at that address.
                        pNewCandidate->ipAddress = pCandidate->ipAddress;
                        // open up a new socket at host candidate's ip address for server reflex candidate.
                        // the new port will be stored in pNewCandidate->ipAddress.port. And the Ip address will later be updated
                        // with the correct ip address once the STUN response is received. Relay candidate's socket is manageed
                        // by TurnConnectino struct.
                        CHK_STATUS(createSocketConnection(&pNewCandidate->ipAddress, NULL, KVS_SOCKET_PROTOCOL_UDP,
                                                          (UINT64) pIceAgent, incomingDataHandler,
                                                          &pNewCandidate->pSocketConnection));
                        CHK_STATUS(connectionListenerAddConnection(pIceAgent->pConnectionListener,
                                                                   pNewCandidate->pSocketConnection));

                        pNewCandidate->iceCandidateType = pIceAgent->iceServers[j].isTurn ? ICE_CANDIDATE_TYPE_RELAYED
                                                                                          : ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
                        pNewCandidate->state = ICE_CANDIDATE_STATE_NEW;
                        pNewCandidate->iceServerIndex = j;
                        pNewCandidate->foundation = pIceAgent->foundationCounter++; // we dont generate candidates that have the same foundation.
                        pNewCandidate->priority = computeCandidatePriority(pNewCandidate);
                        CHK_STATUS(doubleListInsertItemHead(pIceAgent->localCandidates, (UINT64) pNewCandidate));
                        pNewCandidate = NULL;
                    }
                }
            } else {
                pCurNode = NULL; // break the loop as we have gone through all local candidates
            }
        }
    }

    // start gathering turn candidate
    for (j = 0; j < pIceAgent->iceServersCount; ++j) {
        if (pIceAgent->iceServers[j].isTurn) {
            turnConnectionCallbacks.customData = (UINT64) pIceAgent;
            turnConnectionCallbacks.applicationDataAvailableFn = incomingDataHandler;
            turnConnectionCallbacks.relayAddressAvailableFn = newRelayCandidateHandler;

            CHK_STATUS(createTurnConnection(&pIceAgent->iceServers[j], pIceAgent->timerQueueHandle,
                                            pIceAgent->pConnectionListener, TURN_CONNECTION_DATA_TRANSFER_MODE_SEND_INDIDATION,
                                            KVS_ICE_DEFAULT_TURN_PROTOCOL, &turnConnectionCallbacks, &pIceAgent->pTurnConnection, pIceAgent->kvsRtcConfiguration.iceSetInterfaceFilterFunc));

            // when connecting with non-trickle peer, when remote candidates are added, turnConnection would not be created
            // yet. So we need to add them here. turnConnectionAddPeer does ignore duplicates
            CHK_STATUS(doubleListGetHeadNode(pIceAgent->remoteCandidates, &pCurNode));
            while (pCurNode != NULL) {
                CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
                pCurNode = pCurNode->pNext;
                pCandidate = (PIceCandidate) data;

                CHK_STATUS(turnConnectionAddPeer(pIceAgent->pTurnConnection, &pCandidate->ipAddress));
            }
            CHK_STATUS(turnConnectionStart(pIceAgent->pTurnConnection));

            break;
        }
    }

    // start listening for incoming data
    CHK_STATUS(connectionListenerStart(pIceAgent->pConnectionListener));

    if (pIceAgent->pBindingRequest != NULL) {
        CHK_STATUS(freeStunPacket(&pIceAgent->pBindingRequest));
    }
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pIceAgent->pBindingRequest));

    pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceLocalCandidateGatheringTimeout;

    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  pIceAgent->kvsRtcConfiguration.iceConnectionCheckPollingInterval,
                                  iceAgentStateGatheringTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->iceAgentStateTimerCallback));

    pIceAgent->iceAgentState = ICE_AGENT_STATE_GATHERING;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (STATUS_FAILED(retStatus)) {
        pIceAgent->iceAgentStatus = retStatus;
        // step into failed state
        stepStateMachine(pIceAgent->pStateMachine);

        // fix up retStatus so we can successfully transition to failed state.
        retStatus = STATUS_SUCCESS;
    }

    if (pNewCandidate != NULL) {
        SAFE_MEMFREE(pNewCandidate);
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

    // return early if connection found and proceed to next state
    CHK(!connectedCandidatePairFound, retStatus);

    if (currentTime >= pIceAgent->stateEndTime) {
        CHK(FALSE, STATUS_ICE_NO_CONNECTED_CANDIDATE_PAIR);
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
    UINT32 localCandidateCount, remoteCandidateCount, iceCandidatePairCount = 0;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pLocalCandidate = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // return early if we are already in ICE_AGENT_STATE_CHECK_CONNECTION
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_CHECK_CONNECTION, retStatus);

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->localCandidates, &localCandidateCount));
    CHK_STATUS(doubleListGetNodeCount(pIceAgent->remoteCandidates, &remoteCandidateCount));

    if (localCandidateCount == 0) {
        DLOGW("no local candidate available when entering ICE_AGENT_STATE_CHECK_CONNECTION state");
    }

    if (remoteCandidateCount == 0) {
        DLOGW("no remote candidate available when entering ICE_AGENT_STATE_CHECK_CONNECTION state");
    }

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pLocalCandidate = (PIceCandidate) data;

        CHK_STATUS(createIceCandidatePairs(pIceAgent, pLocalCandidate, FALSE));
    }

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->iceCandidatePairs, &iceCandidatePairCount));

    DLOGD("ice candidate pair count %u", iceCandidatePairCount);

    // stop timer task from previous state
    CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerCallback, (UINT64) pIceAgent));

    // move all candidate pairs out of frozen state
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_WAITING;
    }

    if (pIceAgent->pBindingRequest != NULL) {
        CHK_STATUS(freeStunPacket(&pIceAgent->pBindingRequest));
    }
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pIceAgent->pBindingRequest));
    CHK_STATUS(appendStunUsernameAttribute(pIceAgent->pBindingRequest, pIceAgent->combinedUserName));
    CHK_STATUS(appendStunPriorityAttribute(pIceAgent->pBindingRequest, 0));
    CHK_STATUS(appendStunIceControllAttribute(
            pIceAgent->pBindingRequest,
            pIceAgent->isControlling ? STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING : STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED,
            pIceAgent->tieBreaker));

    pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceConnectionCheckTimeout;

    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  pIceAgent->kvsRtcConfiguration.iceConnectionCheckPollingInterval,
                                  iceAgentStateCheckConnectionTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->iceAgentStateTimerCallback));

    pIceAgent->iceAgentState = ICE_AGENT_STATE_CHECK_CONNECTION;

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
    UINT64 currentTime = GETTIME();
    UINT32 validCandidatePairCount = 0, iceCandidateCount = 0;
    BOOL nominatedAndValidCandidatePairFound = FALSE;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL && pState != NULL, STATUS_NULL_ARG);

    // move to failed state if any error happened.
    CHK_STATUS(pIceAgent->iceAgentStatus);

    // return early if changing to disconnected state
    CHK(state != ICE_AGENT_STATE_DISCONNECTED, retStatus);

    // nominatedAndValidCandidatePairFound ? go to ICE_AGENT_STATE_READY : all candidate pairs are valid or connectivity timeout reached ?
    // go to ICE_AGENT_STATE_NOMINATING : remain in ICE_AGENT_STATE_CONNECTED

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->iceCandidatePairs, &iceCandidateCount));

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            validCandidatePairCount++;
            if (pIceCandidatePair->nominated) {
                nominatedAndValidCandidatePairFound = TRUE;
            }
        }
    }

    if (nominatedAndValidCandidatePairFound) {
        state = ICE_AGENT_STATE_READY;
    } else if (validCandidatePairCount == iceCandidateCount || currentTime >= pIceAgent->stateEndTime) {
        state = ICE_AGENT_STATE_NOMINATING;
    }

    // remove unconnected candidate pair when transferring to a new state
    if (state != ICE_AGENT_STATE_CONNECTED) {
        CHK_STATUS(pruneUnconnectedIceCandidatePair(pIceAgent));
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

STATUS executeConnectedIceAgentState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // return early if we are already in ICE_AGENT_STATE_CONNECTED
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_CONNECTED, retStatus);
    // dont stop timer task from previous state. We want to keep running connectivity check

    // use the first connected pair as the data sending pair
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            pIceAgent->pDataSendingIceCandidatePair = pIceCandidatePair;
            break;
        }
    }

    // schedule sending keep alive
    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  KVS_ICE_SEND_KEEP_ALIVE_INTERVAL,
                                  iceAgentSendKeepAliveTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->keepAliveTimerCallback));

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
    // return early if we are already in ICE_AGENT_STATE_NOMINATING
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_NOMINATING, retStatus);

    // stop timer task from previous state
    CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerCallback, (UINT64) pIceAgent));

    if (pIceAgent->isControlling) {
        CHK_STATUS(iceAgentNominateCandidatePair(pIceAgent));

        if (pIceAgent->pBindingRequest != NULL) {
            CHK_STATUS(freeStunPacket(&pIceAgent->pBindingRequest));
        }
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pIceAgent->pBindingRequest));
        CHK_STATUS(appendStunUsernameAttribute(pIceAgent->pBindingRequest, pIceAgent->combinedUserName));
        CHK_STATUS(appendStunPriorityAttribute(pIceAgent->pBindingRequest, 0));
        CHK_STATUS(appendStunIceControllAttribute(
                pIceAgent->pBindingRequest,
                STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING,
                pIceAgent->tieBreaker));
        CHK_STATUS(appendStunFlagAttribute(pIceAgent->pBindingRequest, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE));
    }

    pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceCandidateNominationTimeout;

    // schedule nomination timer task
    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  pIceAgent->kvsRtcConfiguration.iceConnectionCheckPollingInterval,
                                  iceAgentStateNominatingTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->iceAgentStateTimerCallback));

    pIceAgent->iceAgentState = ICE_AGENT_STATE_NOMINATING;

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
    PIceCandidatePair pNominatedAndValidCandidatePair = NULL;
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->iceAgentState != ICE_AGENT_STATE_READY, retStatus);

    // stop timer task from previous state
    CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerCallback, (UINT64) pIceAgent));

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL && pNominatedAndValidCandidatePair == NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->nominated && pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            pNominatedAndValidCandidatePair = pIceCandidatePair;
            break;
        }
    }

    CHK(pNominatedAndValidCandidatePair != NULL, STATUS_ICE_NO_NOMINATED_VALID_CANDIDATE_PAIR_AVAILABLE);

    pIceAgent->pDataSendingIceCandidatePair = pNominatedAndValidCandidatePair;
    CHK_STATUS(getIpAddrStr(&pIceAgent->pDataSendingIceCandidatePair->local->ipAddress,
                            ipAddrStr,
                            ARRAY_SIZE(ipAddrStr)));
    DLOGD("Selected pair ip address: %s, port %u, local candidate type: %s",
          ipAddrStr,
          (UINT16) getInt16(pIceAgent->pDataSendingIceCandidatePair->local->ipAddress.port),
          iceAgentGetCandidateTypeStr(pIceAgent->pDataSendingIceCandidatePair->local->iceCandidateType));

    pIceAgent->stateEndTime = INVALID_TIMESTAMP_VALUE;

    // schedule timer task to keep stepping state and check for disconnection
    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle,
                                  0,
                                  KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL,
                                  iceAgentStateReadyTimerCallback,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->iceAgentStateTimerCallback));

    pIceAgent->iceAgentState = ICE_AGENT_STATE_READY;

    if (!IS_CANN_PAIR_SENDING_FROM_RELAYED(pIceAgent->pDataSendingIceCandidatePair) && pIceAgent->pTurnConnection != NULL) {
        DLOGD("Relayed candidate is not selected. Turn allocation will be freed");
        CHK_STATUS(turnConnectionStop(pIceAgent->pTurnConnection));
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
