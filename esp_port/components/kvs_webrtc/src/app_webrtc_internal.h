/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WEBRTC_INTERNAL_H__
#define __APP_WEBRTC_INTERNAL_H__

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "app_webrtc.h"
#include "WebRtcLogging.h"
#include "webrtc_signaling_if.h"
#include "media_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal Constants */
// Timing constants for internal operations
#define SAMPLE_STATS_DURATION       (60 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SAMPLE_PRE_GENERATE_CERT        TRUE
#define SAMPLE_PRE_GENERATE_CERT_PERIOD (1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
#define SAMPLE_SESSION_CLEANUP_WAIT_PERIOD (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SAMPLE_PENDING_MESSAGE_CLEANUP_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Hash table configuration
#define SAMPLE_HASH_TABLE_BUCKET_COUNT  50
#define SAMPLE_HASH_TABLE_BUCKET_LENGTH 2

// Certificate file extension
#define CA_CERT_PEM_FILE_EXTENSION ".pem"

// Data channel message content
#define MASTER_DATA_CHANNEL_MESSAGE "This message is from the KVS Master"

// JSON message templates for metrics
#define DATA_CHANNEL_MESSAGE_TEMPLATE                                                                                                                \
    "{\"content\":\"%s\",\"firstMessageFromViewerTs\":\"%s\",\"firstMessageFromMasterTs\":\"%s\",\"secondMessageFromViewerTs\":\"%s\","              \
    "\"secondMessageFromMasterTs\":\"%s\",\"lastMessageFromViewerTs\":\"%s\" }"
#define PEER_CONNECTION_METRICS_JSON_TEMPLATE "{\"peerConnectionStartTime\": %llu, \"peerConnectionEndTime\": %llu }"
#define SIGNALING_CLIENT_METRICS_JSON_TEMPLATE                                                                                                       \
    "{\"signalingStartTime\": %llu, \"signalingEndTime\": %llu, \"offerReceiptTime\": %llu, \"sendAnswerTime\": %llu, "                              \
    "\"describeChannelStartTime\": %llu, \"describeChannelEndTime\": %llu, \"getSignalingChannelEndpointStartTime\": %llu, "                         \
    "\"getSignalingChannelEndpointEndTime\": %llu, \"getIceServerConfigStartTime\": %llu, \"getIceServerConfigEndTime\": %llu, "                     \
    "\"getTokenStartTime\": %llu, \"getTokenEndTime\": %llu, \"createChannelStartTime\": %llu, \"createChannelEndTime\": %llu, "                     \
    "\"connectStartTime\": %llu, \"connectEndTime\": %llu }"
#define ICE_AGENT_METRICS_JSON_TEMPLATE "{\"candidateGatheringStartTime\": %llu, \"candidateGatheringEndTime\": %llu }"

// Message size limits
#define MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE     300 // strlen(DATA_CHANNEL_MESSAGE_TEMPLATE) + 20 * 5
#define MAX_PEER_CONNECTION_METRICS_MESSAGE_SIZE  105 // strlen(PEER_CONNECTION_METRICS_JSON_TEMPLATE) + 20 * 2
#define MAX_SIGNALING_CLIENT_METRICS_MESSAGE_SIZE 736 // strlen(SIGNALING_CLIENT_METRICS_JSON_TEMPLATE) + 20 * 10
#define MAX_ICE_AGENT_METRICS_MESSAGE_SIZE        113 // strlen(ICE_AGENT_METRICS_JSON_TEMPLATE) + 20 * 2

// Bitrate control constants
#define TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS 1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define MIN_VIDEO_BITRATE_KBPS              512     // Unit kilobits/sec. Value could change based on codec.
#define MAX_VIDEO_BITRATE_KBPS              2048000 // Unit kilobits/sec. Value could change based on codec.
#define MIN_AUDIO_BITRATE_BPS               4000    // Unit bits/sec. Value could change based on codec.
#define MAX_AUDIO_BITRATE_BPS               650000  // Unit bits/sec. Value could change based on codec.

// Frame timing for sample-based streaming
#define SAMPLE_VIDEO_FRAME_DURATION (33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND) // 33ms for ~30 FPS
#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND) // 20ms for OPUS
#define NUMBER_OF_OPUS_FRAME_FILES 618

/* Internal Data Structures */
typedef struct {
    CHAR content[100];
    CHAR firstMessageFromViewerTs[20];
    CHAR firstMessageFromMasterTs[20];
    CHAR secondMessageFromViewerTs[20];
    CHAR secondMessageFromMasterTs[20];
    CHAR lastMessageFromViewerTs[20];
} DataChannelMessage;

typedef struct {
    MUTEX updateLock;
    UINT64 lastAdjustmentTimeMs;
    UINT64 currentVideoBitrate;
    UINT64 currentAudioBitrate;
    UINT64 newVideoBitrate;
    UINT64 newAudioBitrate;
    DOUBLE averagePacketLoss;
} TwccMetadata, *PTwccMetadata;

typedef struct __SampleStreamingSession SampleStreamingSession;
typedef struct __SampleStreamingSession* PSampleStreamingSession;

typedef struct {
    UINT64 hashValue;
    UINT64 createTime;
    PStackQueue messageQueue;
} PendingMessageQueue, *PPendingMessageQueue;

typedef enum {
    TEST_SOURCE,
    DEVICE_SOURCE,
    RTSP_SOURCE,
} SampleSourceType;

typedef VOID (*StreamSessionShutdownCallback)(UINT64, PSampleStreamingSession);

typedef struct {
    volatile ATOMIC_BOOL appTerminateFlag;
    volatile ATOMIC_BOOL interrupted;
    volatile ATOMIC_BOOL mediaThreadStarted;
    volatile ATOMIC_BOOL recreateSignalingClient;
    volatile ATOMIC_BOOL connected;
    SampleSourceType srcType;
    ChannelInfo channelInfo;
    PCHAR pCaCertPath;
    PAwsCredentialProvider pCredentialProvider;
    SIGNALING_CLIENT_HANDLE signalingClientHandle;
    RTC_CODEC audioCodec;
    RTC_CODEC videoCodec;
    PBYTE pAudioFrameBuffer;
    UINT32 audioBufferSize;
    PBYTE pVideoFrameBuffer;
    UINT32 videoBufferSize;
    TID mediaSenderTid;
    TID audioSenderTid;
    TID videoSenderTid;
    TIMER_QUEUE_HANDLE timerQueueHandle;
    UINT32 iceCandidatePairStatsTimerId;
    app_webrtc_streaming_media_t mediaType;
    startRoutine audioSource;
    startRoutine videoSource;
    startRoutine receiveAudioVideoSource;
    RtcOnDataChannel onDataChannel;
    SignalingClientMetrics signalingClientMetrics;

    // Media capture interfaces
    void* video_capture;
    void* audio_capture;

    // Media player interfaces
    void* video_player;
    void* audio_player;

    // Media player handles
    video_player_handle_t video_player_handle;
    audio_player_handle_t audio_player_handle;

    // Count of active sessions using media players
    UINT32 activePlayerSessionCount;
    MUTEX playerLock;

    // Media reception
    BOOL receive_media;

    // Signaling-only mode (disables media components to save memory)
    BOOL signaling_only;

    // Callbacks for signaling messages
    VOID (*onAnswer)(UINT64, PSignalingMessage);
    VOID (*onIceCandidate)(UINT64, PSignalingMessage);

    PStackQueue pPendingSignalingMessageForRemoteClient;
    PHashTable pRtcPeerConnectionForRemoteClient;

    MUTEX sampleConfigurationObjLock;
    CVAR cvar;
    BOOL trickleIce;
    BOOL useTurn;
    BOOL enableSendingMetricsToViewerViaDc;
    BOOL enableFileLogging;
    UINT64 customData;
    PSampleStreamingSession sampleStreamingSessionList[CONFIG_KVS_MAX_CONCURRENT_STREAMS];
    UINT32 streamingSessionCount;
    MUTEX streamingSessionListReadLock;
    UINT32 iceUriCount;
    SignalingClientCallbacks signalingClientCallbacks;
    SignalingClientInfo clientInfo;

    RtcStats rtcIceCandidatePairMetrics;

    MUTEX signalingSendMessageLock;

    UINT32 pregenerateCertTimerId;
    PStackQueue pregeneratedCertificates; // Max MAX_RTCCONFIGURATION_CERTIFICATES certificates

    PCHAR rtspUri;
    UINT32 logLevel;
    BOOL enableTwcc;
    volatile SIZE_T frameIndex;  // Frame index for sample-based streaming
} SampleConfiguration, *PSampleConfiguration;

struct __SampleStreamingSession {
    volatile ATOMIC_BOOL terminateFlag;
    volatile ATOMIC_BOOL candidateGatheringDone;
    volatile ATOMIC_BOOL peerIdReceived;
    volatile ATOMIC_BOOL firstFrame;
    volatile SIZE_T frameIndex;
    volatile SIZE_T correlationIdPostFix;
    PRtcPeerConnection pPeerConnection;
    PRtcRtpTransceiver pVideoRtcRtpTransceiver;
    PRtcRtpTransceiver pAudioRtcRtpTransceiver;
    RtcSessionDescriptionInit answerSessionDescriptionInit;
    PSampleConfiguration pSampleConfiguration;
    UINT64 audioTimestamp;
    UINT64 videoTimestamp;
    CHAR peerId[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    TID receiveAudioVideoSenderTid;
    UINT64 startUpLatency;
    RtcMetricsHistory rtcMetricsHistory;
    BOOL remoteCanTrickleIce;
    TwccMetadata twccMetadata;

    // this is called when the SampleStreamingSession is being freed
    StreamSessionShutdownCallback shutdownCallback;
    UINT64 shutdownCallbackCustomData;
    UINT64 offerReceiveTime;
    PeerConnectionMetrics peerConnectionMetrics;
    KvsIceAgentMetrics iceMetrics;
    CHAR pPeerConnectionMetricsMessage[MAX_PEER_CONNECTION_METRICS_MESSAGE_SIZE];
    CHAR pSignalingClientMetricsMessage[MAX_SIGNALING_CLIENT_METRICS_MESSAGE_SIZE];
    CHAR pIceAgentMetricsMessage[MAX_ICE_AGENT_METRICS_MESSAGE_SIZE];
};

/* External global variables - defined in app_webrtc.c */
extern PSampleConfiguration gSampleConfiguration;
extern app_webrtc_config_t gWebRtcAppConfig;

/* Forward declarations for functions that will remain in app_webrtc.c */
STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE role_type, BOOL trickleIce, BOOL useTurn, UINT32 logLevel,
                                 BOOL signaling_only, PSampleConfiguration* ppSampleConfiguration);
STATUS freeSampleConfiguration(PSampleConfiguration*);
STATUS createSampleStreamingSession(PSampleConfiguration, PCHAR, BOOL, PSampleStreamingSession*);
STATUS freeSampleStreamingSession(PSampleStreamingSession*);
STATUS streamingSessionOnShutdown(PSampleStreamingSession, UINT64, StreamSessionShutdownCallback);
STATUS sessionCleanupWait(PSampleConfiguration, BOOL);

#ifdef DYNAMIC_SIGNALING_PAYLOAD
/**
 * @brief Allocate memory for the payload of a SignalingMessage
 *
 * @param[in,out] pSignalingMessage The signaling message for which to allocate payload
 * @param[in] size Size in bytes to allocate
 *
 * @return STATUS code of the execution
 */
STATUS allocateSignalingMessagePayload(PSignalingMessage pSignalingMessage, UINT32 size);

/**
 * @brief Free the dynamically allocated payload of a SignalingMessage
 *
 * @param[in,out] pSignalingMessage The signaling message whose payload should be freed
 *
 * @return STATUS code of the execution
 */
STATUS freeSignalingMessagePayload(PSignalingMessage pSignalingMessage);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEBRTC_INTERNAL_H__ */
