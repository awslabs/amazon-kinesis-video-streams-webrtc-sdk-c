#define LOG_CLASS "Signaling"
#include "../Include_i.h"

extern StateMachineState SIGNALING_STATE_MACHINE_STATES[];
extern UINT32 SIGNALING_STATE_MACHINE_STATE_COUNT;

STATUS createSignalingSync(PSignalingClientInfoInternal pClientInfo, PChannelInfo pChannelInfo, PSignalingClientCallbacks pCallbacks,
                           PAwsCredentialProvider pCredentialProvider, PSignalingClient* ppSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    PCHAR userLogLevelStr = NULL;
    UINT32 userLogLevel;
    struct lws_context_creation_info creationInfo;
    PStateMachineState pStateMachineState;
    BOOL cacheFound = FALSE;
    PSignalingFileCacheEntry pFileCacheEntry = NULL;

    CHK(pClientInfo != NULL && pChannelInfo != NULL && pCallbacks != NULL && pCredentialProvider != NULL && ppSignalingClient != NULL,
        STATUS_NULL_ARG);
    CHK(pChannelInfo->version <= CHANNEL_INFO_CURRENT_VERSION, STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION);
    CHK(NULL != (pFileCacheEntry = (PSignalingFileCacheEntry) MEMALLOC(SIZEOF(SignalingFileCacheEntry))), STATUS_NOT_ENOUGH_MEMORY);

    // Allocate enough storage
    CHK(NULL != (pSignalingClient = (PSignalingClient) MEMCALLOC(1, SIZEOF(SignalingClient))), STATUS_NOT_ENOUGH_MEMORY);

    // Initialize the listener and restarter thread trackers
    CHK_STATUS(initializeThreadTracker(&pSignalingClient->listenerTracker));
    CHK_STATUS(initializeThreadTracker(&pSignalingClient->reconnecterTracker));

    // Validate and store the input
    CHK_STATUS(createValidateChannelInfo(pChannelInfo, &pSignalingClient->pChannelInfo));
    CHK_STATUS(validateSignalingCallbacks(pSignalingClient, pCallbacks));
    CHK_STATUS(validateSignalingClientInfo(pSignalingClient, pClientInfo));

    // Set invalid call times
    pSignalingClient->describeTime = INVALID_TIMESTAMP_VALUE;
    pSignalingClient->createTime = INVALID_TIMESTAMP_VALUE;
    pSignalingClient->getEndpointTime = INVALID_TIMESTAMP_VALUE;
    pSignalingClient->getIceConfigTime = INVALID_TIMESTAMP_VALUE;
    pSignalingClient->deleteTime = INVALID_TIMESTAMP_VALUE;
    pSignalingClient->connectTime = INVALID_TIMESTAMP_VALUE;

    if (pSignalingClient->pChannelInfo->cachingPolicy == SIGNALING_API_CALL_CACHE_TYPE_FILE) {
        // Signaling channel name can be NULL in case of pre-created channels in which case we use ARN as the name
        if (STATUS_FAILED(signalingCacheLoadFromFile(pChannelInfo->pChannelName != NULL ? pChannelInfo->pChannelName : pChannelInfo->pChannelArn,
                                                     pChannelInfo->pRegion, pChannelInfo->channelRoleType, pFileCacheEntry, &cacheFound,
                                                     pSignalingClient->clientInfo.cacheFilePath))) {
            DLOGW("Failed to load signaling cache from file");
        } else if (cacheFound) {
            STRCPY(pSignalingClient->channelDescription.channelArn, pFileCacheEntry->channelArn);
            STRCPY(pSignalingClient->channelEndpointHttps, pFileCacheEntry->httpsEndpoint);
            STRCPY(pSignalingClient->channelEndpointWss, pFileCacheEntry->wssEndpoint);
            pSignalingClient->describeTime = pFileCacheEntry->creationTsEpochSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
            pSignalingClient->getEndpointTime = pFileCacheEntry->creationTsEpochSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
        }
    }

    // Attempting to get the logging level from the env var and if it fails then set it from the client info
    if ((userLogLevelStr = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) != NULL && STATUS_SUCCEEDED(STRTOUI32(userLogLevelStr, NULL, 10, &userLogLevel))) {
        userLogLevel = userLogLevel > LOG_LEVEL_SILENT ? LOG_LEVEL_SILENT : userLogLevel < LOG_LEVEL_VERBOSE ? LOG_LEVEL_VERBOSE : userLogLevel;
    } else {
        userLogLevel = pClientInfo->signalingClientInfo.loggingLevel;
    }

    SET_LOGGER_LOG_LEVEL(userLogLevel);

    // Store the credential provider
    pSignalingClient->pCredentialProvider = pCredentialProvider;

    // Create the state machine
    CHK_STATUS(createStateMachine(SIGNALING_STATE_MACHINE_STATES, SIGNALING_STATE_MACHINE_STATE_COUNT,
                                  CUSTOM_DATA_FROM_SIGNALING_CLIENT(pSignalingClient), signalingGetCurrentTime,
                                  CUSTOM_DATA_FROM_SIGNALING_CLIENT(pSignalingClient), &pSignalingClient->pStateMachine));

    // Prepare the signaling channel protocols array
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_HTTPS].name = HTTPS_SCHEME_NAME;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_HTTPS].callback = lwsHttpCallbackRoutine;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_WSS].name = WSS_SCHEME_NAME;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_WSS].callback = lwsWssCallbackRoutine;

    pSignalingClient->currentWsi[PROTOCOL_INDEX_HTTPS] = NULL;
    pSignalingClient->currentWsi[PROTOCOL_INDEX_WSS] = NULL;

    MEMSET(&creationInfo, 0x00, SIZEOF(struct lws_context_creation_info));
    creationInfo.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    creationInfo.port = CONTEXT_PORT_NO_LISTEN;
    creationInfo.protocols = pSignalingClient->signalingProtocols;
    creationInfo.timeout_secs = SIGNALING_SERVICE_API_CALL_TIMEOUT_IN_SECONDS;
    creationInfo.gid = -1;
    creationInfo.uid = -1;
    creationInfo.client_ssl_ca_filepath = pChannelInfo->pCertPath;
    creationInfo.client_ssl_cipher_list = "HIGH:!PSK:!RSP:!eNULL:!aNULL:!RC4:!MD5:!DES:!3DES:!aDH:!kDH:!DSS";
    creationInfo.ka_time = SIGNALING_SERVICE_TCP_KEEPALIVE_IN_SECONDS;
    creationInfo.ka_probes = SIGNALING_SERVICE_TCP_KEEPALIVE_PROBE_COUNT;
    creationInfo.ka_interval = SIGNALING_SERVICE_TCP_KEEPALIVE_PROBE_INTERVAL_IN_SECONDS;
    creationInfo.ws_ping_pong_interval = SIGNALING_SERVICE_WSS_PING_PONG_INTERVAL_IN_SECONDS;

    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->shutdown, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->deleting, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->deleted, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->serviceLockContention, FALSE);

    // Add to the signal handler
    // signal(SIGINT, lwsSignalHandler);

    // Create the sync primitives
    pSignalingClient->connectedCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pSignalingClient->connectedCvar), STATUS_INVALID_OPERATION);
    pSignalingClient->connectedLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->connectedLock), STATUS_INVALID_OPERATION);
    pSignalingClient->sendCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pSignalingClient->sendCvar), STATUS_INVALID_OPERATION);
    pSignalingClient->sendLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->sendLock), STATUS_INVALID_OPERATION);
    pSignalingClient->receiveCvar = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pSignalingClient->receiveCvar), STATUS_INVALID_OPERATION);
    pSignalingClient->receiveLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->receiveLock), STATUS_INVALID_OPERATION);

    pSignalingClient->stateLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->stateLock), STATUS_INVALID_OPERATION);

    pSignalingClient->messageQueueLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->messageQueueLock), STATUS_INVALID_OPERATION);

    pSignalingClient->lwsServiceLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->lwsServiceLock), STATUS_INVALID_OPERATION);

    pSignalingClient->lwsSerializerLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->lwsSerializerLock), STATUS_INVALID_OPERATION);

    pSignalingClient->diagnosticsLock = MUTEX_CREATE(TRUE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->diagnosticsLock), STATUS_INVALID_OPERATION);

    // Create the ongoing message list
    CHK_STATUS(stackQueueCreate(&pSignalingClient->pMessageQueue));

    pSignalingClient->pLwsContext = lws_create_context(&creationInfo);
    CHK(pSignalingClient->pLwsContext != NULL, STATUS_SIGNALING_LWS_CREATE_CONTEXT_FAILED);

    // Initializing the diagnostics mostly is taken care of by zero-mem in MEMCALLOC
    pSignalingClient->diagnostics.createTime = GETTIME();

    // At this point we have constructed the main object and we can assign to the returned pointer
    *ppSignalingClient = pSignalingClient;

    // Set the time out before execution
    pSignalingClient->stepUntil = pSignalingClient->diagnostics.createTime + SIGNALING_CREATE_TIMEOUT;

    // Notify of the state change initially as the state machinery is already in the NEW state
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            getSignalingStateFromStateMachineState(pStateMachineState->state)));
    }

    // Do not force ice config state
    ATOMIC_STORE_BOOL(&pSignalingClient->refreshIceConfig, FALSE);

    // Prime the state machine
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, STATUS_SUCCESS));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        freeSignaling(&pSignalingClient);
    }

    if (ppSignalingClient != NULL) {
        *ppSignalingClient = pSignalingClient;
    }
    SAFE_MEMFREE(pFileCacheEntry);
    LEAVES();
    return retStatus;
}

STATUS freeSignaling(PSignalingClient* ppSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient;

    CHK(ppSignalingClient != NULL, STATUS_NULL_ARG);

    pSignalingClient = *ppSignalingClient;
    CHK(pSignalingClient != NULL, retStatus);

    ATOMIC_STORE_BOOL(&pSignalingClient->shutdown, TRUE);

    terminateOngoingOperations(pSignalingClient);

    if (pSignalingClient->pLwsContext != NULL) {
        MUTEX_LOCK(pSignalingClient->lwsServiceLock);
        lws_context_destroy(pSignalingClient->pLwsContext);
        pSignalingClient->pLwsContext = NULL;
        MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);
    }

    freeStateMachine(pSignalingClient->pStateMachine);

    freeChannelInfo(&pSignalingClient->pChannelInfo);

    stackQueueFree(pSignalingClient->pMessageQueue);

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->connectedLock)) {
        MUTEX_FREE(pSignalingClient->connectedLock);
    }

    if (IS_VALID_CVAR_VALUE(pSignalingClient->connectedCvar)) {
        CVAR_FREE(pSignalingClient->connectedCvar);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->sendLock)) {
        MUTEX_FREE(pSignalingClient->sendLock);
    }

    if (IS_VALID_CVAR_VALUE(pSignalingClient->sendCvar)) {
        CVAR_FREE(pSignalingClient->sendCvar);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->receiveLock)) {
        MUTEX_FREE(pSignalingClient->receiveLock);
    }

    if (IS_VALID_CVAR_VALUE(pSignalingClient->receiveCvar)) {
        CVAR_FREE(pSignalingClient->receiveCvar);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->stateLock)) {
        MUTEX_FREE(pSignalingClient->stateLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->messageQueueLock)) {
        MUTEX_FREE(pSignalingClient->messageQueueLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->lwsServiceLock)) {
        MUTEX_FREE(pSignalingClient->lwsServiceLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->lwsSerializerLock)) {
        MUTEX_FREE(pSignalingClient->lwsSerializerLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSignalingClient->diagnosticsLock)) {
        MUTEX_FREE(pSignalingClient->diagnosticsLock);
    }

    uninitializeThreadTracker(&pSignalingClient->reconnecterTracker);
    uninitializeThreadTracker(&pSignalingClient->listenerTracker);

    MEMFREE(pSignalingClient);

    *ppSignalingClient = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS terminateOngoingOperations(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Terminate the listener thread if alive
    terminateLwsListenerLoop(pSignalingClient);

    // Await for the reconnect thread to exit
    awaitForThreadTermination(&pSignalingClient->reconnecterTracker, SIGNALING_CLIENT_SHUTDOWN_TIMEOUT);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingSendMessageSync(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL removeFromList = FALSE;

    CHK(pSignalingClient != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK(pSignalingMessage->peerClientId != NULL && pSignalingMessage->payload != NULL, STATUS_INVALID_ARG);
    CHK(pSignalingMessage->version <= SIGNALING_MESSAGE_CURRENT_VERSION, STATUS_SIGNALING_INVALID_SIGNALING_MESSAGE_VERSION);

    // Store the signaling message
    CHK_STATUS(signalingStoreOngoingMessage(pSignalingClient, pSignalingMessage));
    removeFromList = TRUE;

    // Perform the call
    CHK_STATUS(sendLwsMessage(pSignalingClient, pSignalingMessage->messageType, pSignalingMessage->peerClientId, pSignalingMessage->payload,
                              pSignalingMessage->payloadLen, pSignalingMessage->correlationId, 0));

    // Update the internal diagnostics only after successfully sending
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfMessagesSent);

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Remove from the list if previously added
    if (removeFromList) {
        signalingRemoveOngoingMessage(pSignalingClient, pSignalingMessage->correlationId);
    }

    LEAVES();
    return retStatus;
}

STATUS signalingGetIceConfigInfoCout(PSignalingClient pSignalingClient, PUINT32 pIceConfigCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && pIceConfigCount != NULL, STATUS_NULL_ARG);

    CHK_STATUS(refreshIceConfiguration(pSignalingClient));

    *pIceConfigCount = pSignalingClient->iceConfigCount;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingGetIceConfigInfo(PSignalingClient pSignalingClient, UINT32 index, PIceConfigInfo* ppIceConfigInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && ppIceConfigInfo != NULL, STATUS_NULL_ARG);

    // Refresh the ICE configuration first
    CHK_STATUS(refreshIceConfiguration(pSignalingClient));

    CHK(index < pSignalingClient->iceConfigCount, STATUS_INVALID_ARG);

    *ppIceConfigInfo = &pSignalingClient->iceConfigs[index];

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingConnectSync(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pState = NULL;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Validate the state
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine,
                                       SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_CONNECTED));

    // Check if we are already connected
    CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->connected), retStatus);

    // Self-prime through the ready state
    pSignalingClient->continueOnReady = TRUE;

    // Store the signaling state in case we error/timeout so we can re-set it on exit
    CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pState));

    // Set the time out before execution
    pSignalingClient->stepUntil = GETTIME() + SIGNALING_CONNECT_STATE_TIMEOUT;

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Re-set the state if we failed
    if (STATUS_FAILED(retStatus) && (pState != NULL)) {
        resetStateMachineRetryCount(pSignalingClient->pStateMachine);
        setStateMachineCurrentState(pSignalingClient->pStateMachine, pState->state);
    }

    LEAVES();
    return retStatus;
}

STATUS signalingDisconnectSync(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Do not self-prime through the ready state
    pSignalingClient->continueOnReady = FALSE;

    // Check if we are already not connected
    CHK(ATOMIC_LOAD_BOOL(&pSignalingClient->connected), retStatus);

    CHK_STATUS(terminateOngoingOperations(pSignalingClient));

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingDeleteSync(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Check if we are already deleting
    CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->deleted), retStatus);

    // Mark as being deleted
    ATOMIC_STORE_BOOL(&pSignalingClient->deleting, TRUE);

    CHK_STATUS(terminateOngoingOperations(pSignalingClient));

    // Set the state directly
    setStateMachineCurrentState(pSignalingClient->pStateMachine, SIGNALING_STATE_DELETE);

    // Set the time out before execution
    pSignalingClient->stepUntil = GETTIME() + SIGNALING_DELETE_TIMEOUT;

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS validateSignalingCallbacks(PSignalingClient pSignalingClient, PSignalingClientCallbacks pCallbacks)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && pCallbacks != NULL, STATUS_NULL_ARG);
    CHK(pCallbacks->version <= SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION, STATUS_SIGNALING_INVALID_SIGNALING_CALLBACKS_VERSION);

    // Store and validate
    pSignalingClient->signalingClientCallbacks = *pCallbacks;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS validateSignalingClientInfo(PSignalingClient pSignalingClient, PSignalingClientInfoInternal pClientInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && pClientInfo != NULL, STATUS_NULL_ARG);
    CHK(pClientInfo->signalingClientInfo.version <= SIGNALING_CLIENT_INFO_CURRENT_VERSION, STATUS_SIGNALING_INVALID_CLIENT_INFO_VERSION);
    CHK(STRNLEN(pClientInfo->signalingClientInfo.clientId, MAX_SIGNALING_CLIENT_ID_LEN + 1) <= MAX_SIGNALING_CLIENT_ID_LEN,
        STATUS_SIGNALING_INVALID_CLIENT_INFO_CLIENT_LENGTH);

    // Copy and store internally
    pSignalingClient->clientInfo = *pClientInfo;

    // V1 features
    switch (pSignalingClient->clientInfo.signalingClientInfo.version) {
        case 0:
            // Set the default path
            STRCPY(pSignalingClient->clientInfo.cacheFilePath, DEFAULT_CACHE_FILE_PATH);

            break;

        case 1:
            // If the path is specified and not empty then we validate and copy/store
            if (pSignalingClient->clientInfo.signalingClientInfo.cacheFilePath != NULL &&
                pSignalingClient->clientInfo.signalingClientInfo.cacheFilePath[0] != '\0') {
                CHK(STRNLEN(pSignalingClient->clientInfo.signalingClientInfo.cacheFilePath, MAX_PATH_LEN + 1) <= MAX_PATH_LEN,
                    STATUS_SIGNALING_INVALID_CLIENT_INFO_CACHE_FILE_PATH_LEN);
                STRCPY(pSignalingClient->clientInfo.cacheFilePath, pSignalingClient->clientInfo.signalingClientInfo.cacheFilePath);
            } else {
                // Set the default path
                STRCPY(pSignalingClient->clientInfo.cacheFilePath, DEFAULT_CACHE_FILE_PATH);
            }

            break;

        default:
            CHK_ERR(FALSE, STATUS_INTERNAL_ERROR, "Internal error checking and validating the ClientInfo version");
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    LEAVES();
    return retStatus;
}

STATUS validateIceConfiguration(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;
    UINT64 minTtl = MAX_UINT64;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->iceConfigCount <= MAX_ICE_CONFIG_COUNT, STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT);
    CHK(pSignalingClient->iceConfigCount > 0, STATUS_SIGNALING_NO_CONFIG_SPECIFIED);

    for (i = 0; i < pSignalingClient->iceConfigCount; i++) {
        CHK(pSignalingClient->iceConfigs[i].version <= SIGNALING_ICE_CONFIG_INFO_CURRENT_VERSION, STATUS_SIGNALING_INVALID_ICE_CONFIG_INFO_VERSION);
        CHK(pSignalingClient->iceConfigs[i].uriCount > 0, STATUS_SIGNALING_NO_CONFIG_URI_SPECIFIED);
        CHK(pSignalingClient->iceConfigs[i].uriCount <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);

        minTtl = MIN(minTtl, pSignalingClient->iceConfigs[i].ttl);
    }

    CHK(minTtl > ICE_CONFIGURATION_REFRESH_GRACE_PERIOD, STATUS_SIGNALING_ICE_TTL_LESS_THAN_GRACE_PERIOD);

    pSignalingClient->iceConfigTime = GETTIME();
    pSignalingClient->iceConfigExpiration = pSignalingClient->iceConfigTime + (minTtl - ICE_CONFIGURATION_REFRESH_GRACE_PERIOD);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS refreshIceConfiguration(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pStateMachineState = NULL;
    CHAR iceRefreshErrMsg[SIGNALING_MAX_ERROR_MESSAGE_LEN + 1];
    UINT32 iceRefreshErrLen;
    UINT64 curTime;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    DLOGD("Refreshing the ICE Server Configuration");

    // Check whether we have a valid not-yet-expired ICE configuration and if so early exit
    curTime = GETTIME();
    CHK(pSignalingClient->iceConfigCount == 0 || curTime > pSignalingClient->iceConfigExpiration, retStatus);

    // ICE config can be retrieved in specific states only
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine,
                                       SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_DISCONNECTED));

    // Get and store the current state to re-set to if we fail
    CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));

    // Force the state machine to revert back to get ICE configuration without re-connection
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE);
    ATOMIC_STORE(&pSignalingClient->refreshIceConfig, TRUE);

    // Iterate the state machinery in steady states only - ready or connected
    if (pStateMachineState->state == SIGNALING_STATE_READY || pStateMachineState->state == SIGNALING_STATE_CONNECTED) {
        // Set the time out before execution
        pSignalingClient->stepUntil = curTime + SIGNALING_REFRESH_ICE_CONFIG_STATE_TIMEOUT;

        CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    // Notify the client in case of an error
    if (pSignalingClient != NULL && STATUS_FAILED(retStatus)) {
        // Update the diagnostics info prior calling the error callback
        ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfRuntimeErrors);

        // Reset the stored state as we could have been connected prior to the ICE refresh and we still need to be connected
        if (pStateMachineState != NULL) {
            setStateMachineCurrentState(pSignalingClient->pStateMachine, pStateMachineState->state);
        }

        // Need to invoke the error handler callback
        if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
            iceRefreshErrLen = SNPRINTF(iceRefreshErrMsg, SIGNALING_MAX_ERROR_MESSAGE_LEN, SIGNALING_ICE_CONFIG_REFRESH_ERROR_MSG, retStatus);
            iceRefreshErrMsg[SIGNALING_MAX_ERROR_MESSAGE_LEN] = '\0';
            pSignalingClient->signalingClientCallbacks.errorReportFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                     STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED, iceRefreshErrMsg, iceRefreshErrLen);
        }
    }

    LEAVES();
    return retStatus;
}

STATUS signalingStoreOngoingMessage(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PSignalingMessage pExistingMessage = NULL;

    CHK(pSignalingClient != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pSignalingClient->messageQueueLock);
    locked = TRUE;
    CHK_STATUS(signalingGetOngoingMessage(pSignalingClient, pSignalingMessage->correlationId, pSignalingMessage->peerClientId, &pExistingMessage));
    CHK(pExistingMessage == NULL, STATUS_SIGNALING_DUPLICATE_MESSAGE_BEING_SENT);
    CHK_STATUS(stackQueueEnqueue(pSignalingClient->pMessageQueue, (UINT64) pSignalingMessage));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->messageQueueLock);
    }

    LEAVES();
    return retStatus;
}

STATUS signalingRemoveOngoingMessage(PSignalingClient pSignalingClient, PCHAR correlationId)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PSignalingMessage pExistingMessage;
    StackQueueIterator iterator;
    UINT64 data;

    CHK(pSignalingClient != NULL && correlationId != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pSignalingClient->messageQueueLock);
    locked = TRUE;

    CHK_STATUS(stackQueueGetIterator(pSignalingClient->pMessageQueue, &iterator));
    while (IS_VALID_ITERATOR(iterator)) {
        CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));

        pExistingMessage = (PSignalingMessage) data;
        CHK(pExistingMessage != NULL, STATUS_INTERNAL_ERROR);

        if ((correlationId[0] == '\0' && pExistingMessage->correlationId[0] == '\0') || 0 == STRCMP(pExistingMessage->correlationId, correlationId)) {
            // Remove the match
            CHK_STATUS(stackQueueRemoveItem(pSignalingClient->pMessageQueue, data));

            // Early return
            CHK(FALSE, retStatus);
        }

        CHK_STATUS(stackQueueIteratorNext(&iterator));
    }

    // Didn't find a match
    CHK(FALSE, STATUS_NOT_FOUND);

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->messageQueueLock);
    }

    LEAVES();
    return retStatus;
}

STATUS signalingGetOngoingMessage(PSignalingClient pSignalingClient, PCHAR correlationId, PCHAR peerClientId, PSignalingMessage* ppSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, checkPeerClientId = TRUE;
    PSignalingMessage pExistingMessage = NULL;
    StackQueueIterator iterator;
    UINT64 data;

    CHK(pSignalingClient != NULL && correlationId != NULL && ppSignalingMessage != NULL, STATUS_NULL_ARG);
    if (peerClientId == NULL || IS_EMPTY_STRING(peerClientId)) {
        checkPeerClientId = FALSE;
    }

    MUTEX_LOCK(pSignalingClient->messageQueueLock);
    locked = TRUE;

    CHK_STATUS(stackQueueGetIterator(pSignalingClient->pMessageQueue, &iterator));
    while (IS_VALID_ITERATOR(iterator)) {
        CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));

        pExistingMessage = (PSignalingMessage) data;
        CHK(pExistingMessage != NULL, STATUS_INTERNAL_ERROR);

        if (((correlationId[0] == '\0' && pExistingMessage->correlationId[0] == '\0') ||
             0 == STRCMP(pExistingMessage->correlationId, correlationId)) &&
            (!checkPeerClientId || 0 == STRCMP(pExistingMessage->peerClientId, peerClientId))) {
            *ppSignalingMessage = pExistingMessage;

            // Early return
            CHK(FALSE, retStatus);
        }

        CHK_STATUS(stackQueueIteratorNext(&iterator));
    }

CleanUp:

    if (ppSignalingMessage != NULL) {
        *ppSignalingMessage = pExistingMessage;
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->messageQueueLock);
    }

    LEAVES();
    return retStatus;
}

STATUS initializeThreadTracker(PThreadTracker pThreadTracker)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pThreadTracker != NULL, STATUS_NULL_ARG);

    pThreadTracker->threadId = INVALID_TID_VALUE;

    pThreadTracker->lock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pThreadTracker->lock), STATUS_INVALID_OPERATION);

    pThreadTracker->await = CVAR_CREATE();
    CHK(IS_VALID_CVAR_VALUE(pThreadTracker->await), STATUS_INVALID_OPERATION);

    ATOMIC_STORE_BOOL(&pThreadTracker->terminated, TRUE);

CleanUp:
    return retStatus;
}

STATUS uninitializeThreadTracker(PThreadTracker pThreadTracker)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pThreadTracker != NULL, STATUS_NULL_ARG);

    if (IS_VALID_MUTEX_VALUE(pThreadTracker->lock)) {
        MUTEX_FREE(pThreadTracker->lock);
    }

    if (IS_VALID_CVAR_VALUE(pThreadTracker->await)) {
        CVAR_FREE(pThreadTracker->await);
    }

CleanUp:
    return retStatus;
}

STATUS awaitForThreadTermination(PThreadTracker pThreadTracker, UINT64 timeout)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pThreadTracker != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pThreadTracker->lock);
    locked = TRUE;
    // Await for the termination
    while (!ATOMIC_LOAD_BOOL(&pThreadTracker->terminated)) {
        CHK_STATUS(CVAR_WAIT(pThreadTracker->await, pThreadTracker->lock, timeout));
    }

    MUTEX_UNLOCK(pThreadTracker->lock);
    locked = FALSE;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pThreadTracker->lock);
    }

    return retStatus;
}

STATUS describeChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL apiCall = TRUE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    switch (pSignalingClient->pChannelInfo->cachingPolicy) {
        case SIGNALING_API_CALL_CACHE_TYPE_NONE:
            break;

        case SIGNALING_API_CALL_CACHE_TYPE_DESCRIBE_GETENDPOINT:
            /* explicit fall-through */
        case SIGNALING_API_CALL_CACHE_TYPE_FILE:
            if (IS_VALID_TIMESTAMP(pSignalingClient->describeTime) &&
                time <= pSignalingClient->describeTime + pSignalingClient->pChannelInfo->cachingPeriod) {
                apiCall = FALSE;
            }

            break;
    }

    // Call DescribeChannel API
    if (STATUS_SUCCEEDED(retStatus)) {
        if (apiCall) {
            // Call pre hook func
            if (pSignalingClient->clientInfo.describePreHookFn != NULL) {
                retStatus = pSignalingClient->clientInfo.describePreHookFn(pSignalingClient->clientInfo.hookCustomData);
            }

            if (STATUS_SUCCEEDED(retStatus)) {
                retStatus = describeChannelLws(pSignalingClient, time);

                // Store the last call time on success
                if (STATUS_SUCCEEDED(retStatus)) {
                    pSignalingClient->describeTime = time;
                }

                // Calculate the latency whether the call succeeded or not
                SIGNALING_API_LATENCY_CALCULATION(pSignalingClient, time, TRUE);
            }

            // Call post hook func
            if (pSignalingClient->clientInfo.describePostHookFn != NULL) {
                retStatus = pSignalingClient->clientInfo.describePostHookFn(pSignalingClient->clientInfo.hookCustomData);
            }
        } else {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        }
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LEAVES();
    return retStatus;
}

STATUS createChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // We are not caching create calls

    if (pSignalingClient->clientInfo.createPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.createPreHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = createChannelLws(pSignalingClient, time);

        // Store the time of the call on success
        if (STATUS_SUCCEEDED(retStatus)) {
            pSignalingClient->createTime = time;
        }

        // Calculate the latency whether the call succeeded or not
        SIGNALING_API_LATENCY_CALCULATION(pSignalingClient, time, TRUE);
    }

    if (pSignalingClient->clientInfo.createPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.createPostHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LEAVES();
    return retStatus;
}

STATUS getChannelEndpoint(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL apiCall = TRUE;
    SignalingFileCacheEntry signalingFileCacheEntry;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    switch (pSignalingClient->pChannelInfo->cachingPolicy) {
        case SIGNALING_API_CALL_CACHE_TYPE_NONE:
            break;

        case SIGNALING_API_CALL_CACHE_TYPE_DESCRIBE_GETENDPOINT:
            /* explicit fall-through */
        case SIGNALING_API_CALL_CACHE_TYPE_FILE:
            if (IS_VALID_TIMESTAMP(pSignalingClient->getEndpointTime) &&
                time <= pSignalingClient->getEndpointTime + pSignalingClient->pChannelInfo->cachingPeriod) {
                apiCall = FALSE;
            }

            break;
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        if (apiCall) {
            if (pSignalingClient->clientInfo.getEndpointPreHookFn != NULL) {
                retStatus = pSignalingClient->clientInfo.getEndpointPreHookFn(pSignalingClient->clientInfo.hookCustomData);
            }

            if (STATUS_SUCCEEDED(retStatus)) {
                retStatus = getChannelEndpointLws(pSignalingClient, time);

                if (STATUS_SUCCEEDED(retStatus)) {
                    pSignalingClient->getEndpointTime = time;

                    if (pSignalingClient->pChannelInfo->cachingPolicy == SIGNALING_API_CALL_CACHE_TYPE_FILE) {
                        signalingFileCacheEntry.creationTsEpochSeconds = time / HUNDREDS_OF_NANOS_IN_A_SECOND;
                        signalingFileCacheEntry.role = pSignalingClient->pChannelInfo->channelRoleType;
                        // In case of pre-created channels, the channel name can be NULL in which case we will use ARN.
                        // The validation logic in the channel info validates that both can't be NULL at the same time.
                        STRCPY(signalingFileCacheEntry.channelName,
                               pSignalingClient->pChannelInfo->pChannelName != NULL ? pSignalingClient->pChannelInfo->pChannelName
                                                                                    : pSignalingClient->pChannelInfo->pChannelArn);
                        STRCPY(signalingFileCacheEntry.region, pSignalingClient->pChannelInfo->pRegion);
                        STRCPY(signalingFileCacheEntry.channelArn, pSignalingClient->channelDescription.channelArn);
                        STRCPY(signalingFileCacheEntry.httpsEndpoint, pSignalingClient->channelEndpointHttps);
                        STRCPY(signalingFileCacheEntry.wssEndpoint, pSignalingClient->channelEndpointWss);
                        if (STATUS_FAILED(signalingCacheSaveToFile(&signalingFileCacheEntry, pSignalingClient->clientInfo.cacheFilePath))) {
                            DLOGW("Failed to save signaling cache to file");
                        }
                    }
                }

                // Calculate the latency whether the call succeeded or not
                SIGNALING_API_LATENCY_CALCULATION(pSignalingClient, time, TRUE);
            }

            if (pSignalingClient->clientInfo.getEndpointPostHookFn != NULL) {
                retStatus = pSignalingClient->clientInfo.getEndpointPostHookFn(pSignalingClient->clientInfo.hookCustomData);
            }
        } else {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        }
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LEAVES();
    return retStatus;
}

STATUS getIceConfig(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // We are not caching ICE server config calls

    if (pSignalingClient->clientInfo.getIceConfigPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getIceConfigPreHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = getIceConfigLws(pSignalingClient, time);

        if (STATUS_SUCCEEDED(retStatus)) {
            pSignalingClient->getIceConfigTime = time;
        }

        // Calculate the latency whether the call succeeded or not
        SIGNALING_API_LATENCY_CALCULATION(pSignalingClient, time, FALSE);
    }

    if (pSignalingClient->clientInfo.getIceConfigPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getIceConfigPostHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LEAVES();
    return retStatus;
}

STATUS deleteChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // We are not caching delete calls

    if (pSignalingClient->clientInfo.deletePreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.deletePreHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = deleteChannelLws(pSignalingClient, time);

        // Store the time of the call on success
        if (STATUS_SUCCEEDED(retStatus)) {
            pSignalingClient->deleteTime = time;
        }

        // Calculate the latency whether the call succeeded or not
        SIGNALING_API_LATENCY_CALCULATION(pSignalingClient, time, TRUE);
    }

    if (pSignalingClient->clientInfo.deletePostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.deletePostHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    LEAVES();
    return retStatus;
}

STATUS connectSignalingChannel(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    THREAD_SLEEP_UNTIL(time);

    // Check for the stale credentials
    CHECK_SIGNALING_CREDENTIALS_EXPIRATION(pSignalingClient);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // We are not caching connect calls

    if (pSignalingClient->clientInfo.connectPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.connectPreHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        // No need to reconnect again if already connected. This can happen if we get to this state after ice refresh
        if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
            retStatus = connectSignalingChannelLws(pSignalingClient, time);

            // Store the time of the call on success
            if (STATUS_SUCCEEDED(retStatus)) {
                pSignalingClient->connectTime = time;
            }
        } else {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        }
    }

    if (pSignalingClient->clientInfo.connectPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.connectPostHookFn(pSignalingClient->clientInfo.hookCustomData);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

UINT64 signalingGetCurrentTime(UINT64 customData)
{
    UNUSED_PARAM(customData);
    return GETTIME();
}

STATUS signalingGetMetrics(PSignalingClient pSignalingClient, PSignalingClientMetrics pSignalingClientMetrics)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 curTime = GETTIME();

    CHK(pSignalingClient != NULL && pSignalingClientMetrics != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClientMetrics->version <= SIGNALING_CLIENT_METRICS_CURRENT_VERSION, STATUS_SIGNALING_INVALID_METRICS_VERSION);

    // Interlock the threading due to data race possibility
    MUTEX_LOCK(pSignalingClient->diagnosticsLock);

    // Fill in the data structures according to the version of the requested structure - currently only v0
    pSignalingClientMetrics->signalingClientStats.signalingClientUptime = curTime - pSignalingClient->diagnostics.createTime;
    pSignalingClientMetrics->signalingClientStats.numberOfMessagesSent = (UINT32) pSignalingClient->diagnostics.numberOfMessagesSent;
    pSignalingClientMetrics->signalingClientStats.numberOfMessagesReceived = (UINT32) pSignalingClient->diagnostics.numberOfMessagesReceived;
    pSignalingClientMetrics->signalingClientStats.iceRefreshCount = (UINT32) pSignalingClient->diagnostics.iceRefreshCount;
    pSignalingClientMetrics->signalingClientStats.numberOfErrors = (UINT32) pSignalingClient->diagnostics.numberOfErrors;
    pSignalingClientMetrics->signalingClientStats.numberOfRuntimeErrors = (UINT32) pSignalingClient->diagnostics.numberOfRuntimeErrors;
    pSignalingClientMetrics->signalingClientStats.numberOfReconnects = (UINT32) pSignalingClient->diagnostics.numberOfReconnects;
    pSignalingClientMetrics->signalingClientStats.cpApiCallLatency = pSignalingClient->diagnostics.cpApiLatency;
    pSignalingClientMetrics->signalingClientStats.dpApiCallLatency = pSignalingClient->diagnostics.dpApiLatency;

    pSignalingClientMetrics->signalingClientStats.connectionDuration =
        ATOMIC_LOAD_BOOL(&pSignalingClient->connected) ? curTime - pSignalingClient->diagnostics.connectTime : 0;

    MUTEX_UNLOCK(pSignalingClient->diagnosticsLock);

CleanUp:

    LEAVES();
    return retStatus;
}
