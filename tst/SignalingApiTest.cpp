#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class SignalingApiTest : public WebRtcClientTestBase {
public:
    SignalingApiTest() : mSignalingClientHandle(INVALID_SIGNALING_CLIENT_HANDLE_VALUE)
    {}

    STATUS initialize() {
        ChannelInfo channelInfo;
        SignalingClientCallbacks signalingClientCallbacks;
        SignalingClientInfo clientInfo;
        PSignalingClient pSignalingClient;
        Tag tags[3];
        STATUS retStatus;

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
        signalingClientCallbacks.messageReceivedFn = NULL;
        signalingClientCallbacks.errorReportFn = NULL;
        signalingClientCallbacks.stateChangeFn = NULL;

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

        retStatus = createSignalingClientSync(&clientInfo, &channelInfo, &signalingClientCallbacks,
                (PAwsCredentialProvider) mTestCredentialProvider,
                &mSignalingClientHandle);

        if (mAccessKeyIdSet) {
            EXPECT_EQ(STATUS_SUCCESS, retStatus);
        } else {
            EXPECT_NE(STATUS_SUCCESS, retStatus);
        }

        return retStatus;
    }

    STATUS deinitialize() {
        EXPECT_EQ(STATUS_SUCCESS, freeSignalingClient(&mSignalingClientHandle));

        return STATUS_SUCCESS;
    }

    SIGNALING_CLIENT_HANDLE mSignalingClientHandle;
};

TEST_F(SignalingApiTest, signalingSendMessageSync)
{
    STATUS expectedStatus;
    SignalingMessage signalingMessage;

    initialize();

    signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRCPY(signalingMessage.peerClientId, TEST_SIGNALING_MASTER_CLIENT_ID);
    MEMSET(signalingMessage.payload, 'A', 100);
    signalingMessage.payload[100] = '\0';
    signalingMessage.payloadLen = 0;

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

    deinitialize();
}

TEST_F(SignalingApiTest, signalingClientConnectSync)
{
    STATUS expectedStatus;

    initialize();
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE));
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));

    // Connect again
    expectedStatus = mAccessKeyIdSet ? STATUS_INVALID_STREAM_STATE : STATUS_NULL_ARG;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));


    deinitialize();
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfoCout)
{
    STATUS expectedStatus;
    UINT32 count;

    initialize();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCout(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &count));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCout(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfoCout(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));

    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientGetIceConfigInfoCout(mSignalingClientHandle, &count));
    if (mAccessKeyIdSet) {
        EXPECT_NE(0, count);
        EXPECT_GE(MAX_ICE_CONFIG_COUNT, count);
    }

    deinitialize();
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfo)
{
    UINT32 i, j, count;
    PIceConfigInfo pIceConfigInfo;

    initialize();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, 0, &pIceConfigInfo));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, 0, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetIceConfigInfo(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, 0, NULL));

    if (mAccessKeyIdSet) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCout(mSignalingClientHandle, &count));
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

    deinitialize();
}

}
}
}
}
}
