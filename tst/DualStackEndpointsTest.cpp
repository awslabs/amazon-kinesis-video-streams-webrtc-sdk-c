#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DualStackEndpointsTest : public WebRtcClientTestBase {};


TEST_F(DualStackEndpointsTest, connectTwoDualStackPeersWithForcedTurn)
{
    // (This fails because service is not yet ready.)


    // RtcConfiguration configuration;
    // PRtcPeerConnection offerPc = NULL, answerPc = NULL;

    // ASSERT_EQ(TRUE, mAccessKeyIdSet);

    // #ifdef _WIN32
    // _putenv_s(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON");
    // #else
    // setenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON", 1);
    // #endif

    // MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    // configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_RELAY;

    // initializeSignalingClient();
    // getIceServers(&configuration);

    // EXPECT_EQ(createPeerConnection(&configuration, &offerPc), STATUS_SUCCESS);
    // EXPECT_EQ(createPeerConnection(&configuration, &answerPc), STATUS_SUCCESS);

    // EXPECT_EQ(connectTwoPeers(offerPc, answerPc), TRUE);

    // closePeerConnection(offerPc);
    // closePeerConnection(answerPc);

    // freePeerConnection(&offerPc);
    // freePeerConnection(&answerPc);

    // deinitializeSignalingClient();

    // unsetenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR);
}



} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com