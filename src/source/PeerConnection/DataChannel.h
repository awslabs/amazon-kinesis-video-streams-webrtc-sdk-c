/*******************************************
DataChannel internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__

#ifdef ENABLE_DATA_CHANNEL
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_CHANNEL_PROTOCOL_STR (PCHAR) "SCTP"

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
} KvsDataChannel, *PKvsDataChannel;

#ifdef __cplusplus
}
#endif
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__ */
