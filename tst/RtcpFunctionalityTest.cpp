#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class RtcpFunctionalityTest : public WebRtcClientTestBase {
};

TEST_F(RtcpFunctionalityTest, setRtpPacketFromBytes)
{
    RtcpPacket rtcpPacket;

    MEMSET(&rtcpPacket, 0x00, SIZEOF(RtcpPacket));

    // Assert that we don't parse buffers that aren't even large enough
    BYTE headerTooSmall[] = {0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_TOO_SMALL, setRtcpPacketFromBytes(headerTooSmall, SIZEOF(headerTooSmall), &rtcpPacket));

    // Assert that we check version field
    BYTE invalidVersionValue[] = {0x01, 0xcd, 0x00, 0x03, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0xa4, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_INVALID_VERSION, setRtcpPacketFromBytes(invalidVersionValue, SIZEOF(invalidVersionValue), &rtcpPacket));

    // Assert that we check the length field
    BYTE invalidLengthValue[] = {0x81, 0xcd, 0x00, 0x00, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0xa4, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH, setRtcpPacketFromBytes(invalidLengthValue, SIZEOF(invalidLengthValue), &rtcpPacket));

    BYTE validRtcpPacket[] = {0x81, 0xcd, 0x00, 0x03, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0xa4, 0x00, 0x00};
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(validRtcpPacket, SIZEOF(validRtcpPacket), &rtcpPacket));

    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK);
    EXPECT_EQ(rtcpPacket.header.receptionReportCount, RTCP_FEEDBACK_MESSAGE_TYPE_NACK);
}

TEST_F(RtcpFunctionalityTest, rtcpNackListGet)
{
    UINT32 senderSsrc = 0, receiverSsrc = 0, ssrcListLen = 0;

    // Assert that NACK list meets the minimum length requirement
    BYTE nackListTooSmall[] = {0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_NACK_LIST_INVALID, rtcpNackListGet(nackListTooSmall, SIZEOF(nackListTooSmall), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    BYTE nackListMalformed[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_NACK_LIST_INVALID, rtcpNackListGet(nackListMalformed, SIZEOF(nackListMalformed), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    BYTE nackListSsrcOnly[] = {0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(nackListSsrcOnly, SIZEOF(nackListSsrcOnly), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    EXPECT_EQ(senderSsrc, 0x2cd1a0de);
    EXPECT_EQ(receiverSsrc, 0x0000abe0);

    BYTE singlePID[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x00};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singlePID, SIZEOF(singlePID), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    std::unique_ptr<UINT16[]> singlePIDBuffer(new UINT16[ssrcListLen]);
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singlePID, SIZEOF(singlePID), &senderSsrc, &receiverSsrc, singlePIDBuffer.get(), &ssrcListLen));

    EXPECT_EQ(ssrcListLen, 1);
    EXPECT_EQ(singlePIDBuffer[0], 3240);

}

TEST_F(RtcpFunctionalityTest, rtcpNackListBLP) {
    UINT32 senderSsrc = 0, receiverSsrc = 0, ssrcListLen = 0;

    BYTE singleBLP[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x04};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singleBLP, SIZEOF(singleBLP), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    std::unique_ptr<UINT16[]> singleBLPBuffer(new UINT16[ssrcListLen]);
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singleBLP, SIZEOF(singleBLP), &senderSsrc, &receiverSsrc, singleBLPBuffer.get(), &ssrcListLen));

    EXPECT_EQ(ssrcListLen, 2);
    EXPECT_EQ(singleBLPBuffer[0], 3240);
    EXPECT_EQ(singleBLPBuffer[1], 3243);
}

TEST_F(RtcpFunctionalityTest, rtcpNackListCompound) {
    UINT32 senderSsrc = 0, receiverSsrc = 0, ssrcListLen = 0;

    BYTE compound[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x00, 0x0c, 0xff, 0x00, 0x00};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(compound, SIZEOF(compound), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    std::unique_ptr<UINT16[]> compoundBuffer(new UINT16[ssrcListLen]);
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(compound, SIZEOF(compound), &senderSsrc, &receiverSsrc, compoundBuffer.get(), &ssrcListLen));

    EXPECT_EQ(ssrcListLen, 2);
    EXPECT_EQ(compoundBuffer[0], 3240);
    EXPECT_EQ(compoundBuffer[1], 3327);
}

}
}
}
}
}
