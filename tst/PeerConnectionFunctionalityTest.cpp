#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class PeerConnectionFunctionalityTest : public WebRtcClientTestBase {
};

// Assert that two PeerConnections can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeers)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Assert that two PeerConnections with forced TURN can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersForcedTURN)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    EXPECT_EQ(signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount), STATUS_SUCCESS);

    // Set the  STUN server
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        EXPECT_EQ(signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo), STATUS_SUCCESS);
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(configuration.iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(configuration.iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(configuration.iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
    }


    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

// Assert that two PeerConnections with host and stun candidate can go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersWithHostAndStun)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set the  STUN server
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Assert that two PeerConnections can connect and then termintate one of them, the other one will eventually report disconnection
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersThenDisconnectTest)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // free offerPc so it wont send anymore keep alives and answerPc will detect disconnection
    freePeerConnection(&offerPc);

    THREAD_SLEEP(KVS_ICE_ENTER_STATE_DISCONNECTION_GRACE_PERIOD);

    for (i = 0; i < 10; ++i) {
        if (ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_DISCONNECTED]) > 0) {
            break;
        }

        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_DISCONNECTED]) > 0);

    freePeerConnection(&answerPc);
}

// Assert that PeerConnection will go to failed state when no turn server was given in turn only mode.
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersExpectFailureBecauseNoCandidatePair)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), FALSE);

    // give time for to gathering to time out.
    THREAD_SLEEP(KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT);
    EXPECT_TRUE(ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_FAILED]) == 2);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Assert that two PeerConnections can connect and then send media until the receiver gets both audio/video
TEST_F(PeerConnectionFunctionalityTest, exchangeMedia)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    // Create track and transceiver and adds to PeerConnection
    auto addTrackToPeerConnection = [](PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track, PRtcRtpTransceiver *transceiver, RTC_CODEC codec, MEDIA_STREAM_TRACK_KIND kind) -> void {

        MEMSET(track, 0x00, SIZEOF(RtcMediaStreamTrack));

        EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, codec), STATUS_SUCCESS);

        track->kind = kind;
        track->codec = codec;
        EXPECT_EQ(generateJSONSafeString(track->streamId, MAX_MEDIA_STREAM_ID_LEN), STATUS_SUCCESS);
        EXPECT_EQ(generateJSONSafeString(track->trackId, MAX_MEDIA_STREAM_ID_LEN), STATUS_SUCCESS);

        EXPECT_EQ(addTransceiver(pRtcPeerConnection, track, NULL, transceiver), STATUS_SUCCESS);
    };

    auto const frameBufferSize = 200000;

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, answerVideoTrack, offerAudioTrack, answerAudioTrack;
    PRtcRtpTransceiver offerVideoTransceiver, answerVideoTransceiver, offerAudioTransceiver, answerAudioTransceiver;
    SIZE_T seenVideo = 0;
    Frame videoFrame;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&videoFrame, 0x00, SIZEOF(Frame));

    videoFrame.frameData = (PBYTE) MEMALLOC(frameBufferSize);
    videoFrame.size = frameBufferSize;

    EXPECT_EQ(readFrameData(videoFrame.frameData, &(videoFrame.size), 1, (PCHAR) "../samples/h264SampleFrames"), STATUS_SUCCESS);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver,RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver,RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
    addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(answerPc, &answerAudioTrack, &answerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);

    auto onFrameHandler = [](UINT64 customData, PFrame pFrame) -> void {
        UNUSED_PARAM(pFrame);
        ATOMIC_STORE((PSIZE_T) customData, 1);
    };
    EXPECT_EQ(transceiverOnFrame(answerVideoTransceiver, (UINT64) &seenVideo, onFrameHandler), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    for (auto i = 0; i <= 1000 && ATOMIC_LOAD(&seenVideo) != 1; i++) {
        EXPECT_EQ(writeFrame(offerVideoTransceiver, &videoFrame), STATUS_SUCCESS);
        videoFrame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 25);

        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    MEMFREE(videoFrame.frameData);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    EXPECT_EQ(ATOMIC_LOAD(&seenVideo), 1);
}


}
}
}
}
}
