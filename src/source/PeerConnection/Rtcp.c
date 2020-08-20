#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

// TODO handle FIR packet https://tools.ietf.org/html/rfc2032#section-5.2.1
static STATUS onRtcpFIRPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 mediaSSRC;
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    mediaSSRC = getUnalignedInt32BigEndian((pRtcpPacket->payload + (SIZEOF(UINT32))));
    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, mediaSSRC))) {
        MUTEX_LOCK(pTransceiver->statsLock);
        pTransceiver->outboundStats.firCount++;
        MUTEX_UNLOCK(pTransceiver->statsLock);
        if (pTransceiver->onPictureLoss != NULL) {
            pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
        }
    } else {
        DLOGW("Received FIR for non existing ssrc: %u", mediaSSRC);
    }

CleanUp:

    return retStatus;
}

// TODO handle SLI packet https://tools.ietf.org/html/rfc2032#section-5.2.1
static STATUS onRtcpSLIPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 mediaSSRC;
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    mediaSSRC = getUnalignedInt32BigEndian((pRtcpPacket->payload + (SIZEOF(UINT32))));
    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, mediaSSRC))) {
        MUTEX_LOCK(pTransceiver->statsLock);
        pTransceiver->outboundStats.sliCount++;
        MUTEX_UNLOCK(pTransceiver->statsLock);
    } else {
        DLOGW("Received FIR for non existing ssrc: %u", mediaSSRC);
    }

CleanUp:

    return retStatus;
}

// TODO better sender report handling https://tools.ietf.org/html/rfc3550#section-6.4.1
static STATUS onRtcpSenderReport(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 senderSSRC;
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    if (pRtcpPacket->payloadLength != RTCP_PACKET_SENDER_REPORT_MINLEN) {
        // TODO: handle sender report containing receiver report blocks
        DLOGW("unhandled packet type RTCP_PACKET_SENDER_REPORT size %d", pRtcpPacket->payloadLength);
        return STATUS_SUCCESS;
    }

    senderSSRC = getUnalignedInt32BigEndian(pRtcpPacket->payload);
    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, senderSSRC))) {
        UINT64 ntpTime = getUnalignedInt64BigEndian(pRtcpPacket->payload + 4);
        UINT32 rtpTs = getUnalignedInt32BigEndian(pRtcpPacket->payload + 12);
        UINT32 packetCnt = getUnalignedInt32BigEndian(pRtcpPacket->payload + 16);
        UINT32 octetCnt = getUnalignedInt32BigEndian(pRtcpPacket->payload + 20);
        DLOGV("RTCP_PACKET_TYPE_SENDER_REPORT %d %" PRIu64 " rtpTs: %u %u pkts %u bytes", senderSSRC, ntpTime, rtpTs, packetCnt, octetCnt);
    } else {
        DLOGW("Received sender report for non existing ssrc: %u", senderSSRC);
    }

CleanUp:

    return retStatus;
}

static STATUS onRtcpReceiverReport(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pTransceiver = NULL;
    DOUBLE fractionLost;
    UINT32 rttPropDelayMsec = 0, rttPropDelay, delaySinceLastSR, lastSR, interarrivalJitter, extHiSeqNumReceived, cumulativeLost, senderSSRC, ssrc1;
    UINT64 currentTimeNTP = convertTimestampToNTP(GETTIME());

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    // https://tools.ietf.org/html/rfc3550#section-6.4.2
    if (pRtcpPacket->payloadLength != RTCP_PACKET_RECEIVER_REPORT_MINLEN) {
        // TODO: handle multiple receiver report blocks
        DLOGW("unhandled packet type RTCP_PACKET_TYPE_RECEIVER_REPORT size %d", pRtcpPacket->payloadLength);
        return STATUS_SUCCESS;
    }

    senderSSRC = getUnalignedInt32BigEndian(pRtcpPacket->payload);
    ssrc1 = getUnalignedInt32BigEndian(pRtcpPacket->payload + 4);

    if (STATUS_FAILED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, ssrc1))) {
        DLOGW("Received receiver report for non existing ssrc: %u", ssrc1);
        return STATUS_SUCCESS; // not really an error ?
    }
    fractionLost = pRtcpPacket->payload[8] / 255.0;
    cumulativeLost = ((UINT32) getUnalignedInt32BigEndian(pRtcpPacket->payload + 8)) & 0x00ffffffu;
    extHiSeqNumReceived = getUnalignedInt32BigEndian(pRtcpPacket->payload + 12);
    interarrivalJitter = getUnalignedInt32BigEndian(pRtcpPacket->payload + 16);
    lastSR = getUnalignedInt32BigEndian(pRtcpPacket->payload + 20);
    delaySinceLastSR = getUnalignedInt32BigEndian(pRtcpPacket->payload + 24);

    DLOGS("RTCP_PACKET_TYPE_RECEIVER_REPORT %u %u loss: %u %u seq: %u jit: %u lsr: %u dlsr: %u", senderSSRC, ssrc1, fractionLost, cumulativeLost,
          extHiSeqNumReceived, interarrivalJitter, lastSR, delaySinceLastSR);
    if (lastSR != 0) {
        // https://tools.ietf.org/html/rfc3550#section-6.4.1
        //      Source SSRC_n can compute the round-trip propagation delay to
        //      SSRC_r by recording the time A when this reception report block is
        //      received.  It calculates the total round-trip time A-LSR using the
        //      last SR timestamp (LSR) field, and then subtracting this field to
        //      leave the round-trip propagation delay as (A - LSR - DLSR).
        rttPropDelay = MID_NTP(currentTimeNTP) - lastSR - delaySinceLastSR;
        rttPropDelayMsec = KVS_CONVERT_TIMESCALE(rttPropDelay, DLSR_TIMESCALE, 1000);
        DLOGS("RTCP_PACKET_TYPE_RECEIVER_REPORT rttPropDelay %u msec", rttPropDelayMsec);
    }

    MUTEX_LOCK(pTransceiver->statsLock);
    pTransceiver->remoteInboundStats.reportsReceived++;
    if (fractionLost > -1.0) {
        pTransceiver->remoteInboundStats.fractionLost = fractionLost;
    }
    pTransceiver->remoteInboundStats.roundTripTimeMeasurements++;
    pTransceiver->remoteInboundStats.totalRoundTripTime += rttPropDelayMsec;
    pTransceiver->remoteInboundStats.roundTripTime = rttPropDelayMsec;
    MUTEX_UNLOCK(pTransceiver->statsLock);

CleanUp:

    return retStatus;
}

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;
    UINT32 currentOffset = 0;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    while (currentOffset < buffLen) {
        CHK_STATUS(setRtcpPacketFromBytes(pBuff + currentOffset, buffLen - currentOffset, &rtcpPacket));

        switch (rtcpPacket.header.packetType) {
            case RTCP_PACKET_TYPE_FIR:
                CHK_STATUS(onRtcpFIRPacket(&rtcpPacket, pKvsPeerConnection));
                break;
            case RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK:
                if (rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK) {
                    CHK_STATUS(resendPacketOnNack(&rtcpPacket, pKvsPeerConnection));
                } else {
                    DLOGW("unhandled RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK %d", rtcpPacket.header.receptionReportCount);
                }
                break;
            case RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK:
                if (rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK &&
                    isRembPacket(rtcpPacket.payload, rtcpPacket.payloadLength) == STATUS_SUCCESS) {
                    CHK_STATUS(onRtcpRembPacket(&rtcpPacket, pKvsPeerConnection));
                } else if (rtcpPacket.header.receptionReportCount == RTCP_PSFB_PLI) {
                    CHK_STATUS(onRtcpPLIPacket(&rtcpPacket, pKvsPeerConnection));
                } else if (rtcpPacket.header.receptionReportCount == RTCP_PSFB_SLI) {
                    CHK_STATUS(onRtcpSLIPacket(&rtcpPacket, pKvsPeerConnection));
                } else {
                    DLOGW("unhandled packet type RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK %d", rtcpPacket.header.receptionReportCount);
                }
                break;
            case RTCP_PACKET_TYPE_SENDER_REPORT:
                CHK_STATUS(onRtcpSenderReport(&rtcpPacket, pKvsPeerConnection));
                break;
            case RTCP_PACKET_TYPE_RECEIVER_REPORT:
                CHK_STATUS(onRtcpReceiverReport(&rtcpPacket, pKvsPeerConnection));
                break;
            case RTCP_PACKET_TYPE_SOURCE_DESCRIPTION:
                DLOGV("unhandled packet type RTCP_PACKET_TYPE_SOURCE_DESCRIPTION");
                break;
            default:
                DLOGW("unhandled packet type %d", rtcpPacket.header.packetType);
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
        CHK_STATUS(doubleListGetHeadNode(pKvsPeerConnection->pTransceivers, &pCurNode));
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
    UINT32 mediaSSRC;
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    mediaSSRC = getUnalignedInt32BigEndian((pRtcpPacket->payload + (SIZEOF(UINT32))));

    CHK_STATUS_ERR(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, mediaSSRC), STATUS_RTCP_INPUT_SSRC_INVALID,
                   "Received PLI for non existing ssrc: %u", mediaSSRC);

    MUTEX_LOCK(pTransceiver->statsLock);
    pTransceiver->outboundStats.pliCount++;
    MUTEX_UNLOCK(pTransceiver->statsLock);

    if (pTransceiver->onPictureLoss != NULL) {
        pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
    }

CleanUp:

    return retStatus;
}
