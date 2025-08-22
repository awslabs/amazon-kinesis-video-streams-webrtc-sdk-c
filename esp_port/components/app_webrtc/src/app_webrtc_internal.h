/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WEBRTC_INTERNAL_H__
#define __APP_WEBRTC_INTERNAL_H__

/* Essential utility headers only - replacing monolithic KVS SDK include */
#include "common_defs.h"           /* Basic types: BOOL, UINT32, STATUS, etc. */
#include "platform_utils.h"        /* Platform utilities: MUTEX, CVAR, TID, etc. */
#include "stack_queue.h"           /* Stack queue data structure from kvs_utils/src */
#include "hash_table.h"            /* Hash table data structure from kvs_utils/src */
#include "crc32.h"                 /* CRC32 utilities for hash table keys */

/* Application includes */
#include "app_webrtc.h"
#include "app_webrtc_if.h"
#include "media_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Internal Constants */
// Essential timing constants for app_webrtc operations
#define SAMPLE_SESSION_CLEANUP_WAIT_PERIOD (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)
#define SAMPLE_PENDING_MESSAGE_CLEANUP_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Hash table configuration
#define SAMPLE_HASH_TABLE_BUCKET_COUNT  50
#define SAMPLE_HASH_TABLE_BUCKET_LENGTH 2

// Data channel constants
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

// Media frame timing and constants
#define SAMPLE_VIDEO_FRAME_DURATION (33 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND) // 33ms for ~30 FPS
#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND) // 20ms for OPUS
#define NUMBER_OF_OPUS_FRAME_FILES 618

/* Internal Data Structures */
typedef struct {
    char content[100];
    char firstMessageFromViewerTs[20];
    char firstMessageFromMasterTs[20];
    char secondMessageFromViewerTs[20];
    char secondMessageFromMasterTs[20];
    char lastMessageFromViewerTs[20];
} DataChannelMessage;

typedef struct {
    MUTEX updateLock;
    uint64_t lastAdjustmentTimeMs;
    uint64_t currentVideoBitrate;
    uint64_t currentAudioBitrate;
    uint64_t newVideoBitrate;
    uint64_t newAudioBitrate;
    double averagePacketLoss;
} TwccMetadata, *PTwccMetadata;

// Forward declarations for the new streamlined session structure
typedef struct __AppWebRTCSession AppWebRTCSession;
typedef struct __AppWebRTCSession* PAppWebRTCSession;

// Legacy typedefs for compatibility during transition
typedef AppWebRTCSession SampleStreamingSession;
typedef PAppWebRTCSession PSampleStreamingSession;

typedef struct {
    uint64_t hashValue;
    uint64_t createTime;
    PStackQueue messageQueue;
} PendingMessageQueue, *PPendingMessageQueue;

typedef void (*StreamSessionShutdownCallback)(uint64_t, PAppWebRTCSession);

/**
 * @brief Clean, minimal WebRTC application context
 *
 * This structure replaces the bloated PSampleConfiguration with only essential fields.
 * All signaling and peer connection specifics are handled by pluggable interfaces.
 */
typedef struct {
    // === Control Flags ===
    volatile ATOMIC_BOOL appTerminateFlag;      /* Main termination flag */
    volatile ATOMIC_BOOL interrupted;           /* Interrupt handling */
    volatile ATOMIC_BOOL mediaThreadStarted;    /* Media thread state */
    volatile ATOMIC_BOOL connected;             /* Connection state */

    // === Media Configuration ===
    app_webrtc_rtc_codec_t audioCodec;          /* Audio codec selection */
    app_webrtc_rtc_codec_t videoCodec;          /* Video codec selection */
    app_webrtc_streaming_media_t mediaType;     /* Media type (audio/video/both) */
    BOOL signaling_only;                        /* Signaling-only mode flag */

    // === Media Buffers ===
    uint8_t* pAudioFrameBuffer;                 /* Audio frame buffer */
    uint32_t audioBufferSize;                   /* Audio buffer size */
    uint8_t* pVideoFrameBuffer;                 /* Video frame buffer */
    uint32_t videoBufferSize;                   /* Video buffer size */

    // === Threading ===
    TID audioSenderTid;                         /* Audio sender thread ID */
    TID videoSenderTid;                         /* Video sender thread ID */
    TID mediaSenderTid;                         /* Media sender thread ID */

    // === Media Interfaces ===
    void* video_capture;                        /* Video capture interface */
    void* audio_capture;                        /* Audio capture interface */
    void* video_player;                         /* Video player interface */
    void* audio_player;                         /* Audio player interface */
    video_player_handle_t video_player_handle;  /* Video player handle */
    audio_player_handle_t audio_player_handle;  /* Audio player handle */
    uint32_t activePlayerSessionCount;          /* Active player session count */
    MUTEX playerLock;                           /* Player synchronization */
    bool receive_media;                         /* Media reception flag */

    // === WebRTC Settings ===
    bool trickleIce;                            /* Trickle ICE enabled */
    bool useTurn;                               /* TURN usage flag */
    bool bridge_mode;                           /* Bridge mode flag */

    // === Signaling Management ===
    volatile ATOMIC_BOOL recreate_signaling_client; /* Recreate signaling client flag */
    webrtc_channel_role_type_t channel_role_type;  /* Channel role (master/viewer) */

    // === Session Management ===
    PAppWebRTCSession webrtcSessionList[CONFIG_KVS_MAX_CONCURRENT_STREAMS]; /* Active sessions */
    uint32_t streamingSessionCount;             /* Number of active sessions */
    PStackQueue pPendingSignalingMessageForRemoteClient; /* Pending message queue */
    PHashTable pRtcPeerConnectionForRemoteClient;       /* Peer connection hash table */

    // === Synchronization ===
    MUTEX sampleConfigurationObjLock;           /* Main configuration lock */
    CVAR cvar;                                  /* Condition variable */
    MUTEX streamingSessionListReadLock;         /* Session list read lock */
    MUTEX signalingSendMessageLock;             /* Signaling send lock */

    // === Logging & Debugging ===
    bool enableFileLogging;                     /* File logging enabled */
    uint32_t logLevel;                          /* Log level setting */
    volatile size_t frameIndex;                 /* Frame index for streaming */

    // === Media Sources ===
    startRoutine audioSource;                   /* Audio source function */
    startRoutine videoSource;                   /* Video source function */
    startRoutine receiveAudioVideoSource;       /* Receive function */
    void* onDataChannel;                        /* Generic data channel callback */

    // === Callbacks ===
    void (*onAnswer)(uint64_t, webrtc_message_t*);     /* Answer callback */
    void (*onIceCandidate)(uint64_t, webrtc_message_t*); /* ICE candidate callback */

    // === Miscellaneous ===
    uint64_t customData;                        /* Custom application data */
    char* rtspUri;                              /* RTSP URI for media source */
    bool enableTwcc;                            /* TWCC enabled flag */
} app_webrtc_context_t, *papp_webrtc_context_t;

/* Legacy typedef for backward compatibility during transition */
typedef app_webrtc_context_t SampleConfiguration;
typedef papp_webrtc_context_t PSampleConfiguration;

/**
 * @brief Streamlined WebRTC session structure for interface-based architecture
 *
 * This structure has been cleaned up from the legacy SampleStreamingSession to only
 * include fields that are actually used in the current pluggable interface design.
 * Legacy KVS SDK-specific fields have been removed in favor of the interface abstraction.
 */
struct __AppWebRTCSession {
    // === Core Session Management ===
    volatile ATOMIC_BOOL terminateFlag;        //!< Flag to indicate session should be terminated
    volatile ATOMIC_BOOL peerIdReceived;       //!< Flag indicating peer ID has been received
    volatile ATOMIC_BOOL firstFrame;           //!< Flag for first frame handling
    char peerId[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN + 1]; //!< Peer identifier string

    // === Interface Integration ===
    void* interface_session_handle;            //!< Handle for pluggable peer connection interface
    void* pPeerConnection;                     //!< Legacy peer connection pointer (NULL for interface mode)
    PSampleConfiguration pSampleConfiguration; //!< Reference to main WebRTC configuration

    // === Lifecycle Management ===
    StreamSessionShutdownCallback shutdownCallback;     //!< Called when session is being freed
    uint64_t shutdownCallbackCustomData;                //!< Custom data for shutdown callback
    uint64_t offerReceiveTime;                         //!< Timestamp when offer was received

    // === Metrics and Monitoring ===
    // Note: KVS-specific metrics removed for independence - handled by interface implementation
    // - RtcMetricsHistory rtcMetricsHistory (KVS metrics tracking)
    // - PeerConnectionMetrics peerConnectionMetrics (KVS performance metrics)
    // - KvsIceAgentMetrics iceMetrics (KVS ICE agent metrics)

    // Note: Removed legacy fields that are no longer used in interface-based architecture:
    // - candidateGatheringDone, frameIndex, correlationIdPostFix (legacy state tracking)
    // - pVideoRtcRtpTransceiver, pAudioRtcRtpTransceiver (direct KVS SDK objects)
    // - answerSessionDescriptionInit (legacy SDP handling)
    // - audioTimestamp, videoTimestamp (legacy media timing)
    // - receiveAudioVideoSenderTid (legacy threading)
    // - startUpLatency, remoteCanTrickleIce (unused)
    // - twccMetadata (TWCC not used in current architecture)
    // - *MetricsMessage buffers (replaced by interface-based metrics)
};

/* External global variables - defined in app_webrtc.c */
extern PSampleConfiguration gSampleConfiguration;
extern app_webrtc_config_t gWebRtcAppConfig;

/* Forward declarations for functions that will remain in app_webrtc.c */
STATUS createAppWebRTCContext(bool trickleIce, bool useTurn, uint32_t logLevel, bool signaling_only,
                              papp_webrtc_context_t* ppContext);
STATUS freeAppWebRTCContext(papp_webrtc_context_t*);
STATUS createAppWebRTCSession(PSampleConfiguration, char*, bool, PAppWebRTCSession*);
STATUS freeAppWebRTCSession(PAppWebRTCSession*);
STATUS appWebRTCSessionOnShutdown(PAppWebRTCSession, uint64_t, StreamSessionShutdownCallback);
STATUS sessionCleanupWait(PSampleConfiguration, bool);

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEBRTC_INTERNAL_H__ */
