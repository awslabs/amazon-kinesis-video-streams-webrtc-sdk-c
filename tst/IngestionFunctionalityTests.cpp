////////////////////////////////////////////////////////////////////
// Join Session Functionality Tests [AWS SDK Deps Required]
////////////////////////////////////////////////////////////////////
#ifdef ENABLE_AWS_SDK_IN_TESTS

#include "SignalingApiFunctionalityTest.h"
#include <aws/kinesisvideo/KinesisVideoClient.h>
#include <aws/kinesisvideo/model/CreateStreamRequest.h>
#include <aws/kinesisvideo/model/CreateSignalingChannelRequest.h>
#include <aws/kinesisvideo/model/DescribeSignalingChannelRequest.h>
#include <aws/kinesisvideo/model/DescribeStreamRequest.h>
#include <aws/kinesisvideo/model/DeleteStreamRequest.h>
#include <aws/kinesisvideo/model/DeleteSignalingChannelRequest.h>
#include <aws/kinesisvideo/model/UpdateMediaStorageConfigurationRequest.h>

#include <aws/kinesis-video-webrtc-storage/KinesisVideoWebRTCStorageClient.h>

#include <random>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IngestionFunctionalityTest : public SignalingApiFunctionalityTest {
  public:
    IngestionFunctionalityTest()
    {
        joinSessionCount = 0;
        joinSessionFail = MAX_UINT32;
        joinSessionRecover = 0;
        joinSessionResult = STATUS_SUCCESS;
    }

    STATUS joinSessionResult;
    UINT32 joinSessionFail;
    UINT32 joinSessionRecover;
    UINT32 joinSessionCount;

    typedef struct {
        std::string streamName;
        std::string streamArn;
        std::string channelName;
        std::string channelArn;
        BOOL enabledStatus;
        BOOL isValid;
    } MediaConfigurationInfo;

    MediaConfigurationInfo createStreamAndChannelAndLink();
    VOID UnlinkAndDeleteStreamAndChannel(MediaConfigurationInfo);

  private:
    Aws::KinesisVideo::KinesisVideoClient mKvsClient;
    Aws::KinesisVideoWebRTCStorage::KinesisVideoWebRTCStorageClient mKvsWebRTCStorageClient;
};

STATUS joinSessionPreHook(UINT64 hookCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    IngestionFunctionalityTest* pTest = (IngestionFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->joinSessionCount >= pTest->joinSessionFail && pTest->joinSessionCount < pTest->joinSessionRecover) {
        retStatus = pTest->joinSessionResult;
    }

    pTest->joinSessionCount++;
    DLOGD("Signaling client join session pre hook returning 0x%08x", retStatus);
    return retStatus;
}

unsigned char random_char() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    return static_cast<unsigned char>(dis(gen));
}

std::string generate_hex(const unsigned int len) {
    std::stringstream ss;
    for(unsigned int i = 0; i < len; i++) {
        auto rc = random_char();
        std::stringstream hexstream;
        hexstream << std::hex << int(rc);
        auto hex = hexstream.str();
        ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
}


// Create a Stream and a Channel and link
IngestionFunctionalityTest::MediaConfigurationInfo IngestionFunctionalityTest::createStreamAndChannelAndLink() {
    IngestionFunctionalityTest::MediaConfigurationInfo mediaConfigurationInfo;

    Aws::KinesisVideo::Model::CreateStreamOutcome createStreamOutcome;
    Aws::KinesisVideo::Model::CreateStreamRequest createStreamRequest;

    std::string nameSuffix = generate_hex(16);
    std::string streamName = "WrtcIngestionTestStream_" + nameSuffix;
    std::string channelName = "WrtcIngestionTestChannel_" + nameSuffix;
    createStreamRequest.WithStreamName(streamName);
    createStreamRequest.WithDataRetentionInHours(24);

    createStreamOutcome = mKvsClient.CreateStream(createStreamRequest);

    if (createStreamOutcome.IsSuccess()) {
        Aws::KinesisVideo::Model::CreateSignalingChannelOutcome createSignalingChannelOutcome;
        Aws::KinesisVideo::Model::CreateSignalingChannelRequest createSignalingChannelRequest;

        createSignalingChannelRequest.WithChannelName(channelName);

        createSignalingChannelOutcome = mKvsClient.CreateSignalingChannel(createSignalingChannelRequest);

        if (createSignalingChannelOutcome.IsSuccess()) {
            // UpdateMediaStorageConfiguration needs the ChannelARN and StreamARN.  We will call the following
            // APIs to get the needed ARNs
            // 1. DescribeSignalingChannel
            // 2. DescribeStream

            // long sleep to make sure the stream and channel are ready for use
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_SECOND);


            Aws::KinesisVideo::Model::DescribeSignalingChannelOutcome describeSignalingChannelOutcome;
            Aws::KinesisVideo::Model::DescribeSignalingChannelRequest describeSignalingChannelRequest;

            describeSignalingChannelRequest.WithChannelName(channelName);

            describeSignalingChannelOutcome = mKvsClient.DescribeSignalingChannel(describeSignalingChannelRequest);


            if (describeSignalingChannelOutcome.IsSuccess()) {
                const Aws::String channelARN = describeSignalingChannelOutcome.GetResult().GetChannelInfo().GetChannelARN();

                Aws::KinesisVideo::Model::DescribeStreamOutcome describeStreamOutcome;
                Aws::KinesisVideo::Model::DescribeStreamRequest describeStreamRequest;

                describeStreamRequest.WithStreamName(streamName);
                describeStreamOutcome = mKvsClient.DescribeStream(describeStreamRequest);

                if (describeStreamOutcome.IsSuccess()) {
                    const Aws::String streamARN = describeStreamOutcome.GetResult().GetStreamInfo().GetStreamARN();

                    Aws::KinesisVideo::Model::UpdateMediaStorageConfigurationOutcome updateMediaStorageConfigurationOutcome;
                    Aws::KinesisVideo::Model::UpdateMediaStorageConfigurationRequest updateMediaStorageConfigurationRequest;

                    Aws::KinesisVideo::Model::MediaStorageConfiguration mediaStorageConfiguration;
                    mediaStorageConfiguration.WithStreamARN(streamARN);
                    mediaStorageConfiguration.WithStatus(Aws::KinesisVideo::Model::MediaStorageConfigurationStatus::ENABLED);

                    updateMediaStorageConfigurationRequest.WithChannelARN(channelARN);
                    updateMediaStorageConfigurationRequest.WithMediaStorageConfiguration(mediaStorageConfiguration);

                    std::string cARN(channelARN.c_str(), channelARN.size());
                    std::string sARN(streamARN.c_str(), streamARN.size());


                    DLOGD("ChannelArn:  %s, StreamArn: %s", (PCHAR)cARN.c_str(), (PCHAR)sARN.c_str());

                    updateMediaStorageConfigurationOutcome = mKvsClient.UpdateMediaStorageConfiguration(updateMediaStorageConfigurationRequest);

                    if (updateMediaStorageConfigurationOutcome.IsSuccess()) {
                        mediaConfigurationInfo.channelName = channelName;
                        mediaConfigurationInfo.channelArn = channelARN;
                        mediaConfigurationInfo.streamName = streamName;
                        mediaConfigurationInfo.streamArn = streamARN;
                        mediaConfigurationInfo.enabledStatus = TRUE;
                        mediaConfigurationInfo.isValid = TRUE;
                        return mediaConfigurationInfo;
                    } else {
                        DLOGE("Update Media Storage Configuration FAILED.  %s", updateMediaStorageConfigurationOutcome.GetError().GetMessage().c_str());
                    }
                }
            }
        } else {
            DLOGE("Creating Signaling Channel FAILED");
        }
    } else {
        DLOGE("Creating Stream FAILED");
    }


    mediaConfigurationInfo.isValid = FALSE;

    return mediaConfigurationInfo;
}


VOID IngestionFunctionalityTest::UnlinkAndDeleteStreamAndChannel(IngestionFunctionalityTest::MediaConfigurationInfo mediaConfigurationInfo) {
    Aws::KinesisVideo::Model::UpdateMediaStorageConfigurationOutcome updateMediaStorageConfigurationOutcome;
    Aws::KinesisVideo::Model::UpdateMediaStorageConfigurationRequest updateMediaStorageConfigurationRequest;

    Aws::KinesisVideo::Model::MediaStorageConfiguration mediaStorageConfiguration;
    mediaStorageConfiguration.WithStreamARN(mediaConfigurationInfo.streamArn);
    mediaStorageConfiguration.WithStatus(Aws::KinesisVideo::Model::MediaStorageConfigurationStatus::DISABLED);

    updateMediaStorageConfigurationRequest.WithChannelARN(mediaConfigurationInfo.channelArn);
    updateMediaStorageConfigurationRequest.WithMediaStorageConfiguration(mediaStorageConfiguration);

    updateMediaStorageConfigurationOutcome = mKvsClient.UpdateMediaStorageConfiguration(updateMediaStorageConfigurationRequest);

    if (!updateMediaStorageConfigurationOutcome.IsSuccess()) {
        DLOGE("Update Media Storage Configuration FAILED! %s", updateMediaStorageConfigurationOutcome.GetError().GetMessage().c_str());
    }

    Aws::KinesisVideo::Model::DeleteSignalingChannelOutcome deleteSignalingChannelOutcome;
    Aws::KinesisVideo::Model::DeleteSignalingChannelRequest deleteSignalingChannelRequest;

    deleteSignalingChannelRequest.WithChannelARN(mediaConfigurationInfo.channelArn);

    deleteSignalingChannelOutcome = mKvsClient.DeleteSignalingChannel(deleteSignalingChannelRequest);

    if (!deleteSignalingChannelOutcome.IsSuccess()) {
        DLOGE("Delete Signaling Channel (%s) FAILED! %s", (PCHAR)mediaConfigurationInfo.channelArn.c_str(), deleteSignalingChannelOutcome.GetError().GetMessage().c_str());
    }

    Aws::KinesisVideo::Model::DeleteStreamOutcome deleteStreamOutcome;
    Aws::KinesisVideo::Model::DeleteStreamRequest deleteStreamRequest;

    deleteStreamRequest.WithStreamARN(mediaConfigurationInfo.streamArn);

    deleteStreamOutcome = mKvsClient.DeleteStream(deleteStreamRequest);

    if (!deleteStreamOutcome.IsSuccess()) {
        DLOGE("Delete Stream (%s) FAILED!  %s", (PCHAR)mediaConfigurationInfo.streamArn.c_str(), deleteStreamOutcome.GetError().GetMessage().c_str());
    }

}


TEST_F(IngestionFunctionalityTest, basicCreateConnectFreeNoJoinSession)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    PSignalingClient pSignalingClient;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.getCurrentTimeFn = NULL;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;
    clientInfoInternal.describeMediaStorageConfPreHookFn = describeMediaPreHook;
    clientInfoInternal.joinSessionPreHookFn = joinSessionPreHook;
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

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

    channelInfo.useMediaStorage = TRUE;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &pSignalingClient));

    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));
    pActiveClient = pSignalingClient;

    // Connect twice - the second time will be no-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Validate the hook counts
    EXPECT_EQ(2, describeCount);
    EXPECT_EQ(1, describeMediaCount);
    EXPECT_EQ(1, getEndpointCount);
    EXPECT_EQ(1, connectCount);

    // This channel does not have an associated stream
    EXPECT_EQ(0, joinSessionCount);

    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}


/* 1. Create a Stream
 * 2. Create a Signaling channel
 * 3. Link the Stream with the Signaling Channel
 * 4. Step To Connect (which should invoke join session and if offer received to join session connected state
 * 5. Un link and delete stream and signaling channel
*/
TEST_F(IngestionFunctionalityTest, basicCreateConnectJoinSession)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    SignalingClientMetrics signalingClientMetrics;
    signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    MediaConfigurationInfo mediaConfigurationInfo = createStreamAndChannelAndLink();
    ASSERT_EQ(TRUE, mediaConfigurationInfo.isValid);
    ASSERT_EQ(TRUE, mediaConfigurationInfo.enabledStatus);


    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    PSignalingClient pSignalingClient;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.getCurrentTimeFn = NULL;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;
    clientInfoInternal.describeMediaStorageConfPreHookFn = describeMediaPreHook;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;

    clientInfoInternal.joinSessionPreHookFn = joinSessionPreHook;
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = (PCHAR)mediaConfigurationInfo.channelName.c_str();
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
    channelInfo.useMediaStorage = TRUE;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));

    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));
    pActiveClient = pSignalingClient;

    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Validate the hook counts
    EXPECT_EQ(1, describeCount);
    EXPECT_EQ(1, describeMediaCount);
    EXPECT_EQ(1, getEndpointCount);
    EXPECT_EQ(1, getIceConfigCount);

    EXPECT_EQ(1, connectCount);

    // This channel has ENABLED status so we should be calling join session
    EXPECT_EQ(1, joinSessionCount);

    UnlinkAndDeleteStreamAndChannel(mediaConfigurationInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}


TEST_F(IngestionFunctionalityTest, iceReconnectEmulationWithJoinSession)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    MediaConfigurationInfo mediaConfigurationInfo = createStreamAndChannelAndLink();
    ASSERT_EQ(TRUE, mediaConfigurationInfo.isValid);
    ASSERT_EQ(TRUE, mediaConfigurationInfo.enabledStatus);


    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    SignalingClientInfoInternal clientInfoInternal;

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.getCurrentTimeFn = NULL;


    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = LOG_LEVEL_VERBOSE;
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;
    clientInfoInternal.getIceConfigPreHookFn = getIceConfigPreHook;
    clientInfoInternal.describeMediaStorageConfPreHookFn = describeMediaPreHook;
    clientInfoInternal.joinSessionPreHookFn = joinSessionPreHook;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = (PCHAR)mediaConfigurationInfo.channelName.c_str();
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
    channelInfo.useMediaStorage = TRUE;

    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                  &pSignalingClient));

    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));

    pSignalingClient = FROM_SIGNALING_CLIENT_HANDLE(signalingHandle);
    pActiveClient = pSignalingClient;

    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));
    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Validate the hook counts
    EXPECT_EQ(1, describeCount);
    EXPECT_EQ(1, describeMediaCount);
    EXPECT_EQ(1, getEndpointCount);
    EXPECT_EQ(1, getIceConfigCount);
    EXPECT_EQ(1, connectCount);

    // This channel has ENABLED status so we should be calling join session
    EXPECT_EQ(1, joinSessionCount);

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

    // Validate the hook counts, caching is disabled in this test, but reconnect should move to
    // Get Ice Server config
    EXPECT_EQ(1, describeCount);
    EXPECT_EQ(1, describeMediaCount);
    EXPECT_EQ(1, getEndpointCount);
    EXPECT_EQ(2, getIceConfigCount);
    EXPECT_EQ(2, connectCount);

    // This channel has ENABLED status so we should be calling join session
    EXPECT_EQ(2, joinSessionCount);

    UnlinkAndDeleteStreamAndChannel(mediaConfigurationInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}


TEST_F(IngestionFunctionalityTest, iceServerConfigRefreshNotConnectedJoinSessionWithBadAuth)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    MediaConfigurationInfo mediaConfigurationInfo = createStreamAndChannelAndLink();
    ASSERT_EQ(TRUE, mediaConfigurationInfo.isValid);
    ASSERT_EQ(TRUE, mediaConfigurationInfo.enabledStatus);

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
    signalingClientCallbacks.getCurrentTimeFn = NULL;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = (PCHAR)mediaConfigurationInfo.channelName.c_str();
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
    channelInfo.useMediaStorage = TRUE;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    pActiveClient = pSignalingClient;

    // Check the states first, we did not connect yet
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Set bad auth info
    BYTE firstByte = pSignalingClient->pAwsCredentials->secretKey[0];
    if(firstByte == 'A') {
        pSignalingClient->pAwsCredentials->secretKey[0] = 'B';
    } else {
        pSignalingClient->pAwsCredentials->secretKey[0] = 'A';
    }

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Reset the auth and ensure we succeed this time
    pSignalingClient->pAwsCredentials->secretKey[0] = firstByte;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Connect to the signaling client
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);

    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
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

    UnlinkAndDeleteStreamAndChannel(mediaConfigurationInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}


TEST_F(IngestionFunctionalityTest, iceServerConfigRefreshConnectedJoinSessionWithBadAuth)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    MediaConfigurationInfo mediaConfigurationInfo = createStreamAndChannelAndLink();
    ASSERT_EQ(TRUE, mediaConfigurationInfo.isValid);
    ASSERT_EQ(TRUE, mediaConfigurationInfo.enabledStatus);

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
    signalingClientCallbacks.getCurrentTimeFn = NULL;


    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

    MEMSET(&channelInfo, 0x00, SIZEOF(ChannelInfo));
    channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    channelInfo.pChannelName = (PCHAR)mediaConfigurationInfo.channelName.c_str();
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
    channelInfo.useMediaStorage = TRUE;

    EXPECT_EQ(STATUS_SUCCESS, createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider, &pSignalingClient));
    signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
    EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));

    // Connect to the channel, will call join session
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    pActiveClient = pSignalingClient;

    // Check the states first
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);

    // We should not be calling create because it's pre-created at the start of the test
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Set bad auth info
    BYTE firstByte = pSignalingClient->pAwsCredentials->secretKey[0];
    if(firstByte == 'A') {
        pSignalingClient->pAwsCredentials->secretKey[0] = 'B';
    } else {
        pSignalingClient->pAwsCredentials->secretKey[0] = 'A';
    }

    // Trigger the ICE refresh on the next call
    pSignalingClient->iceConfigCount = 0;
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // This time the states will circle through connecting/connected again
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_DISCONNECTED]);

    // Reset the auth and ensure we succeed this time
    pSignalingClient->pAwsCredentials->secretKey[0] = firstByte;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(signalingHandle, &iceCount));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(signalingHandle, 0, &pIceConfigInfo));

    // Connect to the signaling client (already connected -- but we will call join session again)
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_NEW]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_CREDENTIALS]);
    EXPECT_LT(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_DESCRIBE_MEDIA]);
    EXPECT_EQ(0, signalingStatesCounts[SIGNALING_CLIENT_STATE_CREATE]);
    EXPECT_EQ(1, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ENDPOINT]);
    EXPECT_LT(2, signalingStatesCounts[SIGNALING_CLIENT_STATE_GET_ICE_CONFIG]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_READY]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTING]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_CONNECTED]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_WAITING]);
    EXPECT_EQ(3, signalingStatesCounts[SIGNALING_CLIENT_STATE_JOIN_SESSION_CONNECTED]);
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

    UnlinkAndDeleteStreamAndChannel(mediaConfigurationInfo);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}


TEST_F(IngestionFunctionalityTest, fileCachingTestWithDescribeMedia)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    PSignalingClient pSignalingClient;
    SIGNALING_CLIENT_HANDLE signalingHandle;
    CHAR signalingChannelName[64];
    const UINT32 totalChannelCount = MAX_SIGNALING_CACHE_ENTRY_COUNT + 1;
    UINT32 i, describeCountNoCache, describeMediaCountNoCache, getEndpointCountNoCache;
    CHAR channelArn[MAX_ARN_LEN + 1];

    signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    signalingClientCallbacks.customData = (UINT64) this;
    signalingClientCallbacks.messageReceivedFn = NULL;
    signalingClientCallbacks.errorReportFn = signalingClientError;
    signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    signalingClientCallbacks.getCurrentTimeFn = NULL;

    MEMSET(&clientInfoInternal, 0x00, SIZEOF(SignalingClientInfoInternal));

    clientInfoInternal.signalingClientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    clientInfoInternal.signalingClientInfo.loggingLevel = mLogLevel;
    STRCPY(clientInfoInternal.signalingClientInfo.clientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    clientInfoInternal.hookCustomData = (UINT64) this;
    clientInfoInternal.connectPreHookFn = connectPreHook;
    clientInfoInternal.describePreHookFn = describePreHook;
    clientInfoInternal.describeMediaStorageConfPreHookFn = describeMediaPreHook;
    clientInfoInternal.getEndpointPreHookFn = getEndpointPreHook;
    setupSignalingStateMachineRetryStrategyCallbacks(&clientInfoInternal);

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
    channelInfo.useMediaStorage = TRUE;


    FREMOVE(DEFAULT_CACHE_FILE_PATH);

    for (i = 0; i < totalChannelCount; ++i) {
        SNPRINTF(signalingChannelName, SIZEOF(signalingChannelName), "%s%u", TEST_SIGNALING_CHANNEL_NAME, i);
        channelInfo.pChannelName = signalingChannelName;
        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient));
        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    }

    describeCountNoCache = describeCount;
    describeMediaCountNoCache = describeMediaCount;
    getEndpointCountNoCache = getEndpointCount;

    for (i = 0; i < totalChannelCount; ++i) {
        SNPRINTF(signalingChannelName, SIZEOF(signalingChannelName), "%s%u", TEST_SIGNALING_CHANNEL_NAME, i);
        channelInfo.pChannelName = signalingChannelName;
        channelInfo.pChannelArn = NULL;
        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient))
            << "Failed on channel name: " << channelInfo.pChannelName;

        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));

        // Store the channel ARN to be used later
        STRCPY(channelArn, pSignalingClient->channelDescription.channelArn);

        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));

        // Repeat the same with the ARN only
        channelInfo.pChannelName = NULL;
        channelInfo.pChannelArn = channelArn;

        EXPECT_EQ(STATUS_SUCCESS,
                  createSignalingSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                      &pSignalingClient));

        signalingHandle = TO_SIGNALING_CLIENT_HANDLE(pSignalingClient);
        EXPECT_TRUE(IS_VALID_SIGNALING_CLIENT_HANDLE(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, signalingClientFetchSync(signalingHandle));
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
    }

    DLOGD("describeCount: %d, describeCountNoCache: %d", describeCount, describeCountNoCache);
    DLOGD("describeMediaCount: %d, describeMediaCountNoCache: %d", describeMediaCount, describeMediaCountNoCache);
    DLOGD("getEndpointCount: %d, getEndpointCountNoCache: %d", getEndpointCount, getEndpointCountNoCache);

    /* describeCount and getEndpointCount should only increase by 2 because they are cached for all channels except one and we iterate twice*/
    EXPECT_TRUE(describeCount > describeCountNoCache && (describeCount - describeCountNoCache) == 2);
    EXPECT_TRUE(describeMediaCount > describeMediaCountNoCache && (describeMediaCount - describeMediaCountNoCache) == 2);
    EXPECT_TRUE(getEndpointCount > getEndpointCountNoCache && (getEndpointCount - getEndpointCountNoCache) == 2);
}



} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
