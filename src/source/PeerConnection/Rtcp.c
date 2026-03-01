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
    UINT32 rttPropDelayMsec = 0, rttPropDelay, delaySinceLastSR, lastSR, interarrivalJitter, extHiSeqNumReceived, cumulativeLost, senderSSRC, ssrc1;
    UINT64 currentTimeNTP = convertTimestampToNTP(GETTIME());

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

STATUS updateTwccHashTable(PTwccManager pTwccManager, PINT64 duration, PUINT64 receivedBytes, PUINT64 receivedPackets, PUINT64 sentBytes,
                           PUINT64 sentPackets)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 localStartTimeKvs, localEndTimeKvs = 0;
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

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    CHK(pKvsPeerConnection->pTwccManager != NULL && pKvsPeerConnection->onSenderBandwidthEstimation != NULL, STATUS_SUCCESS);

    MUTEX_LOCK(pKvsPeerConnection->twccLock);
    locked = TRUE;
    pTwccManager = pKvsPeerConnection->pTwccManager;
    CHK_STATUS(parseRtcpTwccPacket(pRtcpPacket, pTwccManager));

    updateTwccHashTable(pTwccManager, &duration, &receivedBytes, &receivedPackets, &sentBytes, &sentPackets);

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

/*
* 0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|   FMT=4 |   PT=206      |          Length = 4           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  SSRC of Packet Sender                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|        SSRC of Media Source (UNUSED - MUST BE 0)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              SSRC     (useful)                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Seq Nr      |    Reserved (Must be all 0)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
...
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    SSRC2     (technically possible to have multiple ssrcs)    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Seq Nr      |    Reserved (Must be all 0)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
#define PSFB_FIR_MIN_SIZE        16
#define PSFB_FIR_FCI_SSRC_OFFSET 8
STATUS onRtcpPSFBFIRPacket(PRtcpPacket pRtcpPacket, PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 ssrc, offset;
    PKvsRtpTransceiver pTransceiver = NULL;

    CHK(pKvsPeerConnection != NULL && pRtcpPacket != NULL, STATUS_NULL_ARG);
    if (pRtcpPacket->payloadLength >= PSFB_FIR_MIN_SIZE) {
        offset = PSFB_FIR_FCI_SSRC_OFFSET;
        for (; (offset + 4) < pRtcpPacket->payloadLength; offset += PSFB_FIR_FCI_SSRC_OFFSET) {
            ssrc = getUnalignedInt32BigEndian(pRtcpPacket->payload + offset);
            CHK_STATUS_ERR(findTransceiverBySsrc(pKvsPeerConnection, &pTransceiver, ssrc), STATUS_RTCP_INPUT_SSRC_INVALID,
                           "Received FIR for non existing ssrc: %u", ssrc);
            MUTEX_LOCK(pTransceiver->statsLock);
            pTransceiver->outboundStats.firCount++;
            MUTEX_UNLOCK(pTransceiver->statsLock);

            if (pTransceiver->onPictureLoss != NULL) {
                pTransceiver->onPictureLoss(pTransceiver->onPictureLossCustomData);
            }
        }
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
                //  0                   1                   2                   3
                //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |V=2|P|   MBZ   |  PT=RTCP_FIR  |           length              |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
                // |                              SSRC                             |
                // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
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
                } else if (rtcpPacket.header.receptionReportCount == RTCP_PSFB_FIR) {
                    CHK_STATUS(onRtcpPSFBFIRPacket(&rtcpPacket, pKvsPeerConnection));
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

static STATUS writeRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pPacket, UINT32 packetLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pRawPacket = NULL;
    UINT32 allocSize;

    CHK(pKvsPeerConnection != NULL && pPacket != NULL, STATUS_NULL_ARG);

    // srtp_protect_rtcp() in encryptRtcpPacket() assumes memory availability to write 10 bytes of authentication tag and
    // SRTP_MAX_TRAILER_LEN + 4 following the actual rtcp Packet payload
    allocSize = packetLen + SRTP_AUTH_TAG_OVERHEAD + SRTP_MAX_TRAILER_LEN + 4;
    CHK(NULL != (pRawPacket = (PBYTE) MEMALLOC(allocSize)), STATUS_NOT_ENOUGH_MEMORY);

    MEMCPY(pRawPacket, pPacket, packetLen);

    CHK_STATUS(encryptRtcpPacket(pKvsPeerConnection->pSrtpSession, pRawPacket, (PINT32) &packetLen));
    CHK_STATUS(iceAgentSendPacket(pKvsPeerConnection->pIceAgent, pRawPacket, packetLen));

CleanUp:
    SAFE_MEMFREE(pRawPacket);
    return retStatus;
}

STATUS sendRtcpPLI(PKvsPeerConnection pKvsPeerConnection, UINT32 senderSsrc, UINT32 mediaSsrc)
{
    STATUS retStatus = STATUS_SUCCESS;
    BYTE packet[12];
    UINT32 packetLen = sizeof(packet);

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);

    // RTCP Header
    packet[0] = (RTCP_PACKET_VERSION_VAL << 6) | RTCP_PSFB_PLI; // V=2, P=0, FMT=1 (PLI)
    packet[1] = RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK;     // PT=206
    putUnalignedInt16BigEndian(packet + 2, (packetLen / 4) - 1);

    // SSRC of packet sender
    putUnalignedInt32BigEndian(packet + 4, senderSsrc);

    // SSRC of media source
    putUnalignedInt32BigEndian(packet + 8, mediaSsrc);

    CHK_STATUS(writeRtcpPacket(pKvsPeerConnection, packet, packetLen));

CleanUp:
    return retStatus;
}

STATUS sendRtcpFIR(PKvsPeerConnection pKvsPeerConnection, UINT32 senderSsrc, UINT32 mediaSsrc, UINT8* pSeqNum)
{
    STATUS retStatus = STATUS_SUCCESS;
    BYTE packet[20];
    UINT32 packetLen = sizeof(packet);

    CHK(pKvsPeerConnection != NULL && pSeqNum != NULL, STATUS_NULL_ARG);

    // RTCP Header
    packet[0] = (RTCP_PACKET_VERSION_VAL << 6) | RTCP_PSFB_FIR; // V=2, P=0, FMT=4 (FIR)
    packet[1] = RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK;     // PT=206
    putUnalignedInt16BigEndian(packet + 2, (packetLen / 4) - 1);

    // SSRC of packet sender
    putUnalignedInt32BigEndian(packet + 4, senderSsrc);

    // SSRC of media source (unused, set to 0)
    putUnalignedInt32BigEndian(packet + 8, 0);

    // FCI: SSRC
    putUnalignedInt32BigEndian(packet + 12, mediaSsrc);

    // FCI: Sequence number
    (*pSeqNum)++;
    packet[16] = *pSeqNum;

    // FCI: Reserved (set to 0)
    packet[17] = 0;
    packet[18] = 0;
    packet[19] = 0;

    CHK_STATUS(writeRtcpPacket(pKvsPeerConnection, packet, packetLen));

CleanUp:
    return retStatus;
}

//
// TWCC Feedback Generation (Receiver Side)
//

STATUS createTwccReceiverManager(PTwccReceiverManager* ppManager)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTwccReceiverManager pManager = NULL;

    CHK(ppManager != NULL, STATUS_NULL_ARG);

    pManager = (PTwccReceiverManager) MEMCALLOC(1, SIZEOF(TwccReceiverManager));
    CHK(pManager != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(hashTableCreateWithParams(TWCC_RECEIVER_HASH_BUCKET_COUNT, TWCC_RECEIVER_HASH_BUCKET_LENGTH, &pManager->pReceivedPktsHashTable));

    pManager->firstPacketReceived = FALSE;
    pManager->feedbackPacketCount = 0;
    pManager->firstSeqNum = 0;
    pManager->lastSeqNum = 0;
    pManager->mediaSourceSsrc = 0;

    *ppManager = pManager;
    pManager = NULL;

CleanUp:
    if (pManager != NULL) {
        freeTwccReceiverManager(&pManager);
    }
    return retStatus;
}

static STATUS twccReceiverFreeHashEntry(UINT64 customData, PHashEntry pHashEntry)
{
    UNUSED_PARAM(customData);
    if (pHashEntry != NULL && pHashEntry->value != 0) {
        MEMFREE((PVOID) pHashEntry->value);
    }
    return STATUS_SUCCESS;
}

STATUS freeTwccReceiverManager(PTwccReceiverManager* ppManager)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTwccReceiverManager pManager = NULL;

    CHK(ppManager != NULL, STATUS_NULL_ARG);
    pManager = *ppManager;
    CHK(pManager != NULL, retStatus);

    if (pManager->pReceivedPktsHashTable != NULL) {
        hashTableIterateEntries(pManager->pReceivedPktsHashTable, 0, twccReceiverFreeHashEntry);
        hashTableFree(pManager->pReceivedPktsHashTable);
    }

    MEMFREE(pManager);
    *ppManager = NULL;

CleanUp:
    return retStatus;
}

// Helper function to find a specific extension by ID in one-byte header extension payload (0xBEDE format)
// Returns pointer to extension data (after ID|L byte) or NULL if not found
static PBYTE findOneByteExtension(PBYTE pExtPayload, UINT32 extLength, UINT8 targetId, PUINT8 pDataLen)
{
    UINT32 offset = 0;
    UINT8 id, len;

    while (offset < extLength) {
        UINT8 byte = pExtPayload[offset];

        // Skip padding bytes (0x00)
        if (byte == 0) {
            offset++;
            continue;
        }

        // Parse ID (4 bits) and L (4 bits)
        id = (byte >> 4) & 0x0F;
        len = (byte & 0x0F) + 1; // L=0 means 1 byte of data, L=1 means 2 bytes, etc.

        // ID 15 is reserved for future two-byte header, skip rest
        if (id == 15) {
            break;
        }

        // Check if this is the extension we're looking for
        if (id == targetId) {
            if (pDataLen != NULL) {
                *pDataLen = len;
            }
            return pExtPayload + offset + 1; // Return pointer to data after ID|L byte
        }

        // Move to next extension
        offset += 1 + len;
    }

    return NULL;
}

STATUS twccReceiverOnPacketReceived(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTwccReceiverManager pManager = NULL;
    PTwccReceivedPacketInfo pPacketInfo = NULL;
    UINT16 twccSeqNum = 0;
    UINT64 existingValue = 0;
    PBYTE pTwccExtData = NULL;

    CHK(pKvsPeerConnection != NULL && pRtpPacket != NULL, STATUS_NULL_ARG);
    CHK(pRtpPacket->header.extension && pRtpPacket->header.extensionProfile == TWCC_EXT_PROFILE, retStatus);
    CHK(pRtpPacket->header.extensionPayload != NULL, retStatus);

    pManager = pKvsPeerConnection->pTwccReceiverManager;
    CHK(pManager != NULL, retStatus);

    // Find TWCC extension by ID in the one-byte header extension payload
    pTwccExtData =
        findOneByteExtension(pRtpPacket->header.extensionPayload, pRtpPacket->header.extensionLength, (UINT8) pKvsPeerConnection->twccExtId, NULL);
    if (pTwccExtData == NULL) {
        DLOGW("TWCC extension ID %u not found in payload (extLen=%u)", pKvsPeerConnection->twccExtId, pRtpPacket->header.extensionLength);
        CHK(FALSE, retStatus);
    }

    // Extract TWCC sequence number (16-bit big-endian)
    twccSeqNum = (UINT16) getUnalignedInt16BigEndian(pTwccExtData);
    DLOGD("TWCC packet received: seq=%u", twccSeqNum);

    MUTEX_LOCK(pKvsPeerConnection->twccReceiverLock);

    // Check if packet already exists (duplicate)
    if (STATUS_SUCCEEDED(hashTableGet(pManager->pReceivedPktsHashTable, twccSeqNum, &existingValue))) {
        // Already tracked, skip
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        CHK(FALSE, retStatus);
    }

    // Allocate packet info
    pPacketInfo = (PTwccReceivedPacketInfo) MEMCALLOC(1, SIZEOF(TwccReceivedPacketInfo));
    if (pPacketInfo == NULL) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
    }

    // Set base time once (on first ever packet), then store relative arrival time
    if (pManager->baseTimeKvs == 0) {
        pManager->baseTimeKvs = pRtpPacket->receivedTime;
    }
    pPacketInfo->arrivalTimeKvs = pRtpPacket->receivedTime - pManager->baseTimeKvs;

    // Store in hash table
    retStatus = hashTablePut(pManager->pReceivedPktsHashTable, twccSeqNum, (UINT64) pPacketInfo);
    if (STATUS_FAILED(retStatus)) {
        MEMFREE(pPacketInfo);
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        CHK(FALSE, retStatus);
    }

    // Update sequence number tracking
    if (!pManager->firstPacketReceived) {
        pManager->firstSeqNum = twccSeqNum;
        pManager->lastSeqNum = twccSeqNum;
        pManager->firstPacketReceived = TRUE;
    } else {
        // Handle sequence number wraparound using signed comparison
        INT16 diffFromFirst = (INT16) (twccSeqNum - pManager->firstSeqNum);
        INT16 diffFromLast = (INT16) (twccSeqNum - pManager->lastSeqNum);

        if (diffFromFirst < 0) {
            // New packet is before current first (wraparound or out of order)
            pManager->firstSeqNum = twccSeqNum;
        }
        if (diffFromLast > 0) {
            // New packet is after current last
            pManager->lastSeqNum = twccSeqNum;
        }
    }

    // Store media SSRC for feedback
    pManager->mediaSourceSsrc = pRtpPacket->header.ssrc;

    MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);

CleanUp:
    return retStatus;
}

// Helper function to determine packet status and delta
static TWCC_STATUS_SYMBOL getTwccPacketStatus(UINT64 arrivalTimeKvs, UINT64 referenceTimeKvs, PINT32 pDeltaTicks)
{
    INT64 deltaKvs;
    INT64 deltaTicks;

    // Calculate delta from reference time
    deltaKvs = (INT64) arrivalTimeKvs - (INT64) referenceTimeKvs;

    // Convert from 100ns to 250us ticks
    // 100ns * (1us/10 * 100ns) * (1 tick / 250us) = 100ns / 2500
    deltaTicks = deltaKvs / 2500;

    *pDeltaTicks = (INT32) deltaTicks;

    // Determine which delta encoding to use
    if (deltaTicks >= 0 && deltaTicks <= 255) {
        return TWCC_STATUS_SYMBOL_SMALLDELTA;
    } else if (deltaTicks >= -32768 && deltaTicks <= 32767) {
        return TWCC_STATUS_SYMBOL_LARGEDELTA;
    } else {
        // Delta too large - treat as not received
        return TWCC_STATUS_SYMBOL_NOTRECEIVED;
    }
}

STATUS sendRtcpTwccFeedback(PKvsPeerConnection pKvsPeerConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL hasSrtpSession = FALSE;
    PTwccReceiverManager pManager = NULL;
    PBYTE pPacket = NULL;
    UINT32 packetLen = 0;
    UINT32 allocSize = 0;
    UINT16 baseSeqNum = 0;
    UINT16 packetStatusCount = 0;
    UINT32 currentTime = 0;
    UINT64 referenceTimeKvs = 0;
    UINT32 referenceTime24 = 0;
    UINT16 seqNum = 0;
    UINT32 offset = 0;
    UINT64 packetInfoValue = 0;
    PTwccReceivedPacketInfo pPacketInfo = NULL;
    INT32 deltaTicks = 0;
    TWCC_STATUS_SYMBOL status = TWCC_STATUS_SYMBOL_NOTRECEIVED;
    UINT32 senderSsrc = 0;

    // Arrays to store packet statuses and deltas
    TWCC_STATUS_SYMBOL* pStatuses = NULL;
    INT32* pDeltas = NULL;
    UINT32 i = 0;
    UINT32 chunkOffset = 0;
    UINT32 deltaOffset = 0;
    UINT16 runLength = 0;
    TWCC_STATUS_SYMBOL runStatus = TWCC_STATUS_SYMBOL_NOTRECEIVED;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    hasSrtpSession = pKvsPeerConnection->pSrtpSession != NULL;
    MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);
    CHK(hasSrtpSession, retStatus);

    pManager = pKvsPeerConnection->pTwccReceiverManager;
    CHK(pManager != NULL, retStatus);

    MUTEX_LOCK(pKvsPeerConnection->twccReceiverLock);

    // Check if we have packets to report
    if (!pManager->firstPacketReceived) {
        DLOGD("TWCC sendFeedback: no packets received yet");
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        CHK(FALSE, retStatus);
    }

    baseSeqNum = pManager->firstSeqNum;
    // Calculate packet status count (handles wraparound)
    packetStatusCount = (UINT16) ((pManager->lastSeqNum - pManager->firstSeqNum) + 1);

    // Limit packet status count to avoid huge packets
    if (packetStatusCount > TWCC_MAX_PACKET_STATUS_COUNT) {
        packetStatusCount = TWCC_MAX_PACKET_STATUS_COUNT;
    }

    // Allocate arrays for statuses and deltas
    pStatuses = (TWCC_STATUS_SYMBOL*) MEMCALLOC(packetStatusCount, SIZEOF(TWCC_STATUS_SYMBOL));
    pDeltas = (INT32*) MEMCALLOC(packetStatusCount, SIZEOF(INT32));
    if (pStatuses == NULL || pDeltas == NULL) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        SAFE_MEMFREE(pStatuses);
        SAFE_MEMFREE(pDeltas);
        CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
    }

    // Find first received packet to use as reference time base
    // Per TWCC spec, reference time should be based on arrival time of first packet in report
    seqNum = baseSeqNum;
    referenceTimeKvs = 0;
    BOOL foundPacket = FALSE;
    for (i = 0; i < packetStatusCount; i++) {
        if (STATUS_SUCCEEDED(hashTableGet(pManager->pReceivedPktsHashTable, seqNum, &packetInfoValue))) {
            pPacketInfo = (PTwccReceivedPacketInfo) packetInfoValue;
            referenceTimeKvs = pPacketInfo->arrivalTimeKvs;
            foundPacket = TRUE;
            break;
        }
        seqNum++;
    }

    // If no packets found, bail out
    if (!foundPacket) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        SAFE_MEMFREE(pStatuses);
        SAFE_MEMFREE(pDeltas);
        CHK(FALSE, retStatus);
    }

    // Calculate reference time in 64ms units (24-bit field) for the packet
    // Convert from 100ns to ms, then divide by 64
    currentTime = (UINT32) (referenceTimeKvs / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
    referenceTime24 = (currentTime / TWCC_REFERENCE_TIME_DIVISOR) & 0xFFFFFF;

    // Deltas are incremental: delta[i] = arrival[i] - arrival[i-1]
    // First delta uses 64ms-aligned reference (Chrome reconstructs: referenceTime24 * 64ms + delta * 0.25ms)
    // Arrival times are relative to stream start, so values are small and won't overflow
    UINT64 lastArrivalTimeKvs =
        (UINT64) (currentTime / TWCC_REFERENCE_TIME_DIVISOR) * TWCC_REFERENCE_TIME_DIVISOR * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
    DLOGD("TWCC ref: refTimeKvs=%llu currentTime=%u refTime24=%u lastArrival=%llu", (unsigned long long) referenceTimeKvs, currentTime,
          referenceTime24, (unsigned long long) lastArrivalTimeKvs);
    INT32 minDelta = INT32_MAX, maxDelta = INT32_MIN;
    UINT32 receivedCount = 0, lostCount = 0;
    seqNum = baseSeqNum;
    for (i = 0; i < packetStatusCount; i++) {
        if (STATUS_SUCCEEDED(hashTableGet(pManager->pReceivedPktsHashTable, seqNum, &packetInfoValue))) {
            pPacketInfo = (PTwccReceivedPacketInfo) packetInfoValue;
            status = getTwccPacketStatus(pPacketInfo->arrivalTimeKvs, lastArrivalTimeKvs, &deltaTicks);
            pStatuses[i] = status;
            pDeltas[i] = deltaTicks;
            if (status == TWCC_STATUS_SYMBOL_NOTRECEIVED) {
                DLOGD("TWCC delta overflow: seq=%u i=%u delta=%d arrival=%llu la    stArrival=%llu", seqNum, i, deltaTicks,
                      (unsigned long long) pPacketInfo->arrivalTimeKvs, (unsigned long long) lastArrivalTimeKvs);
                lostCount++;
            } else {
                receivedCount++;
                if (deltaTicks < minDelta)
                    minDelta = deltaTicks;
                if (deltaTicks > maxDelta)
                    maxDelta = deltaTicks;
            }
            lastArrivalTimeKvs = pPacketInfo->arrivalTimeKvs; // Update for next packet's delta
        } else {
            pStatuses[i] = TWCC_STATUS_SYMBOL_NOTRECEIVED;
            pDeltas[i] = 0;
            lostCount++;
        }
        seqNum++;
    }
    DLOGD("TWCC feedback: base=%u count=%u received=%u lost=%u minDelta=%d maxDelta=%d", baseSeqNum, packetStatusCount, receivedCount, lostCount,
          minDelta, maxDelta);

    // Calculate packet size:
    // Header: 20 bytes (4 RTCP header + 4 sender SSRC + 4 media SSRC + 4 base seq + 4 ref time)
    // Chunks: variable (2 bytes each, worst case is status vector with 7 packets = packetStatusCount/7 chunks)
    // Deltas: variable (1 byte for small, 2 bytes for large)
    // Add SRTP overhead
    allocSize = 20; // Fixed header size

    // Estimate chunk size (worst case: all status vectors with 7 packets each)
    allocSize += ((packetStatusCount + 6) / 7) * 2;

    // Estimate delta size (worst case: all large deltas)
    for (i = 0; i < packetStatusCount; i++) {
        if (pStatuses[i] == TWCC_STATUS_SYMBOL_SMALLDELTA) {
            allocSize += 1;
        } else if (pStatuses[i] == TWCC_STATUS_SYMBOL_LARGEDELTA) {
            allocSize += 2;
        }
    }

    // Pad to 32-bit boundary + SRTP overhead
    allocSize = (allocSize + 3) & ~3;
    allocSize += SRTP_AUTH_TAG_OVERHEAD + SRTP_MAX_TRAILER_LEN + 4;

    pPacket = (PBYTE) MEMCALLOC(1, allocSize);
    if (pPacket == NULL) {
        MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);
        SAFE_MEMFREE(pStatuses);
        SAFE_MEMFREE(pDeltas);
        CHK(FALSE, STATUS_NOT_ENOUGH_MEMORY);
    }

    // Get sender SSRC (use first transceiver's receive SSRC or generate one)
    // For simplicity, we'll use a fixed SSRC derived from the connection
    senderSsrc = (UINT32) ((UINT64) pKvsPeerConnection & 0xFFFFFFFF);

    // Build RTCP TWCC Feedback packet
    // Byte 0: V=2, P=0, FMT=15
    pPacket[0] = (RTCP_PACKET_VERSION_VAL << 6) | RTCP_FEEDBACK_MESSAGE_TYPE_TWCC;
    // Byte 1: PT=205 (Generic RTP Feedback)
    pPacket[1] = RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK;
    // Bytes 2-3: Length (fill in later)
    offset = 4;

    // Bytes 4-7: Sender SSRC
    putUnalignedInt32BigEndian(pPacket + offset, senderSsrc);
    offset += 4;

    // Bytes 8-11: Media source SSRC
    putUnalignedInt32BigEndian(pPacket + offset, pManager->mediaSourceSsrc);
    offset += 4;

    // Bytes 12-13: Base sequence number
    putUnalignedInt16BigEndian(pPacket + offset, baseSeqNum);
    offset += 2;

    // Bytes 14-15: Packet status count
    putUnalignedInt16BigEndian(pPacket + offset, packetStatusCount);
    offset += 2;

    // Bytes 16-18: Reference time (24-bit)
    pPacket[offset++] = (referenceTime24 >> 16) & 0xFF;
    pPacket[offset++] = (referenceTime24 >> 8) & 0xFF;
    pPacket[offset++] = referenceTime24 & 0xFF;

    // Byte 19: Feedback packet count
    pPacket[offset++] = pManager->feedbackPacketCount++;

    // Build packet chunks using run-length encoding
    chunkOffset = offset;
    i = 0;
    while (i < packetStatusCount) {
        // Count run of same status
        runStatus = pStatuses[i];
        runLength = 1;
        while (i + runLength < packetStatusCount && pStatuses[i + runLength] == runStatus && runLength < TWCC_MAX_PACKET_STATUS_COUNT) {
            runLength++;
        }

        // Write run-length chunk
        putUnalignedInt16BigEndian(pPacket + chunkOffset, TWCC_MAKE_RUNLEN(runStatus, runLength));
        chunkOffset += 2;
        i += runLength;
    }

    // Build receive deltas
    deltaOffset = chunkOffset;
    for (i = 0; i < packetStatusCount; i++) {
        if (pStatuses[i] == TWCC_STATUS_SYMBOL_SMALLDELTA) {
            pPacket[deltaOffset++] = (UINT8) pDeltas[i];
        } else if (pStatuses[i] == TWCC_STATUS_SYMBOL_LARGEDELTA) {
            putUnalignedInt16BigEndian(pPacket + deltaOffset, (INT16) pDeltas[i]);
            deltaOffset += 2;
        }
    }

    // Pad to 32-bit boundary
    while ((deltaOffset % 4) != 0) {
        pPacket[deltaOffset++] = 0;
    }

    packetLen = deltaOffset;

    // Fill in length field (length in 32-bit words minus 1)
    putUnalignedInt16BigEndian(pPacket + 2, (packetLen / 4) - 1);

    // Clear processed packets from hash table
    seqNum = baseSeqNum;
    for (i = 0; i < packetStatusCount; i++) {
        if (STATUS_SUCCEEDED(hashTableGet(pManager->pReceivedPktsHashTable, seqNum, &packetInfoValue))) {
            pPacketInfo = (PTwccReceivedPacketInfo) packetInfoValue;
            hashTableRemove(pManager->pReceivedPktsHashTable, seqNum);
            MEMFREE(pPacketInfo);
        }
        seqNum++;
    }

    // Reset tracking for next feedback
    pManager->firstPacketReceived = FALSE;
    pManager->firstSeqNum = 0;
    pManager->lastSeqNum = 0;

    MUTEX_UNLOCK(pKvsPeerConnection->twccReceiverLock);

    // Encrypt and send
    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    retStatus = encryptRtcpPacket(pKvsPeerConnection->pSrtpSession, pPacket, (PINT32) &packetLen);
    MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);
    CHK_STATUS(retStatus);

    CHK_STATUS(iceAgentSendPacket(pKvsPeerConnection->pIceAgent, pPacket, packetLen));

CleanUp:
    SAFE_MEMFREE(pPacket);
    SAFE_MEMFREE(pStatuses);
    SAFE_MEMFREE(pDeltas);
    return retStatus;
}

STATUS twccFeedbackCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PKvsPeerConnection pKvsPeerConnection = (PKvsPeerConnection) customData;
    UINT64 delay = 0;
    BOOL srtpReady = FALSE;

    CHK(pKvsPeerConnection != NULL, STATUS_NULL_ARG);

    // Skip if SRTP not ready yet - must lock to avoid race with allocateSrtp
    MUTEX_LOCK(pKvsPeerConnection->pSrtpSessionLock);
    srtpReady = pKvsPeerConnection->pSrtpSession != NULL;
    MUTEX_UNLOCK(pKvsPeerConnection->pSrtpSessionLock);

    if (srtpReady) {
        // Send TWCC feedback (ignore errors - will try again next interval)
        sendRtcpTwccFeedback(pKvsPeerConnection);
    } else {
        DLOGD("TWCC callback: SRTP not ready yet");
    }

    // Reschedule timer with jitter (80-120ms).
    // Write to a local first, then copy under twccLock to avoid racing
    // with closePeerConnection which reads twccFeedbackTimerId.
    {
        UINT32 newTimerId = MAX_UINT32;
        delay = 80 + (RAND() % 40);
        CHK_STATUS(timerQueueAddTimer(pKvsPeerConnection->timerQueueHandle, delay * HUNDREDS_OF_NANOS_IN_A_MILLISECOND,
                                      TIMER_QUEUE_SINGLE_INVOCATION_PERIOD, twccFeedbackCallback, (UINT64) pKvsPeerConnection, &newTimerId));
        MUTEX_LOCK(pKvsPeerConnection->twccLock);
        pKvsPeerConnection->twccFeedbackTimerId = newTimerId;
        MUTEX_UNLOCK(pKvsPeerConnection->twccLock);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}
