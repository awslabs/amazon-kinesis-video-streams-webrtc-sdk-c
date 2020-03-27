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
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(invalidLengthValue, SIZEOF(invalidLengthValue), &rtcpPacket));

    BYTE validRtcpPacket[] = {0x81, 0xcd, 0x00, 0x03, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0xa4, 0x00, 0x00};
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(validRtcpPacket, SIZEOF(validRtcpPacket), &rtcpPacket));

    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK);
    EXPECT_EQ(rtcpPacket.header.receptionReportCount, RTCP_FEEDBACK_MESSAGE_TYPE_NACK);
}




TEST_F(RtcpFunctionalityTest, setRtpPacketFromBytesCompound)
{
    RtcpPacket rtcpPacket;

    MEMSET(&rtcpPacket, 0x00, SIZEOF(RtcpPacket));

    // Compound RTCP Packet that contains SR, SDES and REMB
    BYTE compoundPacket[] = {
        0x80, 0xc8, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0xe1, 0xe3, 0x20, 0x43, 0xe5, 0x3d, 0x10, 0x2b,
        0xbf, 0x58, 0xf7, 0xef, 0x00, 0x00, 0x23, 0xf3, 0x00, 0x6c, 0xd3, 0x75, 0x81, 0xca, 0x00, 0x06,
        0xf1, 0x2d, 0x7b, 0x4b, 0x01, 0x10, 0x2f, 0x76, 0x6d, 0x4b, 0x51, 0x6e, 0x47, 0x6e, 0x55, 0x70,
        0x4f, 0x2b, 0x70, 0x38, 0x64, 0x52, 0x00, 0x00, 0x8f, 0xce, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b,
        0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42, 0x02, 0x12, 0x2d, 0x97, 0x0c, 0xef, 0x37, 0x0d,
        0x2d, 0x07, 0x3d, 0x1d
    };

    auto currentOffset = 0;
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(compoundPacket + currentOffset, SIZEOF(compoundPacket) - currentOffset, &rtcpPacket));
    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_SENDER_REPORT);

    currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(compoundPacket + currentOffset, SIZEOF(compoundPacket) - currentOffset, &rtcpPacket));
    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_SOURCE_DESCRIPTION);

    currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(compoundPacket + currentOffset, SIZEOF(compoundPacket) - currentOffset, &rtcpPacket));
    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK);

    currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    ASSERT_EQ(currentOffset, SIZEOF(compoundPacket));
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

TEST_F(RtcpFunctionalityTest, onRtcpPacketCompound) {
    KvsPeerConnection peerConnection;

    BYTE compound[] =  {
        0x80, 0xc8, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0xe1, 0xe3, 0x20, 0x43, 0xe5, 0x3d, 0x10, 0x2b,
        0xbf, 0x58, 0xf7, 0xef, 0x00, 0x00, 0x23, 0xf3, 0x00, 0x6c, 0xd3, 0x75, 0x81, 0xca, 0x00, 0x06,
        0xf1, 0x2d, 0x7b, 0x4b, 0x01, 0x10, 0x2f, 0x76, 0x6d, 0x4b, 0x51, 0x6e, 0x47, 0x6e, 0x55, 0x70,
        0x4f, 0x2b, 0x70, 0x38, 0x64, 0x52, 0x00, 0x00,
    };
    EXPECT_EQ(STATUS_SUCCESS, onRtcpPacket(&peerConnection, compound, SIZEOF(compound)));
}

TEST_F(RtcpFunctionalityTest, rembValueGet) {
    BYTE rawRtcpPacket[] = {
        0x8f, 0xce, 0x00, 0x05, 0x61, 0x7a, 0x37, 0x43, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42,
        0x01, 0x12, 0x46, 0x73, 0x6c, 0x76, 0xe8, 0x55
    };
    RtcpPacket rtcpPacket;

    MEMSET(&rtcpPacket, 0x00, SIZEOF(RtcpPacket));

    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(rawRtcpPacket, SIZEOF(rawRtcpPacket), &rtcpPacket));
    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK);
    EXPECT_EQ(rtcpPacket.header.receptionReportCount, RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK);

    BYTE bufferTooSmall[] = {0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_TOO_SMALL, isRembPacket(bufferTooSmall, SIZEOF(bufferTooSmall)));

    BYTE bufferNoUniqueIdentifier[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_INVALID, isRembPacket(bufferNoUniqueIdentifier, SIZEOF(bufferNoUniqueIdentifier)));

    UINT8 ssrcListLen = 0;
    DOUBLE maximumBitRate = 0;
    UINT32 ssrcList[5];

    BYTE singleSSRC[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42, 0x01, 0x12, 0x76, 0x28,
        0x6c, 0x76, 0xe8, 0x55
    };
    EXPECT_EQ(STATUS_SUCCESS, rembValueGet(singleSSRC, SIZEOF(singleSSRC), &maximumBitRate, ssrcList, &ssrcListLen));
    EXPECT_EQ(ssrcListLen, 1);
    EXPECT_EQ(maximumBitRate, 2581120.0);
    EXPECT_EQ(ssrcList[0], 0x6c76e855);

    BYTE invalidSSRCLength[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42, 0xFF, 0x12, 0x76, 0x28,
        0x6c, 0x76, 0xe8, 0x55
    };
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_INVALID, rembValueGet(invalidSSRCLength, SIZEOF(invalidSSRCLength), &maximumBitRate, ssrcList, &ssrcListLen));
}

static void on_PLI_callback(UINT64 opaque) {
    *(PBOOL) opaque = TRUE;
}

TEST_F(RtcpFunctionalityTest, onpli) {
    initializeEndianness();
    BYTE rawRtcpPacket[] = {0x81, 0xCE, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x1D, 0xC8, 0x69, 0x91};
    RtcpPacket rtcpPacket = {0};
    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(rawRtcpPacket, SIZEOF(rawRtcpPacket), &rtcpPacket));
    ASSERT_TRUE(rtcpPacket.header.packetType == RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK &&
                rtcpPacket.header.receptionReportCount == RTCP_PSFB_PLI);
    KvsPeerConnection kpc = {0};
    doubleListCreate(&kpc.pTransceievers);
    KvsRtpTransceiver kvsRtpTransceiver;
    MEMSET(&kvsRtpTransceiver, 0, sizeof(KvsRtpTransceiver));
    kvsRtpTransceiver.sender.ssrc =  0x1DC86991;
    BOOL on_picture_loss_called = FALSE;
    kvsRtpTransceiver.onPictureLossCustomData = (UINT64) &on_picture_loss_called;
    kvsRtpTransceiver.onPictureLoss = on_PLI_callback;
    doubleListInsertItemHead(kpc.pTransceievers, (UINT64) &kvsRtpTransceiver);
    onRtcpPLIPacket(&rtcpPacket, &kpc);
    ASSERT_TRUE(on_picture_loss_called);
}

}
}
}
}
}
