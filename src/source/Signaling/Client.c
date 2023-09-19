#define LOG_CLASS "SignalingClient"
#include "../Include_i.h"

STATUS createRetryStrategyForCreatingSignalingClient(PSignalingClientInfo pClientInfo, PKvsRetryStrategy pKvsRetryStrategy)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pKvsRetryStrategy != NULL && pClientInfo != NULL, STATUS_NULL_ARG);

    if (pClientInfo->signalingRetryStrategyCallbacks.createRetryStrategyFn == NULL ||
        pClientInfo->signalingRetryStrategyCallbacks.freeRetryStrategyFn == NULL ||
        pClientInfo->signalingRetryStrategyCallbacks.executeRetryStrategyFn == NULL) {
        DLOGV("Using exponential backoff retry strategy for creating signaling client");
        pClientInfo->signalingRetryStrategyCallbacks.createRetryStrategyFn = exponentialBackoffRetryStrategyCreate;
        pClientInfo->signalingRetryStrategyCallbacks.freeRetryStrategyFn = exponentialBackoffRetryStrategyFree;
        pClientInfo->signalingRetryStrategyCallbacks.executeRetryStrategyFn = getExponentialBackoffRetryStrategyWaitTime;
    }

    // Create retry strategy will use default config 'DEFAULT_EXPONENTIAL_BACKOFF_CONFIGURATION' defined in -
    // https://github.com/awslabs/amazon-kinesis-video-streams-pic/blob/develop/src/utils/include/com/amazonaws/kinesis/video/utils/Include.h
    CHK_STATUS(pClientInfo->signalingRetryStrategyCallbacks.createRetryStrategyFn(pKvsRetryStrategy));

    CHK(pKvsRetryStrategy->retryStrategyType == KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT, STATUS_INTERNAL_ERROR);
    CHK(pKvsRetryStrategy->pRetryStrategy != NULL, STATUS_INTERNAL_ERROR);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Some internal error occurred while setting up retry strategy for creating signaling client [0x%08x]", retStatus);
        pClientInfo->signalingRetryStrategyCallbacks.freeRetryStrategyFn(pKvsRetryStrategy);
    }

    LEAVES();
    return retStatus;
}

STATUS freeRetryStrategyForCreatingSignalingClient(PSignalingClientInfo pClientInfo, PKvsRetryStrategy pKvsRetryStrategy)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pClientInfo != NULL && pKvsRetryStrategy != NULL, STATUS_NULL_ARG);

    if (pKvsRetryStrategy->pRetryStrategy != NULL) {
        pClientInfo->signalingRetryStrategyCallbacks.freeRetryStrategyFn(pKvsRetryStrategy);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS createSignalingClientSync(PSignalingClientInfo pClientInfo, PChannelInfo pChannelInfo, PSignalingClientCallbacks pCallbacks,
                                 PAwsCredentialProvider pCredentialProvider, PSIGNALING_CLIENT_HANDLE pSignalingHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = NULL;
    SignalingClientInfoInternal signalingClientInfoInternal;
    KvsRetryStrategy createSignalingClientRetryStrategy = {NULL, NULL, KVS_RETRY_STRATEGY_DISABLED};
    INT32 signalingClientCreationMaxRetryCount;
    UINT64 signalingClientCreationWaitTime;
    UINT64 startTime = 0;

    DLOGI("Creating Signaling Client Sync");
    CHK(pSignalingHandle != NULL && pClientInfo != NULL, STATUS_NULL_ARG);

    // Convert the client info to the internal structure with empty values
    MEMSET(&signalingClientInfoInternal, 0x00, SIZEOF(signalingClientInfoInternal));
    signalingClientInfoInternal.signalingClientInfo = *pClientInfo;

    CHK_STATUS(createRetryStrategyForCreatingSignalingClient(pClientInfo, &createSignalingClientRetryStrategy));

    if (pClientInfo->signalingClientCreationMaxRetryAttempts == CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE) {
        signalingClientCreationMaxRetryCount = DEFAULT_CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS;
    } else {
        signalingClientCreationMaxRetryCount = pClientInfo->signalingClientCreationMaxRetryAttempts;
    }
    startTime = GETTIME();
    while (TRUE) {
        retStatus = createSignalingSync(&signalingClientInfoInternal, pChannelInfo, pCallbacks, pCredentialProvider, &pSignalingClient);
        // NOTE: This will retry on all status codes except SUCCESS.
        // This includes status codes for bad arguments, internal non-recoverable errors etc.
        // Retrying on non-recoverable errors is useless, but it is quite complex to segregate recoverable
        // and non-recoverable errors at this layer. So to simplify, we would retry on all non-success status codes.
        // It is the application's responsibility to fix any validation/null-arg/bad configuration type errors.
        CHK(retStatus != STATUS_SUCCESS, retStatus);

        DLOGE("Create Signaling Sync API returned [0x%08x]  %u\n", retStatus, signalingClientCreationMaxRetryCount);
        if (signalingClientCreationMaxRetryCount <= 0) {
            break;
        }

        pClientInfo->stateMachineRetryCountReadOnly = signalingClientInfoInternal.signalingClientInfo.stateMachineRetryCountReadOnly;

        // Wait before attempting to create signaling client
        CHK_STATUS(pClientInfo->signalingRetryStrategyCallbacks.executeRetryStrategyFn(&createSignalingClientRetryStrategy,
                                                                                       &signalingClientCreationWaitTime));

        DLOGV("Attempting to back off for [%lf] milliseconds before creating signaling client again. "
              "Signaling client creation retry count [%u]",
              retStatus, signalingClientCreationWaitTime / 1000.0f, signalingClientCreationMaxRetryCount);
        THREAD_SLEEP(signalingClientCreationWaitTime);
        signalingClientCreationMaxRetryCount--;
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Create signaling client API failed with return code [0x%08x]", retStatus);
        freeSignaling(&pSignalingClient);
    } else {
        PROFILE_WITH_START_TIME_OBJ(startTime, pSignalingClient->diagnostics.createClientTime, "Create signaling client");
        *pSignalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    }

    freeRetryStrategyForCreatingSignalingClient(pClientInfo, &createSignalingClientRetryStrategy);

    LEAVES();
    return retStatus;
}

STATUS freeSignalingClient(PSIGNALING_CLIENT_HANDLE pSignalingHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient;

    DLOGV("Freeing Signaling Client");
    CHK(pSignalingHandle != NULL, STATUS_NULL_ARG);

    // Get the client handle
    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(*pSignalingHandle);

    CHK_STATUS(freeSignaling(&pSignalingClient));

    // Set the signaling client handle pointer to invalid
    *pSignalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientSendMessageSync(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSignalingMessage pSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGV("Signaling Client Sending Message Sync");

    CHK_STATUS(signalingSendMessageSync(pSignalingClient, pSignalingMessage));

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientConnectSync(SIGNALING_CLIENT_HANDLE signalingClientHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);
    UINT64 startTimeInMacro = 0;

    DLOGV("Signaling Client Connect Sync");

    PROFILE_CALL_WITH_T_OBJ(CHK_STATUS(signalingConnectSync(pSignalingClient)), pSignalingClient->diagnostics.connectClientTime,
                            "Connect signaling client");

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientFetchSync(SIGNALING_CLIENT_HANDLE signalingClientHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);
    SignalingClientInfoInternal signalingClientInfoInternal;
    KvsRetryStrategy createSignalingClientRetryStrategy = {NULL, NULL, KVS_RETRY_STRATEGY_DISABLED};
    INT32 signalingClientCreationMaxRetryCount;
    UINT64 signalingClientCreationWaitTime;
    UINT64 startTime = 0;

    DLOGI("Signaling Client Fetch Sync");
    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Convert the client info to the internal structure with empty values
    MEMSET(&signalingClientInfoInternal, 0x00, SIZEOF(signalingClientInfoInternal));
    signalingClientInfoInternal.signalingClientInfo = pSignalingClient->clientInfo.signalingClientInfo;

    CHK_STATUS(createRetryStrategyForCreatingSignalingClient(&pSignalingClient->clientInfo.signalingClientInfo, &createSignalingClientRetryStrategy));

    signalingClientCreationMaxRetryCount = pSignalingClient->clientInfo.signalingClientInfo.signalingClientCreationMaxRetryAttempts;
    if (signalingClientCreationMaxRetryCount == CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE) {
        signalingClientCreationMaxRetryCount = DEFAULT_CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS;
    }
    startTime = GETTIME();
    while (TRUE) {
        retStatus = signalingFetchSync(pSignalingClient);
        // NOTE: This will retry on all status codes except SUCCESS.
        // This includes status codes for bad arguments, internal non-recoverable errors etc.
        // Retrying on non-recoverable errors is useless, but it is quite complex to segregate recoverable
        // and non-recoverable errors at this layer. So to simplify, we would retry on all non-success status codes.
        // It is the application's responsibility to fix any validation/null-arg/bad configuration type errors.
        CHK(retStatus != STATUS_SUCCESS, retStatus);

        DLOGE("Create Signaling Sync API returned [0x%08x]  %u\n", retStatus, signalingClientCreationMaxRetryCount);
        if (signalingClientCreationMaxRetryCount <= 0) {
            break;
        }

        pSignalingClient->clientInfo.signalingClientInfo.stateMachineRetryCountReadOnly =
            signalingClientInfoInternal.signalingClientInfo.stateMachineRetryCountReadOnly;

        // Wait before attempting to create signaling client
        CHK_STATUS(pSignalingClient->clientInfo.signalingStateMachineRetryStrategyCallbacks.executeRetryStrategyFn(
            &createSignalingClientRetryStrategy, &signalingClientCreationWaitTime));

        DLOGV("Attempting to back off for [%lf] milliseconds before creating signaling client again. "
              "Signaling client creation retry count [%u]",
              retStatus, signalingClientCreationWaitTime / 1000.0f, signalingClientCreationMaxRetryCount);
        THREAD_SLEEP(signalingClientCreationWaitTime);
        signalingClientCreationMaxRetryCount--;
    }

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    if (pSignalingClient != NULL) {
        freeRetryStrategyForCreatingSignalingClient(&pSignalingClient->clientInfo.signalingClientInfo, &createSignalingClientRetryStrategy);
        PROFILE_WITH_START_TIME_OBJ(startTime, pSignalingClient->diagnostics.fetchClientTime, "Fetch signaling client");
    }
    LEAVES();
    return retStatus;
}

STATUS signalingClientDisconnectSync(SIGNALING_CLIENT_HANDLE signalingClientHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGV("Signaling Client Disconnect Sync");

    CHK_STATUS(signalingDisconnectSync(pSignalingClient));

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientDeleteSync(SIGNALING_CLIENT_HANDLE signalingClientHandle)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGV("Signaling Client Delete Sync");

    CHK_STATUS(signalingDeleteSync(pSignalingClient));

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientGetIceConfigInfoCount(SIGNALING_CLIENT_HANDLE signalingClientHandle, PUINT32 pIceConfigCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGV("Signaling Client Get ICE Config Info Count");

    CHK_STATUS(signalingGetIceConfigInfoCount(pSignalingClient, pIceConfigCount));

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientGetIceConfigInfo(SIGNALING_CLIENT_HANDLE signalingClientHandle, UINT32 index, PIceConfigInfo* ppIceConfigInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);

    DLOGV("Signaling Client Get ICE Config Info");

    CHK_STATUS(signalingGetIceConfigInfo(pSignalingClient, index, ppIceConfigInfo));

CleanUp:

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientGetCurrentState(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSIGNALING_CLIENT_STATE pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    SIGNALING_CLIENT_STATE state = SIGNALING_CLIENT_STATE_UNKNOWN;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);
    PStateMachineState pStateMachineState;

    DLOGV("Signaling Client Get Current State");

    CHK(pSignalingClient != NULL && pState != NULL, STATUS_NULL_ARG);

    CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));
    state = getSignalingStateFromStateMachineState(pStateMachineState->state);

    DLOGV("Current state: 0x%016" PRIx64, pStateMachineState->state);

CleanUp:

    if (pState != NULL) {
        *pState = state;
    }

    SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    LEAVES();
    return retStatus;
}

STATUS signalingClientGetStateString(SIGNALING_CLIENT_STATE state, PCHAR* ppStateStr)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppStateStr != NULL, STATUS_NULL_ARG);

    switch (state) {
        case SIGNALING_CLIENT_STATE_NEW:
            *ppStateStr = SIGNALING_CLIENT_STATE_NEW_STR;
            break;

        case SIGNALING_CLIENT_STATE_GET_CREDENTIALS:
            *ppStateStr = SIGNALING_CLIENT_STATE_GET_CREDENTIALS_STR;
            break;

        case SIGNALING_CLIENT_STATE_DESCRIBE:
            *ppStateStr = SIGNALING_CLIENT_STATE_DESCRIBE_STR;
            break;

        case SIGNALING_CLIENT_STATE_CREATE:
            *ppStateStr = SIGNALING_CLIENT_STATE_CREATE_STR;
            break;

        case SIGNALING_CLIENT_STATE_GET_ENDPOINT:
            *ppStateStr = SIGNALING_CLIENT_STATE_GET_ENDPOINT_STR;
            break;

        case SIGNALING_CLIENT_STATE_GET_ICE_CONFIG:
            *ppStateStr = SIGNALING_CLIENT_STATE_GET_ICE_CONFIG_STR;
            break;

        case SIGNALING_CLIENT_STATE_READY:
            *ppStateStr = SIGNALING_CLIENT_STATE_READY_STR;
            break;

        case SIGNALING_CLIENT_STATE_CONNECTING:
            *ppStateStr = SIGNALING_CLIENT_STATE_CONNECTING_STR;
            break;

        case SIGNALING_CLIENT_STATE_CONNECTED:
            *ppStateStr = SIGNALING_CLIENT_STATE_CONNECTED_STR;
            break;

        case SIGNALING_CLIENT_STATE_DISCONNECTED:
            *ppStateStr = SIGNALING_CLIENT_STATE_DISCONNECTED_STR;
            break;

        case SIGNALING_CLIENT_STATE_DELETE:
            *ppStateStr = SIGNALING_CLIENT_STATE_DELETE_STR;
            break;

        case SIGNALING_CLIENT_STATE_DELETED:
            *ppStateStr = SIGNALING_CLIENT_STATE_DELETED_STR;
            break;
        case SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA:
            *ppStateStr = SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA_STR;
            break;
        case SIGNALING_CLIENT_STATE_JOIN_SESSION:
            *ppStateStr = SIGNALING_CLIENT_STATE_JOIN_SESSION_STR;
            break;
        case SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING:
            *ppStateStr = SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING_STR;
            break;
        case SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED:
            *ppStateStr = SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED_STR;
            break;
        case SIGNALING_CLIENT_STATE_MAX_VALUE:
        case SIGNALING_CLIENT_STATE_UNKNOWN:
            // Explicit fall-through
        default:
            *ppStateStr = SIGNALING_CLIENT_STATE_UNKNOWN_STR;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS signalingClientGetMetrics(SIGNALING_CLIENT_HANDLE signalingClientHandle, PSignalingClientMetrics pSignalingClientMetrics)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingClientHandle);
    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    DLOGV("Signaling Client Get Metrics");

    CHK_STATUS(signalingGetMetrics(pSignalingClient, pSignalingClientMetrics));

CleanUp:
    if (pSignalingClient != NULL) {
        SIGNALING_UPDATE_ERROR_COUNT(pSignalingClient, retStatus);
    }
    LEAVES();
    return retStatus;
}
