#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

//
// Global memory allocation counter
//
UINT64 gTotalWebRtcClientMemoryUsage = 0;

//
// Global memory counter lock
//
MUTEX gTotalWebRtcClientMemoryMutex;

WebRtcClientTestBase::WebRtcClientTestBase() :
        mAccessKey(NULL),
        mSecretKey(NULL),
        mSessionToken(NULL),
        mRegion(NULL),
        mCaCertPath(NULL),
        mAccessKeyIdSet(FALSE)
{
    // Initialize the endianness of the library
    initializeEndianness();

    SRAND(12345);

    mStreamingRotationPeriod = TEST_STREAMING_TOKEN_DURATION;
}

void WebRtcClientTestBase::SetUp()
{
    DLOGI("\nSetting up test: %s\n", GetTestName());
    mReadyFrameIndex = 0;
    mDroppedFrameIndex = 0;
    mExpectedFrameCount = 0;
    mExpectedDroppedFrameCount = 0;

    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_DEBUG);

    initKvsWebRtc();

    if (NULL != (mAccessKey = getenv(ACCESS_KEY_ENV_VAR))) {
        mAccessKeyIdSet = TRUE;
    }

    mSecretKey = getenv(SECRET_KEY_ENV_VAR);
    mSessionToken = getenv(SESSION_TOKEN_ENV_VAR);

    if (NULL == (mRegion = getenv(DEFAULT_REGION_ENV_VAR))) {
        mRegion = TEST_DEFAULT_REGION;
    }

    if (NULL == (mCaCertPath = getenv(CACERT_PATH_ENV_VAR))) {
        mCaCertPath = (PCHAR) DEFAULT_KVS_CACERT_PATH;
    }

    if (mAccessKey) {
        ASSERT_EQ(STATUS_SUCCESS, createStaticCredentialProvider(mAccessKey, 0, mSecretKey, 0,
                                                                 mSessionToken, 0, MAX_UINT64, &mTestCredentialProvider));
    } else {
        mTestCredentialProvider = nullptr;
    }

    // Prepare the test channel name by prefixing with test channel name
    // and generating random chars replacing a potentially bad characters with '.'
    STRCPY(mChannelName, TEST_SIGNALING_CHANNEL_NAME);
    UINT32 testNameLen = STRLEN(TEST_SIGNALING_CHANNEL_NAME);
    const UINT32 randSize = 16;

    BYTE randBuffer[randSize];
    RAND_bytes(randBuffer, randSize);
    PCHAR pCur = &mChannelName[testNameLen];

    for (UINT32 i = 0; i < randSize; i++) {
        *pCur++ = SIGNALING_VALID_NAME_CHARS[randBuffer[i % MAX_RAND_BUFFER_SIZE_FOR_NAME] % (ARRAY_SIZE(SIGNALING_VALID_NAME_CHARS) - 1)];
    }

    *pCur = '\0';
}

void WebRtcClientTestBase::TearDown()
{
    DLOGI("\nTearing down test: %s\n", GetTestName());

    deinitKvsWebRtc();

    freeStaticCredentialProvider(&mTestCredentialProvider);
}

VOID WebRtcClientTestBase::initializeJitterBuffer(UINT32 expectedFrameCount, UINT32 expectedDroppedFrameCount, UINT32 rtpPacketCount)
{
    UINT32 i, timestamp;
    EXPECT_EQ(STATUS_SUCCESS, createJitterBuffer(testFrameReadyFunc, testFrameDroppedFunc, testDepayRtpFunc, DEFAULT_JITTER_BUFFER_MAX_LATENCY, TEST_JITTER_BUFFER_CLOCK_RATE, (UINT64) this, &mJitterBuffer));
    mExpectedFrameCount = expectedFrameCount;
    mFrame = NULL;
    if (expectedFrameCount > 0) {
        mPExpectedFrameArr = (PBYTE *) MEMALLOC(SIZEOF(PBYTE) * expectedFrameCount);
        mExpectedFrameSizeArr = (PUINT32) MEMALLOC(SIZEOF(UINT32) * expectedFrameCount);
    }
    mExpectedDroppedFrameCount = expectedDroppedFrameCount;
    if (expectedDroppedFrameCount > 0) {
        mExpectedDroppedFrameTimestampArr = (PUINT32) MEMALLOC(SIZEOF(UINT32) * expectedDroppedFrameCount);
    }

    mPRtpPackets = (PRtpPacket*) MEMALLOC(SIZEOF(PRtpPacket) * rtpPacketCount);
    mRtpPacketCount = rtpPacketCount;

    // Assume timestamp is on time unit ms for test
    for (i = 0, timestamp = 0; i < rtpPacketCount; i++, timestamp += 200) {
        EXPECT_EQ(STATUS_SUCCESS, createRtpPacket(2, FALSE, FALSE, 0, FALSE,
                                                  96, i, timestamp, 0x1234ABCD, NULL,
                                                  0, 0, NULL, NULL, 0, mPRtpPackets + i));
    }
}

VOID WebRtcClientTestBase::setPayloadToFree()
{
    UINT32 i;
    for (i = 0; i < mRtpPacketCount; i++) {
        mPRtpPackets[i]->pRawPacket = mPRtpPackets[i]->payload;
    }
}

VOID WebRtcClientTestBase::clearJitterBufferForTest()
{
    UINT32 i;
    EXPECT_EQ(STATUS_SUCCESS, freeJitterBuffer(&mJitterBuffer));
    if (mExpectedFrameCount > 0) {
        for (i = 0; i < mExpectedFrameCount; i++) {
            MEMFREE(mPExpectedFrameArr[i]);
        }
        MEMFREE(mPExpectedFrameArr);
        MEMFREE(mExpectedFrameSizeArr);
    }
    if (mExpectedDroppedFrameCount > 0) {
        MEMFREE(mExpectedDroppedFrameTimestampArr);
    }
    MEMFREE(mPRtpPackets);
    EXPECT_EQ(mExpectedFrameCount, mReadyFrameIndex);
    EXPECT_EQ(mExpectedDroppedFrameCount, mDroppedFrameIndex);
    if (mFrame != NULL) {
        MEMFREE(mFrame);
    }
}

// Connect two RtcPeerConnections, and wait for them to be connected
// in the given amount of time. Return false if they don't go to connected in
// the expected amounted of time
bool WebRtcClientTestBase::connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc) {
    RtcSessionDescriptionInit sdp;

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        if (candidateStr != NULL) {
            std::thread([customData] (std::string candidate) {
                RtcIceCandidateInit iceCandidate;
                EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate), STATUS_SUCCESS);
                EXPECT_EQ(addIceCandidate((PRtcPeerConnection) customData, iceCandidate.candidate), STATUS_SUCCESS);
            }, std::string(candidateStr)).detach();
        }
    };

    EXPECT_EQ(peerConnectionOnIceCandidate(offerPc, (UINT64) answerPc, onICECandidateHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnIceCandidate(answerPc, (UINT64) offerPc, onICECandidateHdlr), STATUS_SUCCESS);

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        ATOMIC_INCREMENT((PSIZE_T)customData + newState);
    };

    EXPECT_EQ(peerConnectionOnConnectionStateChange(offerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnConnectionStateChange(answerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);

    EXPECT_EQ(createOffer(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(answerPc, &sdp), STATUS_SUCCESS);

    EXPECT_EQ(createAnswer(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(answerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setRemoteDescription(offerPc, &sdp), STATUS_SUCCESS);

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    return ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) == 2;
}

// Create track and transceiver and adds to PeerConnection
void WebRtcClientTestBase::addTrackToPeerConnection(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track, PRtcRtpTransceiver *transceiver, RTC_CODEC codec, MEDIA_STREAM_TRACK_KIND kind)
{
    MEMSET(track, 0x00, SIZEOF(RtcMediaStreamTrack));

    EXPECT_EQ(addSupportedCodec(pRtcPeerConnection, codec), STATUS_SUCCESS);

    track->kind = kind;
    track->codec = codec;
    EXPECT_EQ(generateJSONSafeString(track->streamId, MAX_MEDIA_STREAM_ID_LEN), STATUS_SUCCESS);
    EXPECT_EQ(generateJSONSafeString(track->trackId, MAX_MEDIA_STREAM_ID_LEN), STATUS_SUCCESS);

    EXPECT_EQ(addTransceiver(pRtcPeerConnection, track, NULL, transceiver), STATUS_SUCCESS);
}

void WebRtcClientTestBase::getIceServers(PRtcConfiguration pRtcConfiguration)
{
    UINT32 i, j, iceConfigCount, uriCount;
    PIceConfigInfo pIceConfigInfo;

    // Assume signaling client is already created
    EXPECT_EQ(signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount), STATUS_SUCCESS);

    // Set the  STUN server
    SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        EXPECT_EQ(signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo), STATUS_SUCCESS);
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
    }
}

PCHAR WebRtcClientTestBase::GetTestName()
{
    return (PCHAR) ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
}

}  // namespace webrtcclient
}  // namespace video
}  // namespace kinesis
}  // namespace amazonaws
}  // namespace com;
