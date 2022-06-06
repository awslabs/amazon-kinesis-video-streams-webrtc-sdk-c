#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class JitterBufferFunctionalityTest : public WebRtcClientTestBase {
};

// Also works as closeBufferWithSingleContinousPacket
TEST_F(JitterBufferFunctionalityTest, continousPacketsComeInOrder)
{
    UINT32 i;
    UINT32 pktCount = 5;
    initializeJitterBuffer(3, 0, pktCount);

    // First frame "1" at timestamp 100 - rtp packet #0
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;

    // Expected to get frame "1"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    // Second frame "2" "34" at timestamp 200 - rtp packet #1 #2
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[2]->payloadLength = 2;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 3;
    mPRtpPackets[2]->payload[1] = 4;
    mPRtpPackets[2]->payload[2] = 0; // following packet of a frame
    mPRtpPackets[2]->header.timestamp = 200;

    // Expected to get frame "234"
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[1][0] = 2;
    mPExpectedFrameArr[1][1] = 3;
    mPExpectedFrameArr[1][2] = 4;
    mExpectedFrameSizeArr[1] = 3;

    // Third frame "56" "7" at timestamp 300 - rtp packet #3 #4
    mPRtpPackets[3]->payloadLength = 2;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 5;
    mPRtpPackets[3]->payload[1] = 6;
    mPRtpPackets[3]->payload[2] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 300;
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 7;
    mPRtpPackets[4]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[4]->header.timestamp = 300;

    // Expected to get frame "567" at close
    mPExpectedFrameArr[2] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[2][0] = 5;
    mPExpectedFrameArr[2][1] = 6;
    mPExpectedFrameArr[2][2] = 7;
    mExpectedFrameSizeArr[2] = 3;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 1:
            case 2:
                EXPECT_EQ(1, mReadyFrameIndex);
                break;
            case 3:
            case 4:
                EXPECT_EQ(2, mReadyFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, continousPacketsComeOutOfOrder)
{
    UINT32 i;
    UINT32 pktCount = 5;
    UINT32 startingSequenceNumber = 0;

    //seeding with the current time
    srand(time(0));
    startingSequenceNumber = rand()%UINT16_MAX;
    initializeJitterBuffer(3, 0, pktCount);

    DLOGI("Starting sequence number: %u\n", startingSequenceNumber);
    // First frame "1" at timestamp 100 - rtp packet #0
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = startingSequenceNumber++;

    // Expected to get frame "1"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    // Second frame "2" "34" at timestamp 200 - rtp packet #3 #1
    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 2;
    mPRtpPackets[3]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 200;
    mPRtpPackets[3]->header.sequenceNumber = startingSequenceNumber++;
    mPRtpPackets[1]->payloadLength = 2;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 3;
    mPRtpPackets[1]->payload[1] = 4;
    mPRtpPackets[1]->payload[2] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[1]->header.sequenceNumber = startingSequenceNumber++;

    // Expected to get frame "234"
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[1][0] = 2;
    mPExpectedFrameArr[1][1] = 3;
    mPExpectedFrameArr[1][2] = 4;
    mExpectedFrameSizeArr[1] = 3;

    // Third frame "56" "7" at timestamp 300 - rtp packet #2 #4
    mPRtpPackets[2]->payloadLength = 2;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 5;
    mPRtpPackets[2]->payload[1] = 6;
    mPRtpPackets[2]->payload[2] = 1; // First packet of a frame
    mPRtpPackets[2]->header.timestamp = 300;
    mPRtpPackets[2]->header.sequenceNumber = startingSequenceNumber++;
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 7;
    mPRtpPackets[4]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[4]->header.timestamp = 300;
    mPRtpPackets[4]->header.sequenceNumber = startingSequenceNumber++;

    // Expected to get frame "567" at close
    mPExpectedFrameArr[2] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[2][0] = 5;
    mPExpectedFrameArr[2][1] = 6;
    mPExpectedFrameArr[2][2] = 7;
    mExpectedFrameSizeArr[2] = 3;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
            case 1:
            case 2:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 3:
            case 4:
                EXPECT_EQ(2, mReadyFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }

    clearJitterBufferForTest();
}

// This also serves as closeBufferWithMultipleImcompletePackets
TEST_F(JitterBufferFunctionalityTest, gapBetweenTwoContinousPackets)
{
    UINT32 i;
    UINT32 pktCount = 4;
    initializeJitterBuffer(1, 2, pktCount);

    // First frame "1" "2" "3" at timestamp 100 - rtp packet #0 #1 #2, not receiving #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 3;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;
    mPRtpPackets[1]->header.sequenceNumber = 2;

    // Second frame "4" at timestamp 200 - rtp packet #3
    mPRtpPackets[2]->payloadLength = 1;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 4;
    mPRtpPackets[2]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[2]->header.timestamp = 200;
    mPRtpPackets[2]->header.sequenceNumber = 3;

    // Third frame "5" at timestamp 300 - rtp packet #4, not receiving #4

    // Fourth frame "6" at timestamp 400 - rtp packet #5
    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 6;
    mPRtpPackets[3]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 400;
    mPRtpPackets[3]->header.sequenceNumber = 5;

    // Expected to dropped frames when "2" "5" is not received
    mExpectedDroppedFrameTimestampArr[0] = 100;
    mExpectedDroppedFrameTimestampArr[1] = 200;

    // Expected to get frame "6"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 6;
    mExpectedFrameSizeArr[0] = 1;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        // packet "2" "5" is not received
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        EXPECT_EQ(0, mDroppedFrameIndex);
        EXPECT_EQ(0, mReadyFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, expiredCompleteFrameGotReadyFunc)
{
    UINT32 i;
    UINT32 pktCount = 2;
    initializeJitterBuffer(2, 0, pktCount);

    // First frame "1" at timestamp 100 - rtp packet #0
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;

    // Expected to get frame "1"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    // Second frame "2" at timestamp 3200 - rtp packet #1
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 3200;

    // Expected to get frame "2"
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[1][0] = 2;
    mExpectedFrameSizeArr[1] = 1;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 1:
                EXPECT_EQ(1, mReadyFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }
    clearJitterBufferForTest();
}

// This also serves as closeBufferWithImcompletePacketsAndSingleContinousPacket
TEST_F(JitterBufferFunctionalityTest, expiredIncompleteFrameGotDropFunc)
{
    UINT32 i;
    UINT32 pktCount = 2;
    initializeJitterBuffer(1, 1, pktCount);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1, not receiving #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;

    // Expected to dropped frame when "2" is not received
    mExpectedDroppedFrameTimestampArr[0] = 100;

    // Second frame "3" at timestamp 3200 - rtp packet #2
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 3;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 3200;
    mPRtpPackets[1]->header.sequenceNumber = 2;

    // Expected to get frame "3"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 3;
    mExpectedFrameSizeArr[0] = 1;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        // packet "2" is not received
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 1:
                EXPECT_EQ(1, mDroppedFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mReadyFrameIndex);
    }
    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, closeBufferWithSingleImcompletePacket)
{
    UINT32 i;
    UINT32 pktCount = 2;
    initializeJitterBuffer(0, 1, pktCount);

    // First frame "1" "2" "3" at timestamp 100 - rtp packet #0 #1 #2, not receiving #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 3;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;
    mPRtpPackets[1]->header.sequenceNumber = 2;

    // Expected to dropped frame when "2" is not received
    mExpectedDroppedFrameTimestampArr[0] = 100;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        // packet "2" is not received
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        EXPECT_EQ(0, mDroppedFrameIndex);
        EXPECT_EQ(0, mReadyFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, fillDataGiveExpectedData)
{
    PBYTE buffer = (PBYTE) MEMALLOC(2);
    UINT32 filledSize = 0, i = 0;
    BYTE expectedBuffer[] = {1, 2};
    initializeJitterBuffer(1, 0, 2);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;

    // Expected to get frame "12"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(2);
    mPExpectedFrameArr[0][0] = 1;
    mPExpectedFrameArr[0][1] = 2;
    mExpectedFrameSizeArr[0] = 2;

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
    }

    EXPECT_EQ(STATUS_SUCCESS, jitterBufferFillFrameData(mJitterBuffer, buffer, 2, &filledSize, 0, 1));
    EXPECT_EQ(2, filledSize);
    EXPECT_EQ(0, MEMCMP(buffer, expectedBuffer, 2));

    clearJitterBufferForTest();
    MEMFREE(buffer);
}

TEST_F(JitterBufferFunctionalityTest, fillDataReturnErrorWithImcompleteFrame)
{
    PBYTE buffer = (PBYTE) MEMALLOC(2);
    UINT32 filledSize = 0, i = 0;
    initializeJitterBuffer(0, 1, 2);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #2
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;
    mPRtpPackets[1]->header.sequenceNumber = 2;

    // Expected to drop frame for timestamp 100
    mExpectedDroppedFrameTimestampArr[0] = 100;

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
    }

    EXPECT_EQ(STATUS_NULL_ARG, jitterBufferFillFrameData(mJitterBuffer, buffer, 2, &filledSize, 0, 1));

    clearJitterBufferForTest();
    MEMFREE(buffer);
}

TEST_F(JitterBufferFunctionalityTest, fillDataReturnErrorWithNotEnoughOutputBuffer)
{
    PBYTE buffer = (PBYTE) MEMALLOC(1);
    UINT32 filledSize = 0, i = 0;
    initializeJitterBuffer(1, 0, 2);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;

    // Expected to get frame "12"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(2);
    mPExpectedFrameArr[0][0] = 1;
    mPExpectedFrameArr[0][1] = 2;
    mExpectedFrameSizeArr[0] = 2;

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
    }

    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, jitterBufferFillFrameData(mJitterBuffer, buffer, 1, &filledSize, 0, 1));

    clearJitterBufferForTest();
    MEMFREE(buffer);
}

TEST_F(JitterBufferFunctionalityTest, dropDataGivenSmallStartAndLargeEnd)
{
    UINT32 i = 0;
    initializeJitterBuffer(0, 1, 2);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;

    // Expected to drop frame for timestamp 100
    mExpectedDroppedFrameTimestampArr[0] = 100;

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
    }

    EXPECT_EQ(STATUS_SUCCESS, jitterBufferDropBufferData(mJitterBuffer, 1, 2, 100));

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, dropDataGivenLargeStartAndSmallEnd)
{
    UINT32 i = 0;
    initializeJitterBuffer(0, 0, 2);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[1]->payloadLength = 2;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[2] = 0; // Following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
    }

    // Directly drops all frames 10 - 65535 and 0 - 2, so no frame will be reported as ready/dropped callback
    EXPECT_EQ(STATUS_SUCCESS, jitterBufferDropBufferData(mJitterBuffer, 10, 2, 100));

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, continousPacketsComeInCycling)
{
    UINT32 i;
    UINT32 pktCount = 4;
    initializeJitterBuffer(4, 0, pktCount);

    // First frame "1" at timestamp 100 - rtp packet #65534
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 65534;

    // Expected to get frame "1"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    // Second frame "2" at timestamp 200 - rtp packet #65535
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[1]->header.sequenceNumber = 65535;

    // Expected to get frame "2"
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[1][0] = 2;
    mExpectedFrameSizeArr[1] = 1;

    // Third frame "3" at timestamp 300 - rtp packet #0
    mPRtpPackets[2]->payloadLength = 1;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 3;
    mPRtpPackets[2]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[2]->header.timestamp = 300;
    mPRtpPackets[2]->header.sequenceNumber = 0;

    // Expected to get frame "3" at close
    mPExpectedFrameArr[2] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[2][0] = 3;
    mExpectedFrameSizeArr[2] = 1;

    // Third frame "4" at timestamp 400 - rtp packet #1
    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 4;
    mPRtpPackets[3]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 400;
    mPRtpPackets[3]->header.sequenceNumber = 1;

    // Expected to get frame "4" at close
    mPExpectedFrameArr[3] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[3][0] = 4;
    mExpectedFrameSizeArr[3] = 1;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 1:
                EXPECT_EQ(1, mReadyFrameIndex);
                break;
            case 2:
                EXPECT_EQ(2, mReadyFrameIndex);
                break;
            case 3:
                EXPECT_EQ(3, mReadyFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, getFrameReadyAfterDroppedFrame)
{
    UINT32 i = 0;
    initializeJitterBuffer(3, 1, 5);

    // First frame "1" "2" at timestamp 100 - rtp packet #0 #1, dropped #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;

    // Expected to drop frame for timestamp 100
    mExpectedDroppedFrameTimestampArr[0] = 100;

    // Second frame "3" "4" at timestamp 200 - rtp packet #3 #4
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 3;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[1]->header.sequenceNumber = 3;
    mPRtpPackets[2]->payloadLength = 1;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 4;
    mPRtpPackets[2]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[2]->header.timestamp = 200;
    mPRtpPackets[2]->header.sequenceNumber = 4;

    // Expected to get frame "34" at timestamp 200
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(2);
    mPExpectedFrameArr[0][0] = 3;
    mPExpectedFrameArr[0][1] = 4;
    mExpectedFrameSizeArr[0] = 2;

    // Second frame "5" at timestamp 300 - rtp packet #5
    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 5;
    mPRtpPackets[3]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 300;
    mPRtpPackets[3]->header.sequenceNumber = 5;

    // Expected to get frame "5" at timestamp 300
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[1][0] = 5;
    mExpectedFrameSizeArr[1] = 1;

    // Second frame "6" at timestamp 3000 - rtp packet #6
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 6;
    mPRtpPackets[4]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[4]->header.timestamp = 3000;
    mPRtpPackets[4]->header.sequenceNumber = 6;

    // Expected to get frame "6" at close
    mPExpectedFrameArr[2] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[2][0] = 6;
    mExpectedFrameSizeArr[2] = 1;

    setPayloadToFree();

    for (i = 0; i < 5; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 1:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 2:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 3:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 4:
                EXPECT_EQ(2, mReadyFrameIndex);
                EXPECT_EQ(1, mDroppedFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, pushFrameArrivingLate)
{
    UINT32 i = 0;
    initializeJitterBuffer(1, 0, 2);

    // First frame "1" at timestamp 3000 - rtp packet #1
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 3000;
    mPRtpPackets[0]->header.sequenceNumber = 1;

    // Expected to get frame "1" at timestamp 3000
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    // Second frame "0" at timestamp 200 - rtp packet #0
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 0;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[1]->header.sequenceNumber = 0;

    // No drop frame/frame ready for second frame as it is not pushed into jitter buffer but should get freed

    setPayloadToFree();

    for (i = 0; i < 2; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 1:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, missingSecondPacketInSecondFrame)
{
    UINT32 i;
    UINT32 pktCount = 7;
    initializeJitterBuffer(2, 1, pktCount);

    // First frame "1273" at timestamp 100 
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;

    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 0; // following packet
    mPRtpPackets[1]->header.timestamp = 100;
    mPRtpPackets[1]->header.sequenceNumber = 1;

    mPRtpPackets[2]->payloadLength = 1;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 7;
    mPRtpPackets[2]->payload[1] = 0; // following packet
    mPRtpPackets[2]->header.timestamp = 100;
    mPRtpPackets[2]->header.sequenceNumber = 2;

    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 3;
    mPRtpPackets[3]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[3]->header.timestamp = 100;
    mPRtpPackets[3]->header.sequenceNumber = 3;

    // Expected to get frame "1273"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(4);
    mPExpectedFrameArr[0][0] = 1;
    mPExpectedFrameArr[0][1] = 2;
    mPExpectedFrameArr[0][2] = 7;
    mPExpectedFrameArr[0][3] = 3;
    mExpectedFrameSizeArr[0] = 4;

    // Second frame "4?3" at timestamp 200 - missing contents of packet #5
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 4;
    mPRtpPackets[4]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[4]->header.timestamp = 200;
    mPRtpPackets[4]->header.sequenceNumber = 4;

    mPRtpPackets[5]->payloadLength = 1;
    mPRtpPackets[5]->payload = (PBYTE) MEMALLOC(mPRtpPackets[5]->payloadLength + 1);
    mPRtpPackets[5]->payload[0] = 3;
    mPRtpPackets[5]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[5]->header.timestamp = 200;
    mPRtpPackets[5]->header.sequenceNumber = 6;


    // Third frame "6" at timestamp 400 - rtp packet #7
    mPRtpPackets[6]->payloadLength = 1;
    mPRtpPackets[6]->payload = (PBYTE) MEMALLOC(mPRtpPackets[6]->payloadLength + 1);
    mPRtpPackets[6]->payload[0] = 6;
    mPRtpPackets[6]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[6]->header.timestamp = 400;
    mPRtpPackets[6]->header.sequenceNumber = 7;

    // Expected to dropped a frame when "5" is not received
    mExpectedDroppedFrameTimestampArr[0] = 200;

    // Expected to get frame "6"
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[1][0] = 6;
    mExpectedFrameSizeArr[1] = 1;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        // packet "2" "5" is not received
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
            case 1:
            case 2:
            case 3:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 4:
            case 5:
            case 6:
                EXPECT_EQ(1, mReadyFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }
    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, incompleteFirstFrame)
{
    UINT32 i;
    UINT32 pktCount = 5;
    initializeJitterBuffer(2, 1, pktCount);

    // First frame "1" at timestamp 100, has no start - rtp packet #0
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 0; // following packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;

    // Second frame "2" "34" at timestamp 200 - rtp packet #1 #2
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[2]->payloadLength = 2;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 3;
    mPRtpPackets[2]->payload[1] = 4;
    mPRtpPackets[2]->payload[2] = 0; // following packet of a frame
    mPRtpPackets[2]->header.timestamp = 200;

    // Expected to get frame "234"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[0][0] = 2;
    mPExpectedFrameArr[0][1] = 3;
    mPExpectedFrameArr[0][2] = 4;
    mExpectedFrameSizeArr[0] = 3;

    // Third frame "56" "7" at timestamp 300 - rtp packet #3 #4
    mPRtpPackets[3]->payloadLength = 2;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 5;
    mPRtpPackets[3]->payload[1] = 6;
    mPRtpPackets[3]->payload[2] = 1; // First packet of a frame
    mPRtpPackets[3]->header.timestamp = 300;
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 7;
    mPRtpPackets[4]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[4]->header.timestamp = 300;

    // Expected to get frame "567" at close
    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[1][0] = 5;
    mPExpectedFrameArr[1][1] = 6;
    mPExpectedFrameArr[1][2] = 7;
    mExpectedFrameSizeArr[1] = 3;

    // Expected to drop first frame when we clear the buffer and it is still incomplete
    mExpectedDroppedFrameTimestampArr[0] = 100;
    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        EXPECT_EQ(0, mReadyFrameIndex);
        EXPECT_EQ(0, mDroppedFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, outOfOrderFirstFrame)
{
    UINT32 i;
    UINT32 pktCount = 7;
    initializeJitterBuffer(3, 0, pktCount);

    // First frame "1" at timestamp 100, has no start - rtp packet #0
    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 1;
    mPRtpPackets[0]->payload[1] = 0; // following packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 1;

    // Second frame "2" "34" at timestamp 200 - rtp packet #1 #2
    mPRtpPackets[1]->payloadLength = 1;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 2;
    mPRtpPackets[1]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[1]->header.timestamp = 200;
    mPRtpPackets[1]->header.sequenceNumber = 3;
    mPRtpPackets[2]->payloadLength = 2;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 3;
    mPRtpPackets[2]->payload[1] = 4;
    mPRtpPackets[2]->payload[2] = 0; // following packet of a frame
    mPRtpPackets[2]->header.timestamp = 200;
    mPRtpPackets[2]->header.sequenceNumber = 4;

    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 9;
    mPRtpPackets[3]->payload[1] = 0; // following packet of a frame
    mPRtpPackets[3]->header.timestamp = 100;
    mPRtpPackets[3]->header.sequenceNumber = 2;
    mPRtpPackets[4]->payloadLength = 1;
    mPRtpPackets[4]->payload = (PBYTE) MEMALLOC(mPRtpPackets[4]->payloadLength + 1);
    mPRtpPackets[4]->payload[0] = 8;
    mPRtpPackets[4]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[4]->header.timestamp = 100;
    mPRtpPackets[4]->header.sequenceNumber = 0;

    // Expected to get frames "819" "234"
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[0][0] = 8;
    mPExpectedFrameArr[0][1] = 1;
    mPExpectedFrameArr[0][2] = 9;
    mExpectedFrameSizeArr[0] = 3;

    mPExpectedFrameArr[1] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[1][0] = 2;
    mPExpectedFrameArr[1][1] = 3;
    mPExpectedFrameArr[1][2] = 4;
    mExpectedFrameSizeArr[1] = 3;

    // Third frame "56" "7" at timestamp 300 - rtp packet #3 #4
    mPRtpPackets[5]->payloadLength = 2;
    mPRtpPackets[5]->payload = (PBYTE) MEMALLOC(mPRtpPackets[5]->payloadLength + 1);
    mPRtpPackets[5]->payload[0] = 5;
    mPRtpPackets[5]->payload[1] = 6;
    mPRtpPackets[5]->payload[2] = 1; // First packet of a frame
    mPRtpPackets[5]->header.timestamp = 300;
    mPRtpPackets[5]->header.sequenceNumber = 5;
    mPRtpPackets[6]->payloadLength = 1;
    mPRtpPackets[6]->payload = (PBYTE) MEMALLOC(mPRtpPackets[6]->payloadLength + 1);
    mPRtpPackets[6]->payload[0] = 7;
    mPRtpPackets[6]->payload[1] = 0; // Following packet of a frame
    mPRtpPackets[6]->header.timestamp = 300;
    mPRtpPackets[6]->header.sequenceNumber = 6;

    // Expected to get frame "567" at close
    mPExpectedFrameArr[2] = (PBYTE) MEMALLOC(3);
    mPExpectedFrameArr[2][0] = 5;
    mPExpectedFrameArr[2][1] = 6;
    mPExpectedFrameArr[2][2] = 7;
    mExpectedFrameSizeArr[2] = 3;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
            case 1:
            case 2:
            case 3:
                EXPECT_EQ(0, mReadyFrameIndex);
                break;
            case 4:
                EXPECT_EQ(1, mReadyFrameIndex);
                break;
            case 5:
                EXPECT_EQ(2, mReadyFrameIndex);
                break;
            case 6:
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
        EXPECT_EQ(0, mDroppedFrameIndex);
    }

    clearJitterBufferForTest();
}

TEST_F(JitterBufferFunctionalityTest, latePacketsOfAlreadyDroppedFrame)
{
    UINT32 i = 0;
    UINT32 pktCount = 4;
    initializeJitterBuffer(1, 1, pktCount);

    mPRtpPackets[0]->payloadLength = 1;
    mPRtpPackets[0]->payload = (PBYTE) MEMALLOC(mPRtpPackets[0]->payloadLength + 1);
    mPRtpPackets[0]->payload[0] = 0;
    mPRtpPackets[0]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[0]->header.timestamp = 100;
    mPRtpPackets[0]->header.sequenceNumber = 0;

    mPRtpPackets[1]->payloadLength = 4;
    mPRtpPackets[1]->payload = (PBYTE) MEMALLOC(mPRtpPackets[1]->payloadLength + 1);
    mPRtpPackets[1]->payload[0] = 1;
    mPRtpPackets[1]->payload[1] = 1;
    mPRtpPackets[1]->payload[2] = 1;
    mPRtpPackets[1]->payload[3] = 1;
    mPRtpPackets[1]->payload[4] = 0; // following packet of a frame
    mPRtpPackets[1]->header.timestamp = 100;
    mPRtpPackets[1]->header.sequenceNumber = 1;

    // Second frame "1" at timestamp 3000 - forces drop of earlier incomplete frame due to maxLatency being exceeded
    mPRtpPackets[2]->payloadLength = 1;
    mPRtpPackets[2]->payload = (PBYTE) MEMALLOC(mPRtpPackets[2]->payloadLength + 1);
    mPRtpPackets[2]->payload[0] = 1;
    mPRtpPackets[2]->payload[1] = 1; // First packet of a frame
    mPRtpPackets[2]->header.timestamp = 3000;
    mPRtpPackets[2]->header.sequenceNumber = 3;

    // Expected to get frame "1" at timestamp 3000
    mPExpectedFrameArr[0] = (PBYTE) MEMALLOC(1);
    mPExpectedFrameArr[0][0] = 1;
    mExpectedFrameSizeArr[0] = 1;

    mPRtpPackets[3]->payloadLength = 1;
    mPRtpPackets[3]->payload = (PBYTE) MEMALLOC(mPRtpPackets[3]->payloadLength + 1);
    mPRtpPackets[3]->payload[0] = 2;
    mPRtpPackets[3]->payload[1] = 0; // following packet of a frame
    mPRtpPackets[3]->header.timestamp = 100;
    mPRtpPackets[3]->header.sequenceNumber = 2;

    // Expected to drop first frame
    mExpectedDroppedFrameTimestampArr[0] = 100;

    setPayloadToFree();

    for (i = 0; i < pktCount; i++) {
        EXPECT_EQ(STATUS_SUCCESS, jitterBufferPush(mJitterBuffer, mPRtpPackets[i], nullptr));
        switch (i) {
            case 0:
            case 1:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(0, mDroppedFrameIndex);
                break;
            case 2:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(1, mDroppedFrameIndex);
                break;
            case 3:
                EXPECT_EQ(0, mReadyFrameIndex);
                EXPECT_EQ(1, mDroppedFrameIndex);
                break;
            default:
                ASSERT_TRUE(FALSE);
        }
    }

    clearJitterBufferForTest();
}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
