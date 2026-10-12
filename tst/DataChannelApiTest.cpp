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

// Each message carries its own channel's ordering and reliability, not the session's last DATA_CHANNEL_OPEN
TEST_F(DataChannelApiTest, sctpSessionSetSendInfo_UsesTheChannelSettings)
{
    struct sctp_sendv_spa spa;
    RtcDataChannelInit init;

    // no settings: reliable and ordered
    sctpSessionSetSendInfo(&spa, 7, TRUE, NULL);
    EXPECT_EQ(7, spa.sendv_sndinfo.snd_sid);
    EXPECT_EQ(SCTP_SEND_SNDINFO_VALID, spa.sendv_flags);
    EXPECT_EQ(0, spa.sendv_sndinfo.snd_flags);

    MEMSET(&init, 0x00, SIZEOF(init));
    init.ordered = TRUE;
    NULLABLE_SET_EMPTY(init.maxRetransmits);
    NULLABLE_SET_EMPTY(init.maxPacketLifeTime);
    sctpSessionSetSendInfo(&spa, 1, FALSE, &init);
    EXPECT_EQ(SCTP_SEND_SNDINFO_VALID, spa.sendv_flags);
    EXPECT_EQ(0, spa.sendv_sndinfo.snd_flags);

    init.ordered = FALSE;
    NULLABLE_SET_VALUE(init.maxRetransmits, 3);
    sctpSessionSetSendInfo(&spa, 1, FALSE, &init);
    EXPECT_EQ(SCTP_UNORDERED, spa.sendv_sndinfo.snd_flags);
    EXPECT_EQ(SCTP_SEND_SNDINFO_VALID | SCTP_SEND_PRINFO_VALID, spa.sendv_flags);
    EXPECT_EQ(SCTP_PR_SCTP_RTX, spa.sendv_prinfo.pr_policy);
    EXPECT_EQ(3, spa.sendv_prinfo.pr_value);

    // maxRetransmits wins over maxPacketLifeTime, as in the DATA_CHANNEL_OPEN
    NULLABLE_SET_VALUE(init.maxPacketLifeTime, 500);
    sctpSessionSetSendInfo(&spa, 1, FALSE, &init);
    EXPECT_EQ(SCTP_PR_SCTP_RTX, spa.sendv_prinfo.pr_policy);

    init.ordered = TRUE;
    NULLABLE_SET_EMPTY(init.maxRetransmits);
    sctpSessionSetSendInfo(&spa, 1, FALSE, &init);
    EXPECT_EQ(0, spa.sendv_sndinfo.snd_flags);
    EXPECT_EQ(SCTP_PR_SCTP_TTL, spa.sendv_prinfo.pr_policy);
    EXPECT_EQ(500, spa.sendv_prinfo.pr_value);
}

// A remote channel takes its settings from the channel type and reliability parameter of its DATA_CHANNEL_OPEN
TEST_F(DataChannelApiTest, dcepParseChannelParameters_ReadsChannelTypeAndParameter)
{
    // type, channel type, priority (2), reliability parameter (4), label length (2), protocol length (2)
    BYTE open[] = {0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x2C, 0x00, 0x00, 0x00, 0x00};
    RtcDataChannelInit init;

    dcepParseChannelParameters(open, &init);
    EXPECT_TRUE(init.ordered);
    EXPECT_TRUE(NULLABLE_CHECK_EMPTY(init.maxRetransmits));
    EXPECT_TRUE(NULLABLE_CHECK_EMPTY(init.maxPacketLifeTime));
    EXPECT_FALSE(init.negotiated);

    open[1] = 0x80; // DATA_CHANNEL_RELIABLE_UNORDERED
    dcepParseChannelParameters(open, &init);
    EXPECT_FALSE(init.ordered);
    EXPECT_TRUE(NULLABLE_CHECK_EMPTY(init.maxRetransmits));

    open[1] = 0x01; // DATA_CHANNEL_PARTIAL_RELIABLE_REXMIT
    dcepParseChannelParameters(open, &init);
    EXPECT_TRUE(init.ordered);
    EXPECT_FALSE(NULLABLE_CHECK_EMPTY(init.maxRetransmits));
    EXPECT_EQ(300, init.maxRetransmits.value);
    EXPECT_TRUE(NULLABLE_CHECK_EMPTY(init.maxPacketLifeTime));

    open[1] = 0x82; // DATA_CHANNEL_PARTIAL_RELIABLE_TIMED_UNORDERED
    dcepParseChannelParameters(open, &init);
    EXPECT_FALSE(init.ordered);
    EXPECT_TRUE(NULLABLE_CHECK_EMPTY(init.maxRetransmits));
    EXPECT_FALSE(NULLABLE_CHECK_EMPTY(init.maxPacketLifeTime));
    EXPECT_EQ(300, init.maxPacketLifeTime.value);

    // a parameter beyond 16 bits is clamped
    open[4] = 0x01;
    dcepParseChannelParameters(open, &init);
    EXPECT_EQ(MAX_UINT16, init.maxPacketLifeTime.value);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

#endif
