#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class RtcpFunctionalityTest : public WebRtcClientTestBase {
  public:
    PKvsRtpTransceiver pKvsRtpTransceiver = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PRtcRtpTransceiver pRtcRtpTransceiver = nullptr;

    STATUS initTransceiver(UINT32 ssrc)
    {
        RtcConfiguration config{};
        EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
        pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
        pRtcRtpTransceiver = addTransceiver(ssrc);
        pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
        return STATUS_SUCCESS;
    }

    PRtcRtpTransceiver addTransceiver(UINT32 ssrc)
    {
        RtcMediaStreamTrack track{};
        track.codec = RTC_CODEC_VP8;
        PRtcRtpTransceiver out = nullptr;
        EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &track, nullptr, &out));
        ((PKvsRtpTransceiver) out)->sender.ssrc = ssrc;
        return out;
    }
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
    BYTE compoundPacket[] = {0x80, 0xc8, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0xe1, 0xe3, 0x20, 0x43, 0xe5, 0x3d, 0x10, 0x2b, 0xbf,
                             0x58, 0xf7, 0xef, 0x00, 0x00, 0x23, 0xf3, 0x00, 0x6c, 0xd3, 0x75, 0x81, 0xca, 0x00, 0x06, 0xf1, 0x2d,
                             0x7b, 0x4b, 0x01, 0x10, 0x2f, 0x76, 0x6d, 0x4b, 0x51, 0x6e, 0x47, 0x6e, 0x55, 0x70, 0x4f, 0x2b, 0x70,
                             0x38, 0x64, 0x52, 0x00, 0x00, 0x8f, 0xce, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0x00, 0x00, 0x00, 0x00,
                             0x52, 0x45, 0x4d, 0x42, 0x02, 0x12, 0x2d, 0x97, 0x0c, 0xef, 0x37, 0x0d, 0x2d, 0x07, 0x3d, 0x1d};

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
    EXPECT_EQ(STATUS_RTCP_INPUT_NACK_LIST_INVALID,
              rtcpNackListGet(nackListTooSmall, SIZEOF(nackListTooSmall), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    BYTE nackListMalformed[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_NACK_LIST_INVALID,
              rtcpNackListGet(nackListMalformed, SIZEOF(nackListMalformed), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

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

TEST_F(RtcpFunctionalityTest, rtcpNackListBLP)
{
    UINT32 senderSsrc = 0, receiverSsrc = 0, ssrcListLen = 0;

    BYTE singleBLP[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x04};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singleBLP, SIZEOF(singleBLP), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    std::unique_ptr<UINT16[]> singleBLPBuffer(new UINT16[ssrcListLen]);
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singleBLP, SIZEOF(singleBLP), &senderSsrc, &receiverSsrc, singleBLPBuffer.get(), &ssrcListLen));

    EXPECT_EQ(ssrcListLen, 2);
    EXPECT_EQ(singleBLPBuffer[0], 3240);
    EXPECT_EQ(singleBLPBuffer[1], 3243);
}

TEST_F(RtcpFunctionalityTest, rtcpNackListCompound)
{
    UINT32 senderSsrc = 0, receiverSsrc = 0, ssrcListLen = 0;

    BYTE compound[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x00, 0x0c, 0xff, 0x00, 0x00};
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(compound, SIZEOF(compound), &senderSsrc, &receiverSsrc, NULL, &ssrcListLen));

    std::unique_ptr<UINT16[]> compoundBuffer(new UINT16[ssrcListLen]);
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(compound, SIZEOF(compound), &senderSsrc, &receiverSsrc, compoundBuffer.get(), &ssrcListLen));

    EXPECT_EQ(ssrcListLen, 2);
    EXPECT_EQ(compoundBuffer[0], 3240);
    EXPECT_EQ(compoundBuffer[1], 3327);
}

TEST_F(RtcpFunctionalityTest, rtcpNackListGetSmallBuffer)
{
    UINT32 senderSsrc = 0, receiverSsrc = 0;

    // 8-byte SSRC header + one FCI: PID 0x0ca8, BLP 0x0004 (bit 2 set -> one extra seq num).
    BYTE singleBLP[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0xa8, 0x00, 0x04};

    // Buffer sized for exactly one element; the packet yields two.
    const UINT32 capacity = 1;
    UINT32 ssrcListLen = capacity;
    PUINT16 pSeqList = (PUINT16) MEMALLOC(SIZEOF(UINT16) * capacity);
    ASSERT_TRUE(pSeqList != NULL);

    // Only pSeqList[0] is written; the second sequence number is counted but not stored.
    EXPECT_EQ(STATUS_SUCCESS, rtcpNackListGet(singleBLP, SIZEOF(singleBLP), &senderSsrc, &receiverSsrc, pSeqList, &ssrcListLen));

    // The reported count still reflects the true number of sequence numbers in the packet.
    EXPECT_EQ(2, ssrcListLen);
    EXPECT_EQ(3240, pSeqList[0]);

    SAFE_MEMFREE(pSeqList);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketCompoundNack)
{
    PRtpPacket pRtpPacket = nullptr;
    BYTE validRtcpPacket[] = {0x81, 0xcd, 0x00, 0x03, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0x00, 0x00, 0x00};
    initTransceiver(44000);
    ASSERT_EQ(STATUS_SUCCESS,
              createRtpRollingBuffer(DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS * DEFAULT_EXPECTED_VIDEO_BIT_RATE / 8 / DEFAULT_MTU_SIZE_BYTES,
                                     &pKvsRtpTransceiver->sender.packetBuffer));
    ASSERT_EQ(STATUS_SUCCESS,
              createRetransmitter(DEFAULT_SEQ_NUM_BUFFER_SIZE, DEFAULT_VALID_INDEX_BUFFER_SIZE, &pKvsRtpTransceiver->sender.retransmitter));
    ASSERT_EQ(STATUS_SUCCESS, createRtpPacketWithSeqNum(0, &pRtpPacket));

    ASSERT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pKvsRtpTransceiver->sender.packetBuffer, pRtpPacket));
    ASSERT_EQ(STATUS_SUCCESS, onRtcpPacket(pKvsPeerConnection, validRtcpPacket, SIZEOF(validRtcpPacket)));
    RtcOutboundRtpStreamStats stats{};
    getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats);
    ASSERT_EQ(1, stats.nackCount);
    ASSERT_EQ(1, stats.retransmittedPacketsSent);
    ASSERT_EQ(10, stats.retransmittedBytesSent);
    freePeerConnection(&pRtcPeerConnection);
    freeRtpPacket(&pRtpPacket);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketCompound)
{
    KvsPeerConnection peerConnection{};

    BYTE compound[] = {
        0x80, 0xc8, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0xe1, 0xe3, 0x20, 0x43, 0xe5, 0x3d, 0x10, 0x2b, 0xbf, 0x58, 0xf7,
        0xef, 0x00, 0x00, 0x23, 0xf3, 0x00, 0x6c, 0xd3, 0x75, 0x81, 0xca, 0x00, 0x06, 0xf1, 0x2d, 0x7b, 0x4b, 0x01, 0x10,
        0x2f, 0x76, 0x6d, 0x4b, 0x51, 0x6e, 0x47, 0x6e, 0x55, 0x70, 0x4f, 0x2b, 0x70, 0x38, 0x64, 0x52, 0x00, 0x00,
    };
    EXPECT_EQ(STATUS_SUCCESS, onRtcpPacket(&peerConnection, compound, SIZEOF(compound)));
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketCompoundSenderReport)
{
    auto hexpacket = (PCHAR) "81C900076C1B58915E0C6E520400000000000002000000000102030400424344";
    BYTE rawpacket[64] = {0};
    UINT32 rawpacketSize = 64;
    EXPECT_EQ(STATUS_SUCCESS, hexDecode(hexpacket, strlen(hexpacket), rawpacket, &rawpacketSize));

    // added two transceivers to test correct transceiver stats in getRtpRemoteInboundStats
    initTransceiver(4242);               // fake transceiver
    auto t = addTransceiver(1577872978); // real transceiver

    EXPECT_EQ(STATUS_SUCCESS, onRtcpPacket(pKvsPeerConnection, rawpacket, rawpacketSize));

    RtcRemoteInboundRtpStreamStats stats{};
    EXPECT_EQ(STATUS_SUCCESS, getRtpRemoteInboundStats(pRtcPeerConnection, t, &stats));
    EXPECT_EQ(1, stats.reportsReceived);
    EXPECT_EQ(1, stats.roundTripTimeMeasurements);
    // onRtcpPacket uses real time clock GETTIME to calculate roundTripTime, cant test
    EXPECT_EQ(4.0 / 255.0, stats.fractionLost);
    EXPECT_LT(0, stats.totalRoundTripTime);
    EXPECT_LT(0, stats.roundTripTime);
    freePeerConnection(&pRtcPeerConnection);
}

TEST_F(RtcpFunctionalityTest, rembValueGet)
{
    BYTE rawRtcpPacket[] = {0x8f, 0xce, 0x00, 0x05, 0x61, 0x7a, 0x37, 0x43, 0x00, 0x00, 0x00, 0x00,
                            0x52, 0x45, 0x4d, 0x42, 0x01, 0x12, 0x46, 0x73, 0x6c, 0x76, 0xe8, 0x55};
    RtcpPacket rtcpPacket;

    MEMSET(&rtcpPacket, 0x00, SIZEOF(RtcpPacket));

    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(rawRtcpPacket, SIZEOF(rawRtcpPacket), &rtcpPacket));
    EXPECT_EQ(rtcpPacket.header.packetType, RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK);
    EXPECT_EQ(rtcpPacket.header.receptionReportCount, RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK);

    BYTE bufferTooSmall[] = {0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_TOO_SMALL, isRembPacket(bufferTooSmall, SIZEOF(bufferTooSmall)));

    BYTE bufferNoUniqueIdentifier[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_INVALID, isRembPacket(bufferNoUniqueIdentifier, SIZEOF(bufferNoUniqueIdentifier)));

    UINT8 ssrcListLen = 0;
    DOUBLE maximumBitRate = 0;
    UINT32 ssrcList[5];

    BYTE singleSSRC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42, 0x01, 0x12, 0x76, 0x28, 0x6c, 0x76, 0xe8, 0x55};
    EXPECT_EQ(STATUS_SUCCESS, rembValueGet(singleSSRC, SIZEOF(singleSSRC), &maximumBitRate, ssrcList, &ssrcListLen));
    EXPECT_EQ(ssrcListLen, 1);
    EXPECT_EQ(maximumBitRate, 2581120.0);
    EXPECT_EQ(ssrcList[0], 0x6c76e855);

    BYTE multipleSSRC[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45, 0x4d, 0x42,
                           0x02, 0x12, 0x76, 0x28, 0x6c, 0x76, 0xe8, 0x55, 0x42, 0x42, 0x42, 0x42};
    EXPECT_EQ(STATUS_SUCCESS, rembValueGet(multipleSSRC, SIZEOF(multipleSSRC), &maximumBitRate, ssrcList, &ssrcListLen));
    EXPECT_EQ(ssrcListLen, 2);
    EXPECT_EQ(maximumBitRate, 2581120.0);
    EXPECT_EQ(ssrcList[0], 0x6c76e855);
    EXPECT_EQ(ssrcList[1], 0x42424242);

    BYTE invalidSSRCLength[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45,
                                0x4d, 0x42, 0xFF, 0x12, 0x76, 0x28, 0x6c, 0x76, 0xe8, 0x55};
    EXPECT_EQ(STATUS_RTCP_INPUT_REMB_INVALID, rembValueGet(invalidSSRCLength, SIZEOF(invalidSSRCLength), &maximumBitRate, ssrcList, &ssrcListLen));
}

TEST_F(RtcpFunctionalityTest, onRtcpRembCalled)
{
    RtcpPacket rtcpPacket;

    MEMSET(&rtcpPacket, 0x00, SIZEOF(RtcpPacket));

    BYTE multipleSSRC[] = {0x80, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x52, 0x45,
                           0x4d, 0x42, 0x02, 0x12, 0x76, 0x28, 0x6c, 0x76, 0xe8, 0x55, 0x42, 0x42, 0x42, 0x42};

    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(multipleSSRC, ARRAY_SIZE(multipleSSRC), &rtcpPacket));
    initTransceiver(0x42424242);
    PRtcRtpTransceiver transceiver43 = addTransceiver(0x43);

    BOOL onBandwidthCalled42 = FALSE;
    BOOL onBandwidthCalled43 = FALSE;
    auto callback = [](UINT64 called, DOUBLE /*unused*/) { *((BOOL*) called) = TRUE; };
    transceiverOnBandwidthEstimation(pRtcRtpTransceiver, reinterpret_cast<UINT64>(&onBandwidthCalled42), callback);
    transceiverOnBandwidthEstimation(transceiver43, reinterpret_cast<UINT64>(&onBandwidthCalled43), callback);

    onRtcpRembPacket(&rtcpPacket, pKvsPeerConnection);
    ASSERT_TRUE(onBandwidthCalled42);
    ASSERT_FALSE(onBandwidthCalled43);
    freePeerConnection(&pRtcPeerConnection);
}

TEST_F(RtcpFunctionalityTest, onpli)
{
    BYTE rawRtcpPacket[] = {0x81, 0xCE, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x1D, 0xC8, 0x69, 0x91};
    RtcpPacket rtcpPacket{};
    BOOL on_picture_loss_called = FALSE;
    this->initTransceiver(0x1DC86991);

    pKvsRtpTransceiver->onPictureLossCustomData = (UINT64) &on_picture_loss_called;
    pKvsRtpTransceiver->onPictureLoss = [](UINT64 customData) -> void { *(PBOOL) customData = TRUE; };

    EXPECT_EQ(STATUS_SUCCESS, setRtcpPacketFromBytes(rawRtcpPacket, SIZEOF(rawRtcpPacket), &rtcpPacket));
    ASSERT_TRUE(rtcpPacket.header.packetType == RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK &&
                rtcpPacket.header.receptionReportCount == RTCP_PSFB_PLI);

    onRtcpPLIPacket(&rtcpPacket, pKvsPeerConnection);
    ASSERT_TRUE(on_picture_loss_called);
    RtcOutboundRtpStreamStats stats{};
    EXPECT_EQ(STATUS_SUCCESS, getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats));
    EXPECT_EQ(1, stats.pliCount);
    freePeerConnection(&pRtcPeerConnection);
}

static void testBwHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt, UINT64 duration)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(txBytes);
    UNUSED_PARAM(rxBytes);
    UNUSED_PARAM(txPacketsCnt);
    UNUSED_PARAM(rxPacketsCnt);
    UNUSED_PARAM(duration);
    return;
}

static void parseTwcc(const std::string& hex, const uint32_t expectedReceived, const uint32_t expectedNotReceived)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    RtcConfiguration config{};
    UINT64 value;
    UINT16 twsn;
    UINT16 i = 0;
    UINT32 extpayload, received = 0, lost = 0;

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    // Use the full buffer size as payloadLength so the parser can safely read
    // trailing zero bytes that act as zero-value deltas (matching RTCP padding behavior)
    rtcpPacket.payloadLength = SIZEOF(payload);

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);

    for (i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        twsn = i;
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    for (i = 0; i < MAX_UINT16; i++) {
        if (STATUS_SUCCEEDED(hashTableGet(pKvsPeerConnection->pTwccManager->pTwccRtpPktInfosHashTable, i, &value))) {
            PTwccRtpPacketInfo tempTwccRtpPktInfo = (PTwccRtpPacketInfo) value;
            if (tempTwccRtpPktInfo->remoteTimeKvs == TWCC_PACKET_LOST_TIME) {
                lost++;
            } else if (tempTwccRtpPktInfo->remoteTimeKvs != TWCC_PACKET_UNITIALIZED_TIME) {
                received++;
            }
        }
    }

    EXPECT_EQ(received + lost, TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload));
    EXPECT_EQ(expectedReceived + expectedNotReceived, TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload));
    EXPECT_EQ(expectedReceived, received);
    EXPECT_EQ(expectedNotReceived, lost);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, twccParsePacketTest)
{
    parseTwcc("", 0, 0);
    parseTwcc("4487A9E754B3E6FD01810001147A75A62001C801", 1, 0);
    parseTwcc("4487A9E754B3E6FD12740004148566AAC1402C00", 1, 3);
    parseTwcc("4487A9E754B3E6FD04FA0006147CAF88C554B80400000001", 1, 5);
    parseTwcc("4487A9E754B3E6FD00000002147972002002BC00", 2, 0);
    parseTwcc("4487A9E754B3E6FD06D40004147DDE41D6403C00FFEC0001", 2, 2);
    parseTwcc("4487A9E754B3E6FD04FA0006147CB089D95420FF9804000000000003", 2, 4);
    parseTwcc("4487A9E754B3E6FD000C000314797A052003E40004000003", 3, 0);
    parseTwcc("4487A9E754B3E6FD12740006148568ABD6648800FDA4000268000002", 3, 3);
    parseTwcc("4487A9E754B3E6FD1431000C14868C5A803CEC0028000002", 3, 9);
    parseTwcc("4487A9E754B3E6FD00020004147974012004140000000002", 4, 0);
    parseTwcc("4487A9E754B3E6FD12670008148560A8D66520016C00FD780402902800040002", 4, 4);
    parseTwcc("4487A9E754B3E6FD012E0005147A45872005900000000401", 5, 0);
    parseTwcc("4487A9E754B3E6FD01F20006147AC6D22006600004000000", 6, 0);
    parseTwcc("4487A9E754B3E6FD06690007147D9111200748000000040000000003", 7, 0);
    parseTwcc("4487A9E754B3E6FD020C0008147AD3D8200898000000000008000002", 8, 0);
    parseTwcc("4487A9E754B3E6FD07C20009147E7B8B200990000800000000000001", 9, 0);
    parseTwcc("4487A9E754B3E6FD0177000A147A74A5200A70000000000000040000", 10, 0);
    parseTwcc("4487A9E754B3E6FD1431000C14868E5B2008E540DC00000000000000FE10002800000003", 10, 2);
    parseTwcc("4487A9E754B3E6FD03C6000B147BEB6F200B3000380400000400040000000003", 11, 0);
    parseTwcc("4487A9E754B3E6FD02AB000D147B3013200D4800000004000000000000000401", 13, 0);
    parseTwcc("4487A9E754B3E6FD01BA000E147AA4C3200EA400000000000000000000000400", 14, 0);
    parseTwcc("4487A9E754B3E6FD0610000F147D62F3200FCC0000000000000400000000100000000003", 15, 0);
    parseTwcc("4487A9E754B3E6FD08120010147EAAA92010F80000000000000004040000000000000002", 16, 0);
    parseTwcc("4487A9E754B3E6FD05B80011147D33D52011F40014000000000000000000040000000001", 17, 0);
    parseTwcc("4487A9E754B3E6FD04DA001E147CAC86D556D999D6652009D40000000000EF840001040001DC0004D4000400031400", 17, 13);
    parseTwcc("4487A9E754B3E6FD11EA0012148514932012B40000000000000400000000000000000000", 18, 0);
    parseTwcc("4487A9E754B3E6FD09BC0013147FC45D201348000400000000000000000000000000000000000003", 19, 0);
    parseTwcc("4487A9E754B3E6FD05720014147D05B7201414000000000000100000000000040000000400000002", 20, 0);
    parseTwcc("4487A9E754B3E6FD03820015147BBD5A201554000000000000000000000000000000000400009801", 21, 0);
    parseTwcc("4487A9E754B3E6FD114B001B1484B87381FF200DE41000000000000000000000000000000000140000000002", 21, 6);
    parseTwcc("4487A9E754B3E6FD0B6700161480DD11201678000000000000000000040000000000000000000000", 22, 0);
    parseTwcc("4487A9E754B3E6FD07790017147E4E6F2017D400000000000400000000000000000004000400080000000003", 23, 0);
    parseTwcc("4487A9E754B3E6FD114B001D1484BB74D5592014E4008400000000FD60100000000000000000000000000000000014", 24, 5);
    parseTwcc("4487A9E754B3E6FD1230002914854FA22027E4002400000000000400000000000000040000000000040000001C0000", 41, 0);
    parseTwcc("4487A9E754B3E6FD04B60036147CAA852024C002D999D6407800000000000000000000000000040000000000000000", 43, 11);
    parseTwcc("4487A9E754B3E6FD040200E4147C9F81202700B7E6649000000000000000000004000000000008000018000000001", 43, 185);
}

TEST_F(RtcpFunctionalityTest, updateTwccHashTableTest)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    UINT64 receivedBytes = 0, receivedPackets = 0, sentBytes = 0, sentPackets = 0;
    INT64 duration = 0;
    PTwccRtpPacketInfo pTwccRtpPacketInfo = NULL;
    PHashTable pTwccRtpPktInfosHashTable = NULL;
    UINT16 hashTableInsertionCount = 0;
    UINT16 lowerBound = UINT16_MAX - 3;
    UINT16 upperBound = 3;
    UINT16 i = 0;

    // Initialize structs and members.
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler));

    // Grab the hash table.
    pTwccRtpPktInfosHashTable = pKvsPeerConnection->pTwccManager->pTwccRtpPktInfosHashTable;

    pKvsPeerConnection->pTwccManager->prevReportedBaseSeqNum = lowerBound;
    pKvsPeerConnection->pTwccManager->lastReportedSeqNum = upperBound + 10;

    // Breakup the packet indexes to be across the max int overflow.
    for (i = lowerBound; i <= UINT16_MAX && i != 0; i++) {
        pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, i, (UINT64) pTwccRtpPacketInfo));
        hashTableInsertionCount++;
    }
    for (i = 0; i < upperBound; i++) {
        pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, i, (UINT64) pTwccRtpPacketInfo));
        hashTableInsertionCount++;
    }

    // Add at a non-monotonically-increased index.
    pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, upperBound + 10, (UINT64) pTwccRtpPacketInfo));
    hashTableInsertionCount++;

    // Validate hash table size after and before updating (onRtcpTwccPacket case).
    EXPECT_EQ(hashTableInsertionCount, pTwccRtpPktInfosHashTable->itemCount);
    EXPECT_EQ(STATUS_SUCCESS,
              updateTwccHashTable(pKvsPeerConnection->pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets));
    EXPECT_EQ(0, pTwccRtpPktInfosHashTable->itemCount);

    hashTableInsertionCount = 0;
    pTwccRtpPacketInfo = NULL;
    for (i = 0; i <= upperBound; i++) {
        EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, i, (UINT64) pTwccRtpPacketInfo));
        hashTableInsertionCount++;
    }
    EXPECT_EQ(hashTableInsertionCount, pTwccRtpPktInfosHashTable->itemCount);
    EXPECT_EQ(STATUS_SUCCESS,
              updateTwccHashTable(pKvsPeerConnection->pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets));
    EXPECT_EQ(0, pTwccRtpPktInfosHashTable->itemCount);

    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    MUTEX_UNLOCK(pKvsPeerConnection->twccLock);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, updateTwccHashTableIntPromotionCase)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    PTwccRtpPacketInfo pTwccRtpPacketInfo = NULL;
    INT64 duration = 0;
    UINT64 receivedBytes = 0, receivedPackets = 0, sentBytes = 0, sentPackets = 0;
    UINT16 i;

    // Grab the hash table
    PHashTable pTwccRtpPktInfosHashTable = pKvsPeerConnection->pTwccManager->pTwccRtpPktInfosHashTable;
    UINT16 hashTableInsertionCount = 0;

    // Set up the hash table
    pKvsPeerConnection->pTwccManager->prevReportedBaseSeqNum = UINT16_MAX;
    pKvsPeerConnection->pTwccManager->lastReportedSeqNum = UINT16_MAX;

    // Add packet at UINT16_MAX
    pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, UINT16_MAX, (UINT64) pTwccRtpPacketInfo));
    hashTableInsertionCount++;

    // Even though pTwccManager->lastReportedSeqNum is a UINT16, (pTwccManager->lastReportedSeqNum + 1) can get
    // promoted to an int (32) when pTwccManager->lastReportedSeqNum == UINT16_MAX
    EXPECT_EQ(STATUS_SUCCESS,
              updateTwccHashTable(pKvsPeerConnection->pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets));

    EXPECT_EQ(0, pTwccRtpPktInfosHashTable->itemCount); // Ensure the table is cleared again

    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    MUTEX_UNLOCK(pKvsPeerConnection->twccLock);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verifies correct duration calculation when the previous packet (seqNum - 1)
// exists in the hash table but has a NULL pointer value.
// Hash table should never have a NULL entry so this is not expected.
//
// Hash table state:
//   key 4 -> NULL    (previous packet before range, simulates unexpected state)
//   key 5 -> pkt{localTime=1000}  (start of range)
//   key 6 -> pkt{localTime=2000}
//   key 7 -> pkt{localTime=3000}  (end of range)
//
// Expected behavior:
//   1. seqNum=5: hashTableGet(4) succeeds but returns NULL
//                -> fallback uses seqNum=5's packet -> localStartTimeKvs = 1000
//   2. seqNum=7: last packet processed -> localEndTimeKvs = 3000
//   3. duration = 3000 - 1000 = 2000
TEST_F(RtcpFunctionalityTest, updateTwccHashTableNullPrevPacket)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    UINT64 receivedBytes = 0, receivedPackets = 0, sentBytes = 0, sentPackets = 0;
    INT64 duration = 0;
    PTwccRtpPacketInfo pTwccRtpPacketInfo = NULL;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    PHashTable pTwccRtpPktInfosHashTable = pKvsPeerConnection->pTwccManager->pTwccRtpPktInfosHashTable;

    // Set range to iterate: seqNums 5 through 7
    pKvsPeerConnection->pTwccManager->prevReportedBaseSeqNum = 5;
    pKvsPeerConnection->pTwccManager->lastReportedSeqNum = 7;

    // Simulate a NULL entry at seqNum 4 (the previous packet before the range).
    // This exercises the case where hashTableGet(seqNum-1) succeeds but the
    // stored pointer is NULL, requiring the fallback to determine localStartTimeKvs.
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, 4, (UINT64) NULL));

    // Insert real packets at seqNums 5, 6, 7 with increasing timestamps.
    pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
    pTwccRtpPacketInfo->localTimeKvs = 1000;
    pTwccRtpPacketInfo->remoteTimeKvs = 2000;
    pTwccRtpPacketInfo->packetSize = 50;
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, 5, (UINT64) pTwccRtpPacketInfo));

    pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
    pTwccRtpPacketInfo->localTimeKvs = 2000;
    pTwccRtpPacketInfo->remoteTimeKvs = 3000;
    pTwccRtpPacketInfo->packetSize = 100;
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, 6, (UINT64) pTwccRtpPacketInfo));

    pTwccRtpPacketInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
    pTwccRtpPacketInfo->localTimeKvs = 3000;
    pTwccRtpPacketInfo->remoteTimeKvs = 4000;
    pTwccRtpPacketInfo->packetSize = 150;
    EXPECT_EQ(STATUS_SUCCESS, hashTableUpsert(pTwccRtpPktInfosHashTable, 7, (UINT64) pTwccRtpPacketInfo));

    // duration should be localEndTimeKvs(3000) - localStartTimeKvs(1000) = 2000
    EXPECT_EQ(STATUS_SUCCESS,
              updateTwccHashTable(pKvsPeerConnection->pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets));

    EXPECT_EQ(2000, duration);
    EXPECT_EQ(300, receivedBytes);
    EXPECT_EQ(3, receivedPackets);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// ---- TWCC Trendline and API Tests ----

TEST_F(RtcpFunctionalityTest, computeTwccTrendline_stableNetwork)
{
    // Simulate packets sent and received with constant spacing (no congestion)
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcConfiguration config{};
    DOUBLE delayTrend = 0.0, queueDelay = 0.0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    PTwccManager pTwccManager = pKvsPeerConnection->pTwccManager;
    ASSERT_NE(nullptr, pTwccManager);

    // Insert 10 packets with uniform 10ms send and receive spacing (no delay variation)
    for (UINT16 i = 0; i < 10; i++) {
        PTwccRtpPacketInfo pInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        pInfo->localTimeKvs = (1000 + i * 10) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        pInfo->remoteTimeKvs = (1000 + i * 10) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // same spacing
        pInfo->packetSize = 1200;
        hashTablePut(pTwccManager->pTwccRtpPktInfosHashTable, i, (UINT64) pInfo);
    }
    pTwccManager->prevReportedBaseSeqNum = 0;
    pTwccManager->lastReportedSeqNum = 9;

    EXPECT_EQ(STATUS_SUCCESS, computeTwccTrendline(pTwccManager, &delayTrend, &queueDelay));

    // With uniform spacing, delay variation should be ~0
    EXPECT_NEAR(0.0, delayTrend, 0.001);
    EXPECT_NEAR(0.0, queueDelay, 0.001);

    // freePeerConnection will clean up hash table entries via freeHashEntry
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, computeTwccTrendline_increasingDelay)
{
    // Simulate packets where receiver gaps grow (congestion building)
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcConfiguration config{};
    DOUBLE delayTrend = 0.0, queueDelay = 0.0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    PTwccManager pTwccManager = pKvsPeerConnection->pTwccManager;

    // Sender sends every 10ms, but receiver gets them with increasing gaps (10, 11, 12, 13... ms)
    UINT64 recvTime = 1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    for (UINT16 i = 0; i < 10; i++) {
        PTwccRtpPacketInfo pInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        pInfo->localTimeKvs = i * 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        pInfo->remoteTimeKvs = recvTime;
        pInfo->packetSize = 1200;
        hashTablePut(pTwccManager->pTwccRtpPktInfosHashTable, i, (UINT64) pInfo);
        recvTime += (10 + i) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // growing gaps
    }
    pTwccManager->prevReportedBaseSeqNum = 0;
    pTwccManager->lastReportedSeqNum = 9;

    EXPECT_EQ(STATUS_SUCCESS, computeTwccTrendline(pTwccManager, &delayTrend, &queueDelay));

    // Delay trend should be positive (congestion building)
    EXPECT_GT(delayTrend, 0.0);
    // Queue delay should be positive (accumulated delay variation)
    EXPECT_GT(queueDelay, 0.0);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, computeTwccTrendline_decreasingDelay)
{
    // Simulate packets where receiver gaps are smaller than sender gaps and shrinking (congestion clearing)
    // Sender sends every 10ms, receiver gaps: 9, 8, 7, 6, 5, 4, 3, 2, 1 ms - all below send interval
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcConfiguration config{};
    DOUBLE delayTrend = 0.0, queueDelay = 0.0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    PTwccManager pTwccManager = pKvsPeerConnection->pTwccManager;

    UINT64 recvTime = 1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    for (UINT16 i = 0; i < 10; i++) {
        PTwccRtpPacketInfo pInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        pInfo->localTimeKvs = i * 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        pInfo->remoteTimeKvs = recvTime;
        pInfo->packetSize = 1200;
        hashTablePut(pTwccManager->pTwccRtpPktInfosHashTable, i, (UINT64) pInfo);
        recvTime += (9 - i) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // gaps smaller than send interval and shrinking
    }
    pTwccManager->prevReportedBaseSeqNum = 0;
    pTwccManager->lastReportedSeqNum = 9;

    EXPECT_EQ(STATUS_SUCCESS, computeTwccTrendline(pTwccManager, &delayTrend, &queueDelay));

    // Delay trend should be negative (congestion clearing)
    EXPECT_LT(delayTrend, 0.0);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, computeTwccTrendline_skipsLostPackets)
{
    // Verify that lost packets (remoteTimeKvs == TWCC_PACKET_LOST_TIME) are skipped
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcConfiguration config{};
    DOUBLE delayTrend = 0.0, queueDelay = 0.0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    PTwccManager pTwccManager = pKvsPeerConnection->pTwccManager;

    // 5 packets, middle one is lost
    for (UINT16 i = 0; i < 5; i++) {
        PTwccRtpPacketInfo pInfo = (PTwccRtpPacketInfo) MEMCALLOC(1, SIZEOF(TwccRtpPacketInfo));
        pInfo->localTimeKvs = i * 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        if (i == 2) {
            pInfo->remoteTimeKvs = TWCC_PACKET_LOST_TIME;
        } else {
            pInfo->remoteTimeKvs = i * 10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        }
        pInfo->packetSize = 1200;
        hashTablePut(pTwccManager->pTwccRtpPktInfosHashTable, i, (UINT64) pInfo);
    }
    pTwccManager->prevReportedBaseSeqNum = 0;
    pTwccManager->lastReportedSeqNum = 4;

    EXPECT_EQ(STATUS_SUCCESS, computeTwccTrendline(pTwccManager, &delayTrend, &queueDelay));

    // Should still compute without crashing; stable network so ~0
    EXPECT_NEAR(0.0, queueDelay, 0.5);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, computeTwccTrendline_nullArgs)
{
    DOUBLE delayTrend, queueDelay;
    EXPECT_EQ(STATUS_NULL_ARG, computeTwccTrendline(NULL, &delayTrend, &queueDelay));
}

TEST_F(RtcpFunctionalityTest, setOnPeerCongestionFeedbackFn_basic)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    RtcConfiguration config{};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));

    // Null peer connection
    EXPECT_EQ(STATUS_NULL_ARG, setOnPeerCongestionFeedbackFn(NULL, 0, NULL));

    // Setting NULL callback is valid (disables)
    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, 0, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, setOnTwccFeedbackReceived_basic)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    RtcConfiguration config{};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));

    EXPECT_EQ(STATUS_NULL_ARG, setOnTwccFeedbackReceived(NULL, 0, NULL));
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, 0, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Test context for custom TWCC callback
struct TwccCallbackCtx {
    UINT32 callCount;
    UINT32 feedbackCount;
    PTwccFeedback pCapturedList;
    DOUBLE delayTrendToReturn;
};

static STATUS testTwccFeedbackCallback(UINT64 customData, PTwccFeedback pFeedbackList, UINT32 feedbackListLen, PTwccCongestionState pCongestionState)
{
    TwccCallbackCtx* pCtx = (TwccCallbackCtx*) customData;
    pCtx->callCount++;
    pCtx->feedbackCount = feedbackListLen;
    // Copy the list so we can inspect it after the call
    if (feedbackListLen > 0 && pFeedbackList != NULL) {
        pCtx->pCapturedList = (PTwccFeedback) MEMALLOC(feedbackListLen * SIZEOF(TwccFeedback));
        MEMCPY(pCtx->pCapturedList, pFeedbackList, feedbackListLen * SIZEOF(TwccFeedback));
    }
    pCongestionState->delayTrend = pCtx->delayTrendToReturn;
    return STATUS_SUCCESS;
}

static STATUS testCongestionFeedbackHandler(UINT64 customData, PCongestionCtx pCtx)
{
    UNUSED_PARAM(customData);
    UNUSED_PARAM(pCtx);
    return STATUS_SUCCESS;
}

// Verifies that the custom callback receives a properly populated TwccFeedback list
// with per-packet data from the TWCC report.
TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_feedbackListPopulated)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;
    TwccCallbackCtx ctx{};
    ctx.delayTrendToReturn = 0.5;

    // Use a known TWCC packet: base=0x0181, 1 received, 0 lost
    const std::string hex = "4487A9E754B3E6FD01810001147A75A62001C801";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Need congestion callback so twccManagerOnPacketSent stores packets
    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, 0, testCongestionFeedbackHandler));
    // Register custom callback
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &ctx, testTwccFeedbackCallback));

    // Simulate sending the packets so they exist in the hash table
    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Process the TWCC feedback packet - this should invoke our callback
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // Verify callback was invoked with correct data
    EXPECT_EQ(1u, ctx.callCount);
    EXPECT_EQ(1u, ctx.feedbackCount);
    ASSERT_NE(nullptr, ctx.pCapturedList);
    EXPECT_EQ(baseSeqNum, ctx.pCapturedList[0].twccSeqNum);
    EXPECT_EQ(100u, ctx.pCapturedList[0].packetSize);
    EXPECT_NE(0u, ctx.pCapturedList[0].recvTime);
    EXPECT_NE(TWCC_PACKET_LOST_TIME, ctx.pCapturedList[0].recvTime);

    SAFE_MEMFREE(ctx.pCapturedList);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verifies that lost packets are excluded from the feedback list
TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_excludesLostPackets)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;
    TwccCallbackCtx ctx{};
    ctx.delayTrendToReturn = -0.1;

    // TWCC packet with 1 received, 3 lost (4 total)
    const std::string hex = "4487A9E754B3E6FD12740004148566AAC1402C00";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, 0, testCongestionFeedbackHandler));
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &ctx, testTwccFeedbackCallback));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // Only received packets should be in the list (1 received, 3 lost)
    EXPECT_EQ(1u, ctx.callCount);
    EXPECT_EQ(1u, ctx.feedbackCount);
    ASSERT_NE(nullptr, ctx.pCapturedList);
    EXPECT_NE(TWCC_PACKET_LOST_TIME, ctx.pCapturedList[0].recvTime);

    SAFE_MEMFREE(ctx.pCapturedList);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verifies callback receives multiple received packets
TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_multipleReceivedPackets)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;
    TwccCallbackCtx ctx{};
    ctx.delayTrendToReturn = 0.0;

    // TWCC packet with 3 received, 0 lost
    const std::string hex = "4487A9E754B3E6FD000C000314797A052003E40004000003";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, 0, testCongestionFeedbackHandler));
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &ctx, testTwccFeedbackCallback));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // All 3 packets received
    EXPECT_EQ(1u, ctx.callCount);
    EXPECT_EQ(3u, ctx.feedbackCount);
    ASSERT_NE(nullptr, ctx.pCapturedList);

    // Verify sequence numbers are in order
    for (UINT32 i = 0; i < ctx.feedbackCount; i++) {
        EXPECT_EQ((UINT16) (baseSeqNum + i), ctx.pCapturedList[i].twccSeqNum);
        EXPECT_EQ(100u, ctx.pCapturedList[i].packetSize);
        EXPECT_NE(0u, ctx.pCapturedList[i].recvTime);
        EXPECT_NE(TWCC_PACKET_LOST_TIME, ctx.pCapturedList[i].recvTime);
    }

    SAFE_MEMFREE(ctx.pCapturedList);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

static STATUS testTwccFeedbackCallbackError(UINT64 customData, PTwccFeedback pFeedbackList, UINT32 feedbackListLen,
                                            PTwccCongestionState pCongestionState)
{
    UNUSED_PARAM(pFeedbackList);
    UNUSED_PARAM(feedbackListLen);
    UNUSED_PARAM(pCongestionState);
    TwccCallbackCtx* pCtx = (TwccCallbackCtx*) customData;
    pCtx->callCount++;
    return STATUS_INVALID_OPERATION;
}

// Verifies no deadlock when custom callback returns an error
TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_callbackErrorNoDeadlock)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;
    TwccCallbackCtx ctx{};

    const std::string hex = "4487A9E754B3E6FD01810001147A75A62001C801";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, 0, testCongestionFeedbackHandler));
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &ctx, testTwccFeedbackCallbackError));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Callback returns error - should propagate without deadlock
    EXPECT_EQ(STATUS_INVALID_OPERATION, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(1u, ctx.callCount);

    // Verify mutex is not held - we can still lock/unlock it
    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    MUTEX_UNLOCK(pKvsPeerConnection->twccLock);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verify that a TWCC packet with packetStatusCount=0 does not invoke the custom
// feedback callback
TEST_F(RtcpFunctionalityTest, twccZeroPacketStatusCountDoesNotInvokeCallback)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    UINT32 extpayload;
    UINT32 callbackInvocations = 0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Set custom TWCC feedback callback that counts invocations
    auto twccCallback = [](UINT64 customData, PTwccFeedback pFeedback, UINT32 feedbackCount, PTwccCongestionState pState) -> STATUS {
        UNUSED_PARAM(pFeedback);
        UNUSED_PARAM(feedbackCount);
        (*(PUINT32) customData)++;
        pState->delayTrend = 0.0;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &callbackInvocations, twccCallback));

    // Send a few packets so TWCC manager has state
    for (UINT16 i = 100; i < 110; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), i);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Craft a TWCC feedback packet with packetStatusCount = 0 but a non-zero baseSeqNum.
    BYTE payload[16] = {0};
    payload[0] = 0x44; payload[1] = 0x87; payload[2] = 0xA9; payload[3] = 0xE7;
    payload[4] = 0x54; payload[5] = 0xB3; payload[6] = 0xE6; payload[7] = 0xFD;
    // baseSeqNum = 500
    payload[8] = 0x01; payload[9] = 0xF4;
    // packetStatusCount = 0
    payload[10] = 0x00; payload[11] = 0x00;
    // referenceTime + fb pkt count
    payload[12] = 0x14; payload[13] = 0x7A; payload[14] = 0x00; payload[15] = 0x01;

    rtcpPacket.header.packetLength = SIZEOF(payload) / 4;
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    rtcpPacket.header.receptionReportCount = RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = SIZEOF(payload);

    // onRtcpTwccPacket should NOT invoke the callback for a zero-count report.
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(0u, callbackInvocations);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verify that after a valid TWCC report, a subsequent zero-count report does NOT
// trigger the callback
TEST_F(RtcpFunctionalityTest, twccZeroStatusCountAfterValidReportNoSpuriousCallback)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    UINT32 extpayload;
    UINT32 callbackInvocations = 0;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Set custom TWCC feedback callback
    auto twccCallback = [](UINT64 customData, PTwccFeedback pFeedback, UINT32 feedbackCount, PTwccCongestionState pState) -> STATUS {
        UNUSED_PARAM(pFeedback);
        UNUSED_PARAM(feedbackCount);
        (*(PUINT32) customData)++;
        pState->delayTrend = 0.0;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &callbackInvocations, twccCallback));

    // Send packets with seqNum 0..4
    for (UINT16 i = 0; i < 5; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), i);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // First: a valid TWCC report for baseSeqNum=0, count=2, all received
    BYTE validPayload[20] = {0};
    validPayload[0] = 0x44; validPayload[1] = 0x87; validPayload[2] = 0xA9; validPayload[3] = 0xE7;
    validPayload[4] = 0x54; validPayload[5] = 0xB3; validPayload[6] = 0xE6; validPayload[7] = 0xFD;
    validPayload[8] = 0x00; validPayload[9] = 0x00;  // baseSeqNum = 0
    validPayload[10] = 0x00; validPayload[11] = 0x02; // packetStatusCount = 2
    validPayload[12] = 0x14; validPayload[13] = 0x79; validPayload[14] = 0x72;
    validPayload[15] = 0x00;
    // Run-length chunk: status=1 (small delta), length=2
    validPayload[16] = 0x20; validPayload[17] = 0x02;
    // Two small deltas
    validPayload[18] = 0xBC; validPayload[19] = 0x00;

    rtcpPacket.header.packetLength = SIZEOF(validPayload) / 4;
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    rtcpPacket.header.receptionReportCount = RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK;
    rtcpPacket.payload = validPayload;
    rtcpPacket.payloadLength = SIZEOF(validPayload);

    // Valid report should invoke the callback exactly once
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(1u, callbackInvocations);

    // Now: zero-count TWCC report with baseSeqNum=60000
    BYTE emptyPayload[16] = {0};
    emptyPayload[0] = 0x44; emptyPayload[1] = 0x87; emptyPayload[2] = 0xA9; emptyPayload[3] = 0xE7;
    emptyPayload[4] = 0x54; emptyPayload[5] = 0xB3; emptyPayload[6] = 0xE6; emptyPayload[7] = 0xFD;
    emptyPayload[8] = 0xEA; emptyPayload[9] = 0x60;  // baseSeqNum = 60000
    emptyPayload[10] = 0x00; emptyPayload[11] = 0x00; // packetStatusCount = 0
    emptyPayload[12] = 0x14; emptyPayload[13] = 0x7A; emptyPayload[14] = 0x00;
    emptyPayload[15] = 0x02;

    rtcpPacket.payload = emptyPayload;
    rtcpPacket.payloadLength = SIZEOF(emptyPayload);
    rtcpPacket.header.packetLength = SIZEOF(emptyPayload) / 4;

    // Zero-count report should NOT invoke the callback again.
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(1u, callbackInvocations);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verify that zero packetStatusCount skips ALL callbacks: onTwccFeedbackReceived,
// onPeerCongestionFeedback, and onSenderBandwidthEstimation.
TEST_F(RtcpFunctionalityTest, twccZeroPacketStatusCountSkipsAllCallbacks)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    UINT32 extpayload;

    struct CallbackCounters {
        UINT32 twccFeedback;
        UINT32 congestionFeedback;
        UINT32 bweCallback;
    } counters = {0, 0, 0};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Set all three callbacks
    auto twccCallback = [](UINT64 customData, PTwccFeedback pFeedback, UINT32 feedbackCount, PTwccCongestionState pState) -> STATUS {
        UNUSED_PARAM(pFeedback);
        UNUSED_PARAM(feedbackCount);
        ((CallbackCounters*) customData)->twccFeedback++;
        pState->delayTrend = 0.0;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &counters, twccCallback));

    auto congestionCallback = [](UINT64 customData, PCongestionCtx pCtx) -> STATUS {
        UNUSED_PARAM(pCtx);
        ((CallbackCounters*) customData)->congestionFeedback++;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, (UINT64) &counters, congestionCallback));

    auto bweCallback = [](UINT64 customData, UINT32 sentBytes, UINT32 receivedBytes, UINT32 sentPackets, UINT32 receivedPackets,
                          UINT64 duration) -> VOID {
        UNUSED_PARAM(sentBytes);
        UNUSED_PARAM(receivedBytes);
        UNUSED_PARAM(sentPackets);
        UNUSED_PARAM(receivedPackets);
        UNUSED_PARAM(duration);
        ((CallbackCounters*) customData)->bweCallback++;
    };
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, (UINT64) &counters, bweCallback));

    // Send packets so TWCC manager has state
    for (UINT16 i = 0; i < 10; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), i);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Craft a TWCC feedback packet with packetStatusCount = 0
    BYTE payload[16] = {0};
    payload[0] = 0x44; payload[1] = 0x87; payload[2] = 0xA9; payload[3] = 0xE7;
    payload[4] = 0x54; payload[5] = 0xB3; payload[6] = 0xE6; payload[7] = 0xFD;
    payload[8] = 0x00; payload[9] = 0x00;  // baseSeqNum = 0
    payload[10] = 0x00; payload[11] = 0x00; // packetStatusCount = 0
    payload[12] = 0x14; payload[13] = 0x7A; payload[14] = 0x00; payload[15] = 0x01;

    rtcpPacket.header.packetLength = SIZEOF(payload) / 4;
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    rtcpPacket.header.receptionReportCount = RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = SIZEOF(payload);

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // None of the callbacks should have been invoked
    EXPECT_EQ(0u, counters.twccFeedback);
    EXPECT_EQ(0u, counters.congestionFeedback);
    EXPECT_EQ(0u, counters.bweCallback);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verify that a legitimate TWCC report spanning the UINT16 wraparound boundary
// (e.g., seqNums 65534..1) works correctly and invokes the callback with proper data.
TEST_F(RtcpFunctionalityTest, twccSeqNumWraparoundInvokesCallbackCorrectly)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    UINT32 extpayload;

    struct CallbackCtx {
        UINT32 invocations;
        UINT32 feedbackCount;
    } cbCtx = {0, 0};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    // Custom callback that tracks invocations and feedbackCount
    auto twccCallback = [](UINT64 customData, PTwccFeedback pFeedback, UINT32 feedbackCount, PTwccCongestionState pState) -> STATUS {
        UNUSED_PARAM(pFeedback);
        auto* ctx = (CallbackCtx*) customData;
        ctx->invocations++;
        ctx->feedbackCount = feedbackCount;
        pState->delayTrend = 0.0;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &cbCtx, twccCallback));

    // Send packets with seqNums 65534, 65535, 0, 1 (wrapping around UINT16 boundary)
    UINT16 seqNums[] = {65534, 65535, 0, 1};
    for (int i = 0; i < 4; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), seqNums[i]);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Craft a TWCC feedback packet: baseSeqNum=65534, packetStatusCount=4, all received
    BYTE payload[22] = {0};
    // SSRC fields (arbitrary)
    payload[0] = 0x44; payload[1] = 0x87; payload[2] = 0xA9; payload[3] = 0xE7;
    payload[4] = 0x54; payload[5] = 0xB3; payload[6] = 0xE6; payload[7] = 0xFD;
    // baseSeqNum = 65534 (0xFFFE)
    payload[8] = 0xFF; payload[9] = 0xFE;
    // packetStatusCount = 4
    payload[10] = 0x00; payload[11] = 0x04;
    // referenceTime
    payload[12] = 0x14; payload[13] = 0x79; payload[14] = 0x72;
    // fb pkt count
    payload[15] = 0x01;
    // Run-length chunk: status=1 (small delta), length=4 -> 0x2004
    payload[16] = 0x20; payload[17] = 0x04;
    // Four small deltas (1 byte each): 10, 10, 10, 10
    payload[18] = 0x0A; payload[19] = 0x0A; payload[20] = 0x0A; payload[21] = 0x0A;

    rtcpPacket.header.packetLength = SIZEOF(payload) / 4;
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    rtcpPacket.header.receptionReportCount = RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = SIZEOF(payload);

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // Callback should have been invoked exactly once with all 4 packets reported
    EXPECT_EQ(1u, cbCtx.invocations);
    EXPECT_EQ(4u, cbCtx.feedbackCount);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

// Verify that a malformed TWCC packet where the run-length chunk exceeds
// packetStatusCount does not cause a buffer overflow in the feedback list.
// The crafted packet claims packetStatusCount=2 but has a run-length chunk of 10,
// causing parseRtcpTwccPacket to set lastReportedSeqNum far beyond what
// packetStatusCount indicates. Without the bounds check, the feedback loop would
// write past the allocated buffer.
TEST_F(RtcpFunctionalityTest, twccMalformedRunLengthExceedsStatusCountNoCrash)
{
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    UINT32 extpayload;

    struct CallbackCtx {
        UINT32 invocations;
        UINT32 feedbackCount;
    } cbCtx = {0, 0};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    auto twccCallback = [](UINT64 customData, PTwccFeedback pFeedback, UINT32 feedbackCount, PTwccCongestionState pState) -> STATUS {
        UNUSED_PARAM(pFeedback);
        auto* ctx = (CallbackCtx*) customData;
        ctx->invocations++;
        ctx->feedbackCount = feedbackCount;
        pState->delayTrend = 0.0;
        return STATUS_SUCCESS;
    };
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &cbCtx, twccCallback));

    // Send 10 packets with seqNums 0..9 so they exist in the hash table
    for (UINT16 i = 0; i < 10; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), i);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Craft a malformed TWCC feedback packet:
    //   baseSeqNum = 0
    //   packetStatusCount = 2  (claims only 2 packets)
    //   But the run-length chunk says 10 (small delta, run=10)
    //   Followed by 10 small delta values
    // This causes parseRtcpTwccPacket to advance lastReportedSeqNum to 9,
    // creating a range of 10 while the allocation is only for 2 elements.
    BYTE payload[28] = {0};
    // SSRC of sender
    payload[0] = 0x44; payload[1] = 0x87; payload[2] = 0xA9; payload[3] = 0xE7;
    // SSRC of media source
    payload[4] = 0x54; payload[5] = 0xB3; payload[6] = 0xE6; payload[7] = 0xFD;
    // baseSeqNum = 0
    payload[8] = 0x00; payload[9] = 0x00;
    // packetStatusCount = 2 (the lie - actually 10 packets in the chunk)
    payload[10] = 0x00; payload[11] = 0x02;
    // referenceTime (3 bytes) + fb pkt count (1 byte)
    payload[12] = 0x14; payload[13] = 0x79; payload[14] = 0x72; payload[15] = 0x01;
    // Run-length chunk: bit 15=0 (run-length), status=01 (small delta), run length=10
    // Format: 0|SS|RRRRRRRRRRRRR -> 0|01|0000000001010 = 0x200A
    payload[16] = 0x20; payload[17] = 0x0A;
    // 10 small delta values (1 byte each): all 10 (= 2.5ms each in TWCC ticks)
    payload[18] = 0x0A; payload[19] = 0x0A; payload[20] = 0x0A; payload[21] = 0x0A;
    payload[22] = 0x0A; payload[23] = 0x0A; payload[24] = 0x0A; payload[25] = 0x0A;
    payload[26] = 0x0A; payload[27] = 0x0A;

    rtcpPacket.header.packetLength = SIZEOF(payload) / 4;
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    rtcpPacket.header.receptionReportCount = RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = SIZEOF(payload);

    // Without the feedbackCount < reportLen guard, this would overflow the buffer.
    // With the fix, it should complete without crashing.
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    // Callback should have been invoked
    EXPECT_EQ(1u, cbCtx.invocations);
    // feedbackCount should be capped at packetStatusCount (2), not the full 10
    EXPECT_LE(cbCtx.feedbackCount, 2u);

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, parseRtcpTwccPacketRejectsTruncatedChunks)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler));

    // TWCC feedback payload layout (offsets into payload):
    //   [0..3]   sender SSRC
    //   [4..7]   media source SSRC
    //   [8..9]   base sequence number
    //   [10..11] packet status count  <-- lied: claims 0x4000 packets
    //   [12..14] reference time, [15] feedback packet count
    //   [16..]   status chunks (only ONE run-length chunk supplied here)
    // The single run-length chunk encodes NOTRECEIVED for a large run, so the decode loop's
    // packetsRemaining stays > 0 after the one present chunk is consumed.
    const UINT32 payloadLen = 18; // 16-byte header + exactly one 2-byte chunk
    PBYTE pHeapPayload = (PBYTE) MEMCALLOC(1, payloadLen);
    ASSERT_TRUE(pHeapPayload != NULL);

    putInt16((PINT16) (pHeapPayload + 8), 0x0000);  // base seq num
    putInt16((PINT16) (pHeapPayload + 10), 0x4000); // packet status count (huge, lies)
    // One run-length chunk: top bit 0 (run length), status NOTRECEIVED, run length covering many.
    putInt16((PINT16) (pHeapPayload + 16), (INT16) 0x1FFF);

    RtcpPacket rtcpPacket{};
    rtcpPacket.payload = pHeapPayload;
    rtcpPacket.payloadLength = payloadLen;
    rtcpPacket.header.packetLength = payloadLen / 4;

    // The walk is bounded by payloadLength and the call completes without over-reading.
    EXPECT_EQ(STATUS_SUCCESS, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    SAFE_MEMFREE(pHeapPayload);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, parseRtcpTwccPacketRejectsTruncatedDeltas)
{
    /** Construct a TWCC feedback packet where packetStatusCount claims more
     *  received packets than recv-delta bytes present in the payload.
     *
     *  TWCC feedback layout (RFC draft-holmer-rmcat-transport-wide-cc):
     *    Bytes 0-7:   SSRC of sender + SSRC of media source
     *    Bytes 8-9:   base sequence number
     *    Bytes 10-11: packet status count
     *    Bytes 12-14: reference time (24 bits)
     *    Byte 15:     feedback packet count
     *    Bytes 16+:   packet status chunks (2 bytes each)
     *    After chunks: recv-delta values (1 or 2 bytes each)
     *
     *  This packet claims packetStatusCount=100 with a single run-length chunk
     *  marking all as SMALLDELTA (1 byte each), but only provides 2 delta bytes.
     */
    BYTE twccPayload[] = {
        0x00, 0x00, 0x00, 0x01, // SSRC sender
        0x00, 0x00, 0x00, 0x02, // SSRC media
        0x00, 0x01,             // base seq = 1
        0x00, 0x64,             // packetStatusCount = 100
        0x00, 0x00, 0x01,       // reference time = 1
        0x01,                   // fb pkt count = 1
        0x20, 0x64,             // run-length chunk: status=SMALLDELTA(1), count=100
        0x0A, 0x0B,             // only 2 recv-delta bytes (need 100)
    };

    RtcpPacket rtcpPacket;
    MEMSET(&rtcpPacket, 0, SIZEOF(RtcpPacket));
    rtcpPacket.payload = twccPayload;
    rtcpPacket.payloadLength = SIZEOF(twccPayload);

    RtcConfiguration config;
    PRtcPeerConnection pRtcPeerConnection = NULL;
    PKvsPeerConnection pKvsPeerConnection = NULL;
    MEMSET(&config, 0, SIZEOF(RtcConfiguration));

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler));

    // The parser should gracefully stop when it runs out of delta bytes
    // rather than returning an error. It processes what it can.
    EXPECT_EQ(STATUS_SUCCESS, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, parseRtcpTwccPacketRejectsShortPayload)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    BYTE payload[16] = {0};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    rtcpPacket.payload = payload;

    rtcpPacket.payloadLength = 0;
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_TOO_SMALL, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    rtcpPacket.payloadLength = 10;
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_TOO_SMALL, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    rtcpPacket.payloadLength = 15;
    EXPECT_EQ(STATUS_RTCP_INPUT_PACKET_TOO_SMALL, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    rtcpPacket.payloadLength = 16;
    EXPECT_EQ(STATUS_SUCCESS, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, parseRtcpTwccPacketClampsOversizedPacketStatusCount)
{
    // Verify that the feedback list is bounded to TWCC_MAX_PACKET_STATUS_COUNT.
    // Without the fix, reportLen = 2049 and the callback receives 2049 items.
    // With the fix, it's capped to 2048.
    const UINT16 OVERSIZED_COUNT = TWCC_MAX_PACKET_STATUS_COUNT + 1; // 2049
    const UINT16 BASE_SEQ = 0;

    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtpPacket rtpPacket{};

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler));

    TwccCallbackCtx ctx{};
    ctx.delayTrendToReturn = 0.5;
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &ctx, testTwccFeedbackCallback));

    // Pre-populate the hash table with OVERSIZED_COUNT sent packets
    BYTE extBuf[4];
    for (UINT16 i = 0; i < OVERSIZED_COUNT; i++) {
        MEMSET(&rtpPacket, 0, SIZEOF(RtpPacket));
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = BASE_SEQ + i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        MEMCPY(extBuf, &extpayload, SIZEOF(UINT32));
        rtpPacket.header.extensionPayload = extBuf;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = extBuf;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Build TWCC feedback payload:
    //   16 bytes header + 2 bytes run-length chunk + OVERSIZED_COUNT delta bytes
    const UINT32 payloadSize = 16 + 2 + OVERSIZED_COUNT;
    std::vector<BYTE> twccPayload(payloadSize, 0);

    // Bytes 8-9: base sequence number
    twccPayload[8] = (BYTE) (BASE_SEQ >> 8);
    twccPayload[9] = (BYTE) (BASE_SEQ & 0xFF);
    // Bytes 10-11: packet status count = OVERSIZED_COUNT (2049)
    twccPayload[10] = (BYTE) (OVERSIZED_COUNT >> 8);
    twccPayload[11] = (BYTE) (OVERSIZED_COUNT & 0xFF);
    // Bytes 12-14: reference time = 1
    twccPayload[14] = 0x01;
    // Byte 15: fb packet count
    twccPayload[15] = 0x01;
    // Bytes 16-17: run-length chunk (SMALLDELTA, count=OVERSIZED_COUNT)
    UINT16 runLenChunk = (0x01 << 13) | OVERSIZED_COUNT;
    twccPayload[16] = (BYTE) (runLenChunk >> 8);
    twccPayload[17] = (BYTE) (runLenChunk & 0xFF);
    // Delta bytes: 1 tick (250us) each
    for (UINT32 i = 0; i < OVERSIZED_COUNT; i++) {
        twccPayload[18 + i] = 0x01;
    }

    RtcpPacket rtcpPacket{};
    rtcpPacket.payload = twccPayload.data();
    rtcpPacket.payloadLength = payloadSize;

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));

    EXPECT_EQ(1u, ctx.callCount);
    // Key assertion: without the fix feedbackCount would be 2049, with it <= 2048
    EXPECT_LE(ctx.feedbackCount, (UINT32) TWCC_MAX_PACKET_STATUS_COUNT);

    SAFE_MEMFREE(ctx.pCapturedList);
    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}
  
static STATUS testTwccFeedbackCallbackNaN(UINT64 customData, PTwccFeedback pFeedbackList, UINT32 feedbackListLen,
                                          PTwccCongestionState pCongestionState)
{
    UNUSED_PARAM(pFeedbackList);
    UNUSED_PARAM(feedbackListLen);
    TwccCallbackCtx* pCtx = (TwccCallbackCtx*) customData;
    pCtx->callCount++;
    pCongestionState->delayTrend = pCtx->delayTrendToReturn;
    return STATUS_SUCCESS;
}

struct CongestionCallbackCtx {
    UINT32 callCount;
    DOUBLE lastDelayTrend;
};

static STATUS testCongestionFeedbackCapture(UINT64 customData, PCongestionCtx pCtx)
{
    CongestionCallbackCtx* pCbCtx = (CongestionCallbackCtx*) customData;
    pCbCtx->callCount++;
    pCbCtx->lastDelayTrend = pCtx->congestionState.delayTrend;
    return STATUS_SUCCESS;
}

TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_nanDelayTrendSanitized)
{
    // If a custom callback returns NaN for delayTrend, the SDK should sanitize it
    // to 0.0 rather than propagating undefined behavior to the congestion callback.
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;

    const std::string hex = "4487A9E754B3E6FD01810001147A75A62001C801";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    CongestionCallbackCtx congCtx{};
    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, (UINT64) &congCtx, testCongestionFeedbackCapture));

    TwccCallbackCtx twccCtx{};
    twccCtx.delayTrendToReturn = NAN;
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &twccCtx, testTwccFeedbackCallbackNaN));

    // Send packets so they exist in the hash table
    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(1u, twccCtx.callCount);

    // Process a second report so updateTwccHashTable computes duration > 0,
    // which triggers the congestion feedback callback.
    // Use a different base seq so the hash table reports a time window.
    const std::string hex2 = "4487A9E754B3E6FD01820001147A75A82001C801";
    payloadLen = 256;
    hexDecode(const_cast<PCHAR>(hex2.data()), hex2.size(), payload, &payloadLen);
    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    UINT16 baseSeqNum2 = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount2 = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum2; i < baseSeqNum2 + pktCount2; i++) {
        MEMSET(&rtpPacket, 0, SIZEOF(RtpPacket));
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(2u, twccCtx.callCount);

    // If congestion callback was invoked, verify delayTrend is finite
    if (congCtx.callCount > 0) {
        EXPECT_TRUE(isfinite(congCtx.lastDelayTrend));
        EXPECT_DOUBLE_EQ(0.0, congCtx.lastDelayTrend);
    }

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

TEST_F(RtcpFunctionalityTest, onTwccFeedbackReceived_infDelayTrendSanitized)
{
    // Verify INFINITY from custom callback is sanitized to 0.0
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    RtcConfiguration config{};
    RtcpPacket rtcpPacket{};
    RtpPacket rtpPacket{};
    BYTE payload[256] = {0};
    UINT32 payloadLen = 256;

    const std::string hex = "4487A9E754B3E6FD01810001147A75A62001C801";
    hexDecode(const_cast<PCHAR>(hex.data()), hex.size(), payload, &payloadLen);

    rtcpPacket.header.packetLength = payloadLen / 4;
    rtcpPacket.payload = payload;
    rtcpPacket.payloadLength = payloadLen;

    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);

    CongestionCallbackCtx congCtx{};
    EXPECT_EQ(STATUS_SUCCESS, setOnPeerCongestionFeedbackFn(pRtcPeerConnection, (UINT64) &congCtx, testCongestionFeedbackCapture));

    TwccCallbackCtx twccCtx{};
    twccCtx.delayTrendToReturn = INFINITY;
    EXPECT_EQ(STATUS_SUCCESS, setOnTwccFeedbackReceived(pRtcPeerConnection, (UINT64) &twccCtx, testTwccFeedbackCallbackNaN));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);
    for (UINT16 i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        UINT16 twsn = i;
        UINT32 extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        rtpPacket.payloadLength = 100;
        rtpPacket.payload = payload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    // Should succeed without UB - INFINITY is sanitized to 0.0
    EXPECT_EQ(STATUS_SUCCESS, onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
    EXPECT_EQ(1u, twccCtx.callCount);

    // If congestion callback was invoked, verify delayTrend is finite
    if (congCtx.callCount > 0) {
        EXPECT_TRUE(isfinite(congCtx.lastDelayTrend));
        EXPECT_DOUBLE_EQ(0.0, congCtx.lastDelayTrend);
    }

    EXPECT_EQ(STATUS_SUCCESS, freePeerConnection(&pRtcPeerConnection));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
