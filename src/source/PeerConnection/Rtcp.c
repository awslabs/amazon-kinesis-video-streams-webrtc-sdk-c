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

// TODO handle SLI packet https://tools.ietf.org/html/rfc4585#section-6.3.2
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

// TODO handle TWCC packet https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
STATUS onRtcpTwccPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    /*
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |V=2|P|  FMT=15 |    PT=205     |           length              |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     SSRC of packet sender                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      SSRC of media source                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |      base sequence number     |      packet status count      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                 reference time                | fb pkt. count |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |          packet chunk         |         packet chunk          |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       .                                                               .
       .                                                               .
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |         packet chunk          |  recv delta   |  recv delta   |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       .                                                               .
       .                                                               .
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           recv delta          |  recv delta   | zero padding  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     */
    STATUS retStatus = STATUS_SUCCESS;
    INT32 packetsRemaining;
    UINT16 baseSeqNum, packetStatusCount, packetSeqNum;
    UINT32 chunkOffset, recvOffset;
    UINT8 statusSymbol;
    UINT32 packetChunk;
    INT16 recvDelta;
    UINT32 statuses;
    UINT32 i;
    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    // dont parse if neither callbacks are set
    CHK(pKvsPeerConnection->onPacketNotReceived != NULL || pKvsPeerConnection->onPacketReceived != NULL, STATUS_SUCCESS);

    baseSeqNum = getUnalignedInt16BigEndian(pRtcpPacket->payload + 8);
    packetStatusCount = TWCC_PACKET_STATUS_COUNT(pRtcpPacket->payload);

    packetsRemaining = packetStatusCount;
    chunkOffset = 16;
    while (packetsRemaining > 0 && chunkOffset < pRtcpPacket->payloadLength) {
        packetChunk = getUnalignedInt16BigEndian(pRtcpPacket->payload + chunkOffset);
        if (IS_TWCC_RUNLEN(packetChunk)) {
            packetsRemaining -= TWCC_RUNLEN_GET(packetChunk);
        } else {
            packetsRemaining -= MIN(TWCC_STATUSVECTOR_COUNT(packetChunk), packetsRemaining);
        }
        chunkOffset += TWCC_FB_PACKETCHUNK_SIZE;
    }

    recvOffset = chunkOffset;
    chunkOffset = 16;
    packetSeqNum = baseSeqNum;
    packetsRemaining = packetStatusCount;
    while (packetsRemaining > 0) {
        packetChunk = getUnalignedInt16BigEndian(pRtcpPacket->payload + chunkOffset);
        statusSymbol = TWCC_RUNLEN_STATUS_SYMBOL(packetChunk);
        if (IS_TWCC_RUNLEN(packetChunk)) {
            for (i = 0; i < TWCC_RUNLEN_GET(packetChunk); i++) {
                recvDelta = MIN_INT16;
                switch (statusSymbol) {
                    case TWCC_STATUS_SYMBOL_SMALLDELTA:
                        recvDelta = (INT16) pRtcpPacket->payload[recvOffset];
                        recvOffset++;
                        break;
                    case TWCC_STATUS_SYMBOL_LARGEDELTA:
                        recvDelta = getUnalignedInt16BigEndian(pRtcpPacket->payload + recvOffset);
                        recvOffset += 2;
                        break;
                    case TWCC_STATUS_SYMBOL_NOTRECEIVED:
                        DLOGV("runLength packetSeqNum %u not received", packetSeqNum);
                        if (pKvsPeerConnection->onPacketNotReceived != NULL) {
                            pKvsPeerConnection->onPacketNotReceived(pKvsPeerConnection->onPacketNotReceivedCustomData, packetSeqNum);
                        }
                        break;
                    default:
                        DLOGD("runLength unhandled statusSymbol %u", statusSymbol);
                }
                if (recvDelta != MIN_INT16) {
                    DLOGV("runLength packetSeqNum %u recvDelta %d usec", packetSeqNum,
                          KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, MICROSECONDS_PER_SECOND));
                    if (pKvsPeerConnection->onPacketReceived != NULL) {
                        pKvsPeerConnection->onPacketReceived(pKvsPeerConnection->onPacketReceivedCustomData, packetSeqNum,
                                                             KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, MICROSECONDS_PER_SECOND));
                    }
                }
                packetSeqNum++;
                packetsRemaining--;
            }
        } else {
            statuses = MIN(TWCC_STATUSVECTOR_COUNT(packetChunk), packetsRemaining);

            for (i = 0; i < statuses; i++) {
                statusSymbol = TWCC_STATUSVECTOR_STATUS(packetChunk, i);
                recvDelta = MIN_INT16;
                switch (statusSymbol) {
                    case TWCC_STATUS_SYMBOL_SMALLDELTA:
                        recvDelta = (INT16) pRtcpPacket->payload[recvOffset];
                        recvOffset++;
                        break;
                    case TWCC_STATUS_SYMBOL_LARGEDELTA:
                        recvDelta = getUnalignedInt16BigEndian(pRtcpPacket->payload + recvOffset);
                        recvOffset += 2;
                        break;
                    case TWCC_STATUS_SYMBOL_NOTRECEIVED:
                        DLOGV("statusVector packetSeqNum %u not received", packetSeqNum);
                        if (pKvsPeerConnection->onPacketNotReceived != NULL) {
                            pKvsPeerConnection->onPacketNotReceived(pKvsPeerConnection->onPacketNotReceivedCustomData, packetSeqNum);
                        }
                        break;
                    default:
                        DLOGD("statusVector unhandled statusSymbol %u", statusSymbol);
                }
                if (recvDelta != MIN_INT16) {
                    DLOGV("statusVector packetSeqNum %u recvDelta %d usec", packetSeqNum,
                          KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, MICROSECONDS_PER_SECOND));
                    if (pKvsPeerConnection->onPacketReceived != NULL) {
                        pKvsPeerConnection->onPacketReceived(pKvsPeerConnection->onPacketReceivedCustomData, packetSeqNum,
                                                             KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, MICROSECONDS_PER_SECOND));
                    }
                }
                packetSeqNum++;
                packetsRemaining--;
            }
        }
        chunkOffset += TWCC_FB_PACKETCHUNK_SIZE;
    }

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
                } else if (rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK) {
                    CHK_STATUS(onRtcpTwccPacket(&rtcpPacket, pKvsPeerConnection));
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
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    CHK_STATUS(rembValueGet(pRtcpPacket->payload, pRtcpPacket->payloadLength, &maximumBitRate, (PUINT32) &ssrcList, &ssrcListLen));

    for (i = 0; i < ssrcListLen; i++) {
        pTransceiver = NULL;
        if (STATUS_FAILED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, ssrcList[i]))) {
            DLOGW("Received REMB for non existing ssrcs: ssrc %lu", ssrcList[i]);
        }
        if (pTransceiver != NULL && pTransceiver->onBandwidthEstimation != NULL) {
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
