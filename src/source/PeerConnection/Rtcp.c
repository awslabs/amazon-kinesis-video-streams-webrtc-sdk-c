#define LOG_CLASS "RtcRtcp"

#include "../Include_i.h"

STATUS onRtcpPacket(PKvsPeerConnection pKvsPeerConnection, PBYTE pBuff, INT32 pBuffLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PRtcpPacket pRtcpPacket = NULL;

    CHK(pKvsPeerConnection != NULL && pBuff != NULL, STATUS_NULL_ARG);

    pRtcpPacket = MEMALLOC(SIZEOF(RtcpPacket));
    CHK_STATUS(setRtcpPacketFromBytes(pBuff, pBuffLen, pRtcpPacket));

    if (pRtcpPacket->header.packetType == RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK
        && pRtcpPacket->header.receptionReportCount == RTCP_FEEDBACK_MESSAGE_TYPE_NACK) {
        resendPacketOnNack(pRtcpPacket, pKvsPeerConnection);
    }
CleanUp:
     if (pRtcpPacket != NULL) {
         SAFE_MEMFREE(pRtcpPacket);
     }

    return retStatus;
}
