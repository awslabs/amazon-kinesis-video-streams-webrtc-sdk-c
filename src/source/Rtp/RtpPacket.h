/*******************************************
RTP Packet include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTP_RTPPACKET_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTP_RTPPACKET_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define MIN_HEADER_LENGTH 12
#define VERSION_SHIFT     6
#define VERSION_MASK      0x3
#define SSRC_OFFSET       8
#define CSRC_LENGTH       4
#define RTP_HEADER_VERSION  2

#define RTP_HEADER_LEN(pRtpPacket)                                                                                                                   \
    (12 + (pRtpPacket)->header.csrcCount * CSRC_LENGTH + ((pRtpPacket)->header.extension ? 4 + (pRtpPacket)->header.extensionLength : 0))

#define RTP_GET_RAW_PACKET_SIZE(pRtpPacket) (RTP_HEADER_LEN(pRtpPacket) + ((pRtpPacket)->payloadLength))

#define GET_UINT16_SEQ_NUM(seqIndex) ((UINT16) ((seqIndex) % (MAX_UINT16 + 1)))

/*
 *
     0                   1                   2                   3
      0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |       0xBE    |    0xDE       |           length=1            |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     |  ID   | L=1   |transport-wide sequence number | zero padding  |
     +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
// https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
#define TWCC_EXT_PROFILE                 0xBEDE
#define TWCC_PAYLOAD(extId, sequenceNum) htonl((((extId) & 0xfu) << 28u) | (1u << 24u) | ((UINT32) (sequenceNum) << 8u))
#define TWCC_SEQNUM(extPayload)          ((UINT16) getUnalignedInt16BigEndian(extPayload + 1))

typedef STATUS (*DepayRtpPayloadFunc)(PBYTE, UINT32, PBYTE, PUINT32, PBOOL);

/*
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|  CC   |M|     PT      |       sequence number         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           synchronization source (SSRC) identifier            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |       contributing source (CSRC[0..15]) identifiers           |
 * |                             ....                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

struct __RtpPacketHeader {
    UINT8 version;
    BOOL padding;
    BOOL extension;
    BOOL marker;
    UINT8 csrcCount;
    UINT8 payloadType;
    UINT16 sequenceNumber;
    UINT32 timestamp;
    UINT32 ssrc;
    PUINT32 csrcArray;
    UINT16 extensionProfile;
    PBYTE extensionPayload;
    UINT32 extensionLength;
};
typedef struct __RtpPacketHeader RtpPacketHeader;
typedef RtpPacketHeader* PRtpPacketHeader;

struct __Payloads {
    PBYTE payloadBuffer;
    UINT32 payloadLength;
    UINT32 maxPayloadLength;
    PUINT32 payloadSubLength;
    UINT32 payloadSubLenSize;
    UINT32 maxPayloadSubLenSize;
};
typedef struct __Payloads PayloadArray;
typedef PayloadArray* PPayloadArray;

typedef struct __RtpPacket RtpPacket;
struct __RtpPacket {
    RtpPacketHeader header;
    PBYTE payload;
    UINT32 payloadLength;
    PBYTE pRawPacket;
    UINT32 rawPacketLength;
    // used for jitterBufferDelay calculation
    UINT64 receivedTime;
    // used for twcc time delta calculation
    UINT64 sentTime;
};
typedef RtpPacket* PRtpPacket;

STATUS createRtpPacket(UINT8, BOOL, BOOL, UINT8, BOOL, UINT8, UINT16, UINT32, UINT32, PUINT32, UINT16, UINT32, PBYTE, PBYTE, UINT32, PRtpPacket*);
STATUS setRtpPacket(UINT8, BOOL, BOOL, UINT8, BOOL, UINT8, UINT16, UINT32, UINT32, PUINT32, UINT16, UINT32, PBYTE, PBYTE, UINT32, PRtpPacket);
STATUS freeRtpPacket(PRtpPacket*);
STATUS createRtpPacketFromBytes(PBYTE, UINT32, PRtpPacket*);
STATUS constructRetransmitRtpPacketFromBytes(PBYTE, UINT32, UINT16, UINT8, UINT32, PRtpPacket*);
STATUS setRtpPacketFromBytes(PBYTE, UINT32, PRtpPacket);
STATUS createBytesFromRtpPacket(PRtpPacket, PBYTE, PUINT32);
STATUS setBytesFromRtpPacket(PRtpPacket, PBYTE, UINT32);
STATUS constructRtpPackets(PPayloadArray, UINT8, UINT16, UINT32, UINT32, PRtpPacket, UINT32);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTP_RTPPACKET_H
