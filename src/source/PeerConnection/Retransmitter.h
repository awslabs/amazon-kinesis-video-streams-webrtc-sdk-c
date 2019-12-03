/*******************************************
Retransmitter internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSIMITTER__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSIMITTER__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

typedef struct {
    PUINT16 sequenceNumberList;
    UINT32 seqNumListLen;
    PUINT64 validIndexList;
    UINT32 validIndexListLen;
} Retransmitter, *PRetransmitter;

STATUS createRetransmitter(UINT32, UINT32, PRetransmitter*);
STATUS freeRetransmitter(PRetransmitter*);
STATUS resendPacketOnNack(PRtcpPacket, PKvsPeerConnection);

#pragma pack(pop, include_i)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RETRANSIMITTER__ */
