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
        DLOGW("Received SLI for non existing ssrc: %u", mediaSSRC);
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
    UINT32 rttPropDelayMsec = 0, rttPropDelay, delaySinceLastSR, lastSR, ssrc1;
#ifdef LOG_STREAMING
    UINT32 interarrivalJitter, extHiSeqNumReceived, cumulativeLost, senderSSRC;
#endif
    UINT64 currentTimeNTP = convertTimestampToNTP(GETTIME());

    UNUSED_PARAM(rttPropDelayMsec);
    UNUSED_PARAM(rttPropDelay);
    UNUSED_PARAM(delaySinceLastSR);
    UNUSED_PARAM(lastSR);

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    // https://tools.ietf.org/html/rfc3550#section-6.4.2
    if (pRtcpPacket->payloadLength != RTCP_PACKET_RECEIVER_REPORT_MINLEN) {
        // TODO: handle multiple receiver report blocks
        return STATUS_SUCCESS;
    }

#ifdef LOG_STREAMING
    senderSSRC = getUnalignedInt32BigEndian(pRtcpPacket->payload);
#endif
    ssrc1 = getUnalignedInt32BigEndian(pRtcpPacket->payload + 4);

    if (STATUS_FAILED(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, ssrc1))) {
        DLOGW("Received receiver report for non existing ssrc: %u", ssrc1);
        return STATUS_SUCCESS; // not really an error ?
    }
    fractionLost = pRtcpPacket->payload[8] / 255.0;
#ifdef LOG_STREAMING
    cumulativeLost = ((UINT32) getUnalignedInt32BigEndian(pRtcpPacket->payload + 8)) & 0x00ffffffu;
    extHiSeqNumReceived = getUnalignedInt32BigEndian(pRtcpPacket->payload + 12);
    interarrivalJitter = getUnalignedInt32BigEndian(pRtcpPacket->payload + 16);
#endif
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

// After this function executes, the twccManager saves the indexes of the packets
// reported in this feedback.
// - twccManager.prevReportedBaseSeqNum = base seqNum, first packet in the report
// - twccManager.lastReportedSeqNum = final seqNum, last packet in the report
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
    CHK(pRtcpPacket->payloadLength >= TWCC_FB_PAYLOAD_MIN_LEN, STATUS_RTCP_INPUT_PACKET_TOO_SMALL);

    baseSeqNum = getUnalignedInt16BigEndian(pRtcpPacket->payload + 8);
    pTwccManager->prevReportedBaseSeqNum = baseSeqNum;
    packetStatusCount = TWCC_PACKET_STATUS_COUNT(pRtcpPacket->payload);

    // Empty report, nothing to parse
    if (packetStatusCount == 0u) {
        DLOGD("Received empty TWCC packet");
        CHK(FALSE, STATUS_SUCCESS);
    }

    if (packetStatusCount > TWCC_MAX_PACKET_STATUS_COUNT) {
        DLOGD("Malformed TWCC packet: packetStatusCount %u exceeds max %u, clamping", packetStatusCount, TWCC_MAX_PACKET_STATUS_COUNT);
        packetStatusCount = TWCC_MAX_PACKET_STATUS_COUNT;
    }

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
                        if (recvOffset >= pRtcpPacket->payloadLength) {
                            packetsRemaining = 0;
                            break;
                        }
                        recvDelta = (INT16) pRtcpPacket->payload[recvOffset];
                        recvOffset++;
                        break;
                    case TWCC_STATUS_SYMBOL_LARGEDELTA:
                        if (recvOffset + 1 >= pRtcpPacket->payloadLength) {
                            packetsRemaining = 0;
                            break;
                        }
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
                if (packetsRemaining == 0) {
                    break;
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
                        if (recvOffset >= pRtcpPacket->payloadLength) {
                            packetsRemaining = 0;
                            break;
                        }
                        recvDelta = (INT16) pRtcpPacket->payload[recvOffset];
                        recvOffset++;
                        break;
                    case TWCC_STATUS_SYMBOL_LARGEDELTA:
                        if (recvOffset + 1 >= pRtcpPacket->payloadLength) {
                            packetsRemaining = 0;
                            break;
                        }
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
                if (packetsRemaining == 0) {
                    break;
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

STATUS computeTwccTrendline(PTwccManager pTwccManager, PDOUBLE pDelayTrend, PDOUBLE pQueueDelay)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 twccPktValue = 0;
    PTwccRtpPacketInfo pCurr = NULL, pPrev = NULL;
    UINT16 seqNum;
    DOUBLE accumulatedDelay = 0.0;
    // Accumulators for least-squares: sum(x), sum(y), sum(x*y), sum(x^2)
    DOUBLE sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumX2 = 0.0;
    DOUBLE rawSlope = 0.0;
    UINT32 n = 0;

    CHK(pTwccManager != NULL && pDelayTrend != NULL && pQueueDelay != NULL, STATUS_NULL_ARG);

    for (seqNum = pTwccManager->prevReportedBaseSeqNum; seqNum != (UINT16) (pTwccManager->lastReportedSeqNum + 1); seqNum++) {
        if (STATUS_FAILED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, seqNum, &twccPktValue))) {
            continue;
        }
        pCurr = (PTwccRtpPacketInfo) twccPktValue;
        if (pCurr == NULL || pCurr->remoteTimeKvs == TWCC_PACKET_LOST_TIME || pCurr->remoteTimeKvs == TWCC_PACKET_UNITIALIZED_TIME) {
            continue;
        }

        if (pPrev != NULL) {
            // d_i = (recvTime_i - recvTime_{i-1}) - (sendTime_i - sendTime_{i-1})
            // Positive means receiver-side gap grew relative to sender-side gap (queuing delay increased)
            INT64 interRecv = (INT64) (pCurr->remoteTimeKvs - pPrev->remoteTimeKvs);
            INT64 interSend = (INT64) (pCurr->localTimeKvs - pPrev->localTimeKvs);
            // D_i = D_{i-1} + d_i  (running sum of delay variations)
            accumulatedDelay += (DOUBLE) (interRecv - interSend);

            // Build accumulators for ordinary least-squares (OLS) linear regression.
            // We are fitting the model: y = slope * x + intercept
            //   where x_i = sample index (0, 1, 2, ...)
            //         y_i = accumulated delay D_i at that sample
            //
            // The OLS slope formula requires these running sums:
            //   sumX  = Σ x_i           (sum of all x values)
            //   sumY  = Σ y_i           (sum of all y values)
            //   sumXY = Σ (x_i * y_i)   (sum of products, measures correlation)
            //   sumX2 = Σ (x_i^2)       (sum of squared x, measures x spread)
            //
            // These avoid storing all data points in an array. We compute the
            // slope in O(1) space by maintaining only these four running totals.
            sumX += (DOUBLE) n;
            sumY += accumulatedDelay;
            sumXY += (DOUBLE) n * accumulatedDelay;
            sumX2 += (DOUBLE) n * (DOUBLE) n;
            n++;
        }

        pPrev = pCurr;
    }

    // Least-squares slope: (n*sum(xy) - sum(x)*sum(y)) / (n*sum(x^2) - sum(x)^2)
    // Need at least 2 points to define a line
    if (n >= 2) {
        DOUBLE denom = (DOUBLE) n * sumX2 - sumX * sumX;
        if (denom != 0.0) {
            rawSlope = ((DOUBLE) n * sumXY - sumX * sumY) / denom;
        }
    }

    // EMA smoothing: S_t = alpha * rawSlope + (1 - alpha) * S_{t-1}
    // This filters out short-term noise while tracking sustained trends
    pTwccManager->smoothedSlope = TWCC_TRENDLINE_SMOOTHING_FACTOR * rawSlope + (1.0 - TWCC_TRENDLINE_SMOOTHING_FACTOR) * pTwccManager->smoothedSlope;
    pTwccManager->lastQueueDelay = accumulatedDelay;
    // Convert from internal time units (hundreds of nanoseconds) to milliseconds for output
    *pDelayTrend = pTwccManager->smoothedSlope / (DOUBLE) HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    *pQueueDelay = accumulatedDelay / (DOUBLE) HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    DLOGD("TWCC trendline: delayTrend=%.4f ms queueDelay=%.2f ms (n=%u)", *pDelayTrend, *pQueueDelay, n);

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS updateTwccHashTable(PTwccManager pTwccManager, PINT64 duration, PUINT64 receivedBytes, PUINT64 receivedPackets, PUINT64 sentBytes,
                           PUINT64 sentPackets)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 localStartTimeKvs = TWCC_PACKET_UNITIALIZED_TIME, localEndTimeKvs = TWCC_PACKET_UNITIALIZED_TIME;
    UINT16 baseSeqNum = 0;
    BOOL localStartTimeRecorded = FALSE;
    UINT64 twccPktValue = 0;
    PTwccRtpPacketInfo pTwccPacket = NULL;
    UINT16 seqNum = 0;

    CHK(pTwccManager != NULL && duration != NULL && receivedBytes != NULL && receivedPackets != NULL && sentBytes != NULL && sentPackets != NULL,
        STATUS_NULL_ARG);

    *duration = 0;
    *receivedBytes = 0;
    *receivedPackets = 0;
    *sentBytes = 0;
    *sentPackets = 0;

    baseSeqNum = pTwccManager->prevReportedBaseSeqNum;

    // Use != instead to cover the case where the group of sequence numbers being checked
    // are trending towards MAX_UINT16 and rolling over to 0+, example range [65534, 10]
    // We also check for twcc->lastReportedSeqNum + 1 to include the last seq number in the
    // report. Without this, we do not check for the seqNum that could cause it to not be cleared
    // from memory
    for (seqNum = baseSeqNum; seqNum != (UINT16) (pTwccManager->lastReportedSeqNum + 1); seqNum++) {
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
                *duration = localEndTimeKvs - localStartTimeKvs;
                *sentBytes += pTwccPacket->packetSize;
                (*sentPackets)++;
                if (pTwccPacket->remoteTimeKvs != TWCC_PACKET_LOST_TIME) {
                    *receivedBytes += pTwccPacket->packetSize;
                    (*receivedPackets)++;
                    if (STATUS_SUCCEEDED(hashTableRemove(pTwccManager->pTwccRtpPktInfosHashTable, seqNum))) {
                        SAFE_MEMFREE(pTwccPacket);
                    }
                }
            } else {
                CHK_STATUS(hashTableRemove(pTwccManager->pTwccRtpPktInfosHashTable, seqNum));
            }
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS onRtcpTwccPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTwccManager pTwccManager = NULL;
    BOOL locked = FALSE;
    UINT64 sentBytes = 0, receivedBytes = 0;
    UINT64 sentPackets = 0, receivedPackets = 0;
    INT64 duration = 0;
    DOUBLE delayTrend = 0.0, queueDelay = 0.0;
    PTwccFeedback pFeedbackList = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    // Skip TWCC parsing if no callback is set to consume the results
    CHK(pKvsPeerConnection->pTwccManager != NULL &&
            (pKvsPeerConnection->onSenderBandwidthEstimation != NULL || pKvsPeerConnection->onPeerCongestionFeedback != NULL ||
             pKvsPeerConnection->onTwccFeedbackReceived != NULL),
        STATUS_SUCCESS);

    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    locked = TRUE;
    pTwccManager = pKvsPeerConnection->pTwccManager;
    CHK_STATUS(parseRtcpTwccPacket(pRtcpPacket, pTwccManager));

    // Skip empty reports - nothing useful for any estimator path
    if (TWCC_PACKET_STATUS_COUNT(pRtcpPacket->payload) == 0) {
        DLOGD("Malformed TWCC packet: packetStatusCount is 0, skipping");
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Compute trendline
    if (pKvsPeerConnection->onTwccFeedbackReceived != NULL) {
        // Custom estimator callback, build feedback list from this TWCC report
        TwccCongestionState congestionState;
        UINT32 feedbackCount = 0;
        UINT16 seqNum;
        UINT64 twccPktVal = 0;
        PTwccRtpPacketInfo pPktInfo = NULL;
        UINT16 reportLen = (UINT16) (pTwccManager->lastReportedSeqNum - pTwccManager->prevReportedBaseSeqNum + 1);
        UINT16 packetStatusCount = TWCC_PACKET_STATUS_COUNT(pRtcpPacket->payload);

        // Hard cap to prevent attacker-controlled allocation via crafted TWCC packets
        if (packetStatusCount > TWCC_MAX_PACKET_STATUS_COUNT) {
            DLOGD("Malformed TWCC packet: packetStatusCount %u exceeds max %u", packetStatusCount, TWCC_MAX_PACKET_STATUS_COUNT);
            packetStatusCount = TWCC_MAX_PACKET_STATUS_COUNT;
        }

        // Cap reportLen to packetStatusCount to bound the allocation size
        if (reportLen > packetStatusCount) {
            DLOGD("Malformed TWCC packet: reportLen %u exceeds packetStatusCount %u", reportLen, packetStatusCount);
            reportLen = packetStatusCount;
        }

        MEMSET(&congestionState, 0, SIZEOF(congestionState));
        pFeedbackList = (PTwccFeedback) MEMALLOC(reportLen * SIZEOF(TwccFeedback));
        CHK(pFeedbackList != NULL, STATUS_NOT_ENOUGH_MEMORY);

        for (seqNum = pTwccManager->prevReportedBaseSeqNum; seqNum != (UINT16) (pTwccManager->lastReportedSeqNum + 1); seqNum++) {
            // Malformed packet: run-length in chunk exceeds packetStatusCount
            if (feedbackCount >= reportLen) {
                DLOGD("Malformed TWCC packet: feedback list full at seqNum %u, stopping early (reportLen %u)", seqNum, reportLen);
                break;
            }
            if (STATUS_SUCCEEDED(hashTableGet(pTwccManager->pTwccRtpPktInfosHashTable, seqNum, &twccPktVal))) {
                pPktInfo = (PTwccRtpPacketInfo) twccPktVal;
                if (pPktInfo != NULL && pPktInfo->remoteTimeKvs != TWCC_PACKET_LOST_TIME && pPktInfo->remoteTimeKvs != TWCC_PACKET_UNITIALIZED_TIME) {
                    pFeedbackList[feedbackCount].twccSeqNum = seqNum;
                    pFeedbackList[feedbackCount].sendTime = pPktInfo->localTimeKvs;
                    pFeedbackList[feedbackCount].recvTime = pPktInfo->remoteTimeKvs;
                    pFeedbackList[feedbackCount].packetSize = pPktInfo->packetSize;
                    feedbackCount++;
                }
            }
        }

        MUTEX_UNLOCK(pKvsPeerConnection->twccLock);
        locked = FALSE;

        // Invoke custom callback outside the lock to avoid deadlock if the callback calls back into the SDK.
        // Safe because RTCP is processed on a single receive thread (connectionListenerReceiveDataRoutine),
        // so parseRtcpTwccPacket cannot run concurrently. If RTCP processing becomes multi-threaded,
        // the lock/unlock/relock pattern here and around updateTwccHashTable below must be revisited.
        retStatus = pKvsPeerConnection->onTwccFeedbackReceived(pKvsPeerConnection->onTwccFeedbackReceivedCustomData, pFeedbackList, feedbackCount,
                                                               &congestionState);
        SAFE_MEMFREE(pFeedbackList);
        CHK_STATUS(retStatus);
        MUTEX_LOCK(pKvsPeerConnection->twccLock);
        locked = TRUE;
        delayTrend = congestionState.delayTrend;
        if (!isfinite(delayTrend)) {
            DLOGW("Custom TWCC callback returned non-finite delayTrend (%.4f), resetting to 0", delayTrend);
            delayTrend = 0.0;
        }
    } else {
        // Default trendline estimator
        CHK_STATUS(computeTwccTrendline(pTwccManager, &delayTrend, &queueDelay));
    }

    updateTwccHashTable(pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets);

    if (duration > 0) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccLock);
        locked = FALSE;

        // Invoke the new congestion feedback callback if set
        if (pKvsPeerConnection->onPeerCongestionFeedback != NULL) {
            CongestionCtx ctx;
            ctx.txBytes = sentBytes;
            ctx.rxBytes = receivedBytes;
            ctx.txPackets = sentPackets;
            ctx.rxPackets = receivedPackets;
            ctx.duration = (UINT64) duration;
            ctx.congestionState.delayTrend = delayTrend;
            CHK_LOG_ERR(pKvsPeerConnection->onPeerCongestionFeedback(pKvsPeerConnection->onPeerCongestionFeedbackCustomData, &ctx));
        }

        // Also invoke the legacy callback for backward compatibility
        if (pKvsPeerConnection->onSenderBandwidthEstimation != NULL) {
            pKvsPeerConnection->onSenderBandwidthEstimation(pKvsPeerConnection->onSenderBandwidthEstimationCustomData, sentBytes, receivedBytes,
                                                            sentPackets, receivedPackets, duration);
        }
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    SAFE_MEMFREE(pFeedbackList);
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
