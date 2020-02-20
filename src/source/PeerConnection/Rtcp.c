#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;
    UINT32 currentOffset = 0;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    while (currentOffset < buffLen) {
        CHK_STATUS(setRtcpPacketFromBytes(pBuff + currentOffset, buffLen - currentOffset, &rtcpPacket));

        if (rtcpPacket.header.packetType == RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK && rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK) {
            CHK_STATUS(resendPacketOnNack(&rtcpPacket, pKvsPeerConnection));
        } else if (rtcpPacket.header.packetType == RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK &&
                   rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK &&
                   isRembPacket(rtcpPacket.payload, rtcpPacket.payloadLength) == STATUS_SUCCESS)
        {
            CHK_STATUS(onRtcpRembPacket(&rtcpPacket, pKvsPeerConnection));
        }

        currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    }

CleanUp:
    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}

STATUS onRtcpRembPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 ssrcList[MAX_UINT8] = {0};
    DOUBLE maximumBitRate = 0;
    UINT8 ssrcListLen;
    UINT32 i;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTransceiver = NULL;
    UINT64 item;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    CHK_STATUS(rembValueGet(pRtcpPacket->payload, pRtcpPacket->payloadLength, &maximumBitRate, (PUINT32) &ssrcList, &ssrcListLen));

    for (i = 0; i < ssrcListLen; i++) {
        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
        while(pCurNode != NULL && pTransceiver == NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &item));
            CHK(item != 0, STATUS_INTERNAL_ERROR);

            pTransceiver = (PKvsRtpTransceiver) item;
            if (pTransceiver->sender.ssrc != ssrcList[i]) {
                pTransceiver = NULL;
            }

            pCurNode = pCurNode->pNext;
        }
        CHK_ERR(pTransceiver != NULL, STATUS_RTCP_INPUT_SSRC_INVALID, "Receiving REMB for non existing ssrcs: ssrc %lu", ssrcList[i]);
        if (pTransceiver->onBandwidthEstimation != NULL) {
            pTransceiver->onBandwidthEstimation(pTransceiver->onBandwidthEstimationCustomData, maximumBitRate);
        }
    }

CleanUp:

    return retStatus;
}
