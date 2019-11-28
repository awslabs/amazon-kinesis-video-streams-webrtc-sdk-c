#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// For tight packing
#pragma pack(push, include_i, 1) // for byte alignment

#define DEFAULT_ROLLING_BUFFER_SIZE 5000
#define DEFAULT_SEQ_NUM_BUFFER_SIZE 1000
#define DEFAULT_VALID_INDEX_BUFFER_SIZE 1000
#define DEFAULT_PEER_FRAME_BUFFER_SIZE  5 * 1024

typedef struct {
    UINT8 payloadType;
    UINT16 sequenceNumber;
    UINT32 ssrc;
    PayloadArray payloadArray;

    RtcMediaStreamTrack track;
    PRollingBuffer packetBuffer;
    PRetransmitter retransmitter;
} RtcRtpSender, *PRtcRtpSender;

typedef struct {
    RtcRtpTransceiver transceiver;
    RtcRtpSender sender;

    PKvsPeerConnection pKvsPeerConnection;

    UINT32 jitterBufferSsrc;
    PJitterBuffer pJitterBuffer;

    UINT64 onFrameCustomData;
    RtcOnFrame onFrame;

    PBYTE peerFrameBuffer;
    UINT32 peerFrameBufferSize;
} KvsRtpTransceiver, *PKvsRtpTransceiver;

STATUS createKvsRtpTransceiver(RTC_RTP_TRANSCEIVER_DIRECTION, PKvsPeerConnection, UINT32,
                               PRtcMediaStreamTrack, PJitterBuffer, RTC_CODEC, PKvsRtpTransceiver*);
STATUS freeKvsRtpTransceiver(PKvsRtpTransceiver*);

STATUS kvsRtpTransceiverSetJitterBuffer(PKvsRtpTransceiver, PJitterBuffer);

UINT64 convertTimestampToRTP(UINT64, UINT64);

STATUS writeEncryptedRtpPacketNoCopy(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket);

#pragma pack(pop, include_i)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__ */
