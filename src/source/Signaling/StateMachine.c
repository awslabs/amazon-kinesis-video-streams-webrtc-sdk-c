/**
 * Implementation of a signaling states machine callbacks
 */
#define LOG_CLASS "SignalingState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState SIGNALING_STATE_MACHINE_STATES[] = {
    {SIGNALING_STATE_NEW, SIGNALING_STATE_NONE | SIGNALING_STATE_NEW, fromNewSignalingState, executeNewSignalingState,
     defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_INVALID_READY_STATE},
    {SIGNALING_STATE_GET_TOKEN,
     SIGNALING_STATE_NEW | SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT |
         SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION |
         SIGNALING_STATE_JOIN_SESSION_WAITING | SIGNALING_STATE_JOIN_SESSION_CONNECTED | SIGNALING_STATE_DELETE | SIGNALING_STATE_GET_TOKEN,
     fromGetTokenSignalingState, executeGetTokenSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_GET_TOKEN_CALL_FAILED},
    {SIGNALING_STATE_DESCRIBE,
     SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_CONNECT |
         SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION | SIGNALING_STATE_JOIN_SESSION_CONNECTED | SIGNALING_STATE_DELETE |
         SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_READY | SIGNALING_STATE_DISCONNECTED,
     fromDescribeSignalingState, executeDescribeSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_DESCRIBE_CALL_FAILED},
    {SIGNALING_STATE_DESCRIBE_MEDIA, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA, fromDescribeMediaStorageConfState,
     executeDescribeMediaStorageConfState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_DESCRIBE_MEDIA_CALL_FAILED},
    {SIGNALING_STATE_CREATE, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA | SIGNALING_STATE_CREATE, fromCreateSignalingState,
     executeCreateSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_CREATE_CALL_FAILED},
    {SIGNALING_STATE_GET_ENDPOINT,
     SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_READY |
         SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION | SIGNALING_STATE_JOIN_SESSION_CONNECTED |
         SIGNALING_STATE_GET_ENDPOINT,
     fromGetEndpointSignalingState, executeGetEndpointSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED},
    {SIGNALING_STATE_GET_ICE_CONFIG,
     SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION |
         SIGNALING_STATE_JOIN_SESSION_CONNECTED | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_READY | SIGNALING_STATE_GET_ICE_CONFIG,
     fromGetIceConfigSignalingState, executeGetIceConfigSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED},
    {SIGNALING_STATE_READY, SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_READY, fromReadySignalingState,
     executeReadySignalingState, defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_READY_CALLBACK_FAILED},
    {SIGNALING_STATE_CONNECT,
     SIGNALING_STATE_READY | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION | SIGNALING_STATE_CONNECT,
     fromConnectSignalingState, executeConnectSignalingState, defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_SIGNALING_CONNECT_CALL_FAILED},
    {SIGNALING_STATE_CONNECTED, SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION, fromConnectedSignalingState,
     executeConnectedSignalingState, defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_CONNECTED_CALLBACK_FAILED},
    {SIGNALING_STATE_DISCONNECTED,
     SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION | SIGNALING_STATE_JOIN_SESSION_WAITING |
         SIGNALING_STATE_JOIN_SESSION_CONNECTED,
     fromDisconnectedSignalingState, executeDisconnectedSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_DISCONNECTED_CALLBACK_FAILED},
    {SIGNALING_STATE_DELETE,
     SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_DESCRIBE_MEDIA | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT |
         SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION |
         SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_DELETE,
     fromDeleteSignalingState, executeDeleteSignalingState, defaultSignalingStateTransitionHook, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_DELETE_CALL_FAILED},
    {SIGNALING_STATE_DELETED, SIGNALING_STATE_DELETE | SIGNALING_STATE_DELETED, fromDeletedSignalingState, executeDeletedSignalingState,
     defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_DELETE_CALL_FAILED},
    {SIGNALING_STATE_JOIN_SESSION, SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION_WAITING | SIGNALING_STATE_JOIN_SESSION_CONNECTED,
     fromJoinStorageSessionState, executeJoinStorageSessionState, defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_SIGNALING_JOIN_SESSION_CALL_FAILED},
    {SIGNALING_STATE_JOIN_SESSION_WAITING, SIGNALING_STATE_JOIN_SESSION, fromJoinStorageSessionWaitingState, executeJoinStorageSessionWaitingState,
     defaultSignalingStateTransitionHook, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_JOIN_SESSION_CONNECTED_FAILED},
    {SIGNALING_STATE_JOIN_SESSION_CONNECTED, SIGNALING_STATE_JOIN_SESSION_WAITING | SIGNALING_STATE_JOIN_SESSION_CONNECTED,
     fromJoinStorageSessionConnectedState, executeJoinStorageSessionConnectedState, defaultSignalingStateTransitionHook,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_JOIN_SESSION_CONNECTED_FAILED}

};

UINT32 SIGNALING_STATE_MACHINE_STATE_COUNT = ARRAY_SIZE(SIGNALING_STATE_MACHINE_STATES);

STATUS defaultSignalingStateTransitionHook(UINT64 customData /* customData should be PSignalingClient */, PUINT64 stateTransitionWaitTime)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    STATUS countStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    PKvsRetryStrategy pSignalingStateMachineRetryStrategy = NULL;
    PKvsRetryStrategyCallbacks pSignalingStateMachineRetryStrategyCallbacks = NULL;
    UINT64 retryWaitTime = 0;

    pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    CHK(pSignalingClient != NULL && stateTransitionWaitTime != NULL, STATUS_NULL_ARG);

    pSignalingStateMachineRetryStrategy = &(pSignalingClient->clientInfo.signalingStateMachineRetryStrategy);
    pSignalingStateMachineRetryStrategyCallbacks = &(pSignalingClient->clientInfo.signalingStateMachineRetryStrategyCallbacks);

    // result > SERVICE_CALL_RESULT_OK covers case for -
    // result != SERVICE_CALL_RESULT_NOT_SET and != SERVICE_CALL_RESULT_OK
    // If we support any other 2xx service call results, the condition
    // should change to (pSignalingClient->result > 299 && ..)
    CHK(pSignalingClient->result > SERVICE_CALL_RESULT_OK && pSignalingStateMachineRetryStrategyCallbacks->executeRetryStrategyFn != NULL,
        STATUS_SUCCESS);

    // A retry is considered only after executeRetry is executed. This will avoid publishing count + 1
    if (pSignalingStateMachineRetryStrategyCallbacks->getCurrentRetryAttemptNumberFn != NULL) {
        if ((countStatus = pSignalingStateMachineRetryStrategyCallbacks->getCurrentRetryAttemptNumberFn(
                 pSignalingStateMachineRetryStrategy, &pSignalingClient->diagnostics.stateMachineRetryCount)) != STATUS_SUCCESS) {
            DLOGW("Failed to get retry count. Error code: %08x", countStatus);
        } else {
            DLOGD("Retry count: %llu", pSignalingClient->diagnostics.stateMachineRetryCount);
        }
    }
    DLOGV("Signaling Client base result is [%u]. Executing KVS retry handler of retry strategy type [%u]", pSignalingClient->result,
          pSignalingStateMachineRetryStrategy->retryStrategyType);
    pSignalingStateMachineRetryStrategyCallbacks->executeRetryStrategyFn(pSignalingStateMachineRetryStrategy, &retryWaitTime);
    *stateTransitionWaitTime = retryWaitTime;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingStateMachineIterator(PSignalingClient pSignalingClient, UINT64 expiration, UINT64 finalState)
{
    ENTERS();
    UINT64 currentTime;
    UINT32 i;
    STATUS retStatus = STATUS_SUCCESS;
    PStateMachineState pState = NULL;
    BOOL locked = FALSE;

    MUTEX_LOCK(pSignalingClient->stateLock);
    locked = TRUE;

    while (TRUE) {
        CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

        CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown), retStatus);

        if (STATUS_FAILED(retStatus)) {
            CHK(pSignalingClient->pChannelInfo->retry, retStatus);
            for (i = 0; i < SIGNALING_STATE_MACHINE_STATE_COUNT; i++) {
                CHK(retStatus != SIGNALING_STATE_MACHINE_STATES[i].status, SIGNALING_STATE_MACHINE_STATES[i].status);
            }
        }

        currentTime = SIGNALING_GET_CURRENT_TIME(pSignalingClient);

        CHK(expiration == 0 || currentTime <= expiration, STATUS_OPERATION_TIMED_OUT);

        // Fix-up the expired credentials transition
        // NOTE: Api Gateway might not return an error that can be interpreted as unauthorized to
        // make the correct transition to auth integration state.
        if (retStatus == STATUS_SERVICE_CALL_NOT_AUTHORIZED_ERROR ||
            (pSignalingClient->pAwsCredentials != NULL && pSignalingClient->pAwsCredentials->expiration < currentTime)) {
            // Set the call status as auth error
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_NOT_AUTHORIZED);
        }

        retStatus = stepStateMachine(pSignalingClient->pStateMachine);

        if (STATUS_FAILED(retStatus)) {
            DLOGD("Exited step state machine with status:  0x%08x", retStatus);
        }

        CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pState));

        DLOGV("State Machine - Current state: 0x%016" PRIx64, pState->state);

        CHK(!(pState->state == finalState), STATUS_SUCCESS);
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->stateLock);
    }

    LEAVES();
    return retStatus;
}

SIGNALING_CLIENT_STATE getSignalingStateFromStateMachineState(UINT64 state)
{
    SIGNALING_CLIENT_STATE clientState;
    switch (state) {
        case SIGNALING_STATE_NONE:
            clientState = SIGNALING_CLIENT_STATE_UNKNOWN;
            break;
        case SIGNALING_STATE_NEW:
            clientState = SIGNALING_CLIENT_STATE_NEW;
            break;
        case SIGNALING_STATE_GET_TOKEN:
            clientState = SIGNALING_CLIENT_STATE_GET_CREDENTIALS;
            break;
        case SIGNALING_STATE_DESCRIBE:
            clientState = SIGNALING_CLIENT_STATE_DESCRIBE;
            break;
        case SIGNALING_STATE_CREATE:
            clientState = SIGNALING_CLIENT_STATE_CREATE;
            break;
        case SIGNALING_STATE_GET_ENDPOINT:
            clientState = SIGNALING_CLIENT_STATE_GET_ENDPOINT;
            break;
        case SIGNALING_STATE_GET_ICE_CONFIG:
            clientState = SIGNALING_CLIENT_STATE_GET_ICE_CONFIG;
            break;
        case SIGNALING_STATE_READY:
            clientState = SIGNALING_CLIENT_STATE_READY;
            break;
        case SIGNALING_STATE_CONNECT:
            clientState = SIGNALING_CLIENT_STATE_CONNECTING;
            break;
        case SIGNALING_STATE_CONNECTED:
            clientState = SIGNALING_CLIENT_STATE_CONNECTED;
            break;
        case SIGNALING_STATE_DISCONNECTED:
            clientState = SIGNALING_CLIENT_STATE_DISCONNECTED;
            break;
        case SIGNALING_STATE_DELETE:
            clientState = SIGNALING_CLIENT_STATE_DELETE;
            break;
        case SIGNALING_STATE_DELETED:
            clientState = SIGNALING_CLIENT_STATE_DELETED;
            break;
        case SIGNALING_STATE_DESCRIBE_MEDIA:
            clientState = SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA;
            break;
        case SIGNALING_STATE_JOIN_SESSION:
            clientState = SIGNALING_CLIENT_STATE_JOIN_SESSION;
            break;
        case SIGNALING_STATE_JOIN_SESSION_WAITING:
            clientState = SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING;
            break;
        case SIGNALING_STATE_JOIN_SESSION_CONNECTED:
            clientState = SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED;
            break;
        default:
            clientState = SIGNALING_CLIENT_STATE_UNKNOWN;
    }

    return clientState;
}

STATUS acceptSignalingStateMachineState(PSignalingClient pSignalingClient, UINT64 state)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSignalingClient->stateLock);
    locked = TRUE;

    // Step the state machine
    CHK_STATUS(acceptStateMachineState(pSignalingClient->pStateMachine, state));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->stateLock);
    }

    LEAVES();
    return retStatus;
}

///////////////////////////////////////////////////////////////////////////
// State machine callback functions
///////////////////////////////////////////////////////////////////////////
STATUS fromNewSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    // Transition to auth state
    state = SIGNALING_STATE_GET_TOKEN;
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeNewSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_NEW));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetTokenSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_GET_TOKEN;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    if ((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK) {
        // Check if we are trying to delete a channel
        if (ATOMIC_LOAD_BOOL(&pSignalingClient->deleting)) {
            state = SIGNALING_STATE_DELETE;
        } else if (pSignalingClient->pChannelInfo->pChannelArn != NULL && pSignalingClient->pChannelInfo->pChannelArn[0] != '\0') {
            // If the client application has specified the Channel ARN then we will skip describe and create states
            // Store the ARN in the stream description object first
            STRNCPY(pSignalingClient->channelDescription.channelArn, pSignalingClient->pChannelInfo->pChannelArn, MAX_ARN_LEN);
            pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
            // Move to get endpoint state if the media storage is not enabled.
            state = SIGNALING_STATE_GET_ENDPOINT;
        } else {
            state = SIGNALING_STATE_DESCRIBE;
        }
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetTokenSignalingState(UINT64 customData, UINT64 time)
{
    UNUSED_PARAM(time);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    SERVICE_CALL_RESULT serviceCallResult;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_CREDENTIALS));
    }

    THREAD_SLEEP_UNTIL(time);

    // Use the credential provider to get the token
    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = pSignalingClient->pCredentialProvider->getCredentialsFn(pSignalingClient->pCredentialProvider,
                                                                                                          &pSignalingClient->pAwsCredentials),
                                      pSignalingClient->diagnostics.getTokenStartTime, pSignalingClient->diagnostics.getTokenEndTime,
                                      pSignalingClient->diagnostics.getTokenCallTime, "Get token call");

    // Check the expiration
    if (NULL == pSignalingClient->pAwsCredentials || SIGNALING_GET_CURRENT_TIME(pSignalingClient) >= pSignalingClient->pAwsCredentials->expiration) {
        serviceCallResult = SERVICE_CALL_NOT_AUTHORIZED;
    } else {
        serviceCallResult = SERVICE_CALL_RESULT_OK;
    }

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) serviceCallResult);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDescribeSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_DESCRIBE;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            // If we are trying to delete the channel then move to delete state
            if (ATOMIC_LOAD_BOOL(&pSignalingClient->deleting)) {
                state = SIGNALING_STATE_DELETE;
            } else {
                if (pSignalingClient->pChannelInfo->useMediaStorage) {
                    state = SIGNALING_STATE_DESCRIBE_MEDIA;
                } else {
                    state = SIGNALING_STATE_GET_ENDPOINT;
                }
            }

            break;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_CREATE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        default:
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDescribeSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_DESCRIBE));
    }

    // Call the aggregate function
    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = describeChannel(pSignalingClient, time), pSignalingClient->diagnostics.describeChannelStartTime,
                                      pSignalingClient->diagnostics.describeChannelEndTime, pSignalingClient->diagnostics.describeCallTime,
                                      "Describe signaling call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDescribeMediaStorageConfState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_DESCRIBE_MEDIA;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            // If we are trying to delete the channel then move to delete state
            if (ATOMIC_LOAD_BOOL(&pSignalingClient->deleting)) {
                state = SIGNALING_STATE_DELETE;
            } else {
                state = SIGNALING_STATE_GET_ENDPOINT;
            }
            break;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_CREATE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        default:
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDescribeMediaStorageConfState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 startTimeInMacro = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA));
    }

    // Call the aggregate function
    PROFILE_CALL_WITH_T_OBJ(retStatus = describeMediaStorageConf(pSignalingClient, time), pSignalingClient->diagnostics.describeMediaCallTime,
                            "Describe Media Storage call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromCreateSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CREATE;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        default:
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeCreateSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_CREATE));
    }

    // Call the aggregate function
    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = createChannel(pSignalingClient, time), pSignalingClient->diagnostics.createChannelStartTime,
                                      pSignalingClient->diagnostics.createChannelEndTime, pSignalingClient->diagnostics.createCallTime,
                                      "Create signaling call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetEndpointSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_GET_ENDPOINT;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        default:
            break;
    }
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetEndpointSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_ENDPOINT));
    }

    // Call the aggregate function
    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = getChannelEndpoint(pSignalingClient, time),
                                      pSignalingClient->diagnostics.getSignalingChannelEndpointStartTime,
                                      pSignalingClient->diagnostics.getSignalingChannelEndpointEndTime,
                                      pSignalingClient->diagnostics.getEndpointCallTime, "Get endpoint signaling call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetIceConfigSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_GET_ICE_CONFIG;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            state = SIGNALING_STATE_READY;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;
        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            // This can happen if we read from the cache and the channel either doesn't exist
            // Or was re-created so now has a new channel arn.  We need to invalidate the cache.
            pSignalingClient->describeTime = INVALID_TIMESTAMP_VALUE;
            pSignalingClient->describeMediaTime = INVALID_TIMESTAMP_VALUE;
            pSignalingClient->getEndpointTime = INVALID_TIMESTAMP_VALUE;
            state = SIGNALING_STATE_DESCRIBE;
            break;

        default:
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetIceConfigSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_ICE_CONFIG));
    }

    // Call the aggregate function
    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = getIceConfig(pSignalingClient, time), pSignalingClient->diagnostics.getIceServerConfigStartTime,
                                      pSignalingClient->diagnostics.getIceServerConfigEndTime, pSignalingClient->diagnostics.getIceConfigCallTime,
                                      "Get ICE config signaling call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromReadySignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECT;

    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            state = SIGNALING_STATE_READY;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        default:
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeReadySignalingState(UINT64 customData, UINT64 time)
{
    UNUSED_PARAM(time);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, TRUE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_READY));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromConnectSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECT;
    SIZE_T result;
    BOOL connected;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    connected = ATOMIC_LOAD_BOOL(&pSignalingClient->connected);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            // We also need to check whether we terminated OK and connected or
            // simply terminated without being connected
            if (connected) {
                state = SIGNALING_STATE_CONNECTED;
            }

            break;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        case SERVICE_CALL_INTERNAL_ERROR:
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        case SERVICE_CALL_BAD_REQUEST:
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        case SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT:
        case SERVICE_CALL_NETWORK_READ_TIMEOUT:
        case SERVICE_CALL_REQUEST_TIMEOUT:
        case SERVICE_CALL_GATEWAY_TIMEOUT:
            // Attempt to get a new endpoint
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        default:
            state = SIGNALING_STATE_GET_TOKEN;
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeConnectSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_CONNECTING));
    }

    PROFILE_CALL_WITH_START_END_T_OBJ(retStatus = connectSignalingChannel(pSignalingClient, time), pSignalingClient->diagnostics.connectStartTime,
                                      pSignalingClient->diagnostics.connectEndTime, pSignalingClient->diagnostics.connectCallTime,
                                      "Connect signaling call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromConnectedSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECTED;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
                state = SIGNALING_STATE_DISCONNECTED;
            } else if (pSignalingClient->mediaStorageConfig.storageStatus) {
                state = SIGNALING_STATE_JOIN_SESSION;
            }

            break;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        case SERVICE_CALL_INTERNAL_ERROR:
        case SERVICE_CALL_BAD_REQUEST:
        case SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT:
        case SERVICE_CALL_NETWORK_READ_TIMEOUT:
        case SERVICE_CALL_REQUEST_TIMEOUT:
        case SERVICE_CALL_GATEWAY_TIMEOUT:
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_GO_AWAY:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        default:
            state = SIGNALING_STATE_GET_TOKEN;
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeConnectedSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_CONNECTED));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromJoinStorageSessionState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECT;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);

    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
                state = SIGNALING_STATE_DISCONNECTED;
            } else {
                state = SIGNALING_STATE_JOIN_SESSION_WAITING;
            }
            break;

        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        case SERVICE_CALL_INTERNAL_ERROR:
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        case SERVICE_CALL_BAD_REQUEST:
        case SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT:
        case SERVICE_CALL_NETWORK_READ_TIMEOUT:
        case SERVICE_CALL_REQUEST_TIMEOUT:
        case SERVICE_CALL_GATEWAY_TIMEOUT:
            // Attempt to get a new endpoint
            state = SIGNALING_STATE_GET_ENDPOINT;
            break;

        default:
            DLOGW("unknown response code(%d).", result);
            state = SIGNALING_STATE_GET_TOKEN;
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeJoinStorageSessionState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 startTimeInMacro = 0;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_JOIN_SESSION));
    }

    // In case we are re-trying we need to reset this to false
    ATOMIC_STORE_BOOL(&pSignalingClient->offerReceived, FALSE);
    PROFILE_CALL_WITH_T_OBJ(retStatus = joinStorageSession(pSignalingClient, time), pSignalingClient->diagnostics.joinSessionCallTime,
                            "Join Session call");

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromJoinStorageSessionWaitingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_JOIN_SESSION;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);

    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
                state = SIGNALING_STATE_DISCONNECTED;
            } else {
                state = SIGNALING_STATE_JOIN_SESSION_CONNECTED;
            }
            break;
        case SERVICE_CALL_RESULT_NOT_SET:
            // We timed out and did not get an offer in time
            // so if we are still connected we need to retry join session
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
                state = SIGNALING_STATE_DISCONNECTED;
            } else {
                state = SIGNALING_STATE_JOIN_SESSION;
            }
            break;

        default:
            DLOGW("unknown response code(%d).", result);
            state = SIGNALING_STATE_GET_TOKEN;
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeJoinStorageSessionWaitingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING));
    }

    MUTEX_LOCK(pSignalingClient->jssWaitLock);
    locked = TRUE;
    while (!ATOMIC_LOAD(&pSignalingClient->offerReceived)) {
        DLOGI("Waiting for offer from JoinStorageSession Call.");
        CHK_STATUS(CVAR_WAIT(pSignalingClient->jssWaitCvar, pSignalingClient->jssWaitLock, SIGNALING_JOIN_STORAGE_SESSION_WAIT_TIMEOUT));
    }
    MUTEX_UNLOCK(pSignalingClient->jssWaitLock);
    locked = FALSE;

CleanUp:

    if (retStatus == STATUS_OPERATION_TIMED_OUT) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    } else if (STATUS_SUCCEEDED(retStatus)) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
    } else {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->jssWaitLock);
    }

    LEAVES();
    return retStatus;
}

STATUS fromJoinStorageSessionConnectedState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECTED;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_RESULT_OK:
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
                state = SIGNALING_STATE_DISCONNECTED;
            } else if (pSignalingClient->mediaStorageConfig.storageStatus) {
                // Before calling JoinSession again after stepping out of the
                // storage streaming state, we need to update ice config
                state = SIGNALING_STATE_GET_ICE_CONFIG;
            }

            break;

        case SERVICE_CALL_RESULT_SIGNALING_GO_AWAY:
            state = SIGNALING_STATE_DESCRIBE;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        default:
            state = SIGNALING_STATE_GET_TOKEN;
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeJoinStorageSessionConnectedState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDisconnectedSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_READY;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        case SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE:
            state = SIGNALING_STATE_GET_ICE_CONFIG;
            break;

        default:
            break;
    }

    // Overwrite the state if we are force refreshing
    state = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->refreshIceConfig, FALSE) ? SIGNALING_STATE_GET_ICE_CONFIG : state;

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDisconnectedSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_DISCONNECTED));
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDeleteSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_DELETE;

    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
        case SERVICE_CALL_FORBIDDEN:
        case SERVICE_CALL_NOT_AUTHORIZED:
            state = SIGNALING_STATE_GET_TOKEN;
            break;

        case SERVICE_CALL_RESULT_OK:
        case SERVICE_CALL_RESOURCE_DELETED:
        case SERVICE_CALL_RESOURCE_NOT_FOUND:
            state = SIGNALING_STATE_DELETED;
            break;

        case SERVICE_CALL_BAD_REQUEST:
            // This can happen if we come in from specifying ARN and skipping Describe state
            // during the creation in which case we still need to get the proper update version
            state = SIGNALING_STATE_DESCRIBE;
            break;

        default:
            break;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDeleteSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
    ATOMIC_STORE_BOOL(&pSignalingClient->clientReady, FALSE);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_DELETE));
    }

    // Call the aggregate function
    retStatus = deleteChannel(pSignalingClient, time);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDeletedSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_DELETED;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    // This is a terminal state
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeDeletedSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_DELETED));
    }

    // No-op

CleanUp:

    LEAVES();
    return retStatus;
}
