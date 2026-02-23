#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SdpFunctionalityTest : public WebRtcClientTestBase {};

// --------------- parseTransceiverDirection -----------------

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_NullString)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_NULL_ARG, parseTransceiverDirection(NULL, &direction));

    // It should set the direction to zero on failure
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_UNINITIALIZED);
}

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_Invalid)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_SUCCESS, parseTransceiverDirection((PCHAR) "rtcp-mux", &direction));
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_UNINITIALIZED);
}

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_SendOnly)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_SUCCESS, parseTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY_STR, &direction));
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY);
}

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_SendRecv)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_SUCCESS, parseTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV_STR, &direction));
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV);
}

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_RecvOnly)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_SUCCESS, parseTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY_STR, &direction));
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY);
}

TEST_F(SdpFunctionalityTest, parseTransceiverDirection_Inactive)
{
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    EXPECT_EQ(STATUS_SUCCESS, parseTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE_STR, &direction));
    EXPECT_EQ(direction, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
}

// --------------- writeTransceiverDirection -----------------

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_NullBuffer)
{
    EXPECT_EQ(STATUS_NULL_ARG, writeTransceiverDirection(NULL, 10, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV));
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_ZeroLength)
{
    CHAR buf[10];
    EXPECT_EQ(STATUS_INVALID_ARG_LEN, writeTransceiverDirection(buf, 0, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV));
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_Sendrecv)
{
    CHAR buf[20];
    EXPECT_EQ(STATUS_SUCCESS, writeTransceiverDirection(buf, SIZEOF(buf), RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV));
    EXPECT_STREQ(buf, "sendrecv");
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_Sendonly)
{
    CHAR buf[20];
    EXPECT_EQ(STATUS_SUCCESS, writeTransceiverDirection(buf, SIZEOF(buf), RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY));
    EXPECT_STREQ(buf, "sendonly");
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_Recvonly)
{
    CHAR buf[20];
    EXPECT_EQ(STATUS_SUCCESS, writeTransceiverDirection(buf, SIZEOF(buf), RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY));
    EXPECT_STREQ(buf, "recvonly");
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_Inactive)
{
    CHAR buf[20];
    EXPECT_EQ(STATUS_SUCCESS, writeTransceiverDirection(buf, SIZEOF(buf), RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE));
    EXPECT_STREQ(buf, "inactive");
}

TEST_F(SdpFunctionalityTest, writeTransceiverDirection_bufferTooSmall)
{
    CHAR buf[SIZEOF(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV_STR)]; // too small by 1 since it needs the null-terminator
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, writeTransceiverDirection(buf, SIZEOF(buf), RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV));

    // It should also set the output to empty string
    EXPECT_STREQ(buf, "");
}

// --------------- intersectTransceiverDirection -----------------

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_BothInactive)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
              RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_EitherInactive)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV),
              RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
              RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_BothSendRecv)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV),
              RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_SendRecvWithSendOnly)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY);
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV),
              RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_SendRecvWithRecvOnly)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY);
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV),
              RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_BothSendOnly)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_BothRecvOnly)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_SendOnlyWithRecvOnly)
{
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY);
    EXPECT_EQ(intersectTransceiverDirection(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
              RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
