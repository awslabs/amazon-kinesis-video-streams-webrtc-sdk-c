#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SdpFunctionalityTest : public WebRtcClientTestBase {};

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

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_invalidLocalDirectionDefaultsToSendRecv)
{
    RTC_RTP_TRANSCEIVER_DIRECTION transceiver;
    RTC_RTP_TRANSCEIVER_DIRECTION offer = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;

    // Invalid be treated as sendrecv
    MEMSET(&transceiver, 0x00, SIZEOF(transceiver));

    // SendRecv + SendRecv = SendRecv
    EXPECT_EQ(intersectTransceiverDirection(transceiver, offer), RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV);
}

TEST_F(SdpFunctionalityTest, intersectTransceiverDirection_invalidRemoteDirectionDefaultsToSendRecv)
{
    RTC_RTP_TRANSCEIVER_DIRECTION transceiver = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    RTC_RTP_TRANSCEIVER_DIRECTION offer;

    // Invalid be treated as sendrecv
    MEMSET(&offer, 0x00, SIZEOF(offer));

    // SendRecv + SendRecv = SendRecv
    EXPECT_EQ(intersectTransceiverDirection(transceiver, offer), RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV);
}

class IntersectTransceiverDirectionTest : public SdpFunctionalityTest,
                                          public ::testing::WithParamInterface<std::tuple<RTC_RTP_TRANSCEIVER_DIRECTION, RTC_RTP_TRANSCEIVER_DIRECTION, RTC_RTP_TRANSCEIVER_DIRECTION>> {};

TEST_P(IntersectTransceiverDirectionTest, AllScenarios)
{
    RTC_RTP_TRANSCEIVER_DIRECTION transceiver, offer, expected;
    std::tie(transceiver, offer, expected) = GetParam();
    EXPECT_EQ(intersectTransceiverDirection(transceiver, offer), expected);
}

// tuple: [transceiver (local), offer (remote), generated answer]
INSTANTIATE_TEST_SUITE_P(SdpFunctionalityTest, IntersectTransceiverDirectionTest,
    ::testing::Values(
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),

        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY),

        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),

        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),

        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),
        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE),

        std::make_tuple(RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE, RTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE)
    ));

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
