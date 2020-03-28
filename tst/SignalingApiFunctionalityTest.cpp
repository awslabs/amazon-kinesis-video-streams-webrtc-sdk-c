#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class SignalingApiFunctionalityTest : public WebRtcClientTestBase {
public:
    SignalingApiFunctionalityTest() : pActiveClient(NULL)
    {
        MEMSET(signalingStatesCounts, 0x00, SIZEOF(signalingStatesCounts));

        getIceConfigCount = 0;
        getIceConfigFail = MAX_UINT32;
        getIceConfigRecover = 0;
        getIceConfigResult = STATUS_SUCCESS;

        errStatus = STATUS_SUCCESS;
        errMsg[0] = '\0';
    };

    PSignalingClient pActiveClient;
    UINT32 getIceConfigFail;
    UINT32 getIceConfigRecover;
    UINT32 getIceConfigCount;
    UINT64 getIceConfigResult;
    UINT32 signalingStatesCounts[SIGNALING_CLIENT_STATE_MAX_VALUE];
    STATUS errStatus;
    CHAR errMsg[1024];
};

STATUS masterMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    SignalingApiFunctionalityTest *pTest = (SignalingApiFunctionalityTest*) customData;

    DLOGI("Message received:\ntype: %u\npeer client id: %s\npayload len: %u\npayload: %s\nCorrelationId: %s\nErrorType: %s\nStatusCode: %u\nDescription: %s",
          pReceivedSignalingMessage->signalingMessage.messageType,
          pReceivedSignalingMessage->signalingMessage.peerClientId,
          pReceivedSignalingMessage->signalingMessage.payloadLen,
          pReceivedSignalingMessage->signalingMessage.payload,
          pReceivedSignalingMessage->signalingMessage.correlationId,
          pReceivedSignalingMessage->errorType,
          pReceivedSignalingMessage->statusCode,
          pReceivedSignalingMessage->description);

    // Responding
    STATUS status;
    SignalingMessage message;
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRCPY(message.peerClientId, TEST_SIGNALING_VIEWER_CLIENT_ID);
    MEMSET(message.payload, 'B', 200);
    message.payload[200] = '\0';
    message.payloadLen = 0;

    status = signalingSendMessageSync(pTest->pActiveClient, &message);
    CHK_LOG_ERR(status, "Master failed to send an answer with 0x%08x", status);

    // Return success to continue
    return STATUS_SUCCESS;
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    SignalingApiFunctionalityTest *pTest = (SignalingApiFunctionalityTest*) customData;

    PCHAR pStateStr;
    signalingClientGetStateString(state, &pStateStr);
    DLOGD("Signaling client state changed to %d - '%s'", state, pStateStr);

    pTest->signalingStatesCounts[state]++;

    return STATUS_SUCCESS;
}

STATUS signalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    SignalingApiFunctionalityTest *pTest = (SignalingApiFunctionalityTest*) customData;

    pTest->errStatus = status;
    STRNCPY(pTest->errMsg, msg, msgLen);

    DLOGD("Signaling client generated an error 0x%08x - '%.*s'", status, msgLen, msg);

    return STATUS_SUCCESS;
}

STATUS viewerMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);

    DLOGI("Message received:\ntype: %u\npeer client id: %s\npayload len: %u\npayload: %s\nCorrelationId: %s\nErrorType: %s\nStatusCode: %u\nDescription: %s",
          pReceivedSignalingMessage->signalingMessage.messageType,
          pReceivedSignalingMessage->signalingMessage.peerClientId,
          pReceivedSignalingMessage->signalingMessage.payloadLen,
          pReceivedSignalingMessage->signalingMessage.payload,
          pReceivedSignalingMessage->signalingMessage.correlationId,
          pReceivedSignalingMessage->errorType,
          pReceivedSignalingMessage->statusCode,
          pReceivedSignalingMessage->description);

    // Return success to continue
    return STATUS_SUCCESS;
}

STATUS getIceConfigPreHook(UINT64 state, UINT64 hookCustomData)
{
    UNUSED_PARAM(state);
    STATUS retStatus = STATUS_SUCCESS;
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->getIceConfigCount >= pTest->getIceConfigFail && pTest->getIceConfigCount < pTest->getIceConfigRecover) {
        retStatus = pTest->getIceConfigResult;
    }

    pTest->getIceConfigCount++;
    DLOGD("Signaling client getIceConfigPreHook returning 0x%08x", retStatus);
    return retStatus;
}

////////////////////////////////////////////////////////////////////
// Functionality Tests
////////////////////////////////////////////////////////////////////
TEST_F(SignalingApiFunctionalityTest, basicCreateConnectFree)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    EXPECT_EQ(STATUS_SUCCESS, createSignalingClientSync(&clientInfo,
                                                        &channelInfo,
                                                        &signalingClientCallbacks,
                                                        (PAwsCredentialProvider) mTestCredentialProvider,
                                                        &signalingHandle));

    // Connect twice - the second time will be no-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, mockMaster)
{
    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    Tag tags[3];
    STATUS expectedStatus;

    tags[0].version = TAG_CURRENT_VERSION;
    tags[0].name = (PCHAR) "Tag Name 0";
    tags[0].value = (PCHAR) "Tag Value 0";
    tags[1].version = TAG_CURRENT_VERSION;
    tags[1].name = (PCHAR) "Tag Name 1";
    tags[1].value = (PCHAR) "Tag Value 1";
    tags[2].version = TAG_CURRENT_VERSION;
    tags[2].name = (PCHAR) "Tag Name 2";
    tags[2].value = (PCHAR) "Tag Value 2";

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = masterMessageReceived;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, NULL, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, NULL,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo,
            &signalingClientCallbacks, NULL, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo,
            &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, NULL));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, NULL, NULL, NULL, NULL));

    // Without the tags
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    if (mAccessKeyIdSet) {
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    }

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(signalingHandle));

    // Write something
    SignalingMessage message;
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    STRCPY(message.peerClientId, TEST_SIGNALING_VIEWER_CLIENT_ID);
    message.payloadLen = 0;
    message.correlationId[0] = '\0';
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    MEMSET(message.payload, 'B', MAX_SIGNALING_MESSAGE_LEN);
    message.payload[MAX_SIGNALING_MESSAGE_LEN] = '\0';

    // Expect the bloat to be over the max size
    expectedStatus = mAccessKeyIdSet ? STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus,signalingClientSendMessageSync(signalingHandle, &message));

    // Something reasonable
    message.payload[MAX_SIGNALING_MESSAGE_LEN / 2] = '\0';
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    // Repeat the same message with no correlation id
    STRCPY(message.correlationId, SIGNAING_TEST_CORRELATION_ID);
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));
    message.correlationId[0] = '\0';

    // Smaller answer
    message.payload[100] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    // Small answer
    message.payload[1] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    // Empty
    message.payload[0] = '\0';
    expectedStatus = mAccessKeyIdSet ? STATUS_INVALID_ARG_LEN : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    MEMSET(message.payload, 'C', 300);
    message.payload[300] = '\0';
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    DLOGI("Awaiting a little for tests termination");
    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Delete the created channel
    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Free again
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, mockViewer)
{
    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    Tag tags[3];
    STATUS expectedStatus;

    tags[0].version = TAG_CURRENT_VERSION;
    tags[0].name = (PCHAR) "Tag Name 0";
    tags[0].value = (PCHAR) "Tag Value 0";
    tags[1].version = TAG_CURRENT_VERSION;
    tags[1].name = (PCHAR) "Tag Name 1";
    tags[1].value = (PCHAR) "Tag Value 1";
    tags[2].version = TAG_CURRENT_VERSION;
    tags[2].name = (PCHAR) "Tag Name 2";
    tags[2].value = (PCHAR) "Tag Value 2";

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = viewerMessageReceived;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_VIEWER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider,
                                                  &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, NULL, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider,
                                                  &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, NULL,
                                                  (PAwsCredentialProvider) mTestCredentialProvider,
                                                  &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                  NULL, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, NULL));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, NULL, NULL, NULL, NULL));

    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider,
                                                  &signalingHandle));

    if (mAccessKeyIdSet) {
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    }

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(signalingHandle));

    // Write something
    SignalingMessage message;
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(message.payload, 'A', 100);
    message.payload[100] = '\0';
    message.payloadLen = 0;
    message.correlationId[0] = '\0';

    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    MEMSET(message.payload, 'B', 200);
    message.payload[200] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    MEMSET(message.payload, 'B', 300);
    message.payload[300] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Free again
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, invalidChannelInfoInput)
{
    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    Tag tags[MAX_TAG_COUNT + 1];
    CHAR tempBuf[100000];
    PCHAR pStored;
    STATUS retStatus;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = (SignalingClientMessageReceivedFunc) 1;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pChannelArn = (PCHAR) "Channel ARN";
    channelInfo.pRegion = (PCHAR) "us-east-1";
    channelInfo.pControlPlaneUrl = (PCHAR) "Test Control plane URI";
    channelInfo.pCertPath = (PCHAR) "/usr/test";
    channelInfo.pUserAgentPostfix = (PCHAR) "Postfix";
    channelInfo.pCustomUserAgent = (PCHAR) "Agent";
    channelInfo.pKmsKeyId = (PCHAR) "Kms Key ID";
    channelInfo.cachingEndpoint = TRUE;
    channelInfo.endpointCachingPeriod = 10 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    channelInfo.pTags[0].version = TAG_CURRENT_VERSION;
    channelInfo.pTags[0].name = (PCHAR) "Tag Name 0";
    channelInfo.pTags[0].value = (PCHAR) "Tag Value 0";
    channelInfo.pTags[1].version = TAG_CURRENT_VERSION;
    channelInfo.pTags[1].name = (PCHAR) "Tag Name 1";
    channelInfo.pTags[1].value = (PCHAR) "Tag Value 1";
    channelInfo.pTags[2].version = TAG_CURRENT_VERSION;
    channelInfo.pTags[2].name = (PCHAR) "Tag Name 2";
    channelInfo.pTags[2].value = (PCHAR) "Tag Value 2";

    // Very large string to test max
    MEMSET(tempBuf, 'X', SIZEOF(tempBuf));
    tempBuf[ARRAY_SIZE(tempBuf) - 1] = '\0';

    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider,
                                                  &signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Free again
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Invalid version
    channelInfo.version++;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.version--;

    // Client id - IMPORTANT - this will overflow to the next member of the struct which is log level but
    // we will reset it after this particular test.
    MEMSET(clientInfo.clientId, 'a', MAX_SIGNALING_CLIENT_ID_LEN + 1);
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                          (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CLIENT_INFO_CLIENT_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;

    // Max name
    pStored = channelInfo.pChannelName;
    channelInfo.pChannelName = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_NAME_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelName = pStored;

    // Max ARN
    pStored = channelInfo.pChannelArn;
    channelInfo.pChannelArn = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelArn = pStored;

    // Max Region
    pStored = channelInfo.pRegion;
    channelInfo.pRegion = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_REGION_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pRegion = pStored;

    // Max CPL
    pStored = channelInfo.pControlPlaneUrl;
    channelInfo.pControlPlaneUrl = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CPL_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pControlPlaneUrl = pStored;

    // Max cert path
    pStored = channelInfo.pCertPath;
    channelInfo.pCertPath = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CERTIFICATE_PATH_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pCertPath = pStored;

    // Max user agent postfix
    pStored = channelInfo.pUserAgentPostfix;
    channelInfo.pUserAgentPostfix = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pUserAgentPostfix = pStored;

    // Max user agent
    pStored = channelInfo.pCustomUserAgent;
    channelInfo.pCustomUserAgent = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_AGENT_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pCustomUserAgent = pStored;

    // Max user agent
    pStored = channelInfo.pKmsKeyId;
    channelInfo.pKmsKeyId = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_KMS_KEY_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pKmsKeyId = pStored;

    // Max tag count
    channelInfo.tagCount = MAX_TAG_COUNT + 1;
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(
            &clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.tagCount = 3;

    // Max tag name
    pStored = channelInfo.pTags[1].name;
    channelInfo.pTags[1].name = tempBuf;
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(
            &clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].name = pStored;

    // Max tag value
    pStored = channelInfo.pTags[1].value;
    channelInfo.pTags[1].value = tempBuf;
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(
            &clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].value = pStored;

    // Invalid tag version
    channelInfo.pTags[1].version++;
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(
            &clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].version--;

    // Both name and arn are NULL
    channelInfo.pChannelName = channelInfo.pChannelArn = NULL;
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(
            &clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider,
            &signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    channelInfo.pChannelName = mChannelName;
    channelInfo.pChannelArn = (PCHAR) "Channel ARN";

    // Less than min/greater than max message TTL
    channelInfo.messageTtl = MIN_SIGNALING_MESSAGE_TTL_VALUE - 1;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.messageTtl = MAX_SIGNALING_MESSAGE_TTL_VALUE + 1;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                          (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.messageTtl = SIGNALING_DEFAULT_MESSAGE_TTL_VALUE;

    // Reset the params for proper stream creation
    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = viewerMessageReceived;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_VIEWER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

    // channel name validation error - name with spaces
    channelInfo.pChannelName = (PCHAR) "Name With Spaces";
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                          (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    if (mAccessKeyIdSet) {
        EXPECT_TRUE(retStatus == STATUS_OPERATION_TIMED_OUT || retStatus == STATUS_SIGNALING_DESCRIBE_CALL_FAILED);
    } else {
        EXPECT_EQ(STATUS_NULL_ARG, retStatus);
    }

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelName = mChannelName;

    // ClientId Validation error - name with spaces
    clientInfo.clientId[4] = ' ';
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                          (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, retStatus);

        pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
        EXPECT_EQ(SIGNALING_DEFAULT_MESSAGE_TTL_VALUE, pSignalingClient->pChannelInfo->messageTtl);
    } else {
        EXPECT_EQ(STATUS_NULL_ARG, retStatus);
    }

    // Should fail
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceReconnectEmulation)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    DLOGV("Before RECONNECT_ICE_SERVER injection");

    // Inject a reconnect ice server message
    CHAR message[] = "{\n"
                     "    \"senderClientId\": \"TestSender\",\n"
                     "    \"messageType\": \"RECONNECT_ICE_SERVER\",\n"
                     "    \"messagePayload\": \"MessagePayload\",\n"
                     "    \"statusResponse\": {\n"
                     "        \"correlationId\": \"CorrelationID\",\n"
                     "        \"errorType\": \"Reconnect ice server\",\n"
                     "        \"statusCode\": \"200\",\n"
                     "        \"description\": \"Test attempt to reconnect ice server\"\n"
                     "    }\n"
                     "}";
    EXPECT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, message, ARRAY_SIZE(message)));

    DLOGV("After RECONNECT_ICE_SERVER injection");

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceRefreshEmulation)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
            (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Make sure we get ICE candidate refresh before connecting
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Wait for ICE refresh while connected
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceRefreshEmulationAuthExpiration)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    // Force auth token refresh right after the main API calls
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = GETTIME() + 4 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Make sure we get ICE candidate refresh before connecting
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Reset it back right after the GetIce  is called already
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = MAX_UINT64;

    // Check the states - we should have failed on get credentials
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(12, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Shouldn't be able to connect as it's not in ready state
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceRefreshEmulationWithFaultInjectionNoDisconnect)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_INVALID_OPERATION;
    getIceConfigFail = 1;
    getIceConfigRecover = 3;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Wait for ICE refresh while connected
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceRefreshEmulationWithFaultInjectionAuthErrorNoDisconnect)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_SERVICE_CALL_NOT_AUTHORIZED_ERROR;
    getIceConfigFail = 1;
    getIceConfigRecover = 3;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Wait for ICE refresh while connected
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceRefreshEmulationWithFaultInjectionErrorDisconnect)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Cause a bad auth
    BYTE firstByte = pSignalingClient->pAwsCredentials->secretKey[0];
    pSignalingClient->pAwsCredentials->secretKey[0] = 'A';

    // Wait for ICE refresh while connected
    THREAD_SLEEP(6 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Reset it back to cause normal execution
    pSignalingClient->pAwsCredentials->secretKey[0] = firstByte;

    THREAD_SLEEP(3 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LE(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, goAwayEmulation)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                        (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    DLOGV("Before GO_AWAY injection");

    // Inject a reconnect ice server message
    CHAR message[] = "{\n"
                     "    \"senderClientId\": \"TestSender\",\n"
                     "    \"messageType\": \"GO_AWAY\",\n"
                     "    \"messagePayload\": \"MessagePayload\",\n"
                     "    \"statusResponse\": {\n"
                     "        \"correlationId\": \"CorrelationID\",\n"
                     "        \"errorType\": \"Go away message\",\n"
                     "        \"statusCode\": \"200\",\n"
                     "        \"description\": \"Test attempt to send GO_AWAY message\"\n"
                     "    }\n"
                     "}";
    EXPECT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, message, ARRAY_SIZE(message)));

    DLOGV("After GO_AWAY injection");

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, unknownMessageTypeEmulation)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                                                        (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    DLOGV("Before Unknown message type injection");

    // Inject a reconnect ice server message
    CHAR message[] = "{\n"
                     "    \"senderClientId\": \"TestSender\",\n"
                     "    \"messageType\": \"UNKNOWN_TYPE\",\n"
                     "    \"messagePayload\": \"MessagePayload\",\n"
                     "    \"statusResponse\": {\n"
                     "        \"correlationId\": \"CorrelationID\",\n"
                     "        \"errorType\": \"Unknown message\",\n"
                     "        \"statusCode\": \"200\",\n"
                     "        \"description\": \"Test attempt to send an unknown message\"\n"
                     "    }\n"
                     "}";
    EXPECT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, message, ARRAY_SIZE(message)));

    DLOGV("After Unknown message type injection");

    // Sleep to allow the background thread to send a message on receive
    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, connectTimeoutEmulation)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 0;
    clientInfoInternal.connectTimeout = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client - should time out
    EXPECT_EQ(STATUS_OPERATION_TIMED_OUT, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client - should connect OK
    pSignalingClient->clientInfo.connectTimeout = 0;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, channelInfoArnSkipDescribe)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    CHAR testArn[MAX_ARN_LEN + 1];

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.iceRefreshPeriod = 0;
    clientInfoInternal.connectTimeout = 0;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client - should connect OK
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);

    // Check that we are connected and can send a message
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    //
    // Store the ARN, free the client, repeat the same with the ARN, ensure we don't hit the describe and create states
    //
    STRCPY(testArn, pSignalingClient->channelDescription.channelArn);

    // Free the client, reset the states count
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    MEMSET(signalingStatesCounts, 0x00, SIZEOF(signalingStatesCounts));

    DLOGD("Attempting to create a signaling client for an existing channel %s with channel ARN %s", channelInfo.pChannelName, testArn);

    // Create channel with ARN and without name
    channelInfo.pChannelName = NULL;
    channelInfo.pChannelArn = testArn;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks,
                                                  (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client - should connect OK
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);

    // Check that we are connected and can send a message
    MEMSET(signalingMessage.payload, 'B', 50);
    signalingMessage.payload[50] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

}
}
}
}
}
