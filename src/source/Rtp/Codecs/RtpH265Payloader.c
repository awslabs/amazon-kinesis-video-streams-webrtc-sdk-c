#define LOG_CLASS "RtpH265Payloader"

#include "../../Include_i.h"

STATUS createPayloadForH265(UINT32 mtu, PBYTE nalus, UINT32 nalusLength, PBYTE payloadBuffer, PUINT32 pPayloadLength, PUINT32 pPayloadSubLength,
                            PUINT32 pPayloadSubLenSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE curPtrInNalus = nalus;
    UINT32 remainNalusLength = nalusLength;
    UINT32 nextNaluLength = 0;
    UINT32 startIndex = 0;
    UINT32 singlePayloadLength = 0;
    UINT32 singlePayloadSubLenSize = 0;
    BOOL sizeCalculationOnly = (payloadBuffer == NULL);
    PayloadArray payloadArray;

    CHK(nalus != NULL && pPayloadSubLenSize != NULL && pPayloadLength != NULL && (sizeCalculationOnly || pPayloadSubLength != NULL), STATUS_NULL_ARG);
    CHK(mtu > H265_FU_HEADER_SIZE, STATUS_RTP_INPUT_MTU_TOO_SMALL);

    if (sizeCalculationOnly) {
        payloadArray.payloadLength = 0;
        payloadArray.payloadSubLenSize = 0;
        payloadArray.maxPayloadLength = 0;
        payloadArray.maxPayloadSubLenSize = 0;
    } else {
        payloadArray.payloadLength = *pPayloadLength;
        payloadArray.payloadSubLenSize = *pPayloadSubLenSize;
        payloadArray.maxPayloadLength = *pPayloadLength;
        payloadArray.maxPayloadSubLenSize = *pPayloadSubLenSize;
    }
    payloadArray.payloadBuffer = payloadBuffer;
    payloadArray.payloadSubLength = pPayloadSubLength;

    do {
        CHK_STATUS(getNextNaluLengthH265(curPtrInNalus, remainNalusLength, &startIndex, &nextNaluLength));

        curPtrInNalus += startIndex;

        remainNalusLength -= startIndex;

        CHK(remainNalusLength != 0, retStatus);

        if (sizeCalculationOnly) {
            CHK_STATUS(createPayloadFromNaluH265(mtu, curPtrInNalus, nextNaluLength, NULL, &singlePayloadLength, &singlePayloadSubLenSize));
            payloadArray.payloadLength += singlePayloadLength;
            payloadArray.payloadSubLenSize += singlePayloadSubLenSize;
        } else {
            CHK_STATUS(createPayloadFromNaluH265(mtu, curPtrInNalus, nextNaluLength, &payloadArray, &singlePayloadLength, &singlePayloadSubLenSize));
            payloadArray.payloadBuffer += singlePayloadLength;
            payloadArray.payloadSubLength += singlePayloadSubLenSize;
            payloadArray.maxPayloadLength -= singlePayloadLength;
            payloadArray.maxPayloadSubLenSize -= singlePayloadSubLenSize;
        }

        remainNalusLength -= nextNaluLength;
        curPtrInNalus += nextNaluLength;
    } while (remainNalusLength != 0);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        payloadArray.payloadLength = 0;
        payloadArray.payloadSubLenSize = 0;
    }

    if (pPayloadSubLenSize != NULL && pPayloadLength != NULL) {
        *pPayloadLength = payloadArray.payloadLength;
        *pPayloadSubLenSize = payloadArray.payloadSubLenSize;
    }

    LEAVES();
    return retStatus;
}

STATUS getNextNaluLengthH265(PBYTE nalus, UINT32 nalusLength, PUINT32 pStart, PUINT32 pNaluLength)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 zeroCount = 0, offset = 0;
    BOOL naluFound = FALSE;
    PBYTE pCurrent = NULL;

    CHK(nalus != NULL && pStart != NULL && pNaluLength != NULL, STATUS_NULL_ARG);

    // Annex-B Nalu will have 0x000000001 or 0x000001 start code, at most 4 bytes
    while (offset < 4 && offset < nalusLength && nalus[offset] == 0) {
        offset++;
    }

    CHK(offset < nalusLength && offset < 4 && offset >= 2 && nalus[offset] == 1, STATUS_RTP_INVALID_NALU);
    *pStart = ++offset;
    pCurrent = nalus + offset;

    /* Not doing validation on number of consecutive zeros being less than 4 because some device can produce
     * data with trailing zeros. */
    while (offset < nalusLength) {
        if (*pCurrent == 0) {
            /* Maybe next byte is 1 */
            offset++;
            pCurrent++;

        } else if (*pCurrent == 1) {
            if (*(pCurrent - 1) == 0 && *(pCurrent - 2) == 0) {
                zeroCount = *(pCurrent - 3) == 0 ? 3 : 2;
                naluFound = TRUE;
                break;
            }

            /* The jump is always 3 because of the 1 previously matched.
             * All the 0's must be after this '1' matched at offset */
            offset += 3;
            pCurrent += 3;
        } else {
            /* Can jump 3 bytes forward */
            offset += 3;
            pCurrent += 3;
        }
    }
    *pNaluLength = MIN(offset, nalusLength) - *pStart - (naluFound ? zeroCount : 0);

CleanUp:

    // As we might hit error often in a "bad" frame scenario, we can't use CHK_LOG_ERR as it will be too frequent
    if (STATUS_FAILED(retStatus)) {
        DLOGD("Warning: Failed to get the next NALu in H265 payload with 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

STATUS createPayloadFromNaluH265(UINT32 mtu, PBYTE nalu, UINT32 naluLength, PPayloadArray pPayloadArray, PUINT32 filledLength,
                                 PUINT32 filledSubLenSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pPayload = NULL;
    UINT8 naluType = 0;
    UINT32 maxPayloadSize = 0;
    UINT32 curPayloadSize = 0;
    UINT32 remainingNaluLength = naluLength;
    UINT32 payloadLength = 0;
    UINT32 payloadSubLenSize = 0;
    PBYTE pCurPtrInNalu = NULL;
    BOOL sizeCalculationOnly = (pPayloadArray == NULL);

    CHK(nalu != NULL && filledLength != NULL && filledSubLenSize != NULL, STATUS_NULL_ARG);
    CHK(sizeCalculationOnly || (pPayloadArray->payloadSubLength != NULL && pPayloadArray->payloadBuffer != NULL), STATUS_NULL_ARG);
    CHK(mtu > H265_FU_HEADER_SIZE, STATUS_RTP_INPUT_MTU_TOO_SMALL);

    naluType = (nalu[0] & 0x7E) >> 1; // 6 bits after forbidden zero bit 0x7E(0111 1110)

    if (!sizeCalculationOnly) {
        pPayload = pPayloadArray->payloadBuffer;
    }

    if (naluLength <= mtu) {
        // Single NALU https://www.rfc-editor.org/rfc/rfc7798.html#section-4.4.1
        payloadLength += naluLength;
        payloadSubLenSize++;

        if (!sizeCalculationOnly) {
            CHK(payloadSubLenSize <= pPayloadArray->maxPayloadSubLenSize && payloadLength <= pPayloadArray->maxPayloadLength,
                STATUS_BUFFER_TOO_SMALL);

            MEMCPY(pPayload, nalu, naluLength);
            pPayloadArray->payloadSubLength[payloadSubLenSize - 1] = naluLength;
            pPayload += pPayloadArray->payloadSubLength[payloadSubLenSize - 1];
        }
    } else {
        // Fragmentation units: https://www.rfc-editor.org/rfc/rfc7798.html#section-4.4.3
        maxPayloadSize = mtu - H265_FU_HEADER_SIZE;

        // According to the RFC, the first octet is skipped due to redundant information
        remainingNaluLength -= 2;
        pCurPtrInNalu = nalu + 2;

        while (remainingNaluLength != 0) {
            curPayloadSize = MIN(maxPayloadSize, remainingNaluLength);
            payloadSubLenSize++;
            payloadLength += H265_FU_HEADER_SIZE + curPayloadSize;

            if (!sizeCalculationOnly) {
                CHK(payloadSubLenSize <= pPayloadArray->maxPayloadSubLenSize && payloadLength <= pPayloadArray->maxPayloadLength,
                    STATUS_BUFFER_TOO_SMALL);

                pPayload[0] = (H265_FU_TYPE_ID << 1) | (nalu[0] & 0x81) | (nalu[0] & 0x1); // H265_FU_TYPE_ID indicator is 49
                pPayload[1] = nalu[1] & 0xff;
                pPayload[2] = naluType & 0x3f;
                if (remainingNaluLength == naluLength - 2) {
                    pPayload[2] |= (1 << 7); // Set for starting bit
                } else if (remainingNaluLength == curPayloadSize) {
                    pPayload[2] |= (1 << 6); // Set for ending bit
                }

                MEMCPY(pPayload + H265_FU_HEADER_SIZE, pCurPtrInNalu, curPayloadSize);

                pPayloadArray->payloadSubLength[payloadSubLenSize - 1] = H265_FU_HEADER_SIZE + curPayloadSize;
                pPayload += pPayloadArray->payloadSubLength[payloadSubLenSize - 1];
            }

            pCurPtrInNalu += curPayloadSize;
            remainingNaluLength -= curPayloadSize;
        }
    }

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        payloadLength = 0;
        payloadSubLenSize = 0;
    }

    if (filledLength != NULL && filledSubLenSize != NULL) {
        *filledLength = payloadLength;
        *filledSubLenSize = payloadSubLenSize;
    }

    LEAVES();
    return retStatus;
}

STATUS depayH265FromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pNaluData, PUINT32 pNaluLength, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 naluLength = packetLength, headerSize = 0;
    UINT8 payloadHeaderType;
    BOOL sizeCalculationOnly = (pNaluData == NULL);
    BOOL isStartingPacket = TRUE;
    PBYTE pCurPtrInNalu = pNaluData;
    static BYTE start4ByteCode[] = {0x00, 0x00, 0x00, 0x01};

    CHK(pRawPacket != NULL && pNaluLength != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    payloadHeaderType = (pRawPacket[0] >> 1) & 0x3F;

    if (payloadHeaderType == H265_FU_TYPE_ID) {
        isStartingPacket = (pRawPacket[2] & 0x80) != 0;
        headerSize = H265_FU_HEADER_SIZE;
        naluLength -= headerSize;

        if (isStartingPacket) {
            naluLength += 2;
        }
    }

    if (isStartingPacket) {
        naluLength += SIZEOF(start4ByteCode);
    }

    CHK(!sizeCalculationOnly, retStatus);
    CHK(naluLength <= *pNaluLength, STATUS_BUFFER_TOO_SMALL);

    if (isStartingPacket) {
        MEMCPY(pCurPtrInNalu, start4ByteCode, SIZEOF(start4ByteCode));
        if (payloadHeaderType == H265_FU_TYPE_ID) {
            pCurPtrInNalu[4] = ((pRawPacket[2] & 0x3F) << 1) | (pRawPacket[0] & 0x81);
            pCurPtrInNalu[5] = pRawPacket[1];
            pCurPtrInNalu += 2;
        }
        pCurPtrInNalu += SIZEOF(start4ByteCode);
    }
    MEMCPY(pCurPtrInNalu, pRawPacket + headerSize, packetLength - headerSize);

CleanUp:
    if (STATUS_FAILED(retStatus) && sizeCalculationOnly) {
        naluLength = 0;
    }

    if (pNaluLength != NULL) {
        *pNaluLength = naluLength;
    }

    if (pIsStart != NULL) {
        *pIsStart = isStartingPacket;
    }

    LEAVES();
    return retStatus;
}
