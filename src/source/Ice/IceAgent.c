/**
 * Kinesis Video Producer Callbacks Provider
 */
#define LOG_CLASS "IceAgent"
#include "../Include_i.h"

// https://developer.mozilla.org/en-US/docs/Web/API/RTCIceCandidate/candidate
// https://tools.ietf.org/html/rfc5245#section-15.1
typedef enum {
    SDP_ICE_CANDIDATE_PARSER_STATE_FOUNDATION = 0,
    SDP_ICE_CANDIDATE_PARSER_STATE_COMPONENT,
    SDP_ICE_CANDIDATE_PARSER_STATE_PROTOCOL,
    SDP_ICE_CANDIDATE_PARSER_STATE_PORIORITY,
    SDP_ICE_CANDIDATE_PARSER_STATE_IP,
    SDP_ICE_CANDIDATE_PARSER_STATE_PORT,
    SDP_ICE_CANDIDATE_PARSER_STATE_TYPE,
    SDP_ICE_CANDIDATE_PARSER_STATE_OTHERS
} SDP_ICE_CANDIDATE_PARSER_STATE;

extern StateMachineState ICE_AGENT_STATE_MACHINE_STATES[];
extern UINT32 ICE_AGENT_STATE_MACHINE_STATE_COUNT;

STATUS createIceAgent(PCHAR username, PCHAR password, PIceAgentCallbacks pIceAgentCallbacks, PRtcConfiguration pRtcConfiguration,
                      TIMER_QUEUE_HANDLE timerQueueHandle, PConnectionListener pConnectionListener, PIceAgent* ppIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = NULL;
    UINT32 i;

    CHK(ppIceAgent != NULL && username != NULL && password != NULL && pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(STRNLEN(username, MAX_ICE_CONFIG_USER_NAME_LEN + 1) <= MAX_ICE_CONFIG_USER_NAME_LEN &&
            STRNLEN(password, MAX_ICE_CONFIG_CREDENTIAL_LEN + 1) <= MAX_ICE_CONFIG_CREDENTIAL_LEN,
        STATUS_INVALID_ARG);

    // allocate the entire struct
    pIceAgent = (PIceAgent) MEMCALLOC(1, SIZEOF(IceAgent));
    STRNCPY(pIceAgent->localUsername, username, MAX_ICE_CONFIG_USER_NAME_LEN);
    STRNCPY(pIceAgent->localPassword, password, MAX_ICE_CONFIG_CREDENTIAL_LEN);

    ATOMIC_STORE_BOOL(&pIceAgent->remoteCredentialReceived, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->agentStartGathering, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->candidateGatheringFinished, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->shutdown, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->restart, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->processStun, TRUE);
    pIceAgent->isControlling = FALSE;
    pIceAgent->tieBreaker = (UINT64) RAND();
    pIceAgent->iceTransportPolicy = pRtcConfiguration->iceTransportPolicy;
    pIceAgent->kvsRtcConfiguration = pRtcConfiguration->kvsRtcConfiguration;
    CHK_STATUS(iceAgentValidateKvsRtcConfig(&pIceAgent->kvsRtcConfiguration));

    if (pIceAgentCallbacks != NULL) {
        pIceAgent->iceAgentCallbacks = *pIceAgentCallbacks;
    }
    pIceAgent->stateEndTime = 0;
    pIceAgent->foundationCounter = 0;
    pIceAgent->localNetworkInterfaceCount = ARRAY_SIZE(pIceAgent->localNetworkInterfaces);
    pIceAgent->candidateGatheringEndTime = INVALID_TIMESTAMP_VALUE;

    pIceAgent->lock = MUTEX_CREATE(FALSE);

    // Create the state machine
    CHK_STATUS(createStateMachine(ICE_AGENT_STATE_MACHINE_STATES, ICE_AGENT_STATE_MACHINE_STATE_COUNT, (UINT64) pIceAgent, iceAgentGetCurrentTime,
                                  (UINT64) pIceAgent, &pIceAgent->pStateMachine));
    pIceAgent->iceAgentStatus = STATUS_SUCCESS;
    pIceAgent->iceAgentStateTimerTask = MAX_UINT32;
    pIceAgent->keepAliveTimerTask = MAX_UINT32;
    pIceAgent->iceCandidateGatheringTimerTask = MAX_UINT32;
    pIceAgent->timerQueueHandle = timerQueueHandle;
    pIceAgent->lastDataReceivedTime = INVALID_TIMESTAMP_VALUE;
    pIceAgent->detectedDisconnection = FALSE;
    pIceAgent->disconnectionGracePeriodEndTime = INVALID_TIMESTAMP_VALUE;
    pIceAgent->pConnectionListener = pConnectionListener;
    pIceAgent->pDataSendingIceCandidatePair = NULL;
    CHK_STATUS(createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pIceAgent->pStunBindingRequestTransactionIdStore));

    pIceAgent->relayCandidateCount = 0;

    CHK_STATUS(doubleListCreate(&pIceAgent->localCandidates));
    CHK_STATUS(doubleListCreate(&pIceAgent->remoteCandidates));
    CHK_STATUS(doubleListCreate(&pIceAgent->iceCandidatePairs));
    CHK_STATUS(stackQueueCreate(&pIceAgent->triggeredCheckQueue));

    // Pre-allocate stun packets

    // no other attribtues needed: https://tools.ietf.org/html/rfc8445#section-11
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_INDICATION, NULL, &pIceAgent->pBindingIndication));
    CHK_STATUS(hashTableCreateWithParams(ICE_HASH_TABLE_BUCKET_COUNT, ICE_HASH_TABLE_BUCKET_LENGTH, &pIceAgent->requestTimestampDiagnostics));

    pIceAgent->iceServersCount = 0;
    for (i = 0; i < MAX_ICE_SERVERS_COUNT; i++) {
        if (pRtcConfiguration->iceServers[i].urls[0] != '\0' &&
            STATUS_SUCCEEDED(parseIceServer(&pIceAgent->iceServers[pIceAgent->iceServersCount], (PCHAR) pRtcConfiguration->iceServers[i].urls,
                                            (PCHAR) pRtcConfiguration->iceServers[i].username,
                                            (PCHAR) pRtcConfiguration->iceServers[i].credential))) {
            pIceAgent->rtcIceServerDiagnostics[i].port = (INT32) getInt16(pIceAgent->iceServers[i].ipAddress.port);
            switch (pIceAgent->iceServers[pIceAgent->iceServersCount].transport) {
                case KVS_SOCKET_PROTOCOL_UDP:
                    STRCPY(pIceAgent->rtcIceServerDiagnostics[i].protocol, ICE_URL_TRANSPORT_UDP);
                    break;
                case KVS_SOCKET_PROTOCOL_TCP:
                    STRCPY(pIceAgent->rtcIceServerDiagnostics[i].protocol, ICE_URL_TRANSPORT_TCP);
                    break;
                default:
                    MEMSET(pIceAgent->rtcIceServerDiagnostics[i].protocol, 0, SIZEOF(pIceAgent->rtcIceServerDiagnostics[i].protocol));
            }
            STRCPY(pIceAgent->rtcIceServerDiagnostics[i].url, pRtcConfiguration->iceServers[i].urls);
            pIceAgent->iceServersCount++;
        }
    }

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
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidatePair pIceCandidatePair = NULL;
    PIceCandidate pIceCandidate = NULL;

    CHK(ppIceAgent != NULL, STATUS_NULL_ARG);
    // freeIceAgent is idempotent
    CHK(*ppIceAgent != NULL, retStatus);

    pIceAgent = *ppIceAgent;

    hashTableFree(pIceAgent->requestTimestampDiagnostics);

    if (pIceAgent->localCandidates != NULL) {
        CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
        while (pCurNode != NULL) {
            pIceCandidate = (PIceCandidate) pCurNode->data;
            pCurNode = pCurNode->pNext;

            if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
                CHK_LOG_ERR(freeTurnConnection(&pIceCandidate->pTurnConnection));
            }
        }
    }

    if (pIceAgent->pConnectionListener != NULL) {
        CHK_LOG_ERR(freeConnectionListener(&pIceAgent->pConnectionListener));
    }

    if (pIceAgent->iceCandidatePairs != NULL) {
        CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pIceCandidatePair = (PIceCandidatePair) data;

            CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
        }

        CHK_LOG_ERR(doubleListClear(pIceAgent->iceCandidatePairs, FALSE));
        CHK_LOG_ERR(doubleListFree(pIceAgent->iceCandidatePairs));
    }

    if (pIceAgent->localCandidates != NULL) {
        CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pCurNode = pCurNode->pNext;
            pIceCandidate = (PIceCandidate) data;

            /* turn sockets are freed by freeTurnConnection */
            if (pIceCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_RELAYED) {
                CHK_LOG_ERR(freeSocketConnection(&pIceCandidate->pSocketConnection));
            }
        }
        // free all stored candidates
        CHK_LOG_ERR(doubleListClear(pIceAgent->localCandidates, TRUE));
        CHK_LOG_ERR(doubleListFree(pIceAgent->localCandidates));
    }

    /* In case we fail in the middle of a ICE restart */
    if (ATOMIC_LOAD_BOOL(&pIceAgent->restart) && pIceAgent->pDataSendingIceCandidatePair != NULL) {
        if (IS_CANN_PAIR_SENDING_FROM_RELAYED(pIceAgent->pDataSendingIceCandidatePair)) {
            CHK_LOG_ERR(freeTurnConnection(&pIceAgent->pDataSendingIceCandidatePair->local->pTurnConnection));
        } else {
            CHK_LOG_ERR(freeSocketConnection(&pIceAgent->pDataSendingIceCandidatePair->local->pSocketConnection));
        }

        MEMFREE(pIceAgent->pDataSendingIceCandidatePair->local);
        CHK_LOG_ERR(freeIceCandidatePair(&pIceAgent->pDataSendingIceCandidatePair));

        pIceAgent->pDataSendingIceCandidatePair = NULL;
    }

    if (pIceAgent->remoteCandidates != NULL) {
        // remote candidates dont have socketConnection
        CHK_LOG_ERR(doubleListClear(pIceAgent->remoteCandidates, TRUE));
        CHK_LOG_ERR(doubleListFree(pIceAgent->remoteCandidates));
    }

    if (pIceAgent->triggeredCheckQueue != NULL) {
        CHK_LOG_ERR(stackQueueFree(pIceAgent->triggeredCheckQueue));
    }

    if (IS_VALID_MUTEX_VALUE(pIceAgent->lock)) {
        MUTEX_FREE(pIceAgent->lock);
    }

    freeStateMachine(pIceAgent->pStateMachine);

    if (pIceAgent->pBindingIndication != NULL) {
        freeStunPacket(&pIceAgent->pBindingIndication);
    }

    if (pIceAgent->pBindingRequest != NULL) {
        freeStunPacket(&pIceAgent->pBindingRequest);
    }

    if (pIceAgent->pStunBindingRequestTransactionIdStore != NULL) {
        freeTransactionIdStore(&pIceAgent->pStunBindingRequestTransactionIdStore);
    }

    MEMFREE(pIceAgent);

    *ppIceAgent = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS iceAgentValidateKvsRtcConfig(PKvsRtcConfiguration pKvsRtcConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pKvsRtcConfiguration != NULL, STATUS_NULL_ARG);

    if (pKvsRtcConfiguration->iceLocalCandidateGatheringTimeout == 0) {
        pKvsRtcConfiguration->iceLocalCandidateGatheringTimeout = KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT;
    }

    if (pKvsRtcConfiguration->iceConnectionCheckTimeout == 0) {
        pKvsRtcConfiguration->iceConnectionCheckTimeout = KVS_ICE_CONNECTIVITY_CHECK_TIMEOUT;
    }

    if (pKvsRtcConfiguration->iceCandidateNominationTimeout == 0) {
        pKvsRtcConfiguration->iceCandidateNominationTimeout = KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;
    }

    if (pKvsRtcConfiguration->iceConnectionCheckPollingInterval == 0) {
        pKvsRtcConfiguration->iceConnectionCheckPollingInterval = KVS_ICE_CONNECTION_CHECK_POLLING_INTERVAL;
    }

    DLOGD("\n\ticeLocalCandidateGatheringTimeout: %u ms"
          "\n\ticeConnectionCheckTimeout: %u ms"
          "\n\ticeCandidateNominationTimeout: %u ms"
          "\n\ticeConnectionCheckPollingInterval: %u ms",
          pKvsRtcConfiguration->iceLocalCandidateGatheringTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          pKvsRtcConfiguration->iceConnectionCheckTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          pKvsRtcConfiguration->iceCandidateNominationTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
          pKvsRtcConfiguration->iceConnectionCheckPollingInterval / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

CleanUp:

    return retStatus;
}

STATUS iceAgentReportNewLocalCandidate(PIceAgent pIceAgent, PIceCandidate pIceCandidate)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR serializedIceCandidateBuf[MAX_SDP_ATTRIBUTE_VALUE_LENGTH];
    UINT32 serializedIceCandidateBufLen = ARRAY_SIZE(serializedIceCandidateBuf);

    CHK(pIceAgent != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);

    iceAgentLogNewCandidate(pIceCandidate);

    CHK_WARN(pIceAgent->iceAgentCallbacks.newLocalCandidateFn != NULL, retStatus, "newLocalCandidateFn callback not implemented");
    CHK_WARN(!ATOMIC_LOAD_BOOL(&pIceAgent->candidateGatheringFinished), retStatus,
             "Cannot report new ice candidate because candidate gathering is already finished");
    CHK_STATUS(iceCandidateSerialize(pIceCandidate, serializedIceCandidateBuf, &serializedIceCandidateBufLen));
    pIceAgent->iceAgentCallbacks.newLocalCandidateFn(pIceAgent->iceAgentCallbacks.customData, serializedIceCandidateBuf);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS iceAgentAddRemoteCandidate(PIceAgent pIceAgent, PCHAR pIceCandidateString)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PIceCandidate pIceCandidate = NULL, pDuplicatedIceCandidate = NULL, pLocalIceCandidate = NULL;
    PCHAR curr, tail, next;
    UINT32 tokenLen, portValue, remoteCandidateCount, len;
    BOOL freeIceCandidateIfFail = TRUE;
    BOOL foundIp = FALSE, foundPort = FALSE;
    CHAR ipBuf[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    KvsIpAddress candidateIpAddr;
    PDoubleListNode pCurNode = NULL;
    SDP_ICE_CANDIDATE_PARSER_STATE state;

    CHK(pIceAgent != NULL && pIceCandidateString != NULL, STATUS_NULL_ARG);
    CHK(!IS_EMPTY_STRING(pIceCandidateString), STATUS_INVALID_ARG);

    MEMSET(&candidateIpAddr, 0x00, SIZEOF(KvsIpAddress));

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->remoteCandidates, &remoteCandidateCount));
    CHK(remoteCandidateCount < KVS_ICE_MAX_REMOTE_CANDIDATE_COUNT, STATUS_ICE_MAX_REMOTE_CANDIDATE_COUNT_EXCEEDED);
    // a=candidate:4234997325 1 udp 2043278322 192.168.0.56 44323 typ host
    curr = pIceCandidateString;
    tail = pIceCandidateString + STRLEN(pIceCandidateString);
    state = SDP_ICE_CANDIDATE_PARSER_STATE_FOUNDATION;

    while ((next = STRNCHR(curr, tail - curr, ' ')) != NULL && !(foundIp && foundPort)) {
        tokenLen = (UINT32)(next - curr);

        switch (state) {
            case SDP_ICE_CANDIDATE_PARSER_STATE_FOUNDATION:
            case SDP_ICE_CANDIDATE_PARSER_STATE_COMPONENT:
            case SDP_ICE_CANDIDATE_PARSER_STATE_PORIORITY:
                break;
            case SDP_ICE_CANDIDATE_PARSER_STATE_PROTOCOL:
                CHK(STRNCMPI("tcp", curr, tokenLen) != 0, STATUS_ICE_CANDIDATE_STRING_IS_TCP);
                break;
            case SDP_ICE_CANDIDATE_PARSER_STATE_IP:
                len = MIN(next - curr, KVS_IP_ADDRESS_STRING_BUFFER_LEN - 1);
                STRNCPY(ipBuf, curr, len);
                ipBuf[len] = '\0';
                if ((foundIp = inet_pton(AF_INET, ipBuf, candidateIpAddr.address) == 1 ? TRUE : FALSE)) {
                    candidateIpAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
                } else if ((foundIp = inet_pton(AF_INET6, ipBuf, candidateIpAddr.address) == 1 ? TRUE : FALSE)) {
                    candidateIpAddr.family = KVS_IP_FAMILY_TYPE_IPV6;
                }
                CHK(foundIp, STATUS_ICE_CANDIDATE_STRING_MISSING_IP);
                break;
            case SDP_ICE_CANDIDATE_PARSER_STATE_PORT:
                CHK_STATUS(STRTOUI32(curr, curr + tokenLen, 10, &portValue));
                candidateIpAddr.port = htons(portValue);
                foundPort = TRUE;
                break;
            default:
                DLOGW("supposedly does not happen.");
                break;
        }
        state++;
        curr = next + 1;
    }

    CHK(foundPort, STATUS_ICE_CANDIDATE_STRING_MISSING_PORT);
    CHK(foundIp, STATUS_ICE_CANDIDATE_STRING_MISSING_IP);

    CHK_STATUS(findCandidateWithIp(&candidateIpAddr, pIceAgent->remoteCandidates, &pDuplicatedIceCandidate));
    CHK(pDuplicatedIceCandidate == NULL, retStatus);

    CHK((pIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);
    generateJSONSafeString(pIceCandidate->id, ARRAY_SIZE(pIceCandidate->id));
    pIceCandidate->isRemote = TRUE;
    pIceCandidate->ipAddress = candidateIpAddr;
    pIceCandidate->state = ICE_CANDIDATE_STATE_VALID;
    CHK_STATUS(doubleListInsertItemHead(pIceAgent->remoteCandidates, (UINT64) pIceCandidate));
    freeIceCandidateIfFail = FALSE;

    CHK_STATUS(createIceCandidatePairs(pIceAgent, pIceCandidate, TRUE));

    iceAgentLogNewCandidate(pIceCandidate);

    /* pass remote candidate to each turnConnection */
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        pLocalIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pLocalIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            CHK_STATUS(turnConnectionAddPeer(pLocalIceCandidate->pTurnConnection, &pIceCandidate->ipAddress));
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus) && freeIceCandidateIfFail) {
        SAFE_MEMFREE(pIceCandidate);
    }

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS iceAgentInitHostCandidate(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PKvsIpAddress pIpAddress = NULL;
    PIceCandidate pTmpIceCandidate = NULL, pDuplicatedIceCandidate = NULL, pNewIceCandidate = NULL;
    UINT32 i, localCandidateCount = 0;
    PSocketConnection pSocketConnection = NULL;
    BOOL locked = FALSE;

    for (i = 0; i < pIceAgent->localNetworkInterfaceCount; ++i) {
        pIpAddress = &pIceAgent->localNetworkInterfaces[i];

        // make sure pIceAgent->localCandidates has no duplicates
        CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->localCandidates, &pDuplicatedIceCandidate));

        if (pDuplicatedIceCandidate == NULL &&
            STATUS_SUCCEEDED(createSocketConnection(pIpAddress->family, KVS_SOCKET_PROTOCOL_UDP, pIpAddress, NULL, (UINT64) pIceAgent,
                                                    incomingDataHandler, pIceAgent->kvsRtcConfiguration.sendBufSize, &pSocketConnection))) {
            pTmpIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate));
            generateJSONSafeString(pTmpIceCandidate->id, ARRAY_SIZE(pTmpIceCandidate->id));
            pTmpIceCandidate->isRemote = FALSE;
            pTmpIceCandidate->ipAddress = *pIpAddress;
            pTmpIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_HOST;
            pTmpIceCandidate->state = ICE_CANDIDATE_STATE_VALID;
            // we dont generate candidates that have the same foundation.
            pTmpIceCandidate->foundation = pIceAgent->foundationCounter++;
            pTmpIceCandidate->pSocketConnection = pSocketConnection;
            pTmpIceCandidate->priority = computeCandidatePriority(pTmpIceCandidate);

            /* Another thread could be calling iceAgentAddRemoteCandidate which triggers createIceCandidatePairs.
             * createIceCandidatePairs will read through localCandidates, since we are mutating localCandidates here,
             * need to acquire lock. */
            MUTEX_LOCK(pIceAgent->lock);
            locked = TRUE;

            CHK_STATUS(doubleListInsertItemHead(pIceAgent->localCandidates, (UINT64) pTmpIceCandidate));
            CHK_STATUS(createIceCandidatePairs(pIceAgent, pTmpIceCandidate, FALSE));

            MUTEX_UNLOCK(pIceAgent->lock);
            locked = FALSE;

            localCandidateCount++;
            // make a copy of pTmpIceCandidate so that if iceAgentReportNewLocalCandidate fails pTmpIceCandidate wont get freed.
            pNewIceCandidate = pTmpIceCandidate;
            pTmpIceCandidate = NULL;

            ATOMIC_STORE_BOOL(&pSocketConnection->receiveData, TRUE);
            // connectionListener will free the pSocketConnection at the end.
            CHK_STATUS(connectionListenerAddConnection(pIceAgent->pConnectionListener, pNewIceCandidate->pSocketConnection));
        }
    }

    CHK(localCandidateCount != 0, STATUS_ICE_NO_LOCAL_HOST_CANDIDATE_AVAILABLE);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    SAFE_MEMFREE(pTmpIceCandidate);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    LEAVES();
    return retStatus;
}

STATUS iceAgentStartAgent(PIceAgent pIceAgent, PCHAR remoteUsername, PCHAR remotePassword, BOOL isControlling)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL && remoteUsername != NULL && remotePassword != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pIceAgent->remoteCredentialReceived), retStatus); // make iceAgentStartAgent idempotent
    CHK(STRNLEN(remoteUsername, MAX_ICE_CONFIG_USER_NAME_LEN + 1) <= MAX_ICE_CONFIG_USER_NAME_LEN &&
            STRNLEN(remotePassword, MAX_ICE_CONFIG_CREDENTIAL_LEN + 1) <= MAX_ICE_CONFIG_CREDENTIAL_LEN,
        STATUS_INVALID_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    ATOMIC_STORE_BOOL(&pIceAgent->remoteCredentialReceived, TRUE);
    /* role should not change during ice restart. */
    if (!ATOMIC_LOAD_BOOL(&pIceAgent->restart)) {
        pIceAgent->isControlling = isControlling;
    }

    STRNCPY(pIceAgent->remoteUsername, remoteUsername, MAX_ICE_CONFIG_USER_NAME_LEN);
    STRNCPY(pIceAgent->remotePassword, remotePassword, MAX_ICE_CONFIG_CREDENTIAL_LEN);
    if (STRLEN(pIceAgent->remoteUsername) + STRLEN(pIceAgent->localUsername) + 1 > MAX_ICE_CONFIG_USER_NAME_LEN) {
        DLOGW("remoteUsername:localUsername will be truncated to stay within %u char limit", MAX_ICE_CONFIG_USER_NAME_LEN);
    }
    SNPRINTF(pIceAgent->combinedUserName, ARRAY_SIZE(pIceAgent->combinedUserName), "%s:%s", pIceAgent->remoteUsername, pIceAgent->localUsername);

    MUTEX_UNLOCK(pIceAgent->lock);
    locked = FALSE;

    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle, KVS_ICE_DEFAULT_TIMER_START_DELAY,
                                  pIceAgent->kvsRtcConfiguration.iceConnectionCheckPollingInterval, iceAgentStateTransitionTimerCallback,
                                  (UINT64) pIceAgent, &pIceAgent->iceAgentStateTimerTask));

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

    ATOMIC_STORE_BOOL(&pIceAgent->agentStartGathering, TRUE);

    CHK_STATUS(getLocalhostIpAddresses(pIceAgent->localNetworkInterfaces, &pIceAgent->localNetworkInterfaceCount,
                                       pIceAgent->kvsRtcConfiguration.iceSetInterfaceFilterFunc, pIceAgent->kvsRtcConfiguration.filterCustomData));

    // skip gathering host candidate and srflx candidate if relay only
    if (pIceAgent->iceTransportPolicy != ICE_TRANSPORT_POLICY_RELAY) {
        CHK_STATUS(iceAgentInitHostCandidate(pIceAgent));
        CHK_STATUS(iceAgentInitSrflxCandidate(pIceAgent));
    }

    CHK_STATUS(iceAgentInitRelayCandidates(pIceAgent));

    // start listening for incoming data
    CHK_STATUS(connectionListenerStart(pIceAgent->pConnectionListener));

    pIceAgent->candidateGatheringEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceLocalCandidateGatheringTimeout;

    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle, KVS_ICE_DEFAULT_TIMER_START_DELAY, KVS_ICE_GATHER_CANDIDATE_TIMER_POLLING_INTERVAL,
                                  iceAgentGatherCandidateTimerCallback, (UINT64) pIceAgent, &pIceAgent->iceCandidateGatheringTimerTask));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentSendPacket(PIceAgent pIceAgent, PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, isRelay = FALSE;
    PTurnConnection pTurnConnection = NULL;
    UINT32 packetsDiscarded = 0;
    UINT32 bytesDiscarded = 0;
    UINT32 bytesSent = 0;
    UINT32 packetsSent = 0;

    CHK(pIceAgent != NULL && pBuffer != NULL, STATUS_NULL_ARG);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    /* Do not proceed if ice is shutting down */
    CHK(!ATOMIC_LOAD_BOOL(&pIceAgent->shutdown), retStatus);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    CHK_WARN(pIceAgent->pDataSendingIceCandidatePair != NULL, retStatus, "No valid ice candidate pair available to send data");
    CHK_WARN(pIceAgent->pDataSendingIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED, retStatus,
             "Invalid state for data sending candidate pair.");

    pIceAgent->pDataSendingIceCandidatePair->lastDataSentTime = GETTIME();

    isRelay = IS_CANN_PAIR_SENDING_FROM_RELAYED(pIceAgent->pDataSendingIceCandidatePair);
    if (isRelay) {
        CHK_ERR(pIceAgent->pDataSendingIceCandidatePair->local->pTurnConnection != NULL, STATUS_NULL_ARG,
                "Candidate is relay but pTurnConnection is NULL");
        pTurnConnection = pIceAgent->pDataSendingIceCandidatePair->local->pTurnConnection;
    }

    retStatus = iceUtilsSendData(pBuffer, bufferLen, &pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress,
                                 pIceAgent->pDataSendingIceCandidatePair->local->pSocketConnection, pTurnConnection, isRelay);

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceUtilsSendData failed with 0x%08x", retStatus);
        packetsDiscarded++;
        bytesDiscarded = bufferLen; // This includes header and padding. TODO: update length to remove header and padding
        if (retStatus == STATUS_SOCKET_CONNECTION_CLOSED_ALREADY) {
            DLOGW("IceAgent connection closed unexpectedly");
            pIceAgent->iceAgentStatus = STATUS_SOCKET_CONNECTION_CLOSED_ALREADY;
            pIceAgent->pDataSendingIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_FAILED;
        }
        retStatus = STATUS_SUCCESS;
    } else {
        bytesSent = bufferLen;
        packetsSent++;
    }

CleanUp:

    if (STATUS_SUCCEEDED(retStatus) && pIceAgent->pDataSendingIceCandidatePair != NULL) {
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.packetsDiscardedOnSend += packetsDiscarded;
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.bytesDiscardedOnSend += bytesDiscarded;
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.state = pIceAgent->pDataSendingIceCandidatePair->state;
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.lastPacketSentTimestamp =
            pIceAgent->pDataSendingIceCandidatePair->lastDataSentTime;
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.bytesSent += bytesSent;
        pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.packetsSent += packetsSent;
    }
    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentPopulateSdpMediaDescriptionCandidates(PIceAgent pIceAgent, PSdpMediaDescription pSdpMediaDescription, UINT32 attrBufferLen,
                                                     PUINT32 pIndex)
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

STATUS iceAgentShutdown(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, turnShutdownCompleted = FALSE;
    PDoubleListNode pCurNode = NULL;
    PIceCandidate pLocalCandidate = NULL;
    UINT32 i;
    UINT64 turnShutdownTimeout;
    const UINT64 shortSleep = 50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    PTurnConnection turnConnections[KVS_ICE_MAX_RELAY_CANDIDATE_COUNT];
    UINT32 turnConnectionCount = 0;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_EXCHANGE_BOOL(&pIceAgent->shutdown, TRUE), retStatus);

    if (pIceAgent->iceAgentStateTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerTask, (UINT64) pIceAgent));
        pIceAgent->iceAgentStateTimerTask = MAX_UINT32;
    }

    if (pIceAgent->keepAliveTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->keepAliveTimerTask, (UINT64) pIceAgent));
        pIceAgent->keepAliveTimerTask = MAX_UINT32;
    }

    if (pIceAgent->iceCandidateGatheringTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceCandidateGatheringTimerTask, (UINT64) pIceAgent));
        pIceAgent->iceCandidateGatheringTimerTask = MAX_UINT32;
    }

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        pLocalCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pLocalCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_RELAYED) {
            /* close socket so ice doesnt receive any more data */
            CHK_STATUS(socketConnectionClosed(pLocalCandidate->pSocketConnection));
        } else {
            CHK_STATUS(turnConnectionShutdown(pLocalCandidate->pTurnConnection, 0));
            turnConnections[turnConnectionCount++] = pLocalCandidate->pTurnConnection;
        }
    }

    MUTEX_UNLOCK(pIceAgent->lock);
    locked = FALSE;

    turnShutdownTimeout = GETTIME() + KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT;
    while (!turnShutdownCompleted && GETTIME() < turnShutdownTimeout) {
        turnShutdownCompleted = TRUE;
        for (i = 0; i < turnConnectionCount; ++i) {
            if (!turnConnectionIsShutdownComplete(turnConnections[i])) {
                turnShutdownCompleted = FALSE;
            }
        }

        THREAD_SLEEP(shortSleep);
    }

    if (!turnShutdownCompleted) {
        DLOGW("TurnConnection shutdown did not complete within %u seconds", KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT / HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    /* remove connections last because still need to send data to deallocate turn */
    if (pIceAgent->pConnectionListener != NULL) {
        CHK_STATUS(connectionListenerRemoveAllConnection(pIceAgent->pConnectionListener));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentRestart(PIceAgent pIceAgent, PCHAR localIceUfrag, PCHAR localIcePwd)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PDoubleListNode pCurNode = NULL, pNextNode = NULL;
    PIceCandidate pLocalCandidate = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    UINT32 i;
    ATOMIC_BOOL alreadyRestarting;
    PIceCandidate localCandidates[KVS_ICE_MAX_LOCAL_CANDIDATE_COUNT];
    UINT32 localCandidateCount = 0;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pIceAgent->shutdown), STATUS_INVALID_OPERATION);

    DLOGD("Restarting ICE");

    alreadyRestarting = ATOMIC_EXCHANGE_BOOL(&pIceAgent->restart, TRUE);
    CHK(!alreadyRestarting, retStatus);

    if (pIceAgent->iceAgentStateTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceAgentStateTimerTask, (UINT64) pIceAgent));
        pIceAgent->iceAgentStateTimerTask = MAX_UINT32;
    }

    if (pIceAgent->keepAliveTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->keepAliveTimerTask, (UINT64) pIceAgent));
        pIceAgent->keepAliveTimerTask = MAX_UINT32;
    }

    if (pIceAgent->iceCandidateGatheringTimerTask != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pIceAgent->timerQueueHandle, pIceAgent->iceCandidateGatheringTimerTask, (UINT64) pIceAgent));
        pIceAgent->iceCandidateGatheringTimerTask = MAX_UINT32;
    }

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    /* do not process anymore stun messages because it may need to access resources like iceCandidiate which we are
     * about to free */
    ATOMIC_STORE_BOOL(&pIceAgent->processStun, FALSE);
    pIceAgent->iceAgentStatus = STATUS_SUCCESS;
    pIceAgent->lastDataReceivedTime = INVALID_TIMESTAMP_VALUE;

    pIceAgent->relayCandidateCount = 0;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        pLocalCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pLocalCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            CHK_STATUS(turnConnectionShutdown(pLocalCandidate->pTurnConnection, 0));
        }
        localCandidates[localCandidateCount++] = pLocalCandidate;
    }
    CHK_STATUS(doubleListClear(pIceAgent->localCandidates, FALSE));

    /* free all candidate pairs except the selected pair */
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pNextNode = pCurNode->pNext;

        if (pIceCandidatePair != pIceAgent->pDataSendingIceCandidatePair) {
            CHK_STATUS(freeIceCandidatePair(&pIceCandidatePair));
        }

        pCurNode = pNextNode;
    }
    CHK_STATUS(doubleListClear(pIceAgent->iceCandidatePairs, FALSE));

    MUTEX_UNLOCK(pIceAgent->lock);
    locked = FALSE;

    /* Time given for turn to free its allocation */
    THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);

    /* At this point there should be no thread accessing anything in iceAgent other than
     * pIceAgent->pDataSendingIceCandidatePair and its ice candidates. Therefore safe to proceed freeing resources */

    for (i = 0; i < localCandidateCount; ++i) {
        if (localCandidates[i] != pIceAgent->pDataSendingIceCandidatePair->local) {
            if (localCandidates[i]->iceCandidateType != ICE_CANDIDATE_TYPE_RELAYED) {
                CHK_STATUS(connectionListenerRemoveConnection(pIceAgent->pConnectionListener, localCandidates[i]->pSocketConnection));
                CHK_STATUS(freeSocketConnection(&localCandidates[i]->pSocketConnection));
            } else {
                CHK_STATUS(freeTurnConnection(&localCandidates[i]->pTurnConnection));
            }
            MEMFREE(localCandidates[i]);
        }
    }

    /* Do not free remoteCandidates because new remote candidates could be added before or while restart happens.
     * There is no way to tell which session a remote candidate belongs to. Old ones will eventually fail the
     * connectivity test so it's ok. */

    CHK_STATUS(stackQueueClear(pIceAgent->triggeredCheckQueue, FALSE));

    ATOMIC_STORE_BOOL(&pIceAgent->remoteCredentialReceived, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->agentStartGathering, FALSE);
    ATOMIC_STORE_BOOL(&pIceAgent->candidateGatheringFinished, FALSE);

    pIceAgent->stateEndTime = 0;
    pIceAgent->foundationCounter = 0;
    pIceAgent->localNetworkInterfaceCount = ARRAY_SIZE(pIceAgent->localNetworkInterfaces);
    pIceAgent->candidateGatheringEndTime = INVALID_TIMESTAMP_VALUE;

    pIceAgent->iceAgentStateTimerTask = MAX_UINT32;
    pIceAgent->keepAliveTimerTask = MAX_UINT32;
    pIceAgent->iceCandidateGatheringTimerTask = MAX_UINT32;
    pIceAgent->detectedDisconnection = FALSE;
    pIceAgent->disconnectionGracePeriodEndTime = INVALID_TIMESTAMP_VALUE;

    transactionIdStoreClear(pIceAgent->pStunBindingRequestTransactionIdStore);

    STRNCPY(pIceAgent->localUsername, localIceUfrag, MAX_ICE_CONFIG_USER_NAME_LEN);
    STRNCPY(pIceAgent->localPassword, localIcePwd, MAX_ICE_CONFIG_CREDENTIAL_LEN);

    pIceAgent->iceAgentState = ICE_AGENT_STATE_NEW;
    CHK_STATUS(setStateMachineCurrentState(pIceAgent->pStateMachine, ICE_AGENT_STATE_NEW));

    ATOMIC_STORE_BOOL(&pIceAgent->processStun, TRUE);

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

//////////////////////////////////////////////
// internal functions
//////////////////////////////////////////////

STATUS findCandidateWithIp(PKvsIpAddress pIpAddress, PDoubleList pCandidateList, PIceCandidate* ppIceCandidate)
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
        if (pIpAddress->family == pIceCandidate->ipAddress.family && MEMCMP(pIceCandidate->ipAddress.address, pIpAddress->address, addrLen) == 0 &&
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

STATUS findCandidateWithSocketConnection(PSocketConnection pSocketConnection, PDoubleList pCandidateList, PIceCandidate* ppIceCandidate)
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

/*
 * Need to acquire pIceAgent->lock first
 */
STATUS createIceCandidatePairs(PIceAgent pIceAgent, PIceCandidate pIceCandidate, BOOL isRemoteCandidate)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT64 data;
    PDoubleListNode pCurNode = NULL;
    PDoubleList pDoubleList = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    BOOL freeObjOnFailure = TRUE;
    PIceCandidate pCurrentIceCandidate = NULL;

    CHK(pIceAgent != NULL && pIceCandidate != NULL, STATUS_NULL_ARG);
    CHK_WARN(pIceCandidate->state == ICE_CANDIDATE_STATE_VALID, retStatus, "New ice candidate need to be valid to form pairs");

    // if pIceCandidate is a remote candidate, then form pairs with every single valid local candidate. Otherwize,
    // form pairs with every single valid remote candidate
    pDoubleList = isRemoteCandidate ? pIceAgent->localCandidates : pIceAgent->remoteCandidates;

    CHK_STATUS(doubleListGetHeadNode(pDoubleList, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurrentIceCandidate = (PIceCandidate) data;
        pCurNode = pCurNode->pNext;

        // https://tools.ietf.org/html/rfc8445#section-6.1.2.2
        // pair local and remote candidates with the same family
        if (pCurrentIceCandidate->state == ICE_CANDIDATE_STATE_VALID && pCurrentIceCandidate->ipAddress.family == pIceCandidate->ipAddress.family) {
            pIceCandidatePair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
            CHK(pIceCandidatePair != NULL, STATUS_NOT_ENOUGH_MEMORY);

            if (isRemoteCandidate) {
                pIceCandidatePair->local = (PIceCandidate) data;
                pIceCandidatePair->remote = pIceCandidate;
            } else {
                pIceCandidatePair->local = pIceCandidate;
                pIceCandidatePair->remote = (PIceCandidate) data;
            }
            pIceCandidatePair->nominated = FALSE;

            // ensure the new pair will go through connectivity check as soon as possible
            pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_WAITING;

            CHK_STATUS(createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pIceCandidatePair->pTransactionIdStore));
            CHK_STATUS(hashTableCreateWithParams(ICE_HASH_TABLE_BUCKET_COUNT, ICE_HASH_TABLE_BUCKET_LENGTH, &pIceCandidatePair->requestSentTime));

            pIceCandidatePair->lastDataSentTime = 0;
            STRNCPY(pIceCandidatePair->rtcIceCandidatePairDiagnostics.localCandidateId, pIceCandidatePair->local->id,
                    ARRAY_SIZE(pIceCandidatePair->rtcIceCandidatePairDiagnostics.localCandidateId));
            STRNCPY(pIceCandidatePair->rtcIceCandidatePairDiagnostics.remoteCandidateId, pIceCandidatePair->remote->id,
                    ARRAY_SIZE(pIceCandidatePair->rtcIceCandidatePairDiagnostics.remoteCandidateId));
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.state = pIceCandidatePair->state;
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.nominated = pIceCandidatePair->nominated;
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.lastPacketSentTimestamp = pIceCandidatePair->lastDataSentTime;
            pIceCandidatePair->firstStunRequest = TRUE;
            pIceCandidatePair->priority = computeCandidatePairPriority(pIceCandidatePair, pIceAgent->isControlling);
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.totalRoundTripTime = 0.0;
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.currentRoundTripTime = 0.0;
            // Set data sending ICE candidate pair stats
            NULLABLE_SET_EMPTY(pIceCandidatePair->rtcIceCandidatePairDiagnostics.circuitBreakerTriggerCount);
            CHK_STATUS(insertIceCandidatePair(pIceAgent->iceCandidatePairs, pIceCandidatePair));
            freeObjOnFailure = FALSE;
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && freeObjOnFailure) {
        freeIceCandidatePair(&pIceCandidatePair);
    }

    LEAVES();
    return retStatus;
}

STATUS freeIceCandidatePair(PIceCandidatePair* ppIceCandidatePair)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(ppIceCandidatePair != NULL, STATUS_NULL_ARG);
    // free is idempotent
    CHK(*ppIceCandidatePair != NULL, retStatus);
    pIceCandidatePair = *ppIceCandidatePair;

    CHK_LOG_ERR(freeTransactionIdStore(&pIceCandidatePair->pTransactionIdStore));
    CHK_LOG_ERR(hashTableFree(pIceCandidatePair->requestSentTime));
    SAFE_MEMFREE(pIceCandidatePair);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS insertIceCandidatePair(PDoubleList iceCandidatePairs, PIceCandidatePair pIceCandidatePair)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pCurIceCandidatePair = NULL;

    CHK(iceCandidatePairs != NULL && pIceCandidatePair != NULL, STATUS_NULL_ARG);

    CHK_STATUS(doubleListGetHeadNode(iceCandidatePairs, &pCurNode));

    while (pCurNode != NULL) {
        pCurIceCandidatePair = (PIceCandidatePair) pCurNode->data;

        // insert new candidate pair ordered by priority from max to min.
        if (pCurIceCandidatePair->priority <= pIceCandidatePair->priority) {
            break;
        }
        pCurNode = pCurNode->pNext;
    }

    if (pCurNode != NULL) {
        CHK_STATUS(doubleListInsertItemBefore(iceCandidatePairs, pCurNode, (UINT64) pIceCandidatePair));
    } else {
        CHK_STATUS(doubleListInsertItemTail(iceCandidatePairs, (UINT64) pIceCandidatePair));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS findIceCandidatePairWithLocalSocketConnectionAndRemoteAddr(PIceAgent pIceAgent, PSocketConnection pSocketConnection, PKvsIpAddress pRemoteAddr,
                                                                  BOOL checkPort, PIceCandidatePair* ppIceCandidatePair)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 addrLen;
    PIceCandidatePair pTargetIceCandidatePair = NULL, pIceCandidatePair = NULL;
    PDoubleListNode pCurNode = NULL;

    CHK(pIceAgent != NULL && ppIceCandidatePair != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    addrLen = IS_IPV4_ADDR(pRemoteAddr) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL && pTargetIceCandidatePair == NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state != ICE_CANDIDATE_PAIR_STATE_FAILED && pIceCandidatePair->local->pSocketConnection == pSocketConnection &&
            pIceCandidatePair->remote->ipAddress.family == pRemoteAddr->family &&
            MEMCMP(pIceCandidatePair->remote->ipAddress.address, pRemoteAddr->address, addrLen) == 0 &&
            (!checkPort || pIceCandidatePair->remote->ipAddress.port == pRemoteAddr->port)) {
            pTargetIceCandidatePair = pIceCandidatePair;
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
    PDoubleListNode pCurNode = NULL, pNextNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;

        if (pIceCandidatePair->state != ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            // backup next node as we will lose that after deleting pCurNode.
            pNextNode = pCurNode->pNext;
            CHK_STATUS(freeIceCandidatePair(&pIceCandidatePair));
            CHK_STATUS(doubleListDeleteNode(pIceAgent->iceCandidatePairs, pCurNode));
            pCurNode = pNextNode;
        } else {
            pCurNode = pCurNode->pNext;
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS iceCandidatePairCheckConnection(PStunPacket pStunBindingRequest, PIceAgent pIceAgent, PIceCandidatePair pIceCandidatePair)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunAttributePriority pStunAttributePriority = NULL;
    UINT32 checkSum = 0;

    CHK(pStunBindingRequest != NULL && pIceAgent != NULL && pIceCandidatePair != NULL, STATUS_NULL_ARG);
    CHK_STATUS(getStunAttribute(pStunBindingRequest, STUN_ATTRIBUTE_TYPE_PRIORITY, (PStunAttributeHeader*) &pStunAttributePriority));

    CHK(pStunAttributePriority != NULL, STATUS_INVALID_ARG);

    // update priority and transaction id
    pStunAttributePriority->priority = pIceCandidatePair->local->priority;
    CHK_STATUS(iceUtilsGenerateTransactionId(pStunBindingRequest->header.transactionId, ARRAY_SIZE(pStunBindingRequest->header.transactionId)));
    CHK(pIceCandidatePair->pTransactionIdStore != NULL, STATUS_INVALID_OPERATION);
    transactionIdStoreInsert(pIceCandidatePair->pTransactionIdStore, pStunBindingRequest->header.transactionId);
    checkSum = COMPUTE_CRC32(pStunBindingRequest->header.transactionId, ARRAY_SIZE(pStunBindingRequest->header.transactionId));
    CHK_STATUS(hashTableUpsert(pIceCandidatePair->requestSentTime, checkSum, GETTIME()));

    if (pIceCandidatePair->local->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        pIceAgent->rtcIceServerDiagnostics[pIceCandidatePair->local->iceServerIndex].totalRequestsSent++;
        CHK_STATUS(hashTableUpsert(pIceAgent->requestTimestampDiagnostics, checkSum, GETTIME()));
    }

    CHK_STATUS(iceAgentSendStunPacket(pStunBindingRequest, (PBYTE) pIceAgent->remotePassword,
                                      (UINT32) STRLEN(pIceAgent->remotePassword) * SIZEOF(CHAR), pIceAgent, pIceCandidatePair->local,
                                      &pIceCandidatePair->remote->ipAddress));

    pIceCandidatePair->rtcIceCandidatePairDiagnostics.lastRequestTimestamp = GETTIME();
    pIceCandidatePair->rtcIceCandidatePairDiagnostics.requestsSent++;
CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS iceAgentSendStunPacket(PStunPacket pStunPacket, PBYTE password, UINT32 passwordLen, PIceAgent pIceAgent, PIceCandidate pLocalCandidate,
                              PKvsIpAddress pDestAddr)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pIceCandidatePair = NULL;

    // Assuming holding pIceAgent->lock

    CHK(pStunPacket != NULL && pIceAgent != NULL && pLocalCandidate != NULL && pDestAddr != NULL, STATUS_NULL_ARG);

    retStatus = iceUtilsSendStunPacket(pStunPacket, password, passwordLen, pDestAddr, pLocalCandidate->pSocketConnection,
                                       pLocalCandidate->pTurnConnection, pLocalCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED);

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceUtilsSendStunPacket failed with 0x%08x", retStatus);

        if (retStatus == STATUS_SOCKET_CONNECTION_CLOSED_ALREADY) {
            pLocalCandidate->state = ICE_CANDIDATE_STATE_INVALID;
            iceAgentInvalidateCandidatePair(pIceAgent);
        }

        retStatus = STATUS_SUCCESS;

        /* Update iceCandidatePair state to failed.
         * pIceCandidatePair could no longer exist. */
        CHK_STATUS(findIceCandidatePairWithLocalSocketConnectionAndRemoteAddr(pIceAgent, pLocalCandidate->pSocketConnection, pDestAddr, TRUE,
                                                                              &pIceCandidatePair));

        if (pIceCandidatePair != NULL) {
            DLOGD("mark candidate pair %s_%s as failed", pIceCandidatePair->local->id, pIceCandidatePair->remote->id);
            pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_FAILED;
        }
    } else {
        CHK_STATUS(findIceCandidatePairWithLocalSocketConnectionAndRemoteAddr(pIceAgent, pLocalCandidate->pSocketConnection, pDestAddr, TRUE,
                                                                              &pIceCandidatePair));
        if (pIceCandidatePair != NULL && pIceCandidatePair == pIceAgent->pDataSendingIceCandidatePair &&
            pIceAgent->pDataSendingIceCandidatePair->firstStunRequest) {
            pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.firstRequestTimestamp = GETTIME();
            pIceAgent->pDataSendingIceCandidatePair->firstStunRequest = FALSE;
        }
    }

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
STATUS iceAgentStateTransitionTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // Do not acquire lock because stepIceAgentStateMachine acquires lock.
    // Drive the state machine
    CHK_STATUS(stepIceAgentStateMachine(pIceAgent));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentSendSrflxCandidateRequest(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pCandidate = NULL;
    PIceServer pIceServer = NULL;
    PStunPacket pBindingRequest = NULL;
    UINT64 checkSum = 0;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // Assume holding pIceAgent->lock

    /* Can't reuse pIceAgent->pBindingRequest because candidate gathering could be running in parallel with
     * connection check. */
    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pBindingRequest));

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pCandidate = (PIceCandidate) data;

        if (pCandidate->state == ICE_CANDIDATE_STATE_NEW) {
            switch (pCandidate->iceCandidateType) {
                case ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
                    pIceServer = &(pIceAgent->iceServers[pCandidate->iceServerIndex]);
                    if (pIceServer->ipAddress.family == pCandidate->ipAddress.family) {
                        transactionIdStoreInsert(pIceAgent->pStunBindingRequestTransactionIdStore, pBindingRequest->header.transactionId);
                        checkSum = COMPUTE_CRC32(pBindingRequest->header.transactionId, ARRAY_SIZE(pBindingRequest->header.transactionId));
                        CHK_STATUS(iceAgentSendStunPacket(pBindingRequest, NULL, 0, pIceAgent, pCandidate, &pIceServer->ipAddress));
                        pIceAgent->rtcIceServerDiagnostics[pCandidate->iceServerIndex].totalRequestsSent++;
                        CHK_STATUS(hashTableUpsert(pIceAgent->requestTimestampDiagnostics, checkSum, GETTIME()));
                    }
                    break;

                default:
                    break;
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pBindingRequest != NULL) {
        freeStunPacket(&pBindingRequest);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentCheckCandidatePairConnection(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL triggeredCheckQueueEmpty;
    UINT64 data;
    PIceCandidatePair pIceCandidatePair = NULL;
    PDoubleListNode pCurNode = NULL;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // Assuming pIceAgent->candidatePairs is sorted by priority
    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(stackQueueIsEmpty(pIceAgent->triggeredCheckQueue, &triggeredCheckQueueEmpty));
    if (!triggeredCheckQueueEmpty) {
        // if triggeredCheckQueue is not empty, check its candidate pair first
        stackQueueDequeue(pIceAgent->triggeredCheckQueue, &data);
        pIceCandidatePair = (PIceCandidatePair) data;

        CHK_STATUS(iceCandidatePairCheckConnection(pIceAgent->pBindingRequest, pIceAgent, pIceCandidatePair));
    } else {
        CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
        while (pCurNode != NULL) {
            pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
            pCurNode = pCurNode->pNext;

            switch (pIceCandidatePair->state) {
                case ICE_CANDIDATE_PAIR_STATE_WAITING:
                    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS;
                    // NOTE: Explicit fall-through
                case ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS:
                    CHK_STATUS(iceCandidatePairCheckConnection(pIceAgent->pBindingRequest, pIceAgent, pIceCandidatePair));
                    break;
                default:
                    break;
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentSendKeepAliveTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    PIceCandidatePair pIceCandidatePair = NULL;
    PDoubleListNode pCurNode = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            pIceCandidatePair->lastDataSentTime = currentTime;
            DLOGV("send keep alive");
            CHK_STATUS(iceAgentSendStunPacket(pIceAgent->pBindingIndication, NULL, 0, pIceAgent, pIceCandidatePair->local,
                                              &pIceCandidatePair->remote->ipAddress));
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentGatherCandidateTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    STATUS retStatus = STATUS_SUCCESS;
    IceCandidate newLocalCandidates[KVS_ICE_MAX_NEW_LOCAL_CANDIDATES_TO_REPORT_AT_ONCE];
    UINT32 newLocalCandidateCount = 0;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE, stopScheduling = FALSE;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pIceCandidate = NULL;
    UINT32 pendingSrflxCandidateCount = 0, pendingCandidateCount = 0, i, totalCandidateCount = 0;
    KvsIpAddress relayAddress;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    MEMSET(newLocalCandidates, 0x00, SIZEOF(newLocalCandidates));
    MEMSET(&relayAddress, 0x00, SIZEOF(KvsIpAddress));

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pIceCandidate = (PIceCandidate) data;

        totalCandidateCount++;
        if (pIceCandidate->state == ICE_CANDIDATE_STATE_NEW) {
            pendingCandidateCount++;
            if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE) {
                pendingSrflxCandidateCount++;
            } else if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED && pIceCandidate->pTurnConnection != NULL &&
                       turnConnectionGetRelayAddress(pIceCandidate->pTurnConnection, &relayAddress)) {
                /* Check if any relay address has been obtained. */
                CHK_STATUS(updateCandidateAddress(pIceCandidate, &relayAddress));
                CHK_STATUS(createIceCandidatePairs(pIceAgent, pIceCandidate, FALSE));
            }
        }
    }

    /* keep sending binding request if there is still pending srflx candidate */
    if (pendingSrflxCandidateCount > 0) {
        CHK_STATUS(iceAgentSendSrflxCandidateRequest(pIceAgent));
    }

    /* stop scheduling if there is no more pending candidate or if timeout is reached. */
    if ((totalCandidateCount > 0 && pendingCandidateCount == 0) || currentTime >= pIceAgent->candidateGatheringEndTime) {
        DLOGD("Candidate gathering completed.");
        stopScheduling = TRUE;
        pIceAgent->iceCandidateGatheringTimerTask = MAX_UINT32;
    }

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL && newLocalCandidateCount < ARRAY_SIZE(newLocalCandidates)) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pIceCandidate = (PIceCandidate) data;

        if (pIceCandidate->state == ICE_CANDIDATE_STATE_VALID && !pIceCandidate->reported) {
            newLocalCandidates[newLocalCandidateCount++] = *pIceCandidate;
            pIceCandidate->reported = TRUE;
        }
    }

    MUTEX_UNLOCK(pIceAgent->lock);
    locked = FALSE;

    /* newLocalCandidateCount is at most ARRAY_SIZE(newLocalCandidates). Candidates not reported in this invocation
     * will be reported in next invocation. */
    for (i = 0; i < newLocalCandidateCount; ++i) {
        CHK_STATUS(iceAgentReportNewLocalCandidate(pIceAgent, &newLocalCandidates[i]));
    }

    if (stopScheduling) {
        ATOMIC_STORE_BOOL(&pIceAgent->candidateGatheringFinished, TRUE);
        /* notify that candidate gathering is finished. */
        if (pIceAgent->iceAgentCallbacks.newLocalCandidateFn != NULL) {
            pIceAgent->iceAgentCallbacks.newLocalCandidateFn(pIceAgent->iceAgentCallbacks.customData, NULL);
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }
    if (stopScheduling) {
        retStatus = STATUS_TIMER_QUEUE_STOP_SCHEDULING;
    }

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    return retStatus;
}

STATUS iceAgentSendCandidateNomination(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // do nothing if not controlling
    CHK(pIceAgent->isControlling, retStatus);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // send packet with USE_CANDIDATE flag if is controlling
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->nominated) {
            CHK_STATUS(iceCandidatePairCheckConnection(pIceAgent->pBindingRequest, pIceAgent, pIceCandidatePair));
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentInitSrflxCandidate(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceServer pIceServer = NULL;
    PIceCandidate pCandidate = NULL, pNewCandidate = NULL;
    UINT32 j;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    /* There should be no other thread mutating localCandidates at this time, so safe to read without lock. */
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pCandidate = (PIceCandidate) data;

        if (pCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_HOST) {
            for (j = 0; j < pIceAgent->iceServersCount; j++) {
                pIceServer = &pIceAgent->iceServers[j];
                if (!pIceServer->isTurn && pIceServer->ipAddress.family == pCandidate->ipAddress.family) {
                    CHK((pNewCandidate = (PIceCandidate) MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);
                    generateJSONSafeString(pNewCandidate->id, ARRAY_SIZE(pNewCandidate->id));
                    pNewCandidate->isRemote = FALSE;

                    // copy over host candidate's address to open up a new socket at that address.
                    pNewCandidate->ipAddress = pCandidate->ipAddress;
                    // open up a new socket at host candidate's ip address for server reflex candidate.
                    // the new port will be stored in pNewCandidate->ipAddress.port. And the Ip address will later be updated
                    // with the correct ip address once the STUN response is received.
                    CHK_STATUS(createSocketConnection(pCandidate->ipAddress.family, KVS_SOCKET_PROTOCOL_UDP, &pNewCandidate->ipAddress, NULL,
                                                      (UINT64) pIceAgent, incomingDataHandler, pIceAgent->kvsRtcConfiguration.sendBufSize,
                                                      &pNewCandidate->pSocketConnection));
                    ATOMIC_STORE_BOOL(&pNewCandidate->pSocketConnection->receiveData, TRUE);
                    // connectionListener will free the pSocketConnection at the end.
                    CHK_STATUS(connectionListenerAddConnection(pIceAgent->pConnectionListener, pNewCandidate->pSocketConnection));
                    pNewCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
                    pNewCandidate->state = ICE_CANDIDATE_STATE_NEW;
                    pNewCandidate->iceServerIndex = j;
                    pNewCandidate->foundation = pIceAgent->foundationCounter++; // we dont generate candidates that have the same foundation.
                    pNewCandidate->priority = computeCandidatePriority(pNewCandidate);

                    /* There could be another thread calling iceAgentAddRemoteCandidate which triggers createIceCandidatePairs.
                     * createIceCandidatePairs will read through localCandidates, since we are mutating localCandidates here,
                     * need to acquire lock. */
                    MUTEX_LOCK(pIceAgent->lock);
                    locked = TRUE;

                    CHK_STATUS(doubleListInsertItemHead(pIceAgent->localCandidates, (UINT64) pNewCandidate));

                    MUTEX_UNLOCK(pIceAgent->lock);
                    locked = FALSE;

                    pNewCandidate = NULL;
                }
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (pNewCandidate != NULL) {
        SAFE_MEMFREE(pNewCandidate);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentInitRelayCandidates(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 j;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    for (j = 0; j < pIceAgent->iceServersCount; j++) {
        if (pIceAgent->iceServers[j].isTurn) {
            if (pIceAgent->iceServers[j].transport == KVS_SOCKET_PROTOCOL_UDP || pIceAgent->iceServers[j].transport == KVS_SOCKET_PROTOCOL_NONE) {
                CHK_STATUS(iceAgentInitRelayCandidate(pIceAgent, j, KVS_SOCKET_PROTOCOL_UDP));
            }

            if (pIceAgent->iceServers[j].transport == KVS_SOCKET_PROTOCOL_TCP || pIceAgent->iceServers[j].transport == KVS_SOCKET_PROTOCOL_NONE) {
                CHK_STATUS(iceAgentInitRelayCandidate(pIceAgent, j, KVS_SOCKET_PROTOCOL_TCP));
            }
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentInitRelayCandidate(PIceAgent pIceAgent, UINT32 iceServerIndex, KVS_SOCKET_PROTOCOL protocol)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PIceCandidate pNewCandidate = NULL, pCandidate = NULL;
    BOOL locked = FALSE;
    PTurnConnection pTurnConnection = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    /* we dont support TURN on DTLS yet. */
    CHK(protocol != KVS_SOCKET_PROTOCOL_UDP || !pIceAgent->iceServers[iceServerIndex].isSecure, retStatus);
    CHK_WARN(pIceAgent->relayCandidateCount < KVS_ICE_MAX_RELAY_CANDIDATE_COUNT, retStatus,
             "Cannot create more relay candidate because max count of %u is reached", KVS_ICE_MAX_RELAY_CANDIDATE_COUNT);

    CHK((pNewCandidate = (PIceCandidate) MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);

    generateJSONSafeString(pNewCandidate->id, ARRAY_SIZE(pNewCandidate->id));
    pNewCandidate->isRemote = FALSE;

    // open up a new socket without binding to any host address. The candidate Ip address will later be updated
    // with the correct relay ip address once the Allocation success response is received. Relay candidate's socket is managed
    // by TurnConnection struct.
    CHK_STATUS(createSocketConnection(KVS_IP_FAMILY_TYPE_IPV4, protocol, NULL, &pIceAgent->iceServers[iceServerIndex].ipAddress,
                                      (UINT64) pNewCandidate, incomingRelayedDataHandler, pIceAgent->kvsRtcConfiguration.sendBufSize,
                                      &pNewCandidate->pSocketConnection));
    // connectionListener will free the pSocketConnection at the end.
    CHK_STATUS(connectionListenerAddConnection(pIceAgent->pConnectionListener, pNewCandidate->pSocketConnection));

    pNewCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_RELAYED;
    pNewCandidate->state = ICE_CANDIDATE_STATE_NEW;
    pNewCandidate->iceServerIndex = iceServerIndex;
    pNewCandidate->foundation = pIceAgent->foundationCounter++; // we dont generate candidates that have the same foundation.
    pNewCandidate->priority = computeCandidatePriority(pNewCandidate);

    CHK_STATUS(createTurnConnection(&pIceAgent->iceServers[iceServerIndex], pIceAgent->timerQueueHandle,
                                    TURN_CONNECTION_DATA_TRANSFER_MODE_SEND_INDIDATION, protocol, NULL, pNewCandidate->pSocketConnection,
                                    pIceAgent->pConnectionListener, &pTurnConnection));
    pNewCandidate->pIceAgent = pIceAgent;
    pNewCandidate->pTurnConnection = pTurnConnection;

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListInsertItemHead(pIceAgent->localCandidates, (UINT64) pNewCandidate));
    pNewCandidate = NULL;

    /* add existing remote candidates to turn. Need to acquire lock because remoteCandidates can be mutated by
     * iceAgentAddRemoteCandidate calls. */
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->remoteCandidates, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        pCurNode = pCurNode->pNext;
        pCandidate = (PIceCandidate) data;

        // TODO: Stop skipping IPv6. Since we're allowing IPv6 remote candidates from iceAgentAddRemoteCandidate for host candidates,
        // it's possible to have a situation where the turn server uses IPv4 and the remote candidate uses IPv6.
        if (IS_IPV4_ADDR(&pCandidate->ipAddress)) {
            CHK_STATUS(turnConnectionAddPeer(pTurnConnection, &pCandidate->ipAddress));
        }
    }

    pIceAgent->relayCandidateCount++;

    MUTEX_UNLOCK(pIceAgent->lock);
    locked = FALSE;

    CHK_STATUS(turnConnectionStart(pTurnConnection));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    SAFE_MEMFREE(pNewCandidate);

    return retStatus;
}

STATUS iceAgentCheckConnectionStateSetup(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 iceCandidatePairCount = 0;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->iceCandidatePairs, &iceCandidatePairCount));

    DLOGD("ice candidate pair count %u", iceCandidatePairCount);

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
    CHK_STATUS(appendStunIceControllAttribute(pIceAgent->pBindingRequest,
                                              pIceAgent->isControlling ? STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING : STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED,
                                              pIceAgent->tieBreaker));

    pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceConnectionCheckTimeout;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS updateCandidateStats(PIceAgent pIceAgent, BOOL isRemote)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pIceAgent != NULL && pIceAgent->pDataSendingIceCandidatePair != NULL, STATUS_NULL_ARG);
    PIceCandidate pIceCandidate = pIceAgent->pDataSendingIceCandidatePair->remote;
    PRtcIceCandidateDiagnostics pRtcIceCandidateDiagnostics = &pIceAgent->rtcSelectedRemoteIceCandidateDiagnostics;
    if (!isRemote) {
        pIceCandidate = pIceAgent->pDataSendingIceCandidatePair->local;
        pRtcIceCandidateDiagnostics = &pIceAgent->rtcSelectedLocalIceCandidateDiagnostics;
        STRNCPY(pRtcIceCandidateDiagnostics->url, STATS_NOT_APPLICABLE_STR, ARRAY_SIZE(pRtcIceCandidateDiagnostics->url));
        // URL and relay protocol are populated only for local candidate by spec.
        // If candidate type is host, there is no URL and is set to N/A
        if (pIceCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_HOST) {
            STRNCPY(pRtcIceCandidateDiagnostics->url, pIceAgent->iceServers[pIceCandidate->iceServerIndex].url,
                    ARRAY_SIZE(pRtcIceCandidateDiagnostics->url));
        }
        // Only if candidate is obtained from TURN server will relay protocol be populated. Else, relay protocol is
        // not applicable.
        STRNCPY(pRtcIceCandidateDiagnostics->relayProtocol, STATS_NOT_APPLICABLE_STR, ARRAY_SIZE(pRtcIceCandidateDiagnostics->relayProtocol));
        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED && pIceCandidate->pTurnConnection != NULL) {
            switch (pIceCandidate->pTurnConnection->protocol) {
                case KVS_SOCKET_PROTOCOL_UDP:
                    STRNCPY(pRtcIceCandidateDiagnostics->relayProtocol, ICE_URL_TRANSPORT_UDP,
                            ARRAY_SIZE(pRtcIceCandidateDiagnostics->relayProtocol));
                    break;
                case KVS_SOCKET_PROTOCOL_TCP:
                    STRNCPY(pRtcIceCandidateDiagnostics->relayProtocol, ICE_URL_TRANSPORT_TCP,
                            ARRAY_SIZE(pIceAgent->rtcSelectedLocalIceCandidateDiagnostics.relayProtocol));
                    break;
                default:
                    MEMSET(pRtcIceCandidateDiagnostics->relayProtocol, 0, SIZEOF(pRtcIceCandidateDiagnostics->relayProtocol));
            }
        }
    }
    getIpAddrStr(&pIceCandidate->ipAddress, pRtcIceCandidateDiagnostics->address, ARRAY_SIZE(pRtcIceCandidateDiagnostics->address));
    pRtcIceCandidateDiagnostics->port = (UINT16) getInt16(pIceCandidate->ipAddress.port);
    pRtcIceCandidateDiagnostics->priority = pIceCandidate->priority;
    STRNCPY(pRtcIceCandidateDiagnostics->candidateType, iceAgentGetCandidateTypeStr(pIceCandidate->iceCandidateType),
            ARRAY_SIZE(pRtcIceCandidateDiagnostics->candidateType));

    STRNCPY(pRtcIceCandidateDiagnostics->protocol, ICE_URL_TRANSPORT_UDP, ARRAY_SIZE(pRtcIceCandidateDiagnostics->protocol));
    if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        STRNCPY(pRtcIceCandidateDiagnostics->protocol, pIceAgent->rtcIceServerDiagnostics[pIceCandidate->iceServerIndex].protocol,
                ARRAY_SIZE(pRtcIceCandidateDiagnostics->protocol));
    }
CleanUp:
    return retStatus;
}
STATUS updateSelectedLocalRemoteCandidateStats(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    // Update local candidate stats
    CHK_STATUS(updateCandidateStats(pIceAgent, FALSE));
    // Update remote candidate stats
    CHK_STATUS(updateCandidateStats(pIceAgent, TRUE));
CleanUp:
    return retStatus;
}

STATUS iceAgentConnectedStateSetup(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL, pLastDataSendingIceCandidatePair = NULL;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);
    if (pIceAgent->pDataSendingIceCandidatePair != NULL) {
        MUTEX_LOCK(pIceAgent->lock);
        locked = TRUE;

        /* at this point ice restart is complete */
        ATOMIC_STORE_BOOL(&pIceAgent->restart, FALSE);
        pLastDataSendingIceCandidatePair = pIceAgent->pDataSendingIceCandidatePair;
        pIceAgent->pDataSendingIceCandidatePair = NULL;

        MUTEX_UNLOCK(pIceAgent->lock);
        locked = FALSE;

        /* If pDataSendingIceCandidatePair is not NULL, then it must be the data sending pair before ice restart.
         * Free its resource here since not there is a new connected pair to replace it. */
        if (IS_CANN_PAIR_SENDING_FROM_RELAYED(pLastDataSendingIceCandidatePair)) {
            CHK_STATUS(turnConnectionShutdown(pLastDataSendingIceCandidatePair->local->pTurnConnection, KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT));
            CHK_STATUS(freeTurnConnection(&pLastDataSendingIceCandidatePair->local->pTurnConnection));

        } else {
            CHK_STATUS(
                connectionListenerRemoveConnection(pIceAgent->pConnectionListener, pLastDataSendingIceCandidatePair->local->pSocketConnection));
            CHK_STATUS(freeSocketConnection(&pLastDataSendingIceCandidatePair->local->pSocketConnection));
        }

        MEMFREE(pLastDataSendingIceCandidatePair->local);
        CHK_STATUS(freeIceCandidatePair(&pLastDataSendingIceCandidatePair));
    }

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // use the first connected pair as the data sending pair
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            pIceAgent->pDataSendingIceCandidatePair = pIceCandidatePair;
            retStatus = updateSelectedLocalRemoteCandidateStats(pIceAgent);
            if (STATUS_FAILED(retStatus)) {
                DLOGW("Failed to update candidate stats with status code 0x%08x", retStatus);
            }
            break;
        }
    }

    // schedule sending keep alive
    CHK_STATUS(timerQueueAddTimer(pIceAgent->timerQueueHandle, KVS_ICE_DEFAULT_TIMER_START_DELAY, KVS_ICE_SEND_KEEP_ALIVE_INTERVAL,
                                  iceAgentSendKeepAliveTimerCallback, (UINT64) pIceAgent, &pIceAgent->keepAliveTimerTask));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentNominatingStateSetup(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    if (pIceAgent->isControlling) {
        CHK_STATUS(iceAgentNominateCandidatePair(pIceAgent));

        if (pIceAgent->pBindingRequest != NULL) {
            CHK_STATUS(freeStunPacket(&pIceAgent->pBindingRequest));
        }
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pIceAgent->pBindingRequest));
        CHK_STATUS(appendStunUsernameAttribute(pIceAgent->pBindingRequest, pIceAgent->combinedUserName));
        CHK_STATUS(appendStunPriorityAttribute(pIceAgent->pBindingRequest, 0));
        CHK_STATUS(appendStunIceControllAttribute(pIceAgent->pBindingRequest, STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING, pIceAgent->tieBreaker));
        CHK_STATUS(appendStunFlagAttribute(pIceAgent->pBindingRequest, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE));
    }

    pIceAgent->stateEndTime = GETTIME() + pIceAgent->kvsRtcConfiguration.iceCandidateNominationTimeout;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentReadyStateSetup(PIceAgent pIceAgent)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pNominatedAndValidCandidatePair = NULL;
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    PDoubleListNode pCurNode = NULL, pNodeToDelete = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;
    BOOL locked = FALSE;
    PIceCandidate pIceCandidate = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    CHK_STATUS(timerQueueUpdateTimerPeriod(pIceAgent->timerQueueHandle, (UINT64) pIceAgent, pIceAgent->iceAgentStateTimerTask,
                                           KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL));

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    // find nominated pair
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
    CHK_STATUS(getIpAddrStr(&pIceAgent->pDataSendingIceCandidatePair->local->ipAddress, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
    DLOGD("Selected pair %s_%s, local candidate type: %s. Round trip time %u ms", pIceAgent->pDataSendingIceCandidatePair->local->id,
          pIceAgent->pDataSendingIceCandidatePair->remote->id,
          iceAgentGetCandidateTypeStr(pIceAgent->pDataSendingIceCandidatePair->local->iceCandidateType),
          pIceAgent->pDataSendingIceCandidatePair->roundTripTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    /* no state timeout for ready state */
    pIceAgent->stateEndTime = INVALID_TIMESTAMP_VALUE;

    /* shutdown turn allocations that are not needed. Invalidate not selected local ice candidates. */
    DLOGD("Freeing Turn allocations that are not selected. Total turn allocation count %u", pIceAgent->relayCandidateCount);
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate != pIceAgent->pDataSendingIceCandidatePair->local) {
            if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
                CHK_STATUS(turnConnectionShutdown(pIceCandidate->pTurnConnection, 0));
            }
            pIceCandidate->state = ICE_CANDIDATE_STATE_INVALID;
        }
    }
    CHK_STATUS(iceAgentInvalidateCandidatePair(pIceAgent));

    /* Free not selected ice candidate pairs */
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pNodeToDelete = pCurNode;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_FAILED) {
            freeIceCandidatePair(&pIceCandidatePair);
            doubleListDeleteNode(pIceAgent->iceCandidatePairs, pNodeToDelete);
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pIceAgent->lock);
    }

    if (STATUS_FAILED(retStatus)) {
        iceAgentFatalError(pIceAgent, retStatus);
    }

    return retStatus;
}

STATUS iceAgentNominateCandidatePair(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pNominatedCandidatePair = NULL, pIceCandidatePair = NULL;
    UINT32 iceCandidatePairsCount = FALSE;
    PDoubleListNode pCurNode = NULL;

    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    // Assume holding pIceAgent->lock
    // do nothing if not controlling
    CHK(pIceAgent->isControlling, retStatus);

    DLOGD("Nominating candidate pair");

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->iceCandidatePairs, &iceCandidatePairsCount));
    CHK(iceCandidatePairsCount > 0, STATUS_ICE_CANDIDATE_PAIR_LIST_EMPTY);

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL && pNominatedCandidatePair == NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        // nominate first connected iceCandidatePair. it should have the highest priority since
        // iceCandidatePairs is already sorted by priority.
        if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
            pNominatedCandidatePair = pIceCandidatePair;
        }
    }

    // should have a nominated pair.
    CHK(pNominatedCandidatePair != NULL, STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR);

    pNominatedCandidatePair->nominated = TRUE;

    // reset transaction id list to ignore future connectivity check response.
    transactionIdStoreClear(pNominatedCandidatePair->pTransactionIdStore);

    // move not nominated candidate pairs to frozen state so the second connectivity check only checks the nominated pair.
    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (!pIceCandidatePair->nominated) {
            pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_FROZEN;
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS iceAgentInvalidateCandidatePair(PIceAgent pIceAgent)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidatePair pIceCandidatePair = NULL;
    PDoubleListNode pCurNode = NULL;

    // Assume holding pIceAgent->lock
    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    CHK_STATUS(doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->local->state != ICE_CANDIDATE_STATE_VALID) {
            pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_FAILED;
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS incomingRelayedDataHandler(UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                                  PKvsIpAddress pDest)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidate pRelayedCandidate = (PIceCandidate) customData;
    // this should be more than enough. Usually the number of channel data in each tcp message is around 4
    TurnChannelData turnChannelData[DEFAULT_TURN_CHANNEL_DATA_BUFFER_SIZE];
    UINT32 turnChannelDataCount = ARRAY_SIZE(turnChannelData), i = 0;

    CHK(pRelayedCandidate != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    CHK_STATUS(turnConnectionIncomingDataHandler(pRelayedCandidate->pTurnConnection, pBuffer, bufferLen, pSrc, pDest, turnChannelData,
                                                 &turnChannelDataCount));
    for (i = 0; i < turnChannelDataCount; ++i) {
        incomingDataHandler((UINT64) pRelayedCandidate->pIceAgent, pSocketConnection, turnChannelData[i].data, turnChannelData[i].size,
                            &turnChannelData[i].senderAddr, NULL);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS incomingDataHandler(UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                           PKvsIpAddress pDest)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceAgent pIceAgent = (PIceAgent) customData;
    BOOL locked = FALSE;
    UINT32 addrLen = 0;
    CHK(pIceAgent != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    locked = TRUE;

    pIceAgent->lastDataReceivedTime = GETTIME();

    // for stun packets, first 8 bytes are 4 byte type and length, then 4 byte magic byte
    if ((bufferLen < 8 || !IS_STUN_PACKET(pBuffer)) && pIceAgent->iceAgentCallbacks.inboundPacketFn != NULL) {
        // release lock early
        MUTEX_UNLOCK(pIceAgent->lock);
        locked = FALSE;
        pIceAgent->iceAgentCallbacks.inboundPacketFn(pIceAgent->iceAgentCallbacks.customData, pBuffer, bufferLen);

        MUTEX_LOCK(pIceAgent->lock);
        locked = TRUE;
        addrLen = IS_IPV4_ADDR(pSrc) ? IPV4_ADDRESS_LENGTH : IPV6_ADDRESS_LENGTH;
        if (pIceAgent->pDataSendingIceCandidatePair != NULL &&
            pIceAgent->pDataSendingIceCandidatePair->local->pSocketConnection == pSocketConnection &&
            pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress.family == pSrc->family &&
            MEMCMP(pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress.address, pSrc->address, addrLen) == 0 &&
            (pIceAgent->pDataSendingIceCandidatePair->remote->ipAddress.port == pSrc->port)) {
            pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.lastPacketReceivedTimestamp = GETTIME();
            pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.bytesReceived += bufferLen;
            pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics
                .packetsReceived++; // Since every byte buffer translates to a single RTP packet
        }
    } else {
        if (ATOMIC_LOAD_BOOL(&pIceAgent->processStun)) {
            CHK_STATUS(handleStunPacket(pIceAgent, pBuffer, bufferLen, pSocketConnection, pSrc, pDest));
        }
    }

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
        amountWritten = SNPRINTF(pOutputData, pOutputData == NULL ? 0 : *pOutputLength,
                                 "%u 1 udp %u %d.%d.%d.%d %d typ %s raddr 0.0.0.0 rport 0 generation 0 network-cost 999", pIceCandidate->foundation,
                                 pIceCandidate->priority, pIceCandidate->ipAddress.address[0], pIceCandidate->ipAddress.address[1],
                                 pIceCandidate->ipAddress.address[2], pIceCandidate->ipAddress.address[3],
                                 (UINT16) getInt16(pIceCandidate->ipAddress.port), iceAgentGetCandidateTypeStr(pIceCandidate->iceCandidateType));
    } else {
        amountWritten = SNPRINTF(pOutputData, pOutputData == NULL ? 0 : *pOutputLength,
                                 "%u 1 udp %u %02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X "
                                 "%d typ %s raddr ::/0 rport 0 generation 0 network-cost 999",
                                 pIceCandidate->foundation, pIceCandidate->priority, pIceCandidate->ipAddress.address[0],
                                 pIceCandidate->ipAddress.address[1], pIceCandidate->ipAddress.address[2], pIceCandidate->ipAddress.address[3],
                                 pIceCandidate->ipAddress.address[4], pIceCandidate->ipAddress.address[5], pIceCandidate->ipAddress.address[6],
                                 pIceCandidate->ipAddress.address[7], pIceCandidate->ipAddress.address[8], pIceCandidate->ipAddress.address[9],
                                 pIceCandidate->ipAddress.address[10], pIceCandidate->ipAddress.address[11], pIceCandidate->ipAddress.address[12],
                                 pIceCandidate->ipAddress.address[13], pIceCandidate->ipAddress.address[14], pIceCandidate->ipAddress.address[15],
                                 (UINT16) getInt16(pIceCandidate->ipAddress.port), iceAgentGetCandidateTypeStr(pIceCandidate->iceCandidateType));
    }

    CHK_WARN(amountWritten > 0, STATUS_INTERNAL_ERROR, "SNPRINTF failed");

    if (pOutputData == NULL) {
        *pOutputLength = ((UINT32) amountWritten) + 1; // +1 for null terminator
    } else {
        // amountWritten doesnt account for null char
        CHK(amountWritten < (INT32) *pOutputLength, STATUS_BUFFER_TOO_SMALL);
    }

CleanUp:

    return retStatus;
}

STATUS handleStunPacket(PIceAgent pIceAgent, PBYTE pBuffer, UINT32 bufferLen, PSocketConnection pSocketConnection, PKvsIpAddress pSrcAddr,
                        PKvsIpAddress pDestAddr)
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
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN], ipAddrStr2[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    PCHAR hexStr = NULL;
    UINT32 hexStrLen = 0, checkSum = 0;
    UINT64 requestSentTime = 0;
    UINT64 connectivityCheckRequestsReceived = 0;
    UINT64 connectivityCheckResponsesSent = 0;
    UINT64 connectivityCheckResponsesReceived = 0;

    // need to determine stunPacketType before deserializing because different password should be used depending on the packet type
    stunPacketType = (UINT16) getInt16(*((PUINT16) pBuffer));

    switch (stunPacketType) {
        case STUN_PACKET_TYPE_BINDING_REQUEST:
            connectivityCheckRequestsReceived++;
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, (PBYTE) pIceAgent->localPassword,
                                             (UINT32) STRLEN(pIceAgent->localPassword) * SIZEOF(CHAR), &pStunPacket));
            CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS, pStunPacket->header.transactionId, &pStunResponse));
            CHK_STATUS(appendStunAddressAttribute(pStunResponse, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, pSrcAddr));
            CHK_STATUS(appendStunIceControllAttribute(
                pStunResponse, pIceAgent->isControlling ? STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING : STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED,
                pIceAgent->tieBreaker));

            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_PRIORITY, (PStunAttributeHeader*) &pStunAttributePriority));
            priority = pStunAttributePriority == NULL ? 0 : pStunAttributePriority->priority;
            CHK_STATUS(iceAgentCheckPeerReflexiveCandidate(pIceAgent, pSrcAddr, priority, TRUE, 0));

            CHK_STATUS(findCandidateWithSocketConnection(pSocketConnection, pIceAgent->localCandidates, &pIceCandidate));
            CHK_WARN(pIceCandidate != NULL, retStatus, "Could not find local candidate to send STUN response");
            CHK_STATUS(iceAgentSendStunPacket(pStunResponse, (PBYTE) pIceAgent->localPassword,
                                              (UINT32) STRLEN(pIceAgent->localPassword) * SIZEOF(CHAR), pIceAgent, pIceCandidate, pSrcAddr));

            connectivityCheckResponsesSent++;
            // return early if there is no candidate pair. This can happen when we get connectivity check from the peer
            // before we receive the answer.
            CHK_STATUS(findIceCandidatePairWithLocalSocketConnectionAndRemoteAddr(pIceAgent, pSocketConnection, pSrcAddr, TRUE, &pIceCandidatePair));
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
            if (pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_FROZEN || pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_WAITING ||
                pIceCandidatePair->state == ICE_CANDIDATE_PAIR_STATE_IN_PROGRESS) {
                CHK_STATUS(stackQueueEnqueue(pIceAgent->triggeredCheckQueue, (UINT64) pIceCandidatePair));
            }

            if (pIceCandidatePair == pIceAgent->pDataSendingIceCandidatePair) {
                pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.requestsReceived += connectivityCheckRequestsReceived;
                pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.responsesSent += connectivityCheckResponsesSent;
                pIceAgent->pDataSendingIceCandidatePair->rtcIceCandidatePairDiagnostics.nominated = pIceCandidatePair->nominated;
            }
            break;

        case STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS:
            connectivityCheckResponsesReceived++;
            checkSum = COMPUTE_CRC32(pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET, STUN_TRANSACTION_ID_LEN);
            // check if Binding Response is for finding srflx candidate
            if (transactionIdStoreHasId(pIceAgent->pStunBindingRequestTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET)) {
                CHK_STATUS(findCandidateWithSocketConnection(pSocketConnection, pIceAgent->localCandidates, &pIceCandidate));
                CHK_WARN(pIceCandidate != NULL, retStatus, "Local candidate with socket %d not found. Dropping STUN binding success response",
                         pSocketConnection->localSocket);

                // Update round trip time for serial reflexive candidate
                pIceAgent->rtcIceServerDiagnostics[pIceCandidate->iceServerIndex].totalResponsesReceived++;
                retStatus = hashTableGet(pIceAgent->requestTimestampDiagnostics, checkSum, &requestSentTime);
                if (retStatus != STATUS_SUCCESS) {
                    DLOGW("Unable to fetch request Timestamp from the hash table. No update to totalRoundTripTime (error code: 0x%08x)", retStatus);
                } else {
                    pIceAgent->rtcIceServerDiagnostics[pIceCandidate->iceServerIndex].totalRoundTripTime += GETTIME() - requestSentTime;
                    CHK_STATUS(hashTableRemove(pIceAgent->requestTimestampDiagnostics, checkSum));
                }

                CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, NULL, 0, &pStunPacket));
                CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &pStunAttr));
                CHK_WARN(pStunAttr != NULL, retStatus, "No mapped address attribute found in STUN binding response. Dropping Packet");

                pStunAttributeAddress = (PStunAttributeAddress) pStunAttr;
                CHK_STATUS(updateCandidateAddress(pIceCandidate, &pStunAttributeAddress->address));
                CHK(FALSE, retStatus);
            }

            CHK_STATUS(findIceCandidatePairWithLocalSocketConnectionAndRemoteAddr(pIceAgent, pSocketConnection, pSrcAddr, TRUE, &pIceCandidatePair));
            if (pIceCandidatePair == NULL) {
                CHK_STATUS(getIpAddrStr(pSrcAddr, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
                CHK_STATUS(getIpAddrStr(&pSocketConnection->hostIpAddr, ipAddrStr2, ARRAY_SIZE(ipAddrStr2)));
                CHK_WARN(FALSE, retStatus,
                         "Cannot find candidate pair with local candidate %s and remote candidate %s. Dropping STUN binding success response",
                         ipAddrStr2, ipAddrStr);
            }

            CHK_WARN(transactionIdStoreHasId(pIceCandidatePair->pTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET), retStatus,
                     "Dropping response packet because transaction id does not match");

            // Update round trip time and responses received only for relay candidates.
            if (pIceCandidatePair->local->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
                pIceAgent->rtcIceServerDiagnostics[pIceCandidatePair->local->iceServerIndex].totalResponsesReceived++;
                retStatus = hashTableGet(pIceAgent->requestTimestampDiagnostics, checkSum, &requestSentTime);
                if (retStatus != STATUS_SUCCESS) {
                    DLOGW("Unable to fetch request Timestamp from the hash table. No update to totalRoundTripTime (error code: 0x%08x)", retStatus);
                } else {
                    pIceAgent->rtcIceServerDiagnostics[pIceCandidatePair->local->iceServerIndex].totalRoundTripTime += GETTIME() - requestSentTime;
                    CHK_STATUS(hashTableRemove(pIceAgent->requestTimestampDiagnostics, checkSum));
                }
            }
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, (PBYTE) pIceAgent->remotePassword,
                                             (UINT32) STRLEN(pIceAgent->remotePassword) * SIZEOF(CHAR), &pStunPacket));
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
                CHK_STATUS(iceAgentCheckPeerReflexiveCandidate(pIceAgent, &pStunAttributeAddress->address, pIceCandidatePair->local->priority, FALSE,
                                                               pSocketConnection));

                CHK(FALSE, retStatus);
            }

            if (pIceCandidatePair->state != ICE_CANDIDATE_PAIR_STATE_SUCCEEDED) {
                pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
                retStatus = hashTableGet(pIceCandidatePair->requestSentTime, checkSum, &requestSentTime);
                if (retStatus != STATUS_SUCCESS) {
                    DLOGW("Unable to fetch request Timestamp from the hash table. No update to totalRoundTripTime (error code: 0x%08x)", retStatus);
                } else {
                    pIceCandidatePair->roundTripTime = GETTIME() - requestSentTime;
                    DLOGD("Ice candidate pair %s_%s is connected. Round trip time: %" PRIu64 "ms", pIceCandidatePair->local->id,
                          pIceCandidatePair->remote->id, pIceCandidatePair->roundTripTime / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                    pIceCandidatePair->rtcIceCandidatePairDiagnostics.currentRoundTripTime =
                        (DOUBLE)(pIceCandidatePair->roundTripTime) / HUNDREDS_OF_NANOS_IN_A_SECOND;
                    pIceCandidatePair->rtcIceCandidatePairDiagnostics.totalRoundTripTime +=
                        (DOUBLE)(pIceCandidatePair->roundTripTime) / HUNDREDS_OF_NANOS_IN_A_SECOND;

                    CHK_STATUS(hashTableRemove(pIceCandidatePair->requestSentTime, checkSum));
                }
            }

            pIceCandidatePair->rtcIceCandidatePairDiagnostics.responsesReceived += connectivityCheckResponsesReceived;
            pIceCandidatePair->rtcIceCandidatePairDiagnostics.lastResponseTimestamp = GETTIME();
            break;

        case STUN_PACKET_TYPE_BINDING_INDICATION:
            DLOGD("Received STUN binding indication");
            break;

        default:
            CHK_STATUS(hexEncode(pBuffer, bufferLen, NULL, &hexStrLen));
            hexStr = MEMCALLOC(1, hexStrLen * SIZEOF(CHAR));
            CHK(hexStr != NULL, STATUS_NOT_ENOUGH_MEMORY);
            CHK_STATUS(hexEncode(pBuffer, bufferLen, hexStr, &hexStrLen));
            DLOGW("Dropping unrecognized STUN packet. Packet type: 0x%02x. Packet content: \n\t%s", stunPacketType, hexStr);
            MEMFREE(hexStr);
            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pStunPacket != NULL) {
        freeStunPacket(&pStunPacket);
    }

    if (pStunResponse != NULL) {
        freeStunPacket(&pStunResponse);
    }

    // TODO send error packet

    return retStatus;
}

STATUS iceAgentCheckPeerReflexiveCandidate(PIceAgent pIceAgent, PKvsIpAddress pIpAddress, UINT32 priority, BOOL isRemote,
                                           PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PIceCandidate pIceCandidate = NULL, pLocalIceCandidate = NULL;
    BOOL freeIceCandidateOnError = TRUE;
    UINT32 candidateCount;

    // remote candidate dont have socketConnection
    CHK(pIceAgent != NULL && pIpAddress != NULL && (isRemote || pSocketConnection != NULL), STATUS_NULL_ARG);

    if (!isRemote) {
        // local peer reflexive candidate replaces existing local candidate because the peer sees different address
        // for this local candidate.
        CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->localCandidates, &pIceCandidate));
        CHK(pIceCandidate == NULL, retStatus); // return early if duplicated

        findCandidateWithSocketConnection(pSocketConnection, pIceAgent->localCandidates, &pLocalIceCandidate);
        pLocalIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
        pLocalIceCandidate->ipAddress = *pIpAddress;
        iceAgentLogNewCandidate(pLocalIceCandidate);
        CHK(FALSE, retStatus);
    }

    CHK_STATUS(doubleListGetNodeCount(pIceAgent->remoteCandidates, &candidateCount));
    CHK_WARN(candidateCount < KVS_ICE_MAX_REMOTE_CANDIDATE_COUNT, retStatus, "max remote candidate count exceeded"); // return early if limit exceeded
    CHK_STATUS(findCandidateWithIp(pIpAddress, pIceAgent->remoteCandidates, &pIceCandidate));
    CHK(pIceCandidate == NULL, retStatus); // return early if duplicated
    DLOGD("New remote peer reflexive candidate found");

    CHK((pIceCandidate = MEMCALLOC(1, SIZEOF(IceCandidate))) != NULL, STATUS_NOT_ENOUGH_MEMORY);
    generateJSONSafeString(pIceCandidate->id, ARRAY_SIZE(pIceCandidate->id));
    pIceCandidate->isRemote = TRUE;
    pIceCandidate->ipAddress = *pIpAddress;
    pIceCandidate->iceCandidateType = ICE_CANDIDATE_TYPE_PEER_REFLEXIVE;
    pIceCandidate->priority = priority;
    pIceCandidate->state = ICE_CANDIDATE_STATE_VALID;
    pIceCandidate->pSocketConnection = NULL; // remote candidate dont have PSocketConnection

    CHK_STATUS(doubleListInsertItemHead(pIceAgent->remoteCandidates, (UINT64) pIceCandidate));
    freeIceCandidateOnError = FALSE;

    CHK_STATUS(createIceCandidatePairs(pIceAgent, pIceCandidate, isRemote));

    iceAgentLogNewCandidate(pIceCandidate);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && freeIceCandidateOnError) {
        MEMFREE(pIceCandidate);
    }

    return retStatus;
}

STATUS iceAgentFatalError(PIceAgent pIceAgent, STATUS errorStatus)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pIceAgent != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pIceAgent->lock);
    pIceAgent->iceAgentStatus = errorStatus;
    MUTEX_UNLOCK(pIceAgent->lock);

CleanUp:

    return retStatus;
}

VOID iceAgentLogNewCandidate(PIceCandidate pIceCandidate)
{
    CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    PCHAR protocol = "UDP";

    if (pIceCandidate != NULL) {
        getIpAddrStr(&pIceCandidate->ipAddress, ipAddr, ARRAY_SIZE(ipAddr));
        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED && pIceCandidate->pTurnConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
            protocol = "TCP";
        }
        DLOGD("New %s ice candidate discovered. Id: %s. Ip: %s:%u. Type: %s. Protocol: %s.", pIceCandidate->isRemote ? "remote" : "local",
              pIceCandidate->id, ipAddr, (UINT16) getInt16(pIceCandidate->ipAddress.port),
              iceAgentGetCandidateTypeStr(pIceCandidate->iceCandidateType), protocol);
    }
}

STATUS updateCandidateAddress(PIceCandidate pIceCandidate, PKvsIpAddress pIpAddr)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pIceCandidate != NULL && pIpAddr != NULL, STATUS_NULL_ARG);
    CHK(pIceCandidate->iceCandidateType != ICE_CANDIDATE_TYPE_HOST, STATUS_INVALID_ARG);
    CHK(pIceCandidate->state == ICE_CANDIDATE_STATE_NEW, retStatus);

    pIceCandidate->ipAddress = *pIpAddr;
    pIceCandidate->state = ICE_CANDIDATE_STATE_VALID;

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

    // Reference: https://tools.ietf.org/html/rfc5245#section-4.1.2.1
    // priority = (2^24)*(type preference) +
    //   (2^8)*(local preference) +
    //   (2^0)*(256 - component ID)
    //
    // Since type preference <= 126 and local preference <= 65535, the maximum possible
    // priority is (2^24) * (126) + (2^8) * (65535) + 255 = 2130706431. So, it's safe
    // to use UINT32 since 2130706431 < 2 ^ 32.
    return (1 << 24) * (typePreference) + (1 << 8) * (localPreference) + 255;
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
        2 * MAX(controlledAgentCandidatePri, controllingAgentCandidatePri) + (controllingAgentCandidatePri > controlledAgentCandidatePri ? 1 : 0);
}

PCHAR iceAgentGetCandidateTypeStr(ICE_CANDIDATE_TYPE candidateType)
{
    switch (candidateType) {
        case ICE_CANDIDATE_TYPE_HOST:
            return SDP_CANDIDATE_TYPE_HOST;
        case ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE:
            return SDP_CANDIDATE_TYPE_SERFLX;
        case ICE_CANDIDATE_TYPE_PEER_REFLEXIVE:
            return SDP_CANDIDATE_TYPE_PRFLX;
        case ICE_CANDIDATE_TYPE_RELAYED:
            return SDP_CANDIDATE_TYPE_RELAY;
    }
    return SDP_CANDIDATE_TYPE_UNKNOWN;
}

UINT64 iceAgentGetCurrentTime(UINT64 customData)
{
    UNUSED_PARAM(customData);
    return GETTIME();
}
