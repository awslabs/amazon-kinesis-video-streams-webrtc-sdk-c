/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>
#include "app_webrtc.h"
#include "webrtc_mem_utils.h"
#include "flash_wrapper.h"
#include "fileio.h"

#include "media_stream.h"
#include "signaling_serializer.h"
#include "webrtc_signaling_if.h"

#include "sdkconfig.h"
#include "iot_credential_provider.h"

static const char *TAG = "app_webrtc";

// Event handling
static app_webrtc_event_callback_t gEventCallback = NULL;
static void* gEventUserCtx = NULL;
static MUTEX gEventCallbackLock = INVALID_MUTEX_VALUE;

// Global variables to track app state
static WebRtcAppConfig gWebRtcAppConfig = {0};
static PVOID gSignalingClientData = NULL;  // Store the initialized signaling client
static BOOL gWebRtcAppInitialized = FALSE;

// Global callback for sending signaling messages in split mode
static app_webrtc_send_msg_cb_t g_sendMessageCallback = NULL;

// Task handle for the WebRTC run task
static TaskHandle_t gWebRtcRunTaskHandle = NULL;

// Forward declaration for wrapper function
static WEBRTC_STATUS signalingMessageReceivedWrapper(uint64_t customData, esp_webrtc_signaling_message_t* pWebRtcMessage);

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
    AppWebrtcStreamingMediaType mediaType;
    startRoutine audioSource;
    startRoutine videoSource;
    startRoutine receiveAudioVideoSource;
    RtcOnDataChannel onDataChannel;
    SignalingClientMetrics signalingClientMetrics;

    // Media capture interfaces
    void* videoCapture;
    void* audioCapture;

    // Media player interfaces
    void* videoPlayer;
    void* audioPlayer;

    // Media player handles
    video_player_handle_t video_player_handle;
    audio_player_handle_t audio_player_handle;

    // Count of active sessions using media players
    UINT32 activePlayerSessionCount;
    MUTEX playerLock;

    // Media reception
    BOOL receiveMedia;

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
} SampleConfiguration, *PSampleConfiguration;

typedef VOID (*StreamSessionShutdownCallback)(UINT64, PSampleStreamingSession);

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

PVOID sampleReceiveAudioVideoFrame(PVOID);
STATUS getIceCandidatePairStatsCallback(UINT32, UINT64, UINT64);
STATUS pregenerateCertTimerCallback(UINT32, UINT64, UINT64);
STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, UINT32 logLevel,
                                 BOOL signalingOnly, PSampleConfiguration* ppSampleConfiguration);
STATUS freeSampleConfiguration(PSampleConfiguration*);
STATUS signalingClientStateChanged(UINT64, SIGNALING_CLIENT_STATE);
STATUS signalingMessageReceived(UINT64, PReceivedSignalingMessage);
STATUS handleOffer(PSampleConfiguration, PSampleStreamingSession, PSignalingMessage);
STATUS handleRemoteCandidate(PSampleStreamingSession, PSignalingMessage);
STATUS initializePeerConnection(PSampleConfiguration, PRtcPeerConnection*);
STATUS createSampleStreamingSession(PSampleConfiguration, PCHAR, BOOL, PSampleStreamingSession*);
STATUS freeSampleStreamingSession(PSampleStreamingSession*);
STATUS streamingSessionOnShutdown(PSampleStreamingSession, UINT64, StreamSessionShutdownCallback);
STATUS sendSignalingMessage(PSampleStreamingSession, PSignalingMessage);
STATUS respondWithAnswer(PSampleStreamingSession);

VOID sampleBandwidthEstimationHandler(UINT64, DOUBLE);
VOID sampleSenderBandwidthEstimationHandler(UINT64, UINT32, UINT32, UINT32, UINT32, UINT64);
VOID onDataChannel(UINT64, PRtcDataChannel);
VOID onConnectionStateChange(UINT64, RTC_PEER_CONNECTION_STATE);
STATUS sessionCleanupWait(PSampleConfiguration, BOOL);
// STATUS logStartUpLatency(PSampleConfiguration);
STATUS createMessageQueue(UINT64, PPendingMessageQueue*);
STATUS freeMessageQueue(PPendingMessageQueue);
STATUS submitPendingIceCandidate(PPendingMessageQueue, PSampleStreamingSession);
STATUS removeExpiredMessageQueues(PStackQueue);
STATUS getPendingMessageQueueForHash(PStackQueue, UINT64, BOOL, PPendingMessageQueue*);
STATUS initSignaling(PSampleConfiguration, PCHAR);
BOOL sampleFilterNetworkInterfaces(UINT64, PCHAR);

// Forward declarations for media sender functions
PVOID sendVideoFramesFromCamera(PVOID args);
PVOID sendAudioFramesFromMic(PVOID args);
PVOID sendVideoFramesFromSamples(PVOID args);
PVOID sendAudioFramesFromSamples(PVOID args);
PVOID sampleReceiveAudioVideoFrame(PVOID);

// Forward declarations for media handlers
VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame);
VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame);

// Helper function to raise WebRTC events
static void raiseEvent(app_webrtc_event_t event_id, UINT32 status_code, PCHAR peer_id, PCHAR message)
{
    BOOL locked = FALSE;
    app_webrtc_event_data_t event_data = {0};

    // Check if there's a registered callback
    if (gEventCallback == NULL) {
        return;
    }

    // Prepare event data
    event_data.event_id = event_id;
    event_data.status_code = status_code;

    if (peer_id != NULL) {
        STRNCPY(event_data.peer_id, peer_id, MAX_SIGNALING_CLIENT_ID_LEN);
    }

    if (message != NULL) {
        STRNCPY(event_data.message, message, SIZEOF(event_data.message) - 1);
    }

    // Use lock for thread safety when accessing the callback
    if (IS_VALID_MUTEX_VALUE(gEventCallbackLock)) {
        MUTEX_LOCK(gEventCallbackLock);
        locked = TRUE;
    }

    // Call the user callback if set
    if (gEventCallback != NULL) {
        gEventCallback(&event_data, gEventUserCtx);
    }

    // Release the lock
    if (locked) {
        MUTEX_UNLOCK(gEventCallbackLock);
    }
}

// Function to register event callback
INT32 app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    if (gEventCallbackLock == INVALID_MUTEX_VALUE) {
        gEventCallbackLock = MUTEX_CREATE(TRUE);
        if (!IS_VALID_MUTEX_VALUE(gEventCallbackLock)) {
            return STATUS_INVALID_OPERATION;
        }
    }

    MUTEX_LOCK(gEventCallbackLock);
    locked = TRUE;

    // Set the callback
    gEventCallback = callback;
    gEventUserCtx = user_ctx;

    MUTEX_UNLOCK(gEventCallbackLock);
    locked = FALSE;

    if (locked) { // Is never TRUE currently
        MUTEX_UNLOCK(gEventCallbackLock);
    }
    return STATUS_SUCCESS;
}

#ifdef DYNAMIC_SIGNALING_PAYLOAD
/**
 * @brief Allocate memory for the payload of a SignalingMessage
 *
 * @param[in,out] pSignalingMessage The signaling message for which to allocate payload
 * @param[in] size Size in bytes to allocate
 *
 * @return STATUS code of the execution
 */
STATUS allocateSignalingMessagePayload(PSignalingMessage pSignalingMessage, UINT32 size)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK(size > 0, STATUS_INVALID_ARG);

    // Free any existing payload
    if (pSignalingMessage->payload != NULL) {
        freeSignalingMessagePayload(pSignalingMessage);
    }

    // Allocate new payload
    pSignalingMessage->payload = (PCHAR) MEMALLOC(size);
    CHK(pSignalingMessage->payload != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Clear the memory
    MEMSET(pSignalingMessage->payload, 0, size);

CleanUp:
    return retStatus;
}

/**
 * @brief Free the dynamically allocated payload of a SignalingMessage
 *
 * @param[in,out] pSignalingMessage The signaling message whose payload should be freed
 *
 * @return STATUS code of the execution
 */
STATUS freeSignalingMessagePayload(PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingMessage != NULL, STATUS_NULL_ARG);

    if (pSignalingMessage->payload != NULL) {
        MEMFREE(pSignalingMessage->payload);
        pSignalingMessage->payload = NULL;
        pSignalingMessage->payloadLen = 0;
    }

CleanUp:
    return retStatus;
}
#endif


PSampleConfiguration gSampleConfiguration = NULL;

VOID sigintHandler(INT32 sigNum)
{
    UNUSED_PARAM(sigNum);
    if (gSampleConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gSampleConfiguration->interrupted, TRUE);
        CVAR_BROADCAST(gSampleConfiguration->cvar);
    }
}

STATUS signalingCallFailed(STATUS status)
{
    return (STATUS_SIGNALING_GET_TOKEN_CALL_FAILED == status || STATUS_SIGNALING_DESCRIBE_CALL_FAILED == status ||
            STATUS_SIGNALING_CREATE_CALL_FAILED == status || STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED == status ||
            STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED == status || STATUS_SIGNALING_CONNECT_CALL_FAILED == status ||
            STATUS_SIGNALING_DESCRIBE_MEDIA_CALL_FAILED == status);
}

VOID onConnectionStateChange(UINT64 customData, RTC_PEER_CONNECTION_STATE newState)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    DLOGI("New connection state %u", newState);
    // Log ICE candidate gathering stats
    // DLOGI("ICE Candidates gathered - Host: %d, SRFLX: %d, Relay: %d",
    //       ATOMIC_LOAD(&pSampleConfiguration->hostCandidateCount),
    //       ATOMIC_LOAD(&pSampleConfiguration->srflxCandidateCount),
    //       ATOMIC_LOAD(&pSampleConfiguration->relayCandidateCount));

    switch (newState) {
        case RTC_PEER_CONNECTION_STATE_NEW:
            break;
        case RTC_PEER_CONNECTION_STATE_CONNECTING:
            raiseEvent(APP_WEBRTC_EVENT_PEER_CONNECTING, 0,
                        pSampleStreamingSession->peerId, "Peer connection connecting");
            break;
        case RTC_PEER_CONNECTION_STATE_CONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_PEER_CONNECTED, 0,
                        pSampleStreamingSession->peerId, "Peer connection established");
            break;
        case RTC_PEER_CONNECTION_STATE_DISCONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_PEER_DISCONNECTED, 0,
                        pSampleStreamingSession->peerId, "Peer connection disconnected");
            break;
        case RTC_PEER_CONNECTION_STATE_FAILED:
            raiseEvent(APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED, 0,
                        pSampleStreamingSession->peerId, "Peer connection failed");
            break;
        default:
            break;
    }

    switch (newState) {
        case RTC_PEER_CONNECTION_STATE_CONNECTING:
            // if (ATOMIC_LOAD(&pSampleConfiguration->hostCandidateCount) == 0) {
            //     DLOGW("No host candidates gathered yet!");
            // }
            break;
        case RTC_PEER_CONNECTION_STATE_CONNECTED:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);

            pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionConnectedTime =
                GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
            CHK_STATUS(peerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->peerConnectionMetrics));
            CHK_STATUS(iceAgentGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->iceMetrics));

            if (STATUS_FAILED(retStatus = logSelectedIceCandidatesInformation(pSampleStreamingSession->pPeerConnection))) {
                DLOGW("Failed to get information about selected Ice candidates: 0x%08x", retStatus);
            }
            break;
        case RTC_PEER_CONNECTION_STATE_FAILED:
            // Consider reconnecting if we didn't gather enough candidates
            // if (ATOMIC_LOAD(&pSampleConfiguration->hostCandidateCount) <= 1 &&
            //     ATOMIC_LOAD(&pSampleConfiguration->srflxCandidateCount) == 0) {
            //     DLOGE("Failed with insufficient ICE candidates - triggering reconnect");
            //     ATOMIC_STORE_BOOL(&pSampleConfiguration->restartSignaling, TRUE);
            // }
            /* fall-through */
        case RTC_PEER_CONNECTION_STATE_CLOSED:
            /* fall-through */
        case RTC_PEER_CONNECTION_STATE_DISCONNECTED:
            DLOGD("p2p connection disconnected");
            ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);
            /* fall-through */
        default:
            ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);
            CVAR_BROADCAST(pSampleConfiguration->cvar);

            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
}

STATUS signalingClientStateChanged(UINT64 customData, SIGNALING_CLIENT_STATE state)
{
    UNUSED_PARAM(customData);
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStateStr;

    signalingClientGetStateString(state, &pStateStr);

    DLOGV("Signaling client state changed to %d - '%s'", state, pStateStr);

    switch (state) {
        case SIGNALING_CLIENT_STATE_NEW:
            break;
        case SIGNALING_CLIENT_STATE_CONNECTING:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTING, 0, NULL, "Signaling client connecting");
            break;
        case SIGNALING_CLIENT_STATE_CONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTED, 0, NULL, "Signaling client connected");
            break;
        case SIGNALING_CLIENT_STATE_DISCONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED, 0, NULL, "Signaling client disconnected");
            break;
        default:
            break;
    }

    webrtc_mem_utils_print_stats(TAG);
    // Return success to continue
    return retStatus;
}

STATUS signalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    DLOGW("Signaling client generated an error 0x%08x - '%.*s'", status, msgLen, msg);
    CHAR errorMsg[256];
    SNPRINTF(errorMsg, SIZEOF(errorMsg), "Signaling error: 0x%08" PRIx32 " - %.*s", status, (int) msgLen, msg);
    raiseEvent(APP_WEBRTC_EVENT_SIGNALING_ERROR, status, NULL, errorMsg);

    // We will force re-create the signaling client on the following errors
    if (status == STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED || status == STATUS_SIGNALING_RECONNECT_FAILED) {
        ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);
    }

    return STATUS_SUCCESS;
}

PVOID mediaSenderRoutine(PVOID customData);

STATUS handleAnswer(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    UNUSED_PARAM(pSampleConfiguration);
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;
    BOOL mediaThreadStarted;

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &answerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &answerSessionDescriptionInit));

    // Start the media sender thread if not already started
    // This is needed for viewer mode where we send the offer and receive an answer
    mediaThreadStarted = ATOMIC_EXCHANGE_BOOL(&pSampleConfiguration->mediaThreadStarted, TRUE);
    if (!mediaThreadStarted) {
        DLOGI("Starting media sender thread after receiving answer");
        THREAD_CREATE_EX_EXT(&pSampleConfiguration->mediaSenderTid, "mediaSender", 8 * 1024, TRUE, mediaSenderRoutine, (PVOID) pSampleConfiguration);
    }

    // The audio video receive routine should be per streaming session
    if (pSampleConfiguration->receiveAudioVideoSource != NULL) {
        THREAD_CREATE_EX_EXT(&pSampleStreamingSession->receiveAudioVideoSenderTid, "receiveAudioVideoSrc", 8 * 1024, TRUE, pSampleConfiguration->receiveAudioVideoSource,
                      (PVOID) pSampleStreamingSession);
    }
CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

PVOID mediaSenderRoutine(PVOID customData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->connected) && !ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, 5 * HUNDREDS_OF_NANOS_IN_A_SECOND);
    }
    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    CHK(!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag), retStatus);

    // We are now connected and about to start streaming
    raiseEvent(APP_WEBRTC_EVENT_STREAMING_STARTED, 0, NULL, "Media streaming started");

    // Start video and audio threads based on configuration
    if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_VIDEO ||
        pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {

        // Determine which video source to use
        if (pSampleConfiguration->videoSource != NULL) {
            // Use the configured video source callback
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->videoSenderTid, "videoSender", 8 * 1024, TRUE,
                          pSampleConfiguration->videoSource, (PVOID) pSampleConfiguration);
        } else {
            // Use our built-in video sender functions
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->videoSenderTid, "videoSender", 8 * 1024, TRUE,
                          sendVideoFramesFromCamera, (PVOID) pSampleConfiguration);
        }
    }

    if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {
        // Determine which audio source to use
        if (pSampleConfiguration->audioSource != NULL) {
            // Use the configured audio source callback
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->audioSenderTid, "audioSender", 8 * 1024, TRUE,
                          pSampleConfiguration->audioSource, (PVOID) pSampleConfiguration);
        } else {
            // Use our built-in audio sender functions
            THREAD_CREATE_EX_EXT(&pSampleConfiguration->audioSenderTid, "audioSender", 8 * 1024, TRUE,
                          sendAudioFramesFromMic, (PVOID) pSampleConfiguration);
        }
    }

    if (pSampleConfiguration->videoSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->videoSenderTid, NULL);
    }

    if (pSampleConfiguration->audioSenderTid != INVALID_TID_VALUE) {
        THREAD_JOIN(pSampleConfiguration->audioSenderTid, NULL);
    }

CleanUp:
    // clean the flag of the media thread.
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);

    // Signal that streaming has stopped
    raiseEvent(APP_WEBRTC_EVENT_STREAMING_STOPPED, 0, NULL, "Media streaming stopped");

    CHK_LOG_ERR(retStatus);
    return NULL;
}

STATUS handleOffer(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    NullableBool canTrickle;
    BOOL mediaThreadStarted;

    CHK(pSampleConfiguration != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);

    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    MEMSET(&pSampleStreamingSession->answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    DLOGI("**offer:%s", pSignalingMessage->payload);
    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &offerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));
    canTrickle = canTrickleIceCandidates(pSampleStreamingSession->pPeerConnection);
    /* cannot be null after setRemoteDescription */
    CHECK(!NULLABLE_CHECK_EMPTY(canTrickle));
    pSampleStreamingSession->remoteCanTrickleIce = canTrickle.value;
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
    /*
     * If remote support trickle ice, send answer now. Otherwise answer will be sent once ice candidate gathering is complete.
     */
    if (pSampleStreamingSession->remoteCanTrickleIce) {
        CHK_STATUS(createAnswer(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
        CHK_STATUS(respondWithAnswer(pSampleStreamingSession));
    }
    mediaThreadStarted = ATOMIC_EXCHANGE_BOOL(&pSampleConfiguration->mediaThreadStarted, TRUE);
    if (!mediaThreadStarted) {
        THREAD_CREATE_EX_EXT(&pSampleConfiguration->mediaSenderTid, "mediaSender", 8 * 1024, TRUE, mediaSenderRoutine, (PVOID) pSampleConfiguration);
    }
    // The audio video receive routine should be per streaming session
    if (pSampleConfiguration->receiveAudioVideoSource != NULL) {
        THREAD_CREATE_EX_EXT(&pSampleStreamingSession->receiveAudioVideoSenderTid, "receiveAudioVideoSrc", 8 * 1024, TRUE, pSampleConfiguration->receiveAudioVideoSource,
                      (PVOID) pSampleStreamingSession);
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS sendSignalingMessage(PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    esp_webrtc_signaling_message_t message;

    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL && pMessage != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;

    if (gWebRtcAppConfig.pSignalingClientInterface != NULL && gSignalingClientData != NULL) {
        // Convert the message to the generic format
        message.version = pMessage->version;
        message.message_type = (esp_webrtc_signaling_message_type_t)pMessage->messageType;
        STRNCPY(message.correlation_id, pMessage->correlationId, MAX_CORRELATION_ID_LEN);
        message.correlation_id[MAX_CORRELATION_ID_LEN] = '\0';
        STRNCPY(message.peer_client_id, pMessage->peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
        message.peer_client_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        message.payload = pMessage->payload;
        message.payload_len = pMessage->payloadLen;

        // Use the signaling interface to send the message
        CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->sendMessage(gSignalingClientData, &message));

        if (pMessage->messageType == SIGNALING_MESSAGE_TYPE_ANSWER) {
            DLOGD("Sent answer to peer %s", pMessage->peerClientId);
        }
    } else {
        DLOGE("No signaling client interface available");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS respondWithAnswer(PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    SignalingMessage message;
    UINT32 buffLen = MAX_SIGNALING_MESSAGE_LEN;

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    message.payload = (PCHAR) MEMCALLOC(1, MAX_SIGNALING_MESSAGE_LEN + 1);
    CHK(message.payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
#endif

    CHK_STATUS(serializeSessionDescriptionInit(&pSampleStreamingSession->answerSessionDescriptionInit, message.payload, &buffLen));

    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    STRNCPY(message.peerClientId, pSampleStreamingSession->peerId, MAX_SIGNALING_CLIENT_ID_LEN);
    message.payloadLen = (UINT32) STRLEN(message.payload);
    // SNPRINTF appends null terminator, so we do not manually add it
    SNPRINTF(message.correlationId, MAX_CORRELATION_ID_LEN, "%llu_%zu", GETTIME(), ATOMIC_INCREMENT(&pSampleStreamingSession->correlationIdPostFix));
    DLOGD("Responding With Answer With correlationId: %s", message.correlationId);

    // Emit event for sent answer
    raiseEvent(APP_WEBRTC_EVENT_SENT_ANSWER, 0, pSampleStreamingSession->peerId, "Sent answer to peer");

    CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));

CleanUp:

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    SAFE_MEMFREE(message.payload);
#endif

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

BOOL sampleFilterNetworkInterfaces(UINT64 customData, PCHAR networkInt)
{
    UNUSED_PARAM(customData);
    BOOL useInterface = TRUE;  // Default to allowing interfaces

    // Filter out IPv6 interfaces
    if (STRSTR(networkInt, "::") != NULL) {
        DLOGD("Filtering out IPv6 interface: %s", networkInt);
        return FALSE;
    }

    BOOL isIpv6 = (STRSTR(networkInt, ":") != NULL && STRSTR(networkInt, ".") == NULL);
    if (isIpv6) {
        DLOGD("Filtering out IPv6 interface: %s", networkInt);
        return FALSE;
    }

    // Allow all non-IPv6 interfaces on ESP32
    DLOGD("Using interface: %s", networkInt);
    return useInterface;
}

VOID onIceCandidateHandler(UINT64 customData, PCHAR candidateJson)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    SignalingMessage message;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    if (candidateJson == NULL) {
        DLOGD("ice candidate gathering finished");
        ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, TRUE);

        // ICE gathering is complete
        raiseEvent(APP_WEBRTC_EVENT_ICE_GATHERING_COMPLETE, 0,
                   pSampleStreamingSession->peerId, "ICE candidate gathering completed");

        // if application is master and non-trickle ice, send answer now.
        if (pSampleStreamingSession->pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_MASTER &&
            !pSampleStreamingSession->remoteCanTrickleIce) {
            CHK_STATUS(createAnswer(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->answerSessionDescriptionInit));
            CHK_STATUS(respondWithAnswer(pSampleStreamingSession));
        } else if (pSampleStreamingSession->pSampleConfiguration->channelInfo.channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER &&
                   !pSampleStreamingSession->pSampleConfiguration->trickleIce) {
            CVAR_BROADCAST(pSampleStreamingSession->pSampleConfiguration->cvar);
        }

    } else if (pSampleStreamingSession->remoteCanTrickleIce && ATOMIC_LOAD_BOOL(&pSampleStreamingSession->peerIdReceived)) {
        DLOGI("New local ICE candidate gathered: %s", candidateJson);

        // Send the ICE candidate to the peer through signaling
        message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
        STRNCPY(message.peerClientId, pSampleStreamingSession->peerId, MAX_SIGNALING_CLIENT_ID_LEN);
        message.payloadLen = (UINT32) STRNLEN(candidateJson, MAX_SIGNALING_MESSAGE_LEN);

#ifdef DYNAMIC_SIGNALING_PAYLOAD
        message.payload = (PCHAR) MEMCALLOC(1, MAX_SIGNALING_MESSAGE_LEN + 1);
        CHK(message.payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
        STRNCPY(message.payload, candidateJson, message.payloadLen);
#else
        STRNCPY(message.payload, candidateJson, message.payloadLen);
#endif
        message.correlationId[0] = '\0';

        CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));

        // ICE candidate sent successfully
        raiseEvent(APP_WEBRTC_EVENT_SENT_ICE_CANDIDATE, 0,
                   pSampleStreamingSession->peerId, "Sent ICE candidate");

#ifdef DYNAMIC_SIGNALING_PAYLOAD
        SAFE_MEMFREE(message.payload);
#endif
    } else {
        DLOGI("New local ICE candidate gathered but not sending: candidateJson=%s, remoteCanTrickleIce=%d, peerIdReceived=%d",
              candidateJson ? candidateJson : "NULL",
              pSampleStreamingSession->remoteCanTrickleIce,
              ATOMIC_LOAD_BOOL(&pSampleStreamingSession->peerIdReceived));
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
}

STATUS initializePeerConnection(PSampleConfiguration pSampleConfiguration, PRtcPeerConnection* ppRtcPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcConfiguration configuration;
    UINT64 data;
    PRtcCertificate pRtcCertificate = NULL;
    // BOOL isStreamingOnly = FALSE;

    CHK(pSampleConfiguration != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    // Check if we're in streaming-only mode (no signaling)
    // isStreamingOnly = (pSampleConfiguration->signalingClientHandle == NULL);

    // Initialize the configuration structure to zeros
    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set more aggressive ICE timeouts
    configuration.kvsRtcConfiguration.iceConnectionCheckTimeout = 12 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 12 seconds
    configuration.kvsRtcConfiguration.iceLocalCandidateGatheringTimeout = 10 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 10 seconds
    configuration.kvsRtcConfiguration.iceCandidateNominationTimeout = 15 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 15 seconds
    // More frequent checks for faster connection
    configuration.kvsRtcConfiguration.iceConnectionCheckPollingInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // 100ms

    // Enable interface filtering to handle IPv6 properly
    configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = sampleFilterNetworkInterfaces;

    // Set the ICE mode - use both STUN and TURN (if available)
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    // Configure ICE servers
    if (gWebRtcAppConfig.pSignalingClientInterface != NULL &&
        gSignalingClientData != NULL &&
        gWebRtcAppConfig.pSignalingClientInterface->getIceServers != NULL) {

        // Use the signaling interface to get ICE servers
        CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->getIceServers(
            gSignalingClientData,
            &pSampleConfiguration->iceUriCount,
            &configuration));
        DLOGD("Got %d ICE servers from signaling interface", pSampleConfiguration->iceUriCount);
    } else {
        // Fallback to hardcoded STUN servers
        SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN,
                 "stun:stun.l.google.com:19302");


        // Set the count of ICE servers
        pSampleConfiguration->iceUriCount = 1;

        // Make sure credentials are empty for STUN
        configuration.iceServers[0].username[0] = '\0';
        configuration.iceServers[0].credential[0] = '\0';

        DLOGD("Using hardcoded STUN servers");
    }

    // Log the ICE servers
    for (int i = 0; i < pSampleConfiguration->iceUriCount; i++) {
        DLOGD("ICE server %d: %s", i, configuration.iceServers[i].urls);
    }
    // ICE server count is now set by the getIceServers function or fallback

    // Check if we have any pregenerated certs and use them
    // NOTE: We are running under the config lock
    retStatus = stackQueueDequeue(pSampleConfiguration->pregeneratedCertificates, &data);
    CHK(retStatus == STATUS_SUCCESS || retStatus == STATUS_NOT_FOUND, retStatus);

    if (retStatus == STATUS_NOT_FOUND) {
        retStatus = STATUS_SUCCESS;
    } else {
        // Use the pre-generated cert and get rid of it to not reuse again
        pRtcCertificate = (PRtcCertificate) data;
        configuration.certificates[0] = *pRtcCertificate;
    }

    CHK_STATUS(createPeerConnection(&configuration, ppRtcPeerConnection));
CleanUp:

    CHK_LOG_ERR(retStatus);

    // Free the certificate which can be NULL as we no longer need it and won't reuse
    freeRtcCertificate(pRtcCertificate);

    LEAVES();
    return retStatus;
}

STATUS createSampleStreamingSession(PSampleConfiguration pSampleConfiguration, PCHAR peerId, BOOL isMaster,
                                    PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;

    RtcMediaStreamTrack videoTrack, audioTrack;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    RtcRtpTransceiverInit audioRtpTransceiverInit;
    RtcRtpTransceiverInit videoRtpTransceiverInit;

    MEMSET(&videoTrack, 0x00, SIZEOF(RtcMediaStreamTrack));
    MEMSET(&audioTrack, 0x00, SIZEOF(RtcMediaStreamTrack));

    CHK(pSampleConfiguration != NULL && ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    CHK((isMaster && peerId != NULL) || !isMaster, STATUS_INVALID_ARG);

    pSampleStreamingSession = (PSampleStreamingSession) MEMCALLOC(1, SIZEOF(SampleStreamingSession));
    pSampleStreamingSession->firstFrame = TRUE;
    pSampleStreamingSession->offerReceiveTime = GETTIME();
    CHK(pSampleStreamingSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    if (isMaster) {
        STRCPY(pSampleStreamingSession->peerId, peerId);
    } else {
        STRCPY(pSampleStreamingSession->peerId, SAMPLE_VIEWER_CLIENT_ID);
    }
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->peerIdReceived, TRUE);

    pSampleStreamingSession->pAudioRtcRtpTransceiver = NULL;
    pSampleStreamingSession->pVideoRtcRtpTransceiver = NULL;

    pSampleStreamingSession->pSampleConfiguration = pSampleConfiguration;
    pSampleStreamingSession->rtcMetricsHistory.prevTs = GETTIME();

    pSampleStreamingSession->peerConnectionMetrics.version = PEER_CONNECTION_METRICS_CURRENT_VERSION;
    pSampleStreamingSession->iceMetrics.version = ICE_AGENT_METRICS_CURRENT_VERSION;

    // if we're the viewer, we control the trickle ice mode
    pSampleStreamingSession->remoteCanTrickleIce = !isMaster && pSampleConfiguration->trickleIce;

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, FALSE);
    pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionStartTime = GETTIME() / HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    if (pSampleConfiguration->enableTwcc) {
        pSampleStreamingSession->twccMetadata.updateLock = MUTEX_CREATE(TRUE);
    }

    CHK_STATUS(initializePeerConnection(pSampleConfiguration, &pSampleStreamingSession->pPeerConnection));

    CHK_STATUS(peerConnectionOnIceCandidate(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onIceCandidateHandler));

    DLOGD("Added onIceCandidateHandler handler to peerConnectionOnIceCandidate done");

    CHK_STATUS(
        peerConnectionOnConnectionStateChange(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession, onConnectionStateChange));

#ifdef ENABLE_DATA_CHANNEL
    if (pSampleConfiguration->onDataChannel != NULL) {
        retStatus = peerConnectionOnDataChannel(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                               pSampleConfiguration->onDataChannel);
        if (retStatus != STATUS_SUCCESS) {
            DLOGW("Failed to set data channel callback");
        }
        CHK_STATUS(retStatus);
    }
#endif

    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, pSampleConfiguration->videoCodec));

    CHK_STATUS(addSupportedCodec(pSampleStreamingSession->pPeerConnection, pSampleConfiguration->audioCodec));

    // Add a SendRecv Transceiver of type video
    videoTrack.kind = MEDIA_STREAM_TRACK_KIND_VIDEO;
    videoTrack.codec = pSampleConfiguration->videoCodec;
    if (pSampleConfiguration->receiveMedia) {
        videoRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    } else {
        videoRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    }
    STRCPY(videoTrack.streamId, "myKvsVideoStream");
    STRCPY(videoTrack.trackId, "myVideoTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &videoTrack, &videoRtpTransceiverInit,
                              &pSampleStreamingSession->pVideoRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pVideoRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));

    // Add a SendRecv Transceiver of type audio
    audioTrack.kind = MEDIA_STREAM_TRACK_KIND_AUDIO;
    audioTrack.codec = pSampleConfiguration->audioCodec;
    if (pSampleConfiguration->receiveMedia) {
        audioRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    } else {
        audioRtpTransceiverInit.direction = RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    }
    STRCPY(audioTrack.streamId, "myKvsVideoStream");
    STRCPY(audioTrack.trackId, "myAudioTrack");
    CHK_STATUS(addTransceiver(pSampleStreamingSession->pPeerConnection, &audioTrack, &audioRtpTransceiverInit,
                              &pSampleStreamingSession->pAudioRtcRtpTransceiver));

    CHK_STATUS(transceiverOnBandwidthEstimation(pSampleStreamingSession->pAudioRtcRtpTransceiver, (UINT64) pSampleStreamingSession,
                                                sampleBandwidthEstimationHandler));

    // twcc bandwidth estimation
    if (pSampleConfiguration->enableTwcc) {
        CHK_STATUS(peerConnectionOnSenderBandwidthEstimation(pSampleStreamingSession->pPeerConnection, (UINT64) pSampleStreamingSession,
                                                             sampleSenderBandwidthEstimationHandler));
    }

    pSampleStreamingSession->startUpLatency = 0;
CleanUp:

    if (STATUS_FAILED(retStatus) && pSampleStreamingSession != NULL) {
        freeSampleStreamingSession(&pSampleStreamingSession);
        pSampleStreamingSession = NULL;
    }

    if (ppSampleStreamingSession != NULL) {
        *ppSampleStreamingSession = pSampleStreamingSession;
    }

    return retStatus;
}

STATUS freeSampleStreamingSession(PSampleStreamingSession* ppSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PSampleConfiguration pSampleConfiguration;

    CHK(ppSampleStreamingSession != NULL, STATUS_NULL_ARG);
    pSampleStreamingSession = *ppSampleStreamingSession;
    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL, retStatus);
    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;

    DLOGD("Freeing streaming session with peer id: %s ", pSampleStreamingSession->peerId);

    ATOMIC_STORE_BOOL(&pSampleStreamingSession->terminateFlag, TRUE);

    if (pSampleStreamingSession->shutdownCallback != NULL) {
        pSampleStreamingSession->shutdownCallback(pSampleStreamingSession->shutdownCallbackCustomData, pSampleStreamingSession);
    }

    if (IS_VALID_TID_VALUE(pSampleStreamingSession->receiveAudioVideoSenderTid)) {
        THREAD_JOIN(pSampleStreamingSession->receiveAudioVideoSenderTid, NULL);
    }

    // Update player session count and potentially stop players
    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->playerLock)) {
        MUTEX_LOCK(pSampleConfiguration->playerLock);

        // Decrement active session count
        if (pSampleConfiguration->activePlayerSessionCount > 0) {
            pSampleConfiguration->activePlayerSessionCount--;

            // If this was the last active session, clean up the players
            if (pSampleConfiguration->activePlayerSessionCount == 0) {
                // Clean up video player if initialized
                if (pSampleConfiguration->video_player_handle != NULL && pSampleConfiguration->videoPlayer != NULL) {
                    media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->videoPlayer;
                    ESP_LOGI(TAG, "Stopping video player (last session)");
                    video_player->stop(pSampleConfiguration->video_player_handle);
                    // Note: We don't deinit here, as we might reuse the player for future sessions
                }

                // Clean up audio player if initialized
                if (pSampleConfiguration->audio_player_handle != NULL && pSampleConfiguration->audioPlayer != NULL) {
                    media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audioPlayer;
                    ESP_LOGI(TAG, "Stopping audio player (last session)");
                    audio_player->stop(pSampleConfiguration->audio_player_handle);
                    // Note: We don't deinit here, as we might reuse the player for future sessions
                }
            }
        }

        MUTEX_UNLOCK(pSampleConfiguration->playerLock);
    }

    // De-initialize the session stats timer if there are no active sessions
    // NOTE: we need to perform this under the lock which might be acquired by
    // the running thread but it's OK as it's re-entrant
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    if (pSampleConfiguration->iceCandidatePairStatsTimerId != MAX_UINT32 && pSampleConfiguration->streamingSessionCount == 0 &&
        IS_VALID_TIMER_QUEUE_HANDLE(pSampleConfiguration->timerQueueHandle)) {
        CHK_LOG_ERR(timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->iceCandidatePairStatsTimerId,
                                          (UINT64) pSampleConfiguration));
        pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
    }
    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);

    if (pSampleConfiguration->enableTwcc) {
        if (IS_VALID_MUTEX_VALUE(pSampleStreamingSession->twccMetadata.updateLock)) {
            MUTEX_FREE(pSampleStreamingSession->twccMetadata.updateLock);
        }
    }

    CHK_LOG_ERR(closePeerConnection(pSampleStreamingSession->pPeerConnection));

    CHK_LOG_ERR(freePeerConnection(&pSampleStreamingSession->pPeerConnection));
    SAFE_MEMFREE(pSampleStreamingSession);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS streamingSessionOnShutdown(PSampleStreamingSession pSampleStreamingSession, UINT64 customData,
                                  StreamSessionShutdownCallback streamSessionShutdownCallback)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleStreamingSession != NULL && streamSessionShutdownCallback != NULL, STATUS_NULL_ARG);

    pSampleStreamingSession->shutdownCallbackCustomData = customData;
    pSampleStreamingSession->shutdownCallback = streamSessionShutdownCallback;

CleanUp:

    return retStatus;
}

VOID sampleBandwidthEstimationHandler(UINT64 customData, DOUBLE maximumBitrate)
{
    UNUSED_PARAM(customData);
    DLOGV("received bitrate suggestion: %f", maximumBitrate);
#if CONFIG_IDF_TARGET_ESP32P4
    // FIXME: Do this via media_stream API
    extern esp_err_t esp_h264_hw_enc_set_bitrate(uint32_t bitrate);
    esp_h264_hw_enc_set_bitrate((uint32_t) maximumBitrate);
#endif
}

// Sample callback for TWCC. Average packet is calculated with exponential moving average (EMA). If average packet lost is <= 5%,
// the current bitrate is increased by 5%. If more than 5%, the current bitrate
// is reduced by percent lost. Bitrate update is allowed every second and is increased/decreased upto the limits
VOID sampleSenderBandwidthEstimationHandler(UINT64 customData, UINT32 txBytes, UINT32 rxBytes, UINT32 txPacketsCnt, UINT32 rxPacketsCnt,
                                            UINT64 duration)
{
    UNUSED_PARAM(duration);
    UINT64 videoBitrate, audioBitrate;
    UINT64 currentTimeMs, timeDiff;
    UINT32 lostPacketsCnt = txPacketsCnt - rxPacketsCnt;
    DOUBLE percentLost = (DOUBLE) ((txPacketsCnt > 0) ? (lostPacketsCnt * 100 / txPacketsCnt) : 0.0);
    SampleStreamingSession* pSampleStreamingSession = (SampleStreamingSession*) customData;

    if (pSampleStreamingSession == NULL) {
        DLOGW("Invalid streaming session (NULL object)");
        return;
    }

    // Calculate packet loss
    pSampleStreamingSession->twccMetadata.averagePacketLoss =
        EMA_ACCUMULATOR_GET_NEXT(pSampleStreamingSession->twccMetadata.averagePacketLoss, ((DOUBLE) percentLost));

    currentTimeMs = GETTIME();
    timeDiff = currentTimeMs - pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs;
    if (timeDiff < TWCC_BITRATE_ADJUSTMENT_INTERVAL_MS) {
        // Too soon for another adjustment
        return;
    }

    MUTEX_LOCK(pSampleStreamingSession->twccMetadata.updateLock);
    videoBitrate = pSampleStreamingSession->twccMetadata.currentVideoBitrate;
    audioBitrate = pSampleStreamingSession->twccMetadata.currentAudioBitrate;

    if (pSampleStreamingSession->twccMetadata.averagePacketLoss <= 5) {
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        videoBitrate = (UINT64) MIN(videoBitrate * 1.05, MAX_VIDEO_BITRATE_KBPS);
        // increase encoder bitrate by 5 percent with a cap at MAX_BITRATE
        audioBitrate = (UINT64) MIN(audioBitrate * 1.05, MAX_AUDIO_BITRATE_BPS);
    } else {
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        videoBitrate = (UINT64) MAX(videoBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_VIDEO_BITRATE_KBPS);
        // decrease encoder bitrate by average packet loss percent, with a cap at MIN_BITRATE
        audioBitrate = (UINT64) MAX(audioBitrate * (1.0 - pSampleStreamingSession->twccMetadata.averagePacketLoss / 100.0), MIN_AUDIO_BITRATE_BPS);
    }

    // Update the session with the new bitrate and adjustment time
    pSampleStreamingSession->twccMetadata.newVideoBitrate = videoBitrate;
    pSampleStreamingSession->twccMetadata.newAudioBitrate = audioBitrate;
    MUTEX_UNLOCK(pSampleStreamingSession->twccMetadata.updateLock);

    pSampleStreamingSession->twccMetadata.lastAdjustmentTimeMs = currentTimeMs;

    DLOGI("Adjustment made: average packet loss = %.2f%%, timediff: %llu ms", pSampleStreamingSession->twccMetadata.averagePacketLoss, timeDiff);
    DLOGI("Suggested video bitrate %u kbps, suggested audio bitrate: %u bps, sent: %u bytes %u packets received: %u bytes %u packets in %lu msec",
          videoBitrate, audioBitrate, txBytes, txPacketsCnt, rxBytes, rxPacketsCnt, duration / 10000ULL);
}

STATUS handleRemoteCandidate(PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    RtcIceCandidateInit iceCandidate;
    CHK(pSampleStreamingSession != NULL && pSignalingMessage != NULL, STATUS_NULL_ARG);

    // Add logging for debugging
    DLOGD("Received remote ICE candidate");

    CHK_STATUS(deserializeRtcIceCandidateInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &iceCandidate));

    // Validate candidate before adding
    if (iceCandidate.candidate[0] == '\0') {
        DLOGW("Empty ICE candidate received");
        CHK(FALSE, STATUS_INVALID_ARG);
    }

#if 0
    // Check if candidate contains IPv6 address
    BOOL isIpv6 = (STRSTR(iceCandidate.candidate, "::") != NULL);
    isIpv6 = isIpv6 || (STRSTR(iceCandidate.candidate, ":") != NULL && STRSTR(iceCandidate.candidate, ".") == NULL);

    if (isIpv6) {
        DLOGD("Skipping IPv6 ICE candidate: %s", iceCandidate.candidate);
        CHK(FALSE, STATUS_INVALID_ARG);
    }
#endif

    DLOGD("Adding ICE candidate: %s", iceCandidate.candidate);
    CHK_STATUS(addIceCandidate(pSampleStreamingSession->pPeerConnection, iceCandidate.candidate));

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS traverseDirectoryPEMFileScan(UINT64 customData, DIR_ENTRY_TYPES entryType, PCHAR fullPath, PCHAR fileName)
{
    UNUSED_PARAM(entryType);
    UNUSED_PARAM(fullPath);

    PCHAR certName = (PCHAR) customData;
    UINT32 fileNameLen = STRLEN(fileName);

    if (fileNameLen > ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1 &&
        (STRCMPI(CA_CERT_PEM_FILE_EXTENSION, &fileName[fileNameLen - ARRAY_SIZE(CA_CERT_PEM_FILE_EXTENSION) + 1]) == 0)) {
        certName[0] = FPATHSEPARATOR;
        certName++;
        STRCPY(certName, fileName);
    }

    return STATUS_SUCCESS;
}


// Configuration function without AWS credential options
STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, UINT32 logLevel,
                                 BOOL signalingOnly, PSampleConfiguration* ppSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSampleConfiguration = (PSampleConfiguration) MEMCALLOC(1, SIZEOF(SampleConfiguration))), STATUS_NOT_ENOUGH_MEMORY);

    // Store the log level
    SET_LOGGER_LOG_LEVEL(logLevel);
    pSampleConfiguration->logLevel = logLevel;

    // Initialize mutexes and condition variables
    pSampleConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
    pSampleConfiguration->cvar = CVAR_CREATE();
    pSampleConfiguration->streamingSessionListReadLock = MUTEX_CREATE(FALSE);
    pSampleConfiguration->signalingSendMessageLock = MUTEX_CREATE(FALSE);

    // Only initialize player lock for streaming mode
    if (!signalingOnly) {
        pSampleConfiguration->playerLock = MUTEX_CREATE(TRUE);
    } else {
        pSampleConfiguration->playerLock = INVALID_MUTEX_VALUE;
    }

    // Initialize thread IDs to invalid values
    pSampleConfiguration->mediaSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;

    // Only initialize player-related fields for streaming mode
    if (!signalingOnly) {
        pSampleConfiguration->video_player_handle = NULL;
        pSampleConfiguration->audio_player_handle = NULL;
        pSampleConfiguration->activePlayerSessionCount = 0;
    }

    // Set ICE configuration
    pSampleConfiguration->trickleIce = trickleIce;
    pSampleConfiguration->useTurn = useTurn;

    // Initialize channel info
    pSampleConfiguration->channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    pSampleConfiguration->channelInfo.pChannelName = channelName;
    pSampleConfiguration->channelInfo.pKmsKeyId = NULL;
    pSampleConfiguration->channelInfo.tagCount = 0;
    pSampleConfiguration->channelInfo.pTags = NULL;
    pSampleConfiguration->channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pSampleConfiguration->channelInfo.channelRoleType = roleType;
    pSampleConfiguration->channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    pSampleConfiguration->channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    pSampleConfiguration->channelInfo.asyncIceServerConfig = TRUE;
    pSampleConfiguration->channelInfo.retry = TRUE;
    pSampleConfiguration->channelInfo.reconnect = TRUE;

    // Initialize client info
    pSampleConfiguration->clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    pSampleConfiguration->clientInfo.loggingLevel = logLevel;
    pSampleConfiguration->clientInfo.cacheFilePath = NULL; // Use the default path
    pSampleConfiguration->clientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;

    // Initialize timers and metrics
    pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
    pSampleConfiguration->pregenerateCertTimerId = MAX_UINT32;
    pSampleConfiguration->signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    // Initialize atomic flags
    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);

    // Set default media type to audio-video
    pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;

    // Only create timer queue and certificate pre-generation for streaming mode
    if (!signalingOnly) {
#define TIMER_QUEUE_THREAD_SIZE (8 * 1024)
        // Create timer queue for ICE stats and certificate pre-generation
        timerQueueCreateEx(&pSampleConfiguration->timerQueueHandle, "pregenCertTmr", TIMER_QUEUE_THREAD_SIZE);
        CHK_STATUS(stackQueueCreate(&pSampleConfiguration->pregeneratedCertificates));

        // Start the cert pre-gen timer callback
        if (SAMPLE_PRE_GENERATE_CERT) {
            CHK_LOG_ERR(retStatus =
                            timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, 0, SAMPLE_PRE_GENERATE_CERT_PERIOD, pregenerateCertTimerCallback,
                                            (UINT64) pSampleConfiguration, &pSampleConfiguration->pregenerateCertTimerId));
        }
    } else {
        ESP_LOGI(TAG, "Signaling-only mode: Skipping timer queue and certificate pre-generation to save memory");
        pSampleConfiguration->timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
        pSampleConfiguration->pregeneratedCertificates = NULL;
    }

    // Initialize queues and hash tables for signaling
    CHK_STATUS(stackQueueCreate(&pSampleConfiguration->pPendingSignalingMessageForRemoteClient));
    CHK_STATUS(hashTableCreateWithParams(SAMPLE_HASH_TABLE_BUCKET_COUNT, SAMPLE_HASH_TABLE_BUCKET_LENGTH,
                                         &pSampleConfiguration->pRtcPeerConnectionForRemoteClient));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        freeSampleConfiguration(&pSampleConfiguration);
    }

    if (ppSampleConfiguration != NULL) {
        *ppSampleConfiguration = pSampleConfiguration;
    }

    return retStatus;
}

STATUS initSignaling(PSampleConfiguration pSampleConfiguration, PCHAR clientId)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Check if we have a signaling client interface and data
    if (gWebRtcAppConfig.pSignalingClientInterface != NULL && gSignalingClientData != NULL) {
        ESP_LOGI(TAG, "Using provided signaling client interface");

        // Set up callbacks for the signaling client
        retStatus = gWebRtcAppConfig.pSignalingClientInterface->setCallbacks(
            gSignalingClientData,
            (uint64_t) pSampleConfiguration,
            signalingMessageReceivedWrapper,
            signalingClientStateChanged,
            signalingClientError);

        CHK_STATUS(retStatus);

        // Store the client ID
        STRCPY(pSampleConfiguration->clientInfo.clientId, clientId);

        // Raise event that we're connecting
        raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTING, 0, NULL, "Using provided signaling client");
    } else {
        DLOGE("No signaling client interface provided");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        raiseEvent(APP_WEBRTC_EVENT_SIGNALING_ERROR, retStatus, NULL, "Failed to initialize signaling");
    }
    return retStatus;
}

STATUS getIceCandidatePairStatsCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    UINT32 i;
    BOOL locked = FALSE;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] getPeriodicStats(): Passed argument is NULL");

    pSampleConfiguration->rtcIceCandidatePairMetrics.requestedTypeOfStats = RTC_STATS_TYPE_CANDIDATE_PAIR;

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        locked = TRUE;
    }

    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        // Delegate to WebRtcLogging.c function to handle the actual logging
        retStatus = logIceCandidatePairStats(
            pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection,
            &pSampleConfiguration->rtcIceCandidatePairMetrics,
            &pSampleConfiguration->sampleStreamingSessionList[i]->rtcMetricsHistory
        );

        if (STATUS_FAILED(retStatus)) {
            DLOGW("Failed to log ICE candidate pair stats: 0x%08x", retStatus);
            // Continue with other sessions even if one fails
            retStatus = STATUS_SUCCESS;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS pregenerateCertTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    BOOL locked = FALSE;
    UINT32 certCount;
    PRtcCertificate pRtcCertificate = NULL;

    CHK_WARN(pSampleConfiguration != NULL, STATUS_NULL_ARG, "[KVS Master] pregenerateCertTimerCallback(): Passed argument is NULL");

    // Use MUTEX_TRYLOCK to avoid possible dead lock when canceling timerQueue
    if (!MUTEX_TRYLOCK(pSampleConfiguration->sampleConfigurationObjLock)) {
        return retStatus;
    } else {
        locked = TRUE;
    }

    // Quick check if there is anything that needs to be done.
    CHK_STATUS(stackQueueGetCount(pSampleConfiguration->pregeneratedCertificates, &certCount));
    CHK(certCount != MAX_RTCCONFIGURATION_CERTIFICATES, retStatus);

    // Generate the certificate with the keypair
    CHK_STATUS(createRtcCertificate(&pRtcCertificate));

    // Add to the stack queue
    CHK_STATUS(stackQueueEnqueue(pSampleConfiguration->pregeneratedCertificates, (UINT64) pRtcCertificate));

    DLOGV("New certificate has been pre-generated and added to the queue");

    // Reset it so it won't be freed on exit
    pRtcCertificate = NULL;

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

CleanUp:

    if (pRtcCertificate != NULL) {
        freeRtcCertificate(pRtcCertificate);
    }

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    return retStatus;
}

STATUS freeSampleConfiguration(PSampleConfiguration* ppSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration;
    UINT32 i;
    UINT64 data;
    StackQueueIterator iterator;
    BOOL locked = FALSE;
    // BOOL isStreamingOnly = FALSE;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = *ppSampleConfiguration;

    CHK(pSampleConfiguration != NULL, retStatus);

    // Clean up video player if initialized
    if (pSampleConfiguration->video_player_handle != NULL && pSampleConfiguration->videoPlayer != NULL) {
        media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->videoPlayer;
        ESP_LOGI(TAG, "Cleaning up video player");
        video_player->stop(pSampleConfiguration->video_player_handle);
        video_player->deinit(pSampleConfiguration->video_player_handle);
        pSampleConfiguration->video_player_handle = NULL;
    }

    // Clean up audio player if initialized
    if (pSampleConfiguration->audio_player_handle != NULL && pSampleConfiguration->audioPlayer != NULL) {
        media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audioPlayer;
        ESP_LOGI(TAG, "Cleaning up audio player");
        audio_player->stop(pSampleConfiguration->audio_player_handle);
        audio_player->deinit(pSampleConfiguration->audio_player_handle);
        pSampleConfiguration->audio_player_handle = NULL;
    }

    // Free the player lock
    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->playerLock)) {
        MUTEX_FREE(pSampleConfiguration->playerLock);
    }
    // Check if we're in streaming-only mode
    // isStreamingOnly = (pSampleConfiguration->channelInfo.pChannelName == NULL);

    if (IS_VALID_TIMER_QUEUE_HANDLE(pSampleConfiguration->timerQueueHandle)) {
        if (pSampleConfiguration->iceCandidatePairStatsTimerId != MAX_UINT32) {
            retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->iceCandidatePairStatsTimerId,
                                              (UINT64) pSampleConfiguration);
            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to cancel stats timer with: 0x%08x", retStatus);
            }
            pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
        }

        if (pSampleConfiguration->pregenerateCertTimerId != MAX_UINT32) {
            retStatus = timerQueueCancelTimer(pSampleConfiguration->timerQueueHandle, pSampleConfiguration->pregenerateCertTimerId,
                                              (UINT64) pSampleConfiguration);
            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to cancel certificate pre-generation timer with: 0x%08x", retStatus);
            }
            pSampleConfiguration->pregenerateCertTimerId = MAX_UINT32;
        }

        timerQueueFree(&pSampleConfiguration->timerQueueHandle);
    }

    if (pSampleConfiguration->pPendingSignalingMessageForRemoteClient != NULL) {
        // Iterate and free all the pending queues
        stackQueueGetIterator(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeMessageQueue((PPendingMessageQueue) data);
        }

        stackQueueClear(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, FALSE);
        stackQueueFree(pSampleConfiguration->pPendingSignalingMessageForRemoteClient);
        pSampleConfiguration->pPendingSignalingMessageForRemoteClient = NULL;
    }

    hashTableClear(pSampleConfiguration->pRtcPeerConnectionForRemoteClient);
    hashTableFree(pSampleConfiguration->pRtcPeerConnectionForRemoteClient);

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        locked = TRUE;
    }

    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        retStatus = gatherIceServerStats(pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection,
                                 pSampleConfiguration->iceUriCount);
        if (STATUS_FAILED(retStatus)) {
            DLOGW("Failed to ICE Server Stats for streaming session %d: %08x", i, retStatus);
        }
        freeSampleStreamingSession(&pSampleConfiguration->sampleStreamingSessionList[i]);
    }
    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }
    deinitKvsWebRtc();

    SAFE_MEMFREE(pSampleConfiguration->pVideoFrameBuffer);
    SAFE_MEMFREE(pSampleConfiguration->pAudioFrameBuffer);

    if (IS_VALID_CVAR_VALUE(pSampleConfiguration->cvar) && IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        CVAR_BROADCAST(pSampleConfiguration->cvar);
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->sampleConfigurationObjLock)) {
        MUTEX_FREE(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->streamingSessionListReadLock)) {
        MUTEX_FREE(pSampleConfiguration->streamingSessionListReadLock);
    }

    if (IS_VALID_MUTEX_VALUE(pSampleConfiguration->signalingSendMessageLock)) {
        MUTEX_FREE(pSampleConfiguration->signalingSendMessageLock);
    }

    if (IS_VALID_CVAR_VALUE(pSampleConfiguration->cvar)) {
        CVAR_FREE(pSampleConfiguration->cvar);
    }

    // AWS credential options are now handled by the signaling client

    if (pSampleConfiguration->pregeneratedCertificates != NULL) {
        stackQueueGetIterator(pSampleConfiguration->pregeneratedCertificates, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeRtcCertificate((PRtcCertificate) data);
        }

        CHK_LOG_ERR(stackQueueClear(pSampleConfiguration->pregeneratedCertificates, FALSE));
        CHK_LOG_ERR(stackQueueFree(pSampleConfiguration->pregeneratedCertificates));
        pSampleConfiguration->pregeneratedCertificates = NULL;
    }
    if (pSampleConfiguration->enableFileLogging) {
        freeFileLogger();
    }
    SAFE_MEMFREE(*ppSampleConfiguration);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS sessionCleanupWait(PSampleConfiguration pSampleConfiguration, BOOL isSignalingOnly)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL sampleConfigurationObjLockLocked = FALSE, streamingSessionListReadLockLocked = FALSE;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    UINT32 i, clientIdHash;
    BOOL sessionFreed = FALSE;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Get the signaling client state
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = TRUE;

        // Check for terminated streaming session
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->sampleStreamingSessionList[i]->terminateFlag)) {
                pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];

                // Remove from the hash table
                clientIdHash = COMPUTE_CRC32((PBYTE) pSampleStreamingSession->peerId, (UINT32) STRLEN(pSampleStreamingSession->peerId));
                CHK_STATUS(hashTableRemove(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash));

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = TRUE;

                // Remove from the array
                for (UINT32 j = i; j < pSampleConfiguration->streamingSessionCount - 1; ++j) {
                    pSampleConfiguration->sampleStreamingSessionList[j] = pSampleConfiguration->sampleStreamingSessionList[j + 1];
                }

                pSampleConfiguration->streamingSessionCount--;
                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = FALSE;

                CHK_LOG_ERR(freeSampleStreamingSession(&pSampleStreamingSession));
                sessionFreed = TRUE;

                // Quit the for loop as we have modified the collection and the for loop iterator
                break;
            }
        }

        // Check if we need to reconnect the signaling client for media storage
        if (sessionFreed &&
            pSampleConfiguration->channelInfo.useMediaStorage &&
            gWebRtcAppConfig.pSignalingClientInterface != NULL &&
            gSignalingClientData != NULL &&
            !ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient)) {
            // In the WebRTC Media Storage Ingestion Case the backend will terminate the session after
            // 1 hour. The SDK needs to reconnect to receive a new offer from the backend.
            CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->disconnect(gSignalingClientData));
            CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->connect(gSignalingClientData));
            sessionFreed = FALSE;
        }

        // Check if we need to re-create the signaling client on-the-fly
        if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient) &&
            gWebRtcAppConfig.pSignalingClientInterface != NULL &&
            gSignalingClientData != NULL) {

            DLOGI("Reconnecting signaling client");

            // Disconnect and reconnect
            CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->disconnect(gSignalingClientData));
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->connect(gSignalingClientData);

            if (STATUS_SUCCEEDED(retStatus)) {
                // Re-set the variable again
                ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
            } else {
                DLOGE("Failed to reconnect signaling client: 0x%08x", retStatus);
                // Reset status to avoid breaking the loop
                retStatus = STATUS_SUCCESS;
            }
        }

        if (!isSignalingOnly) {
            // Check if any lingering pending message queues
            CHK_STATUS(removeExpiredMessageQueues(pSampleConfiguration->pPendingSignalingMessageForRemoteClient));
        }
        // periodically wake up and clean up terminated streaming session
        CVAR_WAIT(pSampleConfiguration->cvar, pSampleConfiguration->sampleConfigurationObjLock, SAMPLE_SESSION_CLEANUP_WAIT_PERIOD);
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = FALSE;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (sampleConfigurationObjLockLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (streamingSessionListReadLockLocked) {
        MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
    }

    LEAVES();
    return retStatus;
}

STATUS submitPendingIceCandidate(PPendingMessageQueue pPendingMessageQueue, PSampleStreamingSession pSampleStreamingSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL noPendingSignalingMessageForClient = FALSE;
    PReceivedSignalingMessage pReceivedSignalingMessage = NULL;
    UINT64 hashValue;

    CHK(pPendingMessageQueue != NULL && pPendingMessageQueue->messageQueue != NULL && pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    do {
        CHK_STATUS(stackQueueIsEmpty(pPendingMessageQueue->messageQueue, &noPendingSignalingMessageForClient));
        if (!noPendingSignalingMessageForClient) {
            hashValue = 0;
            CHK_STATUS(stackQueueDequeue(pPendingMessageQueue->messageQueue, &hashValue));
            pReceivedSignalingMessage = (PReceivedSignalingMessage) hashValue;
            CHK(pReceivedSignalingMessage != NULL, STATUS_INTERNAL_ERROR);
            if (pReceivedSignalingMessage->signalingMessage.messageType == SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE) {
                CHK_STATUS(handleRemoteCandidate(pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
            }
            SAFE_MEMFREE(pReceivedSignalingMessage);
        }
    } while (!noPendingSignalingMessageForClient);

    CHK_STATUS(freeMessageQueue(pPendingMessageQueue));

CleanUp:

    SAFE_MEMFREE(pReceivedSignalingMessage);
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/**
 * @brief Wrapper function to adapt portable signaling interface to KVS SDK format
 *
 * This function converts esp_webrtc_signaling_message_t to PReceivedSignalingMessage
 * and calls the actual signalingMessageReceived function.
 */
static WEBRTC_STATUS signalingMessageReceivedWrapper(uint64_t customData, esp_webrtc_signaling_message_t* pWebRtcMessage)
{
    STATUS kvsStatus = STATUS_SUCCESS;

    if (pWebRtcMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // In split mode, forward messages from signaling server to bridge via callback
    // Convert directly from esp_webrtc_signaling_message_t to signaling_msg_t (no extra conversions)
    if (g_sendMessageCallback != NULL) {
        signaling_msg_t splitModeMessage = {0};

        ESP_LOGI(TAG, "Split mode: Forwarding message from signaling server to bridge");

        // Convert esp_webrtc_signaling_message_t to signaling_msg_t format for bridge
        splitModeMessage.version = pWebRtcMessage->version;

        switch (pWebRtcMessage->message_type) {
            case ESP_SIGNALING_MESSAGE_TYPE_OFFER:
                splitModeMessage.messageType = SIGNALING_MSG_TYPE_OFFER;
                break;
            case ESP_SIGNALING_MESSAGE_TYPE_ANSWER:
                splitModeMessage.messageType = SIGNALING_MSG_TYPE_ANSWER;
                break;
            case ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
                splitModeMessage.messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
                break;
            default:
                ESP_LOGW(TAG, "Unknown signaling message type: %d", pWebRtcMessage->message_type);
                return WEBRTC_STATUS_INVALID_ARG;
        }

        STRNCPY(splitModeMessage.correlationId, pWebRtcMessage->correlation_id, SS_MAX_CORRELATION_ID_LEN);
        splitModeMessage.correlationId[SS_MAX_CORRELATION_ID_LEN] = '\0';
        STRNCPY(splitModeMessage.peerClientId, pWebRtcMessage->peer_client_id, SS_MAX_SIGNALING_CLIENT_ID_LEN);
        splitModeMessage.peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
        splitModeMessage.payload = pWebRtcMessage->payload;
        splitModeMessage.payloadLen = pWebRtcMessage->payload_len;

        ESP_LOGI(TAG, "Forwarding message type %d from peer %s to bridge",
                 splitModeMessage.messageType, splitModeMessage.peerClientId);

        // Call the split mode callback to send to bridge
        int callbackResult = g_sendMessageCallback(&splitModeMessage);
        if (callbackResult != 0) {
            ESP_LOGE(TAG, "Split mode callback failed: %d", callbackResult);
            return WEBRTC_STATUS_INTERNAL_ERROR;
        }

        ESP_LOGI(TAG, "Successfully forwarded message to bridge");
        return WEBRTC_STATUS_SUCCESS; // Exit early in split mode - message forwarded to bridge
    }

    // Normal processing when not in split mode - convert to ReceivedSignalingMessage
    ReceivedSignalingMessage receivedMessage;

    // Initialize the received message structure
    memset(&receivedMessage, 0, sizeof(ReceivedSignalingMessage));

    // Convert message type
    switch (pWebRtcMessage->message_type) {
        case ESP_SIGNALING_MESSAGE_TYPE_OFFER:
            receivedMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ANSWER:
            receivedMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            receivedMessage.signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            DLOGE("Unknown message type: %d", pWebRtcMessage->message_type);
            return WEBRTC_STATUS_INVALID_ARG;
    }

    // Copy message data
    receivedMessage.signalingMessage.version = pWebRtcMessage->version;
    strncpy(receivedMessage.signalingMessage.correlationId, pWebRtcMessage->correlation_id, MAX_CORRELATION_ID_LEN);
    receivedMessage.signalingMessage.correlationId[MAX_CORRELATION_ID_LEN] = '\0';
    strncpy(receivedMessage.signalingMessage.peerClientId, pWebRtcMessage->peer_client_id, MAX_SIGNALING_CLIENT_ID_LEN);
    receivedMessage.signalingMessage.peerClientId[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';

    // Copy payload
    receivedMessage.signalingMessage.payloadLen = pWebRtcMessage->payload_len;
    if (pWebRtcMessage->payload != NULL && pWebRtcMessage->payload_len > 0) {
#ifdef DYNAMIC_SIGNALING_PAYLOAD
        receivedMessage.signalingMessage.payload = pWebRtcMessage->payload;
#else
        if (pWebRtcMessage->payload_len <= MAX_SIGNALING_MESSAGE_LEN) {
            memcpy(receivedMessage.signalingMessage.payload, pWebRtcMessage->payload, pWebRtcMessage->payload_len);
            receivedMessage.signalingMessage.payload[pWebRtcMessage->payload_len] = '\0';
        } else {
            DLOGE("Payload too large: %d > %d", pWebRtcMessage->payload_len, MAX_SIGNALING_MESSAGE_LEN);
            return WEBRTC_STATUS_INVALID_ARG;
        }
#endif
    }

    // Set successful status
    receivedMessage.statusCode = SERVICE_CALL_RESULT_OK;
    receivedMessage.errorType[0] = '\0';
    receivedMessage.description[0] = '\0';

    // Call the actual KVS signaling function
    kvsStatus = signalingMessageReceived((UINT64)customData, &receivedMessage);

    // Convert STATUS to WEBRTC_STATUS
    return (kvsStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

STATUS signalingMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Normal processing (split mode is handled in wrapper)
    BOOL peerConnectionFound = FALSE, locked = FALSE, startStats = FALSE, freeStreamingSession = FALSE;
    UINT32 clientIdHash;
    UINT64 hashValue = 0;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PReceivedSignalingMessage pReceivedSignalingMessageCopy = NULL;

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    clientIdHash = COMPUTE_CRC32((PBYTE) pReceivedSignalingMessage->signalingMessage.peerClientId,
                                 (UINT32) STRLEN(pReceivedSignalingMessage->signalingMessage.peerClientId));
    CHK_STATUS(hashTableContains(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));
    if (peerConnectionFound) {
        CHK_STATUS(hashTableGet(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &hashValue));
        pSampleStreamingSession = (PSampleStreamingSession) hashValue;
    }

    switch (pReceivedSignalingMessage->signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            // Check if we already have an ongoing master session with the same peer
            CHK_ERR(!peerConnectionFound, STATUS_INVALID_OPERATION, "Peer connection %s is in progress",
                    pReceivedSignalingMessage->signalingMessage.peerClientId);

            /*
             * Create new streaming session for each offer, then insert the client id and streaming session into
             * pRtcPeerConnectionForRemoteClient for subsequent ice candidate messages. Lastly check if there is
             * any ice candidate messages queued in pPendingSignalingMessageForRemoteClient. If so then submit
             * all of them.
             */

            if (pSampleConfiguration->streamingSessionCount == ARRAY_SIZE(pSampleConfiguration->sampleStreamingSessionList)) {
                DLOGW("Max simultaneous streaming session count reached.");

                // Need to remove the pending queue if any.
                // This is a simple optimization as the session cleanup will
                // handle the cleanup of pending message queue after a while
                CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                         &pPendingMessageQueue));

                CHK(FALSE, retStatus);
            }

            CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, pReceivedSignalingMessage->signalingMessage.peerClientId, TRUE,
                                                    &pSampleStreamingSession));

            freeStreamingSession = TRUE;
            CHK_STATUS(handleOffer(pSampleConfiguration, pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));

            CHK_STATUS(hashTablePut(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, (UINT64) pSampleStreamingSession));

            // If there are any ice candidate messages in the queue for this client id, submit them now.
            CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                     &pPendingMessageQueue));
            if (pPendingMessageQueue != NULL) {
                CHK_STATUS(submitPendingIceCandidate(pPendingMessageQueue, pSampleStreamingSession));

                // NULL the pointer to avoid it being freed in the cleanup
                pPendingMessageQueue = NULL;
            }

            MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
            pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;
            MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
            freeStreamingSession = FALSE;

            startStats = pSampleConfiguration->iceCandidatePairStatsTimerId == MAX_UINT32;
            break;

        case SIGNALING_MESSAGE_TYPE_ANSWER:
            /*
             * for viewer, pSampleStreamingSession should've already been created. insert the client id and
             * streaming session into pRtcPeerConnectionForRemoteClient for subsequent ice candidate messages.
             * Lastly check if there is any ice candidate messages queued in pPendingSignalingMessageForRemoteClient.
             * If so then submit all of them.
             */
            pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[0];
            CHK_STATUS(handleAnswer(pSampleConfiguration, pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
            CHK_STATUS(hashTablePut(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, (UINT64) pSampleStreamingSession));

            // If there are any ice candidate messages in the queue for this client id, submit them now.
            CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                     &pPendingMessageQueue));
            if (pPendingMessageQueue != NULL) {
                CHK_STATUS(submitPendingIceCandidate(pPendingMessageQueue, pSampleStreamingSession));

                // NULL the pointer to avoid it being freed in the cleanup
                pPendingMessageQueue = NULL;
            }

            startStats = pSampleConfiguration->iceCandidatePairStatsTimerId == MAX_UINT32;
            CHK_STATUS(signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &pSampleConfiguration->signalingClientMetrics));
            DLOGP("[Signaling offer sent to answer received time] %" PRIu64 " ms",
                  pSampleConfiguration->signalingClientMetrics.signalingClientStats.offerToAnswerTime);
            break;

        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            /*
             * if peer connection hasn't been created, create an queue to store the ice candidate message. Otherwise
             * submit the signaling message into the corresponding streaming session.
             */
            if (!peerConnectionFound) {
                CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, FALSE,
                                                         &pPendingMessageQueue));
                if (pPendingMessageQueue == NULL) {
                    CHK_STATUS(createMessageQueue(clientIdHash, &pPendingMessageQueue));
                    CHK_STATUS(stackQueueEnqueue(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, (UINT64) pPendingMessageQueue));
                }

                pReceivedSignalingMessageCopy = (PReceivedSignalingMessage) MEMCALLOC(1, SIZEOF(ReceivedSignalingMessage));
                CHK(pReceivedSignalingMessageCopy != NULL, STATUS_NOT_ENOUGH_MEMORY);

#ifdef DYNAMIC_SIGNALING_PAYLOAD
                // Copy all fields except for the payload via direct memory copy
                // This avoids having to manually copy each field
                UINT32 payloadPtr = (UINT32)pReceivedSignalingMessage->signalingMessage.payload;
                UINT32 payloadLen = pReceivedSignalingMessage->signalingMessage.payloadLen;

                // Use memcpy for the structure but preserve the payload pointer to avoid copying it
                memcpy(pReceivedSignalingMessageCopy, pReceivedSignalingMessage, SIZEOF(ReceivedSignalingMessage));

                // Set payload pointer to NULL before allocation to avoid freeing random memory
                pReceivedSignalingMessageCopy->signalingMessage.payload = NULL;

                // Allocate memory for the payload
                CHK_STATUS(allocateSignalingMessagePayload(&pReceivedSignalingMessageCopy->signalingMessage,
                                                         payloadLen + 1));

                // Copy the payload
                if (payloadLen > 0 && payloadPtr != 0) {
                    MEMCPY(pReceivedSignalingMessageCopy->signalingMessage.payload,
                           (PCHAR)payloadPtr,
                           payloadLen);

                    // Ensure null termination
                    pReceivedSignalingMessageCopy->signalingMessage.payload[payloadLen] = '\0';
                }
#else
                // Simple memcpy for fixed-size payload
                memcpy(pReceivedSignalingMessageCopy, pReceivedSignalingMessage, sizeof(ReceivedSignalingMessage));
#endif

                CHK_STATUS(stackQueueEnqueue(pPendingMessageQueue->messageQueue, (UINT64) pReceivedSignalingMessageCopy));

                // NULL the pointers to not free any longer
                pPendingMessageQueue = NULL;
                pReceivedSignalingMessageCopy = NULL;
            } else {
                CHK_STATUS(handleRemoteCandidate(pSampleStreamingSession, &pReceivedSignalingMessage->signalingMessage));
            }
            break;

        default:
            DLOGD("Unhandled signaling message type %u", pReceivedSignalingMessage->signalingMessage.messageType);
            break;
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    if (startStats &&
        STATUS_FAILED(retStatus = timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, SAMPLE_STATS_DURATION, SAMPLE_STATS_DURATION,
                                                     getIceCandidatePairStatsCallback, (UINT64) pSampleConfiguration,
                                                     &pSampleConfiguration->iceCandidatePairStatsTimerId))) {
        DLOGW("Failed to add getIceCandidatePairStatsCallback to add to timer queue (code 0x%08x). "
              "Cannot pull ice candidate pair metrics periodically",
              retStatus);

        // Reset the returned status
        retStatus = STATUS_SUCCESS;
    }

CleanUp:

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    if (pReceivedSignalingMessageCopy != NULL) {
        // Free the allocated payload before freeing the message itself
        freeSignalingMessagePayload(&pReceivedSignalingMessageCopy->signalingMessage);
    }
#endif
    SAFE_MEMFREE(pReceivedSignalingMessageCopy);

    if (pPendingMessageQueue != NULL) {
        freeMessageQueue(pPendingMessageQueue);
    }

    if (freeStreamingSession && pSampleStreamingSession != NULL) {
        freeSampleStreamingSession(&pSampleStreamingSession);
    }

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS createMessageQueue(UINT64 hashValue, PPendingMessageQueue* ppPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;

    CHK(ppPendingMessageQueue != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pPendingMessageQueue = (PPendingMessageQueue) MEMCALLOC(1, SIZEOF(PendingMessageQueue))), STATUS_NOT_ENOUGH_MEMORY);
    pPendingMessageQueue->hashValue = hashValue;
    pPendingMessageQueue->createTime = GETTIME();
    CHK_STATUS(stackQueueCreate(&pPendingMessageQueue->messageQueue));

CleanUp:

    if (STATUS_FAILED(retStatus) && pPendingMessageQueue != NULL) {
        freeMessageQueue(pPendingMessageQueue);
        pPendingMessageQueue = NULL;
    }

    if (ppPendingMessageQueue != NULL) {
        *ppPendingMessageQueue = pPendingMessageQueue;
    }

    return retStatus;
}

STATUS freeMessageQueue(PPendingMessageQueue pPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;

    // free is idempotent
    CHK(pPendingMessageQueue != NULL, retStatus);

    if (pPendingMessageQueue->messageQueue != NULL) {
        stackQueueClear(pPendingMessageQueue->messageQueue, TRUE);
        stackQueueFree(pPendingMessageQueue->messageQueue);
    }

    MEMFREE(pPendingMessageQueue);

CleanUp:
    return retStatus;
}

STATUS getPendingMessageQueueForHash(PStackQueue pPendingQueue, UINT64 clientHash, BOOL remove, PPendingMessageQueue* ppPendingMessageQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    StackQueueIterator iterator;
    BOOL iterate = TRUE;
    UINT64 data;

    CHK(pPendingQueue != NULL && ppPendingMessageQueue != NULL, STATUS_NULL_ARG);

    CHK_STATUS(stackQueueGetIterator(pPendingQueue, &iterator));
    while (iterate && IS_VALID_ITERATOR(iterator)) {
        CHK_STATUS(stackQueueIteratorGetItem(iterator, &data));
        CHK_STATUS(stackQueueIteratorNext(&iterator));

        pPendingMessageQueue = (PPendingMessageQueue) data;

        if (clientHash == pPendingMessageQueue->hashValue) {
            *ppPendingMessageQueue = pPendingMessageQueue;
            iterate = FALSE;

            // Check if the item needs to be removed
            if (remove) {
                // This is OK to do as we are terminating the iterator anyway
                CHK_STATUS(stackQueueRemoveItem(pPendingQueue, data));
            }
        }
    }

CleanUp:

    return retStatus;
}

STATUS removeExpiredMessageQueues(PStackQueue pPendingQueue)
{
    STATUS retStatus = STATUS_SUCCESS;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    UINT32 i, count;
    UINT64 data, curTime;

    CHK(pPendingQueue != NULL, STATUS_NULL_ARG);

    curTime = GETTIME();
    CHK_STATUS(stackQueueGetCount(pPendingQueue, &count));

    // Dequeue and enqueue in order to not break the iterator while removing an item
    for (i = 0; i < count; i++) {
        CHK_STATUS(stackQueueDequeue(pPendingQueue, &data));

        // Check for expiry
        pPendingMessageQueue = (PPendingMessageQueue) data;
        if (pPendingMessageQueue->createTime + SAMPLE_PENDING_MESSAGE_CLEANUP_DURATION < curTime) {
            // Message queue has expired and needs to be freed
            CHK_STATUS(freeMessageQueue(pPendingMessageQueue));
        } else {
            // Enqueue back again as it's still valued
            CHK_STATUS(stackQueueEnqueue(pPendingQueue, data));
        }
    }

CleanUp:

    return retStatus;
}

#ifdef ENABLE_DATA_CHANNEL
VOID onDataChannelMessage(UINT64 customData, PRtcDataChannel pDataChannel, BOOL isBinary, PBYTE pMessage, UINT32 pMessageLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, strLen, tokenCount;
    CHAR pMessageSend[MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE], errorMessage[200];
    PCHAR json;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) customData;
    PSampleConfiguration pSampleConfiguration;
    DataChannelMessage dataChannelMessage;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];

    CHK(pMessage != NULL && pDataChannel != NULL, STATUS_NULL_ARG);

    if (pSampleStreamingSession == NULL) {
        STRCPY(errorMessage, "Could not generate stats since the streaming session is NULL");
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        DLOGE("%s", errorMessage);
        goto CleanUp;
    }

    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL) {
        STRCPY(errorMessage, "Could not generate stats since the sample configuration is NULL");
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        DLOGE("%s", errorMessage);
        goto CleanUp;
    }

    if (pSampleConfiguration->enableSendingMetricsToViewerViaDc) {
        jsmn_init(&parser);
        json = (PCHAR) pMessage;
        tokenCount = jsmn_parse(&parser, json, STRLEN(json), tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));

        MEMSET(dataChannelMessage.content, '\0', SIZEOF(dataChannelMessage.content));
        MEMSET(dataChannelMessage.firstMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.firstMessageFromViewerTs));
        MEMSET(dataChannelMessage.firstMessageFromMasterTs, '\0', SIZEOF(dataChannelMessage.firstMessageFromMasterTs));
        MEMSET(dataChannelMessage.secondMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.secondMessageFromViewerTs));
        MEMSET(dataChannelMessage.secondMessageFromMasterTs, '\0', SIZEOF(dataChannelMessage.secondMessageFromMasterTs));
        MEMSET(dataChannelMessage.lastMessageFromViewerTs, '\0', SIZEOF(dataChannelMessage.lastMessageFromViewerTs));

        if (tokenCount > 1) {
            if (tokens[0].type != JSMN_OBJECT) {
                STRCPY(errorMessage, "Invalid JSON received, please send a valid json as the SDK is operating in datachannel-benchmarking mode");
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
                DLOGE("%s", errorMessage);
                retStatus = STATUS_INVALID_API_CALL_RETURN_JSON;
                goto CleanUp;
            }
            DLOGI("DataChannel json message: %.*s\n", pMessageLen, pMessage);

            for (i = 1; i < tokenCount; i++) {
                if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "content")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.content, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "firstMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    // parse and retain this message from the viewer to send it back again
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.firstMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "firstMessageFromMasterTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.firstMessageFromMasterTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    } else {
                        // if this timestamp was not assigned during the previous message session, add it now
                        SNPRINTF(dataChannelMessage.firstMessageFromMasterTs, 20, "%llu", GETTIME() / 10000);
                        break;
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "secondMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    // parse and retain this message from the viewer to send it back again
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.secondMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "secondMessageFromMasterTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        // since the length is not zero, we have already attached this timestamp to structure in the last iteration
                        STRNCPY(dataChannelMessage.secondMessageFromMasterTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    } else {
                        // if this timestamp was not assigned during the previous message session, add it now
                        SNPRINTF(dataChannelMessage.secondMessageFromMasterTs, 20, "%llu", GETTIME() / 10000);
                        break;
                    }
                } else if (compareJsonString(json, &tokens[i], JSMN_STRING, (PCHAR) "lastMessageFromViewerTs")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    if (strLen != 0) {
                        STRNCPY(dataChannelMessage.lastMessageFromViewerTs, json + tokens[i + 1].start, tokens[i + 1].end - tokens[i + 1].start);
                    }
                }
            }

            if (STRLEN(dataChannelMessage.lastMessageFromViewerTs) == 0) {
                // continue sending the data_channel_metrics_message with new timestamps until we receive the lastMessageFromViewerTs from the viewer
                SNPRINTF(pMessageSend, MAX_DATA_CHANNEL_METRICS_MESSAGE_SIZE, DATA_CHANNEL_MESSAGE_TEMPLATE, MASTER_DATA_CHANNEL_MESSAGE,
                         dataChannelMessage.firstMessageFromViewerTs, dataChannelMessage.firstMessageFromMasterTs,
                         dataChannelMessage.secondMessageFromViewerTs, dataChannelMessage.secondMessageFromMasterTs,
                         dataChannelMessage.lastMessageFromViewerTs);
                DLOGI("Master's response: %s", pMessageSend);

                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pMessageSend, STRLEN(pMessageSend));
            } else {
                // now that we've received the last message, send across the signaling, peerConnection, ice metrics
                SNPRINTF(pSampleStreamingSession->pSignalingClientMetricsMessage, MAX_SIGNALING_CLIENT_METRICS_MESSAGE_SIZE,
                         SIGNALING_CLIENT_METRICS_JSON_TEMPLATE, pSampleConfiguration->signalingClientMetrics.signalingStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.offerReceivedTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.answerTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.describeChannelStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.describeChannelEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getSignalingChannelEndpointStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getSignalingChannelEndpointEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getIceServerConfigStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getIceServerConfigEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getTokenStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.getTokenEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.createChannelStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.createChannelEndTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.connectStartTime,
                         pSampleConfiguration->signalingClientMetrics.signalingClientStats.connectEndTime);
                DLOGI("Sending signaling metrics to the viewer: %s", pSampleStreamingSession->pSignalingClientMetricsMessage);

                CHK_STATUS(peerConnectionGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->peerConnectionMetrics));
                SNPRINTF(pSampleStreamingSession->pPeerConnectionMetricsMessage, MAX_PEER_CONNECTION_METRICS_MESSAGE_SIZE,
                         PEER_CONNECTION_METRICS_JSON_TEMPLATE,
                         pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionStartTime,
                         pSampleStreamingSession->peerConnectionMetrics.peerConnectionStats.peerConnectionConnectedTime);
                DLOGI("Sending peer-connection metrics to the viewer: %s", pSampleStreamingSession->pPeerConnectionMetricsMessage);

                CHK_STATUS(iceAgentGetMetrics(pSampleStreamingSession->pPeerConnection, &pSampleStreamingSession->iceMetrics));
                SNPRINTF(pSampleStreamingSession->pIceAgentMetricsMessage, MAX_ICE_AGENT_METRICS_MESSAGE_SIZE, ICE_AGENT_METRICS_JSON_TEMPLATE,
                         pSampleStreamingSession->iceMetrics.kvsIceAgentStats.candidateGatheringStartTime,
                         pSampleStreamingSession->iceMetrics.kvsIceAgentStats.candidateGatheringEndTime);
                DLOGI("Sending ice-agent metrics to the viewer: %s", pSampleStreamingSession->pIceAgentMetricsMessage);

                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pSignalingClientMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pSignalingClientMetricsMessage));
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pPeerConnectionMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pPeerConnectionMetricsMessage));
                retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pSampleStreamingSession->pIceAgentMetricsMessage,
                                            STRLEN(pSampleStreamingSession->pIceAgentMetricsMessage));
            }
        } else {
            DLOGI("DataChannel string message: %.*s\n", pMessageLen, pMessage);
            STRCPY(errorMessage, "Send a json message for benchmarking as the C SDK is operating in benchmarking mode");
            retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) errorMessage, STRLEN(errorMessage));
        }
    } else {
        if (isBinary) {
            DLOGI("DataChannel Binary Message");
        } else {
            DLOGI("DataChannel String Message: %.*s\n", pMessageLen, pMessage);
        }
        // Send Echo message to the viewer
        retStatus = dataChannelSend(pDataChannel, FALSE, (PBYTE) pMessage, pMessageLen);
    }
    if (retStatus != STATUS_SUCCESS) {
        DLOGI("[KVS Master] dataChannelSend(): operation returned status code: 0x%08x \n", retStatus);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
}

VOID onDataChannel(UINT64 customData, PRtcDataChannel pRtcDataChannel)
{
    DLOGI("New DataChannel has been opened %s \n", pRtcDataChannel->name);
    dataChannelOnMessage(pRtcDataChannel, customData, onDataChannelMessage);
}
#endif

/* WebRTC App API Implementation */

#ifndef CONFIG_USE_ESP_WEBSOCKET_CLIENT
static void *realloc_wrapper(void *ptr, size_t size, const char *reason)
{
    (void) reason;
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

extern void lws_set_allocator(void *(*realloc)(void *ptr, size_t size, const char *reason));
#endif


/**
 * @brief Initialize WebRTC application with the given configuration
 */
STATUS webrtcAppInit(PWebRtcAppConfig pConfig)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(pConfig != NULL, STATUS_NULL_ARG);
    CHK(!gWebRtcAppInitialized, STATUS_INVALID_OPERATION);

    // Initialize flash wrapper first
    retStatus = flash_wrapper_init();
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to initialize flash wrapper");
        goto CleanUp;
    }

    // Store the config in our global structure
    MEMCPY(&gWebRtcAppConfig, pConfig, SIZEOF(WebRtcAppConfig));

    // Initialize KVS WebRTC library - this must be done before any other WebRTC operations
    if (!pConfig->signalingOnly) {
        CHK_STATUS(initKvsWebRtc());
        DLOGI("KVS WebRTC library initialized successfully");
    } else {
        DLOGI("Skipping KVS WebRTC library initialization for signaling-only mode");
    }

    // Initialize the event callback mutex if not already done
    if (!IS_VALID_MUTEX_VALUE(gEventCallbackLock)) {
        gEventCallbackLock = MUTEX_CREATE(FALSE);
    }

    // Create the sample configuration
    CHK_STATUS(createSampleConfiguration(NULL, pConfig->roleType,
                                         pConfig->trickleIce, pConfig->useTurn,
                                         pConfig->logLevel, pConfig->signalingOnly, &pSampleConfiguration));

    // Store the sample configuration for later use
    gSampleConfiguration = pSampleConfiguration;

    pSampleConfiguration->mediaType = pConfig->mediaType;

    // Set the audio and video codecs
    pSampleConfiguration->audioCodec = pConfig->audioCodec;
    pSampleConfiguration->videoCodec = pConfig->videoCodec;

    // Configure media capture interfaces if provided
    if (pConfig->videoCapture != NULL) {
        pSampleConfiguration->videoCapture = pConfig->videoCapture;
    }

    if (pConfig->audioCapture != NULL) {
        pSampleConfiguration->audioCapture = pConfig->audioCapture;
    }

    // Configure media player interfaces if provided
    if (pConfig->videoPlayer != NULL) {
        pSampleConfiguration->videoPlayer = pConfig->videoPlayer;
    }

    if (pConfig->audioPlayer != NULL) {
        pSampleConfiguration->audioPlayer = pConfig->audioPlayer;
    }

    // Configure media reception
    pSampleConfiguration->receiveMedia = pConfig->receiveMedia;

    // Set the WebRTC app as initialized
    gWebRtcAppInitialized = TRUE;

    // Raise the initialized event
    raiseEvent(APP_WEBRTC_EVENT_INITIALIZED, STATUS_SUCCESS, NULL, "WebRTC app initialized successfully");

    DLOGI("WebRTC app initialized successfully");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to initialize WebRTC app with status 0x%08x", retStatus);
        if (pSampleConfiguration != NULL) {
            freeSampleConfiguration(&pSampleConfiguration);
        }
    }

    LEAVES();
    return retStatus;
}

/**
 * @brief Task function to run the WebRTC application
 * This task handles the WebRTC application main loop
 */
static void webrtcAppRunTask(void *pvParameters)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    DLOGI("Running WebRTC application in task");

    // Initialize signaling client using the provided interface and config
    if (gWebRtcAppConfig.pSignalingClientInterface != NULL && gWebRtcAppConfig.pSignalingConfig != NULL) {
        DLOGI("Initializing signaling client using interface");

        // Initialize the signaling client using the interface and opaque config
        retStatus = gWebRtcAppConfig.pSignalingClientInterface->init(
            gWebRtcAppConfig.pSignalingConfig,
            &gSignalingClientData);

        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to initialize signaling client: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }

        DLOGI("Signaling client initialized successfully");

        // Set the role type for the signaling client
        if (gWebRtcAppConfig.pSignalingClientInterface->setRoleType != NULL) {
            DLOGI("Setting signaling client role type: %d", gWebRtcAppConfig.roleType);
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->setRoleType(
                gSignalingClientData,
                gWebRtcAppConfig.roleType);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to set signaling client role type: 0x%08x", retStatus);
                CHK(FALSE, retStatus);
            }
        }

        // Set up callbacks
        if (gWebRtcAppConfig.pSignalingClientInterface->setCallbacks != NULL) {
            DLOGI("Setting up signaling callbacks");
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->setCallbacks(
                gSignalingClientData,
                (uint64_t) gSampleConfiguration,
                signalingMessageReceivedWrapper,
                signalingClientStateChanged,
                signalingClientError);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to set signaling callbacks: 0x%08x", retStatus);
                CHK(FALSE, retStatus);
            }
        }

        // Connect the signaling client
        if (gWebRtcAppConfig.pSignalingClientInterface->connect != NULL) {
            DLOGI("Connecting signaling client");
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->connect(gSignalingClientData);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to connect signaling client: 0x%08x", retStatus);
                CHK(FALSE, retStatus);
            }

            DLOGI("Signaling client connected successfully");
        }
    } else {
        DLOGI("No signaling client interface or config provided, running in streaming-only mode");
    }

    // Wait for termination
    sessionCleanupWait(gSampleConfiguration, gWebRtcAppConfig.signalingOnly);
    DLOGI("WebRTC app terminated");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app run failed with status 0x%08x", retStatus);
        raiseEvent(APP_WEBRTC_EVENT_ERROR, retStatus, NULL, "WebRTC application run failed");
    }

    // Terminate WebRTC application
    webrtcAppTerminate();
    DLOGI("WebRTC task cleanup done");

    // Clear the task handle since we're about to delete the task
    gWebRtcRunTaskHandle = NULL;

    // Delete the task
    vTaskDelete(NULL);

    LEAVES();
}

/**
 * @brief Run the WebRTC application and wait for termination
 */
STATUS webrtcAppRun(VOID)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gWebRtcAppInitialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    DLOGI("WebRTC app running");

    // Check if task is already running
    if (gWebRtcRunTaskHandle != NULL) {
        DLOGI("WebRTC task is already running");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    // Create a task to run the WebRTC application
    // Use a higher stack size for the WebRTC task as signaling requires substantial stack
    DLOGI("Creating WebRTC run task");
    int result = xTaskCreate(
        webrtcAppRunTask,
        "webrtc_run",
        (16 * 1024),  // 16KB stack size for credential provider and signaling
        NULL,
        5,
        &gWebRtcRunTaskHandle
    );

    if (result != pdPASS) {
        DLOGE("Failed to create WebRTC run task");
        CHK(FALSE, STATUS_OPERATION_TIMED_OUT);
    } else {
        DLOGI("WebRTC run task created successfully");
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app run failed with status 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

/**
 * @brief Get the sample configuration
 */
STATUS webrtcAppGetSampleConfiguration(PSampleConfiguration *ppSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);
    CHK(gWebRtcAppInitialized, STATUS_INVALID_OPERATION);

    *ppSampleConfiguration = gSampleConfiguration;

CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * @brief Terminate the WebRTC application
 */
STATUS webrtcAppTerminate(VOID)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gWebRtcAppInitialized, STATUS_INVALID_OPERATION);

    if (gSampleConfiguration != NULL) {
        // Kick off the termination sequence
        ATOMIC_STORE_BOOL(&gSampleConfiguration->appTerminateFlag, TRUE);

        // Wait for media thread to terminate
        if (gSampleConfiguration->mediaSenderTid != INVALID_TID_VALUE) {
            THREAD_JOIN(gSampleConfiguration->mediaSenderTid, NULL);
        }

        // Disconnect signaling client if available
        if (gWebRtcAppConfig.pSignalingClientInterface != NULL &&
            gSignalingClientData != NULL &&
            gWebRtcAppConfig.pSignalingClientInterface->disconnect != NULL) {

            DLOGI("Disconnecting signaling client");
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->disconnect(gSignalingClientData);

            if (STATUS_FAILED(retStatus)) {
                DLOGW("Failed to disconnect signaling client: 0x%08x", retStatus);
                // Continue with termination even if disconnect fails
                retStatus = STATUS_SUCCESS;
            } else {
                DLOGI("Signaling client disconnected successfully");
            }
        }

        // Free signaling client if available
        if (gWebRtcAppConfig.pSignalingClientInterface != NULL &&
            gSignalingClientData != NULL &&
            gWebRtcAppConfig.pSignalingClientInterface->free != NULL) {

            DLOGI("Freeing signaling client");
            retStatus = gWebRtcAppConfig.pSignalingClientInterface->free(gSignalingClientData);

            if (STATUS_FAILED(retStatus)) {
                DLOGW("Failed to free signaling client: 0x%08x", retStatus);
                // Continue with termination even if free fails
                retStatus = STATUS_SUCCESS;
            } else {
                DLOGI("Signaling client freed successfully");
            }
        }

        // Free sample configuration
        freeSampleConfiguration(&gSampleConfiguration);
    }

    // Reset state
    gWebRtcAppInitialized = FALSE;
    gSignalingClientData = NULL;
    MEMSET(&gWebRtcAppConfig, 0, SIZEOF(WebRtcAppConfig));

    DLOGI("WebRTC app terminated successfully");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app termination failed with status 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

// Add these new functions after webrtcAppGetSampleConfiguration

/**
 * @brief Send a video frame to all connected peers
 *
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @param is_key_frame Whether this is a key frame
 * @return STATUS code of the execution
 */
STATUS webrtcAppSendVideoFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp, BOOL is_key_frame)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    Frame frame = {0};
    UINT32 i;

    CHK(frame_data != NULL, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    pSampleConfiguration = gSampleConfiguration;

    frame.version = FRAME_CURRENT_VERSION;
    frame.frameData = frame_data;
    frame.size = frame_size;
    frame.trackId = DEFAULT_VIDEO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = timestamp;
    frame.decodingTs = timestamp;

    // Set key frame flag if needed
    if (is_key_frame) {
        frame.flags = FRAME_FLAG_KEY_FRAME;
    }

    MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        // Check if the peer connection is connected by checking if it's not NULL
        if (pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection != NULL) {
            retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pVideoRtcRtpTransceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for video failed with 0x%08" PRIx32 , retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                vTaskDelay(pdMS_TO_TICKS(100));
                retStatus = STATUS_SUCCESS;
            }
        }
    }
    MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

CleanUp:
    return retStatus;
}

/**
 * @brief Send an audio frame to all connected peers
 *
 * @param frame_data Pointer to frame data
 * @param frame_size Size of the frame in bytes
 * @param timestamp Presentation timestamp
 * @return STATUS code of the execution
 */
STATUS webrtcAppSendAudioFrame(PBYTE frame_data, UINT32 frame_size, UINT64 timestamp)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    Frame frame = {0};
    UINT32 i;

    CHK(frame_data != NULL, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    pSampleConfiguration = gSampleConfiguration;

    frame.version = FRAME_CURRENT_VERSION;
    frame.frameData = frame_data;
    frame.size = frame_size;
    frame.trackId = DEFAULT_AUDIO_TRACK_ID;
    frame.duration = 0;
    frame.index = 0;
    frame.presentationTs = timestamp;
    frame.decodingTs = timestamp;

    MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
        // Check if the peer connection is connected by checking if it's not NULL
        if (pSampleConfiguration->sampleStreamingSessionList[i]->pPeerConnection != NULL) {
            retStatus = writeFrame(pSampleConfiguration->sampleStreamingSessionList[i]->pAudioRtcRtpTransceiver, &frame);
            if (STATUS_FAILED(retStatus)) {
                ESP_LOGW(TAG, "writeFrame for audio failed with 0x%08" PRIx32 , retStatus);
            }
            if (retStatus == STATUS_SRTP_NOT_READY_YET) {
                vTaskDelay(pdMS_TO_TICKS(100));
                retStatus = STATUS_SUCCESS;
            }
        }
    }
    MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);

CleanUp:
    return retStatus;
}

// Add these new media thread functions

/**
 * @brief Thread function to send video frames from camera
 */
PVOID sendVideoFramesFromCamera(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    media_stream_video_capture_t *video_capture = NULL;
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;
    const uint32_t fps = 30;
    // Use precise frame duration to avoid timing drift
    const UINT64 frame_duration_100ns = HUNDREDS_OF_NANOS_IN_A_SECOND / fps; // Precise: 333,333.33 * 100ns units

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the video capture interface from the sample configuration
    video_capture = (media_stream_video_capture_t*) pSampleConfiguration->videoCapture;
    CHK(video_capture != NULL, STATUS_INVALID_ARG);

    // Initialize video capture with H264 configuration
    video_capture_config_t video_config = {
        .codec = VIDEO_CODEC_H264,
        .resolution = {
            .width = 640,
            .height = 480,
            .fps = fps
        },
        .quality = 80,
        .bitrate = 500, // 500 kbps
        .codec_specific = NULL
    };

    // Initialize video capture
    ESP_LOGI(TAG, "Initializing video capture");
    CHK(video_capture->init(&video_config, &video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start video capture
    CHK(video_capture->start(video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        // Get video frame from camera
        if (video_capture->get_frame(video_handle, &video_frame, 0) == ESP_OK) {
            if (video_frame != NULL && video_frame->len > 0) {
                // Send the frame with precise timestamp
                retStatus = webrtcAppSendVideoFrame(
                    video_frame->buffer,
                    video_frame->len,
                    currentTime - refTime,  // Precise relative timestamp
                    video_frame->type == VIDEO_FRAME_TYPE_I
                );

                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGE(TAG, "Failed to send video frame: 0x%08" PRIx32, retStatus);
                }

                // Release the frame when done
                video_capture->release_frame(video_handle, video_frame);
                video_frame = NULL;
            }
        }

        // Schedule next frame with precise timing
        nextFrameTime += frame_duration_100ns;

        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= GETTIME()) {
            // ESP_LOGW(TAG, "Frame pacing falling behind, resetting to current time");
            nextFrameTime = GETTIME() + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (video_handle != NULL && video_capture != NULL) {
        video_capture->stop(video_handle);
        video_capture->deinit(video_handle);
    }

    ESP_LOGI(TAG, "Video frame sending thread exiting");
    return (PVOID) ((uintptr_t) retStatus);
}

/**
 * @brief Thread function to send audio frames from microphone
 */
PVOID sendAudioFramesFromMic(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;
    const uint32_t sample_rate = 48000; // 48kHz sample rate
    const uint32_t frame_size_ms = 20;   // 20ms audio frames
    // Precise frame duration for 20ms audio frames
    const UINT64 frame_duration_100ns = frame_size_ms * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the audio capture interface from the sample configuration
    audio_capture = (media_stream_audio_capture_t*) pSampleConfiguration->audioCapture;
    CHK(audio_capture != NULL, STATUS_INVALID_ARG);

    // Initialize audio capture configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = sample_rate,
            .channels = 1, // Mono
            .bits_per_sample = 16
        },
        .bitrate = 64, // 64 kbps
        .frame_duration_ms = frame_size_ms,
        .codec_specific = NULL
    };

    // Initialize audio capture
    ESP_LOGI(TAG, "Initializing audio capture");
    CHK(audio_capture->init(&audio_config, &audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start audio capture
    CHK(audio_capture->start(audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        // Get audio frame from microphone
        if (audio_capture->get_frame(audio_handle, &audio_frame, 0) == ESP_OK) {
            if (audio_frame != NULL && audio_frame->len > 0) {
                // Send the frame with precise timestamp
                retStatus = webrtcAppSendAudioFrame(
                    audio_frame->buffer,
                    audio_frame->len,
                    currentTime - refTime  // Precise relative timestamp
                );

                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGE(TAG, "Failed to send audio frame: 0x%08" PRIx32, retStatus);
                }

                // Release the frame when done
                audio_capture->release_frame(audio_handle, audio_frame);
                audio_frame = NULL;
            }
        }

        // Schedule next frame with precise timing
        nextFrameTime += frame_duration_100ns;

        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= GETTIME()) {
            // ESP_LOGW(TAG, "Audio frame pacing falling behind, resetting to current time");
            nextFrameTime = GETTIME() + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (audio_handle != NULL && audio_capture != NULL) {
        audio_capture->stop(audio_handle);
        audio_capture->deinit(audio_handle);
    }

    ESP_LOGI(TAG, "Audio frame sending thread exiting");
    return (PVOID) ((uintptr_t) retStatus);
}

/**
 * @brief Thread function to send video frames from sample files
 */
PVOID sendVideoFramesFromSamples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    media_stream_video_capture_t *video_capture = NULL;
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;
    const uint32_t fps = 30;
    // Use precise frame duration to avoid timing drift
    const UINT64 frame_duration_100ns = HUNDREDS_OF_NANOS_IN_A_SECOND / fps; // Precise: 333,333.33 * 100ns units

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the video capture interface from the sample configuration
    video_capture = (media_stream_video_capture_t*) pSampleConfiguration->videoCapture;
    CHK(video_capture != NULL, STATUS_INVALID_ARG);

    // Initialize with sample video configuration
    video_capture_config_t video_config = {
        .codec = VIDEO_CODEC_H264,
        .resolution = {
            .width = 640,
            .height = 480,
            .fps = fps
        },
        .quality = 80,
        .bitrate = 500, // 500 kbps
        .codec_specific = NULL
    };

    // Initialize video capture for samples
    ESP_LOGI(TAG, "Initializing sample video capture");
    CHK(video_capture->init(&video_config, &video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start video capture
    CHK(video_capture->start(video_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        // Get video frame from capture interface
        if (video_capture->get_frame(video_handle, &video_frame, 0) == ESP_OK) {
            if (video_frame != NULL && video_frame->len > 0) {
                // Send the frame with precise timestamp
                retStatus = webrtcAppSendVideoFrame(
                    video_frame->buffer,
                    video_frame->len,
                    currentTime - refTime,  // Precise relative timestamp
                    video_frame->type == VIDEO_FRAME_TYPE_I
                );

                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGE(TAG, "Failed to send video frame: 0x%08" PRIx32, retStatus);
                }

                // Release the frame when done
                video_capture->release_frame(video_handle, video_frame);
                video_frame = NULL;
            }
        } else {
            ESP_LOGW(TAG, "Failed to get sample video frame");
        }

        // Schedule next frame with precise timing
        nextFrameTime += frame_duration_100ns;

        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= GETTIME()) {
            // ESP_LOGW(TAG, "Sample video frame pacing falling behind, resetting to current time");
            nextFrameTime = GETTIME() + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (video_handle != NULL && video_capture != NULL) {
        video_capture->stop(video_handle);
        video_capture->deinit(video_handle);
    }

    ESP_LOGI(TAG, "Sample video frame sending thread exiting");
    return (PVOID) ((uintptr_t) retStatus);
}

/**
 * @brief Thread function to send audio frames from sample files
 */
PVOID sendAudioFramesFromSamples(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) args;
    UINT64 refTime = GETTIME();
    UINT64 nextFrameTime = refTime;
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;
    const uint32_t sample_rate = 48000; // 48kHz sample rate
    const uint32_t frame_size_ms = 20;   // 20ms audio frames
    // Precise frame duration for 20ms audio frames
    const UINT64 frame_duration_100ns = frame_size_ms * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the audio capture interface from the sample configuration
    audio_capture = (media_stream_audio_capture_t*) pSampleConfiguration->audioCapture;
    CHK(audio_capture != NULL, STATUS_INVALID_ARG);

    // Initialize with sample audio configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = sample_rate,
            .channels = 1, // Mono
            .bits_per_sample = 16
        },
        .bitrate = 64, // 64 kbps
        .frame_duration_ms = frame_size_ms,
        .codec_specific = NULL
    };

    // Initialize audio capture for samples
    ESP_LOGI(TAG, "Initializing sample audio capture");
    CHK(audio_capture->init(&audio_config, &audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start audio capture
    CHK(audio_capture->start(audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Sleep until next frame time for consistent pacing
        if (currentTime < nextFrameTime) {
            THREAD_SLEEP(nextFrameTime - currentTime);
            currentTime = nextFrameTime; // Use scheduled time for timestamp consistency
        }

        // Get audio frame from capture interface
        if (audio_capture->get_frame(audio_handle, &audio_frame, 0) == ESP_OK) {
            if (audio_frame != NULL && audio_frame->len > 0) {
                // Send the frame with precise timestamp
                retStatus = webrtcAppSendAudioFrame(
                    audio_frame->buffer,
                    audio_frame->len,
                    currentTime - refTime  // Precise relative timestamp
                );

                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGE(TAG, "Failed to send audio frame: 0x%08" PRIx32, retStatus);
                }

                // Release the frame when done
                audio_capture->release_frame(audio_handle, audio_frame);
                audio_frame = NULL;
            }
        } else {
            ESP_LOGW(TAG, "Failed to get sample audio frame");
        }

        // Schedule next frame with precise timing
        nextFrameTime += frame_duration_100ns;

        // Handle case where we've fallen behind schedule
        if (nextFrameTime <= GETTIME()) {
            // ESP_LOGW(TAG, "Sample audio frame pacing falling behind, resetting to current time");
            nextFrameTime = GETTIME() + frame_duration_100ns / 2;
        }
    }

CleanUp:
    if (audio_handle != NULL && audio_capture != NULL) {
        audio_capture->stop(audio_handle);
        audio_capture->deinit(audio_handle);
    }

    ESP_LOGI(TAG, "Sample audio frame sending thread exiting");
    return (PVOID) ((uintptr_t) retStatus);
}

/**
 * @brief Receiver for audio/video frames
 */
PVOID sampleReceiveAudioVideoFrame(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession) args;

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    ESP_LOGI(TAG, "Setting up media reception callbacks");

    // Get the sample configuration
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    CHK(pSampleConfiguration != NULL, STATUS_INTERNAL_ERROR);

    // Lock for player initialization
    MUTEX_LOCK(pSampleConfiguration->playerLock);

    // Initialize video player if available and not already initialized
    if (pSampleConfiguration->videoPlayer != NULL && pSampleConfiguration->video_player_handle == NULL) {
        media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->videoPlayer;

        // Initialize with default configuration
        video_player_config_t video_config = {
            .codec = VIDEO_PLAYER_CODEC_H264,
            .format = {
                .width = 640,
                .height = 480,
                .framerate = 30
            },
            .buffer_frames = 5,
            .codec_specific = NULL,
            .display_handle = NULL
        };

        ESP_LOGI(TAG, "Initializing video player");
        if (video_player->init(&video_config, &pSampleConfiguration->video_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video player");
        } else if (video_player->start(pSampleConfiguration->video_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start video player");
        } else {
            ESP_LOGI(TAG, "Video player initialized successfully");
        }
    }

    // Initialize audio player if available and not already initialized
    if (pSampleConfiguration->audioPlayer != NULL && pSampleConfiguration->audio_player_handle == NULL) {
        media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audioPlayer;

        // Initialize with default configuration
        audio_player_config_t audio_config = {
            .codec = AUDIO_PLAYER_CODEC_OPUS,
            .format = {
                .sample_rate = 48000,
                .channels = 1,
                .bits_per_sample = 16
            },
            .buffer_ms = 500,
            .codec_specific = NULL
        };

        ESP_LOGI(TAG, "Initializing audio player");
        if (audio_player->init(&audio_config, &pSampleConfiguration->audio_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize audio player");
        } else if (audio_player->start(pSampleConfiguration->audio_player_handle) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start audio player");
        } else {
            ESP_LOGI(TAG, "Audio player initialized successfully");
        }
    }

    // Increment the active session count
    pSampleConfiguration->activePlayerSessionCount++;

    MUTEX_UNLOCK(pSampleConfiguration->playerLock);

    // Set up callback for video frames
    if (pSampleConfiguration->videoPlayer != NULL && pSampleConfiguration->video_player_handle != NULL) {
        CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pVideoRtcRtpTransceiver,
                                      (UINT64)(uintptr_t) pSampleStreamingSession,
                                      sampleVideoFrameHandler));
    }

    // Set up callback for audio frames
    if (pSampleConfiguration->audioPlayer != NULL && pSampleConfiguration->audio_player_handle != NULL) {
    CHK_STATUS(transceiverOnFrame(pSampleStreamingSession->pAudioRtcRtpTransceiver,
                                  (UINT64)(uintptr_t) pSampleStreamingSession,
                                  sampleAudioFrameHandler));
    }

CleanUp:
    return (PVOID) (ULONG_PTR) retStatus;
}

/**
 * @brief Handler for received video frames
 */
VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame)
{
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession)(uintptr_t) customData;

    if (pFrame == NULL || pFrame->frameData == NULL || pSampleStreamingSession == NULL) {
        ESP_LOGW(TAG, "Invalid video frame or session data");
        return;
    }

    // Get the sample configuration and video player interface
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL || pSampleConfiguration->videoPlayer == NULL) {
        ESP_LOGW(TAG, "Video player interface not available");
        return;
    }

    // Use the video player interface from config
    media_stream_video_player_t *video_player = (media_stream_video_player_t*)pSampleConfiguration->videoPlayer;

    // Check if we have a valid handle from initialization
    if (pSampleConfiguration->video_player_handle == NULL) {
        ESP_LOGW(TAG, "Video player handle not initialized");
        return;
    }

    // Play the frame with the correct parameters
    video_player->play_frame(
        pSampleConfiguration->video_player_handle,
        pFrame->frameData,
        pFrame->size,
        (pFrame->flags & FRAME_FLAG_KEY_FRAME) != 0
    );
}

/**
 * @brief Handler for received audio frames
 */
VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame)
{
    PSampleStreamingSession pSampleStreamingSession = (PSampleStreamingSession)(uintptr_t) customData;

    if (pFrame == NULL || pFrame->frameData == NULL || pSampleStreamingSession == NULL) {
        ESP_LOGW(TAG, "Invalid audio frame or session data");
        return;
    }

    // Get the sample configuration and audio player interface
    PSampleConfiguration pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;
    if (pSampleConfiguration == NULL || pSampleConfiguration->audioPlayer == NULL) {
        ESP_LOGW(TAG, "Audio player interface not available");
        return;
    }

    // Use the audio player interface from config
    media_stream_audio_player_t *audio_player = (media_stream_audio_player_t*)pSampleConfiguration->audioPlayer;

    // Check if we have a valid handle from initialization
    if (pSampleConfiguration->audio_player_handle == NULL) {
        ESP_LOGW(TAG, "Audio player handle not initialized");
        return;
    }

    // Play the frame with the correct parameters
    audio_player->play_frame(
        pSampleConfiguration->audio_player_handle,
        pFrame->frameData,
        pFrame->size
    );
}

/**
 * @brief Create and send an offer as the initiator
 *
 * This function creates a WebRTC offer and sends it via the registered signaling callback.
 * It's used when the local peer is the initiator in the session.
 *
 * @param pPeerId Peer ID to send the offer to
 * @return STATUS code of the execution
 */
int webrtcAppCreateAndSendOffer(char *pPeerId)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    RtcSessionDescriptionInit offerSessionDescriptionInit;
    SignalingMessage message;
    UINT32 buffLen = MAX_SIGNALING_MESSAGE_LEN;
    BOOL locked = FALSE;

    CHK(pPeerId != NULL, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    pSampleConfiguration = gSampleConfiguration;

    ESP_LOGI(TAG, "Creating and sending WebRTC offer to peer: %s", pPeerId);

    // Create a streaming session as the initiator
    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    // Check if we already have a session for this peer
    UINT32 clientIdHash = COMPUTE_CRC32((PBYTE) pPeerId, (UINT32) STRLEN(pPeerId));
    BOOL peerConnectionFound = FALSE;

    CHK_STATUS(hashTableContains(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));

    if (peerConnectionFound) {
        ESP_LOGW(TAG, "Peer connection already exists for %s, reusing it", pPeerId);
        UINT64 hashValue = 0;
        CHK_STATUS(hashTableGet(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &hashValue));
        pSampleStreamingSession = (PSampleStreamingSession) hashValue;
    } else {
        // Create new streaming session
        if (pSampleConfiguration->streamingSessionCount >= ARRAY_SIZE(pSampleConfiguration->sampleStreamingSessionList)) {
            ESP_LOGE(TAG, "Max streaming sessions reached");
            CHK(FALSE, STATUS_INVALID_OPERATION);
        }

        CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, pPeerId, TRUE, &pSampleStreamingSession));

        // Add to the list and hash table
        pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount++] = pSampleStreamingSession;
        CHK_STATUS(hashTablePut(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, (UINT64) pSampleStreamingSession));
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    // Create an offer
    MEMSET(&offerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));
    offerSessionDescriptionInit.useTrickleIce = TRUE;

    ESP_LOGI(TAG, "Setting local description");
    // Set local description
    CHK_STATUS(setLocalDescription(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));

    ESP_LOGI(TAG, "Creating offer");
    // Create the offer
    CHK_STATUS(createOffer(pSampleStreamingSession->pPeerConnection, &offerSessionDescriptionInit));

    // Serialize the offer to JSON
    MEMSET(&message, 0x00, SIZEOF(SignalingMessage));

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    message.payload = (PCHAR) MEMCALLOC(1, MAX_SIGNALING_MESSAGE_LEN + 1);
    CHK(message.payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
#endif

    ESP_LOGI(TAG, "Serializing offer");
    CHK_STATUS(serializeSessionDescriptionInit(&offerSessionDescriptionInit, message.payload, &buffLen));

    ESP_LOGI(TAG, "Creating signaling message");
    // Create signaling message
    message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    message.messageType = SIGNALING_MESSAGE_TYPE_OFFER;
    STRNCPY(message.peerClientId, pPeerId, MAX_SIGNALING_CLIENT_ID_LEN);
    message.payloadLen = (UINT32) STRLEN(message.payload);
    SNPRINTF(message.correlationId, MAX_CORRELATION_ID_LEN, "%llu_%zu", GETTIME(), ATOMIC_INCREMENT(&pSampleStreamingSession->correlationIdPostFix));

    ESP_LOGI(TAG, "Sending offer");
    // Emit event for sent offer
    raiseEvent(APP_WEBRTC_EVENT_SENT_OFFER, 0, pPeerId, "Sent offer to peer");

    ESP_LOGI(TAG, "Serializing and sending offer");

    // Use sendSignalingMessage to centralize all message sending logic
    CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));

    ESP_LOGI(TAG, "Offer sent successfully to peer: %s", pPeerId);

CleanUp:

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    SAFE_MEMFREE(message.payload);
#endif

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to create and send offer: 0x%08" PRIx32, (UINT32) retStatus);
    }

    return (int) retStatus;
}

// =============================================================================
// Split Mode API Implementation
// =============================================================================

/**
 * @brief Register a callback for sending signaling messages to bridge (used in split mode)
 *
 * This function registers a callback that will be called when the WebRTC state machine
 * needs to send signaling messages. In split mode, this callback forwards the messages
 * to the streaming device via webrtc_bridge.
 *
 * @param callback Function to call when messages need to be sent to bridge
 * @return 0 on success, non-zero on failure
 */
int webrtcAppRegisterSendToBridgeCallback(app_webrtc_send_msg_cb_t callback)
{
    ESP_LOGI(TAG, "Registering send to bridge callback for split mode");
    g_sendMessageCallback = callback;
    return 0;
}

/**
 * @brief Send a message from bridge to signaling server (used in split mode)
 *
 * This function sends signaling messages received from the streaming device
 * to the signaling server via the signaling interface.
 *
 * @param signalingMessage The signaling message to send to signaling server
 * @return 0 on success, non-zero on failure
 */
int webrtcAppSendMessageToSignalingServer(signaling_msg_t *signalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    esp_webrtc_signaling_message_t message;

    ESP_LOGI(TAG, "Sending message from bridge to signaling server");

    CHK(signalingMessage != NULL, STATUS_NULL_ARG);
    CHK(gWebRtcAppConfig.pSignalingClientInterface != NULL && gSignalingClientData != NULL, STATUS_INVALID_OPERATION);

    // Convert signaling_msg_t to esp_webrtc_signaling_message_t format for signaling interface
    message.version = signalingMessage->version;

    switch (signalingMessage->messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            message.message_type = ESP_SIGNALING_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            message.message_type = ESP_SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            message.message_type = ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown signaling message type: %d", signalingMessage->messageType);
            CHK(FALSE, STATUS_INVALID_ARG);
    }

    // Copy message fields
    STRNCPY(message.correlation_id, signalingMessage->correlationId, MAX_CORRELATION_ID_LEN);
    message.correlation_id[MAX_CORRELATION_ID_LEN] = '\0';
    STRNCPY(message.peer_client_id, signalingMessage->peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
    message.peer_client_id[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    message.payload = signalingMessage->payload;
    message.payload_len = signalingMessage->payloadLen;

    ESP_LOGI(TAG, "Sending message type %d from peer %s to signaling server",
             message.message_type, message.peer_client_id);

    // Send directly to signaling server
    CHK_STATUS(gWebRtcAppConfig.pSignalingClientInterface->sendMessage(gSignalingClientData, &message));

    ESP_LOGI(TAG, "Successfully sent message to signaling server");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to send message to signaling server: 0x%08" PRIx32, (UINT32) retStatus);
    }

    return (int) retStatus;
}
