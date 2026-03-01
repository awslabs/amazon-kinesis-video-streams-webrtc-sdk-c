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
STATUS sendRtcpPLI(PKvsPeerConnection pKvsPeerConnection, UINT32 senderSsrc, UINT32 mediaSsrc);
STATUS sendRtcpFIR(PKvsPeerConnection pKvsPeerConnection, UINT32 senderSsrc, UINT32 mediaSsrc, UINT8* pSeqNum);

// TWCC feedback generation (receiver side)
STATUS createTwccReceiverManager(PTwccReceiverManager* ppManager);
STATUS freeTwccReceiverManager(PTwccReceiverManager* ppManager);
STATUS twccReceiverOnPacketReceived(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket);
STATUS sendRtcpTwccFeedback(PKvsPeerConnection pKvsPeerConnection);
STATUS twccFeedbackCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData);

// https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
// Deltas are represented as multiples of 250us:
#define TWCC_TICKS_PER_SECOND        (1000000LL / 250)
#define MICROSECONDS_PER_SECOND      1000000LL
#define MILLISECONDS_PER_SECOND      1000LL
#define TWCC_PACKET_LOST_TIME        ((UINT64) (-1LL))
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

// TWCC feedback generation constants
#define TWCC_FEEDBACK_INITIAL_DELAY      (100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define TWCC_FEEDBACK_INTERVAL_MS        100
#define TWCC_RECEIVER_HASH_BUCKET_COUNT  100
#define TWCC_RECEIVER_HASH_BUCKET_LENGTH 2
#define TWCC_REFERENCE_TIME_DIVISOR      64     // 64ms granularity for reference time
#define TWCC_MAX_PACKET_STATUS_COUNT     0x1FFF // Max run-length count (13 bits)

// TWCC chunk encoding macros for feedback generation
// Run-length chunk: bit 15 = 0, bits 14-13 = status symbol, bits 12-0 = run length
#define TWCC_MAKE_RUNLEN(status, count) ((UINT16) (((status) << 13) | ((count) & 0x1FFF)))
// Status vector chunk (2-bit): bit 15 = 1, bit 14 = 1, bits 13-0 = 7 status symbols (2 bits each)
#define TWCC_MAKE_STATUS_VECTOR_2BIT(symbols) ((UINT16) (0xC000 | ((symbols) & 0x3FFF)))
// Status vector chunk (1-bit): bit 15 = 1, bit 14 = 0, bits 13-0 = 14 status symbols (1 bit each)
#define TWCC_MAKE_STATUS_VECTOR_1BIT(symbols) ((UINT16) (0x8000 | ((symbols) & 0x3FFF)))

// TWCC feedback packet type
#define RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK 205
#define RTCP_FEEDBACK_MESSAGE_TYPE_TWCC       15

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTCP__ */
