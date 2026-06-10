#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTCP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTCP__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

STATUS onRtcpPacket(PKvsPeerConnection, PBYTE, UINT32);
STATUS onRtcpRembPacket(PRtcpPacket, PKvsPeerConnection);
STATUS onRtcpPLIPacket(PRtcpPacket, PKvsPeerConnection);
STATUS parseRtcpTwccPacket(PRtcpPacket, PTwccManager);
STATUS onRtcpTwccPacket(PRtcpPacket, PKvsPeerConnection);
STATUS updateTwccHashTable(PTwccManager, PINT64, PUINT64, PUINT64, PUINT64, PUINT64);

/**
 * @brief Estimates network congestion trend using least-squares linear regression
 *        on accumulated one-way delay variations, smoothed with an exponential moving average (EMA).
 *
 * Algorithm overview:
 *   1. For each consecutive packet pair (i-1, i), compute the inter-arrival delay variation:
 *        d_i = (recvTime_i - recvTime_{i-1}) - (sendTime_i - sendTime_{i-1})
 *      A positive d_i means the network took longer to deliver packet i than packet i-1.
 *
 *   2. Accumulate these variations into a running sum (the "accumulated delay"):
 *        D_i = D_{i-1} + d_i    (where D_0 = 0)
 *      This represents the total queuing delay buildup over the observation window.
 *
 *   3. Fit a line to the points (x_i, y_i) = (i, D_i) using ordinary least-squares regression.
 *      The slope formula is:
 *        rawSlope = (n * sum(x_i * y_i) - sum(x_i) * sum(y_i))
 *                   / (n * sum(x_i^2) - (sum(x_i))^2)
 *      A positive slope indicates delay is growing (congestion building up).
 *      A near-zero slope indicates stable network conditions.
 *      A negative slope indicates delay is decreasing (congestion clearing).
 *
 *   4. Smooth the slope with an EMA to reduce noise:
 *        smoothedSlope = alpha * rawSlope + (1 - alpha) * prevSmoothedSlope
 *      where alpha = TWCC_TRENDLINE_SMOOTHING_FACTOR (0.2 by default).
 *
 * @param[in]  pTwccManager  TWCC manager containing packet timing history
 * @param[out] pDelayTrend   Smoothed slope in ms per sample (positive = congestion building)
 * @param[out] pQueueDelay   Total accumulated delay variation in ms over the window
 */
STATUS computeTwccTrendline(PTwccManager, PDOUBLE, PDOUBLE);

// https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
// Deltas are represented as multiples of 250us:
#define TWCC_TICKS_PER_SECOND        (1000000LL / 250)
#define MICROSECONDS_PER_SECOND      1000000LL
#define MILLISECONDS_PER_SECOND      1000LL
#define TWCC_PACKET_LOST_TIME        ((UINT64) (-1LL))
#define TWCC_PACKET_UNITIALIZED_TIME 0
// Max packet age in the TWCC hash table. If feedback is not received within this
// window, the entry is evicted. Entries are also evicted once we receive feedback.
#define TWCC_ESTIMATOR_TIME_WINDOW (4 * HUNDREDS_OF_NANOS_IN_A_SECOND)
// Hard upper bound on packets reported in a single TWCC feedback to prevent
// too large allocations via crafted packetStatusCount (UINT16 field).
// 2048 is well above any legitimate report (~270 pkts/sec at typical rates)
// while capping allocation to ~45 KB instead of ~1.4 MB at UINT16 max.
#define TWCC_MAX_PACKET_STATUS_COUNT 2048
// Minimum payload length for a TWCC feedback packet (two SSRCs + base seq + status count + ref time + fb pkt count)
#define TWCC_FB_PAYLOAD_MIN_LEN 16

// Trendline estimation parameters
#define TWCC_TRENDLINE_SMOOTHING_FACTOR 0.2

typedef enum {
    TWCC_STATUS_SYMBOL_NOTRECEIVED = 0,
    TWCC_STATUS_SYMBOL_SMALLDELTA,
    TWCC_STATUS_SYMBOL_LARGEDELTA,
} TWCC_STATUS_SYMBOL;

#define TWCC_FB_PACKETCHUNK_SIZE               2
#define IS_TWCC_RUNLEN(packetChunk)            ((((packetChunk) >> 15u) & 1u) == 0)
#define TWCC_RUNLEN_STATUS_SYMBOL(packetChunk) (((packetChunk) >> 13u) & 3u)
#define TWCC_RUNLEN_GET(packetChunk)           ((packetChunk) & 0x1fffu)
#define TWCC_IS_NOTRECEIVED(statusSymbol)      ((statusSymbol) == TWCC_STATUS_SYMBOL_NOTRECEIVED)
#define TWCC_ISRECEIVED(statusSymbol)          ((statusSymbol) == TWCC_STATUS_SYMBOL_SMALLDELTA || (statusSymbol) == TWCC_STATUS_SYMBOL_LARGEDELTA)
#define TWCC_RUNLEN_ISRECEIVED(packetChunk)    TWCC_ISRECEIVED(TWCC_RUNLEN_STATUS_SYMBOL(packetChunk))
#define TWCC_STATUSVECTOR_IS_2BIT(packetChunk) (((packetChunk) >> 14u) & 1u)
#define TWCC_STATUSVECTOR_SSIZE(packetChunk)   (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 2u : 1u)
#define TWCC_STATUSVECTOR_SMASK(packetChunk)   (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 2u : 1u)
#define TWCC_STATUSVECTOR_STATUS(packetChunk, i)                                                                                                     \
    (((packetChunk) >> (14u - (i) * TWCC_STATUSVECTOR_SSIZE(packetChunk))) & TWCC_STATUSVECTOR_SMASK(packetChunk))
#define TWCC_STATUSVECTOR_COUNT(packetChunk) (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 7 : 14)
#define TWCC_PACKET_STATUS_COUNT(payload)    (getUnalignedInt16BigEndian((payload) + 10))

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTCP__ */
