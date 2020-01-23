/*******************************************
DataChannel internal include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct {
    RtcDataChannel dataChannel;

    PRtcPeerConnection pRtcPeerConnection;
    UINT32 channelId;

    UINT64 onMessageCustomData;
    RtcOnMessage onMessage;

    UINT64 onOpenCustomData;
    RtcOnOpen onOpen;
} KvsDataChannel, *PKvsDataChannel;

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_DATACHANNEL__ */
