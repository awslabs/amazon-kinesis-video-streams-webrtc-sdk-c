#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;
    UINT8 fractionLost;
    UINT32 senderSSRC, rtpTs, packetCnt, octetCnt, ssrc1, cumulativeLost, extHiSeqNumReceived, interarrivalJitter, lastSR, delaySinceLastSR;
    UINT32 rttPropDelay, rttPropDelayMsec;
    UINT64 ntpTime;
    UINT64 currentTimeNTP = convertTimestampToNTP(GETTIME());
    UINT32 currentOffset  = 0;

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
                if (rtcpPacket.payloadLength == 24) {
                    senderSSRC = getUnalignedInt32BigEndian(rtcpPacket.payload);
                    ntpTime    = getUnalignedInt64BigEndian(rtcpPacket.payload + 4);
                    rtpTs      = getUnalignedInt32BigEndian(rtcpPacket.payload + 12);
                    packetCnt  = getUnalignedInt32BigEndian(rtcpPacket.payload + 16);
                    octetCnt   = getUnalignedInt32BigEndian(rtcpPacket.payload + 20);
                    DLOGD("RTCP_PACKET_TYPE_SENDER_REPORT %d " PRIu64 " rtpTs: %u %u pkts %u bytes", senderSSRC, ntpTime, rtpTs, packetCnt, octetCnt);
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_SENDER_REPORT size %d", rtcpPacket.payloadLength);
                }

                break;
            }
            case RTCP_PACKET_TYPE_RECEIVER_REPORT: {
                // https://tools.ietf.org/html/rfc3550#section-6.4.2
                if (rtcpPacket.payloadLength == 28) {
                    senderSSRC          = getUnalignedInt32BigEndian(rtcpPacket.payload);
                    ssrc1               = getUnalignedInt32BigEndian(rtcpPacket.payload + 4);
                    fractionLost        = rtcpPacket.payload[8];
                    cumulativeLost      = ((UINT32) getUnalignedInt32BigEndian(rtcpPacket.payload + 8)) & 0x00ffffffu;
                    extHiSeqNumReceived = getUnalignedInt32BigEndian(rtcpPacket.payload + 12);
                    interarrivalJitter  = getUnalignedInt32BigEndian(rtcpPacket.payload + 16);
                    lastSR              = getUnalignedInt32BigEndian(rtcpPacket.payload + 20);
                    delaySinceLastSR    = getUnalignedInt32BigEndian(rtcpPacket.payload + 24);
                    DLOGD("RTCP_PACKET_TYPE_RECEIVER_REPORT %u %u loss: %u %u seq: %u jit: %u lsr: %u dlsr: %u", senderSSRC, ssrc1, fractionLost,
                          cumulativeLost, extHiSeqNumReceived, interarrivalJitter, lastSR, delaySinceLastSR);
                    if (lastSR != 0) {
                        // https://tools.ietf.org/html/rfc3550#section-6.4.1
                        //      Source SSRC_n can compute the round-trip propagation delay to
                        //      SSRC_r by recording the time A when this reception report block is
                        //      received.  It calculates the total round-trip time A-LSR using the
                        //      last SR timestamp (LSR) field, and then subtracting this field to
                        //      leave the round-trip propagation delay as (A - LSR - DLSR).
                        rttPropDelay     = MID_NTP(currentTimeNTP) - lastSR - delaySinceLastSR;
                        rttPropDelayMsec = CONVERT_TIMESCALE(rttPropDelay, DLSR_TIMESCALE, 1000);
                        DLOGD("RTCP_PACKET_TYPE_RECEIVER_REPORT rttPropDelay %u msec", rttPropDelayMsec);
                    }
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_RECEIVER_REPORT size %d", rtcpPacket.payloadLength);
                }
                break;
            }
            case RTCP_PACKET_TYPE_SOURCE_DESCRIPTION:
                DLOGD("unhandled packet type RTCP_PACKET_TYPE_SOURCE_DESCRIPTION");
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
        while(pCurNode != NULL && pTransceiver == NULL) {
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
    while(pCurNode != NULL && pTransceiver == NULL) {
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
