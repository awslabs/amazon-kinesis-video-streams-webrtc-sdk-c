#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class MetricsApiTest : public WebRtcClientTestBase {
};

TEST_F(MetricsApiTest, webRtcGetMetrics)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection;
    RtcStats rtcMetrics;
    RtcMediaStreamTrack videoTrack;
    PRtcRtpTransceiver videoTransceiver;

    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(NULL, NULL, &rtcMetrics));

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, NULL));

    rtcMetrics.requestedTypeOfStats = (RTC_STATS_TYPE) 20; // Supplying a type that is unavailable
    EXPECT_EQ(STATUS_NOT_IMPLEMENTED, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcMetrics));

    addTrackToPeerConnection(pRtcPeerConnection, &videoTrack, &videoTransceiver, RTC_CODEC_VP8, MEDIA_STREAM_TRACK_KIND_VIDEO);
    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_INBOUND_RTP;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcMetrics));

    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcMetrics));

    EXPECT_EQ(STATUS_SUCCESS, closePeerConnection(pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(MetricsApiTest, webRtcIceServerGetMetrics)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection;
    RtcStats rtcIceMetrics;
    rtcIceMetrics.requestedTypeOfStats = RTC_STATS_TYPE_ICE_SERVER; // Supplying a type that is unavailable
    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 5;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    STRNCPY(configuration.iceServers[0].urls, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443", MAX_ICE_CONFIG_URI_LEN);
    STRNCPY(configuration.iceServers[0].credential, (PCHAR) "", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    STRNCPY(configuration.iceServers[0].username, (PCHAR) "", MAX_ICE_CONFIG_USER_NAME_LEN);

    STRNCPY(configuration.iceServers[1].urls, (PCHAR) "turns:54.202.170.151:443?transport=tcp", MAX_ICE_CONFIG_URI_LEN);
    STRNCPY(configuration.iceServers[1].credential, (PCHAR) "username", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    STRNCPY(configuration.iceServers[1].username, (PCHAR) "password", MAX_ICE_CONFIG_USER_NAME_LEN);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    EXPECT_EQ(STATUS_ICE_SERVER_INDEX_INVALID, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcIceMetrics));

    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 0;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcIceMetrics));

    EXPECT_EQ(443, rtcIceMetrics.rtcStatsObject.iceServerStats.port);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, configuration.iceServers[0].urls, rtcIceMetrics.rtcStatsObject.iceServerStats.url);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "", rtcIceMetrics.rtcStatsObject.iceServerStats.protocol);

    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 1;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcIceMetrics));
    EXPECT_EQ(443, rtcIceMetrics.rtcStatsObject.iceServerStats.port);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, configuration.iceServers[1].urls, rtcIceMetrics.rtcStatsObject.iceServerStats.url);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "transport=tcp", rtcIceMetrics.rtcStatsObject.iceServerStats.protocol);

    EXPECT_EQ(STATUS_SUCCESS, closePeerConnection(pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(MetricsApiTest, webRtcIceCandidateGetMetrics)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PIceAgent pIceAgent;
    RtcStats rtcIceMetrics;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    STRNCPY(configuration.iceServers[0].urls, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443", MAX_ICE_CONFIG_URI_LEN);
    STRNCPY(configuration.iceServers[0].credential, (PCHAR) "", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    STRNCPY(configuration.iceServers[0].username, (PCHAR) "", MAX_ICE_CONFIG_USER_NAME_LEN);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    pIceAgent = ((PKvsPeerConnection) pRtcPeerConnection)->pIceAgent;

    IceCandidate localCandidate;
    IceCandidate remoteCandidate;
    IceCandidatePair iceCandidatePair;

    MEMSET(&localCandidate, 0x00, SIZEOF(IceCandidate));
    localCandidate.state = ICE_CANDIDATE_STATE_VALID;
    localCandidate.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
    localCandidate.iceCandidateType = ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
    localCandidate.isRemote = FALSE;
    localCandidate.ipAddress.address[0] = 0x7f;
    localCandidate.ipAddress.address[1] = 0x00;
    localCandidate.ipAddress.address[2] = 0x00;
    localCandidate.ipAddress.address[3] = 0x01;
    localCandidate.ipAddress.port = getInt16(1234);
    localCandidate.priority = 1;

    MEMSET(&remoteCandidate, 0x00, SIZEOF(IceCandidate));
    remoteCandidate.state = ICE_CANDIDATE_STATE_VALID;
    remoteCandidate.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
    remoteCandidate.iceCandidateType = ICE_CANDIDATE_TYPE_HOST;
    remoteCandidate.isRemote = FALSE;
    remoteCandidate.ipAddress.address[0] = 0x0a;
    remoteCandidate.ipAddress.address[1] = 0x01;
    remoteCandidate.ipAddress.address[2] = 0xff;
    remoteCandidate.ipAddress.address[3] = 0xff;
    remoteCandidate.ipAddress.port = getInt16(1111);
    remoteCandidate.priority = 3;

    iceCandidatePair.local = &localCandidate;
    iceCandidatePair.remote = &remoteCandidate;
    pIceAgent->pDataSendingIceCandidatePair = &iceCandidatePair;

    EXPECT_EQ(STATUS_SUCCESS, updateSelectedLocalRemoteCandidateStats(pIceAgent));

    rtcIceMetrics.requestedTypeOfStats = RTC_STATS_TYPE_LOCAL_CANDIDATE;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcIceMetrics));

    EXPECT_EQ(1234, rtcIceMetrics.rtcStatsObject.localIceCandidateStats.port);
    EXPECT_EQ(1, rtcIceMetrics.rtcStatsObject.localIceCandidateStats.priority);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "127.0.0.1", rtcIceMetrics.rtcStatsObject.localIceCandidateStats.address);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "srflx", rtcIceMetrics.rtcStatsObject.localIceCandidateStats.candidateType);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "", rtcIceMetrics.rtcStatsObject.localIceCandidateStats.protocol);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "stun.kinesisvideo.us-west-2.amazonaws.com", rtcIceMetrics.rtcStatsObject.localIceCandidateStats.url);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "", rtcIceMetrics.rtcStatsObject.localIceCandidateStats.relayProtocol);

    rtcIceMetrics.requestedTypeOfStats = RTC_STATS_TYPE_REMOTE_CANDIDATE;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL, &rtcIceMetrics));
    EXPECT_EQ(1111, rtcIceMetrics.rtcStatsObject.remoteIceCandidateStats.port);
    EXPECT_EQ(3, rtcIceMetrics.rtcStatsObject.remoteIceCandidateStats.priority);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "10.1.255.255", rtcIceMetrics.rtcStatsObject.remoteIceCandidateStats.address);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "host", rtcIceMetrics.rtcStatsObject.remoteIceCandidateStats.candidateType);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "", rtcIceMetrics.rtcStatsObject.remoteIceCandidateStats.protocol);

    EXPECT_EQ(STATUS_SUCCESS, closePeerConnection(pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
