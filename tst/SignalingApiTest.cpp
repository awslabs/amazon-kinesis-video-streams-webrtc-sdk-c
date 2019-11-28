#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class SignalingApiTest : public WebRtcClientTestBase {
public:
    SignalingApiTest() : pActiveClient(NULL)
    {};

    PSignalingClient pActiveClient;
};

STATUS masterMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    SignalingApiTest *pTest = (SignalingApiTest*) customData;

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
    SignalingApiTest *pTest = (SignalingApiTest*) customData;

    DLOGV("Signaling client state changed to %d", state);

    return STATUS_SUCCESS;
}

STATUS viewerMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    SignalingApiTest *pTest = (SignalingApiTest*) customData;
    STATUS status;

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

TEST_F(SignalingApiTest, mockMaster)
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
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

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

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Free again
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
}

TEST_F(SignalingApiTest, mockViewer)
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
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_VIEWER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

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

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Free again
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
}

TEST_F(SignalingApiTest, invalidChannelInfoInput)
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
    signalingClientCallbacks.customData = 0;
    signalingClientCallbacks.messageReceivedFn = (SignalingClientMessageReceivedFunc) 1;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
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
}

TEST_F(SignalingApiTest, iceReconnectEmulation)
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
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

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

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiTest, goAwayEmulation)
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
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

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

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiTest, unknownMessageTypeEmulation)
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
    signalingClientCallbacks.errorReportFn = NULL;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = TEST_SIGNALING_CHANNEL_NAME;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingEndpoint = FALSE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

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
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

}
}
}
}
}
