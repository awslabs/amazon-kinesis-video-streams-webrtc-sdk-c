/*******************************************
DataChannel internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_CHANNEL_PROTOCOL_STR (PCHAR) "SCTP"

// Messages held for a channel whose onMessage is not set yet (a negotiated channel opened on a live association)
#define DATA_CHANNEL_MAX_PENDING_MESSAGES 64
#define DATA_CHANNEL_MAX_PENDING_BYTES    (64 * 1024)

// A held message; its payload follows the struct
typedef struct __DataChannelPendingMessage {
    struct __DataChannelPendingMessage* pNext;
    BOOL isBinary;
    UINT32 length;
} DataChannelPendingMessage, *PDataChannelPendingMessage;

typedef struct {
    RtcDataChannel dataChannel;

    PRtcPeerConnection pRtcPeerConnection;
    RtcDataChannelInit rtcDataChannelInit;
    UINT32 channelId;
    UINT64 onMessageCustomData;
    RtcOnMessage onMessage;
    RtcDataChannelStats rtcDataChannelDiagnostics;

    UINT64 onOpenCustomData;
    RtcOnOpen onOpen;
    BOOL openFired; // onOpen delivered (it can be due both from allocateSctp and from dataChannelOnOpen)
    // Messages that arrived before onMessage was set, oldest first; guarded by dataChannelsLock
    PDataChannelPendingMessage pPendingHead;
    PDataChannelPendingMessage pPendingTail;
    UINT32 pendingCount;
    UINT32 pendingBytes;
    BOOL deliveringPending; // dataChannelOnMessage is delivering them; new messages queue behind
} KvsDataChannel, *PKvsDataChannel;

BOOL dataChannelQueueMessageIfPending(PKvsDataChannel, BOOL, PBYTE, UINT32); // caller holds dataChannelsLock
VOID dataChannelFreePendingMessages(PKvsDataChannel);                        // caller holds dataChannelsLock
STATUS freeDataChannelHashEntry(UINT64, PHashEntry);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__ */
