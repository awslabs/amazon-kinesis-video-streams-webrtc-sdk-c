/**
 * Kinesis Video Producer Callbacks Provider
 */
#define LOG_CLASS "IceAgent"
#include "../Include_i.h"

extern StateMachineState ICE_AGENT_STATE_MACHINE_STATES[];
extern UINT32 ICE_AGENT_STATE_MACHINE_STATE_COUNT;

STATUS createIceAgent(PCHAR username, PCHAR password, UINT64 customData, PIceAgentCallbacks pIceAgentCallbacks,
                      PRtcConfiguration pRtcConfiguration, TIMER_QUEUE_HANDLE timerQueueHandle,
                      PConnectionListener pConnectionListener, PIceAgent* ppIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = NULL;
    UINT32 i, turnServerCount = 0;

    CHK(ppIceAgent != NULL && username != NULL && password != NULL && pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(username, MAX_ICE_CONFIG_USER_NAME_LEN + 1) <= MAX_ICE_CONFIG_USER_NAME_LEN &&
        STRNLEN(password, MAX_ICE_CONFIG_CREDENTIAL_LEN + 1) <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_ARG);

    // allocate the entire struct
    pIceAgent = (PIceAgent) MEMCALLOC(1, SIZEOF(IceAgent));
    STRNCPY(pIceAgent->localUsername, username, MAX_ICE_CONFIG_USER_NAME_LEN);
    STRNCPY(pIceAgent->localPassword, password, MAX_ICE_CONFIG_CREDENTIAL_LEN);

    // candidatePairs start off being invalid since we MEMCALLOC pIceAgent
    pIceAgent->candidatePairCount = 0;
    pIceAgent->agentStarted = FALSE;
    ATOMIC_STORE_BOOL(&pIceAgent->agentStartGathering, FALSE);
    pIceAgent->isControlling = FALSE;
    pIceAgent->tieBreaker = (UINT64) RAND();

    if (pIceAgentCallbacks != NULL) {
        pIceAgent->iceAgentCallbacks = *pIceAgentCallbacks;
    }
    pIceAgent->customData = customData;
    pIceAgent->stateEndTime = 0;
    pIceAgent->foundationCounter = 0;

    pIceAgent->lock = MUTEX_CREATE(TRUE);

    // Create the state machine
    CHK_STATUS(createStateMachine(ICE_AGENT_STATE_MACHINE_STATES,
                                  ICE_AGENT_STATE_MACHINE_STATE_COUNT,
                                  (UINT64) pIceAgent,
                                  kinesisVideoStreamDefaultGetCurrentTime,
                                  (UINT64) pIceAgent,
                                  &pIceAgent->pStateMachine));
    pIceAgent->iceAgentStatus = STATUS_SUCCESS;
    pIceAgent->iceAgentStateTimerCallback = UINT32_MAX;
    pIceAgent->keepAliveTimerCallback = UINT32_MAX;
    pIceAgent->timerQueueHandle = timerQueueHandle;
    pIceAgent->lastDataReceivedTime = INVALID_TIMESTAMP_VALUE;
    pIceAgent->detectedDisconnection = FALSE;
    pIceAgent->disconnectionGracePeriodEndTime = INVALID_TIMESTAMP_VALUE;
    pIceAgent->pConnectionListener = pConnectionListener;

    CHK_STATUS(doubleListCreate(&pIceAgent->localCandidates));
    CHK_STATUS(doubleListCreate(&pIceAgent->remoteCandidates));
    CHK_STATUS(stackQueueCreate(&pIceAgent->triggeredCheckQueue));

    for (i = 0; i < MAX_ICE_SERVERS_COUNT; i++) {
        if (pRtcConfiguration->iceServers[i].urls[0] != '\0') {
            CHK_STATUS(iceAgentAddIceServer(
                    pIceAgent,
                    (PCHAR) pRtcConfiguration->iceServers[i].urls,
                    (PCHAR) pRtcConfiguration->iceServers[i].username,
                    (PCHAR) pRtcConfiguration->iceServers[i].credential
            ));

            // use only one turn server for now
            if (pIceAgent->iceServers[pIceAgent->iceServersCount - 1].isTurn) {
                if (turnServerCount == 0) {
                    turnServerCount++;
                } else {
                    pIceAgent->iceServersCount--;
                }
            }
        }
    }

    // prime the state machine.
    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    if (STATUS_FAILED(retStatus) && pIceAgent != NULL) {
        freeIceAgent(&pIceAgent);
        pIceAgent = NULL;
    }

    if (ppIceAgent != NULL) {
        *ppIceAgent = pIceAgent;
    }

    LEAVES();
    return retStatus;
}

/**
 * Not thread-safe
 * @param ppIceAgent
 * @return
 */
STATUS freeIceAgent(PIceAgent* ppIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = NULL;
    UINT32 i;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pIceCandidate = NULL;


    CHK(ppIceAgent != NULL, STATUS_NULL_ARG);
    // freeIceAgent is idempotent
    CHK(*ppIceAgent != NULL, retStatus);

    pIceAgent = *ppIceAgent;

    // free TurnConnection before ConnectionListener because turn has SocketConnection that would be freed
    // by freeConnectionListener if freeConnectionListener is called first
    CHK_LOG_ERR(freeTurnConnection(&pIceAgent->pTurnConnection));

    // free connection first so no more incoming packets
    if (pIceAgent->pConnectionListener != NULL) {
        CHK_LOG_ERR(freeConnectionListener(&pIceAgent->pConnectionListener));
    }

    if (pIceAgent->localCandidates != NULL) {
        // free all socketConnection first
        CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pIceCandidate = (PIceCandidate) data;

            // turn socket connection is managed by TurnConnection
            if (pIceCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_RELAYED) {
                freeSocketConnection(&pIceCandidate->pSocketConnection);
            }
        }

        // free all stored candidates
        CHK_LOG_ERR(doubleListClear(pIceAgent->localCandidates, TRUE));
        CHK_LOG_ERR(doubleListFree(pIceAgent->localCandidates));
    }

    if (pIceAgent->remoteCandidates != NULL) {
        // remote candidates dont have socketConnection
        CHK_LOG_ERR(doubleListClear(pIceAgent->remoteCandidates, TRUE));
        CHK_LOG_ERR(doubleListFree(pIceAgent->remoteCandidates));
    }

    if (pIceAgent->triggeredCheckQueue != NULL) {
        CHK_LOG_ERR(stackQueueFree(pIceAgent->triggeredCheckQueue));
    }

    if (pIceAgent->candidatePairCount > 0) {
        for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
            freeTransactionIdStore(&pIceAgent->candidatePairs[i]->pTransactionIdStore);
            SAFE_MEMFREE(pIceAgent->candidatePairs[i]);
        }
    }

    if (pIceAgent->iceAgentStateTimerCallback != UINT32_MAX) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerCallback, (UINT64) pIceAgent));
    }

    if (pIceAgent->keepAliveTimerCallback != UINT32_MAX) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->keepAliveTimerCallback, (UINT64) pIceAgent));
    }

    if (IS_VALID_MUTEX_VALUE(pIceAgent->lock)) {
        MUTEX_FREE(pIceAgent->lock);
    }

    freeStateMachine(pIceAgent->pStateMachine);

    MEMFREE(pIceAgent);

    *ppIceAgent = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS iceAgentAddIceServer(PIceAgent pIceAgent, PCHAR url, PCHAR username, PCHAR credential) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceServer pIceServer = NULL;
    PCHAR separator = NULL, urlNoPrefix = NULL;
    UINT32 port = ICE_STUN_DEFAULT_PORT;

    // username and credential is only mandatory for turn server
    CHK(url != NULL && pIceAgent != NULL, STATUS_NULL_ARG);

    pIceServer = &(pIceAgent->iceServers[pIceAgent->iceServersCount]);

    if (STRNCMP(ICE_URL_PREFIX_STUN, url, STRLEN(ICE_URL_PREFIX_STUN)) == 0) {
        urlNoPrefix = STRCHR(url, ':') + 1;
        pIceServer->isTurn = FALSE;
    } else if (STRNCMP(ICE_URL_PREFIX_TURN, url, STRLEN(ICE_URL_PREFIX_TURN)) == 0 ||
               STRNCMP(ICE_URL_PREFIX_TURN_SECURE, url, STRLEN(ICE_URL_PREFIX_TURN_SECURE)) == 0) {
        CHK(username != NULL && username[0] != '\0', STATUS_ICE_URL_TURN_MISSING_USERNAME);
        CHK(credential != NULL && credential[0] != '\0', STATUS_ICE_URL_TURN_MISSING_CREDENTIAL);

        // TODO after getIceServerConfig no longer give turn: ips, do TLS only for turns:
        STRNCPY(pIceServer->username, username, MAX_ICE_CONFIG_USER_NAME_LEN);
        STRNCPY(pIceServer->credential, credential, MAX_ICE_CONFIG_CREDENTIAL_LEN);
        urlNoPrefix = STRCHR(url, ':') + 1;
        pIceServer->isTurn = TRUE;
    } else {
        CHK(FALSE, STATUS_ICE_URL_INVALID_PREFIX);
    }

    if ((separator = STRCHR(urlNoPrefix, ':')) != NULL) {
        separator++;
        CHK_STATUS(STRTOUI32(separator, separator + STRLEN(separator), 10, &port));
        STRNCPY(pIceServer->url, urlNoPrefix, separator - urlNoPrefix - 1);
        // need to null terminate since we are not copying the entire urlNoPrefix
        pIceServer->url[separator - urlNoPrefix - 1] = '\0';
    } else {
        STRNCPY(pIceServer->url, urlNoPrefix, MAX_ICE_CONFIG_URI_LEN);
    }

    CHK_STATUS(getIpWithHostName(pIceServer->url, &pIceServer->ipAddress));
    pIceServer->ipAddress.port = (UINT16) getInt16((INT16) port);

    pIceAgent->iceServersCount++;

CleanUp:

    LEAVES();

    return retStatus;
}

STATUS iceAgentSendStunPacket(PIceAgent pIceAgent, PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen,
                              PIceCandidatePair pIceCandidatePair)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 stunPacketSize = STUN_PACKET_ALLOCATION_SIZE;
    BYTE stunPacketBuffer[STUN_PACKET_ALLOCATION_SIZE];

    CHK_STATUS(iceUtilsPackageStunPacket(pStunPacket, password, passwordLen, stunPacketBuffer, &stunPacketSize));

    if (pIceCandidatePair->local->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        retStatus = turnConnectionSendData(pIceAgent->pTurnConnection, stunPacketBuffer, stunPacketSize, &pIceCandidatePair->remote->ipAddress);
    } else {
        retStatus = socketConnectionSendData(pIceCandidatePair->local->pSocketConnection, stunPacketBuffer, stunPacketSize, &pIceCandidatePair->remote->ipAddress);
    }

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceAgentSendStunPacket failed with 0x%08x", retStatus);
        retStatus = STATUS_SUCCESS;
    }

CleanUp:

    return retStatus;
}

STATUS iceAgentReportNewLocalCandidate(PIceAgent pIceAgent, PIceCandidate pIceCandidate)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR serializedIceCandidateBuf[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    UINT32 serializedIceCandidateBufLen = ARRAY_SIZE(serializedIceCandidateBuf);

    CHK(pIceAgent != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);
    CHK_WARN(pIceAgent->iceAgentCallbacks.newLocalCandidateFn != NULL, retStatus, "newLocalCandidateFn callback not implemented");

    CHK_STATUS(iceCandidateSerialize(pIceCandidate, serializedIceCandidateBuf, &serializedIceCandidateBufLen));
    pIceAgent->iceAgentCallbacks.newLocalCandidateFn(pIceAgent->iceAgentCallbacks.customData, serializedIceCandidateBuf);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceAgentAddRemoteCandidate(PIceAgent pIceAgent, PCHAR pIceCandidateString)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PIceCandidate pIceCandidate = NULL, pDuplicatedIceCandidate = NULL;
    PCHAR curr, tail, next;
    UINT32 tokenLen, portValue, remoteCandidateCount;
    BOOL foundIpAndPort = FALSE, freeIceCandidateIfFail = TRUE;

    CHK(pIceAgent != NULL && pIceCandidateString != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->remoteCandidates, &remoteCandidateCount));
    CHK(remoteCandidateCount < KVS_ICE_MAX_REMOTE_CANDIDATE_COUNT, STATUS_ICE_MAX_REMOTE_CANDIDATE_COUNT_EXCEEDED);

    CHK((pIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);

    curr = pIceCandidateString;
    tail = pIceCandidateString + STRLEN(pIceCandidateString);
    while ((next = STRNCHR(curr, tail - curr, ' ')) != NULL && !foundIpAndPort) {
        tokenLen = (UINT32) (next - curr);
        CHK(STRNCMPI("tcp", curr, tokenLen) != 0, STATUS_ICE_CANDIDATE_STRING_IS_TCP);

        if (pIceCandidate->ipAddress.address[0] != 0) {
            CHK_STATUS(STRTOUI32(curr, curr + tokenLen, 10, &portValue));

            pIceCandidate->ipAddress.port = htons(portValue);
            pIceCandidate->ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
        } else if (STRNCHR(curr, tokenLen, '.') != NULL) {
            CHK(tokenLen <= IP_STRING_LENGTH, STATUS_ICE_CANDIDATE_STRING_INVALID_IP); // IPv4 is 15 characters at most
            CHK_STATUS(populateIpFromString(&pIceCandidate->ipAddress, curr));
        }

        curr = next + 1;
        foundIpAndPort = (pIceCandidate->ipAddress.port != 0) && (pIceCandidate->ipAddress.address[0] != 0);
    }

    CHK(pIceCandidate->ipAddress.port != 0, STATUS_ICE_CANDIDATE_STRING_MISSING_PORT);
    CHK(pIceCandidate->ipAddress.address[0] != 0, STATUS_ICE_CANDIDATE_STRING_MISSING_IP);

    CHK_STATUS(findCandidateWithIp(&pIceCandidate->ipAddress, pIceAgent->remoteCandidates, &pDuplicatedIceCandidate));
    CHK(pDuplicatedIceCandidate == NULL, retStatus);

    // in case turn is not used
    if (pIceAgent->pTurnConnection != NULL) {
        CHK_STATUS(turnConnectionAddPeer(pIceAgent->pTurnConnection, &pIceCandidate->ipAddress));
    }
    CHK_STATUS(doubleListInsertItemHead(pIceAgent->remoteCandidates, (UINT64) pIceCandidate));
    freeIceCandidateIfFail = FALSE;

    // at the end of gathering state, candidate pairs will be created with local and remote candidates gathered so far.
    // do createIceCandidatePairs here if state is not gathering in case some remote candidate comes late
    if (pIceAgent->iceAgentState != ICE_AGENT_STATE_GATHERING) {
        CHK_STATUS(createIceCandidatePairs(pIceAgent, pIceCandidate, TRUE));
        // sort candidate pairs in case connectivity check is already running.
        sortIceCandidatePairs(pIceAgent);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus) && freeIceCandidateIfFail) {
        SAFE_MEMFREE(pIceCandidate);
    }

    LEAVES();
    return retStatus;
}

STATUS iceAgentGatherLocalCandidate(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PKvsIpAddress pIpAddress = NULL;
    BOOL locked = FALSE;
    PIceCandidate pTmpIceCandidate = NULL, pDuplicatedIceCandidate = NULL, pNewIceCandidate = NULL;
    KvsIpAddress localIpAddresses[MAX_LOCAL_NETWORK_INTERFACE_COUNT];
    UINT32 localIpAddressesCount = MAX_LOCAL_NETWORK_INTERFACE_COUNT, i;
    PSocketConnection pSocketConnection = NULL;

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(getLocalhostIpAddresses(localIpAddresses, &localIpAddressesCount));

    for(i = 0; i < localIpAddressesCount; ++i) {
        pIpAddress = localIpAddresses + i;

        // make sure pIceAgent->localCandidates has no duplicates
        CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->localCandidates, &pDuplicatedIceCandidate));

        if (pIpAddress->family == KVS_IP_FAMILY_TYPE_IPV4 && // Disable ipv6 gathering for now
            pDuplicatedIceCandidate == NULL &&
            STATUS_SUCCEEDED(createSocketConnection(pIpAddress, NULL, KVS_SOCKET_PROTOCOL_UDP, (UINT64) pIceAgent,
                                                    incomingDataHandler, &pSocketConnection))) {
            pTmpIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate));
            pTmpIceCandidate->ipAddress = localIpAddresses[i];
            pTmpIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_HOST;
            pTmpIceCandidate->state = ICE_CANDIDATE_STATE_VALID;
            pTmpIceCandidate->foundation = pIceAgent->foundationCounter++; // we dont generate candidates that have the same foundation.
            pTmpIceCandidate->pSocketConnection = pSocketConnection;
            pTmpIceCandidate->priority = computeCandidatePriority(pTmpIceCandidate);
            CHK_STATUS(doubleListInsertItemHead(pIceAgent->localCandidates, (UINT64) pTmpIceCandidate));
            // make a copy of pTmpIceCandidate so that if iceAgentReportNewLocalCandidate fails pTmpIceCandidate wont get freed.
            pNewIceCandidate = pTmpIceCandidate;
            pTmpIceCandidate = NULL;

            CHK_STATUS(iceAgentReportNewLocalCandidate(pIceAgent, pNewIceCandidate));
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    SAFE_MEMFREE(pTmpIceCandidate);

    LEAVES();
    return retStatus;
}

STATUS iceAgentStartAgent(PIceAgent pIceAgent, PCHAR remoteUsername, PCHAR remotePassword, BOOL isControlling)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL && remoteUsername != NULL && remotePassword != NULL, STATUS_NULL_ARG);
    CHK(!pIceAgent->agentStarted, retStatus); // make iceAgentStartAgent idempotent
    CHK(STRNLEN(remoteUsername, MAX_ICE_CONFIG_USER_NAME_LEN + 1) <= MAX_ICE_CONFIG_USER_NAME_LEN &&
        STRNLEN(remotePassword, MAX_ICE_CONFIG_CREDENTIAL_LEN + 1) <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    pIceAgent->agentStarted = TRUE;
    pIceAgent->isControlling = isControlling;

    STRNCPY(pIceAgent->remoteUsername, remoteUsername, MAX_ICE_CONFIG_USER_NAME_LEN);
    STRNCPY(pIceAgent->remotePassword, remotePassword, MAX_ICE_CONFIG_CREDENTIAL_LEN);
    if (STRLEN(pIceAgent->remoteUsername) + STRLEN(pIceAgent->localUsername) + 1 > MAX_ICE_CONFIG_USER_NAME_LEN) {
        DLOGW("remoteUsername:localUsername will be truncated to stay within %u char limit", MAX_ICE_CONFIG_USER_NAME_LEN);
    }
    SNPRINTF(pIceAgent->combinedUserName, ARRAY_SIZE(pIceAgent->combinedUserName), "%s:%s", pIceAgent->remoteUsername, pIceAgent->localUsername);

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS iceAgentStartGathering(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pIceAgent->agentStartGathering), retStatus);

    // Calling thread cant prime the IceAgent state otherwise we could have deadlock.
    // Need to set the flag here and let timer thread traverse the state.
    ATOMIC_STORE_BOOL(&pIceAgent->agentStartGathering, TRUE);

CleanUp:

    return retStatus;
}

STATUS iceAgentSendPacket(PIceAgent pIceAgent, PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL && pBuffer != NULL, STATUS_NULL_ARG);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_WARN(pIceAgent->pDataSendingIceCandidatePair != NULL, retStatus, "No valid ice candidate pair available to send data");

    pIceAgent->pDataSendingIceCandidatePair->lastDataSentTime = GETTIME();
    if (pIceAgent->pDataSendingIceCandidatePair->local->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        retStatus = turnConnectionSendData(pIceAgent->pTurnConnection,
                                        pBuffer,
                                        bufferLen,
                                        &pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress);
    } else {
        retStatus = socketConnectionSendData(pIceAgent->pDataSendingIceCandidatePair->local->pSocketConnection,
                                             pBuffer,
                                             bufferLen,
                                             &pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress);
    }

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceAgentSendPacket failed with 0x%08x", retStatus);
        retStatus = STATUS_SUCCESS;
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentPopulateSdpMediaDescriptionCandidates(PIceAgent pIceAgent, PSdpMediaDescription pSdpMediaDescription, UINT32 attrBufferLen, PUINT32 pIndex)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 data;
    PDoubleListNode pCurNode = NULL;
    BOOL locked = FALSE;
    UINT32 attrIndex;

    CHK(pIceAgent != NULL && pSdpMediaDescription != NULL && pIndex != NULL, STATUS_NULL_ARG);

    attrIndex = *pIndex;

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;

        STRCPY(pSdpMediaDescription->sdpAttributes[attrIndex].attributeName, "candidate");
        CHK_STATUS(iceCandidateSerialize((PIceCandidate) data, pSdpMediaDescription->sdpAttributes[attrIndex].attributeValue, &attrBufferLen));
        attrIndex++;
    }

    *pIndex = attrIndex;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

//////////////////////////////////////////////
// internal functions
//////////////////////////////////////////////

STATUS findCandidateWithIp(PKvsIpAddress pIpAddress, PDoubleList pCandidateList, PIceCandidate *ppIceCandidate)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pIceCandidate = NULL, pTargetIceCandidate = NULL;
    UINT32 addrLen;

    CHK(pIpAddress != NULL && pCandidateList != NULL && ppIceCandidate != NULL, STATUS_NULL_ARG);

    CHK_STATUS(doubleListGetHeadNode(pCandidateList, &pCurNode));
    while (pCurNode != NULL && pTargetIceCandidate == NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pIceCandidate = (PIceCandidate) data;
        pCurNode = pCurNode->pNext;

        addrLen = IS_IPV4_ADDR(pIpAddress) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;
        if (pIpAddress->family == pIceCandidate->ipAddress.family &&
            MEMCMP(pIceCandidate->ipAddress.address, pIpAddress->address, addrLen) == 0 &&
            pIpAddress->port == pIceCandidate->ipAddress.port) {
            pTargetIceCandidate = pIceCandidate;
        }
    }

CleanUp:

    if (ppIceCandidate != NULL) {
        *ppIceCandidate = pTargetIceCandidate;
    }

    LEAVES();
    return retStatus;
}

STATUS findCandidateWithConnectionHandle(PSocketConnection pSocketConnection, PDoubleList pCandidateList,
                                         PIceCandidate* ppIceCandidate)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pIceCandidate = NULL, pTargetIceCandidate = NULL;

    CHK(pCandidateList != NULL && ppIceCandidate != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    CHK_STATUS(doubleListGetHeadNode(pCandidateList, &pCurNode));
    while (pCurNode != NULL && pTargetIceCandidate == NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pIceCandidate = (PIceCandidate) data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate->pSocketConnection == pSocketConnection) {
            pTargetIceCandidate = pIceCandidate;
        }
    }

CleanUp:

    if (ppIceCandidate != NULL) {
        *ppIceCandidate = pTargetIceCandidate;
    }

    LEAVES();
    return retStatus;
}

STATUS createIceCandidatePairs(PIceAgent pIceAgent, PIceCandidate pIceCandidate, BOOL isRemoteCandidate)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT64 data;
    PDoubleListNode pCurNode = NULL;
    PDoubleList pDoubleList = NULL;

    CHK(pIceAgent != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);

    // if pIceCandidate is a remote candidate, then form pairs with every single valid local candidate. Otherwize,
    // form pairs with every single valid remote candidate
    pDoubleList = isRemoteCandidate ? pIceAgent->localCandidates : pIceAgent->remoteCandidates;

    CHK_STATUS(doubleListGetHeadNode(pDoubleList, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;

        // TODO use doublelist to store candidatePairs and insert new pairs sorted.
        // https://sim.amazon.com/issues/KinesisVideo-4892
        pIceAgent->candidatePairs[pIceAgent->candidatePairCount] = MEMCALLOC(1, SIZEOF(IceCandidatePair));
        CHK(pIceAgent->candidatePairs[pIceAgent->candidatePairCount] != NULL, STATUS_NOT_ENOUGH_MEMORY);

        if (isRemoteCandidate) {
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->local = (PIceCandidate) data;
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->remote = pIceCandidate;
        } else {
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->local = pIceCandidate;
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->remote = (PIceCandidate) data;
        }
        pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->nominated = FALSE;

        // if not in gathering state, starting from waiting so new pairs will get picked up connectivity check
        if (pIceAgent->iceAgentState == ICE_AGENT_STATE_GATHERING) {
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->state = ICE_CANDIDATE_PAIR_STATE_FROZEN;
        } else {
            pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->state = ICE_CANDIDATE_PAIR_STATE_WAITING;
        }

        CHK_STATUS(createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT,
                                            &pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->pTransactionIdStore));

        pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->lastDataSentTime = 0;
        pIceAgent->candidatePairs[pIceAgent->candidatePairCount]->priority = computeCandidatePairPriority(
                pIceAgent->candidatePairs[pIceAgent->candidatePairCount],
                pIceAgent->isControlling);
        pIceAgent->candidatePairCount++;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * sort candidate pairs based on priority in descending order using quick sort
 * @param pIceAgent
 * @return STATUS of operation
 */
STATUS sortIceCandidatePairs(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStackQueue pAuxStack = NULL;
    UINT64 low, high, i, j;
    UINT64 pivotPriority;
    BOOL auxStackEmpty = FALSE;
    PIceCandidatePair temp;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->candidatePairCount != 0, retStatus);

    low = 0;
    high = pIceAgent->candidatePairCount - 1;
    CHK_STATUS(stackQueueCreate(&pAuxStack));
    CHK_STATUS(stackQueuePush(pAuxStack, low));
    CHK_STATUS(stackQueuePush(pAuxStack, high));

    while(!auxStackEmpty) {
        CHK_STATUS(stackQueuePop(pAuxStack, &high));
        CHK_STATUS(stackQueuePop(pAuxStack, &low));

        // partition in descending order. pivot is swapped at the last iteration.
        pivotPriority = pIceAgent->candidatePairs[high]->priority;
        for (j = low, i = low; j <= high; ++j) {
            if (pIceAgent->candidatePairs[j]->priority >= pivotPriority) {
                temp = pIceAgent->candidatePairs[j];
                pIceAgent->candidatePairs[j] = pIceAgent->candidatePairs[i];
                pIceAgent->candidatePairs[i] = temp;
                i++;
            }
        }

        // set i to index of pivot
        i--;

        // sort values before pivot
        if (i > 1) {
            CHK_STATUS(stackQueuePush(pAuxStack, 0));
            CHK_STATUS(stackQueuePush(pAuxStack, i - 1));
        }

        // sort values after pivot
        if (high - i > 1) {
            CHK_STATUS(stackQueuePush(pAuxStack, i + 1));
            CHK_STATUS(stackQueuePush(pAuxStack, high));
        }

        CHK_STATUS(stackQueueIsEmpty(pAuxStack, &auxStackEmpty));
    }

CleanUp:

    if (pAuxStack != NULL) {
        stackQueueFree(pAuxStack);
    }

    return retStatus;
}

STATUS findIceCandidatePairWithLocalConnectionHandleAndRemoteAddr(PIceAgent pIceAgent, PSocketConnection pSocketConnection,
                                                                  PKvsIpAddress pRemoteAddr, BOOL checkPort,
                                                                  PIceCandidatePair* ppIceCandidatePair)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, addrLen;
    PIceCandidatePair pTargetIceCandidatePair = NULL;

    CHK(pIceAgent != NULL && ppIceCandidatePair != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    addrLen = IS_IPV4_ADDR(pRemoteAddr) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;

    for (i = 0; i < pIceAgent->candidatePairCount && pTargetIceCandidatePair == NULL; ++i) {
        if (pIceAgent->candidatePairs[i]->state != ICE_CANDIDATE_PAIR_STATE_FAILED &&
            pIceAgent->candidatePairs[i]->local->pSocketConnection == pSocketConnection &&
            pIceAgent->candidatePairs[i]->remote->ipAddress.family == pRemoteAddr->family &&
            MEMCMP(pIceAgent->candidatePairs[i]->remote->ipAddress.address, pRemoteAddr->address, addrLen) == 0 &&
            (!checkPort || pIceAgent->candidatePairs[i]->remote->ipAddress.port == pRemoteAddr->port)) {
            pTargetIceCandidatePair = pIceAgent->candidatePairs[i];
        }
    }

CleanUp:

    if (ppIceCandidatePair != NULL) {
        *ppIceCandidatePair = pTargetIceCandidatePair;
    }

    LEAVES();
    return retStatus;
}

STATUS pruneUnconnectedIceCandidatePair(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, connectedCandidatePairsCount = 0;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->candidatePairCount > 1, retStatus);

    // TODO can optimize pIceAgent->candidatePairs to be an array of PIceCandidatePair to avoid struct copy
    for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
        if (pIceAgent->candidatePairs[i]->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            connectedCandidatePairsCount++;
        } else {
            // set priority to 0 so after sorting unconnected candidate pairs will be shifted to the end.
            pIceAgent->candidatePairs[i]->priority = 0;
        }
    }

    CHK_STATUS(sortIceCandidatePairs(pIceAgent));
    for (i = connectedCandidatePairsCount; i < pIceAgent->candidatePairCount; ++i) {
        freeTransactionIdStore(&pIceAgent->candidatePairs[i]->pTransactionIdStore);
        SAFE_MEMFREE(pIceAgent->candidatePairs[i]);
    }
    pIceAgent->candidatePairCount = connectedCandidatePairsCount;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS pruneIceCandidatePairWithLocalCandidate(PIceAgent pIceAgent, PIceCandidate pIceCandidate)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;

    CHK(pIceAgent != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);
    CHK(pIceAgent->candidatePairCount > 1, retStatus);

    for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
        if (pIceAgent->candidatePairs[i]->local == pIceCandidate) {
            // by setting candidate pair state to failed, they will get removed automatically at the end of connectivity check
            pIceAgent->candidatePairs[i]->state = ICE_CANDIDATE_PAIR_STATE_FAILED;
        }
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS iceCandidatePairCheckConnection(PStunPacket pStunBindingRequest, PIceAgent pIceAgent, PIceCandidatePair pIceCandidatePair)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributePriority pStunAttributePriority = NULL;

    CHK(pStunBindingRequest != NULL && pIceAgent != NULL && pIceCandidatePair, STATUS_NULL_ARG);

    CHK_STATUS(getStunAttribute(pStunBindingRequest, STUN_ATTRIBUTE_TYPE_PRIORITY, (PStunAttributeHeader *) &pStunAttributePriority));
    CHK(pStunAttributePriority != NULL, STATUS_INVALID_ARG);

    // update priority and transaction id
    pStunAttributePriority->priority = pIceCandidatePair->local->priority;
    CHK_STATUS(iceUtilsGenerateTransactionId(pStunBindingRequest->header.transactionId,
                                     ARRAY_SIZE(pStunBindingRequest->header.transactionId)));

    transactionIdStoreInsert(pIceCandidatePair->pTransactionIdStore, pStunBindingRequest->header.transactionId);

    CHK_STATUS(iceUtilsSendStunPacket(pStunBindingRequest,
                                      (PBYTE) pIceAgent->remotePassword,
                                      (UINT32) STRLEN(pIceAgent->remotePassword) * SIZEOF(CHAR),
                                      &pIceCandidatePair->remote->ipAddress,
                                      pIceCandidatePair->local->pSocketConnection,
                                      pIceAgent->pTurnConnection,
                                      IS_CANN_PAIR_SENDING_FROM_RELAYED(pIceCandidatePair)));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

/**
 * timer queue callbacks are interlocked by time queue lock.
 *
 * @param timerId - timer queue task id
 * @param currentTime
 * @param customData - custom data passed to timer queue when task was added
 * @return
 */
STATUS iceAgentStateNewTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // prime the state machine
    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

/**
 * timer queue callbacks are interlocked by time queue lock.
 *
 * @param timerId - timer queue task id
 * @param currentTime
 * @param customData - custom data passed to timer queue when task was added
 * @return
 */
STATUS iceAgentStateGatheringTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    PStunPacket pStunRequest = NULL;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pCandidate = NULL;
    PIceServer pIceServer = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // create stun binding request packet
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pStunRequest));

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pCandidate = (PIceCandidate) data;

        if (pCandidate->state == ICE_CANDIDATE_STATE_NEW){

            switch(pCandidate->iceCandidateType) {
                case ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
                    pIceServer = &(pIceAgent->iceServers[pCandidate->iceServerIndex]);
                    CHK_STATUS(iceUtilsSendStunPacket(pStunRequest,
                                                      NULL,
                                                      0,
                                                      &pIceServer->ipAddress,
                                                      pCandidate->pSocketConnection,
                                                      NULL,
                                                      FALSE));
                    break;

                default:
                    break;
            }
        }
    }

    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pStunRequest != NULL) {
        freeStunPacket(&pStunRequest);
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

/**
 * timer queue callbacks are interlocked by time queue lock.
 *
 * @param timerId - timer queue task id
 * @param currentTime
 * @param customData - custom data passed to timer queue when task was added
 * @return
 */
STATUS iceAgentStateCheckConnectionTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE, triggeredCheckQueueEmpty;
    PStunPacket pStunBindingRequest = NULL;
    UINT64 data;
    PIceCandidatePair pIceCandidatePair = NULL;
    UINT32 i;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // extend endtime if no candidate pair is available or startIceAgent has not been called, in which case we would not
    // have the remote password.
    if (pIceAgent->candidatePairCount == 0 || !pIceAgent->agentStarted) {
        pIceAgent->stateEndTime = GETTIME() + KVS_ICE_CONNECTIVITY_CHECK_TIMEOUT;
        CHK(FALSE, retStatus);
    }

    // create stun binding request packet. Get pointer to its priority attribute so we can update it later.
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST,
                                NULL,
                                &pStunBindingRequest));
    CHK_STATUS(appendStunUsernameAttribute(pStunBindingRequest, pIceAgent->combinedUserName));
    CHK_STATUS(appendStunPriorityAttribute(pStunBindingRequest, 0));
    CHK_STATUS(appendStunIceControllAttribute(
            pStunBindingRequest,
            pIceAgent->isControlling ? STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING : STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED,
            pIceAgent->tieBreaker));

    // assuming pIceAgent->candidatePairs is sorted by priority

    CHK_STATUS(stackQueueIsEmpty(pIceAgent->triggeredCheckQueue, &triggeredCheckQueueEmpty));
    if (!triggeredCheckQueueEmpty) {
        // if triggeredCheckQueue is not empty, check its candidate pair first
        stackQueueDequeue(pIceAgent->triggeredCheckQueue, &data);
        pIceCandidatePair = (PIceCandidatePair) data;

        CHK_STATUS(iceCandidatePairCheckConnection(pStunBindingRequest, pIceAgent, pIceCandidatePair));
    } else {
        for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
            pIceCandidatePair = pIceAgent->candidatePairs[i];

            switch (pIceCandidatePair->state) {
                case ICE_CANDIDATE_PAIR_STATE_WAITING:
                    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS;
                    // NOTE: Explicit fall-through
                case ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS:
                    CHK_STATUS(iceCandidatePairCheckConnection(pStunBindingRequest, pIceAgent, pIceCandidatePair));
                    break;
                default:
                    break;
            }
        }
    }

    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pStunBindingRequest != NULL) {
        freeStunPacket(&pStunBindingRequest);
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

/**
 * timer queue callbacks are interlocked by time queue lock.
 *
 * @param timerId - timer queue task id
 * @param currentTime
 * @param customData - custom data passed to timer queue when task was added
 * @return
 */
STATUS iceAgentSendKeepAliveTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    PStunPacket pStunKeepAlive = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    UINT32 i;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // no other attribtues needed: https://tools.ietf.org/html/rfc8445#section-11
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_INDICATION,
                                NULL,
                                &pStunKeepAlive));

    for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
        pIceCandidatePair = pIceAgent->candidatePairs[i];

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED &&
            currentTime >= pIceCandidatePair->lastDataSentTime + KVS_ICE_SEND_KEEP_ALIVE_INTERVAL) {

            pIceCandidatePair->lastDataSentTime = currentTime;
            DLOGV("send keep alive");
            // Stun Binding Indication seems to not expect any response. Therefore not storing transactionId
            CHK_STATUS(iceAgentSendStunPacket(pIceAgent,
                                              pStunKeepAlive,
                                              NULL,
                                              0,
                                              pIceCandidatePair));

        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pStunKeepAlive != NULL) {
        freeStunPacket(&pStunKeepAlive);
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

/**
 * timer queue callbacks are interlocked by time queue lock.
 *
 * @param timerId - timer queue task id
 * @param currentTime
 * @param customData - custom data passed to timer queue when task was added
 * @return
 */
STATUS iceAgentStateNominatingTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    PStunPacket pStunBindingRequest = NULL;
    UINT32 i;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // send packet with USE_CANDIDATE flag if is controlling
    if (pIceAgent->isControlling) {
        // create stun binding request packet with use candidate flag
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST,
                                    NULL,
                                    &pStunBindingRequest));
        CHK_STATUS(appendStunUsernameAttribute(pStunBindingRequest, pIceAgent->combinedUserName));
        CHK_STATUS(appendStunPriorityAttribute(pStunBindingRequest, 0));
        CHK_STATUS(appendStunIceControllAttribute(
                pStunBindingRequest,
                STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING,
                pIceAgent->tieBreaker));
        CHK_STATUS(appendStunFlagAttribute(pStunBindingRequest, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE));

        for (i = 0; i < pIceAgent->candidatePairCount; ++i) {
            if (pIceAgent->candidatePairs[i]->nominated) {
                CHK_STATUS(iceCandidatePairCheckConnection(pStunBindingRequest, pIceAgent, pIceAgent->candidatePairs[i]));
            }
        }
    }

    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pStunBindingRequest != NULL) {
        freeStunPacket(&pStunBindingRequest);
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentStateReadyTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // step the state every KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL
    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceAgentNominateCandidatePair(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pNominatedCandidatePair = NULL;
    UINT32 i;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // Assume holding pIceAgent->lock
    // do nothing if not controlling
    CHK(pIceAgent->isControlling, retStatus);

    DLOGD("Nominating candidate pair");

    CHK_STATUS(sortIceCandidatePairs(pIceAgent));
    CHK(pIceAgent->candidatePairCount > 0 , STATUS_ICE_CANDIDATE_PAIR_LIST_EMPTY);

    // nominate the candidate with highest priority
    pNominatedCandidatePair = pIceAgent->candidatePairs[0];

    // all that remained in candidatePairs should be ICE_CANDIDATE_PAIR_STATE_SUCCEEDED.
    CHK(pNominatedCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED,
        STATUS_ICE_NOMINATED_CANDIDATE_NOT_CONNECTED);

    pNominatedCandidatePair->nominated = TRUE;

    // reset transaction id list to ignore future connectivity check response.
    transactionIdStoreClear(pNominatedCandidatePair->pTransactionIdStore);

    // move every other candidate pair to frozen state so the second connectivity check only checks the nominated pair.
    for (i = 1; i < pIceAgent->candidatePairCount; ++i) {
        pIceAgent->candidatePairs[i]->state = ICE_CANDIDATE_PAIR_STATE_FROZEN;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();

    return retStatus;
}

STATUS newRelayCandidateHandler(UINT64 customData, PKvsIpAddress pRelayAddress, PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    PIceCandidate pLocalCandidate = NULL;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;

    CHK(pIceAgent != NULL && pRelayAddress != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pLocalCandidate = (PIceCandidate) data;

        if (pLocalCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            pLocalCandidate->pSocketConnection = pSocketConnection;
            CHK_STATUS(updateCandidateAddress(pLocalCandidate, pRelayAddress));
            CHK_STATUS(iceAgentReportNewLocalCandidate(pIceAgent, pLocalCandidate));
            break;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS incomingDataHandler(UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen,
                           PKvsIpAddress pSrc, PKvsIpAddress pDest)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    pIceAgent->lastDataReceivedTime = GETTIME();

    // for stun packets, first 8 bytes are 4 byte type and length, then 4 byte magic byte
    if ((bufferLen < 8 || !IS_STUN_PACKET(pBuffer)) && pIceAgent->iceAgentCallbacks.inboundPacketFn != NULL) {
        // release lock early
        MUTEX_UNLOCK(pIceAgent->lock);
        locked = FALSE;

        pIceAgent->iceAgentCallbacks.inboundPacketFn(pIceAgent->customData, pBuffer, bufferLen);
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(handleStunPacket(pIceAgent, pBuffer, bufferLen, pSocketConnection, pSrc, pDest));

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceCandidateSerialize(PIceCandidate pIceCandidate, PCHAR pOutputData, PUINT32 pOutputLength)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 amountWritten = 0;

    CHK(pIceCandidate != NULL && pOutputLength != NULL, STATUS_NULL_ARG);

    // TODO FIXME real source of randomness
    if (IS_IPV4_ADDR(&(pIceCandidate->ipAddress))) {
        amountWritten = SNPRINTF(pOutputData,
                                 pOutputData == NULL ? 0 : *pOutputLength,
                                 "%u 1 udp %u %d.%d.%d.%d %d typ %s raddr 0.0.0.0 rport 0 generation 0 network-cost 999",
                                 pIceCandidate->foundation,
                                 pIceCandidate->priority,
                                 pIceCandidate->ipAddress.address[0],
                                 pIceCandidate->ipAddress.address[1],
                                 pIceCandidate->ipAddress.address[2],
                                 pIceCandidate->ipAddress.address[3],
                                 (UINT16) getInt16(pIceCandidate->ipAddress.port),
                                 iceAgentGetCandidateTypeStr(pIceCandidate->iceCandidateType));

        CHK_WARN(amountWritten > 0, STATUS_INTERNAL_ERROR, "SNPRINTF failed");

        if (pOutputData == NULL) {
            *pOutputLength = ((UINT32) amountWritten) + 1; // +1 for null terminator
        } else {
            // amountWritten doesnt account for null char
            CHK(amountWritten < *pOutputLength, STATUS_BUFFER_TOO_SMALL);
        }

    } else {
        DLOGW("ipv6 not supported yet");
    }

CleanUp:

    return retStatus;
}

STATUS handleStunPacket(PIceAgent pIceAgent, PBYTE pBuffer, UINT32 bufferLen, PSocketConnection pSocketConnection,
                        PKvsIpAddress pSrcAddr, PKvsIpAddress pDestAddr)
{
    UNUSED_PARAM(pDestAddr);

    STATUS retStatus = STATUS_SUCCESS;
    PStunPacket pStunPacket = NULL, pStunResponse = NULL;
    PStunAttributeHeader pStunAttr = NULL;
    UINT16 stunPacketType = 0;
    PIceCandidatePair pIceCandidatePair = NULL;
    PStunAttributeAddress pStunAttributeAddress = NULL;
    PStunAttributePriority pStunAttributePriority = NULL;
    UINT32 priority = 0;
    PIceCandidate pIceCandidate = NULL;

    // need to determine stunPacketType before deserializing because different password should be used depending on the packet type
    stunPacketType = (UINT16) getInt16(*((PUINT16) pBuffer));

    switch (stunPacketType) {
        case STUN_PACKET_TYPE_BINDING_REQUEST:
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, (PBYTE) pIceAgent->localPassword, (UINT32) STRLEN(pIceAgent->localPassword) * SIZEOF(CHAR), &pStunPacket));
            CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, pStunPacket->header.transactionId, &pStunResponse));
            CHK_STATUS(appendStunAddressAttribute(pStunResponse, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, pSrcAddr));
            CHK_STATUS(appendStunIceControllAttribute(
                    pStunResponse,
                    pIceAgent->isControlling ? STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING : STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED,
                    pIceAgent->tieBreaker
            ));

            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_PRIORITY, (PStunAttributeHeader *) &pStunAttributePriority));
            priority = pStunAttributePriority == NULL ? 0 : pStunAttributePriority->priority;
            CHK_STATUS(iceAgentCheckPeerReflexiveCandidate(pIceAgent, pSrcAddr, priority, TRUE, 0));

            CHK_STATUS(findIceCandidatePairWithLocalConnectionHandleAndRemoteAddr(pIceAgent, pSocketConnection, pSrcAddr, TRUE, &pIceCandidatePair));

            CHK_STATUS(iceUtilsSendStunPacket(pStunResponse,
                                              (PBYTE) pIceAgent->localPassword,
                                              (UINT32) STRLEN(pIceAgent->localPassword) * SIZEOF(CHAR),
                                              pSrcAddr,
                                              pSocketConnection,
                                              pIceAgent->pTurnConnection,
                                              pIceCandidatePair == NULL ? FALSE : IS_CANN_PAIR_SENDING_FROM_RELAYED(pIceCandidatePair)));

            // return early if there is no candidate pair. This can happen when we get connectivity check from the peer
            // before we receive the answer.
            CHK(pIceCandidatePair != NULL, retStatus);

            if (!pIceCandidatePair->nominated) {
                CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE, &pStunAttr));
                if (pStunAttr != NULL) {
                    DLOGD("received candidate with USE_CANDIDATE flag, local candidate type %s.",
                          iceAgentGetCandidateTypeStr(pIceCandidatePair->local->iceCandidateType));
                    pIceCandidatePair->nominated = TRUE;
                }
            }

            // schedule a connectivity check for the pair
            if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_FROZEN ||
                pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_WAITING ||
                pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS) {
                CHK_STATUS(stackQueueEnqueue(pIceAgent->triggeredCheckQueue, (UINT64) pIceCandidatePair));
            }

            break;

        case STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS:
            if (pIceAgent->iceAgentState == ICE_AGENT_STATE_GATHERING) {
                CHK_STATUS(findCandidateWithConnectionHandle(pSocketConnection, pIceAgent->localCandidates, &pIceCandidate));
                CHK_WARN(pIceCandidate != NULL, retStatus,
                         "Local candidate with socket %d not found for STUN packet type 0x%02x. Dropping Packet",
                         pSocketConnection->localSocket, stunPacketType);

                CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, NULL, 0, &pStunPacket));
                CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &pStunAttr));
                CHK_WARN(pStunAttr != NULL, retStatus, "No mapped address attribute found in STUN binding response. Dropping Packet");

                pStunAttributeAddress = (PStunAttributeAddress) pStunAttr;
                CHK_STATUS(updateCandidateAddress(pIceCandidate, &pStunAttributeAddress->address));
                CHK_STATUS(iceAgentReportNewLocalCandidate(pIceAgent, pIceCandidate));
                CHK(FALSE, retStatus);
            }


            CHK_STATUS(findIceCandidatePairWithLocalConnectionHandleAndRemoteAddr(pIceAgent, pSocketConnection, pSrcAddr, TRUE, &pIceCandidatePair));
            CHK_WARN(pIceCandidatePair != NULL, retStatus, "ConnectionHandle not found. Dropping stun packet");

            CHK_WARN(transactionIdStoreHasId(pIceCandidatePair->pTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET), retStatus,
                     "Dropping response packet because transaction id does not match");
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, (PBYTE) pIceAgent->remotePassword, (UINT32) STRLEN(pIceAgent->remotePassword) * SIZEOF(CHAR), &pStunPacket));
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &pStunAttr));
            CHK_WARN(pStunAttr != NULL, retStatus, "No mapped address attribute found in STUN response. Dropping Packet");

            pStunAttributeAddress = (PStunAttributeAddress) pStunAttr;

            if (!isSameIpAddress(&pStunAttributeAddress->address, &pIceCandidatePair->local->ipAddress, FALSE)) {
                // this can happen for host and server reflexive candidates. If the peer
                // is in the same subnet, server reflexive candidate's binding response's xor mapped ip address will be
                // the host candidate ip address. In this case we will ignore the packet since the host candidate will
                // be getting its own response for the connection check.
                DLOGD("local candidate ip address does not match with xor mapped address in binding response");

                // we have a peer reflexive local candidate
                CHK_STATUS(iceAgentCheckPeerReflexiveCandidate(pIceAgent, &pStunAttributeAddress->address, pIceCandidatePair->local->priority, FALSE, pSocketConnection));

                CHK(FALSE, retStatus);
            }

            pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;

            break;
        case STUN_PACKET_TYPE_BINDING_INDICATION:
            DLOGD("Received STUN binding indication");
            break;

        default:
            DLOGW("Dropping unrecognized STUN packet. Packet type: 0x%02", stunPacketType);
            break;
    }

CleanUp:

    if (pStunPacket != NULL) {
        freeStunPacket(&pStunPacket);
    }

    if (pStunResponse != NULL) {
        freeStunPacket(&pStunResponse);
    }

    // No need to fail on packet handling failures. #TODO send error packet
    if (STATUS_FAILED(retStatus)) {
        DLOGW("handleStunPacket failed with 0x%08x", retStatus);
        retStatus = STATUS_SUCCESS;
    }

    return retStatus;
}

STATUS iceAgentCheckPeerReflexiveCandidate(PIceAgent pIceAgent, PKvsIpAddress pIpAddress, UINT32 priority, BOOL isRemote, PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidate pIceCandidate = NULL, pLocalIceCandidate = NULL;;
    BOOL freeIceCandidateOnError = TRUE;
    UINT32 candidateCount;

    // remote candidate dont have socketConnection
    CHK(pIceAgent != NULL && pIpAddress != NULL && (isRemote || pSocketConnection != NULL), STATUS_NULL_ARG);

    if (!isRemote) {
        // local peer reflexive candidate replaces existing local candidate because the peer sees different address
        // for this local candidate.
        CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->localCandidates, &pIceCandidate));
        CHK(pIceCandidate == NULL, retStatus); // return early if duplicated

        findCandidateWithConnectionHandle(pSocketConnection, pIceAgent->localCandidates, &pLocalIceCandidate);
        pLocalIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
        pLocalIceCandidate->ipAddress = *pIpAddress;
        DLOGD("New local peer reflexive candidate found");
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->remoteCandidates, &candidateCount));
    CHK_WARN(candidateCount < KVS_ICE_MAX_REMOTE_CANDIDATE_COUNT, retStatus, "max remote candidate count exceeded"); // return early if limit exceeded
    CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->remoteCandidates, &pIceCandidate));
    CHK(pIceCandidate == NULL, retStatus); // return early if duplicated
    DLOGD("New remote peer reflexive candidate found");

    CHK((pIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pIceCandidate->ipAddress = *pIpAddress;
    pIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
    pIceCandidate->priority = priority;
    pIceCandidate->state = ICE_CANDIDATE_STATE_VALID;
    pIceCandidate->pSocketConnection = NULL; // remote candidate dont have PSocketConnection

    CHK_STATUS(doubleListInsertItemHead(pIceAgent->remoteCandidates, (UINT64) pIceCandidate));
    freeIceCandidateOnError = FALSE;
    CHK_STATUS(createIceCandidatePairs(pIceAgent, pIceCandidate, isRemote));

    // sort candidate pairs in case connectivity check is already running.
    sortIceCandidatePairs(pIceAgent);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && freeIceCandidateOnError) {
        MEMFREE(pIceCandidate);
    }

    return retStatus;
}

STATUS updateCandidateAddress(PIceCandidate pIceCandidate, PKvsIpAddress pIpAddr)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pIceCandidate != NULL && pIpAddr != NULL, STATUS_NULL_ARG);
    CHK(pIceCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_HOST, STATUS_INVALID_ARG);
    CHK(pIceCandidate->state == ICE_CANDIDATE_STATE_NEW, retStatus);

    // only copy the address. Port is already set.
    MEMCPY(pIceCandidate->ipAddress.address, pIpAddr->address, IS_IPV4_ADDR(pIpAddr) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH);
    pIceCandidate->ipAddress.port = pIpAddr->port;

    pIceCandidate->state = ICE_CANDIDATE_STATE_VALID;

    if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE) {
        DLOGD("New server reflexive candidate found");
    } else {
        DLOGD("New relay candidate found");
    }

CleanUp:

    return retStatus;
}

UINT32 computeCandidatePriority(PIceCandidate pIceCandidate)
{
    UINT32 typePreference = 0, localPreference = 0;

    switch (pIceCandidate->iceCandidateType) {
        case ICE_CANDIDATE_TYPE_HOST:
            typePreference = ICE_PRIORITY_HOST_CANDIDATE_TYPE_PREFERENCE;
            break;
        case ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
            typePreference = ICE_PRIORITY_SERVER_REFLEXIVE_CANDIDATE_TYPE_PREFERENCE;
            break;
        case ICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
            typePreference = ICE_PRIORITY_PEER_REFLEXIVE_CANDIDATE_TYPE_PREFERENCE;
            break;
        case ICE_CANDIDATE_TYPE_RELAYED:
            typePreference = ICE_PRIORITY_RELAYED_CANDIDATE_TYPE_PREFERENCE;
            break;
    }

    if (!pIceCandidate->ipAddress.isPointToPoint) {
        localPreference = ICE_PRIORITY_LOCAL_PREFERENCE;
    }

    return (2 ^ 24) * (typePreference) + (2 ^ 8) * (localPreference) + 255;
}

UINT64 computeCandidatePairPriority(PIceCandidatePair pIceCandidatePair, BOOL isLocalControlling)
{
    UINT64 controllingAgentCandidatePri = pIceCandidatePair->local->priority;
    UINT64 controlledAgentCandidatePri = pIceCandidatePair->remote->priority;

    if (!isLocalControlling) {
        controllingAgentCandidatePri = controlledAgentCandidatePri;
        controlledAgentCandidatePri = pIceCandidatePair->local->priority;
    }

    // https://tools.ietf.org/html/rfc5245#appendix-B.5
    return ((UINT64) 1 << 32) * MIN(controlledAgentCandidatePri, controllingAgentCandidatePri) +
           2 * MAX(controlledAgentCandidatePri, controllingAgentCandidatePri) +
           (controllingAgentCandidatePri > controlledAgentCandidatePri ? 1 : 0);
}

PCHAR iceAgentGetCandidateTypeStr(ICE_CANDIDATE_TYPE candidateType) {
    switch(candidateType) {
        case ICE_CANDIDATE_TYPE_HOST:
            return SDP_CANDIDATE_TYPE_HOST;
        case ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
            return SDP_CANDIDATE_TYPE_SERFLX;
        case ICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
            return SDP_CANDIDATE_TYPE_PRFLX;
        case ICE_CANDIDATE_TYPE_RELAYED:
            return SDP_CANDIDATE_TYPE_RELAY;
    }
}


