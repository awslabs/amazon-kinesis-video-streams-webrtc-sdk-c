#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SignalingApiTest : public WebRtcClientTestBase {
};

TEST_F(SignalingApiTest, createValidateChannelInfo)
{
    initializeSignalingClientStructs();
    PChannelInfo rChannelInfo;
    CHAR agentString[MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1];
    UINT32 postfixLen = STRLEN(SIGNALING_USER_AGENT_POSTFIX_NAME) + STRLEN(SIGNALING_USER_AGENT_POSTFIX_VERSION) + 1;
    SNPRINTF(agentString, postfixLen + 1, (PCHAR) "%s/%s", SIGNALING_USER_AGENT_POSTFIX_NAME, SIGNALING_USER_AGENT_POSTFIX_VERSION);
    STRCPY(mChannelArn, TEST_CHANNEL_ARN);
    STRCPY(mStreamArn, TEST_STREAM_ARN);
    STRCPY(mKmsKeyId, TEST_KMS_KEY_ID_ARN);
    mChannelInfo.pChannelArn = mChannelArn;
    mChannelInfo.pStorageStreamArn = mStreamArn;
    mChannelInfo.pKmsKeyId = mKmsKeyId;
    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&mChannelInfo, &rChannelInfo));
    EXPECT_EQ(0, STRCMP(rChannelInfo->pChannelArn, TEST_CHANNEL_ARN));
    EXPECT_EQ(0, STRCMP(rChannelInfo->pStorageStreamArn, TEST_STREAM_ARN));
    EXPECT_EQ(0, STRCMP(rChannelInfo->pKmsKeyId, TEST_KMS_KEY_ID_ARN));
    EXPECT_EQ(rChannelInfo->version, CHANNEL_INFO_CURRENT_VERSION);
    EXPECT_EQ(rChannelInfo->tagCount, 3);
    EXPECT_EQ(rChannelInfo->retry, TRUE);
    EXPECT_EQ(rChannelInfo->channelType, SIGNALING_CHANNEL_TYPE_SINGLE_MASTER);
    EXPECT_EQ(rChannelInfo->channelRoleType, SIGNALING_CHANNEL_ROLE_TYPE_MASTER);
    EXPECT_EQ(rChannelInfo->cachingPolicy, SIGNALING_API_CALL_CACHE_TYPE_NONE);
    // The createValidateChannelInfo() is expected to fix up caching period to an hour
    EXPECT_EQ(rChannelInfo->cachingPeriod, SIGNALING_DEFAULT_API_CALL_CACHE_TTL);
    EXPECT_EQ(rChannelInfo->reconnect, TRUE);
    EXPECT_EQ(0, STRCMP(rChannelInfo->pCertPath, mCaCertPath));
    EXPECT_EQ(rChannelInfo->messageTtl, TEST_SIGNALING_MESSAGE_TTL);
    EXPECT_EQ(0, STRCMP(rChannelInfo->pRegion, TEST_DEFAULT_REGION));
    // Test default agent postfix
    EXPECT_PRED_FORMAT2(testing::IsSubstring, agentString, rChannelInfo->pUserAgent);
    freeChannelInfo(&rChannelInfo);
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, testChannelArnsValid)
{
    PChannelInfo pChannelInfo;
    ChannelInfo channelInfo;

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));

    PCHAR arn1 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/a/0123456789012";
    PCHAR arn2 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/ab/0123456789012";
    PCHAR arn3 = (PCHAR)"arn:aws-cn:kinesisvideo:us-west-2:123456789012:channel/channel_name/0123456789012";
    PCHAR arn4 = (PCHAR)"arn:aws-xyz:kinesisvideo:us-west-2:123456789012:channel/channel_name/0123456789012";
    PCHAR arn5 = (PCHAR)"arn:aws:kinesisvideo:us-east-2:123456789012:channel/channel_name/0123456789012";
    PCHAR arn6 = (PCHAR)"arn:aws:kinesisvideo:us-east-1:123456789012:channel/channel_name/0123456789012";
    PCHAR arn7 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/channel_name/5738283847173";
    PCHAR arn8 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/channel_name/1223445566666";

    channelInfo.pChannelArn = arn1;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn2;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn3;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn4;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn5;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn6;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn7;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);

    channelInfo.pChannelArn = arn8;
    EXPECT_EQ(createValidateChannelInfo(&channelInfo, &pChannelInfo), STATUS_SUCCESS);
    freeChannelInfo(&pChannelInfo);
}

TEST_F(SignalingApiTest, testChannelArnsInValid)
{
    PChannelInfo pChannelInfo;
    ChannelInfo channelInfo;
    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));

    PCHAR arn1 = (PCHAR)"arn:aws:kinesaisvideo:us-west-2:123456789012:channel/a/0123456789012";
    PCHAR arn2 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:chanel/ab/0123456789012";
    PCHAR arn3 = (PCHAR)"arn:aw:kinesisvideo:us-west-2:123456789012:channel/channel_name/0123456789012";
    PCHAR arn4 = (PCHAR)"arn:aws-xyz:kinesisvideo:us-west-2:12345679012:channel/channel_name/0123456789012";
    PCHAR arn5 = (PCHAR)"arn:aws:kinesisvideo:us-east-2:123456789012:channel/channel_name/012345679012";
    PCHAR arn6 = (PCHAR)"arn:aws:kinesisvideo:us-east-1:123456789012:channel//0123456789012";
    PCHAR arn7 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/5738283847173";
    PCHAR arn8 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel1223445566666";
    PCHAR arn9 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012channel/a/0123456789012";
    PCHAR arn10 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channnnnnnnnel/ab/0123456789012";
    PCHAR arn11 = (PCHAR)"arn:aws:kinesisvideo:123456789012:channel/channel_name/0123456789012";
    PCHAR arn12 = (PCHAR)"arn:aws-xyz:kinesisvideo:::channel/channel_name/0123456789012";
    PCHAR arn13 = (PCHAR)"arn:aws:012345679012";
    PCHAR arn14 = (PCHAR)"this:is:a:test:arn:which:is:not:real";
    PCHAR arn15 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:/:/:///5738283847173";
    PCHAR arn16 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:cool_channel_Name";
    PCHAR arn17 = (PCHAR)"arn:aws:kinesisvideo::123456789012:channel/a/0123456789012";
    PCHAR arn18 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/a/01234567890123";
    PCHAR arn19 = (PCHAR)"ar:aws:kinesisvideo:us-west-2:123456789012:channel/a/0123456789012";
    PCHAR arn20 = (PCHAR)"arn:aws:kinesisvideo::us-west-2:123456789012:channel/a/01234567890123";
    PCHAR arn21 = (PCHAR)"arn:aws::kinesisvideo:us-west-2:123456789012:channel/a/01234567890123";
    PCHAR arn22 = (PCHAR)"arn::aws::kinesisvideo:us-west-2:123456789012:channel/a/01234567890123";
    PCHAR arn23 = (PCHAR)"arn:aws::kinesisvideo:us-west-2::123456789012:channel/a/01234567890123";
    PCHAR arn24 = (PCHAR)"arn:aws:kinesisvideo:us-west-2:123456789012:channel/a/b/0123456789012";

    channelInfo.pChannelArn = arn1;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn2;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn3;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn4;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn5;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn6;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn7;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn8;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn9;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn10;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn11;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn12;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn13;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn14;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn15;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn16;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn17;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn18;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn19;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn20;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn21;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn22;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn23;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));

    channelInfo.pChannelArn = arn24;
    EXPECT_EQ(STATUS_SIGNALING_INVALID_CHANNEL_ARN, createValidateChannelInfo(&channelInfo, &pChannelInfo));
}

TEST_F(SignalingApiTest, signalingSendMessageSync)
{
    STATUS expectedStatus;
    SignalingMessage signalingMessage;

    initializeSignalingClient();

    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_NE(STATUS_SUCCESS, signalingClientSendMessageSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &signalingMessage));
    EXPECT_NE(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientSendMessageSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));

    // Not connected
    expectedStatus = mAccessKeyIdSet ? STATUS_INVALID_STREAM_STATE : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // Connect and retry
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // Some correlation id
    STRCPY(signalingMessage.correlationId, SIGNAING_TEST_CORRELATION_ID);
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // No peer id
    signalingMessage.peerClientId[0] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // No peer id no correlation id
    signalingMessage.correlationId[0] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingSendMessageSyncFileCredsProvider)
{
    SignalingMessage signalingMessage;
    PAwsCredentialProvider pAwsCredentialProvider = NULL;
    CHAR fileContent[10000];
    UINT32 length = ARRAY_SIZE(fileContent);
    CHAR futureTime[] = "2200-06-05T09:39:36Z";

    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    if (mSessionToken == NULL) {
        // Store the credentials in a file under the current dir
        length = SNPRINTF(fileContent, length, "CREDENTIALS %s %s", mAccessKey, mSecretKey);
        ASSERT_GT(ARRAY_SIZE(fileContent), length);
    } else {
        // test Temp Creds
        // "CREDENTIALS accessKey expiration secretKey sessionToken"
        length = SNPRINTF(fileContent, length, "CREDENTIALS %s %s %s %s", mAccessKey, futureTime, mSecretKey, mSessionToken);
        ASSERT_GT(ARRAY_SIZE(fileContent), length);
    }

    ASSERT_EQ(STATUS_SUCCESS, writeFile(TEST_FILE_CREDENTIALS_FILE_PATH, FALSE, FALSE, (PBYTE) fileContent, length));
    // Create file creds provider from the file
    EXPECT_EQ(STATUS_SUCCESS, createFileCredentialProvider(TEST_FILE_CREDENTIALS_FILE_PATH, &pAwsCredentialProvider));

    initializeSignalingClient(pAwsCredentialProvider);

    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // Some correlation id
    STRCPY(signalingMessage.correlationId, SIGNAING_TEST_CORRELATION_ID);
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // No peer id
    signalingMessage.peerClientId[0] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    // No peer id no correlation id
    signalingMessage.correlationId[0] = '\0';
    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    deinitializeSignalingClient();

    EXPECT_EQ(STATUS_SUCCESS, freeFileCredentialProvider(&pAwsCredentialProvider));
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientConnectSync)
{
    STATUS expectedStatus;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE));
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));

    // Connect again
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientDeleteSync)
{
    STATUS expectedStatus;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientDeleteSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE));
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientDeleteSync(mSignalingClientHandle));

    // Call again - idempotent
    EXPECT_EQ(expectedStatus, signalingClientDeleteSync(mSignalingClientHandle));

    // Attempt to call a connect should fail
    expectedStatus = mAccessKeyIdSet ? STATUS_INVALID_STREAM_STATE : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));

    // Attempt to send a message should fail
    SignalingMessage signalingMessage;
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';
    EXPECT_EQ(expectedStatus, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfoCount)
{
    STATUS expectedStatus;
    UINT32 count;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &count));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));

    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &count));
    if (mAccessKeyIdSet) {
        EXPECT_NE(0, count);
        EXPECT_GE(MAX_ICE_CONFIG_COUNT, count);
    }

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfo)
{
    UINT32 i, j, count;
    PIceConfigInfo pIceConfigInfo;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, 0, &pIceConfigInfo));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, 0, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, 0, NULL));

    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &count));
        EXPECT_NE(0, count);
        EXPECT_GE(MAX_ICE_CONFIG_COUNT, count);

        // Referencing past the max count
        EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, count, &pIceConfigInfo));

        for (i = 0; i < count; i++) {
            EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
            EXPECT_EQ(SIGNALING_ICE_CONFIG_INFO_CURRENT_VERSION, pIceConfigInfo->version);
            EXPECT_NE(0, pIceConfigInfo->uriCount);
            EXPECT_GE(MAX_ICE_CONFIG_URI_COUNT, pIceConfigInfo->uriCount);
            EXPECT_NE('\0', pIceConfigInfo->password[0]);
            EXPECT_NE('\0', pIceConfigInfo->userName[0]);
            EXPECT_NE(0, pIceConfigInfo->ttl);

            for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                EXPECT_NE('\0', pIceConfigInfo->uris[j][0]);
            }
        }
    }

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientGetCurrentState)
{
    STATUS expectedStatus;
    SIGNALING_CLIENT_STATE state, expectedState;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &state));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));

    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientGetCurrentState(mSignalingClientHandle, &state));

    expectedState = mAccessKeyIdSet ? SIGNALING_CLIENT_STATE_READY : SIGNALING_CLIENT_STATE_UNKNOWN;
    EXPECT_EQ(expectedState, state);

    deinitializeSignalingClient();
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientGetStateString)
{
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetStateString(SIGNALING_CLIENT_STATE_UNKNOWN, NULL));

    for (UINT32 i = 0; i <= (UINT32) SIGNALING_CLIENT_STATE_MAX_VALUE + 1; i++) {
        PCHAR pStateStr;
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetStateString((SIGNALING_CLIENT_STATE) i, &pStateStr));
        DLOGV("Iterating states \"%s\"", pStateStr);
    }
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientDisconnectSync)
{
    EXPECT_NE(STATUS_SUCCESS, signalingClientDisconnectSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE));
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

TEST_F(SignalingApiTest, signalingClientGetMetrics)
{
    SignalingClientMetrics metrics;
    SignalingMessage signalingMessage;
    metrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    // Invalid input
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetMetrics(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &metrics));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetMetrics(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, NULL));

    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    initializeSignalingClient();
    // Valid call
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, &metrics));

    EXPECT_EQ(0, metrics.signalingClientStats.numberOfReconnects);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesSent);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesReceived);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfErrors);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfRuntimeErrors);
    EXPECT_EQ(1, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_EQ(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_NE(0, metrics.signalingClientStats.dpApiCallLatency);

    // Connect and get metrics
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));

    // Await for a little to ensure we get some metrics for the connection duration
    THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, &metrics));
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfReconnects);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesSent);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesReceived);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfErrors);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfRuntimeErrors);
    EXPECT_EQ(1, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_NE(0, metrics.signalingClientStats.dpApiCallLatency);

    // Send a message and get metrics
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;
    signalingMessage.correlationId[0] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, &metrics));
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfReconnects);
    EXPECT_EQ(1, metrics.signalingClientStats.numberOfMessagesSent);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesReceived);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfErrors);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfRuntimeErrors);
    EXPECT_EQ(1, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_NE(0, metrics.signalingClientStats.dpApiCallLatency);

    // Make a couple of bad API invocations
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, 0, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientSendMessageSync(mSignalingClientHandle, NULL));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetMetrics(mSignalingClientHandle, &metrics));
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfReconnects);
    EXPECT_EQ(1, metrics.signalingClientStats.numberOfMessagesSent);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfMessagesReceived);
    EXPECT_EQ(5, metrics.signalingClientStats.numberOfErrors);
    EXPECT_EQ(0, metrics.signalingClientStats.numberOfRuntimeErrors);
    EXPECT_EQ(1, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_NE(0, metrics.signalingClientStats.dpApiCallLatency);

    deinitializeSignalingClient();
}

TEST_F(SignalingApiTest, signalingClientCreateWithClientInfoVariations)
{
    STATUS retStatus;
    CHAR testPath[MAX_PATH_LEN + 2];
    MEMSET(testPath, 'a', MAX_PATH_LEN + 1);
    testPath[MAX_PATH_LEN + 1] = '\0';

    initializeSignalingClientStructs();

    //
    // Invalid version
    //

    // Override the version of the client info struct
    mClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION + 1;
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks, mTestCredentialProvider, &mSignalingClientHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SIGNALING_INVALID_CLIENT_INFO_VERSION, retStatus);
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;

    //
    // Invalid max path
    //
    mClientInfo.cacheFilePath = testPath;
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks, mTestCredentialProvider, &mSignalingClientHandle);

    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SIGNALING_INVALID_CLIENT_INFO_CACHE_FILE_PATH_LEN, retStatus);
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.cacheFilePath = NULL;

    //
    // Version 0 ignoring path
    //

    // Set the version to 0 and the path to non-default
    mClientInfo.version = 0;
    mClientInfo.cacheFilePath = (PCHAR) "/some/test/path";
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks,
                                          mTestCredentialProvider, &mSignalingClientHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, retStatus);

        // Validate the cache file path
        PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);
        EXPECT_EQ(0, STRCMP(DEFAULT_CACHE_FILE_PATH, pSignalingClient->clientInfo.cacheFilePath));
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    mClientInfo.cacheFilePath = NULL;

    //
    // Version 0 setting to large path doesn't error
    //

    // Set the version to 0 and the path to non-default
    mClientInfo.version = 0;
    mClientInfo.cacheFilePath = testPath;
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks,
                                          mTestCredentialProvider, &mSignalingClientHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, retStatus);

        // Validate the cache file path
        PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);
        EXPECT_EQ(0, STRCMP(DEFAULT_CACHE_FILE_PATH, pSignalingClient->clientInfo.cacheFilePath));
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    mClientInfo.cacheFilePath = NULL;

    //
    // Version 1 empty path
    //

    mClientInfo.cacheFilePath = EMPTY_STRING;
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks,
                                          mTestCredentialProvider, &mSignalingClientHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, retStatus);

        // Validate the cache file path
        PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);
        EXPECT_EQ(0, STRCMP(DEFAULT_CACHE_FILE_PATH, pSignalingClient->clientInfo.cacheFilePath));
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.cacheFilePath = NULL;

    //
    // Version 1 non default path
    //

    mClientInfo.cacheFilePath = TEST_CACHE_FILE_PATH;
    retStatus = createSignalingClientSync(&mClientInfo, &mChannelInfo, &mSignalingClientCallbacks,
                                          mTestCredentialProvider, &mSignalingClientHandle);
    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, retStatus);

        // Validate the cache file path
        PSignalingClient pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(mSignalingClientHandle);
        EXPECT_EQ(0, STRCMP(TEST_CACHE_FILE_PATH, pSignalingClient->clientInfo.cacheFilePath));
    } else {
        mSignalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
        EXPECT_NE(STATUS_SUCCESS, retStatus);
    }

    deinitializeSignalingClient();
    mClientInfo.cacheFilePath = NULL;
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

// Shared state for end-to-end payload delivery verification
static struct {
    volatile ATOMIC_BOOL received;
    MUTEX lock;
    UINT32 payloadLen;
    CHAR payload[MAX_SIGNALING_MESSAGE_LEN + 1];
} gReceivedPayload;

STATUS e2eMessageReceivedCallback(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    UNUSED_PARAM(customData);
    MUTEX_LOCK(gReceivedPayload.lock);
    gReceivedPayload.payloadLen = pReceivedSignalingMessage->signalingMessage.payloadLen;
    MEMCPY(gReceivedPayload.payload, pReceivedSignalingMessage->signalingMessage.payload,
           pReceivedSignalingMessage->signalingMessage.payloadLen);
    gReceivedPayload.payload[pReceivedSignalingMessage->signalingMessage.payloadLen] = '\0';
    MUTEX_UNLOCK(gReceivedPayload.lock);
    ATOMIC_STORE_BOOL(&gReceivedPayload.received, TRUE);
    return STATUS_SUCCESS;
}

// End-to-end test: viewer sends a large payload to master, master verifies
// the full payload is received without truncation.
// Requires valid AWS credentials.
//
// Run with: --gtest_filter="SignalingApiTest.verifyLargePayloadDeliveredWithoutTruncation"
TEST_F(SignalingApiTest, verifyLargePayloadDeliveredWithoutTruncation)
{
    if (!mAccessKeyIdSet) {
        GTEST_SKIP() << "Skipping: AWS credentials not set";
    }

    // --- Set up master ---
    SignalingClientCallbacks masterCallbacks;
    masterCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    masterCallbacks.customData = (UINT64) this;
    masterCallbacks.messageReceivedFn = e2eMessageReceivedCallback;
    masterCallbacks.errorReportFn = NULL;
    masterCallbacks.stateChangeFn = NULL;
    masterCallbacks.getCurrentTimeFn = NULL;

    SignalingClientInfo masterClientInfo;
    MEMSET(&masterClientInfo, 0x00, SIZEOF(SignalingClientInfo));
    masterClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    masterClientInfo.loggingLevel = LOG_LEVEL_WARN;
    masterClientInfo.cacheFilePath = NULL;
    masterClientInfo.signalingClientCreationMaxRetryAttempts = 0;
    STRCPY(masterClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    ChannelInfo masterChannelInfo;
    MEMSET(&masterChannelInfo, 0x00, SIZEOF(ChannelInfo));
    masterChannelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    masterChannelInfo.pChannelName = mChannelName;
    masterChannelInfo.pKmsKeyId = NULL;
    masterChannelInfo.tagCount = 0;
    masterChannelInfo.pTags = NULL;
    masterChannelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    masterChannelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER;
    masterChannelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    masterChannelInfo.retry = TRUE;
    masterChannelInfo.reconnect = TRUE;
    masterChannelInfo.pCertPath = mCaCertPath;
    masterChannelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    SIGNALING_CLIENT_HANDLE masterHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&masterClientInfo, &masterChannelInfo, &masterCallbacks,
                                        (PAwsCredentialProvider) mTestCredentialProvider, &masterHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientFetchSync(masterHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(masterHandle));

    // --- Set up viewer ---
    SignalingClientCallbacks viewerCallbacks;
    viewerCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    viewerCallbacks.customData = 0;
    viewerCallbacks.messageReceivedFn = NULL;
    viewerCallbacks.errorReportFn = NULL;
    viewerCallbacks.stateChangeFn = NULL;
    viewerCallbacks.getCurrentTimeFn = NULL;

    SignalingClientInfo viewerClientInfo;
    MEMSET(&viewerClientInfo, 0x00, SIZEOF(SignalingClientInfo));
    viewerClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    viewerClientInfo.loggingLevel = LOG_LEVEL_WARN;
    viewerClientInfo.cacheFilePath = NULL;
    viewerClientInfo.signalingClientCreationMaxRetryAttempts = 0;
    STRCPY(viewerClientInfo.clientId, TEST_SIGNALING_VIEWER_CLIENT_ID);

    ChannelInfo viewerChannelInfo;
    MEMSET(&viewerChannelInfo, 0x00, SIZEOF(ChannelInfo));
    viewerChannelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    viewerChannelInfo.pChannelName = mChannelName;
    viewerChannelInfo.pKmsKeyId = NULL;
    viewerChannelInfo.tagCount = 0;
    viewerChannelInfo.pTags = NULL;
    viewerChannelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    viewerChannelInfo.channelRoleType = SIGNALING_CHANNEL_ROLE_TYPE_VIEWER;
    viewerChannelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    viewerChannelInfo.retry = TRUE;
    viewerChannelInfo.reconnect = TRUE;
    viewerChannelInfo.pCertPath = mCaCertPath;
    viewerChannelInfo.messageTtl = TEST_SIGNALING_MESSAGE_TTL;

    SIGNALING_CLIENT_HANDLE viewerHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&viewerClientInfo, &viewerChannelInfo, &viewerCallbacks,
                                        (PAwsCredentialProvider) mTestCredentialProvider, &viewerHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientFetchSync(viewerHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(viewerHandle));

    // Let connections stabilize
    THREAD_SLEEP(1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // --- Send a large payload from viewer to master ---
    // Use a payload size that exercises the buffer well (10000 bytes with known pattern)
    const UINT32 testPayloadSize = 10000;
    SignalingMessage message;
    MEMSET(&message, 0x00, SIZEOF(SignalingMessage));
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(message.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);

    // Fill with a recognizable pattern: repeating "ABCDEFGH" so we can detect truncation/corruption
    for (UINT32 i = 0; i < testPayloadSize; i++) {
        message.payload[i] = 'A' + (i % 8);
    }
    message.payload[testPayloadSize] = '\0';
    message.payloadLen = testPayloadSize;

    // Also test with max-length correlationId and clientId to account for envelope overhead
    MEMSET(message.correlationId, 'C', MAX_CORRELATION_ID_LEN);
    message.correlationId[MAX_CORRELATION_ID_LEN] = '\0';

    // Reset receive state
    gReceivedPayload.lock = MUTEX_CREATE(TRUE);
    ATOMIC_STORE_BOOL(&gReceivedPayload.received, FALSE);
    gReceivedPayload.payloadLen = 0;

    EXPECT_EQ(STATUS_SUCCESS, signalingClientSendMessageSync(viewerHandle, &message));

    // Wait for the master to receive the message (up to 5 seconds)
    UINT64 timeout = 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    UINT64 elapsed = 0;
    UINT64 sleepInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    while (!ATOMIC_LOAD_BOOL(&gReceivedPayload.received) && elapsed < timeout) {
        THREAD_SLEEP(sleepInterval);
        elapsed += sleepInterval;
    }

    // --- Verify no truncation ---
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&gReceivedPayload.received)) << "Master did not receive the message within 5 seconds";

    MUTEX_LOCK(gReceivedPayload.lock);
    UINT32 receivedLen = gReceivedPayload.payloadLen;
    EXPECT_EQ(testPayloadSize, receivedLen)
        << "Payload was truncated! Expected " << testPayloadSize << " bytes, got " << receivedLen;

    // Verify content integrity byte-by-byte
    BOOL contentMatch = TRUE;
    UINT32 mismatchPos = 0;
    for (UINT32 i = 0; i < testPayloadSize && i < receivedLen; i++) {
        if (gReceivedPayload.payload[i] != ('A' + (i % 8))) {
            contentMatch = FALSE;
            mismatchPos = i;
            break;
        }
    }
    EXPECT_TRUE(contentMatch) << "Payload content mismatch at byte " << mismatchPos
                              << ": expected '" << (char)('A' + (mismatchPos % 8))
                              << "', got '" << gReceivedPayload.payload[mismatchPos] << "'";
    MUTEX_UNLOCK(gReceivedPayload.lock);

    printf("\n[E2E RESULT] Sent %u bytes, received %u bytes. %s\n\n",
           testPayloadSize, receivedLen,
           (testPayloadSize == receivedLen && contentMatch) ? "NO TRUNCATION" : "TRUNCATION DETECTED");

    // Cleanup
    MUTEX_FREE(gReceivedPayload.lock);
    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(masterHandle), 0);
    freeSignalingClient(&viewerHandle);
    freeSignalingClient(&masterHandle);
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

// Integration test that probes the KVS signaling service to find the maximum
// accepted payload size. This sends increasingly larger messages via binary search
// to discover the service-side limit. Requires valid AWS credentials.
//
// Run with: --gtest_filter="SignalingApiTest.probeMaxSignalingMessageSize"
// Set env: AWS_ACCESS_KEY_ID, AWS_SECRET_ACCESS_KEY, AWS_DEFAULT_REGION
TEST_F(SignalingApiTest, probeMaxSignalingMessageSize)
{
    if (!mAccessKeyIdSet) {
        GTEST_SKIP() << "Skipping: AWS credentials not set";
    }

    initializeSignalingClient();
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));

    SignalingMessage signalingMessage;
    MEMSET(&signalingMessage, 0x00, SIZEOF(SignalingMessage));
    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    // Use max-length peerClientId and correlationId to account for worst-case envelope
    MEMSET(signalingMessage.peerClientId, 'P', MAX_SIGNALING_CLIENT_ID_LEN);
    signalingMessage.peerClientId[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    MEMSET(signalingMessage.correlationId, 'C', MAX_CORRELATION_ID_LEN);
    signalingMessage.correlationId[MAX_CORRELATION_ID_LEN] = '\0';

    // Binary search between low (known good) and high (known bad)
    UINT32 low = 100;
    UINT32 high = MAX_SIGNALING_MESSAGE_LEN;
    UINT32 mid;
    UINT32 maxAccepted = 0;
    STATUS status;

    // First, verify baseline works
    MEMSET(signalingMessage.payload, 'A', low);
    signalingMessage.payload[low] = '\0';
    signalingMessage.payloadLen = low;
    status = signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage);
    ASSERT_EQ(STATUS_SUCCESS, status) << "Baseline send of " << low << " bytes failed";

    // Find the upper bound where sends start failing
    // Start by checking if MAX_SIGNALING_MESSAGE_LEN works
    UINT32 testSize = MAX_SIGNALING_MESSAGE_LEN > MAX_SESSION_DESCRIPTION_INIT_SDP_LEN
                          ? MAX_SESSION_DESCRIPTION_INIT_SDP_LEN
                          : MAX_SIGNALING_MESSAGE_LEN;

    MEMSET(signalingMessage.payload, 'A', testSize);
    signalingMessage.payload[testSize] = '\0';
    signalingMessage.payloadLen = testSize;
    status = signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage);
    if (status == STATUS_SUCCESS) {
        // Everything up to the SDK max works
        printf("\n[PROBE] Service accepts payload of %u bytes (SDK maximum). "
               "Service limit is >= SDK limit.\n",
               testSize);
        maxAccepted = testSize;
    } else {
        // Binary search for the actual limit
        high = testSize;
        printf("\n[PROBE] Service rejected payload of %u bytes. Binary searching...\n", high);

        while (low + 1 < high) {
            mid = (low + high) / 2;

            MEMSET(signalingMessage.payload, 'A', mid);
            signalingMessage.payload[mid] = '\0';
            signalingMessage.payloadLen = mid;
            status = signalingClientSendMessageSync(mSignalingClientHandle, &signalingMessage);

            if (status == STATUS_SUCCESS) {
                low = mid;
                printf("[PROBE]   %u bytes: ACCEPTED\n", mid);
            } else {
                high = mid;
                printf("[PROBE]   %u bytes: REJECTED (status: 0x%08x)\n", mid, status);
            }

            // Small delay to avoid throttling
            THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
        maxAccepted = low;
    }

    printf("\n[PROBE RESULT] Maximum accepted signaling payload size: %u bytes\n", maxAccepted);
    printf("[PROBE RESULT] This is the raw payload size BEFORE base64 encoding.\n");
    printf("[PROBE RESULT] After base64, the on-wire size is approximately: %u bytes\n", maxAccepted * 4 / 3);
    printf("\n");

    deinitializeSignalingClient();
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
