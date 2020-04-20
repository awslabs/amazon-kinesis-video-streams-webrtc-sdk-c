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

TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersWithPresetCerts)
{
    RtcConfiguration offerConfig, answerConfig;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    X509* pOfferCert = NULL;
    X509* pAnswerCert = NULL;
    EVP_PKEY* pOfferKey = NULL;
    EVP_PKEY* pAnswerKey = NULL;
    CHAR offerCertFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];
    CHAR answerCertFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];

    // Generate offer cert
    ASSERT_EQ(STATUS_SUCCESS, createCertificateAndKey(GENERATED_CERTIFICATE_BITS, true, &pOfferCert, &pOfferKey));
    ASSERT_EQ(STATUS_SUCCESS, dtlsCertificateFingerprint(pOfferCert, offerCertFingerprint));

    // Generate answer cert
    ASSERT_EQ(STATUS_SUCCESS, createCertificateAndKey(GENERATED_CERTIFICATE_BITS, true, &pAnswerCert, &pAnswerKey));
    ASSERT_EQ(STATUS_SUCCESS, dtlsCertificateFingerprint(pAnswerCert, answerCertFingerprint));

    MEMSET(&offerConfig, 0x00, SIZEOF(RtcConfiguration));
    offerConfig.certificates[0].pCertificate = (PBYTE) pOfferCert;
    offerConfig.certificates[0].certificateSize = 0;
    offerConfig.certificates[0].pPrivateKey = (PBYTE) pOfferKey;
    offerConfig.certificates[0].privateKeySize = 0;

    MEMSET(&answerConfig, 0x00, SIZEOF(RtcConfiguration));
    answerConfig.certificates[0].pCertificate = (PBYTE) pAnswerCert;
    answerConfig.certificates[0].certificateSize = 0;
    answerConfig.certificates[0].pPrivateKey = (PBYTE) pAnswerKey;
    answerConfig.certificates[0].privateKeySize = 0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&offerConfig, &offerPc));
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&answerConfig, &answerPc));

    // Should be fine to free right after create peer connection
    freeCertificateAndKey(&pOfferCert, &pOfferKey);
    freeCertificateAndKey(&pAnswerCert, &pAnswerKey);

    EXPECT_EQ(TRUE, connectTwoPeers(offerPc, answerPc, offerCertFingerprint, answerCertFingerprint));

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

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, freeTurnDueToP2PFoundBeforeTurnEstablished)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_TRUE(((PKvsPeerConnection)offerPc)->pIceAgent->turnConnectionTrackerCount == 0);
    EXPECT_TRUE(((PKvsPeerConnection)answerPc)->pIceAgent->turnConnectionTrackerCount == 0);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, freeTurnDueToP2PFoundAfterTurnEstablished)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcSessionDescriptionInit sdp;
    SIZE_T offerPcDoneGatherCandidate = 0, answerPcDoneGatherCandidate = 0;
    UINT64 candidateGatherTimeout;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        PSIZE_T pDoneGatherCandidate = (PSIZE_T) customData;
        if (candidateStr == NULL) {
            ATOMIC_STORE(pDoneGatherCandidate, 1);
        }
    };

    EXPECT_EQ(peerConnectionOnIceCandidate(offerPc, (UINT64) &offerPcDoneGatherCandidate, onICECandidateHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnIceCandidate(answerPc, (UINT64) &answerPcDoneGatherCandidate, onICECandidateHdlr), STATUS_SUCCESS);

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        ATOMIC_INCREMENT((PSIZE_T)customData + newState);
    };

    EXPECT_EQ(peerConnectionOnConnectionStateChange(offerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnConnectionStateChange(answerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);

    // start gathering candidates
    EXPECT_EQ(setLocalDescription(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(answerPc, &sdp), STATUS_SUCCESS);

    // give time for turn allocation to be finished
    candidateGatherTimeout = GETTIME() + KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT + 2 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!(ATOMIC_LOAD(&offerPcDoneGatherCandidate) > 0 && ATOMIC_LOAD(&answerPcDoneGatherCandidate) > 0) &&
            GETTIME() < candidateGatherTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(ATOMIC_LOAD(&offerPcDoneGatherCandidate) > 0);
    EXPECT_TRUE(ATOMIC_LOAD(&answerPcDoneGatherCandidate) > 0);

    EXPECT_EQ(createOffer(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionGetCurrentLocalDescription(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(answerPc, &sdp), STATUS_SUCCESS);

    EXPECT_EQ(createAnswer(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionGetCurrentLocalDescription(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(offerPc, &sdp), STATUS_SUCCESS);

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) == 2);

    // give time for turn allocated to be freed
    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_TRUE(((PKvsPeerConnection)offerPc)->pIceAgent->turnConnectionTrackerCount == 0);
    EXPECT_TRUE(((PKvsPeerConnection)answerPc)->pIceAgent->turnConnectionTrackerCount == 0);

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

// Assert that two PeerConnections can connect and then terminate one of them, the other one will eventually report disconnection
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
    videoFrame.size = TEST_VIDEO_FRAME_SIZE;
    MEMSET(videoFrame.frameData, 0x11, videoFrame.size);

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

// Same test as exchangeMedia, but assert that if one side is RSA DTLS and Key Extraction works
TEST_F(PeerConnectionFunctionalityTest, exchangeMediaRSA)
{
    if (!mAccessKeyIdSet) {
        return;
    }

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
    videoFrame.size = TEST_VIDEO_FRAME_SIZE;
    MEMSET(videoFrame.frameData, 0x11, videoFrame.size);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    configuration.kvsRtcConfiguration.generateRSACertificate = TRUE;
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

TEST_F(PeerConnectionFunctionalityTest, DISABLED_exchangeMediaThroughTurnRandomStop)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    initializeSignalingClient();

    auto repeatedStreamingRandomStop = [this](int iteration, int maxStreamingDurationMs, int minStreamingDurationMs, bool expectSeenVideo) -> void
    {
        auto const frameBufferSize = 200000;
        Frame videoFrame;
        PRtcPeerConnection offerPc = NULL, answerPc = NULL;
        RtcMediaStreamTrack offerVideoTrack, answerVideoTrack, offerAudioTrack, answerAudioTrack;
        PRtcRtpTransceiver offerVideoTransceiver, answerVideoTransceiver, offerAudioTransceiver, answerAudioTransceiver;
        ATOMIC_BOOL offerSeenVideo = 0, answerSeenVideo = 0, offerStopVideo = 0, answerStopVideo = 0;
        UINT64 streamingTimeMs;
        RtcConfiguration configuration;

        MEMSET(&videoFrame, 0x00, SIZEOF(Frame));
        videoFrame.frameData = (PBYTE) MEMALLOC(frameBufferSize);
        videoFrame.size = TEST_VIDEO_FRAME_SIZE;
        MEMSET(videoFrame.frameData, 0x11, videoFrame.size);

        for(int i = 0; i < iteration; ++i) {
            MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
            configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
            getIceServers(&configuration);

            EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
            EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

            addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver,RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
            addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver,RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
            addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
            addTrackToPeerConnection(answerPc, &answerAudioTrack, &answerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);

            auto onFrameHandler = [](UINT64 customData, PFrame pFrame) -> void {
                UNUSED_PARAM(pFrame);
                ATOMIC_STORE_BOOL((PSIZE_T) customData, TRUE);
            };
            EXPECT_EQ(transceiverOnFrame(offerVideoTransceiver, (UINT64) &offerSeenVideo, onFrameHandler), STATUS_SUCCESS);
            EXPECT_EQ(transceiverOnFrame(answerVideoTransceiver, (UINT64) &answerSeenVideo, onFrameHandler), STATUS_SUCCESS);

            MEMSET(stateChangeCount, 0x00, SIZEOF(stateChangeCount));
            EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

            streamingTimeMs = (UINT64) (RAND() % (maxStreamingDurationMs - minStreamingDurationMs)) + minStreamingDurationMs;
            DLOGI("Stop streaming after %u milliseconds.", streamingTimeMs);

            auto sendVideoWorker = [](PRtcRtpTransceiver pRtcRtpTransceiver, Frame frame, PSIZE_T pTerminationFlag) -> void {
                while(!ATOMIC_LOAD_BOOL(pTerminationFlag)) {
                    EXPECT_EQ(writeFrame(pRtcRtpTransceiver, &frame), STATUS_SUCCESS);
                    // frame was copied by value
                    frame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 25);

                    THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                }
            };

            std::thread offerSendVideoWorker(sendVideoWorker, offerVideoTransceiver, videoFrame, &offerStopVideo);
            std::thread answerSendVideoWorker(sendVideoWorker, answerVideoTransceiver, videoFrame, &answerStopVideo);

            std::this_thread::sleep_for(std::chrono::milliseconds(streamingTimeMs));

            ATOMIC_STORE_BOOL(&offerStopVideo, TRUE);
            offerSendVideoWorker.join();
            freePeerConnection(&offerPc);

            ATOMIC_STORE_BOOL(&answerStopVideo, TRUE);
            answerSendVideoWorker.join();
            freePeerConnection(&answerPc);

            if (expectSeenVideo) {
                EXPECT_EQ(ATOMIC_LOAD_BOOL(&offerSeenVideo), TRUE);
                EXPECT_EQ(ATOMIC_LOAD_BOOL(&answerSeenVideo), TRUE);
            }
        }

        MEMFREE(videoFrame.frameData);
    };

    // Repeated steaming and stop at random times to catch potential deadlocks involving iceAgent and TurnConnection
    repeatedStreamingRandomStop(30, 5000, 1000, TRUE);
    repeatedStreamingRandomStop(30, 1000, 500, FALSE);

    deinitializeSignalingClient();
}


}
}
}
}
}
