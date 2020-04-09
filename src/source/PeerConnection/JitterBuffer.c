#define LOG_CLASS "JitterBuffer"

#include "../Include_i.h"

STATUS createJitterBuffer(FrameReadyFunc onFrameReadyFunc, FrameDroppedFunc onFrameDroppedFunc,
                          DepayRtpPayloadFunc depayRtpPayloadFunc, UINT32 maxLatency, UINT32 clockRate,
                          UINT64 customData, PJitterBuffer* ppJitterBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PJitterBuffer pJitterBuffer = NULL;

    CHK(ppJitterBuffer != NULL && onFrameReadyFunc != NULL && onFrameDroppedFunc != NULL && depayRtpPayloadFunc != NULL, STATUS_NULL_ARG);
    CHK(clockRate != 0, STATUS_INVALID_ARG);

    pJitterBuffer = (PJitterBuffer) MEMALLOC(SIZEOF(JitterBuffer));
    CHK(pJitterBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pJitterBuffer->onFrameReadyFn = onFrameReadyFunc;
    pJitterBuffer->onFrameDroppedFn = onFrameDroppedFunc;
    pJitterBuffer->depayPayloadFn = depayRtpPayloadFunc;
    pJitterBuffer->clockRate = clockRate;

    MEMSET(pJitterBuffer->pktBuffer, 0, SIZEOF(pJitterBuffer->pktBuffer));

    pJitterBuffer->maxLatency = maxLatency;
    if (pJitterBuffer->maxLatency == 0) {
        pJitterBuffer->maxLatency = DEFAULT_JITTER_BUFFER_MAX_LATENCY;
    }
    pJitterBuffer->maxLatency = pJitterBuffer->maxLatency * pJitterBuffer->clockRate / HUNDREDS_OF_NANOS_IN_A_SECOND;

    pJitterBuffer->lastPushTimestamp = 0;
    pJitterBuffer->lastPopTimestamp = MAX_UINT32;
    pJitterBuffer->lastRemovedTimestamp = 0;
    pJitterBuffer->lastRemovedSequenceNumber = MAX_SEQUENCE_NUM;
    pJitterBuffer->started = FALSE;

    pJitterBuffer->customData = customData;

CleanUp:
    if (STATUS_FAILED(retStatus) && pJitterBuffer != NULL) {
        freeJitterBuffer(&pJitterBuffer);
        pJitterBuffer = NULL;
    }

    if (ppJitterBuffer != NULL) {
        *ppJitterBuffer = pJitterBuffer;
    }

    LEAVES();
    return retStatus;
}

STATUS freeJitterBuffer(PJitterBuffer* ppJitterBuffer)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PJitterBuffer pJitterBuffer = NULL;

    CHK(ppJitterBuffer != NULL, STATUS_NULL_ARG);
    // freeJitterBuffer is idempotent
    CHK(*ppJitterBuffer != NULL, retStatus);

    pJitterBuffer = *ppJitterBuffer;

    jitterBufferPop(pJitterBuffer, TRUE);
    jitterBufferDropBufferData(pJitterBuffer, 0, MAX_SEQUENCE_NUM, 0);

    MEMFREE(*ppJitterBuffer);

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferPush(PJitterBuffer pJitterBuffer, PRtpPacket pRtpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRtpPacket pCurPacket = NULL;

    CHK(pJitterBuffer != NULL && pJitterBuffer->pktBuffer != NULL && pRtpPacket != NULL, STATUS_NULL_ARG);
    CHK(pJitterBuffer->lastRemovedTimestamp < pRtpPacket->header.timestamp, retStatus);

    if (!pJitterBuffer->started || (pJitterBuffer->lastRemovedTimestamp == 0 && pJitterBuffer->lastPopTimestamp == pRtpPacket->header.sequenceNumber
        && pJitterBuffer->lastRemovedSequenceNumber >= pRtpPacket->header.sequenceNumber)) {
        // Set to started and initialize the sequence number
        pJitterBuffer->started = TRUE;
        pJitterBuffer->lastRemovedSequenceNumber = UINT16_DEC(pRtpPacket->header.sequenceNumber);
    }

    if (pJitterBuffer->lastPushTimestamp < pRtpPacket->header.timestamp) {
        pJitterBuffer->lastPushTimestamp = pRtpPacket->header.timestamp;
    }

    if ((pRtpPacket->header.timestamp < pJitterBuffer->maxLatency && pJitterBuffer->lastPushTimestamp <= pJitterBuffer->maxLatency)
        || pRtpPacket->header.timestamp >= pJitterBuffer->lastPushTimestamp - pJitterBuffer->maxLatency) {
        pCurPacket = pJitterBuffer->pktBuffer[pRtpPacket->header.sequenceNumber];
        if (pCurPacket != NULL) {
            freeRtpPacketAndRawPacket(&pCurPacket);
            pJitterBuffer->pktBuffer[pRtpPacket->header.sequenceNumber] = NULL;
        }
        pJitterBuffer->pktBuffer[pRtpPacket->header.sequenceNumber] = pRtpPacket;
        pJitterBuffer->lastPopTimestamp = MIN(pJitterBuffer->lastPopTimestamp, pRtpPacket->header.timestamp);
        DLOGS("jitterBufferPush get packet timestamp %lu seqNum %lu", pRtpPacket->header.timestamp, pRtpPacket->header.sequenceNumber);
    } else {
        // Free the packet if it is out of range, jitter buffer need to own the packet and do free
        freeRtpPacketAndRawPacket(&pRtpPacket);
    }

    CHK_STATUS(jitterBufferPop(pJitterBuffer, FALSE));
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferPop(PJitterBuffer pJitterBuffer, BOOL bufferClosed)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index;
    UINT16 lastIndex;
    UINT32 earliestTimestamp = 0;
    BOOL isFrameDataContinuous = TRUE;
    UINT32 curTimestamp = 0;
    UINT16 startDropIndex = 0;
    UINT32 curFrameSize = 0;
    UINT32 partialFrameSize = 0;
    BOOL isStart = FALSE, containStartForEarliestFrame = FALSE;
    UINT16 lastNonNullIndex = 0;

    CHK(pJitterBuffer != NULL && pJitterBuffer->pktBuffer != NULL && pJitterBuffer->onFrameDroppedFn != NULL && pJitterBuffer->onFrameReadyFn != NULL, STATUS_NULL_ARG);
    CHK(pJitterBuffer->lastPushTimestamp != 0, retStatus);

    if (pJitterBuffer->lastPushTimestamp > pJitterBuffer->maxLatency) {
        earliestTimestamp = pJitterBuffer->lastPushTimestamp - pJitterBuffer->maxLatency;
    }

    lastIndex = pJitterBuffer->lastRemovedSequenceNumber;
    index = pJitterBuffer->lastRemovedSequenceNumber + 1;
    startDropIndex = index;
    for (; index != lastIndex; index++) {
        if (pJitterBuffer->pktBuffer[index] == NULL) {
            isFrameDataContinuous = FALSE;
            CHK(pJitterBuffer->lastPopTimestamp < earliestTimestamp || bufferClosed, retStatus);
        } else {
            lastNonNullIndex = index;
            curTimestamp = pJitterBuffer->pktBuffer[index]->header.timestamp;
            if (curTimestamp != pJitterBuffer->lastPopTimestamp) {
                if (pJitterBuffer->lastPopTimestamp < earliestTimestamp || bufferClosed) {
                    if (containStartForEarliestFrame && isFrameDataContinuous) {
                        // TODO: if switch to curBuffer, need to carefully calculate ptr of UINT16_DEC(index) as it is a circulate buffer
                        CHK_STATUS(pJitterBuffer->onFrameReadyFn(pJitterBuffer->customData, startDropIndex,
                                                                 UINT16_DEC(index), curFrameSize));
                        CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, UINT16_DEC(index),
                                                              curTimestamp));
                        curFrameSize = 0;
                        containStartForEarliestFrame = FALSE;
                    } else {
                        CHK_STATUS(pJitterBuffer->onFrameDroppedFn(pJitterBuffer->customData,
                                                                   pJitterBuffer->lastPopTimestamp));
                        CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, UINT16_DEC(index),
                                                              curTimestamp));
                        curFrameSize = 0;
                        isFrameDataContinuous = TRUE;
                    }
                    startDropIndex = index;
                } else {
                    if (containStartForEarliestFrame) {
                        CHK(!bufferClosed, retStatus);
                        if (isFrameDataContinuous) {
                            // TODO: if switch to curBuffer, need to carefully calculate ptr of UINT16_DEC(index) as it is a circulate buffer
                            CHK_STATUS(pJitterBuffer->onFrameReadyFn(pJitterBuffer->customData, startDropIndex,
                                                                     UINT16_DEC(index), curFrameSize));
                            CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, UINT16_DEC(index),
                                                                  curTimestamp));
                            startDropIndex = index;
                            curFrameSize = 0;
                        }
                        containStartForEarliestFrame = FALSE;
                    }
                }
            }

            CHK_STATUS(pJitterBuffer->depayPayloadFn(pJitterBuffer->pktBuffer[index]->payload,
                                                     pJitterBuffer->pktBuffer[index]->payloadLength, NULL,
                                                     &partialFrameSize, &isStart));
            curFrameSize += partialFrameSize;
            if (isStart && pJitterBuffer->lastPopTimestamp == curTimestamp) {
                containStartForEarliestFrame = TRUE;
            }
        }
    }

    // Deal with last frame
    if (bufferClosed && curFrameSize > 0) {
        curFrameSize = 0;
        for (index = startDropIndex; UINT16_DEC(index) != lastNonNullIndex && pJitterBuffer->pktBuffer[index] != NULL; index++) {
            CHK_STATUS(pJitterBuffer->depayPayloadFn(pJitterBuffer->pktBuffer[index]->payload,
                    pJitterBuffer->pktBuffer[index]->payloadLength, NULL,
                    &partialFrameSize, NULL));
            curFrameSize += partialFrameSize;
        }

        // There is no NULL between startIndex and lastNonNullIndex
        if (UINT16_DEC(index) == lastNonNullIndex) {
            CHK_STATUS(pJitterBuffer->onFrameReadyFn(pJitterBuffer->customData, startDropIndex, lastNonNullIndex, curFrameSize));
            CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, lastNonNullIndex, pJitterBuffer->lastPopTimestamp));
        } else {
            CHK_STATUS(pJitterBuffer->onFrameDroppedFn(pJitterBuffer->customData, pJitterBuffer->lastPopTimestamp));
            CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, lastNonNullIndex, pJitterBuffer->lastPopTimestamp));
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferDropBufferData(PJitterBuffer pJitterBuffer, UINT16 startIndex, UINT16 endIndex, UINT32 nextTimestamp)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index = startIndex;
    PRtpPacket pCurPacket = NULL;

    CHK(pJitterBuffer != NULL && pJitterBuffer->pktBuffer != NULL, STATUS_NULL_ARG);
    for (; UINT16_DEC(index) != endIndex; index++) {
        pCurPacket = pJitterBuffer->pktBuffer[index];
        if (pCurPacket != NULL) {
            freeRtpPacketAndRawPacket(&pCurPacket);
            pJitterBuffer->pktBuffer[index] = NULL;
        }
    }
    pJitterBuffer->lastPopTimestamp = nextTimestamp;
    pJitterBuffer->lastRemovedSequenceNumber = endIndex;

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferFillFrameData(PJitterBuffer pJitterBuffer, PBYTE pFrame, UINT32 frameSize, PUINT32 pFilledSize, UINT16 startIndex, UINT16 endIndex)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index = startIndex;
    PRtpPacket pCurPacket = NULL;
    PBYTE pCurPtrInFrame = pFrame;
    UINT32 remainingFrameSize = frameSize;
    UINT32 partialFrameSize = 0;

    CHK(pJitterBuffer != NULL && pJitterBuffer->pktBuffer != NULL && pFrame != NULL && pFilledSize != NULL, STATUS_NULL_ARG);
    for (; UINT16_DEC(index) != endIndex; index++) {
        pCurPacket = pJitterBuffer->pktBuffer[index];
        CHK(pCurPacket != NULL, STATUS_NULL_ARG);
        partialFrameSize = remainingFrameSize;
        CHK_STATUS(pJitterBuffer->depayPayloadFn(pCurPacket->payload, pCurPacket->payloadLength, pCurPtrInFrame, &partialFrameSize, NULL));
        pCurPtrInFrame += partialFrameSize;
        remainingFrameSize -= partialFrameSize;
    }

CleanUp:
    if (pFilledSize != NULL) {
        *pFilledSize = frameSize - remainingFrameSize;
    }
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}
