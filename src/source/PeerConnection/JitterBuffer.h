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
typedef STATUS (*FrameDroppedFunc)(UINT64, UINT16, UINT16, UINT32);
#define UINT16_DEC(a) ((UINT16) ((a) - 1))

#define JITTER_BUFFER_HASH_TABLE_BUCKET_COUNT  3000
#define JITTER_BUFFER_HASH_TABLE_BUCKET_LENGTH 2

typedef struct {
    FrameReadyFunc onFrameReadyFn;
    FrameDroppedFunc onFrameDroppedFn;
    DepayRtpPayloadFunc depayPayloadFn;

    // used for calculating interarrival jitter https://tools.ietf.org/html/rfc3550#section-6.4.1
    // https://tools.ietf.org/html/rfc3550#appendix-A.8
    // holds the relative transit time for the previous packet
    UINT64 transit;
    // holds estimated jitter, in clockRate units
    DOUBLE jitter;
    UINT16 headSequenceNumber;
    UINT16 tailSequenceNumber;
    UINT32 headTimestamp;
    UINT32 tailTimestamp;
    // this is set to U64 even though rtp timestamps are U32
    // in order to allow calculations to not cause overflow
    UINT64 maxLatency;
    UINT64 customData;
    UINT32 clockRate;
    BOOL started;
    BOOL firstFrameProcessed;
    BOOL sequenceNumberOverflowState;
    BOOL timestampOverFlowState;
    PHashTable pPkgBufferHashTable;
} JitterBuffer, *PJitterBuffer;

// constructor
STATUS createJitterBuffer(FrameReadyFunc, FrameDroppedFunc, DepayRtpPayloadFunc, UINT32, UINT32, UINT64, PJitterBuffer*);
// destructor
STATUS freeJitterBuffer(PJitterBuffer*);
STATUS jitterBufferPush(PJitterBuffer, PRtpPacket, PBOOL);
STATUS jitterBufferDropBufferData(PJitterBuffer, UINT16, UINT16, UINT32);
STATUS jitterBufferFillFrameData(PJitterBuffer, PBYTE, UINT32, PUINT32, UINT16, UINT16);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT__JITTERBUFFER_H
