#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

#define NUMBER_OF_FRAME_FILES      403
#define NUMBER_OF_H265_FRAME_FILES 1500
#define DEFAULT_FPS_VALUE          25
BYTE start4ByteCode[] = {0x00, 0x00, 0x00, 0x01};

class RtpFunctionalityTest : public WebRtcClientTestBase {};

TEST_F(RtpFunctionalityTest, packetUnderflow)
{
    BYTE rawPacket[] = {0x00, 0x00, 0x00, 0x00};
    RtpPacket rtpPacket;

    MEMSET(&rtpPacket, 0x00, SIZEOF(RtpPacket));

    for (auto i = 0; i <= 12; i++) {
        ASSERT_EQ(setRtpPacketFromBytes(rawPacket, SIZEOF(rawPacket), &rtpPacket), STATUS_RTP_INPUT_PACKET_TOO_SMALL);
    }
}

TEST_F(RtpFunctionalityTest, marshallUnmarshallGettingSameData)
{
    BYTE payload[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    UINT32 payloadLen = 6;
    PayloadArray payloadArray;
    PRtpPacket packetList = NULL;
    PRtpPacket pRtpPacket = NULL;
    PRtpPacket pNewRtpPacket = NULL;
    PBYTE rawPacket = NULL;
    UINT32 packetLen = 0;
    UINT32 i = 0;

    payloadArray.payloadBuffer = payload;
    payloadArray.payloadLength = payloadLen;
    payloadArray.payloadSubLength = &payloadLen;
    payloadArray.payloadSubLenSize = 1;

    packetList = (PRtpPacket) MEMALLOC(SIZEOF(RtpPacket));

    SRAND(GETTIME());
    EXPECT_EQ(STATUS_SUCCESS,
              constructRtpPackets(&payloadArray, 8, 1, 1324857487, 0x1234ABCD, (PRtpPacket) packetList, payloadArray.payloadSubLenSize));

    EXPECT_TRUE(packetList != NULL);

    for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
        pRtpPacket = packetList + i;

        EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, NULL, &packetLen));
        EXPECT_TRUE(NULL != (rawPacket = (PBYTE) MEMALLOC(packetLen)));
        EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, rawPacket, &packetLen));
        EXPECT_EQ(STATUS_SUCCESS, createRtpPacketFromBytes(rawPacket, packetLen, &pNewRtpPacket));
        // Verify the extracted header is the same as original header
        EXPECT_EQ(pRtpPacket->header.version, pNewRtpPacket->header.version);
        EXPECT_EQ(pRtpPacket->header.sequenceNumber, pNewRtpPacket->header.sequenceNumber);
        EXPECT_EQ(pRtpPacket->header.ssrc, pNewRtpPacket->header.ssrc);
        EXPECT_EQ(pRtpPacket->header.csrcArray, pNewRtpPacket->header.csrcArray);
        EXPECT_EQ(pRtpPacket->header.extensionPayload, pNewRtpPacket->header.extensionPayload);
        EXPECT_EQ(pRtpPacket->header.extension, pNewRtpPacket->header.extension);
        EXPECT_EQ(pRtpPacket->header.timestamp, pNewRtpPacket->header.timestamp);
        EXPECT_EQ(pRtpPacket->header.extensionProfile, pNewRtpPacket->header.extensionProfile);
        EXPECT_EQ(pRtpPacket->header.payloadType, pNewRtpPacket->header.payloadType);
        EXPECT_EQ(pRtpPacket->header.padding, pNewRtpPacket->header.padding);
        EXPECT_EQ(pRtpPacket->header.csrcCount, pNewRtpPacket->header.csrcCount);
        EXPECT_EQ(pRtpPacket->header.extensionLength, pNewRtpPacket->header.extensionLength);
        EXPECT_EQ(pRtpPacket->header.marker, pNewRtpPacket->header.marker);

        // Verify the extracted payload is the same as original payload
        EXPECT_EQ(pRtpPacket->payloadLength, pNewRtpPacket->payloadLength);
        EXPECT_TRUE(MEMCMP(pRtpPacket->payload, pNewRtpPacket->payload, pRtpPacket->payloadLength) == 0);

        EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pNewRtpPacket));
    }

    MEMFREE(packetList);
}

TEST_F(RtpFunctionalityTest, marshallUnmarshallH264Data)
{
    PBYTE payload = (PBYTE) MEMALLOC(200000); // Assuming this is enough
    UINT32 payloadLen = 0;
    PayloadArray payloadArray;
    PRtpPacket pPacketList = NULL;
    PRtpPacket pRtpPacket = NULL;
    PBYTE rawPacket = NULL;
    UINT32 packetLen = 0;
    UINT64 curTime = GETTIME();
    UINT32 fileIndex = 0;
    UINT16 seqNum = 0;
    UINT64 startTimeStamp = curTime;
    UINT32 i = 0;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    for (auto sentAllFrames = 0; sentAllFrames <= 5;) {
        if (fileIndex == NUMBER_OF_FRAME_FILES) {
            sentAllFrames++;
        }

        fileIndex = fileIndex % NUMBER_OF_FRAME_FILES + 1;
        EXPECT_EQ(STATUS_SUCCESS,
                  readFrameData((PBYTE) payload, (PUINT32) &payloadLen, fileIndex, (PCHAR) "../samples/h264SampleFrames",
                                RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));

        // First call for payload size and sub payload length size
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH264(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
                                       &payloadArray.payloadSubLenSize));

        if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
            if (payloadArray.payloadBuffer != NULL) {
                MEMFREE(payloadArray.payloadBuffer);
            }
            payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
            payloadArray.maxPayloadLength = payloadArray.payloadLength;
        }
        if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
            if (payloadArray.payloadSubLength != NULL) {
                MEMFREE(payloadArray.payloadSubLength);
            }
            payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
            payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
        }

        // Second call with actual buffer to fill in data
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH264(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                       payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

        EXPECT_LT(0, payloadArray.payloadSubLenSize);
        pPacketList = (PRtpPacket) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(RtpPacket));

        constructRtpPackets(&payloadArray, 96, seqNum, (UINT32) ((curTime - startTimeStamp) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND), 0x1234ABCD,
                            pPacketList, payloadArray.payloadSubLenSize);

        seqNum = GET_UINT16_SEQ_NUM(seqNum + payloadArray.payloadSubLenSize);

        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            pRtpPacket = pPacketList + i;
            EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, NULL, &packetLen));
            EXPECT_TRUE(NULL != (rawPacket = (PBYTE) MEMALLOC(packetLen)));
            EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, rawPacket, &packetLen));

            MEMFREE(rawPacket);
            rawPacket = NULL;
        }
        curTime = GETTIME();

        MEMFREE(pPacketList);

        pPacketList = NULL;
    }

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(payload);
}

TEST_F(RtpFunctionalityTest, packingUnpackingVerifySameH264Frame)
{
    PBYTE payload = (PBYTE) MEMCALLOC(1, 200000); // Assuming this is enough
    PBYTE depayload = (PBYTE) MEMCALLOC(1, 1500); // This is more than max mtu
    UINT32 depayloadSize = 1500;
    UINT32 payloadLen = 0;
    UINT32 fileIndex = 0;
    PayloadArray payloadArray;
    UINT32 i = 0;
    UINT32 offset = 0;
    UINT32 newPayloadLen = 0, newPayloadSubLen = 0;
    BOOL isStartPacket = FALSE;
    PBYTE pCurPtrInPayload = NULL;
    UINT32 remainPayloadLen = 0;
    UINT32 startIndex = 0, naluLength = 0;
    UINT32 startLen = 0;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    for (fileIndex = 1; fileIndex <= NUMBER_OF_FRAME_FILES; fileIndex++) {
        EXPECT_EQ(STATUS_SUCCESS,
                  readFrameData((PBYTE) payload, (PUINT32) &payloadLen, fileIndex, (PCHAR) "../samples/h264SampleFrames",
                                RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE));

        // First call for payload size and sub payload length size
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH264(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
                                       &payloadArray.payloadSubLenSize));

        if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
            if (payloadArray.payloadBuffer != NULL) {
                MEMFREE(payloadArray.payloadBuffer);
            }
            payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
            payloadArray.maxPayloadLength = payloadArray.payloadLength;
        }
        if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
            if (payloadArray.payloadSubLength != NULL) {
                MEMFREE(payloadArray.payloadSubLength);
            }
            payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
            payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
        }

        // Second call with actual buffer to fill in data
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH264(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                       payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

        EXPECT_LT(0, payloadArray.payloadSubLenSize);

        offset = 0;

        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            EXPECT_EQ(STATUS_SUCCESS,
                      depayH264FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], NULL, &newPayloadSubLen,
                                              &isStartPacket));
            newPayloadLen += newPayloadSubLen;
            if (isStartPacket) {
                newPayloadLen -= SIZEOF(start4ByteCode);
            }
            EXPECT_LT(0, newPayloadSubLen);
            offset += payloadArray.payloadSubLength[i];
        }
        EXPECT_LE(newPayloadLen, payloadLen);

        offset = 0;
        newPayloadLen = 0;
        isStartPacket = FALSE;
        pCurPtrInPayload = payload;
        remainPayloadLen = payloadLen;
        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            newPayloadSubLen = depayloadSize;
            EXPECT_EQ(STATUS_SUCCESS,
                      depayH264FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], depayload, &newPayloadSubLen,
                                              &isStartPacket));
            if (isStartPacket) {
                EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(pCurPtrInPayload, remainPayloadLen, &startIndex, &naluLength));
                pCurPtrInPayload += startIndex;
                startLen = SIZEOF(start4ByteCode);
            } else {
                startLen = 0;
            }
            EXPECT_TRUE(MEMCMP(pCurPtrInPayload, depayload + startLen, newPayloadSubLen - startLen) == 0);
            pCurPtrInPayload += newPayloadSubLen - startLen;
            remainPayloadLen -= newPayloadSubLen;
            offset += payloadArray.payloadSubLength[i];
        }
    }

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(payload);
    MEMFREE(depayload);
}

TEST_F(RtpFunctionalityTest, packingUnpackingVerifySameH265Frame)
{
    PBYTE payload = (PBYTE) MEMCALLOC(1, 200000); // Assuming this is enough
    PBYTE depayload = (PBYTE) MEMCALLOC(1, 1500); // This is more than max mtu
    UINT32 depayloadSize = 1500;
    UINT32 payloadLen = 0;
    UINT32 fileIndex = 0;
    PayloadArray payloadArray;
    UINT32 i = 0;
    UINT32 offset = 0;
    UINT32 newPayloadLen = 0, newPayloadSubLen = 0;
    BOOL isStartPacket = FALSE;
    PBYTE pCurPtrInPayload = NULL;
    UINT32 remainPayloadLen = 0;
    UINT32 startIndex = 0, naluLength = 0;
    UINT32 startLen = 0;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    for (fileIndex = 1; fileIndex <= NUMBER_OF_H265_FRAME_FILES; fileIndex++) {
        EXPECT_EQ(STATUS_SUCCESS,
                  readFrameData((PBYTE) payload, (PUINT32) &payloadLen, fileIndex, (PCHAR) "../samples/h265SampleFrames", RTC_CODEC_H265));

        // First call for payload size and sub payload length size
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH265(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
                                       &payloadArray.payloadSubLenSize));

        if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
            if (payloadArray.payloadBuffer != NULL) {
                MEMFREE(payloadArray.payloadBuffer);
            }
            payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
            payloadArray.maxPayloadLength = payloadArray.payloadLength;
        }
        if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
            if (payloadArray.payloadSubLength != NULL) {
                MEMFREE(payloadArray.payloadSubLength);
            }
            payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
            payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
        }

        // Second call with actual buffer to fill in data
        EXPECT_EQ(STATUS_SUCCESS,
                  createPayloadForH265(DEFAULT_MTU_SIZE_BYTES, (PBYTE) payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                       payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

        EXPECT_LT(0, payloadArray.payloadSubLenSize);

        offset = 0;

        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            EXPECT_EQ(STATUS_SUCCESS,
                      depayH265FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], NULL, &newPayloadSubLen,
                                              &isStartPacket));
            newPayloadLen += newPayloadSubLen;
            if (isStartPacket) {
                newPayloadLen -= SIZEOF(start4ByteCode);
            }
            EXPECT_LT(0, newPayloadSubLen);
            offset += payloadArray.payloadSubLength[i];
        }
        EXPECT_LE(newPayloadLen, payloadLen);

        offset = 0;
        newPayloadLen = 0;
        isStartPacket = FALSE;
        pCurPtrInPayload = payload;
        remainPayloadLen = payloadLen;
        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            newPayloadSubLen = depayloadSize;
            EXPECT_EQ(STATUS_SUCCESS,
                      depayH265FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], depayload, &newPayloadSubLen,
                                              &isStartPacket));
            if (isStartPacket) {
                EXPECT_EQ(STATUS_SUCCESS, getNextNaluLengthH265(pCurPtrInPayload, remainPayloadLen, &startIndex, &naluLength));
                pCurPtrInPayload += startIndex;
                startLen = SIZEOF(start4ByteCode);
            } else {
                startLen = 0;
            }
            EXPECT_TRUE(MEMCMP(pCurPtrInPayload, depayload + startLen, newPayloadSubLen - startLen) == 0);
            pCurPtrInPayload += newPayloadSubLen - startLen;
            remainPayloadLen -= newPayloadSubLen;
            offset += payloadArray.payloadSubLength[i];
        }
    }

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(payload);
    MEMFREE(depayload);
}

TEST_F(RtpFunctionalityTest, packingUnpackingVerifySameOpusFrame)
{
    BYTE payload[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    PBYTE depayload = (PBYTE) MEMALLOC(1500); // This is more than max mtu
    UINT32 depayloadSize = 1500;
    UINT32 payloadLen = 6;
    PayloadArray payloadArray;
    UINT32 newPayloadSubLen = 0;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    // First call for payload size and sub payload length size
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForOpus(DEFAULT_MTU_SIZE_BYTES, (PBYTE) &payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
                                   &payloadArray.payloadSubLenSize));

    if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
        if (payloadArray.payloadBuffer != NULL) {
            MEMFREE(payloadArray.payloadBuffer);
        }
        payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
        payloadArray.maxPayloadLength = payloadArray.payloadLength;
    }
    if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
        if (payloadArray.payloadSubLength != NULL) {
            MEMFREE(payloadArray.payloadSubLength);
        }
        payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
        payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
    }

    // Second call with actual buffer to fill in data
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForOpus(DEFAULT_MTU_SIZE_BYTES, (PBYTE) &payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                   payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

    EXPECT_EQ(1, payloadArray.payloadSubLenSize);
    EXPECT_EQ(6, payloadArray.payloadSubLength[0]);

    EXPECT_EQ(STATUS_SUCCESS, depayOpusFromRtpPayload(payloadArray.payloadBuffer, payloadArray.payloadSubLength[0], NULL, &newPayloadSubLen, NULL));
    EXPECT_EQ(6, newPayloadSubLen);

    newPayloadSubLen = depayloadSize;
    EXPECT_EQ(STATUS_SUCCESS,
              depayOpusFromRtpPayload(payloadArray.payloadBuffer, payloadArray.payloadSubLength[0], depayload, &newPayloadSubLen, NULL));
    EXPECT_TRUE(MEMCMP(payload, depayload, newPayloadSubLen) == 0);

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(depayload);
}

TEST_F(RtpFunctionalityTest, packingEmptyOpusFrameReturnsZeroSubLenSize)
{
    BYTE payload[] = {0x00};
    PayloadArray payloadArray;

    payloadArray.payloadLength = 0;
    payloadArray.payloadSubLenSize = 0;

    EXPECT_EQ(
        STATUS_SUCCESS,
        createPayloadForOpus(DEFAULT_MTU_SIZE_BYTES, (PBYTE) &payload, 0, NULL, &payloadArray.payloadLength, NULL, &payloadArray.payloadSubLenSize));
    EXPECT_EQ(0, payloadArray.payloadLength);
    EXPECT_EQ(0, payloadArray.payloadSubLenSize);
}

TEST_F(RtpFunctionalityTest, packingUnpackingVerifySameShortG711Frame)
{
    BYTE payload[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
    PBYTE depayload = (PBYTE) MEMALLOC(1500); // This is more than max mtu
    UINT32 depayloadSize = 1500;
    UINT32 payloadLen = 6;
    PayloadArray payloadArray;
    UINT32 newPayloadSubLen = 0;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    // First call for payload size and sub payload length size
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForG711(DEFAULT_MTU_SIZE_BYTES, (PBYTE) &payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
                                   &payloadArray.payloadSubLenSize));

    if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
        if (payloadArray.payloadBuffer != NULL) {
            MEMFREE(payloadArray.payloadBuffer);
        }
        payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
        payloadArray.maxPayloadLength = payloadArray.payloadLength;
    }
    if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
        if (payloadArray.payloadSubLength != NULL) {
            MEMFREE(payloadArray.payloadSubLength);
        }
        payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
        payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
    }

    // Second call with actual buffer to fill in data
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForG711(DEFAULT_MTU_SIZE_BYTES, (PBYTE) &payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                   payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

    EXPECT_EQ(1, payloadArray.payloadSubLenSize);
    EXPECT_EQ(6, payloadArray.payloadSubLength[0]);

    EXPECT_EQ(STATUS_SUCCESS, depayG711FromRtpPayload(payloadArray.payloadBuffer, payloadArray.payloadSubLength[0], NULL, &newPayloadSubLen, NULL));
    EXPECT_EQ(6, newPayloadSubLen);

    newPayloadSubLen = depayloadSize;
    EXPECT_EQ(STATUS_SUCCESS,
              depayG711FromRtpPayload(payloadArray.payloadBuffer, payloadArray.payloadSubLength[0], depayload, &newPayloadSubLen, NULL));
    EXPECT_TRUE(MEMCMP(payload, depayload, newPayloadSubLen) == 0);

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(depayload);
}

TEST_F(RtpFunctionalityTest, packingUnpackingVerifySameLongG711Frame)
{
    BYTE payload[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09};
    PBYTE depayload = (PBYTE) MEMALLOC(1500); // This is more than max mtu
    UINT32 depayloadSize = 1500;
    UINT32 payloadLen = 10;
    PayloadArray payloadArray;
    UINT32 i = 0;
    UINT32 newPayloadSubLen = 0;
    UINT32 newPayloadLen = 0;
    UINT32 offset = 0;
    PBYTE pCurPtrInPayload;

    payloadArray.maxPayloadLength = 0;
    payloadArray.maxPayloadSubLenSize = 0;
    payloadArray.payloadBuffer = NULL;
    payloadArray.payloadSubLength = NULL;

    // First call for payload size and sub payload length size
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForG711(4, (PBYTE) &payload, payloadLen, NULL, &payloadArray.payloadLength, NULL, &payloadArray.payloadSubLenSize));

    if (payloadArray.payloadLength > payloadArray.maxPayloadLength) {
        if (payloadArray.payloadBuffer != NULL) {
            MEMFREE(payloadArray.payloadBuffer);
        }
        payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
        payloadArray.maxPayloadLength = payloadArray.payloadLength;
    }
    if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize) {
        if (payloadArray.payloadSubLength != NULL) {
            MEMFREE(payloadArray.payloadSubLength);
        }
        payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
        payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
    }

    // Second call with actual buffer to fill in data
    EXPECT_EQ(STATUS_SUCCESS,
              createPayloadForG711(4, (PBYTE) &payload, payloadLen, payloadArray.payloadBuffer, &payloadArray.payloadLength,
                                   payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

    EXPECT_EQ(3, payloadArray.payloadSubLenSize);
    EXPECT_EQ(4, payloadArray.payloadSubLength[0]);
    EXPECT_EQ(4, payloadArray.payloadSubLength[1]);
    EXPECT_EQ(2, payloadArray.payloadSubLength[2]);

    offset = 0;

    for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
        EXPECT_EQ(STATUS_SUCCESS,
                  depayG711FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], NULL, &newPayloadSubLen, NULL));
        newPayloadLen += newPayloadSubLen;
        EXPECT_LT(0, newPayloadSubLen);
        offset += payloadArray.payloadSubLength[i];
    }
    EXPECT_EQ(newPayloadLen, payloadLen);

    offset = 0;
    pCurPtrInPayload = payload;
    for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
        newPayloadSubLen = depayloadSize;
        EXPECT_EQ(STATUS_SUCCESS,
                  depayG711FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], depayload, &newPayloadSubLen, NULL));
        EXPECT_TRUE(MEMCMP(pCurPtrInPayload, depayload, newPayloadSubLen) == 0);
        pCurPtrInPayload += newPayloadSubLen;
        offset += payloadArray.payloadSubLength[i];
    }

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(depayload);
}

TEST_F(RtpFunctionalityTest, invalidNaluParse)
{
    BYTE data[] = {0x01, 0x00, 0x02};
    BYTE data1[] = {0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02};
    UINT32 startIndex = 0, naluLength = 0;
    EXPECT_EQ(STATUS_RTP_INVALID_NALU, getNextNaluLength(data, 3, &startIndex, &naluLength));
    EXPECT_EQ(STATUS_RTP_INVALID_NALU, getNextNaluLength(data1, 7, &startIndex, &naluLength));

    EXPECT_EQ(STATUS_RTP_INVALID_NALU, getNextNaluLengthH265(data, 3, &startIndex, &naluLength));
    EXPECT_EQ(STATUS_RTP_INVALID_NALU, getNextNaluLengthH265(data1, 7, &startIndex, &naluLength));
}

TEST_F(RtpFunctionalityTest, validNaluParse)
{
    BYTE data[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x02};
    UINT32 startIndex = 0, naluLength = 0;

    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(data, 6, &startIndex, &naluLength));
    EXPECT_EQ(4, startIndex);
    EXPECT_EQ(2, naluLength);

    startIndex = 0;
    naluLength = 0;

    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLengthH265(data, 6, &startIndex, &naluLength));
    EXPECT_EQ(4, startIndex);
    EXPECT_EQ(2, naluLength);
}

TEST_F(RtpFunctionalityTest, validMultipleNaluParse)
{
    BYTE nalus[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x0b, 0x0c};
    UINT32 startIndex = 0, naluLength = 0;
    UINT32 nalusLength = 13;
    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(nalus, nalusLength, &startIndex, &naluLength));
    EXPECT_EQ(4, startIndex);
    EXPECT_EQ(2, naluLength);
    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(&nalus[startIndex + naluLength], nalusLength - startIndex - naluLength, &startIndex, &naluLength));
    EXPECT_EQ(4, startIndex);
    EXPECT_EQ(3, naluLength);
}

TEST_F(RtpFunctionalityTest, trailingZerosWouldBeReturned)
{
    BYTE nalus[] = {0x00, 0x00, 0x00, 0x01, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00};
    UINT32 startIndex = 0, naluLength = 0;
    UINT32 nalusLength = 11;
    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(nalus, nalusLength, &startIndex, &naluLength));
    EXPECT_EQ(4, startIndex);
    EXPECT_EQ(7, naluLength);
}

// https://tools.ietf.org/html/rfc3550#section-5.3.1
TEST_F(RtpFunctionalityTest, createPacketWithExtension)
{
    BYTE payload[10] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};
    BYTE extpayload[8] = {0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49};
    BYTE rawbytes[1024] = {0};
    PBYTE ptr = reinterpret_cast<PBYTE>(&rawbytes);
    PRtpPacket pRtpPacket = NULL;
    PRtpPacket pRtpPacket2 = NULL;

    EXPECT_EQ(STATUS_SUCCESS,
              createRtpPacket(2, FALSE, TRUE, 0, FALSE, 96, 42, 100, 0x1234ABCD, NULL, 0x4243, 8, extpayload, payload, 10, &pRtpPacket));
    EXPECT_EQ(STATUS_SUCCESS, setBytesFromRtpPacket(pRtpPacket, ptr, 1024));

    auto len = RTP_GET_RAW_PACKET_SIZE(pRtpPacket);
    EXPECT_EQ(STATUS_SUCCESS, createRtpPacketFromBytes(ptr, len, &pRtpPacket2));
    EXPECT_TRUE(pRtpPacket2->header.extension);
    EXPECT_EQ(0x4243, pRtpPacket2->header.extensionProfile);
    EXPECT_EQ(0x44, pRtpPacket2->header.extensionPayload[2]);
    EXPECT_EQ(0x15, pRtpPacket2->payload[5]);
    freeRtpPacket(&pRtpPacket);
    pRtpPacket2->pRawPacket = NULL;
    freeRtpPacket(&pRtpPacket2);
}

TEST_F(RtpFunctionalityTest, twccPayload)
{
    UINT32 extpayload = TWCC_PAYLOAD(4u, 420u);
    auto ptr = reinterpret_cast<PBYTE>(&extpayload);
    auto seqNum = TWCC_SEQNUM(ptr);
    EXPECT_EQ(4, (ptr[0] >> 4));
    EXPECT_EQ(1, (ptr[0] & 0xfu));
    EXPECT_EQ(420, seqNum);
    EXPECT_EQ(0, ptr[3]);
}

TEST_F(RtpFunctionalityTest, writeFrameNullArgs)
{
    Frame frame;
    RtcRtpTransceiver transceiver;

    MEMSET(&frame, 0x00, SIZEOF(Frame));
    MEMSET(&transceiver, 0x00, SIZEOF(RtcRtpTransceiver));

    EXPECT_EQ(STATUS_NULL_ARG, writeFrame(NULL, &frame));
    EXPECT_EQ(STATUS_NULL_ARG, writeFrame(&transceiver, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, writeFrame(NULL, NULL));
}

TEST_F(RtpFunctionalityTest, createPayloadForH264NullNalusWithNonNullBuffer)
{
    BYTE payloadBuffer[100];
    UINT32 payloadLength = 0xDEADBEEF;
    UINT32 payloadSubLenSize = 0xDEADBEEF;
    UINT32 payloadSubLength[10];

    // nalus is NULL but payloadBuffer is non-NULL (sizeCalculationOnly = FALSE).
    // Should return STATUS_NULL_ARG and zero out the output parameters.
    EXPECT_EQ(STATUS_NULL_ARG,
              createPayloadForH264(DEFAULT_MTU_SIZE_BYTES, NULL, 100, payloadBuffer, &payloadLength, payloadSubLength, &payloadSubLenSize));
    EXPECT_EQ(0u, payloadLength);
    EXPECT_EQ(0u, payloadSubLenSize);
}

TEST_F(RtpFunctionalityTest, createPayloadForH265NullNalusWithNonNullBuffer)
{
    BYTE payloadBuffer[100];
    UINT32 payloadLength = 0xDEADBEEF;
    UINT32 payloadSubLenSize = 0xDEADBEEF;
    UINT32 payloadSubLength[10];

    // nalus is NULL but payloadBuffer is non-NULL (sizeCalculationOnly = FALSE).
    // Should return STATUS_NULL_ARG and zero out the output parameters.
    EXPECT_EQ(STATUS_NULL_ARG,
              createPayloadForH265(DEFAULT_MTU_SIZE_BYTES, NULL, 100, payloadBuffer, &payloadLength, payloadSubLength, &payloadSubLenSize));
    EXPECT_EQ(0u, payloadLength);
    EXPECT_EQ(0u, payloadSubLenSize);
}

TEST_F(RtpFunctionalityTest, createPayloadForVP8NullDataWithNonNullBuffer)
{
    BYTE payloadBuffer[100];
    UINT32 payloadLength = 0xDEADBEEF;
    UINT32 payloadSubLenSize = 0xDEADBEEF;
    UINT32 payloadSubLength[10];

    // pData is NULL but payloadBuffer is non-NULL (sizeCalculationOnly = FALSE).
    // Should return STATUS_NULL_ARG and zero out the output parameters.
    EXPECT_EQ(STATUS_NULL_ARG,
              createPayloadForVP8(DEFAULT_MTU_SIZE_BYTES, NULL, 100, payloadBuffer, &payloadLength, payloadSubLength, &payloadSubLenSize));
    EXPECT_EQ(0u, payloadLength);
    EXPECT_EQ(0u, payloadSubLenSize);
}

TEST_F(RtpFunctionalityTest, testDepayH264StapBPacket)
{
    /** Construct a valid STAP-B packet:
     * Byte 0: NAL indicator with type 25 (STAP-B)
     * Bytes 1-2: DON (Decoding Order Number)
     * Bytes 3-4: sub-NAL size (big-endian)
     * Bytes 5+: sub-NAL data
     */
    BYTE stapBPacket[] = {
        0x19,             // type=25 (STAP_B_INDICATOR)
        0x00, 0x01,       // DON = 1
        0x00, 0x03,       // sub-NAL size = 3
        0x67, 0x42, 0x00, // sub-NAL data (3 bytes)
        0x00, 0x02,       // second sub-NAL size = 2
        0xAA, 0xBB        // second sub-NAL data (2 bytes)
    };
    UINT32 packetLength = SIZEOF(stapBPacket);
    UINT32 naluLength = 0;
    BOOL isStart = FALSE;

    // Size calculation pass (pNaluData = NULL)
    EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(stapBPacket, packetLength, NULL, &naluLength, &isStart));
    EXPECT_TRUE(isStart);
    // Expected: start4ByteCode(4) + 3 + start4ByteCode(4) + 2 = 13
    EXPECT_EQ(13u, naluLength);

    // Copy pass - allocate exact buffer based on size
    BYTE outputBuffer[13];
    UINT32 outputLen = SIZEOF(outputBuffer);
    EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(stapBPacket, packetLength, outputBuffer, &outputLen, &isStart));
    EXPECT_EQ(13u, outputLen);

    // Verify first sub-NAL has start code prefix
    EXPECT_EQ(0x00, outputBuffer[0]);
    EXPECT_EQ(0x00, outputBuffer[1]);
    EXPECT_EQ(0x00, outputBuffer[2]);
    EXPECT_EQ(0x01, outputBuffer[3]);
    // Verify first sub-NAL data
    EXPECT_EQ(0x67, outputBuffer[4]);
    EXPECT_EQ(0x42, outputBuffer[5]);
    EXPECT_EQ(0x00, outputBuffer[6]);
    // Verify second sub-NAL has start code prefix
    EXPECT_EQ(0x00, outputBuffer[7]);
    EXPECT_EQ(0x00, outputBuffer[8]);
    EXPECT_EQ(0x00, outputBuffer[9]);
    EXPECT_EQ(0x01, outputBuffer[10]);
    // Verify second sub-NAL data
    EXPECT_EQ(0xAA, outputBuffer[11]);
    EXPECT_EQ(0xBB, outputBuffer[12]);
}

TEST_F(RtpFunctionalityTest, testDepayH264StapAPacket)
{
    /** Construct a valid STAP-A packet:
     * Byte 0: NAL indicator with type 24 (STAP-A)
     * No DON field (this is what differentiates STAP-A from STAP-B)
     * Bytes 1-2: sub-NAL size (big-endian)
     * Bytes 3+: sub-NAL data
     */
    BYTE stapAPacket[] = {
        0x18,             // NAL header: type=24 (STAP_A_INDICATOR)
        0x00, 0x03,       // first sub-NAL size = 3
        0x67, 0x42, 0x00, // first sub-NAL data (3 bytes, fake SPS)
        0x00, 0x02,       // second sub-NAL size = 2
        0xCC, 0xDD        // second sub-NAL data (2 bytes)
    };
    UINT32 packetLength = SIZEOF(stapAPacket);
    UINT32 naluLength = 0;
    BOOL isStart = FALSE;

    // Size calculation pass (pNaluData = NULL)
    EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(stapAPacket, packetLength, NULL, &naluLength, &isStart));
    EXPECT_TRUE(isStart);
    // Expected: start4ByteCode(4) + 3 + start4ByteCode(4) + 2 = 13
    EXPECT_EQ(13u, naluLength);

    // Copy pass - allocate exact buffer based on size
    BYTE outputBuffer[13];
    UINT32 outputLen = SIZEOF(outputBuffer);
    EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(stapAPacket, packetLength, outputBuffer, &outputLen, &isStart));
    EXPECT_EQ(13u, outputLen);

    // Verify first sub-NAL: start code + data
    EXPECT_EQ(0x00, outputBuffer[0]);
    EXPECT_EQ(0x00, outputBuffer[1]);
    EXPECT_EQ(0x00, outputBuffer[2]);
    EXPECT_EQ(0x01, outputBuffer[3]);
    EXPECT_EQ(0x67, outputBuffer[4]);
    EXPECT_EQ(0x42, outputBuffer[5]);
    EXPECT_EQ(0x00, outputBuffer[6]);
    // Verify second sub-NAL: start code + data
    EXPECT_EQ(0x00, outputBuffer[7]);
    EXPECT_EQ(0x00, outputBuffer[8]);
    EXPECT_EQ(0x00, outputBuffer[9]);
    EXPECT_EQ(0x01, outputBuffer[10]);
    EXPECT_EQ(0xCC, outputBuffer[11]);
    EXPECT_EQ(0xDD, outputBuffer[12]);
}

TEST_F(RtpFunctionalityTest, depayH264FuARejectsTooSmallPacket)
{
    /** Construct an H.264 FU-A packet (RFC 3984 Section 5.8):
     *  Byte 0: FU Indicator — [F|NRI|Type]
     *    0x7C = 0b0_11_11100 → F=0, NRI=3, Type=28 (FU_A_INDICATOR)
     *  Byte 1: FU Header — [S|E|R|Type]
     *    0x85 = 0b1_0_0_00101 → S=1 (start), E=0, R=0, Type=5 (IDR)
     *  Bytes 2+: NAL fragment payload (absent in these test packets)
     *
     *  FU_A_HEADER_SIZE = 2 (indicator + FU header)
     *  A valid FU-A packet must have packetLength > 2 (at least 1 byte of payload)
     */

    // 1-byte packet: only the FU indicator, no FU header or payload
    BYTE fuAPacket[] = {0x7C};
    UINT32 naluLength = 0;
    BOOL isStart = FALSE;

    EXPECT_EQ(STATUS_RTP_INPUT_PACKET_TOO_SMALL, depayH264FromRtpPayload(fuAPacket, SIZEOF(fuAPacket), NULL, &naluLength, &isStart));
    EXPECT_EQ(0u, naluLength);

    // 2-byte packet: FU indicator + FU header, but no payload (exactly FU_A_HEADER_SIZE)
    BYTE fuAPacket2[] = {0x7C, 0x85};
    naluLength = 0;
    EXPECT_EQ(STATUS_RTP_INPUT_PACKET_TOO_SMALL, depayH264FromRtpPayload(fuAPacket2, SIZEOF(fuAPacket2), NULL, &naluLength, &isStart));
    EXPECT_EQ(0u, naluLength);
}

TEST_F(RtpFunctionalityTest, depayH265FuRejectsTooSmallPacket)
{
    /** Construct an H.265 FU packet (RFC 7798 Section 4.4.3):
     *  Bytes 0-1: PayloadHdr — [F|Type|LayerId|TID]
     *    Type is bits [1..6] of byte 0: (byte0 >> 1) & 0x3F
     *    0x62 = 0b0_1100010_... → Type = (0x62 >> 1) & 0x3F = 49 (H265_FU_TYPE_ID)
     *    0x01 = LayerId/TID byte
     *  Byte 2: FU Header — [S|E|FuType]
     *    0x80 = 0b1_0_100000 → S=1 (start), E=0, FuType=32
     *  Bytes 3+: NAL fragment payload (absent in these test packets)
     *
     *  H265_FU_HEADER_SIZE = 3 (2-byte PayloadHdr + 1-byte FU header)
     *  A valid H.265 FU packet must have packetLength > 3 (at least 1 byte of payload)
     */

    // 2-byte packet: only PayloadHdr, no FU header or payload
    BYTE fuH265Packet[] = {0x62, 0x01};
    UINT32 naluLength = 0;
    BOOL isStart = FALSE;

    EXPECT_EQ(STATUS_RTP_INPUT_PACKET_TOO_SMALL, depayH265FromRtpPayload(fuH265Packet, SIZEOF(fuH265Packet), NULL, &naluLength, &isStart));
    EXPECT_EQ(0u, naluLength);

    // 3-byte packet: PayloadHdr + FU header, but no payload (exactly H265_FU_HEADER_SIZE)
    BYTE fuH265Packet2[] = {0x62, 0x01, 0x80};
    naluLength = 0;
    EXPECT_EQ(STATUS_RTP_INPUT_PACKET_TOO_SMALL, depayH265FromRtpPayload(fuH265Packet2, SIZEOF(fuH265Packet2), NULL, &naluLength, &isStart));
    EXPECT_EQ(0u, naluLength);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
