////////////////////////////////////////////////////////////////////
// Join Session Functionality Tests [AWS SDK Deps Required]
////////////////////////////////////////////////////////////////////
#ifdef ENABLE_AWS_SDK_IN_TESTS

#include "SignalingApiFunctionalityTest.h"
#include <aws/core/Aws.h>

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IngestionFunctionalityTest : public SignalingApiFunctionalityTest {
  public:
    IngestionFunctionalityTest()
    {
        Aws::InitAPI(options)
        joinSessionCount = 0;
        joinSessionFail = MAX_UINT32;
        joinSessionRecover = 0;
        joinSessionResult = STATUS_SUCCESS;
    }

    ~IngestionFunctionalityTest()
    {
        Aws::ShutdownAPI(options);
    }

  private:
    Aws::SDKOptions options;
    Aws::KinesisVideo::KinesisVideoClient kvsClient;
    Aws::KinesisVideoWebRTCStorage::KinesisVideoWebRTCStorageClient kvsWebRTCStorageClient;

    STATUS joinSessionResult;
    UINT32 joinSessionFail;
    UINT32 joinSessionRecover;
    UINT32 joinSessionCount;
};

STATUS joinSessionPreHook(UINT64 hookCustomData)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingApiFunctionalityTest* pTest = (SignalingApiFunctionalityTest*) hookCustomData;
    CHECK(pTest != NULL);

    if (pTest->joinSessionCount >= pTest->joinSessionFail && pTest->joinSessionCount < pTest->joinSessionRecover) {
        retStatus = pTest->joinSessionResult;
    }

    pTest->joinSessionCount++;
    DLOGD("Signaling client join session pre hook returning 0x%08x", retStatus);
    return retStatus;
};

TEST_F(SignalingApiFunctionalityTest, basicCreateConnectFreeNoJoinSession)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    ChannelInfo channelInfo;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfoInternal clientInfoInternal;
    SIGNALING_CLIENT_HANDLE signalingHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;

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
    EXPECT_EQ(STATUS_SUCCESS,
              createSignalingClientSync(&clientInfoInternal, &channelInfo, &signalingClientCallbacks, (PAwsCredentialProvider) mTestCredentialProvider,
                                        &signalingHandle));
    EXPECT_EQ(STATUS_SUCCESS,signalingClientFetchSync(signalingHandle));

    // Connect twice - the second time will be no-op
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(signalingHandle));

    // Validate the hook counts
    EXPECT_EQ(1, describeCount);
    EXPECT_EQ(1, describeMediaCount);
    EXPECT_EQ(1, getEndpointCount);
    EXPECT_EQ(1, connectCount);

    EXPECT_EQ(0, joinSessionCount);


    deleteChannelLws(FROM_SIGNALING_CLIENT_HANDLE(signalingHandle), 0);

    EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&signalingHandle));
}





#endif
