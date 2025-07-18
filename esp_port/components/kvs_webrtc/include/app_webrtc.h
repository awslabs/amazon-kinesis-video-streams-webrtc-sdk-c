/*******************************************
Shared include file for the samples
*******************************************/
#ifndef __KINESIS_VIDEO_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "WebRtcLogging.h"
#include "media_stream.h"
#include "signaling_serializer.h"
#include "webrtc_signaling_if.h"

#define AUDIO_CODEC_NAME_ALAW  "alaw"
#define AUDIO_CODEC_NAME_MULAW "mulaw"
#define AUDIO_CODEC_NAME_OPUS  "opus"
#define VIDEO_CODEC_NAME_H264  "h264"
#define VIDEO_CODEC_NAME_H265  "h265"
#define VIDEO_CODEC_NAME_VP8   "vp8"

#define SAMPLE_MASTER_CLIENT_ID "ProducerMaster"
#define SAMPLE_VIEWER_CLIENT_ID "ConsumerViewer"
#define SAMPLE_CHANNEL_NAME     (PCHAR) "ScaryTestChannel"

#define SAMPLE_STATS_DURATION       (60 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define SAMPLE_PRE_GENERATE_CERT        TRUE
#define SAMPLE_PRE_GENERATE_CERT_PERIOD (1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

#define SAMPLE_SESSION_CLEANUP_WAIT_PERIOD (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define SAMPLE_PENDING_MESSAGE_CLEANUP_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define CA_CERT_PEM_FILE_EXTENSION ".pem"

#define FILE_LOGGING_BUFFER_SIZE (10 * 1024)
#define MAX_NUMBER_OF_LOG_FILES  5

#define SAMPLE_HASH_TABLE_BUCKET_COUNT  50
#define SAMPLE_HASH_TABLE_BUCKET_LENGTH 2

#define RTSP_PIPELINE_MAX_CHAR_COUNT 1000

/* To enable IoT credentials checks in the provided samples, specify
   this through the CMake flag: cmake .. -DIOT_CORE_ENABLE_CREDENTIALS=ON */
#define IOT_CORE_CREDENTIAL_ENDPOINT ((PCHAR) "AWS_IOT_CORE_CREDENTIAL_ENDPOINT")
#define IOT_CORE_CERT                ((PCHAR) "AWS_IOT_CORE_CERT")
#define IOT_CORE_PRIVATE_KEY         ((PCHAR) "AWS_IOT_CORE_PRIVATE_KEY")
#define IOT_CORE_ROLE_ALIAS          ((PCHAR) "AWS_IOT_CORE_ROLE_ALIAS")
#define IOT_CORE_THING_NAME          ((PCHAR) "AWS_IOT_CORE_THING_NAME")
#define IOT_CORE_CERTIFICATE_ID      ((PCHAR) "AWS_IOT_CORE_CERTIFICATE_ID")

#define MASTER_DATA_CHANNEL_MESSAGE "This message is from the KVS Master"
#define VIEWER_DATA_CHANNEL_MESSAGE "This message is from the KVS Viewer"

typedef enum {
    // Initialization / Deinitialization
    APP_WEBRTC_EVENT_INITIALIZED,
    APP_WEBRTC_EVENT_DEINITIALIZING,

    // Signaling Client States
    APP_WEBRTC_EVENT_SIGNALING_CONNECTING,
    APP_WEBRTC_EVENT_SIGNALING_CONNECTED,
    APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED,
    APP_WEBRTC_EVENT_SIGNALING_DESCRIBE,
    APP_WEBRTC_EVENT_SIGNALING_GET_ENDPOINT,
    APP_WEBRTC_EVENT_SIGNALING_GET_ICE,
    APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED,
    APP_WEBRTC_EVENT_PEER_CONNECTED,
    APP_WEBRTC_EVENT_PEER_DISCONNECTED,
    APP_WEBRTC_EVENT_STREAMING_STARTED,        // When media threads actually start for a peer
    APP_WEBRTC_EVENT_STREAMING_STOPPED,        // When media threads stop for a peer
    APP_WEBRTC_EVENT_RECEIVED_OFFER,
    APP_WEBRTC_EVENT_SENT_ANSWER,
    APP_WEBRTC_EVENT_SENT_OFFER,
    APP_WEBRTC_EVENT_RECEIVED_ICE_CANDIDATE,
    APP_WEBRTC_EVENT_SENT_ICE_CANDIDATE,
    APP_WEBRTC_EVENT_ICE_GATHERING_COMPLETE,

    // Peer Connection States
    APP_WEBRTC_EVENT_PEER_CONNECTING,          // Peer connection attempt started
    APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED,   // Specific connection failure

    // Error Events
    APP_WEBRTC_EVENT_ERROR,
    APP_WEBRTC_EVENT_SIGNALING_ERROR,
} app_webrtc_event_t;

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

#define MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE     300 // strlen(DATA_CHANNEL_MESSAGE_TEMPLATE) + 20 * 5
#define MAX_PEER_CONNECTION_METRICS_MESSAGE_SIZE  105 // strlen(PEER_CONNECTION_METRICS_JSON_TEMPLATE) + 20 * 2
#define MAX_SIGNALING_CLIENT_METRICS_MESSAGE_SIZE 736 // strlen(SIGNALING_CLIENT_METRICS_JSON_TEMPLATE) + 20 * 10
#define MAX_ICE_AGENT_METRICS_MESSAGE_SIZE        113 // strlen(ICE_AGENT_METRICS_JSON_TEMPLATE) + 20 * 2

#define TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS 1000 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND
#define MIN_VIDEO_BITRATE_KBPS              512     // Unit kilobits/sec. Value could change based on codec.
#define MAX_VIDEO_BITRATE_KBPS              2048000 // Unit kilobits/sec. Value could change based on codec.
#define MIN_AUDIO_BITRATE_BPS               4000    // Unit bits/sec. Value could change based on codec.
#define MAX_AUDIO_BITRATE_BPS               650000  // Unit bits/sec. Value could change based on codec.

typedef enum {
    APP_WEBRTC_MEDIA_VIDEO,
    APP_WEBRTC_MEDIA_AUDIO_VIDEO,
} AppWebrtcStreamingMediaType;

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

// Event data structure to pass to callbacks
typedef struct {
    app_webrtc_event_t event_id;
    UINT32 status_code;
    CHAR peer_id[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    CHAR message[256];
} app_webrtc_event_data_t;

// Event callback type
typedef void (*app_webrtc_event_callback_t) (app_webrtc_event_data_t *event_data, void *user_ctx);

// Define message sending callback type
typedef int (*app_webrtc_send_msg_cb_t) (signaling_msg_t *signalingMessage);

/**
 * @brief Register a callback for WebRTC events
 *
 * @param callback Function to call when events occur
 * @param user_ctx User context pointer passed to the callback
 *
 * @return 0 on success, non-zero on failure
 */
INT32 app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx);

/**
 * @brief WebRTC application configuration structure
 */
typedef struct {
    // Signaling configuration
    WebRtcSignalingClientInterface *pSignalingClientInterface;  // Signaling client interface
    PVOID pSignalingConfig;                  // Signaling-specific configuration (opaque pointer)

    // WebRTC configuration
    SIGNALING_CHANNEL_ROLE_TYPE roleType;    // Role type (master initiates, viewer receives)
    BOOL trickleIce;                         // Whether to use trickle ICE
    BOOL useTurn;                            // Whether to use TURN servers
    UINT32 logLevel;                         // Log level
    BOOL signalingOnly;                      // If TRUE, disable media streaming components to save memory

    // Media configuration
    RTC_CODEC audioCodec;                    // Audio codec to use
    RTC_CODEC videoCodec;                    // Video codec to use
    AppWebrtcStreamingMediaType mediaType;   // Media type (audio-only, video-only, or both)

    // Media capture interfaces
    void* videoCapture;                      // Video capture interface
    void* audioCapture;                      // Audio capture interface

    // Media player interfaces
    void* videoPlayer;                       // Video player interface
    void* audioPlayer;                       // Audio player interface

    // Media reception
    BOOL receiveMedia;                       // Whether to receive media
} WebRtcAppConfig, *PWebRtcAppConfig;

#define WEBRTC_APP_CONFIG_DEFAULT() \
{ \
    .pSignalingClientInterface = NULL, \
    .pSignalingConfig = NULL, \
    .roleType = WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER, \
    .trickleIce = TRUE, \
    .useTurn = TRUE, \
    .logLevel = 3, \
    .signalingOnly = FALSE, \
    .audioCodec = RTC_CODEC_OPUS, \
    .videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, \
    .mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO, \
    .receiveMedia = FALSE, \
}

/**
 * @brief Opaque handle to the WebRTC sample configuration
 */
typedef struct __SampleConfiguration* WebRtcConfigHandle;

/**
 * @brief Initialize WebRTC application with the given configuration
 *
 * This function creates and initializes the WebRTC configuration, sets up
 * the signaling client, and prepares the WebRTC stack for streaming.
 *
 * @param[in] pConfig Configuration for the WebRTC application
 *
 * @return STATUS code of the execution
 */
WEBRTC_STATUS webrtcAppInit(PWebRtcAppConfig pConfig);

/**
 * @brief Run the WebRTC application and wait for termination
 *
 * This function starts the WebRTC streaming session and waits until it's terminated.
 *
 * @return STATUS code of the execution
 */
WEBRTC_STATUS webrtcAppRun(VOID);

/**
 * @brief Terminate the WebRTC application
 *
 * This function cleans up resources and terminates the WebRTC application.
 *
 * @return STATUS code of the execution
 */
WEBRTC_STATUS webrtcAppTerminate(VOID);

/**
 * @brief Register a callback for sending signaling messages to bridge (used in split mode)
 *
 * @param callback Function to call when messages need to be sent to bridge
 * @return 0 on success, non-zero on failure
 */
int webrtcAppRegisterSendToBridgeCallback(app_webrtc_send_msg_cb_t callback);

/**
 * @brief Send a message from bridge to signaling server (used in split mode)
 * This function sends signaling messages received from the streaming device
 * to the signaling server.
 * @param signalingMessage The signaling message to send to signaling server
 * @return 0 on success, non-zero on failure
 */
int webrtcAppSendMessageToSignalingServer(signaling_msg_t *signalingMessage);

int webrtcAppCreateAndSendOffer(char *pPeerId);

#ifdef __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_SAMPLE_INCLUDE__ */
