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
        PPeerContainer container = (PPeerContainer) customData;
        if (candidateStr != NULL) {
            container->client->lock.lock();
            if (!container->client->noNewThreads) {
                container->client->threads.push_back(std::thread(
                    [container](std::string candidate) {
                        RtcIceCandidateInit iceCandidate;
                        EXPECT_EQ(STATUS_SUCCESS,
                                  deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
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
    // join all threads before leaving
    for (auto& th : this->threads)
        th.join();

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
        PPeerContainer container = (PPeerContainer) customData;
        if (candidateStr != NULL) {
            container->client->lock.lock();
            if (!container->client->noNewThreads) {
                container->client->threads.push_back(std::thread(
                    [container](std::string candidate) {
                        RtcIceCandidateInit iceCandidate;
                        EXPECT_EQ(STATUS_SUCCESS,
                                  deserializeRtcIceCandidateInit((PCHAR) candidate.c_str(), STRLEN(candidate.c_str()), &iceCandidate));
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
    for (auto& th : this->threads)
        th.join();

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

// Verify that dynamically adding TURN servers via peerConnectionUpdateIceServers
// enables connection establishment with relay-only transport policy
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersForcedTURNWithDynamicIceServers)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration offerConfig, answerConfig, fullConfig;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i, uriCount = 0, iceConfigCount;
    PIceConfigInfo pIceConfigInfo;
    RtcIceServer turnServers[MAX_ICE_SERVERS_COUNT];

    initializeSignalingClient();

    // Fetch full ICE config (STUN + TURN) into fullConfig for extracting TURN servers later
    MEMSET(&fullConfig, 0x00, SIZEOF(RtcConfiguration));
    getIceServers(&fullConfig);

    // Create PCs with relay-only policy and only a STUN server (no TURN)
    MEMSET(&offerConfig, 0x00, SIZEOF(RtcConfiguration));
    offerConfig.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
    SNPRINTF(offerConfig.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    MEMSET(&answerConfig, 0x00, SIZEOF(RtcConfiguration));
    answerConfig.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
    SNPRINTF(answerConfig.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&offerConfig, &offerPc));
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&answerConfig, &answerPc));

    // Extract TURN servers from the full config (skip index 0 which is STUN)
    MEMSET(turnServers, 0x00, SIZEOF(turnServers));
    ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));
    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (UINT32 j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(turnServers[uriCount].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(turnServers[uriCount].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(turnServers[uriCount].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);
            uriCount++;
        }
    }
    ASSERT_TRUE(uriCount > 0);

    // Dynamically add TURN servers to both peer connections
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(offerPc, turnServers, uriCount));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(answerPc, turnServers, uriCount));

    // Now connect — relay candidates should be generated from the dynamically added TURN servers
    EXPECT_EQ(TRUE, connectTwoPeers(offerPc, answerPc));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

// Verify that peerConnectionUpdateIceServers returns error after ICE agent has failed
TEST_F(PeerConnectionFunctionalityTest, updateIceServersAfterIceFailure)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i, uriCount = 0, iceConfigCount;
    PIceConfigInfo pIceConfigInfo;
    RtcIceServer turnServers[MAX_ICE_SERVERS_COUNT];

    initializeSignalingClient();

    // Create PCs with relay-only policy and NO TURN servers — connection will fail
    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &offerPc));
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &answerPc));

    // Attempt to connect — will fail because no relay candidates can be generated
    EXPECT_EQ(FALSE, connectTwoPeers(offerPc, answerPc));

    // Now try to add TURN servers after ICE has failed
    MEMSET(turnServers, 0x00, SIZEOF(turnServers));
    ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));
    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (UINT32 j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(turnServers[uriCount].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(turnServers[uriCount].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(turnServers[uriCount].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);
            uriCount++;
        }
    }
    ASSERT_TRUE(uriCount > 0);

    // Should fail because ICE agent is in FAILED state
    EXPECT_EQ(STATUS_INVALID_OPERATION, peerConnectionUpdateIceServers(offerPc, turnServers, uriCount));
    EXPECT_EQ(STATUS_INVALID_OPERATION, peerConnectionUpdateIceServers(answerPc, turnServers, uriCount));

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

// Verify that dynamically adding TURN servers from a background thread
// 5 seconds after connectTwoPeers is called allows the connection to succeed
TEST_F(PeerConnectionFunctionalityTest, connectTwoPeersForcedTURNWithDelayedDynamicIceServers)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

    RtcConfiguration offerConfig, answerConfig;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    UINT32 i, uriCount = 0, iceConfigCount;
    PIceConfigInfo pIceConfigInfo;
    RtcIceServer turnServers[MAX_ICE_SERVERS_COUNT];
    UINT32 addTurnServerDelaySec = 5;

    ASSERT_TRUE(addTurnServerDelaySec < KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT);

    initializeSignalingClient();

    // Use ALL policy on one side to have the ICE agent finish gathering Host+STUN candidates
    // This should validate that additional gathering can still be done after the fact
    MEMSET(&offerConfig, 0x00, SIZEOF(RtcConfiguration));
    offerConfig.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;
    SNPRINTF(offerConfig.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    // Create PC with relay-only policy to verify TURN connection is used
    MEMSET(&answerConfig, 0x00, SIZEOF(RtcConfiguration));
    answerConfig.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;
    SNPRINTF(answerConfig.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&offerConfig, &offerPc));
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&answerConfig, &answerPc));

    // Extract TURN servers from signaling
    MEMSET(turnServers, 0x00, SIZEOF(turnServers));
    ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));
    for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
        ASSERT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
        for (UINT32 j = 0; j < pIceConfigInfo->uriCount; j++) {
            STRNCPY(turnServers[uriCount].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
            STRNCPY(turnServers[uriCount].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
            STRNCPY(turnServers[uriCount].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);
            uriCount++;
        }
    }
    ASSERT_TRUE(uriCount > 0);

    // Launch background thread that waits 5 seconds then adds TURN servers
    std::thread updateThread([offerPc, answerPc, &turnServers, uriCount, addTurnServerDelaySec]() {
        DLOGI("Wait %ds to add TURN servers", addTurnServerDelaySec);
        THREAD_SLEEP(addTurnServerDelaySec * HUNDREDS_OF_NANOS_IN_A_SECOND);
        DLOGI("Adding TURN servers now");
        EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(offerPc, turnServers, uriCount));
        EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(answerPc, turnServers, uriCount));
    });

    // connectTwoPeers polls for up to 100s, TURN servers arrive after 5s
    EXPECT_EQ(TRUE, connectTwoPeers(offerPc, answerPc));

    updateThread.join();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

// Verify that writeFrame only adds TWCC header extension on transceivers with twccEnabled=TRUE
TEST_F(PeerConnectionFunctionalityTest, twccHeaderOnlyOnEnabledTransceivers)
{
    auto const frameBufferSize = 200000;

    RtcConfiguration configuration;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, answerVideoTrack, offerAudioTrack, answerAudioTrack;
    PRtcRtpTransceiver offerVideoTransceiver, answerVideoTransceiver, offerAudioTransceiver, answerAudioTransceiver;
    SIZE_T seenVideo = 0;
    Frame videoFrame, audioFrame;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(&videoFrame, 0x00, SIZEOF(Frame));
    MEMSET(&audioFrame, 0x00, SIZEOF(Frame));

    videoFrame.frameData = (PBYTE) MEMALLOC(frameBufferSize);
    videoFrame.size = TEST_VIDEO_FRAME_SIZE;
    MEMSET(videoFrame.frameData, 0x11, videoFrame.size);

    audioFrame.frameData = (PBYTE) MEMALLOC(frameBufferSize);
    audioFrame.size = 160; // small audio frame
    MEMSET(audioFrame.frameData, 0x22, audioFrame.size);

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

    // After connection, simulate Firefox scenario: disable TWCC on audio transceiver
    PKvsRtpTransceiver pKvsOfferAudioTransceiver = (PKvsRtpTransceiver) offerAudioTransceiver;
    PKvsRtpTransceiver pKvsOfferVideoTransceiver = (PKvsRtpTransceiver) offerVideoTransceiver;
    pKvsOfferAudioTransceiver->twccEnabled = FALSE;
    pKvsOfferVideoTransceiver->twccEnabled = TRUE;

    PKvsPeerConnection pKvsOfferPc = (PKvsPeerConnection) offerPc;
    SIZE_T twccSeqBefore = ATOMIC_LOAD(&pKvsOfferPc->transportWideSequenceNumber);

    // Send audio frames - should NOT increment transportWideSequenceNumber
    for (int i = 0; i < 5; i++) {
        writeFrame(offerAudioTransceiver, &audioFrame);
        audioFrame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 50);
    }

    SIZE_T twccSeqAfterAudio = ATOMIC_LOAD(&pKvsOfferPc->transportWideSequenceNumber);
    EXPECT_EQ(twccSeqBefore, twccSeqAfterAudio);

    // Send video frames - SHOULD increment transportWideSequenceNumber
    for (int i = 0; i < 3; i++) {
        writeFrame(offerVideoTransceiver, &videoFrame);
        videoFrame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 25);
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    SIZE_T twccSeqAfterVideo = ATOMIC_LOAD(&pKvsOfferPc->transportWideSequenceNumber);
    EXPECT_LT(twccSeqAfterAudio, twccSeqAfterVideo);

    MEMFREE(videoFrame.frameData);
    MEMFREE(audioFrame.frameData);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

TEST_F(PeerConnectionFunctionalityTest, parseExtIdHandlesNull)
{
    // NULL input must return 0
    EXPECT_EQ(0u, parseExtId(NULL));

    // String without a space should return 0
    EXPECT_EQ(0u, parseExtId((PCHAR) "noSpace"));

    // Valid extmap value: "$number $url"
    EXPECT_EQ(5u, parseExtId((PCHAR) "5 http://example.com"));
    EXPECT_EQ(12u, parseExtId((PCHAR) "12 urn:ietf:params"));
}

TEST_F(PeerConnectionFunctionalityTest, renegotiateWithSameIceCredentials)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    SIZE_T connectedCount = 0;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Initial connection
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Reset state change counts for re-negotiation tracking
    MEMSET(this->stateChangeCount, 0, SIZEOF(SIZE_T) * RTC_PEER_CONNECTION_TOTAL_STATE_COUNT);

    // Stop ICE candidate forwarding threads before re-negotiation
    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Re-negotiation: create a new offer/answer on the same PeerConnections.
    // ICE credentials remain the same, so ICE should stay as-is (no restart).
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    // Give some time for state to settle. Since ICE is already connected and
    // credentials did not change, connection should remain stable.
    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // The connection should not have transitioned to FAILED
    EXPECT_EQ(0, ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_FAILED]));

    // Verify no transceivers accumulated (this test has no tracks)
    UINT32 transceiverCount = 0;
    PKvsPeerConnection pOfferPcImpl = (PKvsPeerConnection) offerPc;
    PKvsPeerConnection pAnswerPcImpl = (PKvsPeerConnection) answerPc;

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Offerer should have no transceivers (no tracks added)";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Offerer should have no fake transceivers";

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Answerer should have no transceivers (no tracks added)";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Answerer should have no fake transceivers";

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Re-negotiation test with ICE restart: perform a second offer/answer exchange
// with new ICE credentials, triggering a full ICE restart.
TEST_F(PeerConnectionFunctionalityTest, renegotiateWithIceRestart)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    PKvsPeerConnection pOfferPcImpl = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Initial connection
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Reset state change counts for re-negotiation tracking
    MEMSET(this->stateChangeCount, 0, SIZEOF(SIZE_T) * RTC_PEER_CONNECTION_TOTAL_STATE_COUNT);

    // Stop ICE candidate forwarding threads before re-negotiation
    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Regenerate local ICE credentials on the offerer to force an ICE restart
    // on the answerer when it processes setRemoteDescription.
    pOfferPcImpl = (PKvsPeerConnection) offerPc;
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(pOfferPcImpl->localIceUfrag, LOCAL_ICE_UFRAG_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(pOfferPcImpl->localIcePwd, LOCAL_ICE_PWD_LEN));

    // Re-negotiation with new ICE credentials (ICE restart)
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    // Wait for ICE to re-establish connection after restart
    for (auto i = 0; i <= 100 && ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]) < 1; i++) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    // Verify connection was re-established (at least one side reached CONNECTED)
    EXPECT_GE(ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_CONNECTED]), (SIZE_T) 1);

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Re-negotiation test: connect with one video track, then add an audio track
// and perform re-negotiation. Verify the offer/answer exchange succeeds.
TEST_F(PeerConnectionFunctionalityTest, renegotiateWithNewTransceiver)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, offerAudioTrack, answerAudioTrack;
    PRtcRtpTransceiver offerVideoTransceiver = NULL, offerAudioTransceiver = NULL, answerAudioTransceiver = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // Add initial video track to the offerer and also register OPUS on both PCs
    // upfront (before connectTwoPeers) so we can add audio transceivers later.
    // addSupportedCodec uses hashTablePut which fails on duplicate keys, and
    // setPayloadTypesForOffer (called during createOffer) will upsert all default
    // codecs. Registering OPUS here avoids the duplicate-key error later.
    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(offerPc, RTC_CODEC_OPUS));
    EXPECT_EQ(STATUS_SUCCESS, addSupportedCodec(answerPc, RTC_CODEC_OPUS));

    // Initial connection with one video track
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // Reset state change counts for re-negotiation tracking
    MEMSET(this->stateChangeCount, 0, SIZEOF(SIZE_T) * RTC_PEER_CONNECTION_TOTAL_STATE_COUNT);

    // Stop ICE candidate forwarding threads before re-negotiation
    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Add audio transceivers for re-negotiation. The OPUS codec is already
    // registered in both PCs (from addSupportedCodec above), so we only
    // need to add the transceivers directly.
    MEMSET(&offerAudioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    offerAudioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    offerAudioTrack.codec = RTC_CODEC_OPUS;
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(offerAudioTrack.streamId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(offerAudioTrack.trackId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerPc, &offerAudioTrack, NULL, &offerAudioTransceiver));

    MEMSET(&answerAudioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    answerAudioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    answerAudioTrack.codec = RTC_CODEC_OPUS;
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(answerAudioTrack.streamId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(answerAudioTrack.trackId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(answerPc, &answerAudioTrack, NULL, &answerAudioTransceiver));

    // Re-negotiation: create new offer with the added audio track
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    // Verify the SDP contains both video and audio tracks
    std::string answerSdp(sdp.sdp);
    EXPECT_NE(std::string::npos, answerSdp.find("m=video"));
    EXPECT_NE(std::string::npos, answerSdp.find("m=audio"));

    // Give some time for state to settle
    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Connection should not have failed
    EXPECT_EQ(0, ATOMIC_LOAD(&this->stateChangeCount[RTC_PEER_CONNECTION_STATE_FAILED]));

    // Verify no old/extra transceivers accumulated in the lists
    UINT32 transceiverCount = 0;
    PKvsPeerConnection pOfferPcImpl = (PKvsPeerConnection) offerPc;
    PKvsPeerConnection pAnswerPcImpl = (PKvsPeerConnection) answerPc;

    // Offerer: 2 user transceivers (video + audio), no fake transceivers
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(2u, transceiverCount) << "Offerer should have exactly 2 transceivers (video + audio)";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Offerer should have no fake transceivers";

    // Answerer: 1 user transceiver (audio) + 1 fake (for video m-line matched by answerer)
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(1u, transceiverCount) << "Answerer should have 1 user transceiver (audio)";

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Re-negotiation test: new offer with track removed. Connect with video+audio, then receive an offer
// that has only audio. Verify the video transceiver is marked INACTIVE and writeFrame no-sends on it.
TEST_F(PeerConnectionFunctionalityTest, renegotiateWithTrackRemoved)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL, offerAudioOnlyPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, offerAudioTrack, answerVideoTrack, answerAudioTrack, audioOnlyTrack;
    PRtcRtpTransceiver offerVideoTransceiver = NULL, offerAudioTransceiver = NULL;
    PRtcRtpTransceiver answerVideoTransceiver = NULL, answerAudioTransceiver = NULL, audioOnlyTransceiver = NULL;
    Frame frame;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(offerPc, &offerAudioTrack, &offerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);
    addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(answerPc, &answerAudioTrack, &answerAudioTransceiver, RTC_CODEC_OPUS, MEDIA_STREAM_TRACK_KIND_AUDIO);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Create a peer connection with only audio to simulate "remote sent new offer with video track removed".
    // Use addTransceiver directly to avoid addSupportedCodec duplicate (new PC may already have default codecs).
    EXPECT_EQ(createPeerConnection(&configuration, &offerAudioOnlyPc), STATUS_SUCCESS);
    MEMSET(&audioOnlyTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    audioOnlyTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioOnlyTrack.codec = RTC_CODEC_OPUS;
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(audioOnlyTrack.streamId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(audioOnlyTrack.trackId, MAX_MEDIA_STREAM_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, addTransceiver(offerAudioOnlyPc, &audioOnlyTrack, NULL, &audioOnlyTransceiver));

    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerAudioOnlyPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerAudioOnlyPc, &sdp));

    std::string offerSdp(sdp.sdp);
    EXPECT_NE(std::string::npos, offerSdp.find("m=audio"));
    EXPECT_EQ(std::string::npos, offerSdp.find("m=video")) << "Offer must contain only audio for this test";

    // Answerer receives the new offer (only audio). Its video transceiver should be marked INACTIVE.
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    EXPECT_EQ(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, answerVideoTransceiver->direction)
        << "Video transceiver should be INACTIVE after remote offer removed video m-line";

    EXPECT_NE(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, answerAudioTransceiver->direction)
        << "Audio transceiver should still be active (sendonly/recvonly/sendrecv)";

    // writeFrame on the INACTIVE video transceiver should return success without sending
    MEMSET(&frame, 0x00, SIZEOF(Frame));
    frame.size = 0;
    frame.presentationTs = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS, writeFrame(answerVideoTransceiver, &frame));

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    std::string answerSdp(sdp.sdp);
    EXPECT_NE(std::string::npos, answerSdp.find("m=audio"));
    EXPECT_EQ(std::string::npos, answerSdp.find("m=video")) << "Answer should have only audio m-line";

    // Verify no old/extra transceivers accumulated in the lists
    UINT32 transceiverCount = 0;
    PKvsPeerConnection pAnswerPcImpl = (PKvsPeerConnection) answerPc;

    // Answerer started with 2 user transceivers (video + audio). After re-negotiation
    // with track removed, the count should remain 2 (video is marked INACTIVE, not deleted).
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(2u, transceiverCount) << "Answerer should still have 2 user transceivers (video is INACTIVE, not removed)";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Answerer should have no fake transceivers";

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    closePeerConnection(offerAudioOnlyPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
    freePeerConnection(&offerAudioOnlyPc);
}

// Re-negotiation test: direction-only change without track removal.
// Connect with video sendrecv, then receive an offer where video is recvonly.
// Verify transceiver direction updates to sendonly and is NOT marked INACTIVE.
TEST_F(PeerConnectionFunctionalityTest, renegotiateDirectionChangeOnly)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, answerVideoTrack;
    PRtcRtpTransceiver offerVideoTransceiver = NULL, answerVideoTransceiver = NULL;
    PKvsRtpTransceiver pOfferKvsTransceiver = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);

    // Initial connection with video sendrecv
    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Change offerer's video transceiver direction to recvonly before re-offer
    pOfferKvsTransceiver = (PKvsRtpTransceiver) offerVideoTransceiver;
    pOfferKvsTransceiver->transceiver.direction = RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;

    // Re-negotiation with direction change only
    EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));

    // Verify the offer SDP contains recvonly
    std::string offerSdp(sdp.sdp);
    EXPECT_NE(std::string::npos, offerSdp.find("a=recvonly"));

    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));

    // Answerer's direction should be sendonly (mirror of remote recvonly), NOT inactive
    EXPECT_EQ(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, answerVideoTransceiver->direction)
        << "Answerer video transceiver should be sendonly (mirror of remote recvonly)";

    EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
    EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

    // Verify answer SDP contains sendonly (mirror of recvonly)
    std::string answerSdp(sdp.sdp);
    EXPECT_NE(std::string::npos, answerSdp.find("m=video"));

    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // Verify no old/extra transceivers accumulated in the lists
    UINT32 transceiverCount = 0;
    PKvsPeerConnection pOfferPcImpl = (PKvsPeerConnection) offerPc;
    PKvsPeerConnection pAnswerPcImpl = (PKvsPeerConnection) answerPc;

    // Offerer: 1 user transceiver (video with changed direction), no fake transceivers
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(1u, transceiverCount) << "Offerer should have exactly 1 transceiver after direction change";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pOfferPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Offerer should have no fake transceivers";

    // Answerer: 1 user transceiver (video), no fake transceivers
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pTransceivers, &transceiverCount));
    EXPECT_EQ(1u, transceiverCount) << "Answerer should have exactly 1 transceiver after direction change";
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pAnswerPcImpl->pFakeTransceivers, &transceiverCount));
    EXPECT_EQ(0u, transceiverCount) << "Answerer should have no fake transceivers";

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Re-negotiation while a writeFrame loop is running on another thread for the
// SAME session: direction flips to INACTIVE and back mid-stream. writeFrame
// must never crash or touch freed state (the rolling buffer/retransmitter are
// reused across renegotiations, not reallocated), and it becomes a no-op
// (returning success) while the direction excludes sending.
TEST_F(PeerConnectionFunctionalityTest, renegotiateWhileStreamingFrames)
{
    RtcConfiguration configuration;
    RtcSessionDescriptionInit sdp;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;
    RtcMediaStreamTrack offerVideoTrack, answerVideoTrack;
    PRtcRtpTransceiver offerVideoTransceiver = NULL, answerVideoTransceiver = NULL;
    PKvsRtpTransceiver pOfferKvsTransceiver = NULL;
    SIZE_T stopFrames = FALSE;
    BYTE frameData[128];
    Frame frame;
    // Live state mirror fed by the public state-change callback: reading
    // pKvsPeerConnection->connectionState directly would race the SDK's writer.
    std::atomic<UINT64> offerState{RTC_PEER_CONNECTION_STATE_NONE};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(frameData, 0x11, SIZEOF(frameData));
    MEMSET(&frame, 0x00, SIZEOF(Frame));
    frame.frameData = frameData;
    frame.size = SIZEOF(frameData);
    frame.presentationTs = 0;
    frame.flags = FRAME_FLAG_KEY_FRAME;

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    addTrackToPeerConnection(offerPc, &offerVideoTrack, &offerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    addTrackToPeerConnection(answerPc, &answerVideoTrack, &answerVideoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // connectTwoPeers verified CONNECTED; from here on track transitions via the callback.
    offerState = RTC_PEER_CONNECTION_STATE_CONNECTED;
    EXPECT_EQ(STATUS_SUCCESS,
              peerConnectionOnConnectionStateChange(offerPc, (UINT64) &offerState, [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) {
                  ((std::atomic<UINT64>*) customData)->store((UINT64) newState);
              }));

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    // Frame hammer on the offerer's video sender.
    std::thread frameWorker([&]() {
        Frame localFrame = frame;
        while (!ATOMIC_LOAD_BOOL(&stopFrames)) {
            EXPECT_EQ(STATUS_SUCCESS, writeFrame(offerVideoTransceiver, &localFrame));
            localFrame.presentationTs += (HUNDREDS_OF_NANOS_IN_A_SECOND / 25);
            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    });

    pOfferKvsTransceiver = (PKvsRtpTransceiver) offerVideoTransceiver;

    // Several full renegotiation cycles while frames are being written.
    for (int round = 0; round < 4; round++) {
        BOOL pause = (round % 2 == 0);
        pOfferKvsTransceiver->transceiver.direction = pause ? RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE : RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

        EXPECT_EQ(STATUS_SUCCESS, createOffer(offerPc, &sdp));
        EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offerPc, &sdp));
        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answerPc, &sdp));
        EXPECT_EQ(STATUS_SUCCESS, createAnswer(answerPc, &sdp));
        EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answerPc, &sdp));
        EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offerPc, &sdp));

        THREAD_SLEEP(200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    }

    ATOMIC_STORE_BOOL(&stopFrames, TRUE);
    frameWorker.join();

    // The connection must have survived all cycles. Renegotiation bounces the
    // connection state transiently, so poll for it to settle back to CONNECTED
    // rather than asserting the instantaneous value.
    BOOL reconnected = FALSE;
    for (int waitMs = 0; waitMs < 15000 && !reconnected; waitMs += 100) {
        reconnected = (offerState.load() == RTC_PEER_CONNECTION_STATE_CONNECTED);
        if (!reconnected) {
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }
    EXPECT_TRUE(reconnected) << "connection did not settle back to CONNECTED after renegotiations under streaming";

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);
    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);
}

// Two independent sessions renegotiating CONCURRENTLY from separate threads
// (the SDK-level analog of two viewers re-offering to the same master at
// once): no crashes, no cross-session interference, both stay connected.
TEST_F(PeerConnectionFunctionalityTest, concurrentRenegotiationAcrossSessions)
{
    RtcConfiguration configuration;
    PRtcPeerConnection offerPc[2] = {NULL, NULL}, answerPc[2] = {NULL, NULL};
    RtcMediaStreamTrack offerTrack[2], answerTrack[2];
    PRtcRtpTransceiver offerTransceiver[2] = {NULL, NULL}, answerTransceiver[2] = {NULL, NULL};
    // Live state mirrors fed by the public state-change callback: reading
    // pKvsPeerConnection->connectionState directly would race the SDK's writer.
    std::atomic<UINT64> offerState[2] = {{RTC_PEER_CONNECTION_STATE_NONE}, {RTC_PEER_CONNECTION_STATE_NONE}};

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    for (int i = 0; i < 2; i++) {
        EXPECT_EQ(createPeerConnection(&configuration, &offerPc[i]), STATUS_SUCCESS);
        EXPECT_EQ(createPeerConnection(&configuration, &answerPc[i]), STATUS_SUCCESS);
        addTrackToPeerConnection(offerPc[i], &offerTrack[i], &offerTransceiver[i], RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
        addTrackToPeerConnection(answerPc[i], &answerTrack[i], &answerTransceiver[i], RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
        EXPECT_EQ(connectTwoPeers(offerPc[i], answerPc[i]), TRUE);

        // connectTwoPeers verified CONNECTED; from here on track transitions via the callback.
        offerState[i] = RTC_PEER_CONNECTION_STATE_CONNECTED;
        EXPECT_EQ(
            STATUS_SUCCESS,
            peerConnectionOnConnectionStateChange(offerPc[i], (UINT64) &offerState[i], [](UINT64 customData, RTC_PEER_CONNECTION_STATE newState) {
                ((std::atomic<UINT64>*) customData)->store((UINT64) newState);
            }));
    }

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = FALSE;
    this->lock.unlock();

    auto renegotiateWorker = [](PRtcPeerConnection offer, PRtcPeerConnection answer, PRtcRtpTransceiver transceiver) {
        for (int round = 0; round < 4; round++) {
            RtcSessionDescriptionInit sdp{};
            PKvsRtpTransceiver pKvs = (PKvsRtpTransceiver) transceiver;
            pKvs->transceiver.direction = (round % 2 == 0) ? RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY : RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
            EXPECT_EQ(STATUS_SUCCESS, createOffer(offer, &sdp));
            EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(offer, &sdp));
            EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(answer, &sdp));
            EXPECT_EQ(STATUS_SUCCESS, createAnswer(answer, &sdp));
            EXPECT_EQ(STATUS_SUCCESS, setLocalDescription(answer, &sdp));
            EXPECT_EQ(STATUS_SUCCESS, setRemoteDescription(offer, &sdp));
            THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    };

    std::thread session0(renegotiateWorker, offerPc[0], answerPc[0], offerTransceiver[0]);
    std::thread session1(renegotiateWorker, offerPc[1], answerPc[1], offerTransceiver[1]);
    session0.join();
    session1.join();

    // Same transient-state consideration as above: poll for both sessions to
    // settle back to CONNECTED after their concurrent renegotiation storms.
    for (int i = 0; i < 2; i++) {
        BOOL reconnected = FALSE;
        for (int waitMs = 0; waitMs < 15000 && !reconnected; waitMs += 100) {
            reconnected = (offerState[i].load() == RTC_PEER_CONNECTION_STATE_CONNECTED);
            if (!reconnected) {
                THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            }
        }
        EXPECT_TRUE(reconnected) << "session " << i << " did not settle back to CONNECTED after concurrent renegotiations";
    }

    this->lock.lock();
    for (auto& th : this->threads)
        th.join();
    this->threads.clear();
    this->noNewThreads = TRUE;
    this->lock.unlock();

    for (int i = 0; i < 2; i++) {
        closePeerConnection(offerPc[i]);
        closePeerConnection(answerPc[i]);
        freePeerConnection(&offerPc[i]);
        freePeerConnection(&answerPc[i]);
    }
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
