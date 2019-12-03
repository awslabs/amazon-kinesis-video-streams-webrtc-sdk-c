#define LOG_CLASS "Retransmitter"

#include "../Include_i.h"

STATUS createRetransmitter(UINT32 seqNumListLen, UINT32 validIndexListLen, PRetransmitter *ppRetransmitter)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PRetransmitter pRetransmitter = MEMALLOC(SIZEOF(Retransmitter) + SIZEOF(UINT16) * seqNumListLen
            + SIZEOF(UINT64) * validIndexListLen);
    CHK(pRetransmitter != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pRetransmitter->sequenceNumberList = (PUINT16) (pRetransmitter + 1);
    pRetransmitter->seqNumListLen = seqNumListLen;
    pRetransmitter->validIndexList = (PUINT64) (pRetransmitter->sequenceNumberList + seqNumListLen);
    pRetransmitter->validIndexListLen = validIndexListLen;

CleanUp:
    if (STATUS_FAILED(retStatus) && pRetransmitter != NULL) {
        freeRetransmitter(&pRetransmitter);
        pRetransmitter = NULL;
    }
    if (ppRetransmitter != NULL) {
        *ppRetransmitter = pRetransmitter;
    }
    LEAVES();
    return retStatus;
}

STATUS freeRetransmitter(PRetransmitter* ppRetransmitter)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppRetransmitter != NULL, STATUS_NULL_ARG);
    SAFE_MEMFREE(*ppRetransmitter);
CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS resendPacketOnNack(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    UINT32 senderSsrc = 0, receiverSsrc = 0;
    UINT32 filledLen = 0, validIndexListLen = 0;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTransceiver, pReceiverTransceiver = NULL, pSenderTranceiver = NULL;
    UINT64 item;
    UINT32 index;
    PRtpPacket pRtpPacket = NULL, pRtxRtpPacket = NULL;
    PRetransmitter pRetransmitter = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    CHK_STATUS(rtcpNackListGetSsrcs(pRtcpPacket->payload, pRtcpPacket->payloadLength, &senderSsrc, &receiverSsrc));

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while(pCurNode != NULL && (pReceiverTransceiver == NULL || pSenderTranceiver == NULL)) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &item));
        pTransceiver = (PKvsRtpTransceiver) item;

        if (pTransceiver->jitterBufferSsrc == senderSsrc) {
            pReceiverTransceiver = pTransceiver;
        }

        if (pTransceiver->sender.ssrc == receiverSsrc) {
            pSenderTranceiver = pTransceiver;
            pRetransmitter = pSenderTranceiver->sender.retransmitter;
        }
        pCurNode = pCurNode->pNext;
    }

    CHK_ERR(pReceiverTransceiver != NULL && pSenderTranceiver != NULL, STATUS_RTCP_INPUT_SSRC_INVALID,
            "Receiving NACK for non existing ssrcs: senderSsrc %lu receiverSsrc %lu", senderSsrc, receiverSsrc);

    filledLen = pRetransmitter->seqNumListLen;
    CHK_STATUS(rtcpNackListGetSeqNums(pRtcpPacket->payload, pRtcpPacket->payloadLength, pRetransmitter->sequenceNumberList, &filledLen));
    validIndexListLen = pRetransmitter->validIndexListLen;
    CHK_STATUS(rtpRollingBufferGetValidSeqIndexList(pSenderTranceiver->sender.packetBuffer,
                                                    pRetransmitter->sequenceNumberList,
                                                    &filledLen, pRetransmitter->validIndexList, &validIndexListLen));
    for (index = 0; index < validIndexListLen; index++) {
        retStatus = rollingBufferExtractData(pSenderTranceiver->sender.packetBuffer, pRetransmitter->validIndexList[index], &item);
        pRtpPacket = (PRtpPacket) item;
        CHK(retStatus == STATUS_SUCCESS, retStatus);

        if (pRtpPacket != NULL) {
            CHK_STATUS(constructRetransmitRtpPacketFromBytes(pRtpPacket->pRawPacket, pRtpPacket->rawPacketLength,
                    pSenderTranceiver->sender.rtxSequenceNumber, pSenderTranceiver->sender.rtxPayloadType, pSenderTranceiver->sender.rtxSsrc, &pRtxRtpPacket));
            pSenderTranceiver->sender.rtxSequenceNumber++;
            // resendPacket
            retStatus = writeRtpPacket(pKvsPeerConnection, pRtxRtpPacket);
            if (STATUS_SUCCEEDED(retStatus)) {
                DLOGV("Resent packet original seq %lu rtx seq %lu succeeded", pRtpPacket->header.sequenceNumber,
                        pRtxRtpPacket->header.sequenceNumber);
            } else {
                DLOGW("Resent packet %lu failed with orignal seq %lu new seq %lu status %lu",
                        pRtpPacket->header.sequenceNumber, pRtxRtpPacket->header.sequenceNumber, retStatus);
            }
            // putBackPacketToRollingBuffer
            retStatus = rollingBufferInsertData(pSenderTranceiver->sender.packetBuffer, pRetransmitter->sequenceNumberList[index], item);
            CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_ROLLING_BUFFER_NOT_IN_RANGE, retStatus);

            // free the packet if it is not in the valid range any more
            if (retStatus == STATUS_ROLLING_BUFFER_NOT_IN_RANGE) {
                DLOGS("Retransmit STATUS_ROLLING_BUFFER_NOT_IN_RANGE free %lu by self", pRtpPacket->header.sequenceNumber);
                freeRtpPacket(&pRtpPacket);
                retStatus = STATUS_SUCCESS;
            } else {
                DLOGS("Retransmit add back to rolling %lu", pRtpPacket->header.sequenceNumber);
            }

            freeRtpPacketAndRawPacket(&pRtxRtpPacket);
            pRtpPacket = NULL;
        }
    }
CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pRtpPacket != NULL) {
        // free the packet as it is not put back into rolling buffer
        freeRtpPacket(&pRtpPacket);
        pRtpPacket = NULL;
    }
    freeRtpPacketAndRawPacket(&pRtpPacket);

    LEAVES();
    return retStatus;
}
