/*******************************************
Packet pacer internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_PACER__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_PACER__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// How often the pacer timer sends queued packets
#define PACER_PROCESS_INTERVAL (5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// The pacing rate never goes below this, so a burst drains quickly even before the media rate is known
#define PACER_MIN_PACING_RATE_BPS 1000000

// Packets beyond this are rejected with STATUS_PEERCONNECTION_PACER_QUEUE_FULL
#define PACER_MAX_QUEUE_PACKETS 4096

// The media rate is measured over windows of this length and smoothed with PACER_RATE_SMOOTHING_FACTOR (about 1 s)
#define PACER_RATE_WINDOW           (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define PACER_RATE_SMOOTHING_FACTOR 0.1

// Unused budget is capped at this much sending time, so an idle period doesn't turn into a burst
#define PACER_MAX_BURST_TIME (2 * PACER_PROCESS_INTERVAL)

/**
 * Sends one packet that has already been serialized and encrypted
 */
typedef STATUS (*PacerSendPacketFunc)(UINT64, PBYTE, UINT32);

/**
 * Called after a packet that carries a TWCC sequence number was sent, with its TWCC extension payload,
 * RTP payload length and send time
 */
typedef VOID (*PacerPacketSentFunc)(UINT64, UINT32, UINT32, UINT64);

typedef struct {
    DOUBLE pacingFactor;     // Pacing rate as a multiple of the measured media rate
    UINT64 minPacingRateBps; // Floor for the pacing rate
    UINT64 maxQueueDelay;    // The pacer speeds up so that no packet waits longer than this (100 ns units)
    UINT32 maxQueuePackets;  // Queue limit in packets
} PacerConfig, *PPacerConfig;

typedef struct __PacerPacket PacerPacket;
struct __PacerPacket {
    PacerPacket* pNext;
    UINT64 enqueueTime;
    BOOL hasTwcc;
    UINT32 twccExtPayload;
    UINT32 payloadLength;
    UINT32 packetLen;
    // The packet bytes follow the struct in the same allocation
};
typedef PacerPacket* PPacerPacket;

typedef struct {
    MUTEX lock;
    PacerConfig config;
    PacerSendPacketFunc sendPacketFn;
    PacerPacketSentFunc packetSentFn;
    UINT64 customData;

    // FIFO queue of packets waiting to be sent
    PPacerPacket pHead;
    PPacerPacket pTail;
    UINT32 queuedPackets;
    UINT64 queuedBytes;

    // Sending budget in bytes. It goes negative after a packet larger than the remaining budget is sent,
    // and the next packets wait until it is positive again
    INT64 budgetBytes;
    BOOL processed;
    UINT64 lastProcessTime;

    // Media rate measured from the bytes enqueued
    BOOL rateWindowStarted;
    UINT64 rateWindowStart;
    UINT64 rateWindowBytes;
    BOOL mediaRateKnown;
    UINT64 mediaRateBps;
    UINT64 pacingRateBps;
} Pacer, *PPacer;

STATUS createPacer(PPacerConfig, PacerSendPacketFunc, PacerPacketSentFunc, UINT64, PPacer*);
STATUS freePacer(PPacer*);

/**
 * Copies a serialized, encrypted packet into the queue
 *
 * @param - PPacer - IN - Pacer
 * @param - PBYTE - IN - Packet bytes
 * @param - UINT32 - IN - Packet length
 * @param - BOOL - IN - Whether the packet carries a TWCC sequence number
 * @param - UINT32 - IN - TWCC extension payload (ignored when the packet has no TWCC sequence number)
 * @param - UINT32 - IN - RTP payload length, reported back for TWCC
 * @param - UINT64 - IN - Current time
 *
 * @return - STATUS - STATUS_PEERCONNECTION_PACER_QUEUE_FULL if the queue is full
 */
STATUS pacerEnqueuePacket(PPacer, PBYTE, UINT32, BOOL, UINT32, UINT32, UINT64);

/**
 * Sends the queued packets the budget allows. Called every PACER_PROCESS_INTERVAL.
 *
 * @param - PPacer - IN - Pacer
 * @param - UINT64 - IN - Current time
 */
STATUS pacerProcess(PPacer, UINT64);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_PACER__ */
