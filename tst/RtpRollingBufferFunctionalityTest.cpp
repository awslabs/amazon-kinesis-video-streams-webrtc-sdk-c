#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class RtpRollingBufferFunctionalityTest : public WebRtcClientTestBase {
};

VOID updateRtpPacketSeqNum(PRtpPacket pRtpPacket, UINT16 seqNum) {
    pRtpPacket->header.sequenceNumber = seqNum;
}

VOID pushConsecutiveRtpPacketsIntoBuffer(UINT32 packetCount, UINT32 bufferCapacity, PRtpRollingBuffer* ppRtpRollingBuffer, PRtpPacket* ppRtpPacket)
{
    PRtpPacket pRtpPacket;
    PRtpRollingBuffer pRtpRollingBuffer;
    UINT32 i;

    EXPECT_EQ(STATUS_SUCCESS, createRtpRollingBuffer(bufferCapacity, &pRtpRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, createRtpPacketWithSeqNum(0, &pRtpPacket));
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    for (i = 1; i < packetCount; i++) {
        updateRtpPacketSeqNum(pRtpPacket, GET_UINT16_SEQ_NUM(i));
        EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    }

    *ppRtpPacket = pRtpPacket;
    *ppRtpRollingBuffer = pRtpRollingBuffer;
}

TEST_F(RtpRollingBufferFunctionalityTest, appendDataToBufferAndVerify)
{
    PRtpRollingBuffer pRtpRollingBuffer;
    PRtpPacket pRtpPacket;

    EXPECT_EQ(STATUS_SUCCESS, createRtpRollingBuffer(2, &pRtpRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, createRtpPacketWithSeqNum(0, &pRtpPacket));
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(0, pRtpRollingBuffer->lastIndex);
    updateRtpPacketSeqNum(pRtpPacket, 1);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(1, pRtpRollingBuffer->lastIndex);
    updateRtpPacketSeqNum(pRtpPacket, 2);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(2, pRtpRollingBuffer->lastIndex);
    updateRtpPacketSeqNum(pRtpPacket, 3);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(3, pRtpRollingBuffer->lastIndex);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

TEST_F(RtpRollingBufferFunctionalityTest, getIndexForSeqListReturnEmptyList)
{
    PRtpRollingBuffer pRtpRollingBuffer;
    PRtpPacket pRtpPacket = NULL;
    UINT16 seqList[] = {0, 1, 2, 6, 7, 8, 10000, 65535};
    UINT32 seqListLen = ARRAY_SIZE(seqList);
    PUINT64 indexList = (PUINT64) MEMALLOC(SIZEOF(UINT64) * seqListLen);
    UINT32 filledIndexListLen = seqListLen;

    // add 0 1 2 3 4 5, capacity is 3, 0 1 2 are out of rolling buffer, 3 4 5 are in
    pushConsecutiveRtpPacketsIntoBuffer(6, 3, &pRtpRollingBuffer, &pRtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqListLen, indexList, &filledIndexListLen));
    EXPECT_EQ(0, filledIndexListLen);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    SAFE_MEMFREE(indexList);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

TEST_F(RtpRollingBufferFunctionalityTest, getIndexForSeqListReturnCorrectIndexs)
{
    PRtpRollingBuffer pRtpRollingBuffer;
    PRtpPacket pRtpPacket = NULL;
    UINT16 seqList[] = {0, 1, 2, 4, 5, 6, 7, 8, 10000, 65535};
    UINT32 seqListLen = ARRAY_SIZE(seqList);
    PUINT64 indexList = (PUINT64) MEMALLOC(SIZEOF(UINT64) * seqListLen);
    UINT32 filledIndexListLen = seqListLen;

    // add 0 1 2 3 4 5, capacity is 3, 0 1 2 are out of rolling buffer, 3 4 5 are in
    pushConsecutiveRtpPacketsIntoBuffer(6, 3, &pRtpRollingBuffer, &pRtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqListLen, indexList, &filledIndexListLen));
    EXPECT_EQ(2, filledIndexListLen);
    EXPECT_EQ(4, indexList[0]);
    EXPECT_EQ(5, indexList[1]);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    SAFE_MEMFREE(indexList);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

TEST_F(RtpRollingBufferFunctionalityTest, getIndexForSeqListReturnIndexsFitIntoSmallBuffer)
{
    PRtpRollingBuffer pRtpRollingBuffer;
    PRtpPacket pRtpPacket = NULL;
    UINT16 seqList[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 10000, 65535};
    UINT32 seqListLen = ARRAY_SIZE(seqList);
    PUINT64 indexList = (PUINT64) MEMALLOC(SIZEOF(UINT64) * 2);
    UINT32 filledIndexListLen = 2;

    // add 0 1 2 3 4 5, capacity is 3, 0 1 2 are out of rolling buffer, 3 4 5 are in
    pushConsecutiveRtpPacketsIntoBuffer(6, 3, &pRtpRollingBuffer, &pRtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqListLen, indexList, &filledIndexListLen));
    EXPECT_EQ(2, filledIndexListLen);
    EXPECT_EQ(3, indexList[0]);
    EXPECT_EQ(4, indexList[1]);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    SAFE_MEMFREE(indexList);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

TEST_F(RtpRollingBufferFunctionalityTest, getIndexForSeqListReturnCorrectIndexsWhenSeqNumGetOver65535)
{
    PRtpRollingBuffer pRtpRollingBuffer;
    PRtpPacket pRtpPacket = NULL;
    UINT16 seqList[] = {0, 1, 2, 3, 4, 5, 65532, 65533, 65534, 65535};
    UINT32 seqListLen = ARRAY_SIZE(seqList);
    PUINT64 indexList = (PUINT64) MEMALLOC(SIZEOF(UINT64) * 5);
    UINT32 filledIndexListLen = 5;

    // add 0 - 65538, capacity is 5, 65534 65535 0(65536) 1(65537) 2(65538) are in rolling buffer
    pushConsecutiveRtpPacketsIntoBuffer(65539, 5, &pRtpRollingBuffer, &pRtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqListLen, indexList, &filledIndexListLen));
    EXPECT_EQ(5, filledIndexListLen);
    EXPECT_EQ(65536, indexList[0]);
    EXPECT_EQ(65537, indexList[1]);
    EXPECT_EQ(65538, indexList[2]);
    EXPECT_EQ(65534, indexList[3]);
    EXPECT_EQ(65535, indexList[4]);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    SAFE_MEMFREE(indexList);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));

    indexList = (PUINT64) MEMALLOC(SIZEOF(UINT64) * 5);
    // add 0 - 131074(65538 + 65536), capacity is 5, 65534(131070) 65335(131071) 0(131072) 1(131073) 2(131074) are in rolling buffer
    pushConsecutiveRtpPacketsIntoBuffer(131075, 5, &pRtpRollingBuffer, &pRtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqListLen, indexList, &filledIndexListLen));
    EXPECT_EQ(5, filledIndexListLen);
    EXPECT_EQ(131072, indexList[0]);
    EXPECT_EQ(131073, indexList[1]);
    EXPECT_EQ(131074, indexList[2]);
    EXPECT_EQ(131070, indexList[3]);
    EXPECT_EQ(131071, indexList[4]);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    SAFE_MEMFREE(indexList);
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

TEST_F(RtpRollingBufferFunctionalityTest, testRollingBufferParams)
{
    RtcConfiguration config{};
    PKvsRtpTransceiver pKvsRtpTransceiver = nullptr;
    PKvsPeerConnection pKvsPeerConnection = nullptr;
    PRtcPeerConnection pRtcPeerConnection = nullptr;
    PRtcRtpTransceiver pRtcRtpTransceiver = nullptr;
    RtcMediaStreamTrack videoTrack{};

    videoTrack.codec = RTC_CODEC_VP8;
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;

    // Testing with 0,0
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(createRollingBufferConfig(pRtcRtpTransceiver, &videoTrack, 0, 0), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with invalid value for rolling buffer duration and valid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(createRollingBufferConfig(pRtcRtpTransceiver, &videoTrack, 0.001, 4 * 1024 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, 4 * 1024 * 1024);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with valid value for rolling buffer duration and invalid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(createRollingBufferConfig(pRtcRtpTransceiver, &videoTrack, 0.2, 102.39 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, 0.2);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with valid value for rolling buffer duration and valid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(createRollingBufferConfig(pRtcRtpTransceiver, &videoTrack, 0.2, 9.2 * 1024 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, 0.2);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, 9.2 * 1024 * 1024);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
