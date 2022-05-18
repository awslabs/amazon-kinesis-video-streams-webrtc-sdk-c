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

// https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
// Deltas are represented as multiples of 250us:
#define TWCC_TICKS_PER_SECOND        (1000000LL / 250)
#define MICROSECONDS_PER_SECOND      1000000LL
#define MILLISECONDS_PER_SECOND      1000LL
#define TWCC_PACKET_LOST_TIME        ((UINT64)(-1LL))
#define TWCC_PACKET_UNITIALIZED_TIME 0
#define TWCC_ESTIMATOR_TIME_WINDOW   (1 * HUNDREDS_OF_NANOS_IN_A_SECOND)

typedef enum {
    TWCC_STATUS_SYMBOL_NOTRECEIVED = 0,
    TWCC_STATUS_SYMBOL_SMALLDELTA,
    TWCC_STATUS_SYMBOL_LARGEDELTA,
} TWCC_STATUS_SYMBOL;

#define TWCC_FB_PACKETCHUNK_SIZE               2
#define IS_TWCC_RUNLEN(packetChunk)            ((((packetChunk) >> 15u) & 1u) == 0)
#define TWCC_RUNLEN_STATUS_SYMBOL(packetChunk) (((packetChunk) >> 13u) & 3u)
#define TWCC_RUNLEN_GET(packetChunk)           ((packetChunk) &0x1fffu)
#define TWCC_IS_NOTRECEIVED(statusSymbol)      ((statusSymbol) == TWCC_STATUS_SYMBOL_NOTRECEIVED)
#define TWCC_ISRECEIVED(statusSymbol)          ((statusSymbol) == TWCC_STATUS_SYMBOL_SMALLDELTA || (statusSymbol) == TWCC_STATUS_SYMBOL_LARGEDELTA)
#define TWCC_RUNLEN_ISRECEIVED(packetChunk)    TWCC_ISRECEIVED(TWCC_RUNLEN_STATUS_SYMBOL(packetChunk))
#define TWCC_STATUSVECTOR_IS_2BIT(packetChunk) (((packetChunk) >> 14u) & 1u)
#define TWCC_STATUSVECTOR_SSIZE(packetChunk)   (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 2u : 1u)
#define TWCC_STATUSVECTOR_SMASK(packetChunk)   (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 2u : 1u)
#define TWCC_STATUSVECTOR_STATUS(packetChunk, i)                                                                                                     \
    (((packetChunk) >> (14u - (i) *TWCC_STATUSVECTOR_SSIZE(packetChunk))) & TWCC_STATUSVECTOR_SMASK(packetChunk))
#define TWCC_STATUSVECTOR_COUNT(packetChunk) (TWCC_STATUSVECTOR_IS_2BIT(packetChunk) ? 7 : 14)
#define TWCC_PACKET_STATUS_COUNT(payload)    (getUnalignedInt16BigEndian((payload) + 10))

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTCP__ */
