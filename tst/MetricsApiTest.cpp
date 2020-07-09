#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class MetricsApiTest : public WebRtcClientTestBase {
};

TEST_F(MetricsApiTest, webRtcGetMetrics)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection;
    RtcStats rtcMetrics;

    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(NULL, &rtcMetrics));

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    EXPECT_EQ(STATUS_NULL_ARG, rtcPeerConnectionGetMetrics(pRtcPeerConnection, NULL));

    rtcMetrics.requestedTypeOfStats = (RTC_STATS_TYPE) 20; // Supplying a type that is unavailable
    EXPECT_EQ(STATUS_NOT_IMPLEMENTED, rtcPeerConnectionGetMetrics(pRtcPeerConnection, &rtcMetrics));

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(MetricsApiTest, webRtcIceGetMetrics)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pRtcPeerConnection;
    RtcStats rtcIceMetrics;
    rtcIceMetrics.requestedTypeOfStats = RTC_STATS_TYPE_ICE_SERVER; // Supplying a type that is unavailable
    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 5;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    STRNCPY(configuration.iceServers[0].urls, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443", MAX_ICE_CONFIG_URI_LEN);
    STRNCPY(configuration.iceServers[0].credential, (PCHAR) "", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    STRNCPY(configuration.iceServers[0].username,  (PCHAR) "", MAX_ICE_CONFIG_USER_NAME_LEN);

    STRNCPY(configuration.iceServers[1].urls, (PCHAR) "turns:54.202.170.151:443?transport=tcp", MAX_ICE_CONFIG_URI_LEN);
    STRNCPY(configuration.iceServers[1].credential, (PCHAR) "username", MAX_ICE_CONFIG_CREDENTIAL_LEN);
    STRNCPY(configuration.iceServers[1].username,  (PCHAR) "password", MAX_ICE_CONFIG_USER_NAME_LEN);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&configuration, &pRtcPeerConnection));

    EXPECT_EQ(STATUS_ICE_SERVER_INDEX_INVALID, rtcPeerConnectionGetMetrics(pRtcPeerConnection, &rtcIceMetrics));

    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 0;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, &rtcIceMetrics));

    EXPECT_EQ(443, rtcIceMetrics.rtcStatsObject.iceServerStats.port);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, configuration.iceServers[0].urls, rtcIceMetrics.rtcStatsObject.iceServerStats.url);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "", rtcIceMetrics.rtcStatsObject.iceServerStats.protocol);

    rtcIceMetrics.rtcStatsObject.iceServerStats.iceServerIndex = 1;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(pRtcPeerConnection, &rtcIceMetrics));
    EXPECT_EQ(443, rtcIceMetrics.rtcStatsObject.iceServerStats.port);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, configuration.iceServers[1].urls, rtcIceMetrics.rtcStatsObject.iceServerStats.url);
    EXPECT_PRED_FORMAT2(testing::IsSubstring, "transport=tcp", rtcIceMetrics.rtcStatsObject.iceServerStats.protocol);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

}
}
}
}
}
