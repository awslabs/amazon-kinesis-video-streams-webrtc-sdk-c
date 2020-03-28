/**
 * Implementation of a signaling states machine callbacks
 */
#define LOG_CLASS "SignalingState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState SIGNALING_STATE_MACHINE_STATES[] = {
        {SIGNALING_STATE_NEW, SIGNALING_STATE_NONE | SIGNALING_STATE_NEW, fromNewSignalingState, executeNewSignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_INVALID_READY_STATE},
        {SIGNALING_STATE_GET_TOKEN, SIGNALING_STATE_NEW | SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_GET_TOKEN, fromGetTokenSignalingState, executeGetTokenSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_GET_TOKEN_CALL_FAILED},
        {SIGNALING_STATE_DESCRIBE, SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_DESCRIBE, fromDescribeSignalingState, executeDescribeSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_DESCRIBE_CALL_FAILED},
        {SIGNALING_STATE_CREATE, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE, fromCreateSignalingState, executeCreateSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_CREATE_CALL_FAILED},
        {SIGNALING_STATE_GET_ENDPOINT, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_GET_ENDPOINT, fromGetEndpointSignalingState, executeGetEndpointSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED},
        {SIGNALING_STATE_GET_ICE_CONFIG, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_READY | SIGNALING_STATE_GET_ICE_CONFIG, fromGetIceConfigSignalingState, executeGetIceConfigSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED},
        {SIGNALING_STATE_READY, SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_READY, fromReadySignalingState, executeReadySignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_READY_CALLBACK_FAILED},
        {SIGNALING_STATE_CONNECT, SIGNALING_STATE_READY | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_CONNECT, fromConnectSignalingState, executeConnectSignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_CONNECT_CALL_FAILED},
        {SIGNALING_STATE_CONNECTED, SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED, fromConnectedSignalingState, executeConnectedSignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_CONNECTED_CALLBACK_FAILED},
        {SIGNALING_STATE_DISCONNECTED, SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED, fromDisconnectedSignalingState, executeDisconnectedSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_DISCONNECTED_CALLBACK_FAILED},
};

UINT32 SIGNALING_STATE_MACHINE_STATE_COUNT = ARRAY_SIZE(SIGNALING_STATE_MACHINE_STATES);

STATUS stepSignalingStateMachine(PSignalingClient pSignalingClient, STATUS status)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;
    BOOL locked = FALSE;
    UINT64 currentTime;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Check for a shutdown
    CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown), retStatus);

    MUTEX_LOCK(pSignalingClient->stateLock);
    locked = TRUE;

    // Check if an error and the retry is OK
    if (!pSignalingClient->pChannelInfo->retry && STATUS_FAILED(status)) {
        CHK(FALSE, status);
    }

    currentTime = GETTIME();

    CHK(pSignalingClient->stepUntil == 0 || currentTime <= pSignalingClient->stepUntil, STATUS_OPERATION_TIMED_OUT);

    // Check if the status is any of the retry/failed statuses
    if (STATUS_FAILED(status)) {
        for (i = 0; i < SIGNALING_STATE_MACHINE_STATE_COUNT; i++) {
            CHK(status != SIGNALING_STATE_MACHINE_STATES[i].status, SIGNALING_STATE_MACHINE_STATES[i].status);
        }
    }

    // Fix-up the expired credentials transition
    // NOTE: Api Gateway might not return an error that can be interpreted as unauthorized to
    // make the correct transition to auth integration state.
    if (status == STATUS_SERVICE_CALL_NOT_AUTHORIZED_ERROR ||
        (SERVICE_CALL_UNKNOWN == (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) &&
        pSignalingClient->pAwsCredentials->expiration < currentTime)) {
        // Set the call status as auth error
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_NOT_AUTHORIZED);
    }

    // Step the state machine
    CHK_STATUS(stepStateMachine(pSignalingClient->pStateMachine));

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
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
        // If the client application has specified the Channel ARN then we will skip describe and create states
        if (pSignalingClient->pChannelInfo->pChannelArn != NULL && pSignalingClient->pChannelInfo->pChannelArn[0] != '\0') {
            // Store the ARN in the stream description object first
            STRNCPY(pSignalingClient->channelDescription.channelArn, pSignalingClient->pChannelInfo->pChannelArn, MAX_ARN_LEN);
            pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';

            // Move to get endpoint state
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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_GET_CREDENTIALS));
    }

    // Use the credential provider to get the token
    retStatus = pSignalingClient->pCredentialProvider->getCredentialsFn(pSignalingClient->pCredentialProvider,
                                                                        &pSignalingClient->pAwsCredentials);

    // Check the expiration
    if (GETTIME() >= pSignalingClient->pAwsCredentials->expiration) {
        serviceCallResult = SERVICE_CALL_NOT_AUTHORIZED;
    } else {
        serviceCallResult = SERVICE_CALL_RESULT_OK;
    }

    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) serviceCallResult);

    // Self-prime the next state
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
            state = SIGNALING_STATE_GET_ENDPOINT;
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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_DESCRIBE));
    }

    // Call pre hook func
    if (pSignalingClient->clientInfo.describePreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.describePreHookFn(SIGNALING_STATE_DESCRIBE,
                pSignalingClient->clientInfo.hookCustomData);
    }

    // Call DescribeChannel API
    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = describeChannelLws(pSignalingClient, time);
    }

    // Call post hook func
    if (pSignalingClient->clientInfo.describePostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.describePostHookFn(SIGNALING_STATE_DESCRIBE,
                pSignalingClient->clientInfo.hookCustomData);
    }

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_CREATE));
    }

    if (pSignalingClient->clientInfo.createPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.createPreHookFn(SIGNALING_STATE_CREATE,
                pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = createChannelLws(pSignalingClient, time);
    }

    if (pSignalingClient->clientInfo.createPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.createPostHookFn(SIGNALING_STATE_CREATE,
                pSignalingClient->clientInfo.hookCustomData);
    }

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_GET_ENDPOINT));
    }

    if (pSignalingClient->clientInfo.getEndpointPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getEndpointPreHookFn(SIGNALING_STATE_GET_ENDPOINT,
                pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = getChannelEndpointLws(pSignalingClient, time);
    }

    if (pSignalingClient->clientInfo.getEndpointPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getEndpointPostHookFn(SIGNALING_STATE_GET_ENDPOINT,
                pSignalingClient->clientInfo.hookCustomData);
    }

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_GET_ICE_CONFIG));
    }

    if (pSignalingClient->clientInfo.getIceConfigPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getIceConfigPreHookFn(SIGNALING_STATE_GET_ICE_CONFIG,
                pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        retStatus = getIceConfigLws(pSignalingClient, time);
    }

    if (pSignalingClient->clientInfo.getIceConfigPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.getIceConfigPostHookFn(SIGNALING_STATE_GET_ICE_CONFIG,
                pSignalingClient->clientInfo.hookCustomData);
    }

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    // Move to connect only when we had previously connected
    if(SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE == (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result)) {
        state = SIGNALING_STATE_GET_ICE_CONFIG;
    }

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_READY));
    }

    if (pSignalingClient->continueOnReady) {
        // Self-prime the connect
        CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));
    } else {
        // Reset the timeout for the state machine
        pSignalingClient->stepUntil = 0;
    }

    // Reset the ret status
    retStatus = STATUS_SUCCESS;
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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_CONNECTING));
    }

    if (pSignalingClient->clientInfo.connectPreHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.connectPreHookFn(SIGNALING_STATE_CONNECT,
                pSignalingClient->clientInfo.hookCustomData);
    }

    if (STATUS_SUCCEEDED(retStatus)) {
        // No need to reconnect again if already connected. This can happen if we get to this state after ice refresh
        if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);
            retStatus = connectSignalingChannelLws(pSignalingClient, time);
        } else {
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
        }
    }

    if (pSignalingClient->clientInfo.connectPostHookFn != NULL) {
        retStatus = pSignalingClient->clientInfo.connectPostHookFn(SIGNALING_STATE_CONNECT,
                pSignalingClient->clientInfo.hookCustomData);
    }

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_CONNECTED));
    }

    // Reset the timeout for the state machine
    MUTEX_LOCK(pSignalingClient->stateLock);
    pSignalingClient->stepUntil = 0;
    MUTEX_UNLOCK(pSignalingClient->stateLock);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromDisconnectedSignalingState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);
    UINT64 state = SIGNALING_STATE_CONNECT;
    SIZE_T result;

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    // See if we need to retry first of all
    CHK(pSignalingClient->pChannelInfo->reconnect, STATUS_SUCCESS);

    result = ATOMIC_LOAD(&pSignalingClient->result);
    switch (result) {
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

STATUS executeDisconnectedSignalingState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = SIGNALING_CLIENT_FROM_CUSTOM_DATA(customData);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Notify of the state change
    if (pSignalingClient->signalingClientCallbacks.stateChangeFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(
                pSignalingClient->signalingClientCallbacks.customData,
                SIGNALING_CLIENT_STATE_DISCONNECTED));
    }

    // Self-prime the next state
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    LEAVES();
    return retStatus;
}
