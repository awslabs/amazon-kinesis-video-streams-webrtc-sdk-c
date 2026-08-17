/*******************************************
RTCP Packet include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define RTCP_PACKET_LEN_OFFSET  2
#define RTCP_PACKET_TYPE_OFFSET 1

#define RTCP_PACKET_RRC_BITMASK 0x1F

#define RTCP_PACKET_HEADER_LEN 4
#define RTCP_NACK_LIST_LEN     8

#define RTCP_PACKET_VERSION_VAL 2

#define RTCP_PACKET_LEN_WORD_SIZE 4

#define RTCP_PACKET_REMB_MIN_SIZE          16
#define RTCP_PACKET_REMB_IDENTIFIER_OFFSET 8
#define RTCP_PACKET_REMB_MANTISSA_BITMASK  0x3FFFF

// Minimum sender report payload: sender SSRC (4) + NTP timestamp (8) + RTP timestamp (4) +
// sender's packet count (4) + sender's octet count (4) = 24 bytes (RFC 3550 section 6.4.1).
// Reception report blocks, when present, follow this fixed-size sender info section.
#define RTCP_PACKET_SENDER_REPORT_MINLEN 24
// Each reception report block: source SSRC (4) + fraction lost (1) + cumulative lost (3) +
// extended highest seq (4) + interarrival jitter (4) + LSR (4) + DLSR (4) = 24 bytes.
#define RTCP_PACKET_RECEIVER_REPORT_BLOCK_LEN 24
// No longer used by the SDK: receiver report length is now validated dynamically against the
// report block count (4 + RC * RTCP_PACKET_RECEIVER_REPORT_BLOCK_LEN in onRtcpReceiverReport).
// Kept for backward compatibility with any external consumers of this header.
#define RTCP_PACKET_RECEIVER_REPORT_MINLEN 4 + RTCP_PACKET_RECEIVER_REPORT_BLOCK_LEN
// Maximum receiver report blocks processed per RR, keyed to the maximum number of media streams
// a peer connection can negotiate (configurable via -DMAX_SDP_SESSION_MEDIA_COUNT). Report blocks
// beyond the negotiated stream count cannot reference a known transceiver. The RC header field is
// 5 bits, so the protocol-level ceiling is 31 regardless.
#define RTCP_PACKET_RECEIVER_REPORT_MAX_BLOCKS MAX_SDP_SESSION_MEDIA_COUNT

// https://tools.ietf.org/html/rfc3550#section-4
// If the participant has not yet sent an RTCP packet (the variable
// initial is true), the constant Tmin is set to 2.5 seconds, else it
// is set to 5 seconds.
#define RTCP_FIRST_REPORT_DELAY (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)

typedef enum {
    RTCP_PACKET_TYPE_FIR = 192, // https://tools.ietf.org/html/rfc2032#section-5.2.1
    RTCP_PACKET_TYPE_SENDER_REPORT = 200,
    RTCP_PACKET_TYPE_RECEIVER_REPORT = 201, // https://tools.ietf.org/html/rfc3550#section-6.4.2
    RTCP_PACKET_TYPE_SOURCE_DESCRIPTION = 202,
    RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK = 205,
    RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK = 206,
} RTCP_PACKET_TYPE;

typedef enum {
    RTCP_FEEDBACK_MESSAGE_TYPE_NACK = 1,
    RTCP_PSFB_PLI = 1, // https://tools.ietf.org/html/rfc4585#section-6.3
    RTCP_PSFB_SLI = 2, // https://tools.ietf.org/html/rfc4585#section-6.3.2
    RTCP_FEEDBACK_MESSAGE_TYPE_APPLICATION_LAYER_FEEDBACK = 15,
} RTCP_FEEDBACK_MESSAGE_TYPE;

/*
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|    Count   |       PT      |             length         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

typedef struct {
    UINT8 version;
    UINT8 receptionReportCount;
    RTCP_PACKET_TYPE packetType;

    UINT32 packetLength;
} RtcpPacketHeader, *PRtcpPacketHeader;

typedef struct {
    RtcpPacketHeader header;

    PBYTE payload;
    UINT32 payloadLength;
} RtcpPacket, *PRtcpPacket;

STATUS setRtcpPacketFromBytes(PBYTE, UINT32, PRtcpPacket);
STATUS rtcpNackListGet(PBYTE, UINT32, PUINT32, PUINT32, PUINT16, PUINT32);
STATUS rembValueGet(PBYTE, UINT32, PDOUBLE, PUINT32, PUINT8);
STATUS isRembPacket(PBYTE, UINT32);

#define NTP_OFFSET    2208988800ULL
#define NTP_TIMESCALE 4294967296ULL

// converts 100ns precision time to ntp time
UINT64 convertTimestampToNTP(UINT64 time100ns);

#define DLSR_TIMESCALE 65536

// https://tools.ietf.org/html/rfc3550#section-4
// In some fields where a more compact representation is
//   appropriate, only the middle 32 bits are used; that is, the low 16
//   bits of the integer part and the high 16 bits of the fractional part.
#define MID_NTP(ntp_time) (UINT32)((currentTimeNTP >> 16U) & 0xffffffffULL)

#ifdef __cplusplus
}
#endif

#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H
