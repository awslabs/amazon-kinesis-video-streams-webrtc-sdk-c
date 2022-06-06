#define LOG_CLASS "RtpRollingBuffer"

#include "../Include_i.h"

STATUS createRtpRollingBuffer(UINT32 capacity, PRtpRollingBuffer* ppRtpRollingBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpRollingBuffer pRtpRollingBuffer = NULL;
    CHK(capacity != 0, STATUS_INVALID_ARG);
    CHK(ppRtpRollingBuffer != NULL, STATUS_NULL_ARG);

    pRtpRollingBuffer = (PRtpRollingBuffer) MEMALLOC(SIZEOF(RtpRollingBuffer));
    CHK(pRtpRollingBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(createRollingBuffer(capacity, freeRtpRollingBufferData, &pRtpRollingBuffer->pRollingBuffer));

CleanUp:
    if (ppRtpRollingBuffer != NULL) {
        *ppRtpRollingBuffer = pRtpRollingBuffer;
    }
    LEAVES();
    return retStatus;
}

STATUS freeRtpRollingBuffer(PRtpRollingBuffer* ppRtpRollingBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppRtpRollingBuffer != NULL, STATUS_NULL_ARG);

    if (*ppRtpRollingBuffer != NULL) {
        freeRollingBuffer(&(*ppRtpRollingBuffer)->pRollingBuffer);
    }
    SAFE_MEMFREE(*ppRtpRollingBuffer);
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS freeRtpRollingBufferData(PUINT64 pData)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pData != NULL, STATUS_NULL_ARG);
    CHK_STATUS(freeRtpPacket((PRtpPacket*) pData));
CleanUp:
    LEAVES();
    return retStatus;
}

STATUS rtpRollingBufferAddRtpPacket(PRtpRollingBuffer pRollingBuffer, PRtpPacket pRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacket pRtpPacketCopy = NULL;
    PBYTE pRawPacketCopy = NULL;
    UINT64 index = 0;
    CHK(pRollingBuffer != NULL && pRtpPacket != NULL, STATUS_NULL_ARG);

    pRawPacketCopy = (PBYTE) MEMALLOC(pRtpPacket->rawPacketLength);
    CHK(pRawPacketCopy != NULL, STATUS_NOT_ENOUGH_MEMORY);
    MEMCPY(pRawPacketCopy, pRtpPacket->pRawPacket, pRtpPacket->rawPacketLength);
    CHK_STATUS(createRtpPacketFromBytes(pRawPacketCopy, pRtpPacket->rawPacketLength, &pRtpPacketCopy));
    // pRtpPacketCopy took ownership of pRawPacketCopy
    pRawPacketCopy = NULL;

    CHK_STATUS(rollingBufferAppendData(pRollingBuffer->pRollingBuffer, (UINT64) pRtpPacketCopy, &index));
    pRollingBuffer->lastIndex = index;

CleanUp:
    SAFE_MEMFREE(pRawPacketCopy);
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS rtpRollingBufferGetValidSeqIndexList(PRtpRollingBuffer pRollingBuffer, PUINT16 pSequenceNumberList, UINT32 sequenceNumberListLen,
                                            PUINT64 pValidSeqIndexList, PUINT32 pValidIndexListLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 index = 0, returnPacketCount = 0;
    UINT16 startSeq, endSeq;
    BOOL crossMaxSeq = FALSE, foundPacket = FALSE;
    PUINT16 pCurSeqPtr;
    PUINT64 pCurSeqIndexListPtr;
    UINT16 seqNum;
    UINT32 size = 0;

    CHK(pRollingBuffer != NULL && pValidSeqIndexList != NULL && pSequenceNumberList != NULL, STATUS_NULL_ARG);

    CHK_STATUS(rollingBufferGetSize(pRollingBuffer->pRollingBuffer, &size));
    // Empty buffer, just return
    CHK(size > 0, retStatus);

    startSeq = GET_UINT16_SEQ_NUM(pRollingBuffer->lastIndex - size + 1);
    endSeq = GET_UINT16_SEQ_NUM(pRollingBuffer->lastIndex);

    if (startSeq >= endSeq) {
        crossMaxSeq = TRUE;
    }

    for (index = 0, pCurSeqPtr = pSequenceNumberList, pCurSeqIndexListPtr = pValidSeqIndexList; index < sequenceNumberListLen;
         index++, pCurSeqPtr++) {
        seqNum = *pCurSeqPtr;
        foundPacket = FALSE;
        if ((!crossMaxSeq && seqNum >= startSeq && seqNum <= endSeq) || (crossMaxSeq && seqNum >= startSeq)) {
            *pCurSeqIndexListPtr = pRollingBuffer->lastIndex - size + 1 + seqNum - startSeq;
            foundPacket = TRUE;
        } else if (crossMaxSeq && seqNum <= endSeq) {
            *pCurSeqIndexListPtr = pRollingBuffer->lastIndex - endSeq + seqNum;
            foundPacket = TRUE;
        }
        if (foundPacket) {
            pCurSeqIndexListPtr++;
            // Return if filled up given valid sequence number array
            CHK(++returnPacketCount < *pValidIndexListLen, retStatus);
            *pCurSeqIndexListPtr = (UINT64) NULL;
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (pValidIndexListLen != NULL) {
        *pValidIndexListLen = returnPacketCount;
    }

    LEAVES();
    return retStatus;
}
