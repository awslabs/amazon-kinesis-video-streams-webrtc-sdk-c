#define LOG_CLASS "RtpRollingBuffer"

#include "../Include_i.h"

STATUS createRtpRollingBuffer(UINT32 capacity, PRollingBuffer* ppRollingBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHK_STATUS(createRollingBuffer(capacity, freeRtpRollingBufferData, ppRollingBuffer));

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS freeRtpRollingBuffer(PRollingBuffer *ppRollingBuffer)
{
    return freeRollingBuffer(ppRollingBuffer);
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

STATUS addRtpPacket(PRollingBuffer pRollingBuffer, PRtpPacket pRtpPacket)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacket pRtpPacketCopy = NULL;
    CHK(pRollingBuffer != NULL && pRtpPacket != NULL, STATUS_NULL_ARG);

    CHK_STATUS(createRtpPacketFromBytes(pRtpPacket->pRawPacket, pRtpPacket->rawPacketLength, &pRtpPacketCopy));

    if (pRollingBuffer->headIndex == pRollingBuffer->tailIndex) {
        // Empty buffer, set start to same as seq number
        pRollingBuffer->tailIndex = pRtpPacket->header.sequenceNumber;
        pRollingBuffer->headIndex = pRtpPacket->header.sequenceNumber;
    } else {
        CHK(pRollingBuffer->headIndex % MAX_UINT16 == pRtpPacketCopy->header.sequenceNumber, STATUS_INVALID_ARG);
    }
    CHK_STATUS(rollingBufferAppendData(pRollingBuffer, (UINT64) pRtpPacketCopy));
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS getValidSeqIndexList(PRollingBuffer pRollingBuffer, PUINT16 pSequenceNumberList,
                            PUINT32 pSequenceNumberListLen, PUINT64 pValidSeqIndexList, UINT32 validSeqListLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 index = 0, returnPacketCount = 0;
    UINT16 startSeq, endSeq;
    BOOL crossMaxSeq = FALSE, foundPacket = FALSE;
    PUINT16 pCurSeqPtr;
    PUINT64 pCurSeqIndexListPtr;
    UINT16 seqNum;

    CHK(pRollingBuffer != NULL && pValidSeqIndexList != NULL && pSequenceNumberList != NULL && pSequenceNumberListLen != NULL, STATUS_NULL_ARG);

    // Empty buffer, just return
    CHK(pRollingBuffer->headIndex == pRollingBuffer->tailIndex, retStatus);

    startSeq = pRollingBuffer->tailIndex % MAX_UINT16;
    endSeq = (pRollingBuffer->headIndex - 1) % MAX_UINT16;

    if (startSeq >= endSeq) {
        crossMaxSeq = TRUE;
    }

    for (index = 0, pCurSeqPtr = pSequenceNumberList, pCurSeqIndexListPtr = pValidSeqIndexList; index < *pSequenceNumberListLen; index++, pCurSeqPtr++) {
        seqNum = *pCurSeqPtr;
        foundPacket = FALSE;
        if ((!crossMaxSeq && seqNum >= startSeq && seqNum <= endSeq) || (crossMaxSeq && seqNum >= startSeq)) {
            *pCurSeqIndexListPtr = pRollingBuffer->tailIndex + seqNum - startSeq;
            foundPacket = TRUE;
        } else if (crossMaxSeq && seqNum <= endSeq) {
            *pCurSeqIndexListPtr = pRollingBuffer->headIndex - 1 + endSeq - seqNum;
            foundPacket = TRUE;
        }
        if (foundPacket) {
            pCurSeqIndexListPtr++;
            // Return if filled up given valid sequence number array
            CHK(++returnPacketCount < validSeqListLen, retStatus);
            *pCurSeqIndexListPtr = (UINT64) NULL;
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
