/*******************************************
PeerConnection internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT__JITTERBUFFER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT__JITTERBUFFER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef STATUS (*FrameReadyFunc)(UINT64, UINT16, UINT16, UINT32);
typedef STATUS (*FrameDroppedFunc)(UINT64, UINT32);
#define UINT16_DEC(a) ((UINT16)((a) -1))

typedef struct {
    PRtpPacket pktBuffer[MAX_SEQUENCE_NUM + 1];
    FrameReadyFunc onFrameReadyFn;
    FrameDroppedFunc onFrameDroppedFn;
    DepayRtpPayloadFunc depayPayloadFn;

    UINT32 lastPushTimestamp;
    UINT16 lastRemovedSequenceNumber;
    UINT16 lastPopSequenceNumber;
    UINT32 lastPopTimestamp;
    UINT64 maxLatency;
    UINT64 customData;
    UINT32 clockRate;
    BOOL started;
} JitterBuffer, *PJitterBuffer;

STATUS createJitterBuffer(FrameReadyFunc, FrameDroppedFunc, DepayRtpPayloadFunc, UINT32, UINT32, UINT64, PJitterBuffer*);
STATUS freeJitterBuffer(PJitterBuffer*);
STATUS jitterBufferPush(PJitterBuffer, PRtpPacket);
STATUS jitterBufferPop(PJitterBuffer, BOOL);
STATUS jitterBufferDropBufferData(PJitterBuffer, UINT16, UINT16, UINT32);
STATUS jitterBufferFillFrameData(PJitterBuffer, PBYTE, UINT32, PUINT32, UINT16, UINT16);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT__JITTERBUFFER_H
