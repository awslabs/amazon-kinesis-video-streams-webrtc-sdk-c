#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class MetricsFunctionalityTest : public WebRtcClientTestBase {
};

TEST_F(MetricsFunctionalityTest, connectTwoPeersGetIceCandidatePairStats)
{
    ASSERT_EQ(TRUE, mAccessKeyIdSet);
    RtcStats rtcMetrics;
    RtcConfiguration configuration;
    PIceAgent pIceAgent;
    PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    initializeSignalingClient();

    EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    getIceServers(&configuration, offerPc);
    getIceServers(&configuration, answerPc);

    EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    rtcMetrics.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;
    EXPECT_EQ(STATUS_SUCCESS, rtcPeerConnectionGetMetrics(offerPc, NULL, &rtcMetrics));

    pIceAgent = ((PKvsPeerConnection) offerPc)->pIceAgent;
    EXPECT_STREQ(pIceAgent->pDataSendingIceCandidatePair->local->id, rtcMetrics.rtcStatsObject.iceCandidatePairStats.localCandidateId);
    EXPECT_STREQ(pIceAgent->pDataSendingIceCandidatePair->remote->id, rtcMetrics.rtcStatsObject.iceCandidatePairStats.remoteCandidateId);
    EXPECT_EQ(pIceAgent->pDataSendingIceCandidatePair->state, rtcMetrics.rtcStatsObject.iceCandidatePairStats.state);
    EXPECT_EQ(pIceAgent->pDataSendingIceCandidatePair->nominated, rtcMetrics.rtcStatsObject.iceCandidatePairStats.nominated);
    EXPECT_EQ(NULL, rtcMetrics.rtcStatsObject.iceCandidatePairStats.circuitBreakerTriggerCount.value);

    closePeerConnection(offerPc);
    closePeerConnection(answerPc);

    freePeerConnection(&offerPc);
    freePeerConnection(&answerPc);

    deinitializeSignalingClient();
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
