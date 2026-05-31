#include "WebRTCClientTestFixture.h"

#ifdef ENABLE_DATA_CHANNEL
namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DataChannelApiTest : public WebRtcClientTestBase {};

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

TEST_F(DataChannelApiTest, handleDcepPacket_RejectsOversizedLabelLength)
{
    /** Construct a minimal DCEP DataChannelOpen packet (13 bytes total):
     *   [0]     0x03  = DATA_CHANNEL_OPEN message type
     *   [1]     0x00  = channel type (reliable ordered)
     *   [2-3]   0x0000 = priority
     *   [4-7]   0x00000000 = reliability parameter
     *   [8-9]   0xFFFF = labelLength (65535)
     *   [10-11] 0x0000 = protocolLength
     *   [12]    0x41  = 1 byte of actual label data ("A")
     */
    BYTE dcepPacket[] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, // labelLength = 65535 (malformed)
                         0x00, 0x00, 0x41};

    // Create a fake SctpSession with a non-NULL socket to pass the initial NULL check.
    SctpSession sctpSession;
    MEMSET(&sctpSession, 0, SIZEOF(SctpSession));
    sctpSession.socket = (struct socket*) 0x1; // non-NULL dummy

    // handleDcepPacket will reject the packet at the bounds check before reaching usrsctp_sendv.
    EXPECT_EQ(STATUS_SCTP_INVALID_DCEP_PACKET, handleDcepPacket(&sctpSession, 0, dcepPacket, SIZEOF(dcepPacket)));
}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
