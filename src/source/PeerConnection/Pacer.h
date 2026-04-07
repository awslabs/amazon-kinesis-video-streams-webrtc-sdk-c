/*******************************************
Pacer – rate-limits outgoing media between
the sample media feed and writeFrame / RTP tx.

Congestion control (TWCC) updates the target
bitrate via pacerSetBitrate().

Usage:
  1. createPacer(...)
  2. pacerStart(...)
  3. In the media thread: pacerEnqueueFrame(...)
     instead of calling writeFrame() directly.
  4. TWCC callback: pacerSetBitrate(...)
  5. pacerStop(...) / freePacer(...)
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT__PACER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT__PACER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PACER_DEFAULT_BITRATE_BPS   (2 * 1024 * 1024)       // 2 Mbps
#define PACER_MIN_BITRATE_BPS       (50 * 1024)              // 50 Kbps
#define PACER_MAX_BITRATE_BPS       (100 * 1024 * 1024)      // 100 Mbps
#define PACER_DEFAULT_MAX_QUEUE_SIZE 200
#define PACER_DRAIN_INTERVAL_US     (5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND) // 5 ms

// Queued frame node.  Two modes:
//   Zero-copy: caller sets pOwnedFrameData to a heap buffer it relinquishes.
//   Copy mode: pacerEnqueueFrame() copies internally (pOwnedFrameData allocated by pacer).
typedef struct __PacerQueueNode PacerQueueNode;
struct __PacerQueueNode {
    Frame frame;              // frame metadata (presentationTs, flags, size)
    PBYTE pOwnedFrameData;    // heap buffer the pacer will free after send
    UINT64 sendCustomData;    // opaque context for the send callback
    PacerQueueNode* pNext;
};
typedef PacerQueueNode* PPacerQueueNode;

// Callback the pacer invokes to actually transmit a frame.
// Signature matches: STATUS fn(UINT64 customData, PFrame pFrame)
typedef STATUS (*PacerSendFrameFn)(UINT64 customData, PFrame pFrame);

typedef struct Pacer {
    volatile ATOMIC_BOOL running;

    // Target send rate (bits/sec) – updated by congestion control
    volatile SIZE_T targetBitrateBps;

    // Token bucket
    MUTEX tokenLock;
    INT64 availableTokens;  // bytes; can go slightly negative
    UINT64 lastRefillTime;  // 100ns precision

    // FIFO frame queue
    MUTEX queueLock;
    CVAR queueCvar;
    PPacerQueueNode pHead;
    PPacerQueueNode pTail;
    UINT32 queueSize;
    UINT32 maxQueueSize;

    // Drain thread
    TID drainThreadId;
    UINT64 drainIntervalUs; // 100ns units

    // Send callback
    PacerSendFrameFn sendFrameFn;

    // Stats
    volatile SIZE_T framesSent;
    volatile SIZE_T framesDropped;
    volatile SIZE_T bytesSent;
} Pacer, *PPacer;

// Lifecycle
STATUS createPacer(UINT64 initialBitrateBps, UINT32 maxQueueSize,
                   PacerSendFrameFn sendFrameFn, PPacer* ppPacer);
STATUS freePacer(PPacer* ppPacer);

// Start / stop the drain thread
STATUS pacerStart(PPacer pPacer);
STATUS pacerStop(PPacer pPacer);

// Enqueue a frame for paced sending (copies frame data internally).
STATUS pacerEnqueueFrame(PPacer pPacer, UINT64 sendCustomData, PFrame pFrame);

// Zero-copy enqueue: caller allocates pFrameData and transfers ownership to the pacer.
// The pacer will MEMFREE(pFrameData) after sending.  pFrame->frameData must equal pFrameData.
STATUS pacerEnqueueFrameZeroCopy(PPacer pPacer, UINT64 sendCustomData, PFrame pFrame, PBYTE pFrameData);

// Called by congestion control (TWCC) to update the pacing rate
STATUS pacerSetBitrate(PPacer pPacer, UINT64 bitrateBps);

// Query
UINT64 pacerGetBitrate(PPacer pPacer);
BOOL pacerIsRunning(PPacer pPacer);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT__PACER_H
