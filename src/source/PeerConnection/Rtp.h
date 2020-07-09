#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

// Default MTU comes from libwebrtc
// https://groups.google.com/forum/#!topic/discuss-webrtc/gH5ysR3SoZI
#define DEFAULT_MTU_SIZE                                        1200
#define DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS              3
#define HIGHEST_EXPECTED_BIT_RATE                               (10 * 1024 * 1024)
#define DEFAULT_SEQ_NUM_BUFFER_SIZE                             1000
#define DEFAULT_VALID_INDEX_BUFFER_SIZE                         1000
#define DEFAULT_PEER_FRAME_BUFFER_SIZE                          (5 * 1024)
#define SRTP_AUTH_TAG_OVERHEAD                                  10

typedef struct {
    UINT8 payloadType;
    UINT8 rtxPayloadType;
    UINT16 sequenceNumber;
    UINT16 rtxSequenceNumber;
    UINT32 ssrc;
    UINT32 rtxSsrc;
    PayloadArray payloadArray;

    RtcMediaStreamTrack track;
    PRtpRollingBuffer packetBuffer;
    PRetransmitter retransmitter;

    RtcOutboundRtpStreamStats outboundRtpStreamStats;
    UINT64 rtpTimeOffset;
    UINT64 firstFrameWallClockTime; //100ns precision

} RtcRtpSender, *PRtcRtpSender;

typedef struct {
    RtcRtpTransceiver transceiver;
    RtcRtpSender sender;

    PKvsPeerConnection pKvsPeerConnection;

    UINT32 jitterBufferSsrc;
    PJitterBuffer pJitterBuffer;

    UINT64 onFrameCustomData;
    RtcOnFrame onFrame;

    UINT64 onBandwidthEstimationCustomData;
    RtcOnBandwidthEstimation onBandwidthEstimation;
    UINT64 onPictureLossCustomData;
    RtcOnPictureLoss onPictureLoss;

    PBYTE peerFrameBuffer;
    UINT32 peerFrameBufferSize;

    UINT32 rtcpReportsTimerId;
} KvsRtpTransceiver, *PKvsRtpTransceiver;

STATUS createKvsRtpTransceiver(RTC_RTP_TRANSCEIVER_DIRECTION, PKvsPeerConnection, UINT32, UINT32,
                               PRtcMediaStreamTrack, PJitterBuffer, RTC_CODEC, PKvsRtpTransceiver*);
STATUS freeKvsRtpTransceiver(PKvsRtpTransceiver*);

STATUS kvsRtpTransceiverSetJitterBuffer(PKvsRtpTransceiver, PJitterBuffer);

#define convertTimestampToRTP(clockRate, pts) (pts * clockRate / HUNDREDS_OF_NANOS_IN_A_SECOND)

STATUS writeRtpPacket(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__ */
