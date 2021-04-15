/*******************************************
Retransmitter internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSMITTER__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSMITTER__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PUINT16 sequenceNumberList;
    UINT32 seqNumListLen;
    UINT32 validIndexListLen;
    PUINT64 validIndexList;
} Retransmitter, *PRetransmitter;

STATUS createRetransmitter(UINT32, UINT32, PRetransmitter*);
STATUS freeRetransmitter(PRetransmitter*);
STATUS resendPacketOnNack(PRtcpPacket, PKvsPeerConnection);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSMITTER__ */
