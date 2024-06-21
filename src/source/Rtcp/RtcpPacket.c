#define LOG_CLASS "RtcpPacket"

#include "../Include_i.h"
#include "kvsrtcp/rtcp_api.h"

STATUS setRtcpPacketFromBytes(PBYTE pRawPacket, UINT32 pRawPacketsLen, PRtcpPacket pRtcpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 packetLen = 0;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpResult = Rtcp_DeSerialize(&ctx, pRawPacket, pRawPacketsLen, &rtcpPacket);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));
    pRtcpPacket->header.version = RTCP_HEADER_VERSION;
    pRtcpPacket->header.receptionReportCount = rtcpPacket.header.receptionReportCount;
    pRtcpPacket->header.packetType = rtcpPacket.header.packetType;
    pRtcpPacket->header.packetLength = rtcpPacket.header.packetLength;
    pRtcpPacket->payloadLength = rtcpPacket.payloadLength;
    pRtcpPacket->payload = rtcpPacket.pPayload;

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS setBytesFromRtcpValues(PBYTE pRawPacket, UINT32 rawPacketsLen, UINT32 packetLen, UINT32 ssrc, UINT64 ntpTime, UINT64 rtpTime,
                              UINT32 packetCount, UINT32 octetCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;
    RtcpPacket_t rtcpPacket;
    RtcpSenderReport_t senderReport;

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    MEMSET(&rtcpPacket, 0, SIZEOF(RtcpPacket_t));
    MEMSET(&senderReport, 0, SIZEOF(RtcpSenderReport_t));

    rtcpPacket.header.packetLength = (packetLen / RTCP_PACKET_LEN_WORD_SIZE) - 1; // The length of this RTCP packet in 32-bit words minus one
    rtcpPacket.header.packetType = RTCP_PACKET_TYPE_SENDER_REPORT;
    rtcpPacket.pPayload = &(pRawPacket[RTCP_HEADER_LENGTH]);

    senderReport.ssrc = ssrc;
    senderReport.ntpTime = ntpTime;
    senderReport.rtpTime = (UINT32) rtpTime;
    senderReport.octetCount = octetCount;
    senderReport.packetCount = packetCount;

    rtcpResult = Rtcp_CreatePayloadSenderReport(&ctx, &rtcpPacket, rawPacketsLen - RTCP_HEADER_LENGTH, &senderReport);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpResult = Rtcp_Serialize(&ctx, &rtcpPacket, pRawPacket, (size_t*) &rawPacketsLen);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

CleanUp:
    LEAVES();
    return retStatus;
}

// Given a RTCP Packet list extract the list of SSRCes, since the list of SSRCes may not be know ahead of time (because of BLP)
// we need to allocate the list dynamically
STATUS rtcpNackListGet(PBYTE pPayload, UINT32 payloadLen, PUINT32 pSenderSsrc, PUINT32 pReceiverSsrc, PUINT16 pSequenceNumberList,
                       PUINT32 pSequenceNumberListLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 j;
    UINT16 currentSequenceNumber;
    UINT16 sequenceNumberCount = 0, BLP;
    UINT32 i = RTCP_NACK_LIST_LEN;

    CHK(pPayload != NULL && pSequenceNumberListLen != NULL && pSenderSsrc != NULL && pReceiverSsrc != NULL, STATUS_NULL_ARG);
    CHK(payloadLen >= RTCP_NACK_LIST_LEN && (payloadLen % 4 == 0), STATUS_RTCP_INPUT_NACK_LIST_INVALID);

    *pSenderSsrc = getInt32(*(PUINT32) pPayload);
    *pReceiverSsrc = getInt32(*(PUINT32) (pPayload + 4));

    for (; i < payloadLen; i += 4) {
        currentSequenceNumber = getInt16(*(PUINT16) (pPayload + i));
        BLP = getInt16(*(PUINT16) (pPayload + i + 2));

        // If pSsrcList is not NULL and we have space push and increment
        if (pSequenceNumberList != NULL && sequenceNumberCount <= *pSequenceNumberListLen) {
            pSequenceNumberList[sequenceNumberCount] = currentSequenceNumber;
        }
        sequenceNumberCount++;

        for (j = 0; j < 16; j++) {
            if ((BLP & (1 << j)) >> j) {
                if (pSequenceNumberList != NULL && sequenceNumberCount <= *pSequenceNumberListLen) {
                    pSequenceNumberList[sequenceNumberCount] = (currentSequenceNumber + j + 1);
                }
                sequenceNumberCount++;
            }
        }
    }

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        *pSequenceNumberListLen = sequenceNumberCount;
    }

    LEAVES();
    return retStatus;
}

// Assert that Application Layer Feedback payload is REMB
STATUS isRembPacket(PBYTE pPayload, UINT32 payloadLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    const BYTE rembUniqueIdentifier[] = {0x52, 0x45, 0x4d, 0x42};

    CHK(pPayload != NULL, STATUS_NULL_ARG);
    CHK(payloadLen >= RTCP_PACKET_REMB_MIN_SIZE, STATUS_RTCP_INPUT_REMB_TOO_SMALL);
    CHK(MEMCMP(rembUniqueIdentifier, pPayload + RTCP_PACKET_REMB_IDENTIFIER_OFFSET, SIZEOF(rembUniqueIdentifier)) == 0,
        STATUS_RTCP_INPUT_REMB_INVALID);

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Get values from RTCP Payload
 *
 * Parameters:
 *     pPayload         - REMB Payload
 *     payloadLen       - Total length of payload
 *     pMaximumBitRate  - REMB Value
 *     pSsrcList        - buffer to write list of SSRCes into.
 *     pSsrcListLen     - destination PUINT32 to store the count of SSRCes from the incoming REMB.
 */
STATUS rembValueGet(PBYTE pPayload, UINT32 payloadLen, PDOUBLE pMaximumBitRate, PUINT32 pSsrcList, PUINT8 pSsrcListLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 ssrcListLen = 0, exponent = 0;
    UINT32 mantissa = 0, i;
    DOUBLE maximumBitRate = 0;
    PUINT32 pSsrcListRead;
    RtcpContext_t ctx;
    RtcpResult_t rtcpResult;

    CHK(pPayload != NULL && pMaximumBitRate != NULL && pSsrcListLen != NULL, STATUS_NULL_ARG);
    CHK(payloadLen >= RTCP_PACKET_REMB_MIN_SIZE, STATUS_RTCP_INPUT_REMB_TOO_SMALL);

    rtcpResult = Rtcp_Init(&ctx);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));

    rtcpResult = Rtcp_ParseRembPacket(&ctx, pPayload, payloadLen, (size_t*) &ssrcListLen, &pSsrcListRead, &mantissa, &exponent);
    CHK(rtcpResult == RTP_RESULT_OK, convertRtcpErrorCode(rtcpResult));
    maximumBitRate = mantissa << exponent;

    for (i = 0; i < ssrcListLen; i++) {
        pSsrcList[i] = getInt32(*(PUINT32) & (pSsrcListRead[i]));
    }

CleanUp:
    if (STATUS_SUCCEEDED(retStatus)) {
        *pSsrcListLen = ssrcListLen;
        *pMaximumBitRate = maximumBitRate;
    }

    LEAVES();
    return retStatus;
}

// converts 100ns precision time to ntp time
UINT64 convertTimestampToNTP(UINT64 time100ns)
{
    UINT64 sec = time100ns / HUNDREDS_OF_NANOS_IN_A_SECOND;
    UINT64 _100ns = time100ns % HUNDREDS_OF_NANOS_IN_A_SECOND;

    UINT64 ntp_sec = sec + NTP_OFFSET;
    UINT64 ntp_frac = KVS_CONVERT_TIMESCALE(_100ns, HUNDREDS_OF_NANOS_IN_A_SECOND, NTP_TIMESCALE);
    return (ntp_sec << 32U | ntp_frac);
}
