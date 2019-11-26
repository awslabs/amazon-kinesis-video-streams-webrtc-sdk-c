//
// Sctp
//

#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_SCTP_SCTP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_SCTP_SCTP__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

// 1200 - 12 (SCTP header Size)
#define SCTP_MTU 1188
#define SCTP_ASSOCIATION_DEFAULT_PORT 5000
#define SCTP_DCEP_HEADER_LENGTH 12

#define DEFAULT_USRSCTP_TEARDOWN_POLLING_INTERVAL           10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND

enum {
    SCTP_PPID_DCEP = 50,
    SCTP_PPID_STRING = 51,
    SCTP_PPID_BINARY = 53,
    SCTP_PPID_STRING_EMPTY = 56,
    SCTP_PPID_BINARY_EMPTY = 57
};

enum {
    DCEP_DATA_CHANNEL_OPEN = 0x03,
};

// Callback that is fired when SCTP Association wishes to send packet
typedef VOID (*SctpSessionOutboundPacketFunc)(UINT64, PBYTE, UINT32);

// Callback that is fired when SCTP has a new DataChannel
// Argument is ChannelID and ChannelName + Len
typedef VOID (*SctpSessionDataChannelOpenFunc)(UINT64, UINT32, PBYTE, UINT32);

// Callback that is fired when SCTP has a DataChannel Message.
// Argument is ChannelID and Message + Len
typedef VOID (*SctpSessionDataChannelMessageFunc)(UINT64, UINT32, BOOL, PBYTE, UINT32);

typedef struct {
    UINT64 customData;
    SctpSessionOutboundPacketFunc outboundPacketFunc;
    SctpSessionDataChannelOpenFunc dataChannelOpenFunc;
    SctpSessionDataChannelMessageFunc dataChannelMessageFunc;
} SctpSessionCallbacks, *PSctpSessionCallbacks;

typedef struct {
    struct socket *socket;
    SctpSessionCallbacks sctpSessionCallbacks;
} SctpSession, *PSctpSession;

STATUS createSctpSession(PSctpSessionCallbacks, PSctpSession*);
STATUS freeSctpSession(PSctpSession*);
STATUS putSctpPacket(PSctpSession, PBYTE, UINT32);
STATUS sctpSessionWriteMessage(PSctpSession, UINT32, BOOL, PBYTE, UINT32);

// Callbacks used by usrsctp
INT32 onSctpOutboundPacket(PVOID, PVOID, ULONG, UINT8, UINT8);
INT32 onSctpInboundPacket(struct socket*, union sctp_sockstore, PVOID, ULONG, struct sctp_rcvinfo, INT32, PVOID);

#pragma pack(pop, include_i)

#ifdef  __cplusplus
}
#endif
#endif  //__KINESIS_VIDEO_WEBRTC_CLIENT_SCTP_SCTP__
