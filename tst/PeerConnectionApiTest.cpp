#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class PeerConnectionApiTest : public WebRtcClientTestBase {};

TEST_F(PeerConnectionApiTest, deserializeRtcIceCandidateInit)
{
    RtcIceCandidateInit rtcIceCandidateInit;

    MEMSET(&rtcIceCandidateInit, 0x00, SIZEOF(rtcIceCandidateInit));

    auto notAnObject = "helloWorld";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) notAnObject, STRLEN(notAnObject), &rtcIceCandidateInit), STATUS_INVALID_API_CALL_RETURN_JSON);

    auto emptyObject = "{}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) emptyObject, STRLEN(emptyObject), &rtcIceCandidateInit), STATUS_INVALID_API_CALL_RETURN_JSON);

    auto noCandidate = "{randomKey: \"randomValue\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) noCandidate, STRLEN(noCandidate), &rtcIceCandidateInit), STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

    auto keyNoValue = "{1,2,3,4,5}candidate";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) keyNoValue, STRLEN(keyNoValue), &rtcIceCandidateInit), STATUS_ICE_CANDIDATE_MISSING_CANDIDATE);

    auto validCandidate = "{candidate: \"foobar\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) validCandidate, STRLEN(validCandidate), &rtcIceCandidateInit), STATUS_SUCCESS);
    EXPECT_STREQ(rtcIceCandidateInit.candidate, "foobar");

    auto validCandidate2 = "{candidate: \"candidate: 1 2 3\", \"sdpMid\": 0}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) validCandidate2, STRLEN(validCandidate2), &rtcIceCandidateInit), STATUS_SUCCESS);
    EXPECT_STREQ(rtcIceCandidateInit.candidate, "candidate: 1 2 3");

    std::string oversizedCandidate = "{candidate: \"";
    oversizedCandidate += std::string(MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN + 10, 'A');
    oversizedCandidate += "\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) oversizedCandidate.c_str(), STRLEN(oversizedCandidate.c_str()), &rtcIceCandidateInit),
              STATUS_ICE_CANDIDATE_INIT_MALFORMED);

    std::string maxLengthCandidate = "{candidate: \"";
    maxLengthCandidate += std::string(MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN, 'B');
    maxLengthCandidate += "\"}";
    EXPECT_EQ(deserializeRtcIceCandidateInit((PCHAR) maxLengthCandidate.c_str(), STRLEN(maxLengthCandidate.c_str()), &rtcIceCandidateInit), STATUS_SUCCESS);
}

TEST_F(PeerConnectionApiTest, serializeSessionDescriptionInit)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    UINT32 sessionDescriptionJSONLen = 0;
    CHAR sessionDescriptionJSON[500] = {0};

    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    EXPECT_EQ(serializeSessionDescriptionInit(NULL, sessionDescriptionJSON, &sessionDescriptionJSONLen), STATUS_NULL_ARG);
    EXPECT_EQ(serializeSessionDescriptionInit(&rtcSessionDescriptionInit, sessionDescriptionJSON, NULL), STATUS_NULL_ARG);

    STRCPY(rtcSessionDescriptionInit.sdp, "KVS\nWebRTC\nSDP\nValue\n");
    rtcSessionDescriptionInit.type = SDP_TYPE_OFFER;
    sessionDescriptionJSONLen = 500;

    EXPECT_EQ(serializeSessionDescriptionInit(&rtcSessionDescriptionInit, sessionDescriptionJSON, &sessionDescriptionJSONLen), STATUS_SUCCESS);
    EXPECT_STREQ(sessionDescriptionJSON, "{\"type\": \"offer\", \"sdp\": \"KVS\\r\\nWebRTC\\r\\nSDP\\r\\nValue\\r\\n\"}");
}

TEST_F(PeerConnectionApiTest, suppliedCertificatesVariation)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    // Private key is null but the size is not zero
    configuration.certificates[0].pCertificate = (PBYTE) 1;
    configuration.certificates[0].certificateSize = 0;
    configuration.certificates[0].pPrivateKey = NULL;
    configuration.certificates[0].privateKeySize = 1;
    EXPECT_EQ(STATUS_SSL_INVALID_CERTIFICATE_BITS, createPeerConnection(&configuration, &pRtcPeerConnection));

    // Private key is null but the size is not zero with specified size for the cert
    configuration.certificates[0].pCertificate = (PBYTE) 1;
    configuration.certificates[0].certificateSize = 100;
    configuration.certificates[0].pPrivateKey = NULL;
    configuration.certificates[0].privateKeySize = 1;
    EXPECT_EQ(STATUS_SSL_INVALID_CERTIFICATE_BITS, createPeerConnection(&configuration, &pRtcPeerConnection));

    // Bad private key size later in the chain that should be ignored
    configuration.certificates[0].pCertificate = NULL;
    configuration.certificates[0].certificateSize = 0;
    configuration.certificates[0].pPrivateKey = NULL;
    configuration.certificates[0].privateKeySize = 1;
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));

    // Bad private key size later in the chain with cert size not zero that should be ignored
    configuration.certificates[0].pCertificate = NULL;
    configuration.certificates[0].certificateSize = 100;
    configuration.certificates[0].pPrivateKey = NULL;
    configuration.certificates[0].privateKeySize = 1;
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(PeerConnectionApiTest, deserializeSessionDescriptionInit)
{
    RtcSessionDescriptionInit rtcSessionDescriptionInit;
    MEMSET(&rtcSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    auto notAnObject = "helloWorld";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) notAnObject, STRLEN(notAnObject), &rtcSessionDescriptionInit),
              STATUS_INVALID_API_CALL_RETURN_JSON);

    auto emptyObject = "{}";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) emptyObject, STRLEN(emptyObject), &rtcSessionDescriptionInit),
              STATUS_INVALID_API_CALL_RETURN_JSON);

    auto noSDPKey = "{type: \"offer\"}";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) noSDPKey, STRLEN(noSDPKey), &rtcSessionDescriptionInit),
              STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP);

    auto noTypeKey = "{\"sdp\": \"KVS\\r\\nWebRTC\\r\\nSDP\\r\\nValue\\r\\n\"}";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) noTypeKey, STRLEN(noTypeKey), &rtcSessionDescriptionInit),
              STATUS_SESSION_DESCRIPTION_INIT_MISSING_TYPE);

    auto invalidTypeKey = "{sdp: \"kvsSdp\", type: \"foobar\"}";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) invalidTypeKey, STRLEN(invalidTypeKey), &rtcSessionDescriptionInit),
              STATUS_SESSION_DESCRIPTION_INIT_INVALID_TYPE);

    auto keyNoValue = "{1,2,3,4,5}sdp";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) keyNoValue, STRLEN(keyNoValue), &rtcSessionDescriptionInit),
              STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP);

    auto validSessionDescriptionInit = "{sdp: \"KVS\\r\\nWebRTC\\r\\nSDP\\r\\nValue\\r\\n\", type: \"offer\"}";
    EXPECT_EQ(deserializeSessionDescriptionInit((PCHAR) validSessionDescriptionInit, STRLEN(validSessionDescriptionInit), &rtcSessionDescriptionInit),
              STATUS_SUCCESS);
    EXPECT_STREQ(rtcSessionDescriptionInit.sdp, "KVS\r\nWebRTC\r\nSDP\r\nValue\r\n");
    EXPECT_EQ(rtcSessionDescriptionInit.type, SDP_TYPE_OFFER);
}

TEST_F(PeerConnectionApiTest, fmtpForPayloadType)
{
    auto rawSessionDescription = R"(v=0
o=- 686950092 1576880200 IN IP4 0.0.0.0
s=-
t=0 0
m=audio 9 UDP/TLS/RTP/SAVPF 109
a=rtpmap:109 opus/48000/2
a=fmtp:109 minptime=10;useinbandfec=1
m=video 9 UDP/TLS/RTP/SAVPF 97
a=rtpmap:97 H264/90000
a=fmtp:97 profile-level-id=42e01f;level-asymmetry-allowed=1
)";

    SessionDescription sessionDescription;
    MEMSET(&sessionDescription, 0x00, SIZEOF(SessionDescription));
    EXPECT_EQ(deserializeSessionDescription(&sessionDescription, (PCHAR) rawSessionDescription), STATUS_SUCCESS);

    EXPECT_STREQ(fmtpForPayloadType(97, &sessionDescription), "profile-level-id=42e01f;level-asymmetry-allowed=1");
    EXPECT_STREQ(fmtpForPayloadType(109, &sessionDescription), "minptime=10;useinbandfec=1");
    EXPECT_STREQ(fmtpForPayloadType(25, &sessionDescription), NULL);
}

TEST_F(PeerConnectionApiTest, CONVERT_TIMESTAMP_TO_RTP_BigTimestamp)
{
    UINT64 timestamp = 16034753564030000;
    UINT64 rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(VIDEO_CLOCKRATE, timestamp);
    EXPECT_EQ(144312782076270, rtpTimestamp);
}

TEST_F(PeerConnectionApiTest, CONVERT_TIMESTAMP_TO_RTP_MacroWithMathOperations)
{
    UINT64 rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(40000 + 50000, HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_EQ(90000, rtpTimestamp);

    rtpTimestamp = CONVERT_TIMESTAMP_TO_RTP(90000, HUNDREDS_OF_NANOS_IN_A_SECOND + HUNDREDS_OF_NANOS_IN_A_SECOND);
    EXPECT_EQ(180000, rtpTimestamp);
}

RTC_PEER_CONNECTION_STATE fromIceAgentState(PRtcPeerConnection pRtcPeerConnection, UINT64 iceConnectionState)
{
    ((PKvsPeerConnection) pRtcPeerConnection)->connectionState = RTC_PEER_CONNECTION_STATE_NONE;
    RTC_PEER_CONNECTION_STATE state = RTC_PEER_CONNECTION_STATE_NONE;
    peerConnectionOnConnectionStateChange(pRtcPeerConnection, (UINT64) &state, [](UINT64 state64, RTC_PEER_CONNECTION_STATE newState) {
        *(RTC_PEER_CONNECTION_STATE*) state64 = newState;
    });
    onIceConnectionStateChange((UINT64) pRtcPeerConnection, iceConnectionState);
    return state;
}

TEST_F(PeerConnectionApiTest, connectionState)
{
    PRtcPeerConnection pc = nullptr;
    RtcConfiguration config{};
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pc));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_NEW, fromIceAgentState(pc, ICE_AGENT_STATE_NEW));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_CONNECTING, fromIceAgentState(pc, ICE_AGENT_STATE_CHECK_CONNECTION));
    // RTC_PEER_CONNECTION_STATE is set to CONNECTED only when dtls is connected therefore ICE_AGENT_STATE_CONNECTED is still considered CONNECTING
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_CONNECTING, fromIceAgentState(pc, ICE_AGENT_STATE_CONNECTED));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_CONNECTING, fromIceAgentState(pc, ICE_AGENT_STATE_NOMINATING));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_CONNECTING, fromIceAgentState(pc, ICE_AGENT_STATE_READY));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_DISCONNECTED, fromIceAgentState(pc, ICE_AGENT_STATE_DISCONNECTED));
    EXPECT_EQ(RTC_PEER_CONNECTION_STATE_FAILED, fromIceAgentState(pc, ICE_AGENT_STATE_FAILED));

    closePeerConnection(pc);
    freePeerConnection(&pc);
}

TEST_F(PeerConnectionApiTest, peerConnectionUpdateIceServersNullArgs)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    RtcConfiguration configuration;
    RtcIceServer iceServers[1];

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(iceServers, 0x00, SIZEOF(iceServers));
    SNPRINTF(iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443");

    // NULL peer connection
    EXPECT_EQ(STATUS_NULL_ARG, peerConnectionUpdateIceServers(NULL, iceServers, 1));

    // NULL ice servers array
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_NULL_ARG, peerConnectionUpdateIceServers(pRtcPeerConnection, NULL, 1));

    // Zero count
    EXPECT_EQ(STATUS_INVALID_ARG, peerConnectionUpdateIceServers(pRtcPeerConnection, iceServers, 0));

    closePeerConnection(pRtcPeerConnection);
    freePeerConnection(&pRtcPeerConnection);
}

TEST_F(PeerConnectionApiTest, peerConnectionUpdateIceServersSuccess)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    RtcConfiguration configuration;
    RtcIceServer iceServers[1];

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(iceServers, 0x00, SIZEOF(iceServers));
    SNPRINTF(iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443");

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(pRtcPeerConnection, iceServers, 1));

    closePeerConnection(pRtcPeerConnection);
    freePeerConnection(&pRtcPeerConnection);
}

TEST_F(PeerConnectionApiTest, peerConnectionUpdateIceServersDuplicate)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    RtcConfiguration configuration;
    RtcIceServer iceServers[1];

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Create peer connection without any ICE servers
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    // Add a STUN server via the update API
    MEMSET(iceServers, 0x00, SIZEOF(iceServers));
    SNPRINTF(iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443");
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(pRtcPeerConnection, iceServers, 1));

    // Try to add the same server again -- should succeed with duplicate silently skipped
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionUpdateIceServers(pRtcPeerConnection, iceServers, 1));

    closePeerConnection(pRtcPeerConnection);
    freePeerConnection(&pRtcPeerConnection);
}

TEST_F(PeerConnectionApiTest, onSetStunServerIpCopiesCompleteAddress)
{
    PWebRtcClientContext pWebRtcClientContext;
    PStunIpAddrContext pStunIpAddrCtx;
    StunIpAddrContext savedCtx;
    DualKvsIpAddresses ipAddresses;
    const BYTE ipv4[IPV4_ADDRESS_LENGTH] = {203, 0, 113, 7};
    const BYTE ipv6[IPV6_ADDRESS_LENGTH] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x42};
    BOOL createdHere = FALSE, resolverDone = FALSE;
    UINT32 i;

    // The client instance is only created by initKvsWebRtc() when ENABLE_KVS_THREADPOOL is on
    pWebRtcClientContext = getWebRtcClientInstance();
    if (!ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized)) {
        releaseHoldOnInstance(pWebRtcClientContext);
        ASSERT_EQ(STATUS_SUCCESS, createWebRtcClientInstance());
        createdHere = TRUE;
        pWebRtcClientContext = getWebRtcClientInstance();
    }
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized));
    pStunIpAddrCtx = pWebRtcClientContext->pStunIpAddrCtx;
    ASSERT_TRUE(pStunIpAddrCtx != NULL);

    // Wait for the startup STUN resolution thread; it sets startTime when done
    for (i = 0; i < 1000 && !createdHere && !resolverDone; i++) {
        MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
        resolverDone = (pStunIpAddrCtx->startTime != 0);
        MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
        if (!resolverDone) {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    savedCtx = *pStunIpAddrCtx;
    STRCPY(pStunIpAddrCtx->hostname, "stun.test.invalid");
    MEMSET(&pStunIpAddrCtx->kvsIpAddresses, 0x00, SIZEOF(DualKvsIpAddresses));
    pStunIpAddrCtx->kvsIpAddresses.ipv4Address.family = KVS_IP_FAMILY_TYPE_IPV4;
    pStunIpAddrCtx->kvsIpAddresses.ipv4Address.port = 3478;
    MEMCPY(pStunIpAddrCtx->kvsIpAddresses.ipv4Address.address, ipv4, IPV4_ADDRESS_LENGTH);
    pStunIpAddrCtx->kvsIpAddresses.ipv6Address.family = KVS_IP_FAMILY_TYPE_IPV6;
    pStunIpAddrCtx->kvsIpAddresses.ipv6Address.port = 3479;
    MEMCPY(pStunIpAddrCtx->kvsIpAddresses.ipv6Address.address, ipv6, IPV6_ADDRESS_LENGTH);
    pStunIpAddrCtx->isIpInitialized = TRUE;
    pStunIpAddrCtx->startTime = GETTIME();
    pStunIpAddrCtx->expirationDuration = 2 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);

    MEMSET(&ipAddresses, 0x00, SIZEOF(ipAddresses));
    EXPECT_EQ(STATUS_SUCCESS, onSetStunServerIp(0, (PCHAR) "stun.test.invalid", &ipAddresses));

    EXPECT_EQ(KVS_IP_FAMILY_TYPE_IPV4, ipAddresses.ipv4Address.family);
    EXPECT_EQ(3478, ipAddresses.ipv4Address.port);
    EXPECT_EQ(0, MEMCMP(ipAddresses.ipv4Address.address, ipv4, IPV4_ADDRESS_LENGTH));
    EXPECT_EQ(KVS_IP_FAMILY_TYPE_IPV6, ipAddresses.ipv6Address.family);
    EXPECT_EQ(3479, ipAddresses.ipv6Address.port);
    EXPECT_EQ(0, MEMCMP(ipAddresses.ipv6Address.address, ipv6, IPV6_ADDRESS_LENGTH));

    // Different URL than the cached hostname
    MEMSET(&ipAddresses, 0x00, SIZEOF(ipAddresses));
    EXPECT_EQ(STATUS_PEERCONNECTION_EARLY_DNS_RESOLUTION_FAILED, onSetStunServerIp(0, (PCHAR) "stun.other.invalid", &ipAddresses));
    EXPECT_EQ(KVS_IP_FAMILY_TYPE_NOT_SET, ipAddresses.ipv4Address.family);

    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    *pStunIpAddrCtx = savedCtx;
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
    releaseHoldOnInstance(pWebRtcClientContext);

    if (createdHere) {
        EXPECT_EQ(STATUS_SUCCESS, cleanupWebRtcClientInstance());
    }
}

#if defined(KVS_USE_OPENSSL) && defined(ENABLE_KVS_THREADPOOL)
// C1: Verify dtlsSessionStartThread uses DtlsSessionStartArgs (not raw peer connection)
// and properly acquires/releases the DTLS session refcount.
TEST_F(PeerConnectionApiTest, dtlsSessionStartThread_HoldsRefOnDtlsSession)
{
    DtlsSessionCallbacks callbacks;
    PDtlsSession pDtlsSession = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;

    MEMSET(&callbacks, 0, SIZEOF(callbacks));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pDtlsSession));
    ASSERT_NE(pDtlsSession, nullptr);

    // Baseline: refcount should be 0
    EXPECT_EQ(0, ATOMIC_LOAD(&pDtlsSession->objRefCount));

    // Mark the session as shutting down so dtlsSessionHandshakeInThread bails
    // at its isCleanUp check instead of driving SSL_do_handshake (which would
    // call the NULL outboundPacketFn and segfault).
    ATOMIC_STORE_BOOL(&pDtlsSession->isCleanUp, TRUE);

    PDtlsSessionStartArgs pArgs = (PDtlsSessionStartArgs) MEMCALLOC(1, SIZEOF(DtlsSessionStartArgs));
    ASSERT_NE(pArgs, nullptr);
    pArgs->pDtlsSession = pDtlsSession;
    pArgs->isServer = FALSE;
    acquireDtlsSession(pDtlsSession);
    EXPECT_EQ(1, ATOMIC_LOAD(&pDtlsSession->objRefCount));

    // The handshake aborts immediately (isCleanUp), but the task must still
    // release the ref that was acquired before the push.
    dtlsSessionStartThread((PVOID) pArgs);
    // pArgs is freed inside dtlsSessionStartThread

    EXPECT_EQ(0, ATOMIC_LOAD(&pDtlsSession->objRefCount));

    // Reset so freeDtlsSession doesn't spin forever
    ATOMIC_STORE_BOOL(&pDtlsSession->isCleanUp, FALSE);
    freeDtlsSession(&pDtlsSession);
    timerQueueFree(&timerQueueHandle);
}

// C1: Verify NULL args are handled gracefully
TEST_F(PeerConnectionApiTest, dtlsSessionStartThread_NullArgs)
{
    // Should not crash
    EXPECT_EQ(NULL, dtlsSessionStartThread(NULL));
}
#endif

// C2: threadpoolContextPush must return STATUS_INVALID_OPERATION when the pool
// has been destroyed. The mutex is process-lifetime so push safely locks it,
// sees isInitialized == FALSE, and returns without UB.
#ifdef ENABLE_KVS_THREADPOOL
TEST_F(PeerConnectionApiTest, threadpoolContextPush_AfterDestroyReturnsInvalidOp)
{
    // The test fixture calls initKvsWebRtc() in SetUp which creates the pool.
    EXPECT_EQ(STATUS_SUCCESS, destroyThreadPoolContext());

    // Push after destroy locks the (still-valid) mutex, sees !isInitialized
    EXPECT_EQ(STATUS_INVALID_OPERATION, threadpoolContextPush(NULL, NULL));

    // Re-create so the fixture teardown (deinitKvsWebRtc) succeeds
    EXPECT_EQ(STATUS_SUCCESS, createThreadPoolContext());
}
#endif

// F1: When the startup STUN DNS lookup failed (isIpInitialized=FALSE),
// onSetStunServerIp must return EARLY_DNS_RESOLUTION_FAILED so parseIceServer
// falls back to synchronous DNS instead of silently proceeding with no address.
TEST_F(PeerConnectionApiTest, onSetStunServerIp_EmptyCacheFallsBack)
{
    PWebRtcClientContext pWebRtcClientContext;
    PStunIpAddrContext pStunIpAddrCtx;
    StunIpAddrContext savedCtx;
    DualKvsIpAddresses ipAddresses;
    BOOL createdHere = FALSE, resolverDone = FALSE;
    UINT32 i;

    pWebRtcClientContext = getWebRtcClientInstance();
    if (!ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized)) {
        releaseHoldOnInstance(pWebRtcClientContext);
        ASSERT_EQ(STATUS_SUCCESS, createWebRtcClientInstance());
        createdHere = TRUE;
        pWebRtcClientContext = getWebRtcClientInstance();
    }
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized));
    pStunIpAddrCtx = pWebRtcClientContext->pStunIpAddrCtx;
    ASSERT_TRUE(pStunIpAddrCtx != NULL);

    // Wait for any in-flight resolver
    for (i = 0; i < 1000 && !createdHere && !resolverDone; i++) {
        MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
        resolverDone = (pStunIpAddrCtx->startTime != 0);
        MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
        if (!resolverDone) {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

    // Save and seed a cache where hostname matches but isIpInitialized is FALSE
    // (simulates a failed startup DNS lookup)
    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    savedCtx = *pStunIpAddrCtx;
    STRCPY(pStunIpAddrCtx->hostname, "stun.test.invalid");
    MEMSET(&pStunIpAddrCtx->kvsIpAddresses, 0x00, SIZEOF(DualKvsIpAddresses));
    pStunIpAddrCtx->isIpInitialized = FALSE;
    pStunIpAddrCtx->startTime = GETTIME();
    pStunIpAddrCtx->expirationDuration = 2 * HUNDREDS_OF_NANOS_IN_AN_HOUR;
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);

    MEMSET(&ipAddresses, 0x00, SIZEOF(ipAddresses));
    // Must return EARLY_DNS_RESOLUTION_FAILED so parseIceServer falls back
    EXPECT_EQ(STATUS_PEERCONNECTION_EARLY_DNS_RESOLUTION_FAILED,
              onSetStunServerIp(0, (PCHAR) "stun.test.invalid", &ipAddresses));
    // No address should have been copied
    EXPECT_EQ(KVS_IP_FAMILY_TYPE_NOT_SET, ipAddresses.ipv4Address.family);

    // Restore
    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    *pStunIpAddrCtx = savedCtx;
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
    releaseHoldOnInstance(pWebRtcClientContext);

    if (createdHere) {
        EXPECT_EQ(STATUS_SUCCESS, cleanupWebRtcClientInstance());
    }
}

// F1: When the 2-hour cache refresh fails, onSetStunServerIp must return
// EARLY_DNS_RESOLUTION_FAILED so parseIceServer falls back to synchronous DNS.
TEST_F(PeerConnectionApiTest, onSetStunServerIp_ExpiredCacheRefreshFailureFallsBack)
{
    PWebRtcClientContext pWebRtcClientContext;
    PStunIpAddrContext pStunIpAddrCtx;
    StunIpAddrContext savedCtx;
    DualKvsIpAddresses ipAddresses;
    BOOL createdHere = FALSE, resolverDone = FALSE;
    UINT32 i;

    pWebRtcClientContext = getWebRtcClientInstance();
    if (!ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized)) {
        releaseHoldOnInstance(pWebRtcClientContext);
        ASSERT_EQ(STATUS_SUCCESS, createWebRtcClientInstance());
        createdHere = TRUE;
        pWebRtcClientContext = getWebRtcClientInstance();
    }
    ASSERT_TRUE(ATOMIC_LOAD_BOOL(&pWebRtcClientContext->isContextInitialized));
    pStunIpAddrCtx = pWebRtcClientContext->pStunIpAddrCtx;
    ASSERT_TRUE(pStunIpAddrCtx != NULL);

    for (i = 0; i < 1000 && !createdHere && !resolverDone; i++) {
        MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
        resolverDone = (pStunIpAddrCtx->startTime != 0);
        MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
        if (!resolverDone) {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

    // Seed a cache that is initialized but expired, with an unresolvable hostname
    // so getStunAddr will fail during refresh
    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    savedCtx = *pStunIpAddrCtx;
    STRCPY(pStunIpAddrCtx->hostname, "this-host-does-not-exist.invalid");
    pStunIpAddrCtx->isIpInitialized = TRUE;
    pStunIpAddrCtx->startTime = 1; // far in the past
    pStunIpAddrCtx->expirationDuration = 0; // already expired
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);

    MEMSET(&ipAddresses, 0x00, SIZEOF(ipAddresses));
    EXPECT_EQ(STATUS_PEERCONNECTION_EARLY_DNS_RESOLUTION_FAILED,
              onSetStunServerIp(0, (PCHAR) "this-host-does-not-exist.invalid", &ipAddresses));

    MUTEX_LOCK(pWebRtcClientContext->stunCtxlock);
    *pStunIpAddrCtx = savedCtx;
    MUTEX_UNLOCK(pWebRtcClientContext->stunCtxlock);
    releaseHoldOnInstance(pWebRtcClientContext);

    if (createdHere) {
        EXPECT_EQ(STATUS_SUCCESS, cleanupWebRtcClientInstance());
    }
}

// C3: Verify threadpoolContextPush succeeds even when the pool is at capacity
// (the saturation warning is logged but the task is still queued).
#ifdef ENABLE_KVS_THREADPOOL
static PVOID noopTask(PVOID args)
{
    UNUSED_PARAM(args);
    return NULL;
}

TEST_F(PeerConnectionApiTest, threadpoolContextPush_SaturationWarningDoesNotFail)
{
    // Push a task — it should succeed regardless of pool occupancy.
    // The saturation warning is a DLOGW, not a failure.
    EXPECT_EQ(STATUS_SUCCESS, threadpoolContextPush(noopTask, NULL));
}
#endif

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
