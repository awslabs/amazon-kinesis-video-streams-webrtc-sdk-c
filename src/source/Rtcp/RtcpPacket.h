/*******************************************
RTCP Packet include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

#define RTCP_PACKET_LEN_OFFSET  2
#define RTCP_PACKET_TYPE_OFFSET 1

#define RTCP_PACKET_RRC_BITMASK 0x1F

#define RTCP_PACKET_HEADER_LEN  4
#define RTCP_NACK_LIST_LEN  8

#define RTCP_PACKET_VERSION_VAL 2

#define RTCP_PACKET_LEN_WORD_SIZE 4

#define RTCP_PACKET_REMB_MIN_SIZE 16
#define RTCP_PACKET_REMB_IDENTIFIER_OFFSET 8
#define RTCP_PACKET_REMB_MANTISSA_BITMASK 0x3FFFF

typedef enum {
    RTCP_PACKET_TYPE_SENDER_REPORT = 200,
    RTCP_PACKET_TYPE_SOURCE_DESCRIPTION = 202,
    RTCP_PACKET_TYPE_GENERIC_RTP_FEEDBACK = 205,
    RTCP_PACKET_TYPE_PAYLOAD_SPECIFIC_FEEDBACK = 206,
} RTCP_PACKET_TYPE;

typedef enum {
    RTCP_FEEDBACK_MESSAGE_TYPE_NACK = 1,
    RTCP_PSFB_PLI = 1, //https://tools.ietf.org/html/rfc4585#section-6.3
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

#ifdef  __cplusplus
}
#endif

#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_RTCPPACKET_H
