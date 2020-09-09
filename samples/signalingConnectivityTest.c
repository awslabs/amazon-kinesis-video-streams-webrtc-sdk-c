#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

typedef struct {
    volatile ATOMIC_BOOL response_received;
    MUTEX mutex;
    CVAR conditionVariable;
} TestData, *PTestData;

STATUS signalingClientErrorMaster(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{

    printf("Signaling master generated an error 0x%08x - '%.*s'\n", status, msgLen, msg);
    return STATUS_SUCCESS;
}

STATUS signalingClientErrorViewer(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    printf("Signaling viewer generated an error 0x%08x - '%.*s'\n", status, msgLen, msg);
    return STATUS_SUCCESS;
}

STATUS signalingMessageReceivedMaster(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    PSIGNALING_CLIENT_HANDLE pMasterSignalingClientHandle = (PSIGNALING_CLIENT_HANDLE) customData;
    SignalingMessage message;
    STATUS retStatus = STATUS_SUCCESS;

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    STRCPY(message.peerClientId, (PCHAR) "TEST_VIEWER");
    message.payloadLen = pReceivedSignalingMessage->signalingMessage.payloadLen;
    STRCPY(message.payload, pReceivedSignalingMessage->signalingMessage.payload);
    message.correlationId[0] = '\0';
    CHK_STATUS(signalingClientSendMessageSync(*pMasterSignalingClientHandle, &message));

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS signalingMessageReceivedViewer(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    PTestData pData = (PTestData) customData;
    ATOMIC_STORE_BOOL(&pData->response_received, TRUE);
    CVAR_SIGNAL(pData->conditionVariable);
    return STATUS_SUCCESS;
}

INT32 main(INT32 argc, CHAR* argv[])
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pAccessKey, pSecretKey, pSessionToken = NULL;
    ChannelInfo channelInfo;
    PAwsCredentialProvider pCredentialProvider;
    SIGNALING_CLIENT_HANDLE masterSignalingClientHandle, viewerSignalingClientHandle;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    TestData data;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));

    CHK_ERR((pAccessKey = getenv(ACCESS_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_ACCESS_KEY_ID must be set");
    CHK_ERR((pSecretKey = getenv(SECRET_KEY_ENV_VAR)) != NULL, STATUS_INVALID_OPERATION, "AWS_SECRET_ACCESS_KEY must be set");

    if ((channelInfo.pRegion = getenv(DEFAULT_REGION_ENV_VAR)) == NULL) {
        channelInfo.pRegion = DEFAULT_AWS_REGION;
    }

    CHK_STATUS(
        createStaticCredentialProvider(pAccessKey, 0, pSecretKey, 0, pSessionToken, 0, MAX_UINT64, &pCredentialProvider));

    data.mutex = MUTEX_CREATE(FALSE);
    data.conditionVariable = CVAR_CREATE();
    data.response_received = FALSE;

    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = (PCHAR) "foo40";
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_FILE;
    channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    channelInfo.asyncIceServerConfig = TRUE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = KVS_CA_CERT_PATH;
    channelInfo.messageTtl = 0; // Default is 60 seconds

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.errorReportFn = signalingClientErrorMaster;
    signalingClientCallbacks.stateChangeFn = NULL;
    signalingClientCallbacks.messageReceivedFn = signalingMessageReceivedMaster;
    signalingClientCallbacks.customData = (UINT64) &masterSignalingClientHandle;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_DEBUG;
    sprintf(clientInfo.clientId, "%s", "TEST_MASTER");

    initKvsWebRtc();

    CHK_STATUS(createSignalingClientSync(&clientInfo, &channelInfo,
                                          &signalingClientCallbacks, pCredentialProvider,
                                          &masterSignalingClientHandle));

    sprintf(clientInfo.clientId, "%s", "TEST_VIEWER");
    signalingClientCallbacks.errorReportFn = signalingClientErrorViewer;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    signalingClientCallbacks.messageReceivedFn = signalingMessageReceivedViewer;
    signalingClientCallbacks.customData = (UINT64) &data;

    CHK_STATUS(createSignalingClientSync(&clientInfo, &channelInfo,
                                         &signalingClientCallbacks, pCredentialProvider,
                                         &viewerSignalingClientHandle));

    CHK_STATUS(signalingClientConnectSync(masterSignalingClientHandle));
    CHK_STATUS(signalingClientConnectSync(viewerSignalingClientHandle));

    MUTEX_LOCK(data.mutex);

    SignalingMessage message;
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    STRCPY(message.peerClientId, (PCHAR) "TEST_MASTER");
    message.payloadLen = (UINT32) STRLEN("test_message");
    STRCPY(message.payload, (PCHAR) "test_message");
    message.correlationId[0] = '\0';

    UINT64 index = 0;
    UINT64 messageSentTime = 0;
    BOOL response_received = FALSE;
    while(TRUE) {
        messageSentTime = GETTIME();
        DLOGD("send meesage %" PRIu64 , index);
        CHK_STATUS(signalingClientSendMessageSync(viewerSignalingClientHandle, &message));
        while(!response_received) {
            CVAR_WAIT(data.conditionVariable, data.mutex, 60 * HUNDREDS_OF_NANOS_IN_A_SECOND);
            response_received = ATOMIC_LOAD_BOOL(&data.response_received);
            ATOMIC_STORE_BOOL(&data.response_received, FALSE);
            if (!response_received) {
                DLOGD("response %" PRIu64 " not received after one minute\n", index);
                break;
            }
        }

        if (response_received) {
            UINT64 currentTime = GETTIME();
            UINT64 delay = (currentTime - messageSentTime) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
            DLOGD("delay for message %" PRIu64 " %" PRIu64 ", %" PRIu64 "\n", index, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, delay);
        }
        THREAD_SLEEP(60 * HUNDREDS_OF_NANOS_IN_A_SECOND);
        index++;
    }

    MUTEX_UNLOCK(data.mutex);

CleanUp:
    CHK_LOG_ERR(retStatus);

    return 0;
}