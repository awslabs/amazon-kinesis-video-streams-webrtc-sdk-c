#define LOG_CLASS "Signaling"
#include "../Include_i.h"

extern StateMachineState SIGNALING_STATE_MACHINE_STATES[];
extern UINT32 SIGNALING_STATE_MACHINE_STATE_COUNT;

STATUS createSignalingSync(PSignalingClientInfo pClientInfo, PChannelInfo pChannelInfo,
        PSignalingClientCallbacks pCallbacks, PAwsCredentialProvider pCredentialProvider,
        PSignalingClient *ppSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    PCHAR userLogLevelStr = NULL;
    UINT32 userLogLevel;
    struct lws_context_creation_info creationInfo;

    CHK(pClientInfo != NULL &&
         pChannelInfo != NULL &&
         pCallbacks != NULL &&
         pCredentialProvider != NULL &&
         ppSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pChannelInfo->version <= CHANNEL_INFO_CURRENT_VERSION, STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION);

    // Allocate enough storage
    CHK(NULL != (pSignalingClient = (PSignalingClient) MEMCALLOC(1, SIZEOF(SignalingClient))), STATUS_NOT_ENOUGH_MEMORY);

    // Initialize the listener and restarter thread trackers
    CHK_STATUS(initializeThreadTracker(&pSignalingClient->listenerTracker));
    CHK_STATUS(initializeThreadTracker(&pSignalingClient->reconnecterTracker));

    // Validate and store the input
    CHK_STATUS(createChannelInfo(pChannelInfo->pChannelName,
                                 pChannelInfo->pChannelArn,
                                 pChannelInfo->pRegion,
                                 pChannelInfo->pControlPlaneUrl,
                                 pChannelInfo->pCertPath,
                                 pChannelInfo->pUserAgentPostfix,
                                 pChannelInfo->pCustomUserAgent,
                                 pChannelInfo->pKmsKeyId,
                                 pChannelInfo->channelType,
                                 pChannelInfo->channelRoleType,
                                 pChannelInfo->cachingEndpoint,
                                 pChannelInfo->endpointCachingPeriod,
                                 pChannelInfo->retry,
                                 pChannelInfo->reconnect,
                                 pChannelInfo->messageTtl,
                                 pChannelInfo->tagCount,
                                 pChannelInfo->pTags,
                                 &pSignalingClient->pChannelInfo));
    CHK_STATUS(validateSignalingCallbacks(pSignalingClient, pCallbacks));
    CHK_STATUS(validateSignalingClientInfo(pSignalingClient, pClientInfo));

    // Attempting to get the logging level from the env var and if it fails then set it from the client info
    if ((userLogLevelStr = GETENV(DEBUG_LOG_LEVEL_ENV_VAR)) != NULL && STATUS_SUCCEEDED(STRTOUI32(userLogLevelStr, NULL, 10, &userLogLevel))) {
        userLogLevel = userLogLevel > LOG_LEVEL_SILENT ? LOG_LEVEL_SILENT : userLogLevel < LOG_LEVEL_VERBOSE ? LOG_LEVEL_VERBOSE : userLogLevel;
    } else {
        userLogLevel = pClientInfo->loggingLevel;
    }

    SET_LOGGER_LOG_LEVEL(userLogLevel);

    // Store the credential provider
    pSignalingClient->pCredentialProvider = pCredentialProvider;

    // Create the state machine
    CHK_STATUS(createStateMachine(SIGNALING_STATE_MACHINE_STATES,
                                  SIGNALING_STATE_MACHINE_STATE_COUNT,
                                  CUSTOM_DATA_FROM_SIGNALING_CLIENT(pSignalingClient),
                                  kinesisVideoStreamDefaultGetCurrentTime,
                                  CUSTOM_DATA_FROM_SIGNALING_CLIENT(pSignalingClient),
                                  &pSignalingClient->pStateMachine));

    // Prepare the signaling channel protocols array
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_HTTPS].name = HTTPS_SCHEME_NAME;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_HTTPS].callback = lwsHttpCallbackRoutine;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_WSS].name = WSS_SCHEME_NAME;
    pSignalingClient->signalingProtocols[PROTOCOL_INDEX_WSS].callback = lwsWssCallbackRoutine;

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

    CHK(NULL != (pSignalingClient->pLwsContext = lws_create_context(&creationInfo)), STATUS_SIGNALING_LWS_CREATE_CONTEXT_FAILED);

    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->shutdown, FALSE);
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);

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

    pSignalingClient->lwsServiceLock = MUTEX_CREATE(FALSE);
    CHK(IS_VALID_MUTEX_VALUE(pSignalingClient->lwsServiceLock), STATUS_INVALID_OPERATION);

    // Create the ongoing message list
    CHK_STATUS(stackQueueCreate(&pSignalingClient->pMessageQueue));

    // Create the timer queue for handling stale ICE configuration
    pSignalingClient->timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    CHK_STATUS(timerQueueCreate(&pSignalingClient->timerQueueHandle));

    *ppSignalingClient = pSignalingClient;

    // Set the time out before execution
    pSignalingClient->stepUntil = GETTIME() + SIGNALING_CREATE_TIMEOUT;

    // Prime the state machine
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, STATUS_SUCCESS));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (STATUS_FAILED(retStatus)) {
        freeSignaling(&pSignalingClient);
    }

    if (ppSignalingClient != NULL) {
        *ppSignalingClient = pSignalingClient;
    }

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

    timerQueueFree(&pSignalingClient->timerQueueHandle);

    // Terminate the listener thread if alive
    terminateLwsListenerLoop(pSignalingClient);

    // Await for the reconnect thread to exit
    awaitForThreadTermination(&pSignalingClient->reconnecterTracker, SIGNALING_CLIENT_SHUTDOWN_TIMEOUT);

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

    uninitializeThreadTracker(&pSignalingClient->reconnecterTracker);
    uninitializeThreadTracker(&pSignalingClient->listenerTracker);

    MEMFREE(pSignalingClient);

    *ppSignalingClient = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingSendMessageSync(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pOfferType;
    BOOL removeFromList = FALSE;

    CHK(pSignalingClient != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK(pSignalingMessage->peerClientId != NULL && pSignalingMessage->payload != NULL, STATUS_INVALID_ARG);
    CHK(pSignalingMessage->version <= SIGNALING_MESSAGE_CURRENT_VERSION, STATUS_SIGNALING_INVALID_SIGNALING_MESSAGE_VERSION);

    // Prepare the buffer to send
    switch (pSignalingMessage->messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            pOfferType = (PCHAR) SIGNALING_SDP_TYPE_OFFER;
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            pOfferType = (PCHAR) SIGNALING_SDP_TYPE_ANSWER;
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            pOfferType = (PCHAR) SIGNALING_ICE_CANDIDATE;
            break;
        default:
            CHK(FALSE, STATUS_INVALID_ARG);
    }

    // Store the signaling message
    CHK_STATUS(signalingStoreOngoingMessage(pSignalingClient, pSignalingMessage));
    removeFromList = TRUE;

    // Perform the call
    CHK_STATUS(sendLwsMessage(pSignalingClient, pOfferType, pSignalingMessage->peerClientId,
                              pSignalingMessage->payload, pSignalingMessage->payloadLen,
                              pSignalingMessage->correlationId, 0));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

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

    // Validate the state
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine, SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED));

    *pIceConfigCount = pSignalingClient->iceConfigCount;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingGetIceConfigInfo(PSignalingClient pSignalingClient, UINT32 index, PIceConfigInfo* ppIceConfigInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && ppIceConfigInfo != NULL, STATUS_NULL_ARG);
    CHK(index < pSignalingClient->iceConfigCount, STATUS_INVALID_ARG);

    // Validate the state
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine, SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED));

    *ppIceConfigInfo = &pSignalingClient->iceConfigs[index];

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signalingConnectSync(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Validate the state
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine, SIGNALING_STATE_READY | SIGNALING_STATE_CONNECTED));

    // Check if we are already connected
    CHK (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected), retStatus);

    // Self-prime through the ready state
    pSignalingClient->continueOnReady = TRUE;

    // Set the time out before execution
    pSignalingClient->stepUntil = GETTIME() + SIGNALING_CONNECT_STATE_TIMEOUT;

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

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

    CHK_LOG_ERR_NV(retStatus);

    LEAVES();
    return retStatus;
}

STATUS validateSignalingClientInfo(PSignalingClient pSignalingClient, PSignalingClientInfo pClientInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL && pClientInfo != NULL, STATUS_NULL_ARG);
    CHK(pClientInfo->version <= SIGNALING_CLIENT_INFO_CURRENT_VERSION, STATUS_SIGNALING_INVALID_CLIENT_INFO_VERSION);
    CHK(STRNLEN(pClientInfo->clientId, MAX_SIGNALING_CLIENT_ID_LEN + 1) <= MAX_SIGNALING_CLIENT_ID_LEN, STATUS_SIGNALING_INVALID_CLIENT_INFO_CLIENT_LENGTH);

    // Store and validate
    pSignalingClient->clientInfo = *pClientInfo;

CleanUp:

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

    // Schedule the refresh on the timer queue
    CHK_STATUS(timerQueueAddTimer(pSignalingClient->timerQueueHandle,
                                  minTtl - ICE_CONFIGURATION_REFRESH_GRACE_PERIOD,
                                  TIMER_QUEUE_SINGLE_INVOCATION_PERIOD,
                                  refreshIceConfigurationCallback,
                                  (UINT64) pSignalingClient,
                                  &i));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS refreshIceConfigurationCallback(UINT32 timerId, UINT64 scheduledTime, UINT64 customData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = (PSignalingClient) customData;

    UNUSED_PARAM(timerId);
    UNUSED_PARAM(scheduledTime);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Kick off refresh of the ICE configurations
    CHK_STATUS(terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE));

    // Iterate the state machinery
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingStoreOngoingMessage(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PSignalingMessage pExistingMessage;

    CHK(pSignalingClient != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pSignalingClient->messageQueueLock);
    locked = TRUE;
    CHK_STATUS(signalingGetOngoingMessage(pSignalingClient, pSignalingMessage->correlationId, &pExistingMessage));
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

        if ((correlationId[0] == '\0' && pExistingMessage->correlationId[0] == '\0') ||
            0 == STRCMP(pExistingMessage->correlationId, correlationId)) {
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

STATUS signalingGetOngoingMessage(PSignalingClient pSignalingClient, PCHAR correlationId, PSignalingMessage* ppSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PSignalingMessage pExistingMessage;
    StackQueueIterator iterator;
    UINT64 data;

    CHK(pSignalingClient != NULL && correlationId != NULL && ppSignalingMessage != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pSignalingClient->messageQueueLock);
    locked = TRUE;

    CHK_STATUS(stackQueueGetIterator(pSignalingClient->pMessageQueue, &iterator));
    while (IS_VALID_ITERATOR(iterator)) {
        CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));

        pExistingMessage = (PSignalingMessage) data;
        CHK(pExistingMessage != NULL, STATUS_INTERNAL_ERROR);

        if ((correlationId[0] == '\0' && pExistingMessage->correlationId[0] == '\0') ||
            0 == STRCMP(pExistingMessage->correlationId, correlationId)) {
            *ppSignalingMessage = pExistingMessage;

            // Early return
            CHK(FALSE, retStatus);
        }

        CHK_STATUS(stackQueueIteratorNext(&iterator));
    }

    // Didn't find a match
    *ppSignalingMessage = NULL;

CleanUp:

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

    ATOMIC_STORE_BOOL(&pThreadTracker->terminated, FALSE);

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
        CHK_STATUS(CVAR_WAIT(pThreadTracker->await,
                             pThreadTracker->lock,
                             timeout));
    }

    pThreadTracker->threadId = INVALID_TID_VALUE;

    MUTEX_UNLOCK(pThreadTracker->lock);
    locked = FALSE;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pThreadTracker->lock);
    }

    return retStatus;
}
