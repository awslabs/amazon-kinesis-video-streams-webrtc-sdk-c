/*******************************************
Shared include file for the samples
*******************************************/
#ifndef __KINESIS_VIDEO_SAMPLE_INCLUDE__
#define __KINESIS_VIDEO_SAMPLE_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

#define DEFAULT_MAX_CONCURRENT_STREAMING_SESSION 2

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

/**
 * @brief Structure to hold AWS credentials and related options
 */
typedef struct {
    // IoT Core credentials
    BOOL enableIotCredentials;
    PCHAR iotCoreCredentialEndpoint;
    PCHAR iotCoreCert;
    PCHAR iotCorePrivateKey;
    PCHAR iotCoreRoleAlias;
    PCHAR iotCoreThingName;

    // Direct AWS credentials
    PCHAR accessKey;
    PCHAR secretKey;
    PCHAR sessionToken;

    // Common AWS options
    PCHAR region;
    PCHAR caCertPath;
    UINT32 logLevel;
} AwsCredentialOptions, *PAwsCredentialOptions;

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
    SAMPLE_STREAMING_VIDEO_ONLY,
    SAMPLE_STREAMING_AUDIO_VIDEO,
} SampleStreamingMediaType;

typedef enum {
    TEST_SOURCE,
    DEVICE_SOURCE,
    RTSP_SOURCE,
} SampleSourceType;

typedef struct __SampleStreamingSession SampleStreamingSession;
typedef struct __SampleStreamingSession* PSampleStreamingSession;

typedef struct {
    UINT64 prevNumberOfPacketsSent;
    UINT64 prevNumberOfPacketsReceived;
    UINT64 prevNumberOfBytesSent;
    UINT64 prevNumberOfBytesReceived;
    UINT64 prevPacketsDiscardedOnSend;
    UINT64 prevTs;
} RtcMetricsHistory, *PRtcMetricsHistory;

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
    SampleStreamingMediaType mediaType;
    startRoutine audioSource;
    startRoutine videoSource;
    startRoutine receiveAudioVideoSource;
    RtcOnDataChannel onDataChannel;
    SignalingClientMetrics signalingClientMetrics;

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

    // AWS credential options
    PAwsCredentialOptions pAwsCredentialOptions;
} SampleConfiguration, *PSampleConfiguration;

typedef struct {
    CHAR content[100];
    CHAR firstMessageFromViewerTs[20];
    CHAR firstMessageFromMasterTs[20];
    CHAR secondMessageFromViewerTs[20];
    CHAR secondMessageFromMasterTs[20];
    CHAR lastMessageFromViewerTs[20];
} DataChannelMessage;

typedef struct {
    UINT64 hashValue;
    UINT64 createTime;
    PStackQueue messageQueue;
} PendingMessageQueue, *PPendingMessageQueue;

typedef VOID (*StreamSessionShutdownCallback)(UINT64, PSampleStreamingSession);

typedef struct {
    MUTEX updateLock;
    UINT64 lastAdjustmentTimeMs;
    UINT64 currentVideoBitrate;
    UINT64 currentAudioBitrate;
    UINT64 newVideoBitrate;
    UINT64 newAudioBitrate;
    DOUBLE averagePacketLoss;
} TwccMetadata, *PTwccMetadata;

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

VOID sigintHandler(INT32);
STATUS readFrameFromDisk(PBYTE, PUINT32, PCHAR);
PVOID receiveGstreamerAudioVideo(PVOID);
PVOID sendVideoPackets(PVOID);
PVOID sendAudioPackets(PVOID);
PVOID sendGstreamerAudioVideo(PVOID);
PVOID sampleReceiveAudioVideoFrame(PVOID);
PVOID getPeriodicIceCandidatePairStats(PVOID);
STATUS getIceCandidatePairStatsCallback(UINT32, UINT64, UINT64);
STATUS pregenerateCertTimerCallback(UINT32, UINT64, UINT64);
STATUS createSampleConfiguration(PCHAR, SIGNALING_CHANNEL_ROLE_TYPE, BOOL, BOOL, UINT32, PAwsCredentialOptions, PSampleConfiguration*);
STATUS freeSampleConfiguration(PSampleConfiguration*);
STATUS signalingClientStateChanged(UINT64, SIGNALING_CLIENT_STATE);
STATUS signalingMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS handleOffer(PSampleConfiguration, PSampleStreamingSession, PSignalingMessage);
STATUS handleRemoteCandidate(PSampleStreamingSession, PSignalingMessage);
STATUS initializePeerConnection(PSampleConfiguration, PRtcPeerConnection*);
STATUS lookForSslCert(PSampleConfiguration*);
STATUS createSampleStreamingSession(PSampleConfiguration, PCHAR, BOOL, PSampleStreamingSession*);
STATUS freeSampleStreamingSession(PSampleStreamingSession*);
STATUS streamingSessionOnShutdown(PSampleStreamingSession, UINT64, StreamSessionShutdownCallback);
STATUS sendSignalingMessage(PSampleStreamingSession, PSignalingMessage);
STATUS respondWithAnswer(PSampleStreamingSession);
STATUS resetSampleConfigurationState(PSampleConfiguration);

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

VOID sampleVideoFrameHandler(UINT64, PFrame);
VOID sampleAudioFrameHandler(UINT64, PFrame);
VOID sampleFrameHandler(UINT64, PFrame);
VOID sampleBandwidthEstimationHandler(UINT64, DOUBLE);
VOID sampleSenderBandwidthEstimationHandler(UINT64, UINT32, UINT32, UINT32, UINT32, UINT64);
VOID onDataChannel(UINT64, PRtcDataChannel);
VOID onConnectionStateChange(UINT64, RTC_PEER_CONNECTION_STATE);
STATUS sessionCleanupWait(PSampleConfiguration);
STATUS logSignalingClientStats(PSignalingClientMetrics);
STATUS logSelectedIceCandidatesInformation(PSampleStreamingSession);
STATUS logStartUpLatency(PSampleConfiguration);
STATUS createMessageQueue(UINT64, PPendingMessageQueue*);
STATUS freeMessageQueue(PPendingMessageQueue);
STATUS submitPendingIceCandidate(PPendingMessageQueue, PSampleStreamingSession);
STATUS removeExpiredMessageQueues(PStackQueue);
STATUS getPendingMessageQueueForHash(PStackQueue, UINT64, BOOL, PPendingMessageQueue*);
STATUS initSignaling(PSampleConfiguration, PCHAR);
BOOL sampleFilterNetworkInterfaces(UINT64, PCHAR);
UINT32 setLogLevel();

// Event data structure to pass to callbacks
typedef struct {
    app_webrtc_event_t event_id;
    UINT32 status_code;
    CHAR peer_id[MAX_SIGNALING_CLIENT_ID_LEN + 1];
    CHAR message[256];
} app_webrtc_event_data_t;

// Event callback type
typedef void (*app_webrtc_event_callback_t) (app_webrtc_event_data_t *event_data, void *user_ctx);

/**
 * @brief Register a callback for WebRTC events
 *
 * @param callback Function to call when events occur
 * @param user_ctx User context pointer passed to the callback
 *
 * @return 0 on success, non-zero on failure
 */
INT32 app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx);

typedef enum {
    APP_WEBRTC_CLASSIC_MODE, // Default: both signaling and streaming
    APP_WEBRTC_SIGNALING_ONLY_MODE,
    APP_WEBRTC_STREAMING_ONLY_MODE,
} app_webrtc_mode_t;

/**
 * @brief WebRTC application configuration structure
 */
typedef struct {
    // Channel configuration
    PCHAR pChannelName;                      // Name of the signaling channel
    SIGNALING_CHANNEL_ROLE_TYPE roleType;    // Role type (master or viewer)

    // AWS credentials configuration
    BOOL useIotCredentials;                  // Whether to use IoT Core credentials
    PCHAR iotCoreCredentialEndpoint;         // IoT Core credential endpoint
    PCHAR iotCoreCert;                       // Path to IoT Core certificate
    PCHAR iotCorePrivateKey;                 // Path to IoT Core private key
    PCHAR iotCoreRoleAlias;                  // IoT Core role alias
    PCHAR iotCoreThingName;                  // IoT Core thing name

    // Direct AWS credentials (if not using IoT credentials)
    PCHAR awsAccessKey;                      // AWS access key
    PCHAR awsSecretKey;                      // AWS secret key
    PCHAR awsSessionToken;                   // AWS session token

    // Common AWS options
    PCHAR awsRegion;                         // AWS region
    PCHAR caCertPath;                        // Path to CA certificates

    // WebRTC configuration
    BOOL trickleIce;                         // Whether to use trickle ICE
    BOOL useTurn;                            // Whether to use TURN servers
    UINT32 logLevel;                         // Log level

    // Media configuration
    RTC_CODEC audioCodec;                    // Audio codec to use
    RTC_CODEC videoCodec;                    // Video codec to use
    SampleStreamingMediaType mediaType;      // Media type (audio-only, video-only, or both)
    app_webrtc_mode_t mode;                  // Mode of the application

    // Callbacks
    startRoutine audioSourceCallback;        // Callback for audio source
    startRoutine videoSourceCallback;        // Callback for video source
    startRoutine receiveAudioVideoCallback;  // Callback for receiving audio/video
} WebRtcAppConfig, *PWebRtcAppConfig;

#define WEBRTC_APP_CONFIG_DEFAULT() \
{ \
    .roleType = SIGNALING_CHANNEL_ROLE_TYPE_MASTER, \
    .useIotCredentials = TRUE, \
    .trickleIce = TRUE, \
    .awsRegion = CONFIG_AWS_DEFAULT_REGION, \
    .useTurn = TRUE, \
    .logLevel = 3, \
    .audioCodec = RTC_CODEC_OPUS, \
    .videoCodec = RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE, \
    .mediaType = SAMPLE_STREAMING_AUDIO_VIDEO, \
    .mode = APP_WEBRTC_CLASSIC_MODE, \
    .audioSourceCallback = NULL, \
    .videoSourceCallback = NULL, \
    .receiveAudioVideoCallback = NULL, \
}

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
STATUS webrtcAppInit(PWebRtcAppConfig pConfig);

/**
 * @brief Run the WebRTC application and wait for termination
 *
 * This function starts the WebRTC streaming session and waits until it's terminated.
 *
 * @return STATUS code of the execution
 */
STATUS webrtcAppRun(VOID);

/**
 * @brief Terminate the WebRTC application
 *
 * This function cleans up resources and terminates the WebRTC application.
 *
 * @return STATUS code of the execution
 */
STATUS webrtcAppTerminate(VOID);

/**
 * @brief Get the sample configuration
 *
 * @return STATUS code of the execution
 */
STATUS webrtcAppGetSampleConfiguration(PSampleConfiguration *ppSampleConfiguration);

#ifdef __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_SAMPLE_INCLUDE__ */
