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

TEST_F(RtcpFunctionalityTest, onRtcpPacketCompoundNack)
{
    PRtpPacket pRtpPacket = nullptr;
    RtcOutboundRtpStreamStats stats{};
    BYTE validRtcpPacket[] = {0x81, 0xcd, 0x00, 0x03, 0x2c, 0xd1, 0xa0, 0xde, 0x00, 0x00, 0xab, 0xe0, 0x00, 0x00, 0x00, 0x00};
    initTransceiver(44000);
    EXPECT_EQ(createRtpRollingBuffer(DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS * DEFAULT_EXPECTED_VIDEO_BIT_RATE / 8 / DEFAULT_MTU_SIZE_BYTES,
                                     &pKvsRtpTransceiver->sender.packetBuffer), STATUS_SUCCESS);
    EXPECT_EQ(createRetransmitter(DEFAULT_SEQ_NUM_BUFFER_SIZE, DEFAULT_VALID_INDEX_BUFFER_SIZE, &pKvsRtpTransceiver->sender.retransmitter), STATUS_SUCCESS);
    EXPECT_EQ(createRtpPacketWithSeqNum(0, &pRtpPacket), STATUS_SUCCESS);

    EXPECT_EQ(rtpRollingBufferAddRtpPacket(pKvsRtpTransceiver->sender.packetBuffer, pRtpPacket), STATUS_SUCCESS);
    EXPECT_EQ(onRtcpPacket(pKvsPeerConnection, validRtcpPacket, SIZEOF(validRtcpPacket)), STATUS_SUCCESS);
    getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats);
    EXPECT_EQ(stats.nackCount, 1);
    EXPECT_EQ(stats.retransmittedPacketsSent, 1);
    EXPECT_EQ(stats.retransmittedBytesSent, 10);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
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

static void testBwHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt,
                                                   UINT64 duration) {
    UNUSED_PARAM(customData);
    UNUSED_PARAM(txBytes);
    UNUSED_PARAM(rxBytes);
    UNUSED_PARAM(txPacketsCnt);
    UNUSED_PARAM(rxPacketsCnt);
    UNUSED_PARAM(duration);
    return;
}
static void testPictureLossCb(UINT64 data) {
    *(PBOOL) data = TRUE;
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
    rtcpPacket.payloadLength = payloadLen;


    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(STATUS_SUCCESS, peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0,
                                                                        testBwHandler));

    UINT16 baseSeqNum = getUnalignedInt16BigEndian(rtcpPacket.payload + 8);
    UINT16 pktCount = TWCC_PACKET_STATUS_COUNT(rtcpPacket.payload);

    for(i = baseSeqNum; i < baseSeqNum + pktCount; i++) {
        rtpPacket.header.extension = TRUE;
        rtpPacket.header.extensionProfile = TWCC_EXT_PROFILE;
        rtpPacket.header.extensionLength = SIZEOF(UINT32);
        twsn = i;
        extpayload = TWCC_PAYLOAD(parseExtId(TWCC_EXT_URL), twsn);
        rtpPacket.header.extensionPayload = (PBYTE) &extpayload;
        EXPECT_EQ(STATUS_SUCCESS, twccManagerOnPacketSent(pKvsPeerConnection, &rtpPacket));
    }

    EXPECT_EQ(STATUS_SUCCESS, parseRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection->pTwccManager));

    for(i = 0; i < MAX_UINT16; i++) {
        if(hashTableGet(pKvsPeerConnection->pTwccManager->pTwccRtpPktInfosHashTable, i, &value) == STATUS_SUCCESS) {
            PTwccRtpPacketInfo tempTwccRtpPktInfo = (PTwccRtpPacketInfo) value;
            if(tempTwccRtpPktInfo->remoteTimeKvs == TWCC_PACKET_LOST_TIME) {
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
    parseTwcc("4487A9E754B3E6FD12740004148566AAC1402C00", 2, 2);
    parseTwcc("4487A9E754B3E6FD04FA0006147CAF88C554B80400000001", 5, 1);
    parseTwcc("4487A9E754B3E6FD00000002147972002002BC00", 2, 0);
    parseTwcc("4487A9E754B3E6FD06D40004147DDE41D6403C00FFEC0001", 4, 0);
    parseTwcc("4487A9E754B3E6FD04FA0006147CB089D95420FF9804000000000003", 6, 0);
    parseTwcc("4487A9E754B3E6FD000C000314797A052003E40004000003", 3, 0);
    parseTwcc("4487A9E754B3E6FD12740006148568ABD6648800FDA4000268000002", 6, 0);
    parseTwcc("4487A9E754B3E6FD1431000C14868C5A803CEC0028000002", 4, 8);
    parseTwcc("4487A9E754B3E6FD00020004147974012004140000000002", 4, 0);
    parseTwcc("4487A9E754B3E6FD12670008148560A8D66520016C00FD780402902800040002", 8, 0);
    parseTwcc("4487A9E754B3E6FD012E0005147A45872005900000000401", 5, 0);
    parseTwcc("4487A9E754B3E6FD01F20006147AC6D22006600004000000", 6, 0);
    parseTwcc("4487A9E754B3E6FD06690007147D9111200748000000040000000003", 7, 0);
    parseTwcc("4487A9E754B3E6FD020C0008147AD3D8200898000000000008000002", 8, 0);
    parseTwcc("4487A9E754B3E6FD07C20009147E7B8B200990000800000000000001", 9, 0);
    parseTwcc("4487A9E754B3E6FD0177000A147A74A5200A70000000000000040000", 10, 0);
    parseTwcc("4487A9E754B3E6FD1431000C14868E5B2008E540DC00000000000000FE10002800000003", 12, 0);
    parseTwcc("4487A9E754B3E6FD03C6000B147BEB6F200B3000380400000400040000000003", 11, 0);
    parseTwcc("4487A9E754B3E6FD02AB000D147B3013200D4800000004000000000000000401", 13, 0);
    parseTwcc("4487A9E754B3E6FD01BA000E147AA4C3200EA400000000000000000000000400", 14, 0);
    parseTwcc("4487A9E754B3E6FD0610000F147D62F3200FCC0000000000000400000000100000000003", 15, 0);
    parseTwcc("4487A9E754B3E6FD08120010147EAAA92010F80000000000000004040000000000000002", 16, 0);
    parseTwcc("4487A9E754B3E6FD05B80011147D33D52011F40014000000000000000000040000000001", 17, 0);
    parseTwcc("4487A9E754B3E6FD04DA001E147CAC86D556D999D6652009D40000000000EF840001040001DC0004D4000400031400", 30, 0);
    parseTwcc("4487A9E754B3E6FD11EA0012148514932012B40000000000000400000000000000000000", 18, 0);
    parseTwcc("4487A9E754B3E6FD09BC0013147FC45D201348000400000000000000000000000000000000000003", 19, 0);
    parseTwcc("4487A9E754B3E6FD05720014147D05B7201414000000000000100000000000040000000400000002", 20, 0);
    parseTwcc("4487A9E754B3E6FD03820015147BBD5A201554000000000000000000000000000000000400009801", 21, 0);
    parseTwcc("4487A9E754B3E6FD114B001B1484B87381FF200DE41000000000000000000000000000000000140000000002", 22, 5);
    parseTwcc("4487A9E754B3E6FD0B6700161480DD11201678000000000000000000040000000000000000000000", 22, 0);
    parseTwcc("4487A9E754B3E6FD07790017147E4E6F2017D400000000000400000000000000000004000400080000000003", 23, 0);
    parseTwcc("4487A9E754B3E6FD114B001D1484BB74D5592014E4008400000000FD60100000000000000000000000000000000014", 29, 0);
    parseTwcc("4487A9E754B3E6FD1230002914854FA22027E4002400000000000400000000000000040000000000040000001C0000", 41, 0);
    parseTwcc("4487A9E754B3E6FD04B60036147CAA852024C002D999D6407800000000000000000000000000040000000000000000", 48, 6);
    parseTwcc("4487A9E754B3E6FD040200E4147C9F81202700B7E6649000000000000000000004000000000008000018000000001", 45, 183);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketTwccReport)
{
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PKvsPeerConnection pKvsPeerConnection;
    RtcpPacket rtcpPacket;
    RtcConfiguration config{};
    auto hexpacket = (PCHAR) "8FCD00054487A9E754B3E6FD01810001147A75A62001C801";
    BYTE rawpacket[256] = {0};
    UINT32 rawpacketSize = 256;
    EXPECT_EQ(hexDecode(hexpacket, strlen(hexpacket), rawpacket, &rawpacketSize), STATUS_SUCCESS);

    EXPECT_EQ(onRtcpTwccPacket(NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(createPeerConnection(&config, &pRtcPeerConnection), STATUS_SUCCESS);
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    EXPECT_EQ(onRtcpTwccPacket(NULL, pKvsPeerConnection), STATUS_NULL_ARG);
    EXPECT_EQ(onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection), STATUS_SUCCESS); // Testing before setting callback
    EXPECT_EQ(peerConnectionOnSenderBandwidthEstimation(pRtcPeerConnection, 0, testBwHandler), STATUS_SUCCESS);
    EXPECT_EQ(onRtcpPacket(pKvsPeerConnection, rawpacket, rawpacketSize), STATUS_SUCCESS);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketFirReport)
{
    RtcOutboundRtpStreamStats stats{};
    auto hexpacket = (PCHAR) "80C000014487A9E7";
    BYTE rawpacket[256] = {0};
    UINT32 rawpacketSize = 256;
    BOOL onPictureLossCalled = FALSE;


    EXPECT_EQ(hexDecode(hexpacket, strlen(hexpacket), rawpacket, &rawpacketSize), STATUS_SUCCESS);
    initTransceiver(0x4487A9E7);
    pKvsRtpTransceiver->onPictureLossCustomData = (UINT64) &onPictureLossCalled;
    pKvsRtpTransceiver->onPictureLoss = [](UINT64 customData) -> void { *(PBOOL) customData = TRUE; };

    EXPECT_EQ(onRtcpPacket(pKvsPeerConnection, rawpacket, rawpacketSize), STATUS_SUCCESS);
    EXPECT_EQ(getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats), STATUS_SUCCESS);
    EXPECT_EQ(stats.firCount, 1);
    EXPECT_TRUE(onPictureLossCalled);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketSliReport)
{
    RtcOutboundRtpStreamStats stats{};
    auto hexpacket = (PCHAR) "82CE00014487A9E7";
    BYTE rawpacket[256] = {0};
    UINT32 rawpacketSize = 256;

    EXPECT_EQ(hexDecode(hexpacket, strlen(hexpacket), rawpacket, &rawpacketSize), STATUS_SUCCESS);
    initTransceiver(0x4487A9E7);

    EXPECT_EQ(onRtcpSLIPacket(NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(onRtcpSLIPacket(NULL, pKvsPeerConnection), STATUS_NULL_ARG);
    EXPECT_EQ(onRtcpPacket(pKvsPeerConnection, rawpacket, rawpacketSize), STATUS_SUCCESS);
    EXPECT_EQ(getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats), STATUS_SUCCESS);
    EXPECT_EQ(stats.sliCount, 1);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
}

TEST_F(RtcpFunctionalityTest, onRtcpPacketPliReport)
{
    RtcOutboundRtpStreamStats stats{};
    auto hexpacket = (PCHAR) "81CE00014487A9E7";
    BYTE rawpacket[256] = {0};
    UINT32 rawpacketSize = 256;
    BOOL onPictureLossCalled = FALSE;

    EXPECT_EQ(hexDecode(hexpacket, strlen(hexpacket), rawpacket, &rawpacketSize), STATUS_SUCCESS);
    initTransceiver(0x4487A9E7);

    EXPECT_EQ(transceiverOnPictureLoss(NULL, (UINT64) &onPictureLossCalled, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(transceiverOnPictureLoss(pRtcRtpTransceiver, (UINT64) &onPictureLossCalled, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(transceiverOnPictureLoss(NULL, (UINT64) &onPictureLossCalled, testPictureLossCb), STATUS_NULL_ARG);
    EXPECT_EQ(transceiverOnPictureLoss(pRtcRtpTransceiver, (UINT64) &onPictureLossCalled, testPictureLossCb), STATUS_SUCCESS);

    EXPECT_EQ(onRtcpPLIPacket(NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(onRtcpPLIPacket(NULL, pKvsPeerConnection), STATUS_NULL_ARG);
    EXPECT_EQ(onRtcpPacket(pKvsPeerConnection, rawpacket, rawpacketSize), STATUS_SUCCESS);
    EXPECT_EQ(getRtpOutboundStats(pRtcPeerConnection, nullptr, &stats), STATUS_SUCCESS);
    EXPECT_EQ(stats.pliCount, 1);
    EXPECT_TRUE(onPictureLossCalled);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
}

TEST_F(RtcpFunctionalityTest, testRollingBufferParams)
{
    RtcConfiguration config{};
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    pKvsPeerConnection = reinterpret_cast<PKvsPeerConnection>(pRtcPeerConnection);
    PRtcRtpTransceiver out = nullptr;
    RtcMediaStreamTrack trackEmptyRtcRtpTransceiverInitTrack{};
    RtcMediaStreamTrack trackInvalidRbTimeTrack{};
    RtcRtpTransceiverInit trackInvalidRbTimeInit{};
    RtcMediaStreamTrack trackInvalidRbBitrateTrack{};
    RtcRtpTransceiverInit trackInvalidRbBitrateInit{};
    RtcMediaStreamTrack trackInvalidRbBitrateRbTimeTrack{};
    RtcRtpTransceiverInit trackInvalidRbBitrateRbTimeInit{};
    RtcMediaStreamTrack trackValidRbTrack{};
    RtcRtpTransceiverInit trackValidRbInit{};
    RtcMediaStreamTrack trackInvalidMediaKindTrack{};
    RtcRtpTransceiverInit trackInvalidMediaKindInit{};

    trackEmptyRtcRtpTransceiverInitTrack.codec = RTC_CODEC_VP8;
    trackEmptyRtcRtpTransceiverInitTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackEmptyRtcRtpTransceiverInitTrack, nullptr, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);

    trackInvalidRbTimeTrack.codec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE;
    trackInvalidRbTimeTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    trackInvalidRbTimeInit.rollingBufferDurationSec = 0.01;
    trackInvalidRbTimeInit.rollingBufferBitratebps = 2 * 1024 * 1024;

    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackInvalidRbTimeTrack, &trackInvalidRbTimeInit, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, trackInvalidRbTimeInit.rollingBufferBitratebps);

    trackInvalidRbBitrateTrack.codec = RTC_CODEC_OPUS;
    trackInvalidRbBitrateTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    trackInvalidRbBitrateInit.rollingBufferDurationSec = 0.1;
    trackInvalidRbBitrateInit.rollingBufferBitratebps = 0.01 * 1024 * 1024;

    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackInvalidRbBitrateTrack, &trackInvalidRbBitrateInit, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, trackInvalidRbBitrateInit.rollingBufferDurationSec);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, DEFAULT_EXPECTED_AUDIO_BIT_RATE);

    trackInvalidRbBitrateRbTimeTrack.codec = RTC_CODEC_OPUS;
    trackInvalidRbBitrateRbTimeTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    trackInvalidRbBitrateRbTimeInit.rollingBufferDurationSec = 0.001;
    trackInvalidRbBitrateRbTimeInit.rollingBufferBitratebps = 0.01 * 1024 * 1024;

    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackInvalidRbBitrateRbTimeTrack, &trackInvalidRbBitrateRbTimeInit, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, DEFAULT_EXPECTED_AUDIO_BIT_RATE);

    trackValidRbTrack.codec = RTC_CODEC_OPUS;
    trackValidRbTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    trackValidRbInit.rollingBufferDurationSec = 10;
    trackValidRbInit.rollingBufferBitratebps = 10 * 1024 * 1024;

    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackValidRbTrack, &trackValidRbInit, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, trackValidRbInit.rollingBufferDurationSec);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, trackValidRbInit.rollingBufferBitratebps);

    trackInvalidMediaKindTrack.codec = RTC_CODEC_VP8;
    trackInvalidMediaKindTrack.kind = (MEDIA_STREAM_TRACK_KIND) 4;
    trackInvalidMediaKindInit.rollingBufferBitratebps = MIN_EXPECTED_BIT_RATE - 1;
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &trackInvalidMediaKindTrack, &trackInvalidMediaKindInit, &out));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(out);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
