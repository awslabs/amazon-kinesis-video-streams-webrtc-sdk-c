#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;
    UINT32 currentOffset = 0;
    UINT64 currentTimeNTP = convertTimestampToNTP(GETTIME());

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    while (currentOffset < buffLen) {
        CHK_STATUS(setRtcpPacketFromBytes(pBuff + currentOffset, buffLen - currentOffset, &rtcpPacket));

        switch (rtcpPacket.header.packetType) {
            case RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK:
                if (rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK)
                    CHK_STATUS(resendPacketOnNack(&rtcpPacket, pKvsPeerConnection));
                break;
            case RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK:
                if (rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK &&
                    isRembPacket(rtcpPacket.payload, rtcpPacket.payloadLength) == STATUS_SUCCESS) {
                    CHK_STATUS(onRtcpRembPacket(&rtcpPacket, pKvsPeerConnection));
                } else if (rtcpPacket.header.receptionReportCount == RTCP_PSFB_PLI) {
                    CHK_STATUS(onRtcpPLIPacket(&rtcpPacket, pKvsPeerConnection));
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK");
                }
                break;
            case RTCP_PACKET_TYPE_SENDER_REPORT: {
                // https://tools.ietf.org/html/rfc3550#section-6.4.1
                UINT32 x = rtcpPacket.payloadLength;
                if (x == 24) {
                    INT32 senderSSRC = getUnalignedInt32BigEndian(rtcpPacket.payload);
                    UINT32 ntpHi = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 4);
                    UINT32 ntpLo = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 8);
                    UINT32 rtpTs = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 12);
                    UINT32 packetCnt = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 16);
                    UINT32 octetCnt = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 20);
                    DLOGD("RTCP_PACKET_TYPE_SENDER_REPORT %d %u %u rtpTs: %u %u pkts %u bytes", senderSSRC, ntpHi, ntpLo, rtpTs, packetCnt, octetCnt);
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_SENDER_REPORT size %d", x);
                }

                break;
            }
            case RTCP_PACKET_TYPE_RECEIVER_REPORT: {
                // https://tools.ietf.org/html/rfc3550#section-6.4.2
                UINT32 x = rtcpPacket.payloadLength;
                if (x == 28) {
                    INT32 senderSSRC = getUnalignedInt32BigEndian(rtcpPacket.payload);
                    UINT32 ssrc1 = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 4);
                    UINT8 fractionLost = rtcpPacket.payload[8];
                    UINT32 cumulativeLost = ((UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 8)) & 0x00ffffffu;
                    UINT32 extendedHighestSeqNumberReceived = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 12);
                    UINT32 interarrivalJitter = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 16);
                    UINT32 lastSR = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 20);
                    UINT32 delaySinceLastSR = (UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 24);
                    DLOGD("RTCP_PACKET_TYPE_RECEIVER_REPORT %u %u loss: %u %u seq: %u jit: %u lsr: %u dlsr: %u", senderSSRC, ssrc1, fractionLost,
                          cumulativeLost, extendedHighestSeqNumberReceived, interarrivalJitter, lastSR, delaySinceLastSR);
                    if (lastSR != 0) {
                        // https://tools.ietf.org/html/rfc3550#section-6.4.1
                        //      Source SSRC_n can compute the round-trip propagation delay to
                        //      SSRC_r by recording the time A when this reception report block is
                        //      received.  It calculates the total round-trip time A-LSR using the
                        //      last SR timestamp (LSR) field, and then subtracting this field to
                        //      leave the round-trip propagation delay as (A - LSR - DLSR).
                        UINT32 A = MID_NTP(currentTimeNTP);
                        UINT32 rttPropDelay = A - lastSR - delaySinceLastSR;
                        UINT32 rttPropDelayMsec = CONVERT_TIMESCALE(rttPropDelay, DLSR_TIMESCALE, 1000);
                        DLOGD("RTCP_PACKET_TYPE_RECEIVER_REPORT rttPropDelay %u msec", rttPropDelayMsec);
                    }
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_RECEIVER_REPORT size %d", x);
                }
                break;
            }
            case RTCP_PACKET_TYPE_SOURCE_DESCRIPTION:
                DLOGI("unhandled packet type RTCP_PACKET_TYPE_SOURCE_DESCRIPTION");
                break;
            default:
                DLOGW("unhandled packet type %d", rtcpPacket.header.packetType);
                // unhandled packet type
                break;
        }

        currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

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
        while (pCurNode != NULL && pTransceiver == NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &item));
            CHK(item != 0, STATUS_INTERNAL_ERROR);

            pTransceiver = (PKvsRtpTransceiver) item;
            if (pTransceiver->sender.ssrc != ssrcList[i] && pTransceiver->sender.rtxSsrc != ssrcList[i]) {
                pTransceiver = NULL;
            }

            pCurNode = pCurNode->pNext;
        }

        CHK_ERR(pTransceiver != NULL, STATUS_RTCP_INPUT_SSRC_INVALID, "Received REMB for non existing ssrcs: ssrc %lu", ssrcList[i]);
        if (pTransceiver->onBandwidthEstimation != NULL) {
            pTransceiver->onBandwidthEstimation(pTransceiver->onBandwidthEstimationCustomData, maximumBitRate);
        }
    }

CleanUp:

    return retStatus;
}

STATUS onRtcpPLIPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 mediaSSRC = 0;
    PDoubleListNode pCurNode = NULL;
    PKvsRtpTransceiver pTransceiver = NULL;
    UINT64 item;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    mediaSSRC = getUnalignedInt32BigEndian((pRtcpPacket->payload + (SIZEOF(UINT32))));

    CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceievers, &pCurNode));
    while (pCurNode != NULL && pTransceiver == NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &item));
        CHK(item != 0, STATUS_INTERNAL_ERROR);

        pTransceiver = (PKvsRtpTransceiver) item;
        if (pTransceiver->sender.ssrc != mediaSSRC && pTransceiver->sender.rtxSsrc != mediaSSRC) {
            pTransceiver = NULL;
        }

        pCurNode = pCurNode->pNext;
    }

    CHK_ERR(pTransceiver != NULL, STATUS_RTCP_INPUT_SSRC_INVALID, "Received PLI for non existing ssrcs: ssrc %lu", mediaSSRC);
    if (pTransceiver->onPictureLoss != NULL) {
        pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
    }

CleanUp:

    return retStatus;
}
