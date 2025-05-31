#define LOG_CLASS "RtpH264Payloader"

#include "../../Include_i.h"

STATUS createPayloadForH264(UINT32 mtu, PBYTE nalus, UINT32 nalusLength, PBYTE payloadBuffer, PUINT32 pPayloadLength, PUINT32 pPayloadSubLength,
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
    CHK(mtu > FU_A_HEADER_SIZE, STATUS_RTP_INPUT_MTU_TOO_SMALL);

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
        CHK_STATUS(getNextNaluLength(curPtrInNalus, remainNalusLength, &startIndex, &nextNaluLength));

        curPtrInNalus += startIndex;

        remainNalusLength -= startIndex;

        CHK(remainNalusLength != 0, retStatus);

        if (sizeCalculationOnly) {
            CHK_STATUS(createPayloadFromNalu(mtu, curPtrInNalus, nextNaluLength, NULL, &singlePayloadLength, &singlePayloadSubLenSize));
            payloadArray.payloadLength += singlePayloadLength;
            payloadArray.payloadSubLenSize += singlePayloadSubLenSize;
        } else {
            CHK_STATUS(createPayloadFromNalu(mtu, curPtrInNalus, nextNaluLength, &payloadArray, &singlePayloadLength, &singlePayloadSubLenSize));
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

STATUS getNextNaluLength(PBYTE nalus, UINT32 nalusLength, PUINT32 pStart, PUINT32 pNaluLength)
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
        DLOGD("Warning: Failed to get the next NALu in H264 payload with 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

STATUS createPayloadFromNalu(UINT32 mtu, PBYTE nalu, UINT32 naluLength, PPayloadArray pPayloadArray, PUINT32 filledLength, PUINT32 filledSubLenSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pPayload = NULL;
    UINT8 naluType = 0;
    UINT8 naluRefIdc = 0;
    UINT32 maxPayloadSize = 0;
    UINT32 curPayloadSize = 0;
    UINT32 remainingNaluLength = naluLength;
    UINT32 payloadLength = 0;
    UINT32 payloadSubLenSize = 0;
    PBYTE pCurPtrInNalu = NULL;
    BOOL sizeCalculationOnly = (pPayloadArray == NULL);

    CHK(nalu != NULL && filledLength != NULL && filledSubLenSize != NULL, STATUS_NULL_ARG);
    CHK(sizeCalculationOnly || (pPayloadArray->payloadSubLength != NULL && pPayloadArray->payloadBuffer != NULL), STATUS_NULL_ARG);
    CHK(mtu > FU_A_HEADER_SIZE, STATUS_RTP_INPUT_MTU_TOO_SMALL);

    // https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/
    naluType = *nalu & 0x1F;   // last 5 bits
    naluRefIdc = *nalu & 0x60; // 2 bits higher than naluType

    if (!sizeCalculationOnly) {
        pPayload = pPayloadArray->payloadBuffer;
    }

    if (naluLength <= mtu) {
        payloadLength += naluLength;
        payloadSubLenSize++;

        if (!sizeCalculationOnly) {
            CHK(payloadSubLenSize <= pPayloadArray->maxPayloadSubLenSize && payloadLength <= pPayloadArray->maxPayloadLength,
                STATUS_BUFFER_TOO_SMALL);

            // Single NALU https://tools.ietf.org/html/rfc6184#section-5.6
            MEMCPY(pPayload, nalu, naluLength);
            pPayloadArray->payloadSubLength[payloadSubLenSize - 1] = naluLength;
            pPayload += pPayloadArray->payloadSubLength[payloadSubLenSize - 1];
        }
    } else {
        // FU-A https://tools.ietf.org/html/rfc6184#section-5.8
        maxPayloadSize = mtu - FU_A_HEADER_SIZE;

        // According to the RFC, the first octet is skipped due to redundant information
        remainingNaluLength--;
        pCurPtrInNalu = nalu + 1;

        while (remainingNaluLength != 0) {
            curPayloadSize = MIN(maxPayloadSize, remainingNaluLength);
            payloadSubLenSize++;
            payloadLength += FU_A_HEADER_SIZE + curPayloadSize;

            if (!sizeCalculationOnly) {
                CHK(payloadSubLenSize <= pPayloadArray->maxPayloadSubLenSize && payloadLength <= pPayloadArray->maxPayloadLength,
                    STATUS_BUFFER_TOO_SMALL);

                MEMCPY(pPayload + FU_A_HEADER_SIZE, pCurPtrInNalu, curPayloadSize);
                /* FU-A indicator is 28 */
                pPayload[0] = 28 | naluRefIdc;
                pPayload[1] = naluType;
                if (remainingNaluLength == naluLength - 1) {
                    // Set for starting bit
                    pPayload[1] |= 1 << 7;
                } else if (remainingNaluLength == curPayloadSize) {
                    // Set for ending bit
                    pPayload[1] |= 1 << 6;
                }

                pPayloadArray->payloadSubLength[payloadSubLenSize - 1] = FU_A_HEADER_SIZE + curPayloadSize;
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

STATUS depayH264FromRtpPayload(PBYTE pRawPacket, UINT32 packetLength, PBYTE pNaluData, PUINT32 pNaluLength, PBOOL pIsStart)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 naluLength = 0;
    UINT8 naluType = 0;
    UINT8 naluRefIdc = 0;
    UINT8 indicator = 0;
    BOOL sizeCalculationOnly = (pNaluData == NULL);
    BOOL isStartingPacket = FALSE;
    PBYTE pCurPtr = pRawPacket;
    static BYTE start4ByteCode[] = {0x00, 0x00, 0x00, 0x01};
    UINT16 subNaluSize = 0;

    CHK(pRawPacket != NULL && pNaluLength != NULL, STATUS_NULL_ARG);
    CHK(packetLength > 0, retStatus);

    // TODO: Add support for Aggregate Packets https://tools.ietf.org/html/rfc6184#section-5.7
    // indicator for types https://tools.ietf.org/html/rfc3984#section-5.2
    indicator = *pRawPacket & NAL_TYPE_MASK;
    switch (indicator) {
        case FU_A_INDICATOR:
            // FU-A indicator
            naluRefIdc = *pCurPtr & 0x60;
            pCurPtr++;
            naluType = *pCurPtr & 0x1f;
            isStartingPacket = (*pCurPtr & (1 << 7)) != 0;
            if (isStartingPacket) {
                naluLength = packetLength - FU_A_HEADER_SIZE + 1;
            } else {
                naluLength = packetLength - FU_A_HEADER_SIZE;
            }
            break;
        case FU_B_INDICATOR:
            // FU-B indicator
            naluLength = packetLength - FU_A_HEADER_SIZE + 1;
            break;
        case STAP_A_INDICATOR:
            pCurPtr += STAP_A_HEADER_SIZE;
            do {
                subNaluSize = getUnalignedInt16BigEndian(pCurPtr);
                pCurPtr += SIZEOF(UINT16);
                naluLength += subNaluSize + SIZEOF(start4ByteCode);
                pCurPtr += subNaluSize;
            } while (subNaluSize > 0 && pCurPtr < pRawPacket + packetLength);
            isStartingPacket = TRUE;
            break;
        case STAP_B_INDICATOR:
            pCurPtr += STAP_B_HEADER_SIZE;
            do {
                subNaluSize = getUnalignedInt16BigEndian(pCurPtr);
                pCurPtr += SIZEOF(UINT16);
                naluLength += subNaluSize + SIZEOF(start4ByteCode);
                pCurPtr += subNaluSize;
            } while (subNaluSize > 0 && pCurPtr < pRawPacket + packetLength);
            isStartingPacket = TRUE;
            break;
        default:
            // Single NALU https://tools.ietf.org/html/rfc6184#section-5.6
            naluLength = packetLength;
            isStartingPacket = TRUE;
    }

    if (isStartingPacket && indicator != STAP_A_INDICATOR && indicator != STAP_B_INDICATOR) {
        naluLength += SIZEOF(start4ByteCode);
    }

    // Only return size if given buffer is NULL
    CHK(!sizeCalculationOnly, retStatus);
    CHK(naluLength <= *pNaluLength, STATUS_BUFFER_TOO_SMALL);

    if (isStartingPacket && indicator != STAP_A_INDICATOR && indicator != STAP_B_INDICATOR) {
        MEMCPY(pNaluData, start4ByteCode, SIZEOF(start4ByteCode));
        naluLength -= SIZEOF(start4ByteCode);
        pNaluData += SIZEOF(start4ByteCode);
    }
    switch (indicator) {
        case FU_A_INDICATOR:
            DLOGS("FU_A_INDICATOR starting packet %d len %d", isStartingPacket, naluLength);
            if (isStartingPacket) {
                MEMCPY(pNaluData + 1, pRawPacket + FU_A_HEADER_SIZE, naluLength - 1);
                *pNaluData = naluRefIdc | naluType;
            } else {
                MEMCPY(pNaluData, pRawPacket + FU_A_HEADER_SIZE, naluLength);
            }
            break;
        case FU_B_INDICATOR:
            DLOGS("FU_B_INDICATOR starting packet %d len %d", isStartingPacket, naluLength);
            if (isStartingPacket) {
                MEMCPY(pNaluData + 1, pRawPacket + FU_B_HEADER_SIZE, naluLength - 1);
                *pNaluData = naluRefIdc | naluType;
            } else {
                MEMCPY(pNaluData, pRawPacket + FU_B_HEADER_SIZE, naluLength);
            }
            break;
        case STAP_A_INDICATOR:
            naluLength = 0;
            pCurPtr = pRawPacket + STAP_A_HEADER_SIZE;
            do {
                subNaluSize = getUnalignedInt16BigEndian(pCurPtr);
                pCurPtr += SIZEOF(UINT16);
                MEMCPY(pNaluData, start4ByteCode, SIZEOF(start4ByteCode));
                pNaluData += SIZEOF(start4ByteCode);
                MEMCPY(pNaluData, pCurPtr, subNaluSize);
                pCurPtr += subNaluSize;
                pNaluData += subNaluSize;
                naluLength += SIZEOF(start4ByteCode) + subNaluSize;
            } while (subNaluSize > 0 && pCurPtr < pRawPacket + packetLength);
            DLOGS("STAP_A_INDICATOR starting packet %d len %d", isStartingPacket, naluLength);
            break;
        case STAP_B_INDICATOR:
            naluLength = 0;
            pCurPtr = pRawPacket + STAP_A_HEADER_SIZE;
            do {
                subNaluSize = getUnalignedInt16BigEndian(pCurPtr);
                pCurPtr += SIZEOF(UINT16);
                MEMCPY(pNaluData, start4ByteCode, SIZEOF(start4ByteCode));
                pNaluData += SIZEOF(start4ByteCode);
                MEMCPY(pNaluData, pCurPtr, subNaluSize);
                pCurPtr += subNaluSize;
                pNaluData += subNaluSize;
                naluLength += SIZEOF(start4ByteCode) + subNaluSize;
            } while (subNaluSize > 0 && pCurPtr < pRawPacket + packetLength);
            DLOGS("STAP_B_INDICATOR starting packet %d len %d", isStartingPacket, naluLength);
            break;
        default:
            DLOGS("Single NALU %d len %d", isStartingPacket, packetLength);
            MEMCPY(pNaluData, pRawPacket, naluLength);
    }
    if (isStartingPacket && indicator != STAP_A_INDICATOR && indicator != STAP_B_INDICATOR) {
        naluLength += SIZEOF(start4ByteCode);
    }
    DLOGS("Wrote naluLength %d isStartingPacket %d", naluLength, isStartingPacket);

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
