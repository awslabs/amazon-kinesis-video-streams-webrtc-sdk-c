#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class PeerConnectionFunctionalityTest : public WebRtcClientTestBase {};

// Assert that two PeerConnections can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeers)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersWithDelay)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    SIZE_T connectedCount = 0;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PeerContainer offer;
    PeerContainer answer;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        PPeerContainer container = (PPeerContainer)customData;
        if (candidateStr != NULL) {
            container->client->lock.lock();
            if(!container->client->noNewThreads) {
                container->client->threads.push_back(std::thread(
                    [container](std::string candidate) {
                        RtcIceCandidateInit iceCandidate;
                        EXPECT_EQ(STATUS_SUCCESS, deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
                        EXPECT_EQ(STATUS_SUCCESS, addIceCandidate((PRtcPeerConnection) container->pc, iceCandidate.candidate));
                    },
                    std::string(candidateStr)));
            }
            container->client->lock.unlock();
        }
    };

    offer.pc = offerPc;
    offer.client = this;
    answer.pc = answerPc;
    answer.client = this;

    auto onICECandidateHdlrDone = [](UINT64 customData, PCHAR candidateStr) -> void {
        UNUSED_PARAM(customData);
        UNUSED_PARAM(candidateStr);
    };

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) &answer, onICECandidateHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) &offer, onICECandidateHdlr));

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        if (newState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
            ATOMIC_INCREMENT((PSIZE_T) customData);
        }
    };

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(offerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(answerPc, (UINT64) &connectedCount, onICEConnectionStateChangeHdlr));

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));

    THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);

    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&connectedCount) != 2; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_EQ(2, connectedCount);

    this->lock.lock();
    //join all threads before leaving
    for (auto& th : this->threads) th.join();

    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) 0, onICECandidateHdlrDone));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) 0, onICECandidateHdlrDone));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

#ifdef KVS_USE_OPENSSL
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

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}
#elif KVS_USE_MBEDTLS
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersWithPresetCerts)
{
    RtcConfiguration offerConfig, answerConfig;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    mbedtls_x509_crt offerCert;
    mbedtls_x509_crt answerCert;
    mbedtls_pk_context offerKey;
    mbedtls_pk_context answerKey;
    CHAR offerCertFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];
    CHAR answerCertFingerprint[CERTIFICATE_FINGERPRINT_LENGTH];

    // Generate offer cert
    ASSERT_EQ(STATUS_SUCCESS, createCertificateAndKey(GENERATED_CERTIFICATE_BITS, true, &offerCert, &offerKey));
    ASSERT_EQ(STATUS_SUCCESS, dtlsCertificateFingerprint(&offerCert, offerCertFingerprint));

    // Generate answer cert
    ASSERT_EQ(STATUS_SUCCESS, createCertificateAndKey(GENERATED_CERTIFICATE_BITS, true, &answerCert, &answerKey));
    ASSERT_EQ(STATUS_SUCCESS, dtlsCertificateFingerprint(&answerCert, answerCertFingerprint));

    MEMSET(&offerConfig, 0x00, SIZEOF(RtcConfiguration));
    offerConfig.certificates[0].pCertificate = (PBYTE) &offerCert;
    offerConfig.certificates[0].certificateSize = 0;
    offerConfig.certificates[0].pPrivateKey = (PBYTE) &offerKey;
    offerConfig.certificates[0].privateKeySize = 0;

    MEMSET(&answerConfig, 0x00, SIZEOF(RtcConfiguration));
    answerConfig.certificates[0].pCertificate = (PBYTE) &answerCert;
    answerConfig.certificates[0].certificateSize = 0;
    answerConfig.certificates[0].pPrivateKey = (PBYTE) &answerKey;
    answerConfig.certificates[0].privateKeySize = 0;

    ASSERT_EQ(STATUS_SUCCESS, createPeerConnection(&offerConfig, &offerPc));
    ASSERT_EQ(STATUS_SUCCESS, createPeerConnection(&answerConfig, &answerPc));

    // Should be fine to free right after create peer connection
    freeCertificateAndKey(&offerCert, &offerKey);
    freeCertificateAndKey(&answerCert, &answerKey);

    ASSERT_EQ(TRUE, connectTwoPeers(offerPc, answerPc, offerCertFingerprint, answerCertFingerprint));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}
#endif

// Assert that two PeerConnections with forced TURN can connect to each other and go to connected
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersForcedTURN)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, sendDataWithClosedSocketConnectionWithHostAndStun)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcMediaStreamTrack offerVideoTrack;
    PRtcRtpTransceiver offerVideoTransceiver;
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PKvsPeerConnection pOfferPcImpl;
    PIceAgent pIceAgent;
    PIceCandidate pLocalCandidate;
    PSocketConnection pSocketConnection;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // addTrackToPeerConnection is necessary because we need to add a transceiver which will trigger the RTCP callback. The RTCP callback
    // will send application data. The expected behavior for the PeerConnection is to bail out when the socket connection that's being used
    // is already closed.
    //
    // In summary, the scenario looks like the following:
    //   1. Connect the two peers
    //   2. Add a transceiver, which will send RTCP feedback in a regular interval + some randomness
    //   3. Do fault injection to the ICE agent, simulate early closed connection
    //   4. Wait for the RTCP callback to fire, which will change the ICE agent status to STATUS_SOCKET_CONNECTION_CLOSED_ALREADY
    //   5. Wait for the ICE agent state regular polling to check the status and update the ICE agent state to FAILED
    //   6. When ICE agent state changes to FAILED, the PeerConnection will be notified and change its state to FAILED as well
    //   7. Verify that we the counter for RTC_PEER_CONNECTION_STATE_FAILED is not 0
    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    pOfferPcImpl = (PKvsPeerConnection) offerPc;
    pIceAgent = pOfferPcImpl->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    pLocalCandidate = pIceAgent->pDataSendingIceCandidatePair->local;

    if (pLocalCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        pSocketConnection = pLocalCandidate->pTurnConnection->pControlChannel;
    } else {
        pSocketConnection = pLocalCandidate->pSocketConnection;
    }
    EXPECT_EQ(STATUS_SUCCESS, socketConnectionClosed(pSocketConnection));
    MUTEX_UNLOCK(pIceAgent->lock);

    // The next poll should check the current ICE agent status and drives the ICE agent state machine to failed,
    // change the PeerConnection state to failed as well.
    //
    // We need to add 2 seconds because we need to first wait the RTCP callback to fire first after the fault injection.
    THREAD_SLEEP(KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL + 2 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_NE(0, ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_FAILED]));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(PeerConnectionFunctionalityTest, sendDataWithClosedSocketConnectionWithForcedTurn)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcMediaStreamTrack offerVideoTrack;
    PRtcRtpTransceiver offerVideoTransceiver;
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PKvsPeerConnection pOfferPcImpl;
    PIceAgent pIceAgent;
    PIceCandidate pLocalCandidate;
    PSocketConnection pSocketConnection;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // addTrackToPeerConnection is necessary because we need to add a transceiver which will trigger the RTCP callback. The RTCP callback
    // will send application data. The expected behavior for the PeerConnection is to bail out when the socket connection that's being used
    // is already closed.
    //
    // In summary, the scenario looks like the following:
    //   1. Connect the two peers
    //   2. Add a transceiver, which will send RTCP feedback in a regular interval + some randomness
    //   3. Do fault injection to the ICE agent, simulate early closed connection
    //   4. Wait for the RTCP callback to fire, which will change the ICE agent status to STATUS_SOCKET_CONNECTION_CLOSED_ALREADY
    //   5. Wait for the ICE agent state regular polling to check the status and update the ICE agent state to FAILED
    //   6. When ICE agent state changes to FAILED, the PeerConnection will be notified and change its state to FAILED as well
    //   7. Verify that we the counter for RTC_PEER_CONNECTION_STATE_FAILED is not 0
    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    pOfferPcImpl = (PKvsPeerConnection) offerPc;
    pIceAgent = pOfferPcImpl->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    pLocalCandidate = pIceAgent->pDataSendingIceCandidatePair->local;

    if (pLocalCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
        pSocketConnection = pLocalCandidate->pTurnConnection->pControlChannel;
    } else {
        pSocketConnection = pLocalCandidate->pSocketConnection;
    }
    EXPECT_EQ(STATUS_SUCCESS, socketConnectionClosed(pSocketConnection));
    MUTEX_UNLOCK(pIceAgent->lock);

    // The next poll should check the current ICE agent status and drives the ICE agent state machine to failed,
    // change the PeerConnection state to failed as well.
    //
    // We need to add 2 seconds because we need to first wait the RTCP callback to fire first after the fault injection.
    THREAD_SLEEP(KVS_ICE_STATE_READY_TIMER_POLLING_INTERVAL + 2 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_NE(0, ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_FAILED]));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, shutdownTurnDueToP2PFoundBeforeTurnEstablished)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PIceAgent pIceAgent = NULL;
    PDoubleListNode pCurNode = NULL;
    PIceCandidate pIceCandidate = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    pIceAgent = ((PKvsPeerConnection) offerPc)->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    EXPECT_EQ(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode), STATUS_SUCCESS);
    while (pCurNode != NULL) {
        pIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->hasAllocation) ||
                        ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->stopTurnConnection));
        }
    }
    MUTEX_UNLOCK(pIceAgent->lock);

    pIceAgent = ((PKvsPeerConnection) answerPc)->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    EXPECT_EQ(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode), STATUS_SUCCESS);
    while (pCurNode != NULL) {
        pIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->hasAllocation) ||
                        ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->stopTurnConnection));
        }
    }
    MUTEX_UNLOCK(pIceAgent->lock);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, shutdownTurnDueToP2PFoundAfterTurnEstablished)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcSessionDescriptionInit sdp;
    SIZE_T offerPcDoneGatherCandidate = 0, answerPcDoneGatherCandidate = 0;
    UINT64 candidateGatherTimeout;
    PIceAgent pIceAgent = NULL;
    PDoubleListNode pCurNode = NULL;
    PIceCandidate pIceCandidate = NULL;

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
        ATOMIC_INCREMENT((PSIZE_T) customData + newState);
    };

    EXPECT_EQ(peerConnectionOnConnectionStateChange(offerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);
    EXPECT_EQ(peerConnectionOnConnectionStateChange(answerPc, (UINT64) this->stateChangeCount, onICEConnectionStateChangeHdlr), STATUS_SUCCESS);

    // start gathering candidates
    EXPECT_EQ(setLocalDescription(offerPc, &sdp), STATUS_SUCCESS);
    EXPECT_EQ(setLocalDescription(answerPc, &sdp), STATUS_SUCCESS);

    // give time for turn allocation to be finished
    candidateGatherTimeout = GETTIME() + KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT + 2 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!(ATOMIC_LOAD(&offerPcDoneGatherCandidate) > 0 && ATOMIC_LOAD(&answerPcDoneGatherCandidate) > 0) && GETTIME() < candidateGatherTimeout) {
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

    pIceAgent = ((PKvsPeerConnection) offerPc)->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    EXPECT_EQ(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode), STATUS_SUCCESS);
    while (pCurNode != NULL) {
        pIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->hasAllocation) ||
                        ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->stopTurnConnection));
        }
    }
    MUTEX_UNLOCK(pIceAgent->lock);

    pIceAgent = ((PKvsPeerConnection) answerPc)->pIceAgent;
    MUTEX_LOCK(pIceAgent->lock);
    EXPECT_EQ(doubleListGetHeadNode(pIceAgent->localCandidates, &pCurNode), STATUS_SUCCESS);
    while (pCurNode != NULL) {
        pIceCandidate = (PIceCandidate) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidate->iceCandidateType == ICE_CANDIDATE_TYPE_RELAYED) {
            EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->hasAllocation) ||
                        ATOMIC_LOAD_BOOL(&pIceCandidate->pTurnConnection->stopTurnConnection));
        }
    }
    MUTEX_UNLOCK(pIceAgent->lock);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

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
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Assert that two PeerConnections can connect and then terminate one of them, the other one will eventually report disconnection
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersThenDisconnectTest)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);
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

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(PeerConnectionFunctionalityTest, noLostFramesAfterConnected)
{
    struct Context {
        MUTEX mutex;
        ATOMIC_BOOL done;
        CVAR cvar;
    };

    RtcConfiguration configuration;
    Context context;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, answerVideoTrack;
    PRtcRtpTransceiver offerVideoTransceiver, answerVideoTransceiver;
    RtcSessionDescriptionInit sdp;
    ATOMIC_BOOL seenFirstFrame = FALSE;
    Frame videoFrame;

    PeerContainer offer;
    PeerContainer answer;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&videoFrame, 0x00, SIZEOF(Frame));

    videoFrame.frameData = (PBYTE) MEMALLOC(1);
    videoFrame.size = 1;
    videoFrame.presentationTs = 0;

    context.mutex = MUTEX_CREATE(FALSE);
    ASSERT_NE(context.mutex, INVALID_MUTEX_VALUE);
    context.cvar = CVAR_CREATE();
    ASSERT_NE(context.cvar, INVALID_CVAR_VALUE);
    ATOMIC_STORE_BOOL(&context.done, FALSE);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        PPeerContainer container = (PPeerContainer)customData;
        if (candidateStr != NULL) {
            container->client->lock.lock();
            if(!container->client->noNewThreads) {
                container->client->threads.push_back(std::thread(
                    [container](std::string candidate) {
                        RtcIceCandidateInit iceCandidate;
                        EXPECT_EQ(STATUS_SUCCESS, deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
                        EXPECT_EQ(STATUS_SUCCESS, addIceCandidate((PRtcPeerConnection) container->pc, iceCandidate.candidate));
                    },
                    std::string(candidateStr)));
            }
            container->client->lock.unlock();
        }
    };

    offer.pc = offerPc;
    offer.client = this;
    answer.pc = answerPc;
    answer.client = this;

    auto onICECandidateHdlrDone = [](UINT64 customData, PCHAR candidateStr) -> void {
        UNUSED_PARAM(customData);
        UNUSED_PARAM(candidateStr);
    };

    auto onFrameHandler = [](UINT64 customData, PFrame pFrame) -> void {
        UNUSED_PARAM(pFrame);
        if (pFrame->frameData[0] == 1) {
            ATOMIC_STORE_BOOL((PSIZE_T) customData, 1);
        }
    };
    EXPECT_EQ(transceiverOnFrame(answerVideoTransceiver, (UINT64) &seenFirstFrame, onFrameHandler), STATUS_SUCCESS);

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) &answer, onICECandidateHdlr));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) &offer, onICECandidateHdlr));

    auto onICEConnectionStateChangeHdlr = [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) -> void {
        Context* pContext = (Context*) customData;

        if (newState == RTC_PEER_CONNECTION_STATE_CONNECTED) {
            ATOMIC_STORE_BOOL(&pContext->done, TRUE);
            CVAR_SIGNAL(pContext->cvar);
        }
    };

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnConnectionStateChange(offerPc, (UINT64) &context, onICEConnectionStateChangeHdlr));

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    MUTEX_LOCK(context.mutex);
    while (!ATOMIC_LOAD_BOOL(&context.done)) {
        CVAR_WAIT(context.cvar, context.mutex, INFINITE_TIME_VALUE);
    }
    MUTEX_UNLOCK(context.mutex);

    for (BYTE i = 1; i <= 3; i++) {
        videoFrame.frameData[0] = i;
        EXPECT_EQ(writeFrame(offerVideoTransceiver, &videoFrame), STATUS_SUCCESS);
        videoFrame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 25);
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND / 25);
    }

    for (auto i = 0; i <= 1000 && !ATOMIC_LOAD_BOOL(&seenFirstFrame); i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    this->lock.lock();
    for (auto& th : this->threads) th.join();

    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(offerPc, (UINT64) 0, onICECandidateHdlrDone));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnIceCandidate(answerPc, (UINT64) 0, onICECandidateHdlrDone));

    MEMFREE(videoFrame.frameData);
    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    CVAR_FREE(context.cvar);
    MUTEX_FREE(context.mutex);

    EXPECT_EQ(ATOMIC_LOAD_BOOL(&seenFirstFrame), TRUE);
}

// Assert that two PeerConnections can connect and then send media until the receiver gets both audio/video
TEST_F(PeerConnectionFunctionalityTest, exchangeMedia)
{
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

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
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
    RtcOutboundRtpStreamStats stats{};
    EXPECT_EQ(STATUS_SUCCESS, getRtpOutboundStats(offerPc, offerVideoTransceiver, &stats));
    EXPECT_EQ(206, stats.sent.packetsSent);
#ifdef KVS_USE_MBEDTLS
    EXPECT_EQ(248026, stats.sent.bytesSent);
#else
    EXPECT_EQ(246790, stats.sent.bytesSent);
#endif
    EXPECT_EQ(2, stats.framesSent);
    EXPECT_EQ(2472, stats.headerBytesSent);
    EXPECT_LT(0, stats.lastPacketSentTimestamp);

    RtcInboundRtpStreamStats answerStats{};
    EXPECT_EQ(STATUS_SUCCESS, getRtpInboundStats(answerPc, answerVideoTransceiver, &answerStats));
    EXPECT_LE(1, answerStats.framesReceived);
    EXPECT_LT(103, answerStats.received.packetsReceived);
    EXPECT_LT(120000, answerStats.bytesReceived);
    EXPECT_LT(1234, answerStats.headerBytesReceived);
    EXPECT_LT(0, answerStats.lastPacketReceivedTimestamp);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    EXPECT_EQ(ATOMIC_LOAD(&seenVideo), 1);
}

// Same test as exchangeMedia, but assert that if one side is RSA DTLS and Key Extraction works
TEST_F(PeerConnectionFunctionalityTest, exchangeMediaRSA)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

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

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
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

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    EXPECT_EQ(ATOMIC_LOAD(&seenVideo), 1);
}

TEST_F(PeerConnectionFunctionalityTest, iceRestartTest)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    EXPECT_EQ(restartIce(offerPc), STATUS_SUCCESS);

    /* reset state change count */
    MEMSET(&stateChangeCount, 0x00, SIZEOF(stateChangeCount));

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(PeerConnectionFunctionalityTest, iceRestartTestForcedTurn)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    EXPECT_EQ(restartIce(offerPc), STATUS_SUCCESS);

    /* reset state change count */
    MEMSET(&stateChangeCount, 0x00, SIZEOF(stateChangeCount));

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, peerConnectionOfferCloseConnection)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();
    getIceServers(&configuration);

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(offerPc);
    EXPECT_EQ(ATOMIC_LOAD(&stateChangeCount[RTC_PEER_CONNECTION_STATE_CLOSED]), 2);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, peerConnectionAnswerCloseConnection)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    ASSERT_EQ(TRUE, mAccessKeyIdSet);
    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    closePeerConnection(answerPc);
    EXPECT_EQ(stateChangeCount[RTC_PEER_CONNECTION_STATE_CLOSED], 2);
    closePeerConnection(offerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

TEST_F(PeerConnectionFunctionalityTest, DISABLED_exchangeMediaThroughTurnRandomStop)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    initializeSignalingClient();

    auto repeatedStreamingRandomStop = [this](int iteration, int maxStreamingDurationMs, int minStreamingDurationMs, bool expectSeenVideo) -> void {
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

        for (int i = 0; i < iteration; ++i) {
            MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
            configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
            getIceServers(&configuration);

            EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
            EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

            addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
            addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
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
                while (!ATOMIC_LOAD_BOOL(pTerminationFlag)) {
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

// Check that even when multiple successful candidate pairs are found, only one dtls negotiation takes place
// and that it is on the same candidate throughout the connection.
TEST_F(PeerConnectionFunctionalityTest, multipleCandidateSuccessOneDTLSCheck)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    // This test can succeed if the highest priority candidate pair happens to be the first one
    // to be nominated, even if the DTLS is broken. To be sure that this issue is fixed we want to
    // run the test 10 times and have it never break once in that cycle.
    for (auto i = 0; i < 10; i++) {
        offerPc = NULL;
        answerPc = NULL;
        MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

        EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
        EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

        // create a callback that can check values at every state of the ice agent state machine
        auto masterOnIceConnectionStateChangeTest = [](UINT64 customData, UINT64 connectionState) -> void {
            static PIceCandidatePair pSendingPair;
            PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
            // still use normal callback
            onIceConnectionStateChange(customData, connectionState);
            switch (connectionState) {
                case ICE_AGENT_STATE_CHECK_CONNECTION:
                    // sleep(1);
                    break;
                case ICE_AGENT_STATE_CONNECTED:
                    if (pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair != NULL) {
                        pSendingPair = pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair;
                    }
                    break;
                case ICE_AGENT_STATE_READY:
                    if (pSendingPair != NULL) {
                        EXPECT_EQ(pSendingPair, pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair);
                        pSendingPair = NULL;
                    }
                    break;
                default:
                    break;
            }
        };

        auto viewerOnIceConnectionStateChangeTest = [](UINT64 customData, UINT64 connectionState) -> void {
            PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
            PIceAgent pIceAgent = pKvsPeerConnection->pIceAgent;
            PDoubleListNode pCurNode = NULL;
            PIceCandidatePair pIceCandidatePair;
            BOOL locked = FALSE;
            // still use normal callback
            onIceConnectionStateChange(customData, connectionState);
            switch (connectionState) {
                case ICE_AGENT_STATE_CONNECTED:
                    // send 'USE_CANDIDATE' for every ice candidate pair
                    MUTEX_LOCK(pIceAgent->lock);
                    locked = TRUE;
                    doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode);
                    while (pCurNode != NULL) {
                        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
                        pCurNode = pCurNode->pNext;

                        pIceCandidatePair->nominated = TRUE;
                    }
                    if (locked) {
                        MUTEX_UNLOCK(pIceAgent->lock);
                    }

                    break;
                default:
                    break;
            }
        };

        // overwrite normal callback
        ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.connectionStateChangedFn = masterOnIceConnectionStateChangeTest;
        ((PKvsPeerConnection) offerPc)->pIceAgent->iceAgentCallbacks.connectionStateChangedFn = viewerOnIceConnectionStateChangeTest;

        EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

        closePeerConnection(offerPc);
        closePeerConnection(answerPc);

        freePeerConnection(&offerPc);
        freePeerConnection(&answerPc);
        MEMSET(this->stateChangeCount, 0, SIZEOF(SIZE_T) * RTC_PEER_CONNECTION_TOTAL_STATE_COUNT);
        if (::testing::Test::HasFailure()) {
            break;
        }
    }
}

// Check that even when multiple successful candidate pairs are found, only one dtls negotiation takes place
// and that it is on the same candidate throughout the connection. This time setting the viewer to use
// aggressive nomination
TEST_F(PeerConnectionFunctionalityTest, aggressiveNominationDTLSRaceConditionCheck)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    // This test can succeed if the highest priority candidate pair happens to be the first one
    // to be nominated, even if the DTLS is broken. To be sure that this issue is fixed we want to
    // run the test 10 times and have it never break once in that cycle.
    for (auto i = 0; i < 10; i++) {
        offerPc = NULL;
        answerPc = NULL;
        MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

        EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
        EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

        // create a callback that can check values at every state of the ice agent state machine
        auto masterOnIceConnectionStateChangeTest = [](UINT64 customData, UINT64 connectionState) -> void {
            static PIceCandidatePair pSendingPair;
            PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
            // still use normal callback
            onIceConnectionStateChange(customData, connectionState);
            switch (connectionState) {
                case ICE_AGENT_STATE_CHECK_CONNECTION:
                    // sleep(1);
                    break;
                case ICE_AGENT_STATE_CONNECTED:
                    if (pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair != NULL) {
                        pSendingPair = pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair;
                    }
                    break;
                case ICE_AGENT_STATE_READY:
                    if (pSendingPair != NULL) {
                        EXPECT_EQ(pSendingPair, pKvsPeerConnection->pIceAgent->pDataSendingIceCandidatePair);
                        pSendingPair = NULL;
                    }
                    break;
                default:
                    break;
            }
        };

        auto viewerOnIceConnectionStateChangeTest = [](UINT64 customData, UINT64 connectionState) -> void {
            static BOOL setUseCandidate = FALSE;
            PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
            PIceAgent pIceAgent = pKvsPeerConnection->pIceAgent;
            PDoubleListNode pCurNode = NULL;
            PIceCandidatePair pIceCandidatePair;
            BOOL locked = FALSE;
            // still use normal callback
            onIceConnectionStateChange(customData, connectionState);
            switch (connectionState) {
                case ICE_AGENT_STATE_CHECK_CONNECTION:
                    MUTEX_LOCK(pIceAgent->lock);
                    locked = TRUE;
                    if (!setUseCandidate) {
                        setUseCandidate = TRUE;
                        appendStunFlagAttribute(pIceAgent->pBindingRequest, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE);
                    }
                    doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode);
                    while (pCurNode != NULL) {
                        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
                        pCurNode = pCurNode->pNext;

                        pIceCandidatePair->nominated = TRUE;
                        iceCandidatePairCheckConnection(pIceAgent->pBindingRequest, pIceAgent, pIceCandidatePair);
                    }
                    if (locked) {
                        MUTEX_UNLOCK(pIceAgent->lock);
                    }
                    break;
                case ICE_AGENT_STATE_CONNECTED:
                    // send 'USE_CANDIDATE' for every ice candidate pair
                    setUseCandidate = FALSE;
                    MUTEX_LOCK(pIceAgent->lock);
                    locked = TRUE;
                    doubleListGetHeadNode(pIceAgent->iceCandidatePairs, &pCurNode);
                    while (pCurNode != NULL) {
                        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
                        pCurNode = pCurNode->pNext;

                        pIceCandidatePair->nominated = TRUE;
                    }
                    if (locked) {
                        MUTEX_UNLOCK(pIceAgent->lock);
                    }

                    break;
                default:
                    break;
            }
        };

        // overwrite normal callback
        ((PKvsPeerConnection) answerPc)->pIceAgent->iceAgentCallbacks.connectionStateChangedFn = masterOnIceConnectionStateChangeTest;
        ((PKvsPeerConnection) offerPc)->pIceAgent->iceAgentCallbacks.connectionStateChangedFn = viewerOnIceConnectionStateChangeTest;

        EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

        closePeerConnection(offerPc);
        closePeerConnection(answerPc);

        freePeerConnection(&offerPc);
        freePeerConnection(&answerPc);
        MEMSET(this->stateChangeCount, 0, SIZEOF(SIZE_T) * RTC_PEER_CONNECTION_TOTAL_STATE_COUNT);
        if (::testing::Test::HasFailure()) {
            break;
        }
    }
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
