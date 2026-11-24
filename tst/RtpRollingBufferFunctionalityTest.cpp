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
    UINT64 tailIndex = 0, headIndex = 0;
    auto lastIndex = [&]() -> UINT64 {
        EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetIndexRange(pRtpRollingBuffer->pRollingBuffer, &tailIndex, &headIndex));
        return headIndex - 1;
    };

    EXPECT_EQ(STATUS_SUCCESS, createRtpRollingBuffer(2, &pRtpRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, createRtpPacketWithSeqNum(0, &pRtpPacket));
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(0, lastIndex());
    updateRtpPacketSeqNum(pRtpPacket, 1);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(1, lastIndex());
    updateRtpPacketSeqNum(pRtpPacket, 2);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(2, lastIndex());
    updateRtpPacketSeqNum(pRtpPacket, 3);
    EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket));
    EXPECT_EQ(3, lastIndex());
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
    EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pRtpPacket));
}

// writeFrame() appends on the media thread while resendPacketOnNack() looks packets up on the receive thread.
// Every index returned for a NACKed sequence number must hold that sequence number's packet. Reading the
// buffer range without a consistent snapshot maps some lookups to the next packet instead.
TEST_F(RtpRollingBufferFunctionalityTest, getValidSeqIndexListWhileAppendingMapsToRequestedPackets)
{
    const UINT32 capacity = 512, seqCount = 32;
    PRtpRollingBuffer pRtpRollingBuffer = NULL;
    std::atomic<UINT64> appended(0);
    std::atomic<bool> stop(false);
    UINT16 seqList[seqCount];
    UINT64 indexList[seqCount], item, newest;
    UINT32 filledIndexListLen, evicted, i;
    UINT64 checked = 0, mismatches = 0, endTime;

    ASSERT_EQ(STATUS_SUCCESS, createRtpRollingBuffer(capacity, &pRtpRollingBuffer));

    std::thread writer([&]() {
        PRtpPacket pRtpPacket = NULL;
        for (UINT64 seq = 0; !stop.load(std::memory_order_relaxed); seq++) {
            if (STATUS_SUCCESS != createRtpPacketWithSeqNum(GET_UINT16_SEQ_NUM(seq), &pRtpPacket)) {
                break;
            }
            rtpRollingBufferAddRtpPacket(pRtpRollingBuffer, pRtpPacket);
            freeRtpPacket(&pRtpPacket);
            // Relaxed on purpose: the test must not add ordering the SDK code does not have.
            appended.store(seq + 1, std::memory_order_relaxed);
        }
    });

    endTime = GETTIME() + HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (GETTIME() < endTime) {
        newest = appended.load(std::memory_order_relaxed);
        if (newest < capacity + seqCount) {
            THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            continue;
        }
        // Ask for packets well inside the window, as a receiver NACKs losses about one RTT old.
        for (i = 0; i < seqCount; i++) {
            seqList[i] = GET_UINT16_SEQ_NUM(newest - capacity / 2 + i);
        }
        filledIndexListLen = seqCount;
        EXPECT_EQ(STATUS_SUCCESS, rtpRollingBufferGetValidSeqIndexList(pRtpRollingBuffer, seqList, seqCount, indexList, &filledIndexListLen));
        EXPECT_LE(filledIndexListLen, seqCount);
        if (filledIndexListLen > seqCount) {
            break; // not ASSERT: the writer thread must still be joined
        }
        // If this thread was descheduled, the writer may have evicted some requested packets. Those are the
        // oldest, the first ones in seqList, so the returned indices line up with the end of the list.
        evicted = seqCount - filledIndexListLen;
        for (i = 0; i < filledIndexListLen; i++) {
            EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRtpRollingBuffer->pRollingBuffer, indexList[i], &item));
            if (item == 0) {
                continue;
            }
            checked++;
            if (((PRtpPacket) item)->header.sequenceNumber != seqList[evicted + i]) {
                mismatches++;
            }
            if (STATUS_ROLLING_BUFFER_NOT_IN_RANGE == rollingBufferInsertData(pRtpRollingBuffer->pRollingBuffer, indexList[i], item)) {
                freeRtpPacket((PRtpPacket*) &item);
            }
        }
    }

    stop = true;
    writer.join();

    EXPECT_GT(checked, 0);
    EXPECT_EQ(0, mismatches) << mismatches << " of " << checked << " lookups returned another packet";
    EXPECT_EQ(STATUS_SUCCESS, freeRtpRollingBuffer(&pRtpRollingBuffer));
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
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, 0, 0), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with invalid value for rolling buffer duration and valid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, 0.001, 4 * 1024 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, 4 * 1024 * 1024);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with valid value for rolling buffer duration and invalid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, 0.2, 102.39 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, 0.2);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with invalid value for rolling buffer duration and invalid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, 0.0000001, 102.39 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, DEFAULT_EXPECTED_VIDEO_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);


    // Testing with max value for rolling buffer duration and max rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, MAX_ROLLING_BUFFER_DURATION_IN_SECONDS, MAX_EXPECTED_BIT_RATE), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, MAX_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, MAX_EXPECTED_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with max value for rolling buffer duration and min rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, MAX_ROLLING_BUFFER_DURATION_IN_SECONDS, MIN_EXPECTED_BIT_RATE), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, MAX_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, MIN_EXPECTED_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);


    // Testing with min value for rolling buffer duration and max rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, MIN_ROLLING_BUFFER_DURATION_IN_SECONDS, MAX_EXPECTED_BIT_RATE), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, MIN_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, MAX_EXPECTED_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with min value for rolling buffer duration and min rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, MIN_ROLLING_BUFFER_DURATION_IN_SECONDS, MIN_EXPECTED_BIT_RATE), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, MIN_ROLLING_BUFFER_DURATION_IN_SECONDS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, MIN_EXPECTED_BIT_RATE);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

    // Testing with valid value for rolling buffer duration and valid rolling buffer bitrate
    EXPECT_EQ(STATUS_SUCCESS, createPeerConnection(&config, &pRtcPeerConnection));
    EXPECT_EQ(STATUS_SUCCESS, ::addTransceiver(pRtcPeerConnection, &videoTrack, nullptr, &pRtcRtpTransceiver));
    pKvsRtpTransceiver = reinterpret_cast<PKvsRtpTransceiver>(pRtcRtpTransceiver);
    EXPECT_TRUE(pKvsRtpTransceiver->pRollingBufferConfig == nullptr);
    EXPECT_EQ(configureTransceiverRollingBuffer(pRtcRtpTransceiver, &videoTrack, 0.2, 9.2 * 1024 * 1024), STATUS_SUCCESS);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferDurationSec, 0.2);
    EXPECT_EQ(pKvsRtpTransceiver->pRollingBufferConfig->rollingBufferBitratebps, 9.2 * 1024 * 1024);
    EXPECT_EQ(freePeerConnection(&pRtcPeerConnection), STATUS_SUCCESS);

}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
