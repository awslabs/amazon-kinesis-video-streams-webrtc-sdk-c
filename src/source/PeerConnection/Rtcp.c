#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, INT32 pBuffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcpPacket rtcpPacket;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    MEMSET(&rtcpPacket, 0x00, SIZEOF(rtcpPacket));
    CHK_STATUS(setRtcpPacketFromBytes(pBuff, pBuffLen, &rtcpPacket));

    if (rtcpPacket.header.packetType == RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK
        && rtcpPacket.header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK) {
        CHK_STATUS(resendPacketOnNack(&rtcpPacket, pKvsPeerConnection));
    }
CleanUp:

    return retStatus;
}
