#define LOG_CLASS "JitterBuffer"

#include "../Include_i.h"

//Applies only to the case where the very first frame has its first packets out of order
#define MAX_OUT_OF_ORDER_PACKET_DIFFERENCE      512

//forward declaration
STATUS jitterBufferInternalParse(PJitterBuffer pJitterBuffer, BOOL bufferClosed);

STATUS createJitterBuffer(FrameReadyFunc onFrameReadyFunc, FrameDroppedFunc onFrameDroppedFunc, DepayRtpPayloadFunc depayRtpPayloadFunc,
                          UINT32 maxLatency, UINT32 clockRate, UINT64 customData, PJitterBuffer* ppJitterBuffer)
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

    pJitterBuffer->maxLatency = maxLatency;
    if (pJitterBuffer->maxLatency == 0) {
        pJitterBuffer->maxLatency = DEFAULT_JITTER_BUFFER_MAX_LATENCY;
    }
    pJitterBuffer->maxLatency = pJitterBuffer->maxLatency * pJitterBuffer->clockRate / HUNDREDS_OF_NANOS_IN_A_SECOND;

    pJitterBuffer->lastPushTimestamp = 0;
    pJitterBuffer->headTimestamp = MAX_UINT32;
    pJitterBuffer->headSequenceNumber = MAX_SEQUENCE_NUM;
    pJitterBuffer->started = FALSE;
    pJitterBuffer->firstFrameProcessed = FALSE;

    pJitterBuffer->customData = customData;
    CHK_STATUS(hashTableCreateWithParams(JITTER_BUFFER_HASH_TABLE_BUCKET_COUNT, JITTER_BUFFER_HASH_TABLE_BUCKET_LENGTH,
                                         &pJitterBuffer->pPkgBufferHashTable));

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

    jitterBufferInternalParse(pJitterBuffer, TRUE);
    jitterBufferDropBufferData(pJitterBuffer, 0, MAX_SEQUENCE_NUM, 0);
    hashTableFree(pJitterBuffer->pPkgBufferHashTable);

    SAFE_MEMFREE(*ppJitterBuffer);

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferPush(PJitterBuffer pJitterBuffer, PRtpPacket pRtpPacket, PBOOL pPacketDiscarded)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS, status = STATUS_SUCCESS;
    UINT64 hashValue = 0;
    PRtpPacket pCurPacket = NULL;

    CHK(pJitterBuffer != NULL && pRtpPacket != NULL, STATUS_NULL_ARG);

    if (!pJitterBuffer->started) {
        // Set to started and initialize the sequence number
        pJitterBuffer->started = TRUE;
        pJitterBuffer->headSequenceNumber = pRtpPacket->header.sequenceNumber;
        pJitterBuffer->headTimestamp = pRtpPacket->header.timestamp;
    }

    if (pJitterBuffer->lastPushTimestamp < pRtpPacket->header.timestamp) {
        pJitterBuffer->lastPushTimestamp = pRtpPacket->header.timestamp;
    }

    //is the packet within the accepted latency range, if so, add it to the hashtable
    if ((pRtpPacket->header.timestamp < pJitterBuffer->maxLatency && pJitterBuffer->lastPushTimestamp <= pJitterBuffer->maxLatency) ||
        pRtpPacket->header.timestamp >= pJitterBuffer->lastPushTimestamp - pJitterBuffer->maxLatency) {

        status = hashTableGet(pJitterBuffer->pPkgBufferHashTable, pRtpPacket->header.sequenceNumber, &hashValue);
        pCurPacket = (PRtpPacket) hashValue;
        if (STATUS_SUCCEEDED(status) && pCurPacket != NULL) {
            freeRtpPacket(&pCurPacket);
            CHK_STATUS(hashTableRemove(pJitterBuffer->pPkgBufferHashTable, pRtpPacket->header.sequenceNumber));
        }

        CHK_STATUS(hashTablePut(pJitterBuffer->pPkgBufferHashTable, pRtpPacket->header.sequenceNumber, (UINT64) pRtpPacket));

        /*If we haven't yet processed a frame yet, then we don't have a definitive way of knowing if
         *the first packet we receive is actually the earliest packet we'll ever receive. Since sequence numbers
         *can start anywhere from 0 - 65535, we need to incorporate some checks to determine if a newly received packet
         *should be considered the new head. Part of how we determine this is by setting a limit to how many packets off we allow
         *this out of order case to be. Without setting a limit, then we could run into an odd scenario.
         * Example:
         * Push->Packet->SeqNumber == 0. //FIRST PACKET! new head of buffer!
         * Push->Packet->SeqNumber == 3. //... new head of 65532 packet sized frame? maybe? was 0 the tail?
         *
         * To resolve that insanity we set a MAX, and will use that MAX for the range.
         *
         *After the first frame has been processed we don't need or want to make this consideration, since if our parser has
         *dropped a frame for a good reason then we want to ignore any packets from that dropped frame that may come later.
         */
        if (!(pJitterBuffer->firstFrameProcessed)) {
            //if the timestamp is less, we'll accept it as a new head, since it must be an earlier frame.
            if (pRtpPacket->header.timestamp < pJitterBuffer->headTimestamp) {
                pJitterBuffer->headSequenceNumber = pRtpPacket->header.sequenceNumber;
                pJitterBuffer->headTimestamp = pRtpPacket->header.timestamp;
            }
            //timestamp is equal, we're in the same frame.
            else if (pRtpPacket->header.timestamp == pJitterBuffer->headTimestamp) {
                if(pJitterBuffer->headSequenceNumber < MAX_OUT_OF_ORDER_PACKET_DIFFERENCE) {
                    if ((pRtpPacket->header.sequenceNumber >= (MAX_UINT16 - (MAX_OUT_OF_ORDER_PACKET_DIFFERENCE - pJitterBuffer->headSequenceNumber))) ||
                       (pJitterBuffer->headSequenceNumber > pRtpPacket->header.sequenceNumber)) {
                        pJitterBuffer->headSequenceNumber = pRtpPacket->header.sequenceNumber;
                    }
                }
                else if ((pRtpPacket->header.sequenceNumber >= pJitterBuffer->headSequenceNumber - MAX_OUT_OF_ORDER_PACKET_DIFFERENCE) && 
                        (pRtpPacket->header.sequenceNumber < pJitterBuffer->headSequenceNumber) {
                    pJitterBuffer->headSequenceNumber = pRtpPacket->header.sequenceNumber;
                }
            }
        }
        //DONE with considering the head.

        DLOGS("jitterBufferPush get packet timestamp %lu seqNum %lu", pRtpPacket->header.timestamp, pRtpPacket->header.sequenceNumber);
    } else {
        // Free the packet if it is out of range, jitter buffer need to own the packet and do free
        freeRtpPacket(&pRtpPacket);
        if (pPacketDiscarded != NULL) {
            *pPacketDiscarded = TRUE;
        }
    }

    CHK_STATUS(jitterBufferInternalParse(pJitterBuffer, FALSE));

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS jitterBufferInternalParse(PJitterBuffer pJitterBuffer, BOOL bufferClosed)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index;
    UINT16 lastIndex;
    UINT32 earliestAllowedTimestamp = 0;
    BOOL isFrameDataContinuous = TRUE;
    UINT32 curTimestamp = 0;
    UINT16 startDropIndex = 0;
    UINT32 curFrameSize = 0;
    UINT32 partialFrameSize = 0;
    UINT64 hashValue = 0;
    BOOL isStart = FALSE, containStartForEarliestFrame = FALSE, hasEntry = FALSE;
    UINT16 lastNonNullIndex = 0;
    PRtpPacket pCurPacket = NULL;

    CHK(pJitterBuffer != NULL && pJitterBuffer->onFrameDroppedFn != NULL && pJitterBuffer->onFrameReadyFn != NULL, STATUS_NULL_ARG);
    CHK(pJitterBuffer->lastPushTimestamp != 0, retStatus);

    if (pJitterBuffer->lastPushTimestamp > pJitterBuffer->maxLatency) {
        earliestAllowedTimestamp = pJitterBuffer->lastPushTimestamp - pJitterBuffer->maxLatency;
    }

    lastIndex = pJitterBuffer->headSequenceNumber - 1;
    index = pJitterBuffer->headSequenceNumber;
    startDropIndex = index;
    //Loop through entire buffer to find complete frames.
    /*A Frame is ready when these conditions are met:
     * 1. We have a starting packet 
     * 2. There were no missing sequence numbers up to this point
     * 3. A different timestamp in a sequential packet was found
     * 4. There are no earlier frames still in the buffer
     *
     *A Frame is dropped when the above conditions are not met, and the following conditions have been:
     * 1. the buffer is being closed
     * 2. The time between the most recently pushed RTP packet and oldest stored packet has surpassed the
     *    maximum allowed latency
     *
     *The buffer is parsed in order of sequence numbers. It is important to note that if the Frame ready
     *conditions have been met from dropping an earlier frame, then it will be processed.
     *
    */
    for (; index != lastIndex; index++) {
        CHK_STATUS(hashTableContains(pJitterBuffer->pPkgBufferHashTable, index, &hasEntry));
        if (!hasEntry) {
            isFrameDataContinuous = FALSE;
            //if the max latency has not been reached, or the buffer is not being closed, exit parse when a missing entry is found
            CHK(pJitterBuffer->headTimestamp < earliestAllowedTimestamp || bufferClosed, retStatus);
        } else {

            lastNonNullIndex = index;
            retStatus = hashTableGet(pJitterBuffer->pPkgBufferHashTable, index, &hashValue);
            pCurPacket = (PRtpPacket) hashValue;
            if (retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT) {
                retStatus = STATUS_SUCCESS;
            } else {
                CHK(FALSE, retStatus);
            }
            curTimestamp = pCurPacket->header.timestamp;
            //new timestamp on an RTP packet means new frame
            if (curTimestamp != pJitterBuffer->headTimestamp) {
                //was previous frame complete? Deliver it
                if (containStartForEarliestFrame && isFrameDataContinuous)
                {
                    //Decrement the index because this is an inclusive end parser, and we don't want to include the current index in the processed frame.
                    CHK_STATUS(pJitterBuffer->onFrameReadyFn(pJitterBuffer->customData, startDropIndex, UINT16_DEC(index), curFrameSize));
                    CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, UINT16_DEC(index), curTimestamp));
                    pJitterBuffer->firstFrameProcessed = TRUE;
                    startDropIndex = index;
                    containStartForEarliestFrame = FALSE;
                }
                //are we force clearing out the buffer? if so drop the contents of incomplete frame
                else if (pJitterBuffer->headTimestamp < earliestAllowedTimestamp || bufferClosed) {
                    CHK_STATUS(pJitterBuffer->onFrameDroppedFn(pJitterBuffer->customData, startDropIndex, UINT16_DEC(index),
                                                               pJitterBuffer->headTimestamp));
                    CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, UINT16_DEC(index), curTimestamp));
                    pJitterBuffer->firstFrameProcessed = TRUE;
                    isFrameDataContinuous = TRUE;
                    startDropIndex = index;
                }
                else
                {
                    //if you're here, it means we're not force clearing the buffer, and the previous frame must be missing its starting packet.
                    //The starting packet isn't going to be found at an incremental sequence number, so we can save some time and break here.
                    break;
                }
                //new timestamp means new frame, drop tracking for previous frame size
                curFrameSize = 0;
            }

            //With the missing output buffer parameter, this will only return the size of the packet, and identify if it is a starting packet of a frame
            CHK_STATUS(pJitterBuffer->depayPayloadFn(pCurPacket->payload, pCurPacket->payloadLength, NULL, &partialFrameSize, &isStart));
            curFrameSize += partialFrameSize;
            if (isStart && pJitterBuffer->headTimestamp == curTimestamp) {
                containStartForEarliestFrame = TRUE;
            }
        }
    }

    // Deal with last frame, we're force clearing the entire buffer.
    if (bufferClosed && curFrameSize > 0) {
        curFrameSize = 0;
        hasEntry = TRUE;
        for (index = startDropIndex; UINT16_DEC(index) != lastNonNullIndex && hasEntry; index++) {
            CHK_STATUS(hashTableContains(pJitterBuffer->pPkgBufferHashTable, index, &hasEntry));
            if (hasEntry) {
                CHK_STATUS(hashTableGet(pJitterBuffer->pPkgBufferHashTable, index, &hashValue));
                pCurPacket = (PRtpPacket) hashValue;
                CHK_STATUS(pJitterBuffer->depayPayloadFn(pCurPacket->payload, pCurPacket->payloadLength, NULL, &partialFrameSize, NULL));
                curFrameSize += partialFrameSize;
            }
        }

        // There is no NULL between startIndex and lastNonNullIndex
        if (UINT16_DEC(index) == lastNonNullIndex) {
            CHK_STATUS(pJitterBuffer->onFrameReadyFn(pJitterBuffer->customData, startDropIndex, lastNonNullIndex, curFrameSize));
            CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, lastNonNullIndex, pJitterBuffer->headTimestamp));
        } else {
            CHK_STATUS(
                pJitterBuffer->onFrameDroppedFn(pJitterBuffer->customData, startDropIndex, UINT16_DEC(index), pJitterBuffer->headTimestamp));
            CHK_STATUS(jitterBufferDropBufferData(pJitterBuffer, startDropIndex, lastNonNullIndex, pJitterBuffer->headTimestamp));
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}


//Remove all packets containing sequence numbers between and including the startIndex and endIndex for the JitterBuffer.
//The nextTimestamp is assumed to be the timestamp of the next earliest Frame
STATUS jitterBufferDropBufferData(PJitterBuffer pJitterBuffer, UINT16 startIndex, UINT16 endIndex, UINT32 nextTimestamp)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index = startIndex;
    UINT64 hashValue;
    PRtpPacket pCurPacket = NULL;
    BOOL hasEntry = FALSE;

    CHK(pJitterBuffer != NULL, STATUS_NULL_ARG);
    for (; UINT16_DEC(index) != endIndex; index++) {
        CHK_STATUS(hashTableContains(pJitterBuffer->pPkgBufferHashTable, index, &hasEntry));
        if (hasEntry) {
            CHK_STATUS(hashTableGet(pJitterBuffer->pPkgBufferHashTable, index, &hashValue));
            pCurPacket = (PRtpPacket) hashValue;
            freeRtpPacket(&pCurPacket);
            CHK_STATUS(hashTableRemove(pJitterBuffer->pPkgBufferHashTable, index));
        }
    }
    pJitterBuffer->headTimestamp = nextTimestamp;
    pJitterBuffer->headSequenceNumber = endIndex + 1;

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

//Depay all packets containing sequence numbers between and including the startIndex and endIndex for the JitterBuffer.
STATUS jitterBufferFillFrameData(PJitterBuffer pJitterBuffer, PBYTE pFrame, UINT32 frameSize, PUINT32 pFilledSize, UINT16 startIndex, UINT16 endIndex)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 index = startIndex;
    UINT64 hashValue;
    PRtpPacket pCurPacket = NULL;
    PBYTE pCurPtrInFrame = pFrame;
    UINT32 remainingFrameSize = frameSize;
    UINT32 partialFrameSize = 0;

    CHK(pJitterBuffer != NULL && pFrame != NULL && pFilledSize != NULL, STATUS_NULL_ARG);
    for (; UINT16_DEC(index) != endIndex; index++) {
        hashValue = 0;
        retStatus = hashTableGet(pJitterBuffer->pPkgBufferHashTable, index, &hashValue);
        pCurPacket = (PRtpPacket) hashValue;
        if (retStatus == STATUS_SUCCESS || retStatus == STATUS_HASH_KEY_NOT_PRESENT) {
            retStatus = STATUS_SUCCESS;
        } else {
            CHK(FALSE, retStatus);
        }
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
