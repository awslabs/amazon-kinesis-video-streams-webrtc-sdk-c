#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"
#include "kvsrtcp/rtcp_api.h"

static RtcpPacketType_t getDetailedRtcpPacketType(uint8_t packetType, uint8_t receptionReportCount)
{
    RtcpPacketType_t result = RTCP_PACKET_UNKNOWN;

    switch (packetType) {
        case RTCP_PACKET_TYPE_FIR:
            if (receptionReportCount == 0) {
                result = RTCP_PACKET_FIR;
            }
            break;
        case RTCP_PACKET_TYPE_SENDER_REPORT:
            result = RTCP_PACKET_SENDER_REPORT;
            break;
        case RTCP_PACKET_TYPE_RECEIVER_REPORT:
            result = RTCP_PACKET_RECEIVER_REPORT;
            break;
        case RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK:
            if (receptionReportCount == RTCP_FMT_TRANSPORT_SPECIFIC_FEEDBACK_NACK) {
                result = RTCP_PACKET_TRANSPORT_FEEDBACK_NACK;
            } else if (receptionReportCount == RTCP_FMT_TRANSPORT_SPECIFIC_FEEDBACK_TWCC) {
                result = RTCP_PACKET_TRANSPORT_FEEDBACK_TWCC;
            }
            break;
        case RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK:
            if (receptionReportCount == RTCP_FMT_PAYLOAD_SPECIFIC_FEEDBACK_PLI) {
                result = RTCP_PACKET_PAYLOAD_FEEDBACK_PLI;
            } else if (receptionReportCount == RTCP_FMT_PAYLOAD_SPECIFIC_FEEDBACK_SLI) {
                result = RTCP_PACKET_PAYLOAD_FEEDBACK_SLI;
            } else if (receptionReportCount == RTCP_FMT_PAYLOAD_SPECIFIC_FEEDBACK_REMB) {
                result = RTCP_PACKET_PAYLOAD_FEEDBACK_REMB;
            }
            break;
        default:
            break;
    }

    return result;
}

// TODO handle FIR packet https://tools.ietf.org/html/rfc2032#section-5.2.1
static STATUS onRtcpFIRPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PKvsRtpTransceiver pTransceiver = NULL;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpFirPacket_t firPacket;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpPacket.header.packetType = getDetailedRtcpPacketType(pRtcpPacket->header.packetType, pRtcpPacket->header.receptionReportCount);
    rtcpPacket.header.receptionReportCount = pRtcpPacket->header.receptionReportCount;
    rtcpPacket.pPayload = (const PBYTE) pRtcpPacket->payload;
    rtcpPacket.payloadLength = (size_t) pRtcpPacket->payloadLength;

    rtcpResult = Rtcp_ParseFirPacket(&ctx, &rtcpPacket, &firPacket);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, firPacket.senderSsrc))) {
        MUTEX_LOCK(pTransceiver->statsLock);
        pTransceiver->outboundStats.firCount++;
        MUTEX_UNLOCK(pTransceiver->statsLock);
        if (pTransceiver->onPictureLoss != NULL) {
            pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
        }
    } else {
        DLOGW("Received FIR for non existing ssrc: %u", firPacket.senderSsrc);
    }

CleanUp:

    return retStatus;
}

// TODO handle SLI packet https://tools.ietf.org/html/rfc4585#section-6.3.2
STATUS onRtcpSLIPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 noSliInfo;
    PKvsRtpTransceiver pTransceiver = NULL;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpSliPacket_t sliPacket = {0};

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpPacket.header.packetType = getDetailedRtcpPacketType(pRtcpPacket->header.packetType, pRtcpPacket->header.receptionReportCount);
    rtcpPacket.header.receptionReportCount = pRtcpPacket->header.receptionReportCount;
    rtcpPacket.pPayload = (const PBYTE) pRtcpPacket->payload;
    rtcpPacket.payloadLength = (size_t) pRtcpPacket->payloadLength;

    noSliInfo = (UINT32) ((rtcpPacket.payloadLength - SIZEOF(sliPacket.senderSsrc - SIZEOF(sliPacket.mediaSourceSsrc))) / 4);
    if (noSliInfo > 0) {
        sliPacket.numSliInfos = noSliInfo;
        sliPacket.pSliInfos = MEMALLOC(noSliInfo * SIZEOF(UINT32));
    }

    rtcpResult = Rtcp_ParseSliPacket(&ctx, &rtcpPacket, &sliPacket);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, sliPacket.mediaSourceSsrc))) {
        MUTEX_LOCK(pTransceiver->statsLock);
        pTransceiver->outboundStats.sliCount++;
        MUTEX_UNLOCK(pTransceiver->statsLock);
    } else {
        DLOGW("Received SLI for non existing ssrc: %u", sliPacket.mediaSourceSsrc);
    }

CleanUp:

    if (sliPacket.pSliInfos != NULL) {
        MEMFREE(sliPacket.pSliInfos);
        sliPacket.pSliInfos = NULL;
    }
    return retStatus;
}

// TODO better sender report handling https://tools.ietf.org/html/rfc3550#section-6.4.1
static STATUS onRtcpSenderReport(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 senderSSRC;
    PKvsRtpTransceiver pTransceiver = NULL;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpSenderReport_t senderReport;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    if (pRtcpPacket->payloadLength != RTCP_PACKET_SENDER_REPORT_MINLEN) {
        // TODO: handle sender report containing receiver report blocks
        return STATUS_SUCCESS;
    }

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpPacket.pPayload = (const PBYTE) pRtcpPacket->payload;
    rtcpPacket.payloadLength = (size_t) pRtcpPacket->payloadLength;
    rtcpPacket.header.packetType = getDetailedRtcpPacketType(pRtcpPacket->header.packetType, pRtcpPacket->header.receptionReportCount);
    rtcpPacket.header.receptionReportCount = pRtcpPacket->header.receptionReportCount;

    rtcpResult = Rtcp_ParseSenderReport(&ctx, &rtcpPacket, &senderReport);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    senderSSRC = senderReport.senderSsrc;
    if (STATUS_SUCCEEDED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, senderSSRC))) {
        UINT64 ntpTime = senderReport.senderInfo.ntpTime;
        UINT32 rtpTs = senderReport.senderInfo.rtpTime;
        UINT32 packetCnt = senderReport.senderInfo.packetCount;
        UINT32 octetCnt = senderReport.senderInfo.octetCount;
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
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpReceiverReport_t receiverReport = {0};

    UNUSED_PARAM(rttPropDelayMsec);
    UNUSED_PARAM(rttPropDelay);
    UNUSED_PARAM(delaySinceLastSR);
    UNUSED_PARAM(lastSR);
    UNUSED_PARAM(interarrivalJitter);
    UNUSED_PARAM(extHiSeqNumReceived);
    UNUSED_PARAM(cumulativeLost);
    UNUSED_PARAM(senderSSRC);

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    // https://tools.ietf.org/html/rfc3550#section-6.4.2
    if (pRtcpPacket->payloadLength != RTCP_PACKET_RECEIVER_REPORT_MINLEN) {
        // TODO: handle multiple receiver report blocks
        return STATUS_SUCCESS;
    }

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpPacket.pPayload = (const PBYTE) pRtcpPacket->payload;
    rtcpPacket.payloadLength = (size_t) pRtcpPacket->payloadLength;
    rtcpPacket.header.packetType = getDetailedRtcpPacketType(pRtcpPacket->header.packetType, pRtcpPacket->header.receptionReportCount);
    rtcpPacket.header.receptionReportCount = pRtcpPacket->header.receptionReportCount;
    receiverReport.numReceptionReports = pRtcpPacket->header.receptionReportCount;
    if (receiverReport.numReceptionReports > 0) {
        receiverReport.pReceptionReports = (RtcpReceptionReport_t*) MEMALLOC(rtcpPacket.header.receptionReportCount * sizeof(RtcpReceptionReport_t));
    }

    rtcpResult = Rtcp_ParseReceiverReport(&ctx, &rtcpPacket, &receiverReport);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    senderSSRC = receiverReport.senderSsrc;
    ssrc1 = receiverReport.pReceptionReports->sourceSsrc;

    if (STATUS_FAILED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, ssrc1))) {
        DLOGW("Received receiver report for non existing ssrc: %u", ssrc1);
        return STATUS_SUCCESS; // not really an error ?
    }
    fractionLost = receiverReport.pReceptionReports->fractionLost / 255.0;
    cumulativeLost = receiverReport.pReceptionReports->cumulativePacketsLost;
    extHiSeqNumReceived = receiverReport.pReceptionReports->extendedHighestSeqNumReceived;
    interarrivalJitter = receiverReport.pReceptionReports->interArrivalJitter;
    lastSR = receiverReport.pReceptionReports->lastSR;
    delaySinceLastSR = receiverReport.pReceptionReports->delaySinceLastSR;

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

    if (receiverReport.pReceptionReports != NULL) {
        MEMFREE(receiverReport.pReceptionReports);
        receiverReport.pReceptionReports = NULL;
    }
    return retStatus;
}

STATUS parseRtcpTwccPacket(PRtcpPacket pRtcpPacket, PTwccManager pTwccManager)
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
    UINT64 referenceTime;
    PTwccRtpPacketInfo pTwccPacket = NULL;
    UINT64 twccPktValue = 0;
    CHK(pTwccManager != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    baseSeqNum = getUnalignedInt16BigEndian(pRtcpPacket->payload + 8);
    pTwccManager->prevReportedBaseSeqNum = baseSeqNum;
    packetStatusCount = TWCC_PACKET_STATUS_COUNT(pRtcpPacket->payload);
    referenceTime = (pRtcpPacket->payload[12] << 16) | (pRtcpPacket->payload[13] << 8) | (pRtcpPacket->payload[14] & 0xff);
    referenceTime = KVS_CONVERT_TIMESCALE(referenceTime * 64, MILLISECONDS_PER_SECOND, HUNDREDS_OF_NANOS_IN_A_SECOND);
    // TODO: handle lost twcc report packets

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
                        DLOGS("runLength packetSeqNum %u not received %lu", packetSeqNum, referenceTime);
                        // If it does not exist it means the packet was already visited
                        if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, &twccPktValue))) {
                            pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                            if (pTwccPacket != NULL) {
                                pTwccPacket->remoteTimeKvs = TWCC_PACKET_LOST_TIME;
                                CHK_STATUS(hashTableUpsert(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, (UINT64) pTwccPacket));
                            }
                        }
                        pTwccManager->lastReportedSeqNum = packetSeqNum;
                        break;
                    default:
                        DLOGD("runLength unhandled statusSymbol %u", statusSymbol);
                }
                if (recvDelta != MIN_INT16) {
                    referenceTime += KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, HUNDREDS_OF_NANOS_IN_A_SECOND);
                    DLOGS("runLength packetSeqNum %u received %lu", packetSeqNum, referenceTime);

                    // If it does not exist it means the packet was already visited
                    if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, &twccPktValue))) {
                        pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                        if (pTwccPacket != NULL) {
                            pTwccPacket->remoteTimeKvs = referenceTime;
                            CHK_STATUS(hashTableUpsert(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, (UINT64) pTwccPacket));
                        }
                    }
                    pTwccManager->lastReportedSeqNum = packetSeqNum;
                }
                packetSeqNum++;
                packetsRemaining--;
                // Reset to NULL before next iteration
                pTwccPacket = NULL;
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
                        DLOGS("statusVector packetSeqNum %u not received %lu", packetSeqNum, referenceTime);
                        // If it does not exist it means the packet was already visited
                        if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, &twccPktValue))) {
                            pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                            if (pTwccPacket != NULL) {
                                pTwccPacket->remoteTimeKvs = TWCC_PACKET_LOST_TIME;
                                CHK_STATUS(hashTableUpsert(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, (UINT64) pTwccPacket));
                            }
                        }
                        pTwccManager->lastReportedSeqNum = packetSeqNum;
                        break;
                    default:
                        DLOGD("statusVector unhandled statusSymbol %u", statusSymbol);
                }
                if (recvDelta != MIN_INT16) {
                    referenceTime += KVS_CONVERT_TIMESCALE(recvDelta, TWCC_TICKS_PER_SECOND, HUNDREDS_OF_NANOS_IN_A_SECOND);
                    DLOGS("statusVector packetSeqNum %u received %lu", packetSeqNum, referenceTime);
                    // If it does not exist it means the packet was already visited
                    if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, &twccPktValue))) {
                        pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                        if (pTwccPacket != NULL) {
                            pTwccPacket->remoteTimeKvs = referenceTime;
                            CHK_STATUS(hashTableUpsert(pTwccManager->pTwccRtpPktInfosHashTable, packetSeqNum, (UINT64) pTwccPacket));
                        }
                    }
                    pTwccManager->lastReportedSeqNum = packetSeqNum;
                }
                packetSeqNum++;
                packetsRemaining--;
                // Reset to NULL before next iteration
                pTwccPacket = NULL;
            }
        }
        chunkOffset += TWCC_FB_PACKETCHUNK_SIZE;
    }
    DLOGV("Checking seqNum %d to %d of TWCC reports", baseSeqNum, pTwccManager->lastReportedSeqNum);
CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS onRtcpTwccPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTwccManager pTwccManager = NULL;
    BOOL locked = FALSE;
    UINT16 baseSeqNum = 0;
    UINT64 localStartTimeKvs, localEndTimeKvs;
    UINT64 sentBytes = 0, receivedBytes = 0;
    UINT64 sentPackets = 0, receivedPackets = 0;
    INT64 duration = 0;
    UINT16 seqNum = 0;
    PTwccRtpPacketInfo pTwccPacket = NULL;
    UINT64 twccPktValue = 0;
    BOOL localStartTimeRecorded = FALSE;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    CHK(pKvsPeerConnection->onSenderBandwidthEstimation != NULL && pKvsPeerConnection->pTwccManager != NULL, STATUS_SUCCESS);

    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    locked = TRUE;
    pTwccManager = pKvsPeerConnection->pTwccManager;
    CHK_STATUS(parseRtcpTwccPacket(pRtcpPacket, pTwccManager));
    baseSeqNum = pTwccManager->prevReportedBaseSeqNum;

    // Use != instead to cover the case where the group of sequence numbers being checked
    // are trending towards MAX_UINT16 and rolling over to 0+, example range [65534, 10]
    // We also check for twcc->lastReportedSeqNum + 1 to include the last seq number in the
    // report. Without this, we do not check for the seqNum that could cause it to not be cleared
    // from memory
    for (seqNum = baseSeqNum; seqNum != (pTwccManager->lastReportedSeqNum + 1); seqNum++) {
        if (!localStartTimeRecorded) {
            // This could happen if the prev packet was deleted as part of rolling window or if there
            // is an overlap of RTP packet statuses between TWCC packets. This could also fail if it is
            // the first ever packet (seqNum 0)
            if (hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, seqNum - 1, &twccPktValue) == STATUS_HASH_KEY_NOT_PRESENT) {
                localStartTimeKvs = TWCC_PACKET_UNITIALIZED_TIME;
            } else {
                pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                if (pTwccPacket != NULL) {
                    localStartTimeKvs = pTwccPacket->localTimeKvs;
                    localStartTimeRecorded = TRUE;
                }
            }
            if (localStartTimeKvs == TWCC_PACKET_UNITIALIZED_TIME) {
                // time not yet set. If prev seqNum was deleted
                if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, seqNum, &twccPktValue))) {
                    pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
                    if (pTwccPacket != NULL) {
                        localStartTimeKvs = pTwccPacket->localTimeKvs;
                        localStartTimeRecorded = TRUE;
                    }
                }
            }
        }

        // The time it would not succeed is if there is an overlap in the RTP packet status between the TWCC
        // packets
        if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, seqNum, &twccPktValue))) {
            pTwccPacket = (PTwccRtpPacketInfo) twccPktValue;
            if (pTwccPacket != NULL) {
                localEndTimeKvs = pTwccPacket->localTimeKvs;
                duration = localEndTimeKvs - localStartTimeKvs;
                sentBytes += pTwccPacket->packetSize;
                sentPackets++;
                if (pTwccPacket->remoteTimeKvs != TWCC_PACKET_LOST_TIME) {
                    receivedBytes += pTwccPacket->packetSize;
                    receivedPackets++;
                    if (STATUS_SUCCEEDED(hashTableRemove(pTwccManager->pTwccRtpPktInfosHashTable, seqNum))) {
                        SAFE_MEMFREE(pTwccPacket);
                    }
                }
            }
        }
    }

    if (duration > 0) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccLock);
        locked = FALSE;
        pKvsPeerConnection->onSenderBandwidthEstimation(pKvsPeerConnection->onSenderBandwidthEstimationCustomData, sentBytes, receivedBytes,
                                                        sentPackets, receivedPackets, duration);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (locked) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccLock);
    }
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
    PKvsRtpTransceiver pTransceiver = NULL;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpPliPacket_t pliPacket;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpPacket.header.packetType = getDetailedRtcpPacketType(pRtcpPacket->header.packetType, pRtcpPacket->header.receptionReportCount);
    rtcpPacket.header.receptionReportCount = pRtcpPacket->header.receptionReportCount;
    rtcpPacket.pPayload = (const PBYTE) pRtcpPacket->payload;
    rtcpPacket.payloadLength = (size_t) pRtcpPacket->payloadLength;

    rtcpResult = Rtcp_ParsePliPacket(&ctx, &rtcpPacket, &pliPacket);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    CHK_STATUS_ERR(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, pliPacket.mediaSourceSsrc), STATUS_RTCP_INPUT_SSRC_INVALID,
                   "Received PLI for non existing ssrc: %u", pliPacket.mediaSourceSsrc);

    MUTEX_LOCK(pTransceiver->statsLock);
    pTransceiver->outboundStats.pliCount++;
    MUTEX_UNLOCK(pTransceiver->statsLock);

    if (pTransceiver->onPictureLoss != NULL) {
        pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
    }

CleanUp:

    return retStatus;
}
