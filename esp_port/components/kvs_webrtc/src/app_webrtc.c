#include "app_webrtc.h"
#include "webrtc_mem_utils.h"
#include "flash_wrapper.h"

#include "media_stream.h"
#include "signaling_serializer.h"
#include "webrtc_bridge.h"

#include "sdkconfig.h"
#include "iot_credential_provider.h"

static const char *TAG = "app_webrtc";

// Frame size limits
#define MAX_VIDEO_FRAME_SIZE (1024 * 1024)  // 1MB should be enough for most video frames
#define MAX_AUDIO_FRAME_SIZE (32 * 1024)    // 32KB should be enough for audio frames

// Frame durations
#define DEFAULT_FPS_VALUE                        25
#define SAMPLE_VIDEO_FRAME_DURATION (HUNDREDS_OF_NANOS_IN_A_SECOND / DEFAULT_FPS_VALUE)
#define SAMPLE_AUDIO_FRAME_DURATION (20 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

// Track IDs
#define DEFAULT_VIDEO_TRACK_ID 1
#define DEFAULT_AUDIO_TRACK_ID 2

#define NUMBER_OF_H264_FRAME_FILES               60 //1500
// Event handling
static app_webrtc_event_callback_t gEventCallback = NULL;
static void* gEventUserCtx = NULL;
static MUTEX gEventCallbackLock = INVALID_MUTEX_VALUE;

// Global variables to track app state
static WebRtcAppConfig gWebRtcAppConfig = {0};
static BOOL gWebRtcAppInitialized = FALSE;

// Forward declarations for media sender functions
PVOID sendVideoFramesFromCamera(PVOID args);
PVOID sendAudioFramesFromMic(PVOID args);
PVOID sendVideoFramesFromSamples(PVOID args);
PVOID sendAudioFramesFromSamples(PVOID args);
PVOID sampleReceiveAudioVideoFrame(PVOID);

// Forward declarations for media handlers
VOID sampleVideoFrameHandler(UINT64 customData, PFrame pFrame);
VOID sampleAudioFrameHandler(UINT64 customData, PFrame pFrame);

// Task handle for the WebRTC run task
static TaskHandle_t gWebRtcRunTaskHandle = NULL;

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

STATUS handleAnswer(PSampleConfiguration pSampleConfiguration, PSampleStreamingSession pSampleStreamingSession, PSignalingMessage pSignalingMessage)
{
    UNUSED_PARAM(pSampleConfiguration);
    STATUS retStatus = STATUS_SUCCESS;
    RtcSessionDescriptionInit answerSessionDescriptionInit;

    MEMSET(&answerSessionDescriptionInit, 0x00, SIZEOF(RtcSessionDescriptionInit));

    CHK_STATUS(deserializeSessionDescriptionInit(pSignalingMessage->payload, pSignalingMessage->payloadLen, &answerSessionDescriptionInit));
    CHK_STATUS(setRemoteDescription(pSampleStreamingSession->pPeerConnection, &answerSessionDescriptionInit));

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

    DLOGD("**offer:%s", pSignalingMessage->payload);
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
    BOOL locked = FALSE;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(pSampleStreamingSession != NULL && pSampleStreamingSession->pSampleConfiguration != NULL && pMessage != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = pSampleStreamingSession->pSampleConfiguration;

    if (gWebRtcAppConfig.mode == APP_WEBRTC_STREAMING_ONLY_MODE) {
        // Serialize and forward message to host
        size_t serialized_len = 0;
        char *serialized_msg = serialize_signaling_message(
            (signaling_msg_t *) pMessage,
            &serialized_len);

        if (serialized_msg) {
            webrtc_bridge_send_message(serialized_msg, serialized_len);
        } else {
            DLOGW("Failed to serialize signaling message");
            retStatus = STATUS_INTERNAL_ERROR;
        }
    } else {
        CHK(IS_VALID_MUTEX_VALUE(pSampleConfiguration->signalingSendMessageLock) &&
                IS_VALID_SIGNALING_CLIENT_HANDLE(pSampleConfiguration->signalingClientHandle),
            STATUS_INVALID_OPERATION);

        MUTEX_LOCK(pSampleConfiguration->signalingSendMessageLock);
        locked = TRUE;
        CHK_STATUS(signalingClientSendMessageSync(pSampleConfiguration->signalingClientHandle, pMessage));
        if (pMessage->messageType == SIGNALING_MESSAGE_TYPE_ANSWER) {
            CHK_STATUS(signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &pSampleConfiguration->signalingClientMetrics));
            DLOGP("[Signaling offer received to answer sent time] %" PRIu64 " ms",
                    pSampleConfiguration->signalingClientMetrics.signalingClientStats.offerToAnswerTime);
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleStreamingSession->pSampleConfiguration->signalingSendMessageLock);
    }

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

    if (candidateJson == NULL && pSampleStreamingSession != NULL) {
        // ICE gathering is complete
        raiseEvent(APP_WEBRTC_EVENT_ICE_GATHERING_COMPLETE, 0,
                   pSampleStreamingSession->peerId, "ICE candidate gathering completed");
    } else if (candidateJson != NULL && pSampleStreamingSession != NULL) {
        // ICE candidate gathered - we don't need to do anything special
        // as the original handler already processes this
        raiseEvent(APP_WEBRTC_EVENT_SENT_ICE_CANDIDATE, 0,
                   pSampleStreamingSession->peerId, "Sent ICE candidate");
    }

    CHK(pSampleStreamingSession != NULL, STATUS_NULL_ARG);

    if (candidateJson != NULL) {
        DLOGI("New local ICE candidate gathered: %s", candidateJson);
    }

    if (candidateJson == NULL) {
        DLOGD("ice candidate gathering finished");
        ATOMIC_STORE_BOOL(&pSampleStreamingSession->candidateGatheringDone, TRUE);

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
        message.version = SIGNALING_MESSAGE_CURRENT_VERSION;
        message.messageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
        STRNCPY(message.peerClientId, pSampleStreamingSession->peerId, MAX_SIGNALING_CLIENT_ID_LEN);
        message.payloadLen = (UINT32) STRLEN(candidateJson);
#ifdef DYNAMIC_SIGNALING_PAYLOAD
        message.payload = MEMCALLOC(1, message.payloadLen + 1);
        CHK(message.payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
#endif
        STRNCPY(message.payload, candidateJson, message.payloadLen);
        message.correlationId[0] = '\0';
        CHK_STATUS(sendSignalingMessage(pSampleStreamingSession, &message));
    }

CleanUp:

#ifdef DYNAMIC_SIGNALING_PAYLOAD
    if (candidateJson != NULL &&
        pSampleStreamingSession->remoteCanTrickleIce &&
        ATOMIC_LOAD_BOOL(&pSampleStreamingSession->peerIdReceived) &&
        message.payload != NULL) {
        // freeSignalingMessagePayload(&message);
        SAFE_MEMFREE(message.payload);
    }
#endif

    CHK_LOG_ERR(retStatus);
}

STATUS initializePeerConnection(PSampleConfiguration pSampleConfiguration, PRtcPeerConnection* ppRtcPeerConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    RtcConfiguration configuration;
    UINT32 i, j, iceConfigCount, uriCount = 0, maxTurnServer = 1;
    PIceConfigInfo pIceConfigInfo;
    UINT64 data;
    PRtcCertificate pRtcCertificate = NULL;
    BOOL isStreamingOnly = FALSE;

    CHK(pSampleConfiguration != NULL && ppRtcPeerConnection != NULL, STATUS_NULL_ARG);

    // Check if we're in streaming-only mode (no signaling)
    isStreamingOnly = (pSampleConfiguration->channelInfo.pChannelName == NULL);

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));

    // Set more aggressive ICE timeouts
    configuration.kvsRtcConfiguration.iceConnectionCheckTimeout = 12 * HUNDREDS_OF_NANOS_IN_A_SECOND; // 2 seconds
    configuration.kvsRtcConfiguration.iceLocalCandidateGatheringTimeout = 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    configuration.kvsRtcConfiguration.iceCandidateNominationTimeout = 15 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    // More frequent checks for faster connection
    configuration.kvsRtcConfiguration.iceConnectionCheckPollingInterval = 100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND; // 100ms

    // Enable interface filtering to handle IPv6 properly
    configuration.kvsRtcConfiguration.iceSetInterfaceFilterFunc = sampleFilterNetworkInterfaces;

    // Set the ICE mode explicitly to allow TURN
    configuration.iceTransportPolicy = ICE_TRANSPORT_POLICY_ALL;

    // Set the STUN server
    PCHAR pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX;
    // If region is in CN, add CN region uri postfix
    if (STRSTR(pSampleConfiguration->channelInfo.pRegion, "cn-")) {
        pKinesisVideoStunUrlPostFix = KINESIS_VIDEO_STUN_URL_POSTFIX_CN;
    }
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUN_URL, pSampleConfiguration->channelInfo.pRegion,
             pKinesisVideoStunUrlPostFix);

    if (pSampleConfiguration->useTurn && !isStreamingOnly) {
        // Set the URIs from the configuration
        CHK_STATUS(signalingClientGetIceConfigInfoCount(pSampleConfiguration->signalingClientHandle, &iceConfigCount));

        /* signalingClientGetIceConfigInfoCount can return more than one turn server. Use only one to optimize
         * candidate gathering latency. But user can also choose to use more than 1 turn server. */
        for (uriCount = 0, i = 0; i < maxTurnServer; i++) {
            CHK_STATUS(signalingClientGetIceConfigInfo(pSampleConfiguration->signalingClientHandle, i, &pIceConfigInfo));
            for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                CHECK(uriCount < MAX_ICE_SERVERS_COUNT);
                /*
                 * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=udp" then ICE will try TURN over UDP
                 * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
                 * if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=udp", it's currently ignored because sdk dont do TURN
                 * over DTLS yet. if configuration.iceServers[uriCount + 1].urls is "turns:ip:port?transport=tcp" then ICE will try TURN over TCP/TLS
                 * if configuration.iceServers[uriCount + 1].urls is "turn:ip:port" then ICE will try both TURN over UDP and TCP/TLS
                 *
                 * It's recommended to not pass too many TURN iceServers to configuration because it will slow down ice gathering in non-trickle mode.
                 */

                STRNCPY(configuration.iceServers[uriCount + 1].urls, pIceConfigInfo->uris[j], MAX_ICE_CONFIG_URI_LEN);
                STRNCPY(configuration.iceServers[uriCount + 1].credential, pIceConfigInfo->password, MAX_ICE_CONFIG_CREDENTIAL_LEN);
                STRNCPY(configuration.iceServers[uriCount + 1].username, pIceConfigInfo->userName, MAX_ICE_CONFIG_USER_NAME_LEN);

                uriCount++;
            }
        }
    }

    pSampleConfiguration->iceUriCount = uriCount + 1;
    DLOGD("Total ICE servers configured: %d", pSampleConfiguration->iceUriCount);

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

STATUS createCredentialProvider(PSampleConfiguration pSampleConfiguration, PAwsCredentialOptions pAwsCredentialOptions)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL isStreamingOnly = FALSE;
    PCHAR pAccessKey = NULL, pSecretKey = NULL, pSessionToken = NULL;
    PCHAR pIotCoreCredentialEndPoint = NULL, pIotCoreCert = NULL, pIotCorePrivateKey = NULL;
    PCHAR pIotCoreRoleAlias = NULL, pIotCoreThingName = NULL;

    CHK(pSampleConfiguration != NULL && pAwsCredentialOptions != NULL, STATUS_NULL_ARG);

    // Check if we're in streaming-only mode (no signaling)
    isStreamingOnly = (pSampleConfiguration->channelInfo.pChannelName == NULL);

    // If in streaming-only mode, we don't need credentials
    CHK(!isStreamingOnly, STATUS_SUCCESS);

    // Make sure we don't already have a credential provider
    if (pSampleConfiguration->pCredentialProvider != NULL) {
        DLOGI("Credential provider already exists, skipping creation");
        CHK(FALSE, STATUS_SUCCESS);
    }

    DLOGI("Creating credential provider with region: %s", pAwsCredentialOptions->region);
    DLOGI("Channel name: %s", pSampleConfiguration->channelInfo.pChannelName);
    DLOGI("Credential type: %s", pAwsCredentialOptions->enableIotCredentials ? "IoT Core" : "Static");

    if (pAwsCredentialOptions->enableIotCredentials) {
        // Use IoT Core credentials from the options
        pIotCoreCredentialEndPoint = pAwsCredentialOptions->iotCoreCredentialEndpoint;
        pIotCoreCert = pAwsCredentialOptions->iotCoreCert;
        pIotCorePrivateKey = pAwsCredentialOptions->iotCorePrivateKey;
        pIotCoreRoleAlias = pAwsCredentialOptions->iotCoreRoleAlias;
        pIotCoreThingName = pAwsCredentialOptions->iotCoreThingName;

        // Validate required fields
        CHK_ERR(pIotCoreCredentialEndPoint != NULL && pIotCoreCredentialEndPoint[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core credential endpoint must be set");
        CHK_ERR(pIotCoreCert != NULL && pIotCoreCert[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core certificate must be set");
        CHK_ERR(pIotCorePrivateKey != NULL && pIotCorePrivateKey[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core private key must be set");
        CHK_ERR(pIotCoreRoleAlias != NULL && pIotCoreRoleAlias[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core role alias must be set");
        CHK_ERR(pIotCoreThingName != NULL && pIotCoreThingName[0] != '\0', STATUS_INVALID_OPERATION,
                "IoT Core thing name must be set");

        DLOGI("Creating IoT credential provider with endpoint: %s", pIotCoreCredentialEndPoint);
        DLOGI("IoT Core thing name: %s, role alias: %s", pIotCoreThingName, pIotCoreRoleAlias);
        DLOGI("Certificate path: %s", pIotCoreCert);
        DLOGI("Private key path: %s", pIotCorePrivateKey);
        DLOGI("CA cert path: %s", pSampleConfiguration->pCaCertPath);

        // Try to read the certificate file to verify it exists and is accessible
        FILE* certFile = fopen(pIotCoreCert, "r");
        if (certFile == NULL) {
            DLOGE("Failed to open certificate file: %s", pIotCoreCert);
            CHK(FALSE, STATUS_INVALID_OPERATION);
        } else {
            fclose(certFile);
            DLOGI("Successfully verified certificate file exists and is readable");
        }

        // Try to read the private key file to verify it exists and is accessible
        FILE* keyFile = fopen(pIotCorePrivateKey, "r");
        if (keyFile == NULL) {
            DLOGE("Failed to open private key file: %s", pIotCorePrivateKey);
            CHK(FALSE, STATUS_INVALID_OPERATION);
        } else {
            fclose(keyFile);
            DLOGI("Successfully verified private key file exists and is readable");
        }

        // Try to read the CA cert file to verify it exists and is accessible
        if (pSampleConfiguration->pCaCertPath != NULL) {
            FILE* caFile = fopen(pSampleConfiguration->pCaCertPath, "r");
            if (caFile == NULL) {
                DLOGE("Failed to open CA cert file: %s", pSampleConfiguration->pCaCertPath);
                CHK(FALSE, STATUS_INVALID_OPERATION);
            } else {
                fclose(caFile);
                DLOGI("Successfully verified CA cert file exists and is readable");
            }
        }

        retStatus = createIotCredentialProvider(
            pIotCoreCredentialEndPoint,
            pAwsCredentialOptions->region,
            pIotCoreCert,
            pIotCorePrivateKey,
            pSampleConfiguration->pCaCertPath,
            pIotCoreRoleAlias,
            pIotCoreThingName,
            &pSampleConfiguration->pCredentialProvider);

        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to create IoT credential provider: 0x%08x", retStatus);
            if ((retStatus >> 24) == 0x52) {
                DLOGE("This appears to be a networking error. Check network connectivity.");
            } else if ((retStatus >> 24) == 0x50) {
                DLOGE("This appears to be a platform error. Check certificate and key files.");
            } else if ((retStatus >> 24) == 0x58) {
                DLOGE("This appears to be a client error. Check IoT Core configuration.");
            }
            CHK(FALSE, retStatus);
        }

        DLOGI("IoT credential provider created successfully");
    } else {
        // Use direct AWS credentials from the options
        pAccessKey = pAwsCredentialOptions->accessKey;
        pSecretKey = pAwsCredentialOptions->secretKey;
        pSessionToken = pAwsCredentialOptions->sessionToken;

        // Validate required fields
        CHK_ERR(pAccessKey != NULL && pAccessKey[0] != '\0', STATUS_INVALID_OPERATION,
                "AWS access key must be set");
        CHK_ERR(pSecretKey != NULL && pSecretKey[0] != '\0', STATUS_INVALID_OPERATION,
                "AWS secret key must be set");

        DLOGI("Creating static credential provider with access key ID: %.*s...", 4, pAccessKey);
        retStatus = createStaticCredentialProvider(
            pAccessKey,
            0,
            pSecretKey,
            0,
            pSessionToken,
            0,
            MAX_UINT64,
            &pSampleConfiguration->pCredentialProvider);

        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to create static credential provider: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }

        DLOGI("Static credential provider created successfully");
    }

CleanUp:
    return retStatus;
}

// Now update the createSampleConfiguration function to remove credential provider creation
STATUS createSampleConfiguration(PCHAR channelName, SIGNALING_CHANNEL_ROLE_TYPE roleType, BOOL trickleIce, BOOL useTurn, UINT32 logLevel,
                                PAwsCredentialOptions pAwsCredentialOptions, PSampleConfiguration* ppSampleConfiguration)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(ppSampleConfiguration != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pSampleConfiguration = (PSampleConfiguration) MEMCALLOC(1, SIZEOF(SampleConfiguration))), STATUS_NOT_ENOUGH_MEMORY);

    pSampleConfiguration->pAwsCredentialOptions = (PAwsCredentialOptions) MEMCALLOC(1, SIZEOF(AwsCredentialOptions));
    CHK(pSampleConfiguration->pAwsCredentialOptions != NULL, STATUS_NOT_ENOUGH_MEMORY);
    MEMCPY(pSampleConfiguration->pAwsCredentialOptions, pAwsCredentialOptions, SIZEOF(AwsCredentialOptions));

    SET_LOGGER_LOG_LEVEL(logLevel);

#if 0
    // If the env is set, we generate normal log files apart from filtered profile log files
    // If not set, we generate only the filtered profile log files
    if (NULL != GETENV(ENABLE_FILE_LOGGING)) {
        retStatus = createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, TRUE, LOG_LEVEL_PROFILE, NULL);

        if (retStatus != STATUS_SUCCESS) {
            DLOGW("[KVS Master] createFileLogger(): operation returned status code: 0x%08x", retStatus);
        } else {
            pSampleConfiguration->enableFileLogging = TRUE;
        }
    } else {
        retStatus = createFileLoggerWithLevelFiltering(FILE_LOGGING_BUFFER_SIZE, MAX_NUMBER_OF_LOG_FILES, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH,
                                                       TRUE, TRUE, FALSE, LOG_LEVEL_PROFILE, NULL);

        if (retStatus != STATUS_SUCCESS) {
            DLOGW("[KVS Master] createFileLogger(): operation returned status code: 0x%08x", retStatus);
        } else {
            pSampleConfiguration->enableFileLogging = TRUE;
        }
    }
#endif

    pSampleConfiguration->channelInfo.pRegion = pAwsCredentialOptions->region;
    pSampleConfiguration->pCaCertPath = pAwsCredentialOptions->caCertPath;

    // Note: Credential provider creation moved to createCredentialProvider function

    pSampleConfiguration->mediaSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->audioSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->videoSenderTid = INVALID_TID_VALUE;
    pSampleConfiguration->signalingClientHandle = INVALID_SIGNALING_CLIENT_HANDLE_VALUE;
    pSampleConfiguration->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
    pSampleConfiguration->cvar = CVAR_CREATE();
    pSampleConfiguration->streamingSessionListReadLock = MUTEX_CREATE(FALSE);
    pSampleConfiguration->signalingSendMessageLock = MUTEX_CREATE(FALSE);

    // Initialize player-related fields
    pSampleConfiguration->video_player_handle = NULL;
    pSampleConfiguration->audio_player_handle = NULL;
    pSampleConfiguration->activePlayerSessionCount = 0;
    pSampleConfiguration->playerLock = MUTEX_CREATE(TRUE);

    /* This is ignored for master. Master can extract the info from offer. Viewer has to know if peer can trickle or
     * not ahead of time. */
    pSampleConfiguration->trickleIce = trickleIce;
    pSampleConfiguration->useTurn = useTurn;
    pSampleConfiguration->enableSendingMetricsToViewerViaDc = FALSE;
    pSampleConfiguration->receiveAudioVideoSource = NULL;

    pSampleConfiguration->channelInfo.version = CHANNEL_INFO_CURRENT_VERSION;
    pSampleConfiguration->channelInfo.pChannelName = channelName;
    pSampleConfiguration->channelInfo.pKmsKeyId = NULL;
    pSampleConfiguration->channelInfo.tagCount = 0;
    pSampleConfiguration->channelInfo.pTags = NULL;
    pSampleConfiguration->channelInfo.channelType = SIGNALING_CHANNEL_TYPE_SINGLE_MASTER;
    pSampleConfiguration->channelInfo.channelRoleType = roleType;
    // pSampleConfiguration->channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_FILE;
    pSampleConfiguration->channelInfo.cachingPolicy = SIGNALING_API_CALL_CACHE_TYPE_NONE;
    pSampleConfiguration->channelInfo.cachingPeriod = SIGNALING_API_CALL_CACHE_TTL_SENTINEL_VALUE;
    pSampleConfiguration->channelInfo.asyncIceServerConfig = TRUE; // has no effect
    pSampleConfiguration->channelInfo.retry = TRUE;
    pSampleConfiguration->channelInfo.reconnect = TRUE;
    pSampleConfiguration->channelInfo.pCertPath = pSampleConfiguration->pCaCertPath;
    pSampleConfiguration->channelInfo.messageTtl = 0; // Default is 60 seconds

    pSampleConfiguration->signalingClientCallbacks.version = SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION;
    pSampleConfiguration->signalingClientCallbacks.errorReportFn = signalingClientError;
    pSampleConfiguration->signalingClientCallbacks.stateChangeFn = signalingClientStateChanged;
    pSampleConfiguration->signalingClientCallbacks.customData = (UINT64) pSampleConfiguration;

    pSampleConfiguration->clientInfo.version = SIGNALING_CLIENT_INFO_CURRENT_VERSION;
    pSampleConfiguration->clientInfo.loggingLevel = logLevel;
    pSampleConfiguration->clientInfo.cacheFilePath = NULL; // Use the default path
    pSampleConfiguration->clientInfo.signalingClientCreationMaxRetryAttempts = CREATE_SIGNALING_CLIENT_RETRY_ATTEMPTS_SENTINEL_VALUE;
    pSampleConfiguration->iceCandidatePairStatsTimerId = MAX_UINT32;
    pSampleConfiguration->pregenerateCertTimerId = MAX_UINT32;
    pSampleConfiguration->signalingClientMetrics.version = SIGNALING_CLIENT_METRICS_CURRENT_VERSION;

    // Flag to enable/disable TWCC
    // pSampleConfiguration->enableTwcc = TRUE;

    ATOMIC_STORE_BOOL(&pSampleConfiguration->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
    ATOMIC_STORE_BOOL(&pSampleConfiguration->connected, FALSE);

    // Set default media type to audio-video
    pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;

    if (gWebRtcAppConfig.mode == APP_WEBRTC_SIGNALING_ONLY_MODE) {
        // Optimization: We don't need to create a timer queue in signaling-only mode
        DLOGI("Skipping timer queue creation in signaling-only mode");
    } else {
#define TIMER_QUEUE_THREAD_SIZE (8 * 1024) // I think even this is too much
        timerQueueCreateEx(&pSampleConfiguration->timerQueueHandle, "pregenCertTmr", TIMER_QUEUE_THREAD_SIZE);

        CHK_STATUS(stackQueueCreate(&pSampleConfiguration->pregeneratedCertificates));

        // Start the cert pre-gen timer callback
        if (SAMPLE_PRE_GENERATE_CERT) {
            CHK_LOG_ERR(retStatus =
                            timerQueueAddTimer(pSampleConfiguration->timerQueueHandle, 0, SAMPLE_PRE_GENERATE_CERT_PERIOD, pregenerateCertTimerCallback,
                                            (UINT64) pSampleConfiguration, &pSampleConfiguration->pregenerateCertTimerId));
        }
    }

    pSampleConfiguration->iceUriCount = 0;

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

    // For streaming-only mode, don't initialize signaling
    if (pSampleConfiguration->channelInfo.pChannelName == NULL) {
        ESP_LOGI(TAG, "Skipping signaling initialization for streaming-only mode");
        return STATUS_SUCCESS;
    }

    // Ensure credential provider is created before initializing signaling
    if (pSampleConfiguration->pCredentialProvider == NULL) {
        DLOGE("Credential provider is NULL, cannot initialize signaling");
        return STATUS_INVALID_OPERATION;
    }

    SignalingClientMetrics signalingClientMetrics = pSampleConfiguration->signalingClientMetrics;
    pSampleConfiguration->signalingClientCallbacks.messageReceivedFn = signalingMessageReceived;
    STRCPY(pSampleConfiguration->clientInfo.clientId, clientId);

    ESP_LOGI(TAG, "Creating signaling client");
    raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTING, 0, NULL, "Creating signaling client");
    CHK_STATUS(createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                         &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                         &pSampleConfiguration->signalingClientHandle));

    ESP_LOGI(TAG, "Fetching signaling client");
    raiseEvent(APP_WEBRTC_EVENT_SIGNALING_DESCRIBE, 0, NULL, "Fetching signaling client");
    // Enable the processing of the messages
    CHK_STATUS(signalingClientFetchSync(pSampleConfiguration->signalingClientHandle));

    ESP_LOGI(TAG, "Connecting signaling client");
    raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTING, 0, NULL, "Connecting signaling client");
    CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));

    ESP_LOGI(TAG, "Getting signaling client metrics");
    signalingClientGetMetrics(pSampleConfiguration->signalingClientHandle, &signalingClientMetrics);

    // Logging this here since the logs in signaling library do not get routed to file
    DLOGP("[Signaling Get token] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getTokenCallTime);
    DLOGP("[Signaling Describe] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.describeCallTime);
    DLOGP("[Signaling Describe Media] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.describeMediaCallTime);
    DLOGP("[Signaling Create Channel] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.createCallTime);
    DLOGP("[Signaling Get endpoint] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getEndpointCallTime);
    DLOGP("[Signaling Get ICE config] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.getIceConfigCallTime);
    DLOGP("[Signaling Connect] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.connectCallTime);
    if (signalingClientMetrics.signalingClientStats.joinSessionCallTime != 0) {
        DLOGP("[Signaling Join Session] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.joinSessionCallTime);
    }
    DLOGP("[Signaling create client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.createClientTime);
    DLOGP("[Signaling fetch client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.fetchClientTime);
    DLOGP("[Signaling connect client] %" PRIu64 " ms", signalingClientMetrics.signalingClientStats.connectClientTime);
    pSampleConfiguration->signalingClientMetrics = signalingClientMetrics;
    gSampleConfiguration = pSampleConfiguration;
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
    BOOL isStreamingOnly = FALSE;

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
    isStreamingOnly = (pSampleConfiguration->channelInfo.pChannelName == NULL);

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

    // Since, we use config to decide if we are using iot credentials, we need to free checking the flag
    if (!isStreamingOnly && pSampleConfiguration->pCredentialProvider != NULL) {
        if (pSampleConfiguration->pAwsCredentialOptions != NULL && pSampleConfiguration->pAwsCredentialOptions->enableIotCredentials) {
            freeIotCredentialProvider(&pSampleConfiguration->pCredentialProvider);
        } else {
            freeStaticCredentialProvider(&pSampleConfiguration->pCredentialProvider);
        }
    }

    SAFE_MEMFREE(pSampleConfiguration->pAwsCredentialOptions);

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

STATUS sessionCleanupWait(PSampleConfiguration pSampleConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    UINT32 i, clientIdHash;
    BOOL sampleConfigurationObjLockLocked = FALSE, streamingSessionListReadLockLocked = FALSE, peerConnectionFound = FALSE, sessionFreed = FALSE;
    SIGNALING_CLIENT_STATE signalingClientState;
    BOOL isStreamingOnly = FALSE;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Check if we're in streaming-only mode
    isStreamingOnly = (pSampleConfiguration->channelInfo.pChannelName == NULL);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->interrupted)) {
        // Keep the main set of operations interlocked until cvar wait which would atomically unlock
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = TRUE;

        // scan and cleanup terminated streaming session
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->sampleStreamingSessionList[i]->terminateFlag)) {
                pSampleStreamingSession = pSampleConfiguration->sampleStreamingSessionList[i];

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = TRUE;

                // swap with last element and decrement count
                pSampleConfiguration->streamingSessionCount--;
                pSampleConfiguration->sampleStreamingSessionList[i] =
                    pSampleConfiguration->sampleStreamingSessionList[pSampleConfiguration->streamingSessionCount];

                // Remove from the hash table
                clientIdHash = COMPUTE_CRC32((PBYTE) pSampleStreamingSession->peerId, (UINT32) STRLEN(pSampleStreamingSession->peerId));
                CHK_STATUS(hashTableContains(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));
                if (peerConnectionFound) {
                    CHK_STATUS(hashTableRemove(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash));
                }

                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = FALSE;

                CHK_STATUS(freeSampleStreamingSession(&pSampleStreamingSession));
                sessionFreed = TRUE;
            }
        }

        if (!isStreamingOnly && sessionFreed && pSampleConfiguration->channelInfo.useMediaStorage && !ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient)) {
            // In the WebRTC Media Storage Ingestion Case the backend will terminate the session after
            // 1 hour.  The SDK needs to make a new JoinSession Call in order to receive a new
            // offer from the backend.  We will create a new sample streaming session upon receipt of the
            // offer.  The signalingClientConnectSync call will result in a JoinSession API call being made.
            CHK_STATUS(signalingClientDisconnectSync(pSampleConfiguration->signalingClientHandle));
            CHK_STATUS(signalingClientFetchSync(pSampleConfiguration->signalingClientHandle));
            CHK_STATUS(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));
            sessionFreed = FALSE;
        }

        // Check if we need to re-create the signaling client on-the-fly
        if (!isStreamingOnly && ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreateSignalingClient)) {
            retStatus = signalingClientFetchSync(pSampleConfiguration->signalingClientHandle);
            if (STATUS_SUCCEEDED(retStatus)) {
                // Re-set the variable again
                ATOMIC_STORE_BOOL(&pSampleConfiguration->recreateSignalingClient, FALSE);
            } else if (signalingCallFailed(retStatus)) {
                printf("[KVS Common] recreating Signaling Client\n");
                freeSignalingClient(&pSampleConfiguration->signalingClientHandle);
                createSignalingClientSync(&pSampleConfiguration->clientInfo, &pSampleConfiguration->channelInfo,
                                          &pSampleConfiguration->signalingClientCallbacks, pSampleConfiguration->pCredentialProvider,
                                          &pSampleConfiguration->signalingClientHandle);
            }
        }

        // Check the signaling client state and connect if needed
        if (!isStreamingOnly && IS_VALID_SIGNALING_CLIENT_HANDLE(pSampleConfiguration->signalingClientHandle)) {
            CHK_STATUS(signalingClientGetCurrentState(pSampleConfiguration->signalingClientHandle, &signalingClientState));
            if (signalingClientState == SIGNALING_CLIENT_STATE_READY) {
                UNUSED_PARAM(signalingClientConnectSync(pSampleConfiguration->signalingClientHandle));
            }
        }

        // Check if any lingering pending message queues
        CHK_STATUS(removeExpiredMessageQueues(pSampleConfiguration->pPendingSignalingMessageForRemoteClient));

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

STATUS signalingMessageReceived(UINT64 customData, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;
    BOOL peerConnectionFound = FALSE, locked = FALSE, startStats = FALSE, freeStreamingSession = FALSE;
    UINT32 clientIdHash;
    UINT64 hashValue = 0;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    PSampleStreamingSession pSampleStreamingSession = NULL;
    PReceivedSignalingMessage pReceivedSignalingMessageCopy = NULL;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Check if we're in signaling-only mode by configuration
    if (gWebRtcAppConfig.mode == APP_WEBRTC_SIGNALING_ONLY_MODE) {
        DLOGD("Signaling only mode: received message type %d", pReceivedSignalingMessage->signalingMessage.messageType);

        // Forward message directly to host without creating a streaming session
        switch (pReceivedSignalingMessage->signalingMessage.messageType) {
            case SIGNALING_MESSAGE_TYPE_OFFER:
                DLOGI("Received OFFER in signaling-only mode");
                raiseEvent(APP_WEBRTC_EVENT_RECEIVED_OFFER, 0,
                           pReceivedSignalingMessage->signalingMessage.peerClientId,
                           "Received offer in signaling-only mode");
                /* fall-through */
            case SIGNALING_MESSAGE_TYPE_ANSWER:
                /* fall-through */
            case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE: {
                // Serialize and forward message to host
                size_t serialized_len = 0;
                char *serialized_msg = serialize_signaling_message(
                    (signaling_msg_t *) &pReceivedSignalingMessage->signalingMessage,
                    &serialized_len);

                if (serialized_msg) {
                    webrtc_bridge_send_message(serialized_msg, serialized_len);
                } else {
                    DLOGW("Failed to serialize signaling message");
                }
                return retStatus;
            }
            default:
                DLOGW("Unhandled signaling message type %u in signaling-only mode",
                      pReceivedSignalingMessage->signalingMessage.messageType);
                break;
        }

        // For signaling-only mode, we don't process further
        return retStatus;
    }

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

            if (gWebRtcAppConfig.mode == APP_WEBRTC_SIGNALING_ONLY_MODE) {
                // In signaling-only mode, we don't create streaming sessions
                DLOGI("Skipping streaming session creation in signaling-only mode");
                CHK(FALSE, retStatus);
            } else {
                CHK_STATUS(createSampleStreamingSession(pSampleConfiguration, pReceivedSignalingMessage->signalingMessage.peerClientId, TRUE,
                                                        &pSampleStreamingSession));
            }

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
        SAFE_MEMFREE(pReceivedSignalingMessageCopy);
    }
#else
    SAFE_MEMFREE(pReceivedSignalingMessageCopy);
#endif

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
    AwsCredentialOptions awsCredentialOptions = {0};

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

    // Set up AWS credential options
    awsCredentialOptions.enableIotCredentials = pConfig->useIotCredentials;

    if (pConfig->mode != APP_WEBRTC_STREAMING_ONLY_MODE && pConfig->useIotCredentials) {
        // Configure IoT Core credentials
        awsCredentialOptions.iotCoreCredentialEndpoint = pConfig->iotCoreCredentialEndpoint;
        awsCredentialOptions.iotCoreCert = pConfig->iotCoreCert;
        awsCredentialOptions.iotCorePrivateKey = pConfig->iotCorePrivateKey;
        awsCredentialOptions.iotCoreRoleAlias = pConfig->iotCoreRoleAlias;
        awsCredentialOptions.iotCoreThingName = pConfig->iotCoreThingName;

        // Validate IoT Core credentials are provided
        if (pConfig->iotCoreCredentialEndpoint == NULL || pConfig->iotCoreCredentialEndpoint[0] == '\0' ||
            pConfig->iotCoreCert == NULL || pConfig->iotCoreCert[0] == '\0' ||
            pConfig->iotCorePrivateKey == NULL || pConfig->iotCorePrivateKey[0] == '\0' ||
            pConfig->iotCoreRoleAlias == NULL || pConfig->iotCoreRoleAlias[0] == '\0' ||
            pConfig->iotCoreThingName == NULL || pConfig->iotCoreThingName[0] == '\0') {
            DLOGE("IoT Core credentials are not properly configured");
            goto CleanUp;
        }
    } else if (pConfig->mode != APP_WEBRTC_STREAMING_ONLY_MODE) {
        // Configure direct AWS credentials
        awsCredentialOptions.accessKey = pConfig->awsAccessKey;
        awsCredentialOptions.secretKey = pConfig->awsSecretKey;
        awsCredentialOptions.sessionToken = pConfig->awsSessionToken;

        // Validate static credentials are provided
        if (pConfig->awsAccessKey == NULL || pConfig->awsAccessKey[0] == '\0' ||
            pConfig->awsSecretKey == NULL || pConfig->awsSecretKey[0] == '\0') {

            DLOGE("Static AWS credentials are not properly configured but useIotCredentials is FALSE");
            goto CleanUp;
        }
    }

    // Set common AWS options
    awsCredentialOptions.region = pConfig->awsRegion;
    awsCredentialOptions.caCertPath = pConfig->caCertPath;
    awsCredentialOptions.logLevel = pConfig->logLevel;

    // Initialize KVS WebRTC
    DLOGI("Creating WebRTC sample configuration");
    CHK_STATUS(createSampleConfiguration(
        pConfig->pChannelName,
        pConfig->roleType,
        TRUE, // trickleIce
        pConfig->useTurn,
        pConfig->logLevel,
        &awsCredentialOptions,
        &pSampleConfiguration
    ));

    // Map the media type from WebRtcAppConfig to SampleConfiguration
    pSampleConfiguration->mediaType = pConfig->mediaType;

    // Set the codecs explicitly
    pSampleConfiguration->videoCodec = pConfig->videoCodec;
    pSampleConfiguration->audioCodec = pConfig->audioCodec;

    // Store media capture interfaces
    pSampleConfiguration->videoCapture = pConfig->videoCapture;
    pSampleConfiguration->audioCapture = pConfig->audioCapture;

    // Store media player interfaces
    pSampleConfiguration->videoPlayer = pConfig->videoPlayer;
    pSampleConfiguration->audioPlayer = pConfig->audioPlayer;

    // Store media reception flag
    pSampleConfiguration->receiveMedia = pConfig->receiveMedia;

    // Disable media storage for simplicity
    pSampleConfiguration->channelInfo.useMediaStorage = FALSE;

    // Handle signaling-only mode
    if (pConfig->mode == APP_WEBRTC_SIGNALING_ONLY_MODE) {
        DLOGI("Initializing in signaling-only mode");
        // In signaling-only mode, we don't initialize media sources
        pSampleConfiguration->audioSource = NULL;
        pSampleConfiguration->videoSource = NULL;
    } else {
        DLOGI("Initializing KVS WebRTC stack");

        // Initialize media sources based on media type
        if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_VIDEO ||
            pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {
            pSampleConfiguration->videoSource = sendVideoFramesFromCamera;
        }

        if (pSampleConfiguration->mediaType == APP_WEBRTC_MEDIA_AUDIO_VIDEO) {
            pSampleConfiguration->audioSource = sendAudioFramesFromMic;
        }

        if (pSampleConfiguration->receiveMedia) {
            // Set the receive callback
            pSampleConfiguration->receiveAudioVideoSource = sampleReceiveAudioVideoFrame;
        }

        // Initialize KVS WebRTC
        CHK_STATUS(initKvsWebRtc());
    }

#ifndef CONFIG_USE_ESP_WEBSOCKET_CLIENT
    // Custom allocator for libwebsockets
    lws_set_allocator(realloc_wrapper);
#endif

#ifdef ENABLE_DATA_CHANNEL
    pSampleConfiguration->onDataChannel = onDataChannel;
#endif

    // Store the sample configuration globally
    gSampleConfiguration = pSampleConfiguration;

    // Set as initialized but not yet running
    gWebRtcAppInitialized = TRUE;

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

    // Create credential provider if not in streaming-only mode
    if (gWebRtcAppConfig.mode != APP_WEBRTC_STREAMING_ONLY_MODE) {
        // Credential provider creation moved to webrtcAppInit
        // DLOGI("Creating credential provider");
        retStatus = createCredentialProvider(gSampleConfiguration, gSampleConfiguration->pAwsCredentialOptions);
        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to create credential provider: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }

        DLOGI("Initializing signaling client");
        // Initialize signaling client with a client ID based on role type
        retStatus = initSignaling(
            gSampleConfiguration,
            gWebRtcAppConfig.roleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER ? SAMPLE_VIEWER_CLIENT_ID : SAMPLE_MASTER_CLIENT_ID
        );
        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to initialize signaling: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }
    } else {
        DLOGI("Streaming only mode, skipping credential provider and signaling initialization");
    }

    if (gWebRtcAppConfig.mode == APP_WEBRTC_STREAMING_ONLY_MODE) {
        DLOGI("Streaming only mode, skipping termination wait");
        goto CleanUp;
    }

    // Wait for termination
    sessionCleanupWait(gSampleConfiguration);
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

        // Free resources
        freeSignalingClient(&gSampleConfiguration->signalingClientHandle);
        freeSampleConfiguration(&gSampleConfiguration);
    }

    // Reset state
    gWebRtcAppInitialized = FALSE;
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
    UINT64 lastFrameTime = GETTIME();
    media_stream_video_capture_t *video_capture = NULL;
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;
    const uint32_t fps = 30;
    const uint32_t frame_duration_ms = 1000 / fps;

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

        // Get video frame from camera
        if (video_capture->get_frame(video_handle, &video_frame, 0) == ESP_OK) {
            if (video_frame != NULL && video_frame->len > 0) {
                // Send the frame
                retStatus = webrtcAppSendVideoFrame(
                    video_frame->buffer,
                    video_frame->len,
                    currentTime - lastFrameTime,
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

        // Adjust sleep for proper timing
        UINT64 elapsed = currentTime - lastFrameTime;
        UINT64 frame_duration_ns = frame_duration_ms * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        if (elapsed < frame_duration_ns) {
            THREAD_SLEEP(frame_duration_ns - elapsed);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // Small delay
        }
        lastFrameTime = currentTime;
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
    UINT64 lastFrameTime = GETTIME();
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;
    const uint32_t frame_duration_ms = 20;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the audio capture interface from the sample configuration
    audio_capture = (media_stream_audio_capture_t*) pSampleConfiguration->audioCapture;
    CHK(audio_capture != NULL, STATUS_INVALID_ARG);

    // Initialize audio capture with Opus configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = 48000,
            .channels = 1,
            .bits_per_sample = 16
        },
        .bitrate = 64,  // 64 kbps
        .frame_duration_ms = frame_duration_ms,
        .codec_specific = NULL
    };

    // Initialize audio capture
    ESP_LOGI(TAG, "Initializing audio capture");
    CHK(audio_capture->init(&audio_config, &audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start audio capture
    CHK(audio_capture->start(audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Get audio frame from microphone
        if (audio_capture->get_frame(audio_handle, &audio_frame, 0) == ESP_OK) {
            if (audio_frame != NULL && audio_frame->len > 0) {
                // Send the frame
                retStatus = webrtcAppSendAudioFrame(
                    audio_frame->buffer,
                    audio_frame->len,
                    currentTime - lastFrameTime
                );

                if (STATUS_FAILED(retStatus)) {
                    ESP_LOGE(TAG, "Failed to send audio frame: 0x%08" PRIx32, retStatus);
                }

                // Release the frame when done
                audio_capture->release_frame(audio_handle, audio_frame);
                audio_frame = NULL;
            }
        }

        // Adjust sleep for proper timing
        UINT64 elapsed = currentTime - lastFrameTime;
        UINT64 frame_duration_ns = frame_duration_ms * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        if (elapsed < frame_duration_ns) {
            THREAD_SLEEP(frame_duration_ns - elapsed);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // Small delay
        }
        lastFrameTime = currentTime;
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
    UINT64 lastFrameTime = GETTIME();
    media_stream_video_capture_t *video_capture = NULL;
    video_capture_handle_t video_handle = NULL;
    video_frame_t *video_frame = NULL;

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
            .fps = 30
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

        // Get video frame from capture interface
        if (video_capture->get_frame(video_handle, &video_frame, 0) == ESP_OK) {
            if (video_frame != NULL && video_frame->len > 0) {
                // Send the frame
                retStatus = webrtcAppSendVideoFrame(
                    video_frame->buffer,
                    video_frame->len,
                    currentTime - lastFrameTime,
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

        // Adjust sleep for proper timing
        UINT64 elapsed = currentTime - lastFrameTime;
        if (elapsed < SAMPLE_VIDEO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_VIDEO_FRAME_DURATION - elapsed);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // Small delay
        }
        lastFrameTime = currentTime;
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
    UINT64 lastFrameTime = GETTIME();
    media_stream_audio_capture_t *audio_capture = NULL;
    audio_capture_handle_t audio_handle = NULL;
    audio_frame_t *audio_frame = NULL;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Get the audio capture interface from the sample configuration
    audio_capture = (media_stream_audio_capture_t*) pSampleConfiguration->audioCapture;
    CHK(audio_capture != NULL, STATUS_INVALID_ARG);

    // Initialize with sample audio configuration
    audio_capture_config_t audio_config = {
        .codec = AUDIO_CODEC_OPUS,
        .format = {
            .sample_rate = 48000,
            .channels = 1,
            .bits_per_sample = 16
        },
        .bitrate = 64,  // 64 kbps
        .frame_duration_ms = 20,
        .codec_specific = NULL
    };

    // Initialize audio capture for samples
    ESP_LOGI(TAG, "Initializing sample audio capture");
    CHK(audio_capture->init(&audio_config, &audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    // Start audio capture
    CHK(audio_capture->start(audio_handle) == ESP_OK, STATUS_INTERNAL_ERROR);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        UINT64 currentTime = GETTIME();

        // Get audio frame from capture interface
        if (audio_capture->get_frame(audio_handle, &audio_frame, 0) == ESP_OK) {
            if (audio_frame != NULL && audio_frame->len > 0) {
                // Send the frame
                retStatus = webrtcAppSendAudioFrame(
                    audio_frame->buffer,
                    audio_frame->len,
                    currentTime - lastFrameTime
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

        // Adjust sleep for proper timing
        UINT64 elapsed = currentTime - lastFrameTime;
        if (elapsed < SAMPLE_AUDIO_FRAME_DURATION) {
            THREAD_SLEEP(SAMPLE_AUDIO_FRAME_DURATION - elapsed);
        } else {
            THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND); // Small delay
        }
        lastFrameTime = currentTime;
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
