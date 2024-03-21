#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SignalingApiTest : public WebRtcClientTestBase {
};

TEST_F(SignalingApiTest, invalidRoleType_deserializeSignalingCacheEntries)
{
    FREMOVE(DEFAULT_CACHE_FILE_PATH);
    srand(GETTIME());

    BOOL cacheFound = FALSE;
    SignalingFileCacheEntry testEntry;
    MEMSET(&testEntry, 0x00, SIZEOF(testEntry));
    UINT32 serializedCacheEntryLen;
    char testWssEndpoint[MAX_SIGNALING_ENDPOINT_URI_LEN + 1] = {0};
    char testHttpsEndpoint[MAX_SIGNALING_ENDPOINT_URI_LEN + 1] = {0};
    char testWebrtcEndpoint[MAX_SIGNALING_ENDPOINT_URI_LEN + 1] = {0};
    char testRegion[MAX_REGION_NAME_LEN + 1] = {0};
    char testChannelArn[MAX_ARN_LEN + 1] = {0};
    char testChannel[MAX_CHANNEL_NAME_LEN + 1] = {0};
    UINT64 time = GETTIME() / HUNDREDS_OF_NANOS_IN_A_SECOND;
    int append = 0;
    CHAR serializedCacheEntry[MAX_SERIALIZED_SIGNALING_CACHE_ENTRY_LEN];


    const int TEST_CHANNEL_COUNT = 5;

    MEMSET(testWssEndpoint, 0, MAX_SIGNALING_ENDPOINT_URI_LEN+1);
    MEMSET(testHttpsEndpoint, 0, MAX_SIGNALING_ENDPOINT_URI_LEN+1);
    MEMSET(testRegion, 0, MAX_REGION_NAME_LEN + 1);
    MEMSET(testChannelArn, 0, MAX_ARN_LEN+1);
    MEMSET(testChannel, 0, MAX_CHANNEL_NAME_LEN+1);

    append = rand() % TEST_CHANNEL_COUNT;
    SNPRINTF(testWssEndpoint, SIZEOF(testWssEndpoint), "%s%d", "testWssEndpoint", append);
    SNPRINTF(testHttpsEndpoint, SIZEOF(testHttpsEndpoint),"%s%d", "testHttpsEndpoint", append);
    SNPRINTF(testRegion, SIZEOF(testRegion),"%s%d", "testRegion", append);
    SNPRINTF(testChannelArn, SIZEOF(testChannelArn),"%s%d", "testChannelArn", append);
    SNPRINTF(testChannel, SIZEOF(testChannel),"%s%d", "testChannel", append);

    EXPECT_EQ(STATUS_SUCCESS, createFileIfNotExist(DEFAULT_CACHE_FILE_PATH));

    serializedCacheEntryLen = SNPRINTF(serializedCacheEntry, ARRAY_SIZE(serializedCacheEntry), "%s,%s,%s,%s,%s,%s,%s,%s,%s,%.10" PRIu64 "\n", testChannelArn, SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR,
    testRegion, testChannelArn, testHttpsEndpoint, testWssEndpoint, "0", testChannelArn, testWebrtcEndpoint, time);
    EXPECT_EQ(STATUS_SUCCESS, writeFile(DEFAULT_CACHE_FILE_PATH, FALSE, TRUE, (PBYTE) serializedCacheEntry, serializedCacheEntryLen));

    EXPECT_EQ(STATUS_INVALID_ARG, signalingCacheLoadFromFile(testChannel, testRegion, SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN, &testEntry, &cacheFound, DEFAULT_CACHE_FILE_PATH));
    FREMOVE(DEFAULT_CACHE_FILE_PATH);
}

TEST_F(SignalingApiTest, createValidateChannelInfo)
{
    initializeSignalingClientStructs();
    PChannelInfo rChannelInfo;
    CHAR agentString[MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1];
    CHAR region[20];
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
    STRCPY(region, (PCHAR) "cn-north-1");
    mChannelInfo.pRegion = region;
    EXPECT_EQ(STATUS_SUCCESS, createValidateChannelInfo(&mChannelInfo, &rChannelInfo));
    EXPECT_PRED_FORMAT2(testing::IsSubstring, ".cn", rChannelInfo->pControlPlaneUrl);

    freeChannelInfo(&rChannelInfo);
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
    EXPECT_EQ(0, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_EQ(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_EQ(0, metrics.signalingClientStats.dpApiCallLatency);

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
    EXPECT_EQ(0, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_EQ(0, metrics.signalingClientStats.dpApiCallLatency);

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
    EXPECT_EQ(0, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_EQ(0, metrics.signalingClientStats.dpApiCallLatency);

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
    EXPECT_EQ(0, metrics.signalingClientStats.iceRefreshCount);
    EXPECT_NE(0, metrics.signalingClientStats.signalingClientUptime);
    EXPECT_NE(0, metrics.signalingClientStats.connectionDuration);
    EXPECT_NE(0, metrics.signalingClientStats.cpApiCallLatency);
    EXPECT_EQ(0, metrics.signalingClientStats.dpApiCallLatency);

    UINT32 iceCount = 0;
    //Get ice config
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceCount));

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
    //wait for threads of threadpool to close
    THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
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

TEST_F(SignalingApiTest, getChannelRoleTypeFromStringApi)
{
    EXPECT_EQ(SIGNALING_CHANNEL_ROLE_TYPE_MASTER, getChannelRoleTypeFromString(SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR, STRLEN(SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR)));
    EXPECT_EQ(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER, getChannelRoleTypeFromString(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR, STRLEN(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR)));
    EXPECT_EQ(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN, getChannelRoleTypeFromString(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR, STRLEN(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR)));
    EXPECT_EQ(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN, getChannelRoleTypeFromString((PCHAR) "test", STRLEN("test")));
}

TEST_F(SignalingApiTest, getStringFromChannelRoleTypeApi)
{
    EXPECT_STREQ(SIGNALING_CHANNEL_ROLE_TYPE_MASTER_STR, getStringFromChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE_MASTER));
    EXPECT_STREQ(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER_STR, getStringFromChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE_VIEWER));
    EXPECT_STREQ(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN_STR, getStringFromChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN));
}

TEST_F(SignalingApiTest, getChannelStatusFromStringApi)
{
    EXPECT_EQ(SIGNALING_CHANNEL_STATUS_ACTIVE, getChannelStatusFromString((PCHAR) "ACTIVE", STRLEN("ACTIVE")));
    EXPECT_EQ(SIGNALING_CHANNEL_STATUS_CREATING, getChannelStatusFromString((PCHAR) "CREATING", STRLEN("CREATING")));
    EXPECT_EQ(SIGNALING_CHANNEL_STATUS_UPDATING, getChannelStatusFromString((PCHAR) "UPDATING", STRLEN("UPDATING")));
    EXPECT_EQ(SIGNALING_CHANNEL_STATUS_DELETING, getChannelStatusFromString((PCHAR) "DELETING", STRLEN("DELETING")));
    EXPECT_EQ(SIGNALING_CHANNEL_STATUS_DELETING, getChannelStatusFromString((PCHAR) "test", STRLEN("test")));
}

TEST_F(SignalingApiTest, getSignalingStateFromStateMachineStateApi)
{
    EXPECT_EQ(SIGNALING_CLIENT_STATE_UNKNOWN, getSignalingStateFromStateMachineState(SIGNALING_STATE_NONE));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_NEW, getSignalingStateFromStateMachineState(SIGNALING_STATE_NEW));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_GET_CREDENTIALS, getSignalingStateFromStateMachineState(SIGNALING_STATE_GET_TOKEN));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_DESCRIBE, getSignalingStateFromStateMachineState(SIGNALING_STATE_DESCRIBE));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_CREATE, getSignalingStateFromStateMachineState(SIGNALING_STATE_CREATE));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_GET_ENDPOINT, getSignalingStateFromStateMachineState(SIGNALING_STATE_GET_ENDPOINT));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_GET_ICE_CONFIG, getSignalingStateFromStateMachineState(SIGNALING_STATE_GET_ICE_CONFIG));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_READY, getSignalingStateFromStateMachineState(SIGNALING_STATE_READY));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_CONNECTING, getSignalingStateFromStateMachineState(SIGNALING_STATE_CONNECT));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_CONNECTED, getSignalingStateFromStateMachineState(SIGNALING_STATE_CONNECTED));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_DISCONNECTED, getSignalingStateFromStateMachineState(SIGNALING_STATE_DISCONNECTED));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_DELETE, getSignalingStateFromStateMachineState(SIGNALING_STATE_DELETE));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_DELETED, getSignalingStateFromStateMachineState(SIGNALING_STATE_DELETED));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA, getSignalingStateFromStateMachineState(SIGNALING_STATE_DESCRIBE_MEDIA));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_JOIN_SESSION, getSignalingStateFromStateMachineState(SIGNALING_STATE_JOIN_SESSION));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING, getSignalingStateFromStateMachineState(SIGNALING_STATE_JOIN_SESSION_WAITING));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED, getSignalingStateFromStateMachineState(SIGNALING_STATE_JOIN_SESSION_CONNECTED));
}

TEST_F(SignalingApiTest, getMessageTypeInStringApiTest)
{
    EXPECT_STREQ(SIGNALING_SDP_TYPE_OFFER, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_OFFER));
    EXPECT_STREQ(SIGNALING_SDP_TYPE_ANSWER, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_ANSWER));
    EXPECT_STREQ(SIGNALING_ICE_CANDIDATE, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE));
    EXPECT_STREQ(SIGNALING_GO_AWAY, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_GO_AWAY));
    EXPECT_STREQ(SIGNALING_RECONNECT_ICE_SERVER, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER));
    EXPECT_STREQ(SIGNALING_STATUS_RESPONSE, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE));
    EXPECT_STREQ(SIGNALING_MESSAGE_UNKNOWN, getMessageTypeInString(SIGNALING_MESSAGE_TYPE_UNKNOWN));
    EXPECT_STREQ(SIGNALING_MESSAGE_UNKNOWN, getMessageTypeInString((SIGNALING_MESSAGE_TYPE) 10));
}

TEST_F(SignalingApiTest, getMessageTypeFromStringApiTest)
{
    SIGNALING_MESSAGE_TYPE messageType;
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_SDP_TYPE_OFFER, 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_OFFER);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_SDP_TYPE_ANSWER, STRLEN(SIGNALING_SDP_TYPE_ANSWER), &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_ANSWER);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_ICE_CANDIDATE, 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_GO_AWAY, 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_GO_AWAY);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_RECONNECT_ICE_SERVER, 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) SIGNALING_STATUS_RESPONSE, 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE);
    EXPECT_EQ(STATUS_SUCCESS, getMessageTypeFromString((PCHAR) "test", 0, &messageType));
    EXPECT_EQ(messageType, SIGNALING_MESSAGE_TYPE_UNKNOWN);
}

TEST_F(SignalingApiTest, getStringFromChannelTypeApiTest)
{
    EXPECT_STREQ(SIGNALING_CHANNEL_TYPE_SINGLE_MASTER_STR, getStringFromChannelType(SIGNALING_CHANNEL_TYPE_SINGLE_MASTER));
    EXPECT_STREQ(SIGNALING_CHANNEL_TYPE_UNKNOWN_STR, getStringFromChannelType((SIGNALING_CHANNEL_TYPE)0xffff));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
