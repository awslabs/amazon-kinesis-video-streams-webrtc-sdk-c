#define LOG_CLASS "RtcpPacket"

#include "../Include_i.h"

STATUS setRtcpPacketFromBytes(PBYTE pRawPacket, UINT32 pRawPacketsLen, PRtcpPacket pRtcpPacket) {
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 packetLen = 0;

    CHK(pRtcpPacket != NULL, STATUS_NULL_ARG);
    CHK(pRawPacketsLen >= RTCP_PACKET_HEADER_LEN, STATUS_RTCP_INPUT_PACKET_TOO_SMALL);

    // RTCP packet len is length of packet in 32 bit words - 1
    // We don't assert exact length since this may be a compound packet, it
    // is the callers responsibility to parse subsequent entries
    packetLen = getInt16(*(PUINT16) (pRawPacket + RTCP_PACKET_LEN_OFFSET));
    CHK((packetLen + 1) * RTCP_PACKET_LEN_WORD_SIZE >= pRawPacketsLen, STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH);

    pRtcpPacket->header.version = (pRawPacket[0] >> VERSION_SHIFT) & VERSION_MASK;
    CHK(pRtcpPacket->header.version == RTCP_PACKET_VERSION_VAL, STATUS_RTCP_INPUT_PACKET_INVALID_VERSION);

    pRtcpPacket->header.receptionReportCount = pRawPacket[0] & RTCP_PACKET_RRC_BITMASK;
    pRtcpPacket->header.packetType = pRawPacket[RTCP_PACKET_TYPE_OFFSET];

    pRtcpPacket->payloadLength = packetLen * RTCP_PACKET_LEN_WORD_SIZE;
    pRtcpPacket->payload = pRawPacket + RTCP_PACKET_LEN_WORD_SIZE;

CleanUp:
    LEAVES();
    return retStatus;
}

// Given a RTCP Packet list extract the list of SSRCes, since the list of SSRCes may not be know ahead of time (because of BLP)
// we need to allocate the list dynamically
STATUS rtcpNackListGet(PBYTE pPayload, UINT32 payloadLen, PUINT32 pSenderSsrc, PUINT32 pReceiverSsrc, PUINT16 pSequenceNumberList, PUINT32 pSequenceNumberListLen) {
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

        for(j = 0; j < 16; j++) {
            if ((BLP & ( 1 << j )) >> j) {
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
