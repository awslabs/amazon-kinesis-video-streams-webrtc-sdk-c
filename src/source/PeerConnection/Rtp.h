#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Default MTU comes from libwebrtc
// https://groups.google.com/forum/#!topic/discuss-webrtc/gH5ysR3SoZI
#define DEFAULT_MTU_SIZE_BYTES                     1200
#define DEFAULT_ROLLING_BUFFER_DURATION_IN_SECONDS (DOUBLE) 3
#define DEFAULT_EXPECTED_VIDEO_BIT_RATE            (DOUBLE)(10 * 1024 * 1024)
#define DEFAULT_EXPECTED_AUDIO_BIT_RATE            (DOUBLE)(10 * 1024 * 1024)
#define DEFAULT_SEQ_NUM_BUFFER_SIZE                1000
#define DEFAULT_VALID_INDEX_BUFFER_SIZE            1000
#define DEFAULT_PEER_FRAME_BUFFER_SIZE             (5 * 1024)
#define SRTP_AUTH_TAG_OVERHEAD                     10
#define MIN_ROLLING_BUFFER_DURATION_IN_SECONDS     (DOUBLE) 0.1
#define MIN_EXPECTED_BIT_RATE                      (DOUBLE)(102.4 * 1024) // Considering 1Kib = 1024 bits
#define MAX_ROLLING_BUFFER_DURATION_IN_SECONDS     (DOUBLE) 10
#define MAX_EXPECTED_BIT_RATE                      (DOUBLE)(240 * 1024 * 1024) // Considering 1Kib = 1024 bits

// https://www.w3.org/TR/webrtc-stats/#dom-rtcoutboundrtpstreamstats-huge
// Huge frames, by definition, are frames that have an encoded size at least 2.5 times the average size of the frames.
#define HUGE_FRAME_MULTIPLIER 2.5

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

    UINT64 rtpTimeOffset;
    UINT64 firstFrameWallClockTime; // 100ns precision

    // used for fps calculation
    UINT64 lastKnownFrameCount;
    UINT64 lastKnownFrameCountTime; // 100ns precision

} RtcRtpSender, *PRtcRtpSender;

typedef struct {
    DOUBLE rollingBufferDurationSec; //!< Maximum duration of media that needs to be buffered (in seconds). The lowest allowed is 0.1 seconds (100ms)
    DOUBLE rollingBufferBitratebps;  //!< Maximum expected bitrate of media (In bits/second). It is used to determine the buffer capacity. The lowest
                                     //!< allowed is 100 Kbps
} RollingBufferConfig, *PRollingBufferConfig;

typedef struct {
    RtcRtpTransceiver transceiver;
    RtcRtpSender sender;

    PKvsPeerConnection pKvsPeerConnection;

    UINT32 jitterBufferSsrc;
    PJitterBuffer pJitterBuffer;

    PRollingBufferConfig pRollingBufferConfig;

    UINT64 onFrameCustomData;
    RtcOnFrame onFrame;

    UINT64 onBandwidthEstimationCustomData;
    RtcOnBandwidthEstimation onBandwidthEstimation;
    UINT64 onPictureLossCustomData;
    RtcOnPictureLoss onPictureLoss;

    PBYTE peerFrameBuffer;
    UINT32 peerFrameBufferSize;

    UINT32 rtcpReportsTimerId;

    MUTEX statsLock;
    RtcOutboundRtpStreamStats outboundStats;
    RtcRemoteInboundRtpStreamStats remoteInboundStats;
    RtcInboundRtpStreamStats inboundStats;

    BOOL twccEnabled; // whether TWCC was negotiated for this transceiver's m-line

    // Caller-supplied fmtp override (H264 only). Empty string means "use the default".
    CHAR fmtpOverride[MAX_SDP_ATTRIBUTE_VALUE_LENGTH + 1];

    // The direction the application configured at addTransceiver() time. Re-negotiation
    // intersects the remote offer against THIS, not against the previously-negotiated
    // transceiver.direction — otherwise a narrowed direction (e.g. the peer paused our
    // track with a sendonly offer) could never widen again when the peer re-enables it.
    RTC_RTP_TRANSCEIVER_DIRECTION configuredDirection;

    // Lock-free snapshot of transceiver.direction for the media path. writeFrame() runs on
    // the application's media thread while re-negotiation (signaling thread) or the
    // application itself rewrites transceiver.direction, so the media path must not read
    // the plain field. Updated wherever the SDK writes transceiver.direction, and re-synced
    // from the field in createOffer/createAnswer to publish direct application writes.
    volatile SIZE_T atomicDirection;

    // Lock-free snapshot of (sender.payloadType << 8) | sender.rtxPayloadType, packed so
    // readers get a consistent pair. Same rationale as atomicDirection: re-negotiation
    // (setTransceiverPayloadTypes, signaling thread) rewrites the payload types while
    // writeFrame (media thread) and the retransmitter (RTCP thread) read them.
    volatile SIZE_T atomicSenderPayloadTypes;

    // Codec the sender's rolling buffer capacity was sized for. Re-negotiation reuses the
    // buffer rather than re-allocating it, so this is how a codec change is detected.
    RTC_CODEC rollingBufferCodec;
} KvsRtpTransceiver, *PKvsRtpTransceiver;

#define KVS_RTP_TRANSCEIVER_PACK_PAYLOAD_TYPES(payloadType, rtxPayloadType) ((SIZE_T) ((((UINT32) (payloadType)) << 8) | (UINT32) (rtxPayloadType)))
#define KVS_RTP_TRANSCEIVER_UNPACK_PAYLOAD_TYPE(packed)                     ((UINT8) ((packed) >> 8))
#define KVS_RTP_TRANSCEIVER_UNPACK_RTX_PAYLOAD_TYPE(packed)                 ((UINT8) ((packed) & 0xFF))

STATUS createKvsRtpTransceiver(RTC_RTP_TRANSCEIVER_DIRECTION, PKvsPeerConnection, UINT32, UINT32, PRtcMediaStreamTrack, PJitterBuffer, RTC_CODEC,
                               PKvsRtpTransceiver*);
STATUS freeKvsRtpTransceiver(PKvsRtpTransceiver*);

STATUS kvsRtpTransceiverSetJitterBuffer(PKvsRtpTransceiver, PJitterBuffer);

#define CONVERT_TIMESTAMP_TO_RTP(clockRate, pts) ((UINT64) ((DOUBLE) (pts) * ((DOUBLE) (clockRate) / HUNDREDS_OF_NANOS_IN_A_SECOND)))

STATUS writeRtpPacket(PKvsPeerConnection pKvsPeerConnection, PRtpPacket pRtpPacket);

STATUS hasTransceiverWithSsrc(PKvsPeerConnection pKvsPeerConnection, UINT32 ssrc);
STATUS findTransceiverBySsrc(PKvsPeerConnection pKvsPeerConnection, PKvsRtpTransceiver* ppTransceiver, UINT32 ssrc);

STATUS setUpRollingBufferConfigInternal(PKvsRtpTransceiver, PRtcMediaStreamTrack, DOUBLE, DOUBLE);
STATUS freeRollingBufferConfig(PRollingBufferConfig);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_PEERCONNECTION_RTP__ */
