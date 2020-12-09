#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SignalingApiFunctionalityTest : public WebRtcClientTestBase {
  public:
    SignalingApiFunctionalityTest() : pActiveClient(NULL)
    {
        MEMSET(signalingStatesCounts, 0x00, SIZEOF(signalingStatesCounts));

        getIceConfigCount = 0;
        getIceConfigFail = MAX_UINT32;
        getIceConfigRecover = 0;
        getIceConfigResult = STATUS_SUCCESS;

        connectCount = 0;
        connectFail = MAX_UINT32;
        connectRecover = 0;
        connectResult = STATUS_SUCCESS;

        describeCount = 0;
        describeFail = MAX_UINT32;
        describeRecover = 0;
        describeResult = STATUS_SUCCESS;

        getEndpointCount = 0;
        getEndpointFail = MAX_UINT32;
        getEndpointRecover = 0;
        getEndpointResult = STATUS_SUCCESS;

        errStatus = STATUS_SUCCESS;
        errMsg[0] = '\0';
    };

    PSignalingClient pActiveClient;
    UINT32 getIceConfigFail;
    UINT32 getIceConfigRecover;
    UINT32 getIceConfigCount;
    STATUS getIceConfigResult;
    UINT32 signalingStatesCounts[SIGNALING_CLIENT_STATE_MAX_VALUE];
    STATUS errStatus;
    CHAR errMsg[1024];

    STATUS connectResult;
    UINT32 connectFail;
    UINT32 connectRecover;
    UINT32 connectCount;

    STATUS describeResult;
    UINT32 describeFail;
    UINT32 describeRecover;
    UINT32 describeCount;

    STATUS getEndpointResult;
    UINT32 getEndpointFail;
    UINT32 getEndpointRecover;
    UINT32 getEndpointCount;
};

STATUS masterMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) customData;

    DLOGI("Message received:\ntype: %u\npeer client id: %s\npayload len: %u\npayload: %s\nCorrelationId: %s\nErrorType: %s\nStatusCode: "
          "%u\nDescription: %s",
          pReceivedSignalingMessage->signalingMessage.messageType, pReceivedSignalingMessage->signalingMessage.peerClientId,
          pReceivedSignalingMessage->signalingMessage.payloadLen, pReceivedSignalingMessage->signalingMessage.payload,
          pReceivedSignalingMessage->signalingMessage.correlationId, pReceivedSignalingMessage->errorType, pReceivedSignalingMessage->statusCode,
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
    CHK_LOG_ERR(status);

    // Return success to continue
    return STATUS_SUCCESS;
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) customData;

    PCHAR pStateStr;
    signalingClientGetStateString(state, &pStateStr);
    DLOGD("Signaling client state changed to %d - '%s'", state, pStateStr);

    pTest->signalingStatesCounts[state]++;

    return STATUS_SUCCESS;
}

STATUS signalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) customData;

    pTest->errStatus = status;
    STRNCPY(pTest->errMsg, msg, msgLen);

    DLOGD("Signaling client generated an error 0x%08x - '%.*s'", status, msgLen, msg);

    return STATUS_SUCCESS;
}

STATUS viewerMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);

    DLOGI("Message received:\ntype: %u\npeer client id: %s\npayload len: %u\npayload: %s\nCorrelationId: %s\nErrorType: %s\nStatusCode: "
          "%u\nDescription: %s",
          pReceivedSignalingMessage->signalingMessage.messageType, pReceivedSignalingMessage->signalingMessage.peerClientId,
          pReceivedSignalingMessage->signalingMessage.payloadLen, pReceivedSignalingMessage->signalingMessage.payload,
          pReceivedSignalingMessage->signalingMessage.correlationId, pReceivedSignalingMessage->errorType, pReceivedSignalingMessage->statusCode,
          pReceivedSignalingMessage->description);

    // Return success to continue
    return STATUS_SUCCESS;
}

STATUS getIceConfigPreHook(UINT64 hookCustomData)
{
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

STATUS connectPreHook(UINT64 hookCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->connectCount >= pTest->connectFail && pTest->connectCount < pTest->connectRecover) {
        retStatus = pTest->connectResult;
    }

    pTest->connectCount++;
    DLOGD("Signaling client connect hook returning 0x%08x", retStatus);
    return retStatus;
};

STATUS describePreHook(UINT64 hookCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->describeCount >= pTest->describeFail && pTest->describeCount < pTest->describeRecover) {
        retStatus = pTest->describeResult;
    }

    pTest->describeCount++;
    DLOGD("Signaling client describe hook returning 0x%08x", retStatus);
    return retStatus;
};

STATUS getEndpointPreHook(UINT64 hookCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->getEndpointCount >= pTest->getEndpointFail && pTest->getEndpointCount < pTest->getEndpointRecover) {
        retStatus = pTest->getEndpointResult;
    }

    pTest->getEndpointCount++;
    DLOGD("Signaling client get endpoint hook returning 0x%08x", retStatus);
    return retStatus;
};

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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(NULL, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(&clientInfo, NULL, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, NULL, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, NULL, &signalingHandle));
    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, NULL));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, NULL, NULL, NULL, NULL));

    // Without the tags
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
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
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(signalingHandle, &message));

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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_VIEWER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 3;
    channelInfo.pTags = tags;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(NULL, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(&clientInfo, NULL, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, NULL, (PAwsCredentialProvider) mTestCredentialProvider, &signalingHandle));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, NULL, &signalingHandle));
    EXPECT_NE(
        STATUS_SUCCESS,
        createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, NULL));
    EXPECT_NE(STATUS_SUCCESS, createSignalingClientSync(NULL, NULL, NULL, NULL, NULL));

    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
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
    clientInfo.cacheFilePath = NULL;

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
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
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

    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
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
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.version--;

    // Client id - IMPORTANT - this will overflow to the next member of the struct which is log level but
    // we will reset it after this particular test.
    MEMSET(clientInfo.clientId, 'a', MAX_SIGNALING_CLIENT_ID_LEN + 1);
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CLIENT_INFO_CLIENT_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfo.loggingLevel = LOG_LEVEL_VERBOSE;

    // Max name
    pStored = channelInfo.pChannelName;
    channelInfo.pChannelName = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_NAME_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelName = pStored;

    // Max ARN
    pStored = channelInfo.pChannelArn;
    channelInfo.pChannelArn = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelArn = pStored;

    // Max Region
    pStored = channelInfo.pRegion;
    channelInfo.pRegion = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_REGION_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pRegion = pStored;

    // Max CPL
    pStored = channelInfo.pControlPlaneUrl;
    channelInfo.pControlPlaneUrl = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CPL_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pControlPlaneUrl = pStored;

    // Max cert path
    pStored = channelInfo.pCertPath;
    channelInfo.pCertPath = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_CERTIFICATE_PATH_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pCertPath = pStored;

    // Max user agent postfix
    pStored = channelInfo.pUserAgentPostfix;
    channelInfo.pUserAgentPostfix = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pUserAgentPostfix = pStored;

    // Max user agent
    pStored = channelInfo.pCustomUserAgent;
    channelInfo.pCustomUserAgent = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_AGENT_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pCustomUserAgent = pStored;

    // Max user agent
    pStored = channelInfo.pKmsKeyId;
    channelInfo.pKmsKeyId = tempBuf;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_KMS_KEY_LENGTH : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pKmsKeyId = pStored;

    // Max tag count
    channelInfo.tagCount = MAX_TAG_COUNT + 1;
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.tagCount = 3;

    // Max tag name
    pStored = channelInfo.pTags[1].name;
    channelInfo.pTags[1].name = tempBuf;
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].name = pStored;

    // Max tag value
    pStored = channelInfo.pTags[1].value;
    channelInfo.pTags[1].value = tempBuf;
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].value = pStored;

    // Invalid tag version
    channelInfo.pTags[1].version++;
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pTags[1].version--;

    // Both name and arn are NULL
    channelInfo.pChannelName = channelInfo.pChannelArn = NULL;
    EXPECT_NE(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    EXPECT_FALSE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    channelInfo.pChannelName = mChannelName;
    channelInfo.pChannelArn = (PCHAR) "Channel ARN";

    // Less than min/greater than max message TTL
    channelInfo.messageTtl = MIN_SIGNALING_MESSAGE_TTL_VALUE - 1;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    EXPECT_EQ(mAccessKeyIdSet ? STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE : STATUS_NULL_ARG, retStatus);
    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.messageTtl = MAX_SIGNALING_MESSAGE_TTL_VALUE + 1;
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
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
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;

    // channel name validation error - name with spaces
    channelInfo.pChannelName = (PCHAR) "Name With Spaces";
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
    if (mAccessKeyIdSet) {
        EXPECT_TRUE(retStatus == STATUS_OPERATION_TIMED_OUT || retStatus == STATUS_SIGNALING_DESCRIBE_CALL_FAILED);
    } else {
        EXPECT_EQ(STATUS_NULL_ARG, retStatus);
    }

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    channelInfo.pChannelName = mChannelName;

    // ClientId Validation error - name with spaces
    clientInfo.clientId[4] = ' ';
    retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                          &signalingHandle);
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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshNotConnectedVariations)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 i, iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    // Set the ICE hook
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_INVALID_OPERATION;
    getIceConfigFail = 10000; // large enough to not cause any failures
    getIceConfigRecover = 3000000;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;


    //
    // Normal case after the signaling client creation.
    // Validate the count and retrieve the configs
    // Ensure no API call is made
    //

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

    // The ICE api should have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);
    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
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


    //
    // Setting the count of the ice configs to 0 to trigger the refresh on get count
    //

    // Make the count 0 to trigger the ICE refresh
    pSignalingClient->iceConfigCount = 0;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called again
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
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


    //
    // Setting the count of the ice configs to 0 to trigger the refresh on get ICE info
    //

    // Make the count 0 to trigger the ICE refresh
    pSignalingClient->iceConfigCount = 0;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Attempt to get the 0th element which would trigger the refresh
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);


    //
    // Setting the expiration to trigger the refresh on get count
    //

    // Set the expired time to trigger the refresh
    pSignalingClient->iceConfigExpiration = GETTIME() - 1;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called again
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    //
    // Setting the expiration to trigger the refresh on get ICE info
    //

    // Set the expired time to trigger the refresh
    pSignalingClient->iceConfigExpiration = GETTIME() - 1;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Attempt to get the 0th element which would trigger the refresh
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    //
    // Attempt to send a message which should fail as we are not connected
    //
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_INVALID_STREAM_STATE, signalingClientSendMessageSync(signalingHandle, &signalingMessage));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshConnectedVariations)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 i, iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    // Set the ICE hook
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_INVALID_OPERATION;
    getIceConfigFail = 10000; // large enough to not cause any failures
    getIceConfigRecover = 3000000;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));


    //
    // Normal case after the signaling client creation.
    // Validate the count and retrieve the configs
    // Ensure no API call is made
    //

    // Check the states first
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

    // The ICE api should have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);
    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
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


    //
    // Setting the count of the ice configs to 0 to trigger the refresh on get count
    //

    // Make the count 0 to trigger the ICE refresh
    pSignalingClient->iceConfigCount = 0;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called again
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);


    //
    // Setting the count of the ice configs to 0 to trigger the refresh on get ICE info
    //

    // Make the count 0 to trigger the ICE refresh
    pSignalingClient->iceConfigCount = 0;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Attempt to get the 0th element which would trigger the refresh
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);


    //
    // Setting the expiration to trigger the refresh on get count
    //

    // Set the expired time to trigger the refresh
    pSignalingClient->iceConfigExpiration = GETTIME() - 1;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
    }

    // Make sure no APIs have been called again
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    //
    // Setting the expiration to trigger the refresh on get ICE info
    //

    // Set the expired time to trigger the refresh
    pSignalingClient->iceConfigExpiration = GETTIME() - 1;

    // Zero the ICE call count
    getIceConfigCount = 0;

    // Attempt to get the 0th element which would trigger the refresh
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Make sure the API has been called
    EXPECT_EQ(1, getIceConfigCount);

    // Ensure we can get the ICE configurations
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(0, iceCount);

    // Make sure no APIs have been called
    EXPECT_EQ(1, getIceConfigCount);

    // Other state transacted
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(5, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    //
    // Attempt to send a message which should succeed as we are connected
    //
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshNotConnectedAuthExpiration)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    // Force auth token refresh right after the main API calls
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = GETTIME() + 4 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
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

    // Make sure the credentials expire
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Initially, calling the API should succeed as the ICE configuration is already retrieved
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

    // Resetting to trigger the ice refresh
    pSignalingClient->iceConfigCount = 0;

    // Calling get ICE server count should trigger ICE refresh which should fail
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Reset it back right after the GetIce is called already
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = MAX_UINT64;

    // Check the states - we should have failed on get credentials
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(23, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Attempt to retrieve the ice configuration should succeed
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(23, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // We should be in the ready state so connecting should be OK
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    // Ensure we had failed the ICE config
    EXPECT_EQ(STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED, errStatus);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshConnectedAuthExpiration)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    // Force auth token refresh right after the main API calls
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = GETTIME() + 4 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Connect the client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
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

    // Make sure the credentials expire
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Initially, calling the API should succeed as the ICE configuration is already retrieved
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

    // Resetting to trigger the ice refresh
    pSignalingClient->iceConfigCount = 0;

    // Calling get ICE server count should trigger ICE refresh which should fail
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Reset it back right after the GetIce is called already
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = MAX_UINT64;

    // Check the states - we should have failed on get credentials
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(23, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Attempt to retrieve the ice configuration should succeed
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(23, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // We should have already been connected. This should be a No-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    // Ensure we had failed the ICE config
    EXPECT_EQ(STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED, errStatus);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshNotConnectedWithFaultInjectionRecovered)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
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
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
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

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshConnectedWithFaultInjectionRecovered)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
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
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
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

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_NE((UINT64) NULL, (UINT64) pIceConfigInfo);

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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshNotConnectedWithFaultInjectionNotRecovered)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_INVALID_OPERATION;
    getIceConfigFail = 1;
    getIceConfigRecover = 1000000;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
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

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshConnectedWithFaultInjectionNot1669)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    // Make it fail after the first call and recover after two failures on the 3rd call
    getIceConfigResult = STATUS_INVALID_OPERATION;
    getIceConfigFail = 1;
    getIceConfigRecover = 1000000;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Connect first
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states first
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

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Connect to the signaling client - no-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(4, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshNotConnectedWithBadAuth)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
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

    // Set bad auth info
    BYTE firstByte = pSignalingClient->pAwsCredentials->secretKey[0];
    pSignalingClient->pAwsCredentials->secretKey[0] = 'A';

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Reset the auth and ensure we succeed this time
    pSignalingClient->pAwsCredentials->secretKey[0] = firstByte;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
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

TEST_F(SignalingApiFunctionalityTest, iceServerConfigRefreshConnectedWithBadAuth)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Connect to the channel
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
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

    // Set bad auth info
    BYTE firstByte = pSignalingClient->pAwsCredentials->secretKey[0];
    pSignalingClient->pAwsCredentials->secretKey[0] = 'A';

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Reset the auth and ensure we succeed this time
    pSignalingClient->pAwsCredentials->secretKey[0] = firstByte;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Connect to the signaling client = no-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
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
    clientInfo.cacheFilePath = NULL;
    STRCPY(clientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
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
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.connectTimeout = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.connectTimeout = 0;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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

TEST_F(SignalingApiFunctionalityTest, deleteChannelCreatedWithArn)
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
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.connectTimeout = 0;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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

    EXPECT_EQ(STATUS_SUCCESS, signalingClientDeleteSync(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, deleteChannelCreatedAuthExpiration)
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
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.connectTimeout = 0;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    // Force auth token refresh right after the main API calls
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = GETTIME() + 4 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
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
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETE]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETED]);

    // Force the auth error on the delete API call
    THREAD_SLEEP(7 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_NE(STATUS_SUCCESS, signalingClientDeleteSync(signalingHandle));

    // Check the states - we should have failed on get credentials
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(12, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETE]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETED]);

    // Reset it back right after the GetIce  is called already
    ((PStaticCredentialProvider) mTestCredentialProvider)->pAwsCredentials->expiration = MAX_UINT64;

    // Should succeed properly
    EXPECT_EQ(STATUS_SUCCESS, signalingClientDeleteSync(signalingHandle));

    // Check the states - we should have got the credentials now and directly moved to delete
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(13, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DELETED]);

    // Shouldn't be able to connect as it's not in ready state
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, signalingClientDisconnectSyncVariations)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    initializeSignalingClient();

    EXPECT_EQ(STATUS_SUCCESS, signalingClientDisconnectSync(mSignalingClientHandle));

    // Connect and disconnect
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientDisconnectSync(mSignalingClientHandle));

    // Retry
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientDisconnectSync(mSignalingClientHandle));

    // Can't send a message
    SignalingMessage message;
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRCPY(message.peerClientId, TEST_SIGNALING_VIEWER_CLIENT_ID);
    MEMSET(message.payload, 'A', 200);
    message.payload[200] = '\0';
    message.payloadLen = 0;
    message.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_INVALID_STREAM_STATE, signalingClientSendMessageSync(mSignalingClientHandle, &message));

    // Get ICE info is OK
    UINT32 count;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &count));

    // State should be in Ready state
    SIGNALING_CLIENT_STATE state;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetCurrentState(mSignalingClientHandle, &state));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_READY, state);

    // Reconnect and send a message successfully
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &message));

    deinitializeSignalingClient();
}

TEST_F(SignalingApiFunctionalityTest, cachingWithFaultInjection)
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
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;

    // Make describe and getendpoint fail once so we can check the no-caching behavior
    // in case when there is a failure.
    describeResult = STATUS_SERVICE_CALL_TIMEOUT_ERROR;
    describeFail = 0;
    describeRecover = 1;
    getEndpointResult = STATUS_SERVICE_CALL_TIMEOUT_ERROR;
    getEndpointFail = 0;
    getEndpointRecover = 1;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    // NOTE: the default 15 seconds of retries + 5 second of wait in the test
    // should make the cached value to expire.
    channelInfo.cachingPeriod = 20 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_DESCRIBE_GETENDPOINT;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);

    // Accounting for 1 failure 1 more time to loop back from the create
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);

    // Account for 1 time failure
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Validate the hook count
    EXPECT_EQ(2, describeCount);
    EXPECT_EQ(2, getEndpointCount);

    // Connect to the signaling client and make it fail
    connectResult = STATUS_SERVICE_CALL_NOT_AUTHORIZED_ERROR;
    connectFail = 0;
    connectRecover = MAX_UINT32;
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Validate the hook count
    EXPECT_EQ(2, describeCount);
    EXPECT_EQ(2, getEndpointCount);

    // Wait for the cache TTL to expire and retry
    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    // Connect to the signaling client after failing once to test the caching
    connectRecover = connectCount + 1;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Check the states
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_LE(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Validate the hook count is incremented due to cache miss
    EXPECT_EQ(3, describeCount);
    EXPECT_EQ(3, getEndpointCount);

    EXPECT_EQ(STATUS_SUCCESS, signalingClientDisconnectSync(signalingHandle));

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}

TEST_F(SignalingApiFunctionalityTest, fileCachingTest)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    CHAR signalingChannelName[64];
    const UINT32 totalChannelCount = MAX_SIGNALING_CACHE_ENTRY_COUNT + 1;
    UINT32 i, describeCountNoCache, getEndpointCountNoCache;
    CHAR channelArn[MAX_ARN_LEN + 1];

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_FILE;
    channelInfo.pRegion = TEST_DEFAULT_REGION;

    FREMOVE(DEFAULT_CACHE_FILE_PATH);

    for (i = 0; i < totalChannelCount; ++i) {
        SPRINTF(signalingChannelName, "%s%u", TEST_SIGNALING_CHANNEL_NAME, i);
        channelInfo.pChannelName = signalingChannelName;
        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient));
        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    }

    describeCountNoCache = describeCount;
    getEndpointCountNoCache = getEndpointCount;

    for (i = 0; i < totalChannelCount; ++i) {
        SPRINTF(signalingChannelName, "%s%u", TEST_SIGNALING_CHANNEL_NAME, i);
        channelInfo.pChannelName = signalingChannelName;
        channelInfo.pChannelArn = NULL;
        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient))
            << "Failed on channel name: " << channelInfo.pChannelName;

        // Store the channel ARN to be used later
        STRCPY(channelArn, pSignalingClient->channelDescription.channelArn);

        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));

        // Repeat the same with the ARN only
        channelInfo.pChannelName = NULL;
        channelInfo.pChannelArn = channelArn;

        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient));

        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    }

    /* describeCount and getEndpointCount should only increase by 1 because they are cached for all channels except one */
    EXPECT_TRUE(describeCount > describeCountNoCache && (describeCount - describeCountNoCache) == 1);
    EXPECT_TRUE(getEndpointCount > getEndpointCountNoCache && (getEndpointCount - 2 * getEndpointCountNoCache) == 1);
}

TEST_F(SignalingApiFunctionalityTest, fileCachingUpdateCache)
{
    FREMOVE(DEFAULT_CACHE_FILE_PATH);

    SignalingFileCacheEntry testEntry;
    SignalingFileCacheEntry testEntry2;

    testEntry.role = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    STRCPY(testEntry.wssEndpoint, "testWssEnpoint");
    STRCPY(testEntry.httpsEndpoint, "testHttpsEnpoint");
    STRCPY(testEntry.region, "testRegion");
    STRCPY(testEntry.channelArn, "testChannelArn");
    STRCPY(testEntry.channelName, "testChannel");
    testEntry.creationTsEpochSeconds = GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND;
    EXPECT_EQ(STATUS_SUCCESS, signalingCacheSaveToFile(&testEntry, DEFAULT_CACHE_FILE_PATH));

    testEntry.role = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    STRCPY(testEntry2.wssEndpoint, "testWssEnpoint");
    STRCPY(testEntry2.httpsEndpoint, "testHttpsEnpoint");
    STRCPY(testEntry2.region, "testRegion");
    STRCPY(testEntry2.channelArn, "testChannelArn2");
    STRCPY(testEntry2.channelName, "testChannel2");
    testEntry2.creationTsEpochSeconds = GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND;
    EXPECT_EQ(STATUS_SUCCESS, signalingCacheSaveToFile(&testEntry2, DEFAULT_CACHE_FILE_PATH));

    testEntry.creationTsEpochSeconds = GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND;
    /* update first cache entry*/
    EXPECT_EQ(STATUS_SUCCESS, signalingCacheSaveToFile(&testEntry, DEFAULT_CACHE_FILE_PATH));
}

TEST_F(SignalingApiFunctionalityTest, receivingIceConfigOffer)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    UINT32 i, iceCount;
    PIceConfigInfo pIceConfigInfo;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = mChannelName;
    channelInfo.pKmsKeyId = NULL;
    channelInfo.tagCount = 0;
    channelInfo.pTags = NULL;
    channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    channelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    channelInfo.retry = TRUE;
    channelInfo.reconnect = TRUE;
    channelInfo.pCertPath = mCaCertPath;
    channelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    // Connect to the channel
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    pActiveClient = pSignalingClient;

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Should have an exiting ICE configuration
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

    // Ensure the ICE is not refreshed as we already have a current non-expired set
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);

    // Trigger the ICE refresh immediately on any of the ICE accessor calls
    pSignalingClient->iceConfigCount = 0;

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));
    EXPECT_NE(0, iceCount);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);

    // Set to invalid again and trigger an update via offer message
    pSignalingClient->iceConfigCount = 0;

    // Inject a reconnect ice server message
    CHAR message[] = "{\n"
                     "    \"messageType\": \"SDP_OFFER\",\n"
                     "    \"senderClientId\": \"ProducerMaster\",\n"
                     "    \"messagePayload\": \"eyJ0eXBlIjogIm9mZmVyIiwgInNkcCI6ICJ2PTBcclxubz0tIDIwNTE4OTcyNDggMiBJTiBJUDQgMTI3LjAuMC4xXHJcbnM9LVxyXG50PTAgMFxyXG5hPWdyb3VwOkJVTkRMRSAwIDEgMlxyXG5hPW1zaWQtc2VtYW50aWM6IFdNUyBteUt2c1ZpZGVvU3RyZWFtXHJcbm09YXVkaW8gOSBVRFAvVExTL1JUUC9TQVZQRiAxMTFcclxuYz1JTiBJUDQgMTI3LjAuMC4xXHJcbmE9Y2FuZGlkYXRlOjUgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTo0IDEgdWRwIDE2Nzc3MjE1IDAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMCAwIHR5cCByZWxheSByYWRkciA6Oi8wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MyAxIHVkcCAxNjk0NDk4ODE1IDE5Mi4xNjguMC4yMyA1MTIwNSB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjIgMSB1ZHAgMTY3NzcyMTg1NSAxMC45NS4yMDQuNjEgNTI2NDYgdHlwIHNyZmx4IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZToxIDEgdWRwIDIxMTM5Mjk0NzEgMTAuOTUuMjA0LjYxIDUzNDI4IHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTowIDEgdWRwIDIxMzA3MDY0MzEgMTkyLjE2OC4wLjIzIDUwMTIzIHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPW1zaWQ6bXlLdnNWaWRlb1N0cmVhbSBteUF1ZGlvVHJhY2tcclxuYT1zc3JjOjE4OTEzODY4OTYgY25hbWU6QlA0bEVqdTBHK0VBQk0yS1xyXG5hPXNzcmM6MTg5MTM4Njg5NiBtc2lkOm15S3ZzVmlkZW9TdHJlYW0gbXlBdWRpb1RyYWNrXHJcbmE9c3NyYzoxODkxMzg2ODk2IG1zbGFiZWw6bXlLdnNWaWRlb1N0cmVhbVxyXG5hPXNzcmM6MTg5MTM4Njg5NiBsYWJlbDpteUF1ZGlvVHJhY2tcclxuYT1ydGNwOjkgSU4gSVA0IDAuMC4wLjBcclxuYT1pY2UtdWZyYWc6VVhwM1xyXG5hPWljZS1wd2Q6NGZZbTlEa1FQazl1YmRRQ2RyaFBhVFpnXHJcbmE9aWNlLW9wdGlvbnM6dHJpY2tsZVxyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjBcclxuYT1zZW5kcmVjdlxyXG5hPXJ0Y3AtbXV4XHJcbmE9cnRjcC1yc2l6ZVxyXG5hPXJ0cG1hcDoxMTEgb3B1cy80ODAwMC8yXHJcbmE9Zm10cDoxMTEgbWlucHRpbWU9MTA7dXNlaW5iYW5kZmVjPTFcclxuYT1ydGNwLWZiOjExMSBuYWNrXHJcbm09dmlkZW8gOSBVRFAvVExTL1JUUC9TQVZQRiAxMjVcclxuYz1JTiBJUDQgMTI3LjAuMC4xXHJcbmE9Y2FuZGlkYXRlOjUgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTo0IDEgdWRwIDE2Nzc3MjE1IDAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMCAwIHR5cCByZWxheSByYWRkciA6Oi8wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MyAxIHVkcCAxNjk0NDk4ODE1IDE5Mi4xNjguMC4yMyA1MTIwNSB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjIgMSB1ZHAgMTY3NzcyMTg1NSAxMC45NS4yMDQuNjEgNTI2NDYgdHlwIHNyZmx4IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZToxIDEgdWRwIDIxMTM5Mjk0NzEgMTAuOTUuMjA0LjYxIDUzNDI4IHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTowIDEgdWRwIDIxMzA3MDY0MzEgMTkyLjE2OC4wLjIzIDUwMTIzIHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPW1zaWQ6bXlLdnNWaWRlb1N0cmVhbSBteVZpZGVvVHJhY2tcclxuYT1zc3JjOjIxNDEwMjk1OTIgY25hbWU6QlA0bEVqdTBHK0VBQk0yS1xyXG5hPXNzcmM6MjE0MTAyOTU5MiBtc2lkOm15S3ZzVmlkZW9TdHJlYW0gbXlWaWRlb1RyYWNrXHJcbmE9c3NyYzoyMTQxMDI5NTkyIG1zbGFiZWw6bXlLdnNWaWRlb1N0cmVhbVxyXG5hPXNzcmM6MjE0MTAyOTU5MiBsYWJlbDpteVZpZGVvVHJhY2tcclxuYT1ydGNwOjkgSU4gSVA0IDAuMC4wLjBcclxuYT1pY2UtdWZyYWc6VVhwM1xyXG5hPWljZS1wd2Q6NGZZbTlEa1FQazl1YmRRQ2RyaFBhVFpnXHJcbmE9aWNlLW9wdGlvbnM6dHJpY2tsZVxyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjFcclxuYT1zZW5kcmVjdlxyXG5hPXJ0Y3AtbXV4XHJcbmE9cnRjcC1yc2l6ZVxyXG5hPXJ0cG1hcDoxMjUgSDI2NC85MDAwMFxyXG5hPWZtdHA6MTI1IGxldmVsLWFzeW1tZXRyeS1hbGxvd2VkPTE7cGFja2V0aXphdGlvbi1tb2RlPTE7cHJvZmlsZS1sZXZlbC1pZD00MmUwMWZcclxuYT1ydGNwLWZiOjEyNSBuYWNrXHJcbm09YXBwbGljYXRpb24gOSBVRFAvRFRMUy9TQ1RQIHdlYnJ0Yy1kYXRhY2hhbm5lbFxyXG5jPUlOIElQNCAxMjcuMC4wLjFcclxuYT1jYW5kaWRhdGU6NSAxIHVkcCAxNjc3NzIxNSAwMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDAgMCB0eXAgcmVsYXkgcmFkZHIgOjovMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjQgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTozIDEgdWRwIDE2OTQ0OTg4MTUgMTkyLjE2OC4wLjIzIDUxMjA1IHR5cCBzcmZseCByYWRkciAwLjAuMC4wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MiAxIHVkcCAxNjc3NzIxODU1IDEwLjk1LjIwNC42MSA1MjY0NiB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjEgMSB1ZHAgMjExMzkyOTQ3MSAxMC45NS4yMDQuNjEgNTM0MjggdHlwIGhvc3QgcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjAgMSB1ZHAgMjEzMDcwNjQzMSAxOTIuMTY4LjAuMjMgNTAxMjMgdHlwIGhvc3QgcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9cnRjcDo5IElOIElQNCAwLjAuMC4wXHJcbmE9aWNlLXVmcmFnOlVYcDNcclxuYT1pY2UtcHdkOjRmWW05RGtRUGs5dWJkUUNkcmhQYVRaZ1xyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjJcclxuYT1zY3RwLXBvcnQ6NTAwMFxyXG4ifQ==\",\n"
                     "    \"IceServerList\": [{\n"
                     "            \"Password\": \"ZEXx/a0G7reNO4SrDoK0zYoXZCamD+k/mIn6PEiuDTk=\",\n"
                     "            \"Ttl\": 298,\n"
                     "            \"Uris\": [\"turn:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp\"],\n"
                     "            \"Username\": \"1607424954:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjgzNjIwMzExNzk3MTpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTU5OTg1NjczODM5OA==\"\n"
                     "        },\n"
                     "        {\n"
                     "            \"Password\": \"k5PFpnyKu+oLa3Y3QUIhi+NA3BONdSUevw7NAAy/Nms=\",\n"
                     "            \"Ttl\": 298,\n"
                     "            \"Uris\": [\"turn:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp\"],\n"
                     "            \"Username\": \"1607424954:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjgzNjIwMzExNzk3MTpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTU5OTg1NjczODM5OA==\"\n"
                     "        }\n"
                     "    ],\n"
                     "    \"statusResponse\": {\n"
                     "        \"correlationId\": \"CorrelationID\",\n"
                     "        \"errorType\": \"Unknown message\",\n"
                     "        \"statusCode\": \"200\",\n"
                     "        \"description\": \"Test attempt to send an unknown message\"\n"
                     "    }\n"
                     "}";

    EXPECT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, message, ARRAY_SIZE(message)));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // ICE should not have been called again as we updated it via a message
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);

    // Validate the retrieved info
    EXPECT_EQ(2, iceCount);

    for (i = 0; i < iceCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, i, &pIceConfigInfo));
        EXPECT_NE(0, pIceConfigInfo->uriCount);
        EXPECT_EQ(298 * HUNDREDS_OF_NANOS_IN_A_SECOND, pIceConfigInfo->ttl);
        EXPECT_EQ(SIGNALING_ICE_CONFIG_INFO_CURRENT_VERSION, pIceConfigInfo->version);
        EXPECT_EQ(0, STRCMP("1607424954:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjgzNjIwMzExNzk3MTpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTU5OTg1NjczODM5OA==", pIceConfigInfo->userName));
    }

    //
    // Set to invalid again to trigger an update.
    // The message will not update as the type is not an offer
    //
    pSignalingClient->iceConfigCount = 0;

    // Inject a reconnect ice server message
    CHAR message2[] = "{\n"
                     "    \"messageType\": \"SDP_ANSWER\",\n"
                     "    \"senderClientId\": \"ProducerMaster\",\n"
                     "    \"messagePayload\": \"eyJ0eXBlIjogIm9mZmVyIiwgInNkcCI6ICJ2PTBcclxubz0tIDIwNTE4OTcyNDggMiBJTiBJUDQgMTI3LjAuMC4xXHJcbnM9LVxyXG50PTAgMFxyXG5hPWdyb3VwOkJVTkRMRSAwIDEgMlxyXG5hPW1zaWQtc2VtYW50aWM6IFdNUyBteUt2c1ZpZGVvU3RyZWFtXHJcbm09YXVkaW8gOSBVRFAvVExTL1JUUC9TQVZQRiAxMTFcclxuYz1JTiBJUDQgMTI3LjAuMC4xXHJcbmE9Y2FuZGlkYXRlOjUgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTo0IDEgdWRwIDE2Nzc3MjE1IDAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMCAwIHR5cCByZWxheSByYWRkciA6Oi8wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MyAxIHVkcCAxNjk0NDk4ODE1IDE5Mi4xNjguMC4yMyA1MTIwNSB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjIgMSB1ZHAgMTY3NzcyMTg1NSAxMC45NS4yMDQuNjEgNTI2NDYgdHlwIHNyZmx4IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZToxIDEgdWRwIDIxMTM5Mjk0NzEgMTAuOTUuMjA0LjYxIDUzNDI4IHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTowIDEgdWRwIDIxMzA3MDY0MzEgMTkyLjE2OC4wLjIzIDUwMTIzIHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPW1zaWQ6bXlLdnNWaWRlb1N0cmVhbSBteUF1ZGlvVHJhY2tcclxuYT1zc3JjOjE4OTEzODY4OTYgY25hbWU6QlA0bEVqdTBHK0VBQk0yS1xyXG5hPXNzcmM6MTg5MTM4Njg5NiBtc2lkOm15S3ZzVmlkZW9TdHJlYW0gbXlBdWRpb1RyYWNrXHJcbmE9c3NyYzoxODkxMzg2ODk2IG1zbGFiZWw6bXlLdnNWaWRlb1N0cmVhbVxyXG5hPXNzcmM6MTg5MTM4Njg5NiBsYWJlbDpteUF1ZGlvVHJhY2tcclxuYT1ydGNwOjkgSU4gSVA0IDAuMC4wLjBcclxuYT1pY2UtdWZyYWc6VVhwM1xyXG5hPWljZS1wd2Q6NGZZbTlEa1FQazl1YmRRQ2RyaFBhVFpnXHJcbmE9aWNlLW9wdGlvbnM6dHJpY2tsZVxyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjBcclxuYT1zZW5kcmVjdlxyXG5hPXJ0Y3AtbXV4XHJcbmE9cnRjcC1yc2l6ZVxyXG5hPXJ0cG1hcDoxMTEgb3B1cy80ODAwMC8yXHJcbmE9Zm10cDoxMTEgbWlucHRpbWU9MTA7dXNlaW5iYW5kZmVjPTFcclxuYT1ydGNwLWZiOjExMSBuYWNrXHJcbm09dmlkZW8gOSBVRFAvVExTL1JUUC9TQVZQRiAxMjVcclxuYz1JTiBJUDQgMTI3LjAuMC4xXHJcbmE9Y2FuZGlkYXRlOjUgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTo0IDEgdWRwIDE2Nzc3MjE1IDAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMCAwIHR5cCByZWxheSByYWRkciA6Oi8wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MyAxIHVkcCAxNjk0NDk4ODE1IDE5Mi4xNjguMC4yMyA1MTIwNSB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjIgMSB1ZHAgMTY3NzcyMTg1NSAxMC45NS4yMDQuNjEgNTI2NDYgdHlwIHNyZmx4IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZToxIDEgdWRwIDIxMTM5Mjk0NzEgMTAuOTUuMjA0LjYxIDUzNDI4IHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTowIDEgdWRwIDIxMzA3MDY0MzEgMTkyLjE2OC4wLjIzIDUwMTIzIHR5cCBob3N0IHJhZGRyIDAuMC4wLjAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPW1zaWQ6bXlLdnNWaWRlb1N0cmVhbSBteVZpZGVvVHJhY2tcclxuYT1zc3JjOjIxNDEwMjk1OTIgY25hbWU6QlA0bEVqdTBHK0VBQk0yS1xyXG5hPXNzcmM6MjE0MTAyOTU5MiBtc2lkOm15S3ZzVmlkZW9TdHJlYW0gbXlWaWRlb1RyYWNrXHJcbmE9c3NyYzoyMTQxMDI5NTkyIG1zbGFiZWw6bXlLdnNWaWRlb1N0cmVhbVxyXG5hPXNzcmM6MjE0MTAyOTU5MiBsYWJlbDpteVZpZGVvVHJhY2tcclxuYT1ydGNwOjkgSU4gSVA0IDAuMC4wLjBcclxuYT1pY2UtdWZyYWc6VVhwM1xyXG5hPWljZS1wd2Q6NGZZbTlEa1FQazl1YmRRQ2RyaFBhVFpnXHJcbmE9aWNlLW9wdGlvbnM6dHJpY2tsZVxyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjFcclxuYT1zZW5kcmVjdlxyXG5hPXJ0Y3AtbXV4XHJcbmE9cnRjcC1yc2l6ZVxyXG5hPXJ0cG1hcDoxMjUgSDI2NC85MDAwMFxyXG5hPWZtdHA6MTI1IGxldmVsLWFzeW1tZXRyeS1hbGxvd2VkPTE7cGFja2V0aXphdGlvbi1tb2RlPTE7cHJvZmlsZS1sZXZlbC1pZD00MmUwMWZcclxuYT1ydGNwLWZiOjEyNSBuYWNrXHJcbm09YXBwbGljYXRpb24gOSBVRFAvRFRMUy9TQ1RQIHdlYnJ0Yy1kYXRhY2hhbm5lbFxyXG5jPUlOIElQNCAxMjcuMC4wLjFcclxuYT1jYW5kaWRhdGU6NSAxIHVkcCAxNjc3NzIxNSAwMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDAgMCB0eXAgcmVsYXkgcmFkZHIgOjovMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjQgMSB1ZHAgMTY3NzcyMTUgMDAwMDowMDAwOjAwMDA6MDAwMDowMDAwOjAwMDA6MDAwMDowMDAwIDAgdHlwIHJlbGF5IHJhZGRyIDo6LzAgcnBvcnQgMCBnZW5lcmF0aW9uIDAgbmV0d29yay1jb3N0IDk5OVxyXG5hPWNhbmRpZGF0ZTozIDEgdWRwIDE2OTQ0OTg4MTUgMTkyLjE2OC4wLjIzIDUxMjA1IHR5cCBzcmZseCByYWRkciAwLjAuMC4wIHJwb3J0IDAgZ2VuZXJhdGlvbiAwIG5ldHdvcmstY29zdCA5OTlcclxuYT1jYW5kaWRhdGU6MiAxIHVkcCAxNjc3NzIxODU1IDEwLjk1LjIwNC42MSA1MjY0NiB0eXAgc3JmbHggcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjEgMSB1ZHAgMjExMzkyOTQ3MSAxMC45NS4yMDQuNjEgNTM0MjggdHlwIGhvc3QgcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9Y2FuZGlkYXRlOjAgMSB1ZHAgMjEzMDcwNjQzMSAxOTIuMTY4LjAuMjMgNTAxMjMgdHlwIGhvc3QgcmFkZHIgMC4wLjAuMCBycG9ydCAwIGdlbmVyYXRpb24gMCBuZXR3b3JrLWNvc3QgOTk5XHJcbmE9cnRjcDo5IElOIElQNCAwLjAuMC4wXHJcbmE9aWNlLXVmcmFnOlVYcDNcclxuYT1pY2UtcHdkOjRmWW05RGtRUGs5dWJkUUNkcmhQYVRaZ1xyXG5hPWZpbmdlcnByaW50OnNoYS0yNTYgQkQ6RTk6QkI6RTE6ODE6NzQ6MDU6RkQ6Mzc6QUI6MzU6MTU6OTE6NTQ6ODc6RDU6NDI6QkU6RjQ6RjE6MUQ6NjA6OEI6REQ6NEQ6RUM6QzM6NDQ6RkU6OTc6ODg6MjBcclxuYT1zZXR1cDphY3RwYXNzXHJcbmE9bWlkOjJcclxuYT1zY3RwLXBvcnQ6NTAwMFxyXG4ifQ==\",\n"
                     "    \"IceServerList\": [{\n"
                     "            \"Password\": \"ZEXx/a0G7reNO4SrDoK0zYoXZCamD+k/mIn6PEiuDTk=\",\n"
                     "            \"Ttl\": 298,\n"
                     "            \"Uris\": [\"turn:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:18-236-143-60.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp\"],\n"
                     "            \"Username\": \"1607424954:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjgzNjIwMzExNzk3MTpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTU5OTg1NjczODM5OA==\"\n"
                     "        },\n"
                     "        {\n"
                     "            \"Password\": \"k5PFpnyKu+oLa3Y3QUIhi+NA3BONdSUevw7NAAy/Nms=\",\n"
                     "            \"Ttl\": 298,\n"
                     "            \"Uris\": [\"turn:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=udp\", \"turns:52-25-38-73.t-4f692171.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp\"],\n"
                     "            \"Username\": \"1607424954:djE6YXJuOmF3czpraW5lc2lzdmlkZW86dXMtd2VzdC0yOjgzNjIwMzExNzk3MTpjaGFubmVsL1NjYXJ5VGVzdENoYW5uZWwvMTU5OTg1NjczODM5OA==\"\n"
                     "        }\n"
                     "    ],\n"
                     "    \"statusResponse\": {\n"
                     "        \"correlationId\": \"CorrelationID\",\n"
                     "        \"errorType\": \"Unknown message\",\n"
                     "        \"statusCode\": \"200\",\n"
                     "        \"description\": \"Test attempt to send an unknown message\"\n"
                     "    }\n"
                     "}";

    EXPECT_EQ(STATUS_SUCCESS, receiveLwsMessage(pSignalingClient, message2, ARRAY_SIZE(message2)));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // ICE should have been called again as we couldn't have updated via the message
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);

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

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
