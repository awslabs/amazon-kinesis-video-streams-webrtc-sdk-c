#define LOG_CLASS "RtcpPacket"

#include "../Include_i.h"

STATUS setRtcpPacketFromBytes(PBYTE pRawPacket, UINT32 pRawPacketsLen, PRtcpPacket pRtcpPacket)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 packetLen = 0;

    CHK(pRtcpPacket != NULL, STATUS_NULL_ARG);
    CHK(pRawPacketsLen >= RTCP_PACKET_HEADER_LEN, STATUS_RTCP_INPUT_PACKET_TOO_SMALL);

    // RTCP packet len is length of packet in 32 bit words - 1
    // We don't assert exact length since this may be a compound packet, it
    // is the callers responsibility to parse subsequent entries
    packetLen = getInt16(*(PUINT16) (pRawPacket + RTCP_PACKET_LEN_OFFSET));
    CHK((packetLen + 1) * RTCP_PACKET_LEN_WORD_SIZE <= pRawPacketsLen, STATUS_RTCP_INPUT_PARTIAL_PACKET);

    pRtcpPacket->header.version = (pRawPacket[0] >> VERSION_SHIFT) & VERSION_MASK;
    CHK(pRtcpPacket->header.version == RTCP_PACKET_VERSION_VAL, STATUS_RTCP_INPUT_PACKET_INVALID_VERSION);

    pRtcpPacket->header.receptionReportCount = pRawPacket[0] & RTCP_PACKET_RRC_BITMASK;
    pRtcpPacket->header.packetType = pRawPacket[RTCP_PACKET_TYPE_OFFSET];
    pRtcpPacket->header.packetLength = packetLen;

    pRtcpPacket->payloadLength = packetLen * RTCP_PACKET_LEN_WORD_SIZE;
    pRtcpPacket->payload = pRawPacket + RTCP_PACKET_LEN_WORD_SIZE;

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

    CHK(pPayload != NULL && pMaximumBitRate != NULL && pSsrcListLen != NULL, STATUS_NULL_ARG);
    CHK(payloadLen >= RTCP_PACKET_REMB_MIN_SIZE, STATUS_RTCP_INPUT_REMB_TOO_SMALL);

    MEMCPY(&mantissa, pPayload + RTCP_PACKET_REMB_IDENTIFIER_OFFSET + SIZEOF(UINT32), SIZEOF(UINT32));
    mantissa = htonl(mantissa);
    mantissa &= RTCP_PACKET_REMB_MANTISSA_BITMASK;

    exponent = pPayload[RTCP_PACKET_REMB_IDENTIFIER_OFFSET + SIZEOF(UINT32) + SIZEOF(BYTE)] >> 2;
    maximumBitRate = mantissa << exponent;

    // Only populate SSRC list if caller requests
    ssrcListLen = pPayload[RTCP_PACKET_REMB_IDENTIFIER_OFFSET + SIZEOF(UINT32)];
    CHK(payloadLen >= RTCP_PACKET_REMB_MIN_SIZE + (ssrcListLen * SIZEOF(UINT32)), STATUS_RTCP_INPUT_REMB_INVALID);

    for (i = 0; i < ssrcListLen; i++) {
        pSsrcList[i] = getInt32(*(PUINT32) (pPayload + RTCP_PACKET_REMB_IDENTIFIER_OFFSET + 8 + (i * SIZEOF(UINT32))));
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
