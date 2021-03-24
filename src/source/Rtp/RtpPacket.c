#define LOG_CLASS "RtpPacket"

#include "../Include_i.h"

STATUS createRtpPacket(UINT8 version, BOOL padding, BOOL extension, UINT8 csrcCount, BOOL marker, UINT8 payloadType, UINT16 sequenceNumber,
                       UINT32 timestamp, UINT32 ssrc, PUINT32 csrcArray, UINT16 extensionProfile, UINT32 extensionLength, PBYTE extensionPayload,
                       PBYTE payload, UINT32 payloadLength, PRtpPacket* ppRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacket pRtpPacket = (PRtpPacket) MEMALLOC(SIZEOF(RtpPacket));
    CHK(pRtpPacket != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pRtpPacket->pRawPacket = NULL;
    pRtpPacket->rawPacketLength = 0;
    CHK_STATUS(setRtpPacket(version, padding, extension, csrcCount, marker, payloadType, sequenceNumber, timestamp, ssrc, csrcArray, extensionProfile,
                            extensionLength, extensionPayload, payload, payloadLength, pRtpPacket));

CleanUp:
    if (STATUS_FAILED(retStatus) && pRtpPacket != NULL) {
        freeRtpPacket(&pRtpPacket);
        pRtpPacket = NULL;
    }

    if (pRtpPacket != NULL) {
        *ppRtpPacket = pRtpPacket;
    }
    LEAVES();
    return retStatus;
}

STATUS setRtpPacket(UINT8 version, BOOL padding, BOOL extension, UINT8 csrcCount, BOOL marker, UINT8 payloadType, UINT16 sequenceNumber,
                    UINT32 timestamp, UINT32 ssrc, PUINT32 csrcArray, UINT16 extensionProfile, UINT32 extensionLength, PBYTE extensionPayload,
                    PBYTE payload, UINT32 payloadLength, PRtpPacket pRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pRtpPacket != NULL && (extension == FALSE || extensionPayload != NULL), STATUS_NULL_ARG);

    pRtpPacket->header.version = version;
    pRtpPacket->header.padding = padding;
    pRtpPacket->header.extension = extension;
    pRtpPacket->header.csrcCount = csrcCount;
    pRtpPacket->header.marker = marker;
    pRtpPacket->header.payloadType = payloadType;
    pRtpPacket->header.sequenceNumber = sequenceNumber;
    pRtpPacket->header.timestamp = timestamp;
    pRtpPacket->header.ssrc = ssrc;
    pRtpPacket->header.csrcArray = csrcArray;
    if (extension) {
        pRtpPacket->header.extensionProfile = extensionProfile;
        pRtpPacket->header.extensionPayload = extensionPayload;
        pRtpPacket->header.extensionLength = extensionLength;
    } else {
        pRtpPacket->header.extensionProfile = 0;
        pRtpPacket->header.extensionPayload = NULL;
        pRtpPacket->header.extensionLength = 0;
    }
    pRtpPacket->payload = payload;
    pRtpPacket->payloadLength = payloadLength;
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS freeRtpPacket(PRtpPacket* ppRtpPacket)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppRtpPacket != NULL, STATUS_NULL_ARG);

    if (*ppRtpPacket != NULL) {
        SAFE_MEMFREE((*ppRtpPacket)->pRawPacket);
    }
    SAFE_MEMFREE(*ppRtpPacket);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS createRtpPacketFromBytes(PBYTE rawPacket, UINT32 packetLength, PRtpPacket* ppRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacket pRtpPacket = (PRtpPacket) MEMALLOC(SIZEOF(RtpPacket));
    CHK(pRtpPacket != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pRtpPacket->pRawPacket = rawPacket;
    pRtpPacket->rawPacketLength = packetLength;
    CHK_STATUS(setRtpPacketFromBytes(rawPacket, packetLength, pRtpPacket));

CleanUp:

    if (STATUS_FAILED(retStatus) && pRtpPacket != NULL) {
        freeRtpPacket(&pRtpPacket);
        pRtpPacket = NULL;
    }

    if (ppRtpPacket != NULL) {
        *ppRtpPacket = pRtpPacket;
    }

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS constructRetransmitRtpPacketFromBytes(PBYTE rawPacket, UINT32 packetLength, UINT16 sequenceNum, UINT8 payloadType, UINT32 ssrc,
                                             PRtpPacket* ppRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pPayload = NULL;
    PRtpPacket pRtpPacket = (PRtpPacket) MEMALLOC(SIZEOF(RtpPacket));

    CHK(pRtpPacket != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(setRtpPacketFromBytes(rawPacket, packetLength, pRtpPacket));
    pPayload = (PBYTE) MEMALLOC(pRtpPacket->payloadLength + SIZEOF(UINT16));
    CHK(pPayload != NULL, STATUS_NOT_ENOUGH_MEMORY);
    // Retransmission payload header is OSN original sequence number
    putUnalignedInt16BigEndian((PINT16) pPayload, pRtpPacket->header.sequenceNumber);
    MEMCPY(pPayload + SIZEOF(UINT16), pRtpPacket->payload, pRtpPacket->payloadLength);
    pRtpPacket->payloadLength += SIZEOF(UINT16);
    pRtpPacket->payload = pPayload;

    pRtpPacket->header.sequenceNumber = sequenceNum;
    pRtpPacket->header.ssrc = ssrc;
    pRtpPacket->header.payloadType = payloadType;
    pRtpPacket->header.padding = FALSE;
    pRtpPacket->pRawPacket = NULL;

    CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, NULL, &pRtpPacket->rawPacketLength));
    CHK(NULL != (pRtpPacket->pRawPacket = (PBYTE) MEMALLOC(pRtpPacket->rawPacketLength)), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(createBytesFromRtpPacket(pRtpPacket, pRtpPacket->pRawPacket, &pRtpPacket->rawPacketLength));
    pRtpPacket->payload = pRtpPacket->pRawPacket + RTP_GET_RAW_PACKET_SIZE(pRtpPacket) - pRtpPacket->payloadLength;

CleanUp:
    SAFE_MEMFREE(pPayload);
    if (STATUS_FAILED(retStatus) && pRtpPacket != NULL) {
        SAFE_MEMFREE(pRtpPacket->pRawPacket);
        freeRtpPacket(&pRtpPacket);
        pRtpPacket = NULL;
    }

    if (ppRtpPacket != NULL) {
        *ppRtpPacket = pRtpPacket;
    }
    LEAVES();
    return retStatus;
}

STATUS setRtpPacketFromBytes(PBYTE rawPacket, UINT32 packetLength, PRtpPacket pRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 version;
    BOOL padding;
    BOOL extension;
    UINT8 csrcCount;
    BOOL marker;
    UINT8 payloadType;
    UINT16 sequenceNumber;
    UINT32 timestamp;
    UINT32 ssrc;
    PUINT32 csrcArray = NULL;
    UINT16 extensionProfile = 0;
    UINT16 extensionLength = 0;
    PBYTE extensionPayload = NULL;
    UINT32 currOffset = 0;

    CHK(pRtpPacket != NULL && rawPacket != NULL, STATUS_NULL_ARG);
    CHK(packetLength >= MIN_HEADER_LENGTH, STATUS_RTP_INPUT_PACKET_TOO_SMALL);

    version = (rawPacket[0] >> VERSION_SHIFT) & VERSION_MASK;
    padding = ((rawPacket[0] >> PADDING_SHIFT) & PADDING_MASK) > 0;
    extension = ((rawPacket[0] >> EXTENSION_SHIFT) & EXTENSION_MASK) > 0;
    csrcCount = rawPacket[0] & CSRC_COUNT_MASK;
    marker = ((rawPacket[1] >> MARKER_SHIFT) & MARKER_MASK) > 0;
    payloadType = rawPacket[1] & PAYLOAD_TYPE_MASK;
    sequenceNumber = getInt16(*(PUINT16)(rawPacket + SEQ_NUMBER_OFFSET));
    timestamp = getInt32(*(PUINT32)(rawPacket + TIMESTAMP_OFFSET));
    ssrc = getInt32(*(PUINT32)(rawPacket + SSRC_OFFSET));

    currOffset = CSRC_OFFSET + (csrcCount * CSRC_LENGTH);
    CHK(packetLength >= currOffset, STATUS_RTP_INPUT_PACKET_TOO_SMALL);

    if (csrcCount > 0) {
        csrcArray = (PUINT32)(rawPacket + CSRC_OFFSET);
    }

    if (extension) {
        extensionProfile = getInt16(*(PUINT16)(rawPacket + currOffset));
        currOffset += SIZEOF(UINT16);
        extensionLength = getInt16(*(PUINT16)(rawPacket + currOffset)) * 4;
        currOffset += SIZEOF(UINT16);
        extensionPayload = (PBYTE)(rawPacket + currOffset);
        currOffset += extensionLength;
    }

    CHK_STATUS(setRtpPacket(version, padding, extension, csrcCount, marker, payloadType, sequenceNumber, timestamp, ssrc, csrcArray, extensionProfile,
                            extensionLength, extensionPayload, rawPacket + currOffset, packetLength - currOffset, pRtpPacket));

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS createBytesFromRtpPacket(PRtpPacket pRtpPacket, PBYTE pRawPacket, PUINT32 pPacketLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 packetLength = 0;

    CHK(pRtpPacket != NULL && pPacketLength != NULL, STATUS_NULL_ARG);

    packetLength = RTP_GET_RAW_PACKET_SIZE(pRtpPacket);

    // Check if we are trying to calculate the required size only
    CHK(pRawPacket != NULL, retStatus);

    // Otherwise, check if the specified size is enough
    CHK(*pPacketLength >= packetLength, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(setBytesFromRtpPacket(pRtpPacket, pRawPacket, packetLength));

CleanUp:

    if (pPacketLength != NULL) {
        *pPacketLength = packetLength;
    }

    LEAVES();
    return retStatus;
}

STATUS setBytesFromRtpPacket(PRtpPacket pRtpPacket, PBYTE pRawPacket, UINT32 packetLength)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacketHeader pHeader = &pRtpPacket->header;
    UINT32 packetLengthNeeded = 0;
    PBYTE pCurPtr = pRawPacket;
    UINT8 i;

    CHK(pRtpPacket != NULL && pRawPacket != NULL, STATUS_NULL_ARG);

    packetLengthNeeded = RTP_GET_RAW_PACKET_SIZE(pRtpPacket);
    CHK(packetLength >= packetLengthNeeded, STATUS_BUFFER_TOO_SMALL);
    /*
     *  0                   1                   2                   3
     *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |                           timestamp                           |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |           synchronization source (SSRC) identifier            |
     * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
     * |            contributing source (CSRC) identifiers             |
     * |                             ....                              |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */

    // The first byte contains the version, padding bit, extension bit, and csrc size
    *pCurPtr = ((pHeader->version << VERSION_SHIFT) | pHeader->csrcCount);
    if (pHeader->padding) {
        *pCurPtr |= (1 << PADDING_SHIFT);
    }
    if (pHeader->extension) {
        *pCurPtr |= (1 << EXTENSION_SHIFT);
    }
    pCurPtr++;

    // The second byte contains the marker bit and payload type.
    *pCurPtr = pHeader->payloadType;
    if (pHeader->marker) {
        *pCurPtr |= (1 << MARKER_SHIFT);
    }
    pCurPtr++;

    // https://tools.ietf.org/html/rfc7741#page-5
    // All integer fields in the specifications are encoded as
    //   unsigned integers in network octet order.
    putUnalignedInt16BigEndian(pCurPtr, pHeader->sequenceNumber);
    pCurPtr += SIZEOF(UINT16);

    putUnalignedInt32BigEndian(pCurPtr, pHeader->timestamp);
    pCurPtr += SIZEOF(UINT32);

    putUnalignedInt32BigEndian(pCurPtr, pHeader->ssrc);
    pCurPtr += SIZEOF(UINT32);

    for (i = 0; i < pHeader->csrcCount; i++, pCurPtr += SIZEOF(UINT32)) {
        putUnalignedInt32BigEndian(pCurPtr, pHeader->csrcArray[i]);
    }

    if (pHeader->extension) {
        // the payload must be in 32-bit words.
        CHK((pHeader->extensionLength) % SIZEOF(UINT32) == 0, STATUS_RTP_INVALID_EXTENSION_LEN);

        putUnalignedInt16BigEndian(pCurPtr, pHeader->extensionProfile);
        pCurPtr += SIZEOF(UINT16);
        putUnalignedInt16BigEndian(pCurPtr, pHeader->extensionLength / SIZEOF(UINT32));
        pCurPtr += SIZEOF(UINT16);
        MEMCPY(pCurPtr, pHeader->extensionPayload, pHeader->extensionLength);
        pCurPtr += pHeader->extensionLength;
    }

    if (pRtpPacket->payload != NULL && pRtpPacket->payloadLength > 0) {
        MEMCPY(pCurPtr, pRtpPacket->payload, pRtpPacket->payloadLength);
    }
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS constructRtpPackets(PPayloadArray pPayloadArray, UINT8 payloadType, UINT16 startSequenceNumber, UINT32 timestamp, UINT32 ssrc,
                           PRtpPacket pPackets, UINT32 packetCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 sequenceNumber = startSequenceNumber;
    PBYTE curPtrInPayload = NULL;
    PUINT32 curPtrInPayloadSubLen = NULL;
    UINT32 i = 0;

    CHK(pPayloadArray != NULL && pPayloadArray->payloadLength > 0, retStatus);
    CHK(pPackets != NULL, STATUS_NULL_ARG);
    CHK(pPayloadArray->payloadSubLenSize <= packetCount, STATUS_BUFFER_TOO_SMALL);

    curPtrInPayload = pPayloadArray->payloadBuffer;
    for (i = 0, curPtrInPayloadSubLen = pPayloadArray->payloadSubLength; i < pPayloadArray->payloadSubLenSize; i++, curPtrInPayloadSubLen++) {
        CHK_STATUS(setRtpPacket(2, FALSE, FALSE, 0, i == pPayloadArray->payloadSubLenSize - 1, payloadType, sequenceNumber, timestamp, ssrc, NULL, 0,
                                0, NULL, curPtrInPayload, *curPtrInPayloadSubLen, pPackets + i));

        sequenceNumber = GET_UINT16_SEQ_NUM(sequenceNumber + 1);

        curPtrInPayload += *curPtrInPayloadSubLen;
    }

CleanUp:
    LEAVES();
    return retStatus;
}
