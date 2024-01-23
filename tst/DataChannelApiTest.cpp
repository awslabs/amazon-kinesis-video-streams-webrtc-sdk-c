#include "WebRTCClientTestFixture.h"

#ifdef ENABLE_DATA_CHANNEL
namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DataChannelApiTest : public WebRtcClientTestBase {
};

TEST_F(DataChannelApiTest, createDataChannel_Disconnected)
{
    RtcConfiguration configuration;
    PRtcPeerConnection pPeerConnection = nullptr;
    PRtcDataChannel pDataChannel = nullptr;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    EXPECT_EQ(createPeerConnection(&configuration, &pPeerConnection), STATUS_SUCCESS);

    // Create two DataChannels
    EXPECT_EQ(createDataChannel(pPeerConnection, (PCHAR) "DataChannel 1", nullptr, &pDataChannel), STATUS_SUCCESS);
    EXPECT_EQ(createDataChannel(pPeerConnection, (PCHAR) "DataChannel 2", nullptr, &pDataChannel), STATUS_SUCCESS);

    // Don't allow NULL
    EXPECT_EQ(createDataChannel(nullptr, (PCHAR) "DataChannel 2", nullptr, &pDataChannel), STATUS_NULL_ARG);
    EXPECT_EQ(createDataChannel(pPeerConnection, nullptr, nullptr, &pDataChannel), STATUS_NULL_ARG);
    EXPECT_EQ(createDataChannel(pPeerConnection, (PCHAR) "DataChannel 2", nullptr, nullptr), STATUS_NULL_ARG);

    closePeerConnection(pPeerConnection);
    freePeerConnection(&pPeerConnection);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
