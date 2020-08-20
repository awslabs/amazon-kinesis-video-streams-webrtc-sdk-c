/**
 * Implementation of a signaling states machine callbacks
 */
#define LOG_CLASS "SignalingState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState SIGNALING_STATE_MACHINE_STATES[] = {
    {SIGNALING_STATE_NEW, SIGNALING_STATE_NONE | SIGNALING_STATE_NEW, fromNewSignalingState, executeNewSignalingState, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_SIGNALING_INVALID_READY_STATE},
    {SIGNALING_STATE_GET_TOKEN,
     SIGNALING_STATE_NEW | SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG |
         SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_DELETE | SIGNALING_STATE_GET_TOKEN,
     fromGetTokenSignalingState, executeGetTokenSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_GET_TOKEN_CALL_FAILED},
    {SIGNALING_STATE_DESCRIBE,
     SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_CONNECT |
         SIGNALING_STATE_CONNECTED | SIGNALING_STATE_DELETE | SIGNALING_STATE_DESCRIBE,
     fromDescribeSignalingState, executeDescribeSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_DESCRIBE_CALL_FAILED},
    {SIGNALING_STATE_CREATE, SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE, fromCreateSignalingState, executeCreateSignalingState,
     SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_CREATE_CALL_FAILED},
    {SIGNALING_STATE_GET_ENDPOINT,
     SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT |
         SIGNALING_STATE_CONNECTED | SIGNALING_STATE_GET_ENDPOINT,
     fromGetEndpointSignalingState, executeGetEndpointSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED},
    {SIGNALING_STATE_GET_ICE_CONFIG,
     SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_READY |
         SIGNALING_STATE_GET_ICE_CONFIG,
     fromGetIceConfigSignalingState, executeGetIceConfigSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT,
     STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED},
    {SIGNALING_STATE_READY, SIGNALING_STATE_GET_ICE_CONFIG | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_READY, fromReadySignalingState,
     executeReadySignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_READY_CALLBACK_FAILED},
    {SIGNALING_STATE_CONNECT, SIGNALING_STATE_READY | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_CONNECT,
     fromConnectSignalingState, executeConnectSignalingState, INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_CONNECT_CALL_FAILED},
    {SIGNALING_STATE_CONNECTED, SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED, fromConnectedSignalingState, executeConnectedSignalingState,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_CONNECTED_CALLBACK_FAILED},
    {SIGNALING_STATE_DISCONNECTED, SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED, fromDisconnectedSignalingState,
     executeDisconnectedSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_DISCONNECTED_CALLBACK_FAILED},
    {SIGNALING_STATE_DELETE,
     SIGNALING_STATE_GET_TOKEN | SIGNALING_STATE_DESCRIBE | SIGNALING_STATE_CREATE | SIGNALING_STATE_GET_ENDPOINT | SIGNALING_STATE_GET_ICE_CONFIG |
         SIGNALING_STATE_READY | SIGNALING_STATE_CONNECT | SIGNALING_STATE_CONNECTED | SIGNALING_STATE_DISCONNECTED | SIGNALING_STATE_DELETE,
     fromDeleteSignalingState, executeDeleteSignalingState, SIGNALING_STATES_DEFAULT_RETRY_COUNT, STATUS_SIGNALING_DELETE_CALL_FAILED},
    {SIGNALING_STATE_DELETED, SIGNALING_STATE_DELETE | SIGNALING_STATE_DELETED, fromDeletedSignalingState, executeDeletedSignalingState,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_SIGNALING_DELETE_CALL_FAILED},
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
        case SIGNALING_STATE_DELETE:
            clientState = SIGNALING_CLIENT_STATE_DELETE;
            break;
        case SIGNALING_STATE_DELETED:
            clientState = SIGNALING_CLIENT_STATE_DELETED;
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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_CREDENTIALS));
    }

    // Use the credential provider to get the token
    retStatus = pSignalingClient->pCredentialProvider->getCredentialsFn(pSignalingClient->pCredentialProvider, &pSignalingClient->pAwsCredentials);

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
    retStatus = describeChannel(pSignalingClient, time);

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_CREATE));
    }

    // Call the aggregate function
    retStatus = createChannel(pSignalingClient, time);

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_ENDPOINT));
    }

    // Call the aggregate function
    retStatus = getChannelEndpoint(pSignalingClient, time);

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
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.stateChangeFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                            SIGNALING_CLIENT_STATE_GET_ICE_CONFIG));
    }

    // Call the aggregate function
    retStatus = getIceConfig(pSignalingClient, time);

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

    // Ensure we won't async the GetIceConfig as we reach the ready state
    ATOMIC_STORE_BOOL(&pSignalingClient->asyncGetIceConfig, FALSE);

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

    retStatus = connectSignalingChannel(pSignalingClient, time);

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
    UINT64 state = SIGNALING_STATE_READY;
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

    // Self-prime the next state
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

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

    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

    // Reset the ret status
    retStatus = STATUS_SUCCESS;

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
