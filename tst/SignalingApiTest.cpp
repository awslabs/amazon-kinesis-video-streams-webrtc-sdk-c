#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class SignalingApiTest : public WebRtcClientTestBase {
};

TEST_F(SignalingApiTest, signalingSendMessageSync)
{
    if (!mAccessKeyIdSet) {
        return;
    }

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
}

TEST_F(SignalingApiTest, signalingClientConnectSync)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    STATUS expectedStatus;

    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientConnectSync(INVALID_SIGNALING_CLIENT_HANDLE_VALUE));
    expectedStatus = mAccessKeyIdSet ? STATUS_SUCCESS : STATUS_NULL_ARG;
    EXPECT_EQ(expectedStatus, signalingClientConnectSync(mSignalingClientHandle));

    // Connect again
    expectedStatus = mAccessKeyIdSet ? STATUS_INVALID_STREAM_STATE : STATUS_NULL_ARG;
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));
    EXPECT_EQ(STATUS_SUCCESS, signalingClientConnectSync(mSignalingClientHandle));

    deinitializeSignalingClient();
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfoCount)
{
    if (!mAccessKeyIdSet) {
        return;
    }

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
}

TEST_F(SignalingApiTest, signalingClientGetIceConfigInfo)
{
    if (!mAccessKeyIdSet) {
        return;
    }

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
}

TEST_F(SignalingApiTest, signalingClientGetCurrentState)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    SIGNALING_CLIENT_STATE state;
    initializeSignalingClient();
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, &state));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(mSignalingClientHandle, NULL));
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetCurrentState(INVALID_SIGNALING_CLIENT_HANDLE_VALUE, NULL));

    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetCurrentState(mSignalingClientHandle, &state));
    EXPECT_EQ(SIGNALING_CLIENT_STATE_READY, state);

    deinitializeSignalingClient();
}

TEST_F(SignalingApiTest, signalingClientGetStateString)
{
    EXPECT_NE(STATUS_SUCCESS, signalingClientGetStateString(SIGNALING_CLIENT_STATE_UNKNOWN, NULL));

    for (UINT32 i = 0; i <= (UINT32) SIGNALING_CLIENT_STATE_MAX_VALUE + 1; i++) {
        PCHAR pStateStr;
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetStateString((SIGNALING_CLIENT_STATE) i, &pStateStr));
        DLOGI("Iterating states \"%s\"", pStateStr);
    }
}

}
}
}
}
}
