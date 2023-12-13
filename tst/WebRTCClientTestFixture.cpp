#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

//
// Global memory allocation counter
//
UINT64 gTotalWebRtcClientMemoryUsage = 0;

//
// Global memory counter lock
//
MUTEX gTotalWebRtcClientMemoryMutex;

STATUS createRtpPacketWithSeqNum(UINT16 seqNum, PRtpPacket* ppRtpPacket)
{
    STATUS retStatus = STATUS_SUCCESS;
    BYTE payload[10];
    PRtpPacket pRtpPacket = NULL;

    CHK_STATUS(createRtpPacket(2, FALSE, FALSE, 0, FALSE, 96, seqNum, 100, 0x1234ABCD, NULL, 0, 0, NULL, payload, 10, &pRtpPacket));
    *ppRtpPacket = pRtpPacket;

    CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, NULL, &pRtpPacket->rawPacketLength));
    CHK(NULL != (pRtpPacket->pRawPacket = (PBYTE) MEMALLOC(pRtpPacket->rawPacketLength)), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, pRtpPacket->pRawPacket, &pRtpPacket->rawPacketLength));

CleanUp:
    return retStatus;
}

PVOID asyncGetIceConfigInfo(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    AsyncGetIceStruct* data = (AsyncGetIceStruct*) args;
    PIceConfigInfo pIceConfigInfo = NULL;
    UINT32 uriCount = 0;
    UINT32 i = 0, maxTurnServer = 1;

    if (data != NULL) {
        /* signalingClientGetIceConfigInfoCount can return more than one turn server. Use only one to optimize
         * candidate gathering latency. But user can also choose to use more than 1 turn server. */
        for (uriCount = 0, i = 0; i < maxTurnServer; i++) {
            /*
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
             * over DTLS yet. if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
             * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
             *
             * It's recommended to not pass too many TURN iceServers to configuration because it will slow down ice gathering in non-trickle mode.
             */
            CHK_STATUS(signalingClientGetIceConfigInfo(data->signalingClientHandle, i, &pIceConfigInfo));
            CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
            uriCount += pIceConfigInfo->uriCount;
            CHK_STATUS(addConfigToServerList(&(data->pAnswer), pIceConfigInfo));
            CHK_STATUS(addConfigToServerList(&(data->pOffer), pIceConfigInfo));
        }
    }
    *(data->pUriCount) += uriCount;

CleanUp:
    SAFE_MEMFREE(data);
    CHK_LOG_ERR(retStatus);
    return NULL;
}

WebRtcClientTestBase::WebRtcClientTestBase()
    : mSignalingClientHandle(INVALID_SIGNALING_CLIENT_HANDLE_VALUE), mAccessKey(NULL), mSecretKey(NULL), mSessionToken(NULL), mRegion(NULL),
      mCaCertPath(NULL), mAccessKeyIdSet(FALSE)
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

    SET_INSTRUMENTED_ALLOCATORS();

    mLogLevel = LOG_LEVEL_DEBUG;

    PCHAR logLevelStr = GETENV(DEBUG_LOG_LEVEL_ENV_VAR);
    if (logLevelStr != NULL) {
        ASSERT_EQ(STATUS_SUCCESS, STRTOUI32(logLevelStr, NULL, 10, &mLogLevel));
    }

    SET_LOGGER_LOG_LEVEL(mLogLevel);

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
        ASSERT_EQ(STATUS_SUCCESS,
                  createStaticCredentialProvider(mAccessKey, 0, mSecretKey, 0, mSessionToken, 0, MAX_UINT64, &mTestCredentialProvider));
    } else {
        mTestCredentialProvider = nullptr;
    }

    // Prepare the test channel name by prefixing with test channel name
    // and generating random chars replacing a potentially bad characters with '.'
    STRCPY(mChannelName, TEST_SIGNALING_CHANNEL_NAME);
    UINT32 testNameLen = STRLEN(TEST_SIGNALING_CHANNEL_NAME);
    const UINT32 randSize = 16;

    PCHAR pCur = &mChannelName[testNameLen];

    for (UINT32 i = 0; i < randSize; i++) {
        *pCur++ = SIGNALING_VALID_NAME_CHARS[RAND() % (ARRAY_SIZE(SIGNALING_VALID_NAME_CHARS) - 1)];
    }

    *pCur = '\0';
}

void WebRtcClientTestBase::TearDown()
{
    DLOGI("\nTearing down test: %s\n", GetTestName());

    deinitKvsWebRtc();
    // Need this sleep for threads in threadpool to close
    THREAD_SLEEP(400 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

    freeStaticCredentialProvider(&mTestCredentialProvider);

    EXPECT_EQ(STATUS_SUCCESS, RESET_INSTRUMENTED_ALLOCATORS());
}

VOID WebRtcClientTestBase::initializeJitterBuffer(UINT32 expectedFrameCount, UINT32 expectedDroppedFrameCount, UINT32 rtpPacketCount)
{
    UINT32 i, timestamp;
    EXPECT_EQ(STATUS_SUCCESS,
              createJitterBuffer(testFrameReadyFunc, testFrameDroppedFunc, testDepayRtpFunc, DEFAULT_JITTER_BUFFER_MAX_LATENCY,
                                 TEST_JITTER_BUFFER_CLOCK_RATE, (UINT64) this, &mJitterBuffer));
    mExpectedFrameCount = expectedFrameCount;
    mFrame = NULL;
    if (expectedFrameCount > 0) {
        mPExpectedFrameArr = (PBYTE*) MEMALLOC(SIZEOF(PBYTE) * expectedFrameCount);
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
        EXPECT_EQ(STATUS_SUCCESS,
                  createRtpPacket(2, FALSE, FALSE, 0, FALSE, 96, i, timestamp, 0x1234ABCD, NULL, 0, 0, NULL, NULL, 0, mPRtpPackets + i));
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
bool WebRtcClientTestBase::connectTwoPeers(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc, PCHAR pOfferCertFingerprint,
                                           PCHAR pAnswerCertFingerprint)
{
    RtcSessionDescriptionInit sdp;
    PeerContainer offer;
    PeerContainer answer;

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        PPeerContainer container = (PPeerContainer)customData;
        if (candidateStr != NULL) {
            container->client->threads.push_back(std::thread(
                [container](std::string candidate) {
                    RtcIceCandidateInit iceCandidate;
                    EXPECT_EQ(STATUS_SUCCESS, deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
                    EXPECT_EQ(STATUS_SUCCESS, addIceCandidate((PRtcPeerConnection) container->pc, iceCandidate.candidate));
                },
                std::string(candidateStr)));
        }
    };

    offer.pc = offerPc;
    offer.client = this;
    answer.pc = answerPc;
    answer.client = this;

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) &answer, onICECandidateHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) &offer, onICECandidateHdlr));

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        ATOMIC_INCREMENT((PSIZE_T) customData + newState);
    };

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(offerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(answerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr));

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    // Validate the cert fingerprint if we are asked to do so
    if (pOfferCertFingerprint != NULL) {
        EXPECT_NE((PCHAR) NULL, STRSTR(sdp.sdp, pOfferCertFingerprint));
    }

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    if (pAnswerCertFingerprint != NULL) {
        EXPECT_NE((PCHAR) NULL, STRSTR(sdp.sdp, pAnswerCertFingerprint));
    }

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    //join all threads before leaving
    for (auto& th : this->threads) th.join();

    this->threads.clear();

    return ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) == 2;
}

bool WebRtcClientTestBase::connectTwoPeersAsyncIce(PRtcPeerConnection offerPc, PRtcPeerConnection answerPc, PCHAR pOfferCertFingerprint,
                                                   PCHAR pAnswerCertFingerprint)
{
    RtcSessionDescriptionInit sdp;
    PeerContainer offer;
    PeerContainer answer;

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        PPeerContainer container = (PPeerContainer)customData;
        if (candidateStr != NULL) {
            container->client->threads.push_back(std::thread(
                [container](std::string candidate) {
                    RtcIceCandidateInit iceCandidate;
                    EXPECT_EQ(STATUS_SUCCESS, deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
                    EXPECT_EQ(STATUS_SUCCESS, addIceCandidate((PRtcPeerConnection) container->pc, iceCandidate.candidate));
                },
                std::string(candidateStr)));
        }
    };

    offer.pc = offerPc;
    offer.client = this;
    answer.pc = answerPc;
    answer.client = this;

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) &answer, onICECandidateHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) &offer, onICECandidateHdlr));

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        ATOMIC_INCREMENT((PSIZE_T) customData + newState);
    };

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(offerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(answerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr));

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    // Validate the cert fingerprint if we are asked to do so
    if (pOfferCertFingerprint != NULL) {
        EXPECT_NE((PCHAR) NULL, STRSTR(sdp.sdp, pOfferCertFingerprint));
    }

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    asyncGetIceConfig(offerPc, answerPc);

    if (pAnswerCertFingerprint != NULL) {
        EXPECT_NE((PCHAR) NULL, STRSTR(sdp.sdp, pAnswerCertFingerprint));
    }

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    this->threads.clear();

    return ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) == 2;
}

// Create track and transceiver and adds to PeerConnection
void WebRtcClientTestBase::addTrackToPeerConnection(PRtcPeerConnection pRtcPeerConnection, PRtcMediaStreamTrack track,
                                                    PRtcRtpTransceiver* transceiver, RTC_CODEC codec, MEDIA_STREAM_TRACK_KIND kind)
{
    MEMSET(track, 0x00, SIZEOF(RtcMediaStreamTrack));

    EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(pRtcPeerConnection, codec));

    track->kind = kind;
    track->codec = codec;
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(track->streamId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(track->trackId, MAX_MEDIA_STREAM_ID_LEN));

    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(pRtcPeerConnection, track, NULL, transceiver));
}

void WebRtcClientTestBase::getIceServers(PRtcConfiguration pRtcConfiguration, PIceAgent pIceAgent)
{
    UINT32 i, j, iceConfigCount = 0, uriCount;
    PIceConfigInfo pIceConfigInfo;

    // Assume signaling client is already created
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

    // Set the  STUN server
    SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION,
             TEST_DEFAULT_STUN_URL_POSTFIX);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
        EXPECT_EQ(STATUS_SUCCESS, iceAgentAddConfig(pIceAgent, pIceConfigInfo));
    }
}

void WebRtcClientTestBase::getIceServers(PRtcConfiguration pRtcConfiguration, PRtcPeerConnection pRtcPeerConnection)
{
    UINT32 i, j, iceConfigCount = 0, uriCount;
    PIceConfigInfo pIceConfigInfo;

    // Assume signaling client is already created
    EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

    // Set the  STUN server
    SNPRINTF(pRtcConfiguration->iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION,
             TEST_DEFAULT_STUN_URL_POSTFIX);

    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(pRtcConfiguration->iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

            uriCount++;
        }
        EXPECT_EQ(STATUS_SUCCESS, addConfigToServerList(&pRtcPeerConnection, pIceConfigInfo));
    }
}

PCHAR WebRtcClientTestBase::GetTestName()
{
    return (PCHAR)::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
