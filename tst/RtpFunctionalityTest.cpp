#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

#define NUMBER_OF_FRAME_FILES               403
#define DEFAULT_FPS_VALUE                   25
static BYTE start4ByteCode[] = {0x00, 0x00, 0x00, 0x01};

class RtpFunctionalityTest : public WebRtcClientTestBase {
};

STATUS readFrameData(PBYTE pFrame, PUINT32 pSize, UINT32 index, PCHAR frameFilePath)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT64 size = 0;

    CHK(pFrame != NULL && pSize != NULL, STATUS_NULL_ARG);

    SNPRINTF(filePath, MAX_PATH_LEN, "%s/frame-%03d.h264", frameFilePath, index);

    // Get the size and read into frame
    CHK_STATUS(readFile(filePath, TRUE, NULL, &size));
    CHK_STATUS(readFile(filePath, TRUE, pFrame, &size));

    *pSize = (UINT32) size;

CleanUp:

    return retStatus;
}

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
    INT32 status = 0;

    payloadArray.payloadBuffer = payload;
    payloadArray.payloadLength = payloadLen;
    payloadArray.payloadSubLength = &payloadLen;
    payloadArray.payloadSubLenSize = 1;

    packetList = (PRtpPacket) MEMALLOC(SIZEOF(RtpPacket));

    SRAND(GETTIME());
    EXPECT_EQ(STATUS_SUCCESS, constructRtpPackets(&payloadArray, 8, 1, 1324857487, 0x1234ABCD, (PRtpPacket) packetList, payloadArray.payloadSubLenSize));

    EXPECT_NE(NULL, (UINT64) packetList);

    for (i = 0; i < payloadArray.payloadSubLenSize; i++)
    {
        pRtpPacket = packetList + i;

        EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, &rawPacket, &packetLen));
        EXPECT_EQ(STATUS_SUCCESS, createRtpPacketFromBytes(rawPacket, packetLen, &pNewRtpPacket));
        // Verify the extracted header is the same as original header
        EXPECT_TRUE(memcmp(&pRtpPacket->header, &pNewRtpPacket->header, SIZEOF(RtpPacketHeader)) == 0);
        // Verify the extracted payload is the same as original payload
        EXPECT_EQ(pRtpPacket->payloadLength, pNewRtpPacket->payloadLength);
        EXPECT_TRUE(memcmp(pRtpPacket->payload, pNewRtpPacket->payload, pRtpPacket->payloadLength) == 0);

        EXPECT_EQ(STATUS_SUCCESS, freeRtpPacket(&pNewRtpPacket));
        MEMFREE(rawPacket);
        rawPacket = NULL;
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
    PRtpPacket pNewRtpPacket = NULL;
    UINT32 packetLen = 0;
    INT32 status = 0;
    UINT64 curTime = GETTIME();
    UINT32 fileIndex = 0;
    UINT32 clockRate = 90000; // 90kHz for h264
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
        EXPECT_EQ(STATUS_SUCCESS, readFrameData((PBYTE) payload, (PUINT32) &payloadLen, fileIndex,
                (PCHAR) "../samples/h264SampleFrames"));

        // First call for payload size and sub payload length size
        EXPECT_EQ(STATUS_SUCCESS, createPayloadForH264(DEFAULT_MTU_SIZE,
                                                       (PBYTE) payload,
                                                       payloadLen,
                                                       NULL,
                                                       &payloadArray.payloadLength,
                                                       NULL,
                                                       &payloadArray.payloadSubLenSize));

        if (payloadArray.payloadLength > payloadArray.maxPayloadLength)
        {
            if (payloadArray.payloadBuffer != NULL)
            {
                MEMFREE(payloadArray.payloadBuffer);
            }
            payloadArray.payloadBuffer = (PBYTE) MEMALLOC(payloadArray.payloadLength);
            payloadArray.maxPayloadLength = payloadArray.payloadLength;
        }
        if (payloadArray.payloadSubLenSize > payloadArray.maxPayloadSubLenSize)
        {
            if (payloadArray.payloadSubLength != NULL)
            {
                MEMFREE(payloadArray.payloadSubLength);
            }
            payloadArray.payloadSubLength = (PUINT32) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(UINT32));
            payloadArray.maxPayloadSubLenSize = payloadArray.payloadSubLenSize;
        }

        // Second call with actual buffer to fill in data
        EXPECT_EQ(STATUS_SUCCESS, createPayloadForH264(DEFAULT_MTU_SIZE,
                                                       (PBYTE) payload,
                                                       payloadLen,
                                                       payloadArray.payloadBuffer,
                                                       &payloadArray.payloadLength,
                                                       payloadArray.payloadSubLength,
                                                       &payloadArray.payloadSubLenSize));

        EXPECT_LT(0, payloadArray.payloadSubLenSize);
        pPacketList = (PRtpPacket) MEMALLOC(payloadArray.payloadSubLenSize * SIZEOF(RtpPacket));

        constructRtpPackets(&payloadArray,
                            96,
                            seqNum,
                            (UINT32) ((curTime - startTimeStamp) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND),
                            0x1234ABCD,
                            pPacketList,
                            payloadArray.payloadSubLenSize);

        seqNum = GET_UINT16_SEQ_NUM(seqNum + payloadArray.payloadSubLenSize);

        for (i = 0; i < payloadArray.payloadSubLenSize; i++)
        {
            pRtpPacket = pPacketList + i;
            EXPECT_EQ(STATUS_SUCCESS, createBytesFromRtpPacket(pRtpPacket, &rawPacket, &packetLen));
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
    PBYTE payload = (PBYTE) MEMALLOC(200000); // Assuming this is enough
    PBYTE depayload = (PBYTE) MEMALLOC(1500); // This is more than max mtu
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
        EXPECT_EQ(STATUS_SUCCESS, readFrameData((PBYTE) payload, (PUINT32) &payloadLen, fileIndex,
                (PCHAR) "../samples/h264SampleFrames"));

        // First call for payload size and sub payload length size
        EXPECT_EQ(STATUS_SUCCESS,createPayloadForH264(DEFAULT_MTU_SIZE,
                                                      (PBYTE) payload,
                                                      payloadLen,
                                                      NULL,
                                                      &payloadArray.payloadLength,
                                                      NULL,
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
        EXPECT_EQ(STATUS_SUCCESS, createPayloadForH264(DEFAULT_MTU_SIZE, (PBYTE) payload, payloadLen, payloadArray.payloadBuffer,
                &payloadArray.payloadLength, payloadArray.payloadSubLength,
                &payloadArray.payloadSubLenSize));

        EXPECT_LT(0, payloadArray.payloadSubLenSize);

        offset = 0;

        for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
            EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(payloadArray.payloadBuffer + offset,
                                                              payloadArray.payloadSubLength[i],
                                                              NULL,
                                                              &newPayloadSubLen,
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
            EXPECT_EQ(STATUS_SUCCESS, depayH264FromRtpPayload(payloadArray.payloadBuffer + offset,
                                                              payloadArray.payloadSubLength[i],
                                                              depayload,
                                                              &newPayloadSubLen,
                                                              &isStartPacket));
            if (isStartPacket) {
                EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(pCurPtrInPayload, remainPayloadLen, &startIndex, &naluLength));
                pCurPtrInPayload += startIndex;
                startLen = SIZEOF(start4ByteCode);
            } else {
                startLen = 0;
            }
            EXPECT_TRUE(memcmp(pCurPtrInPayload, depayload + startLen, newPayloadSubLen - startLen) == 0);
            pCurPtrInPayload += newPayloadSubLen - startLen;
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
    EXPECT_EQ(STATUS_SUCCESS,createPayloadForOpus(DEFAULT_MTU_SIZE, (PBYTE) &payload, payloadLen, NULL, &payloadArray.payloadLength, NULL,
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
    EXPECT_EQ(STATUS_SUCCESS, createPayloadForOpus(DEFAULT_MTU_SIZE, (PBYTE) &payload, payloadLen, payloadArray.payloadBuffer,
            &payloadArray.payloadLength, payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

    EXPECT_EQ(1, payloadArray.payloadSubLenSize);
    EXPECT_EQ(6, payloadArray.payloadSubLength[0]);

    EXPECT_EQ(STATUS_SUCCESS, depayOpusFromRtpPayload(payloadArray.payloadBuffer,
                                                      payloadArray.payloadSubLength[0],
                                                      NULL,
                                                      &newPayloadSubLen,
                                                      NULL));
    EXPECT_EQ(6, newPayloadSubLen);

    newPayloadSubLen = depayloadSize;
    EXPECT_EQ(STATUS_SUCCESS, depayOpusFromRtpPayload(payloadArray.payloadBuffer,
                                                      payloadArray.payloadSubLength[0],
                                                      depayload,
                                                      &newPayloadSubLen,
                                                      NULL));
    EXPECT_TRUE(memcmp(payload, depayload, newPayloadSubLen) == 0);

    MEMFREE(payloadArray.payloadBuffer);
    MEMFREE(payloadArray.payloadSubLength);
    MEMFREE(depayload);
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
    EXPECT_EQ(STATUS_SUCCESS,createPayloadForG711(DEFAULT_MTU_SIZE,
                                                  (PBYTE) &payload,
                                                  payloadLen,
                                                  NULL,
                                                  &payloadArray.payloadLength,
                                                  NULL,
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
    EXPECT_EQ(STATUS_SUCCESS, createPayloadForG711(DEFAULT_MTU_SIZE,
                                                   (PBYTE) &payload,
                                                   payloadLen,
                                                   payloadArray.payloadBuffer,
                                                   &payloadArray.payloadLength,
                                                   payloadArray.payloadSubLength,
                                                   &payloadArray.payloadSubLenSize));

    EXPECT_EQ(1, payloadArray.payloadSubLenSize);
    EXPECT_EQ(6, payloadArray.payloadSubLength[0]);

    EXPECT_EQ(STATUS_SUCCESS, depayG711FromRtpPayload(payloadArray.payloadBuffer,
                                                      payloadArray.payloadSubLength[0],
                                                      NULL,
                                                      &newPayloadSubLen,
                                                      NULL));
    EXPECT_EQ(6, newPayloadSubLen);

    newPayloadSubLen = depayloadSize;
    EXPECT_EQ(STATUS_SUCCESS, depayG711FromRtpPayload(payloadArray.payloadBuffer,
                                                      payloadArray.payloadSubLength[0],
                                                      depayload,
                                                      &newPayloadSubLen,
                                                      NULL));
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
    EXPECT_EQ(STATUS_SUCCESS,createPayloadForG711(4,
                                                  (PBYTE) &payload,
                                                  payloadLen, NULL,
                                                  &payloadArray.payloadLength,
                                                  NULL,
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
    EXPECT_EQ(STATUS_SUCCESS, createPayloadForG711(4, (PBYTE) &payload, payloadLen, payloadArray.payloadBuffer,&payloadArray.payloadLength,
           payloadArray.payloadSubLength, &payloadArray.payloadSubLenSize));

    EXPECT_EQ(3, payloadArray.payloadSubLenSize);
    EXPECT_EQ(4, payloadArray.payloadSubLength[0]);
    EXPECT_EQ(4, payloadArray.payloadSubLength[1]);
    EXPECT_EQ(2, payloadArray.payloadSubLength[2]);

    offset = 0;

    for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
        EXPECT_EQ(STATUS_SUCCESS, depayG711FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], NULL, &newPayloadSubLen, NULL));
        newPayloadLen += newPayloadSubLen;
        EXPECT_LT(0, newPayloadSubLen);
        offset += payloadArray.payloadSubLength[i];
    }
    EXPECT_EQ(newPayloadLen, payloadLen);

    offset = 0;
    pCurPtrInPayload = payload;
    for (i = 0; i < payloadArray.payloadSubLenSize; i++) {
        newPayloadSubLen = depayloadSize;
        EXPECT_EQ(STATUS_SUCCESS, depayG711FromRtpPayload(payloadArray.payloadBuffer + offset, payloadArray.payloadSubLength[i], depayload, &newPayloadSubLen, NULL));
        EXPECT_TRUE(memcmp(pCurPtrInPayload, depayload, newPayloadSubLen) == 0);
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
}

TEST_F(RtpFunctionalityTest, validNaluParse)
{
    BYTE data[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x02};
    UINT32 startIndex = 0, naluLength = 0;
    EXPECT_EQ(STATUS_SUCCESS, getNextNaluLength(data, 6, &startIndex, &naluLength));
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

}
}
}
}
}
