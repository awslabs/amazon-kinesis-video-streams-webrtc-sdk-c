#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, UINT32 buffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;
    UINT32 currentOffset = 0;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    while (currentOffset < buffLen) {
        CHK_STATUS(setRtcpPacketFromBytes(pBuff + currentOffset, buffLen - currentOffset, &rtcpPacket));

        if (rtcpPacket.header.packetType == RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK && rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK) {
            CHK_STATUS(resendPacketOnNack(&rtcpPacket, pKvsPeerConnection));
        }

        currentOffset += (rtcpPacket.payloadLength + RTCP_PACKET_HEADER_LEN);
    }

CleanUp:
    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}
