/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include "app_webrtc.h"
#include "app_webrtc_internal.h"
#include "webrtc_mem_utils.h"
#include "esp_work_queue.h"

#include "flash_wrapper.h"
#include "logger.h"
#include "filelogger.h"
#include "fileio.h"

#include "media_stream.h"

#include "sdkconfig.h"
#include "esp_log.h"

static const char *TAG = "app_webrtc";

// Event handling
static app_webrtc_event_callback_t gEventCallback = NULL;
static void* gEventUserCtx = NULL;
static MUTEX gEventCallbackLock = INVALID_MUTEX_VALUE;

// Global variables to track app state
app_webrtc_config_t gWebRtcAppConfig = {0};
static PVOID gSignalingClientData = NULL;  // Store the initialized signaling client
static BOOL gapp_webrtc_initialized = FALSE;

// Global storage for early data channel callbacks
static struct {
    bool callbacks_registered;
    app_webrtc_rtc_on_open_t onOpen;
    app_webrtc_rtc_on_message_t onMessage;
    uint64_t custom_data;
} g_early_data_channel_callbacks = {0};

// Task handle for the WebRTC run task
static TaskHandle_t gWebRtcRunTaskHandle = NULL;

// Global KVS WebRTC client handle for interface calls
static void* g_kvs_webrtc_client = NULL;

// Progressive ICE server callback for dynamic updates
static WEBRTC_STATUS app_webrtc_on_ice_servers_updated(uint64_t customData, uint32_t newServerCount);

// Progressive ICE trigger function to eliminate code duplication
static WEBRTC_STATUS app_webrtc_trigger_progressive_ice(const char* context, bool useTurn);

// Forward declarations for wrapper functions
static WEBRTC_STATUS signalingMessageReceivedWrapper(uint64_t customData, webrtc_message_t* pWebRtcMessage);
static WEBRTC_STATUS peerConnectionStateChangedWrapper(uint64_t customData, webrtc_peer_state_t state);

// Core WebRTC app function declarations
STATUS getIceCandidatePairStatsCallback(UINT32, UINT64, UINT64);

STATUS createAppWebRTCContext(bool trickleIce, bool useTurn, uint32_t logLevel, bool signaling_only,
                              papp_webrtc_context_t* ppContext);
STATUS freeAppWebRTCContext(papp_webrtc_context_t*);
STATUS signalingMessageReceived(UINT64, webrtc_message_t*);
STATUS sendSignalingMessage(PAppWebRTCSession, webrtc_message_t*);

VOID onConnectionStateChange(UINT64, webrtc_peer_state_t);
STATUS sessionCleanupWait(PSampleConfiguration, bool);
STATUS createMessageQueue(UINT64, PPendingMessageQueue*);
STATUS freeMessageQueue(PPendingMessageQueue);
STATUS submitPendingIceCandidate(PPendingMessageQueue, PAppWebRTCSession);
STATUS removeExpiredMessageQueues(PStackQueue);
STATUS getPendingMessageQueueForHash(PStackQueue, UINT64, BOOL, PPendingMessageQueue*);

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
        STRNCPY(event_data.peer_id, peer_id, APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
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

PSampleConfiguration gSampleConfiguration = NULL;

VOID sigintHandler(INT32 sigNum)
{
    UNUSED_PARAM(sigNum);
    if (gSampleConfiguration != NULL) {
        ATOMIC_STORE_BOOL(&gSampleConfiguration->interrupted, TRUE);
        CVAR_BROADCAST(gSampleConfiguration->cvar);
    }
}

/**
 * @brief Handle signaling state changes directly from the interface
 */
static WEBRTC_STATUS signalingClientStateChangedWrapper(uint64_t customData, webrtc_signaling_state_t state)
{
    UNUSED_PARAM(customData);
    PCHAR pStateStr;

    // Map webrtc signaling state to string representation
    static CHAR stateStrNew[] = "NEW";
    static CHAR stateStrConnecting[] = "CONNECTING";
    static CHAR stateStrConnected[] = "CONNECTED";
    static CHAR stateStrDisconnected[] = "DISCONNECTED";
    static CHAR stateStrFailed[] = "FAILED";
    static CHAR stateStrUnknown[] = "UNKNOWN";

    switch (state) {
        case WEBRTC_SIGNALING_STATE_NEW:
            pStateStr = stateStrNew;
            break;
        case WEBRTC_SIGNALING_STATE_CONNECTING:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTING, 0, NULL, "Signaling client connecting");
            pStateStr = stateStrConnecting;
            break;
        case WEBRTC_SIGNALING_STATE_CONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_CONNECTED, 0, NULL, "Signaling client connected");
            pStateStr = stateStrConnected;

            // Reset retry logic on successful connection
            if (gSampleConfiguration != NULL) {
                ATOMIC_STORE_BOOL(&gSampleConfiguration->recreate_signaling_client, FALSE);
                ESP_LOGD(TAG, "Signaling client connected successfully, retry logic reset");
            }
            break;
        case WEBRTC_SIGNALING_STATE_DISCONNECTED:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED, 0, NULL, "Signaling client disconnected");
            pStateStr = stateStrDisconnected;
            break;
        case WEBRTC_SIGNALING_STATE_FAILED:
            raiseEvent(APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED, 0, NULL, "Signaling client failed");
            pStateStr = stateStrFailed;
            break;
        default:
            pStateStr = stateStrUnknown;
            break;
    }

    ESP_LOGD(TAG, "Signaling state changed: state=%d ('%s')", state, pStateStr);

    // webrtc_mem_utils_print_stats(TAG);
    return WEBRTC_STATUS_SUCCESS;
}

STATUS signalingClientError(UINT64 customData, STATUS status, PCHAR msg, UINT32 msgLen)
{
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    DLOGW("Signaling client generated an error 0x%08x - '%.*s'", status, msgLen, msg);
    CHAR errorMsg[256];
    SNPRINTF(errorMsg, SIZEOF(errorMsg), "Signaling error: 0x%08" PRIx32 " - %.*s", status, (int) msgLen, msg);
    raiseEvent(APP_WEBRTC_EVENT_SIGNALING_ERROR, status, NULL, errorMsg);

    // Check for signaling errors that should trigger client recreation/reconnection
    if (status == WEBRTC_STATUS_SIGNALING_ICE_REFRESH_FAILED ||
        status == WEBRTC_STATUS_SIGNALING_RECONNECT_FAILED ||
        status == WEBRTC_STATUS_SIGNALING_CONNECTION_LOST ||
        status == WEBRTC_STATUS_SIGNALING_AUTH_FAILED) {
        DLOGI("DEBUG: Signaling error (category: %s) requires reconnection, setting recreateSignalingClient=TRUE",
              WEBRTC_STATUS_IS_SIGNALING_ERROR(status) ? "SIGNALING" : "OTHER");
        ATOMIC_STORE_BOOL(&pSampleConfiguration->recreate_signaling_client, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);
    } else if (WEBRTC_STATUS_IS_SIGNALING_ERROR(status)) {
        DLOGI("DEBUG: Signaling error (code: %d) does not require reconnection, handled locally", status);
    } else {
        DLOGI("DEBUG: Non-signaling error (code: %d) handled locally", status);
    }

    return STATUS_SUCCESS;
}

/**
 * @brief Wrapper for signaling error callback with WEBRTC_STATUS return type
 *
 * NOTE: For error callbacks, we preserve the original KVS status codes
 * rather than converting them, as specific error codes like
 * STATUS_SIGNALING_RECONNECT_FAILED are needed for reconnection logic.
 */
static WEBRTC_STATUS signalingClientErrorWrapper(uint64_t customData, WEBRTC_STATUS status, char* msg, uint32_t msgLen)
{
    // Preserve the original status code - don't convert it to generic WEBRTC_STATUS
    // The status parameter is actually a KVS STATUS value passed as WEBRTC_STATUS
    STATUS kvsStatus = (STATUS)status;

    STATUS result = signalingClientError((UINT64)customData, kvsStatus, (PCHAR)msg, (UINT32)msgLen);
    return (result == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

/**
 * @brief Wrapper for peer connection state change callbacks
 * This handles states from the peer connection interface, including media startup signals
 */
static WEBRTC_STATUS peerConnectionStateChangedWrapper(uint64_t customData, webrtc_peer_state_t state)
{
    // Try to determine if this is a specific session or global config
    // Bridge mode passes global config, real WebRTC passes specific session
    PAppWebRTCSession pAppWebRTCSession = NULL;
    PSampleConfiguration pSampleConfiguration = NULL;

    // Simple heuristic: check if the pointer looks like a session structure
    // by checking if it has valid peerId field (sessions have peerId, global config doesn't)
    PAppWebRTCSession test_session = (PAppWebRTCSession)customData;
    if (test_session != NULL && test_session->peerId[0] != '\0' &&
        STRLEN(test_session->peerId) > 0 && STRLEN(test_session->peerId) < 256) {
        // Looks like a specific session
        pAppWebRTCSession = test_session;
        pSampleConfiguration = pAppWebRTCSession->pSampleConfiguration;
        ESP_LOGI(TAG, "peerConnectionStateChangedWrapper: peer=%s, state=%d",
                 pAppWebRTCSession->peerId, state);
    } else {
        // Looks like global config (bridge mode)
        pSampleConfiguration = (PSampleConfiguration)customData;
        ESP_LOGI(TAG, "peerConnectionStateChangedWrapper: bridge_mode, state=%d", state);
    }

    if (pSampleConfiguration == NULL) {
        ESP_LOGE(TAG, "peerConnectionStateChangedWrapper: Unable to determine configuration!");
        return WEBRTC_STATUS_NULL_ARG;
    }

    switch (state) {
        case WEBRTC_PEER_STATE_NEW:
        case WEBRTC_PEER_STATE_CONNECTING:
        case WEBRTC_PEER_STATE_CONNECTED:
            if (pAppWebRTCSession != NULL) {
                ESP_LOGI(TAG, "Peer connection state for %s: %d", pAppWebRTCSession->peerId, state);
            } else {
                ESP_LOGI(TAG, "Peer connection state (bridge): %d", state);
            }
            break;

        case WEBRTC_PEER_STATE_DISCONNECTED:
        case WEBRTC_PEER_STATE_FAILED:
            if (pAppWebRTCSession != NULL) {
                // FIXED: Mark only the specific session that failed, not the first one found!
                ESP_LOGI(TAG, "Peer %s failed/disconnected - marking for cleanup", pAppWebRTCSession->peerId);
                ATOMIC_STORE_BOOL(&pAppWebRTCSession->terminateFlag, TRUE);  // Mark THIS session
                CVAR_BROADCAST(pSampleConfiguration->cvar);
                ESP_LOGI(TAG, "Marked specific peer %s for termination (not affecting other peers)",
                         pAppWebRTCSession->peerId);
            } else {
                // Bridge mode - use old logic as fallback
                ESP_LOGI(TAG, "Bridge mode connection failed/disconnected");
                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                for (UINT32 i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
                    PAppWebRTCSession s = pSampleConfiguration->webrtcSessionList[i];
                    if (s != NULL && s->pPeerConnection == NULL && s->interface_session_handle != NULL) {
                        ATOMIC_STORE_BOOL(&s->terminateFlag, TRUE);
                        CVAR_BROADCAST(pSampleConfiguration->cvar);
                        ESP_LOGI(TAG, "Bridge mode: marked session %s for termination", s->peerId);
                        break;
                    }
                }
                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
            }
            break;

        case WEBRTC_PEER_STATE_MEDIA_STARTING:
            if (pAppWebRTCSession != NULL) {
                ESP_LOGI(TAG, "Media starting for peer %s (handled by kvs_webrtc interface)",
                         pAppWebRTCSession->peerId);
            } else {
                ESP_LOGI(TAG, "Media starting (bridge mode)");
            }
            break;

        default:
            if (pAppWebRTCSession != NULL) {
                ESP_LOGW(TAG, "Unknown peer connection state: %d for peer %s", state, pAppWebRTCSession->peerId);
            } else {
                ESP_LOGW(TAG, "Unknown peer connection state: %d (bridge mode)", state);
            }
            break;
    }

    return WEBRTC_STATUS_SUCCESS;
}

// Outbound peer-connection message callback: forward to bridge or signaling
static WEBRTC_STATUS peerOutboundMessageWrapper(uint64_t customData, webrtc_message_t* pWebRtcMessage)
{
    STATUS retStatus = STATUS_SUCCESS;

    if (pWebRtcMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    ESP_LOGD(TAG, "peerOutboundMessageWrapper: type=%d len=%u peer=%s",
             (int) pWebRtcMessage->message_type,
             (unsigned) pWebRtcMessage->payload_len,
             pWebRtcMessage->peer_client_id);

    // DEBUG: Log SDP answer content for comparison with legacy
    if (pWebRtcMessage->message_type == WEBRTC_MESSAGE_TYPE_ANSWER) {
        ESP_LOGD(TAG, "INTERFACE_SDP_ANSWER: len=%" PRIu32 ", corr_id=%s",
                 pWebRtcMessage->payload_len, pWebRtcMessage->correlation_id);
        ESP_LOGD(TAG, "INTERFACE_SDP_ANSWER_CONTENT: %.500s", pWebRtcMessage->payload);

        raiseEvent(APP_WEBRTC_EVENT_SENT_ANSWER, 0, pWebRtcMessage->peer_client_id, "Sent answer to peer");
    }

    // Otherwise, send directly via signaling interface (if available)
    if (gWebRtcAppConfig.signaling_client_if != NULL && gSignalingClientData != NULL &&
        gWebRtcAppConfig.signaling_client_if->send_message != NULL) {
        ESP_LOGD(TAG, "peerOutboundMessageWrapper: forwarding to signaling (type=%d, len=%u)",
                 (int) pWebRtcMessage->message_type, (unsigned) pWebRtcMessage->payload_len);
        retStatus = gWebRtcAppConfig.signaling_client_if->send_message(gSignalingClientData, pWebRtcMessage);
        return STATUS_FAILED(retStatus) ? WEBRTC_STATUS_INTERNAL_ERROR : WEBRTC_STATUS_SUCCESS;
    }

    return WEBRTC_STATUS_INVALID_OPERATION;
}

/**
 * @brief Query ICE server by index through signaling abstraction
 */
WEBRTC_STATUS app_webrtc_get_server_by_idx(int index, bool useTurn, uint8_t **data, int *len, bool *have_more)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(data != NULL && len != NULL && have_more != NULL, STATUS_NULL_ARG);
    CHK(gSignalingClientData != NULL && gWebRtcAppConfig.signaling_client_if != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.signaling_client_if->get_ice_server_by_idx != NULL, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "app_webrtc_get_server_by_idx: index=%d, useTurn=%s", index, useTurn ? "true" : "false");

    // Delegate to the signaling interface implementation
    CHK_STATUS(gWebRtcAppConfig.signaling_client_if->get_ice_server_by_idx(
        gSignalingClientData,
        index,
        useTurn,
        data,
        len,
        have_more
    ));

    ESP_LOGI(TAG, "Successfully queried ICE server by index (have_more: %s)", *have_more ? "true" : "false");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to query ICE server by index: 0x%08" PRIx32, (UINT32) retStatus);
    }

    LEAVES();
    return retStatus;
}

/**
 * @brief Check if ICE configuration refresh is needed through signaling abstraction
 */
WEBRTC_STATUS app_webrtc_is_ice_refresh_needed(bool *refreshNeeded)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(refreshNeeded != NULL, STATUS_NULL_ARG);
    CHK(gSignalingClientData != NULL && gWebRtcAppConfig.signaling_client_if != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.signaling_client_if->is_ice_refresh_needed != NULL, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "app_webrtc_is_ice_refresh_needed: checking ICE refresh status");

    // Delegate to the signaling interface implementation
    CHK_STATUS(gWebRtcAppConfig.signaling_client_if->is_ice_refresh_needed(
        gSignalingClientData,
        refreshNeeded
    ));

    ESP_LOGI(TAG, "ICE refresh check completed: refreshNeeded=%s", *refreshNeeded ? "true" : "false");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to check ICE refresh status: 0x%08" PRIx32, (UINT32) retStatus);
        if (refreshNeeded != NULL) {
            *refreshNeeded = true;  // Default to refresh needed on error
        }
    }

    LEAVES();
    return retStatus;
}

WEBRTC_STATUS app_webrtc_refresh_ice_configuration(void)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gSignalingClientData != NULL && gWebRtcAppConfig.signaling_client_if != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.signaling_client_if->refresh_ice_configuration != NULL, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "app_webrtc_refresh_ice_configuration: triggering background ICE refresh");

    // Delegate to the signaling interface implementation
    CHK_STATUS(gWebRtcAppConfig.signaling_client_if->refresh_ice_configuration(
        gSignalingClientData
    ));

    ESP_LOGI(TAG, "ICE refresh triggered successfully");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to trigger ICE refresh: 0x%08" PRIx32, (UINT32) retStatus);
    }

    LEAVES();
    return retStatus;
}

STATUS sendSignalingMessage(PAppWebRTCSession pAppWebRTCSession, webrtc_message_t* pMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(pAppWebRTCSession != NULL && pAppWebRTCSession->pSampleConfiguration != NULL && pMessage != NULL, STATUS_NULL_ARG);
    pSampleConfiguration = pAppWebRTCSession->pSampleConfiguration;

    if (gWebRtcAppConfig.signaling_client_if != NULL && gSignalingClientData != NULL) {
        // Message is already in generic format - pass directly to signaling interface
        CHK_STATUS(gWebRtcAppConfig.signaling_client_if->send_message(gSignalingClientData, pMessage));

        if (pMessage->message_type == WEBRTC_MESSAGE_TYPE_ANSWER) {
            DLOGD("Sent answer to peer %s", pMessage->peer_client_id);
        }
    } else {
        DLOGE("No signaling client interface available");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS freeAppWebRTCSession(PAppWebRTCSession* ppAppWebRTCSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    PAppWebRTCSession pAppWebRTCSession = NULL;
    PSampleConfiguration pSampleConfiguration;

    CHK(ppAppWebRTCSession != NULL, STATUS_NULL_ARG);
    pAppWebRTCSession = *ppAppWebRTCSession;
    CHK(pAppWebRTCSession != NULL && pAppWebRTCSession->pSampleConfiguration != NULL, retStatus);
    pSampleConfiguration = pAppWebRTCSession->pSampleConfiguration;

    DLOGD("Freeing WebRTC session with peer id: %s ", pAppWebRTCSession->peerId);

    ATOMIC_STORE_BOOL(&pAppWebRTCSession->terminateFlag, TRUE);

    if (pAppWebRTCSession->shutdownCallback != NULL) {
        pAppWebRTCSession->shutdownCallback(pAppWebRTCSession->shutdownCallbackCustomData, pAppWebRTCSession);
    }

    // =================================================================
    // UNIFIED SESSION CLEANUP: Interface Only
    // =================================================================

    if (pAppWebRTCSession->interface_session_handle != NULL) {
        // Interface path: Call interface destroy_session (handles stats collection internally)
        ESP_LOGI(TAG, "Cleaning up interface session for peer: %s", pAppWebRTCSession->peerId);

        webrtc_peer_connection_if_t* pc_interface = gWebRtcAppConfig.peer_connection_if;
        if (pc_interface != NULL && pc_interface->destroy_session != NULL) {
            WEBRTC_STATUS destroy_status = pc_interface->destroy_session(pAppWebRTCSession->interface_session_handle);
            if (destroy_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGW(TAG, "Failed to destroy interface session for peer %s: 0x%08x",
                         pAppWebRTCSession->peerId, destroy_status);
            } else {
                ESP_LOGI(TAG, "Successfully destroyed interface session for peer: %s", pAppWebRTCSession->peerId);
            }
        } else {
            ESP_LOGW(TAG, "No interface destroy_session function available for peer: %s", pAppWebRTCSession->peerId);
        }

        pAppWebRTCSession->interface_session_handle = NULL;
        // Mark for monitor loop to remove and decrement count
        ATOMIC_STORE_BOOL(&pAppWebRTCSession->terminateFlag, TRUE);
        CVAR_BROADCAST(pSampleConfiguration->cvar);
    } else {
        ESP_LOGW(TAG, "Session has no interface_session_handle for peer: %s", pAppWebRTCSession->peerId);
    }

    SAFE_MEMFREE(pAppWebRTCSession);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

// Configuration function without AWS credential options
/**
 * @brief Create a clean WebRTC application context
 *
 * This replaces createSampleConfiguration with a minimal, KVS-independent implementation.
 * Only essential fields needed by the app_webrtc layer are initialized.
 */
STATUS createAppWebRTCContext(bool trickleIce, bool useTurn, uint32_t logLevel, bool signaling_only,
                              papp_webrtc_context_t* ppContext)
{
    STATUS retStatus = STATUS_SUCCESS;
    papp_webrtc_context_t pContext = NULL;

    CHK(ppContext != NULL, STATUS_NULL_ARG);

    /* Allocate the context structure */
    CHK(NULL != (pContext = (papp_webrtc_context_t) MEMCALLOC(1, SIZEOF(app_webrtc_context_t))), STATUS_NOT_ENOUGH_MEMORY);

    /* === Core Configuration === */
    SET_LOGGER_LOG_LEVEL(logLevel);
    pContext->logLevel = logLevel;
    pContext->signaling_only = signaling_only;
    pContext->trickleIce = trickleIce;
    pContext->useTurn = useTurn;

    /* === Initialize Synchronization === */
    pContext->sampleConfigurationObjLock = MUTEX_CREATE(TRUE);
    pContext->cvar = CVAR_CREATE();
    pContext->streamingSessionListReadLock = MUTEX_CREATE(FALSE);
    pContext->signalingSendMessageLock = MUTEX_CREATE(FALSE);
    pContext->playerLock = MUTEX_CREATE(FALSE);

    /* === Initialize Control Flags === */
    ATOMIC_STORE_BOOL(&pContext->interrupted, FALSE);
    ATOMIC_STORE_BOOL(&pContext->mediaThreadStarted, FALSE);
    ATOMIC_STORE_BOOL(&pContext->appTerminateFlag, FALSE);
    ATOMIC_STORE_BOOL(&pContext->connected, FALSE);

    /* === Media Configuration === */
    pContext->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
    pContext->audioCodec = APP_WEBRTC_CODEC_OPUS;  /* Default audio codec */
    pContext->videoCodec = APP_WEBRTC_CODEC_H264;  /* Default video codec */
    pContext->receive_media = TRUE;

    /* === Threading === */
    pContext->audioSenderTid = INVALID_TID_VALUE;
    pContext->videoSenderTid = INVALID_TID_VALUE;
    pContext->mediaSenderTid = INVALID_TID_VALUE;

    /* === Session Management === */
    pContext->streamingSessionCount = 0;
    pContext->activePlayerSessionCount = 0;

    /* === Initialize Signaling Data Structures === */
    CHK_STATUS(stackQueueCreate(&pContext->pPendingSignalingMessageForRemoteClient));
    CHK_STATUS(hashTableCreateWithParams(SAMPLE_HASH_TABLE_BUCKET_COUNT, SAMPLE_HASH_TABLE_BUCKET_LENGTH,
                                         &pContext->pRtcPeerConnectionForRemoteClient));

    /* === Frame Handling === */
    pContext->frameIndex = 0;

    /* === Bridge Mode === */
    /* Bridge mode: Interface exists but create_session is NULL (bridge-only mode) */
    pContext->bridge_mode = (gWebRtcAppConfig.peer_connection_if != NULL &&
                            gWebRtcAppConfig.peer_connection_if->create_session == NULL);

    /* === Signaling Management === */
    ATOMIC_STORE_BOOL(&pContext->recreate_signaling_client, FALSE);
    pContext->channel_role_type = WEBRTC_CHANNEL_ROLE_TYPE_MASTER;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        freeAppWebRTCContext(&pContext);
    }

    if (ppContext != NULL) {
        *ppContext = pContext;
    }

    return retStatus;
}

// Note: getIceCandidatePairStatsCallback moved to kvs_webrtc.c where it belongs

/**
 * @brief Free WebRTC application context
 *
 * This replaces freeSampleConfiguration with a clean implementation that only
 * cleans up resources actually used by the app_webrtc layer.
 */
STATUS freeAppWebRTCContext(papp_webrtc_context_t* ppContext)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    papp_webrtc_context_t pContext;
    UINT32 i;
    UINT64 data;
    StackQueueIterator iterator;
    BOOL locked = FALSE;

    CHK(ppContext != NULL, STATUS_NULL_ARG);
    pContext = *ppContext;

    CHK(pContext != NULL, retStatus);

    /* === Free Player Resources === */
    if (IS_VALID_MUTEX_VALUE(pContext->playerLock)) {
        MUTEX_FREE(pContext->playerLock);
    }

    /* === Free Signaling Data Structures === */
    if (pContext->pPendingSignalingMessageForRemoteClient != NULL) {
        /* Iterate and free all the pending queues */
        stackQueueGetIterator(pContext->pPendingSignalingMessageForRemoteClient, &iterator);
        while (IS_VALID_ITERATOR(iterator)) {
            stackQueueIteratorGetItem(iterator, &data);
            stackQueueIteratorNext(&iterator);
            freeMessageQueue((PPendingMessageQueue) data);
        }

        stackQueueClear(pContext->pPendingSignalingMessageForRemoteClient, FALSE);
        stackQueueFree(pContext->pPendingSignalingMessageForRemoteClient);
        pContext->pPendingSignalingMessageForRemoteClient = NULL;
    }

    if (pContext->pRtcPeerConnectionForRemoteClient != NULL) {
        hashTableClear(pContext->pRtcPeerConnectionForRemoteClient);
        hashTableFree(pContext->pRtcPeerConnectionForRemoteClient);
    }

    /* === Free Sessions === */
    if (IS_VALID_MUTEX_VALUE(pContext->sampleConfigurationObjLock)) {
        MUTEX_LOCK(pContext->sampleConfigurationObjLock);
        locked = TRUE;
    }

    /* Free all active sessions and reset session count */
    UINT32 sessionCount = pContext->streamingSessionCount;
    for (i = 0; i < sessionCount; ++i) {
        if (pContext->webrtcSessionList[i] != NULL) {
            /* Sessions are freed via interface layer which handles stats collection */
            freeAppWebRTCSession(&pContext->webrtcSessionList[i]);
            pContext->webrtcSessionList[i] = NULL;
        }
    }
    pContext->streamingSessionCount = 0;

    if (locked) {
        MUTEX_UNLOCK(pContext->sampleConfigurationObjLock);
    }

    /* === Free Media Buffers === */
    SAFE_MEMFREE(pContext->pVideoFrameBuffer);
    SAFE_MEMFREE(pContext->pAudioFrameBuffer);

    /* === Synchronization Cleanup === */
    if (IS_VALID_CVAR_VALUE(pContext->cvar) && IS_VALID_MUTEX_VALUE(pContext->sampleConfigurationObjLock)) {
        CVAR_BROADCAST(pContext->cvar);
        MUTEX_LOCK(pContext->sampleConfigurationObjLock);
        MUTEX_UNLOCK(pContext->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pContext->sampleConfigurationObjLock)) {
        MUTEX_FREE(pContext->sampleConfigurationObjLock);
    }

    if (IS_VALID_MUTEX_VALUE(pContext->streamingSessionListReadLock)) {
        MUTEX_FREE(pContext->streamingSessionListReadLock);
    }

    if (IS_VALID_MUTEX_VALUE(pContext->signalingSendMessageLock)) {
        MUTEX_FREE(pContext->signalingSendMessageLock);
    }

    if (IS_VALID_CVAR_VALUE(pContext->cvar)) {
        CVAR_FREE(pContext->cvar);
    }

    /* === File Logging === */
    if (pContext->enableFileLogging) {
        /* Note: freeFileLogger() is KVS-specific and not essential for core functionality */
#ifdef KVS_FILE_LOGGER_H
        freeFileLogger();
#else
        ESP_LOGW(TAG, "KVS file logging not available, cleanup skipped");
#endif
    }

    /* === Free Context === */
    SAFE_MEMFREE(*ppContext);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS sessionCleanupWait(PSampleConfiguration pSampleConfiguration, bool isSignalingOnly)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL sampleConfigurationObjLockLocked = FALSE, streamingSessionListReadLockLocked = FALSE;
    PAppWebRTCSession pAppWebRTCSession = NULL;
    UINT32 i, clientIdHash;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    while (!ATOMIC_LOAD_BOOL(&pSampleConfiguration->appTerminateFlag)) {
        // Get the signaling client state
        MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
        sampleConfigurationObjLockLocked = TRUE;

        // Check for terminated streaming session
        for (i = 0; i < pSampleConfiguration->streamingSessionCount; ++i) {
            if (ATOMIC_LOAD_BOOL(&pSampleConfiguration->webrtcSessionList[i]->terminateFlag)) {
                pAppWebRTCSession = pSampleConfiguration->webrtcSessionList[i];

                // Remove from the hash table
                clientIdHash = COMPUTE_CRC32((PBYTE) pAppWebRTCSession->peerId, (UINT32) STRLEN(pAppWebRTCSession->peerId));
                CHK_STATUS(hashTableRemove(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash));

                MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = TRUE;

                // Remove from the array
                for (UINT32 j = i; j < pSampleConfiguration->streamingSessionCount - 1; ++j) {
                    pSampleConfiguration->webrtcSessionList[j] = pSampleConfiguration->webrtcSessionList[j + 1];
                }

                pSampleConfiguration->streamingSessionCount--;
                MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
                streamingSessionListReadLockLocked = FALSE;

                CHK_LOG_ERR(freeAppWebRTCSession(&pAppWebRTCSession));

                // Quit the for loop as we have modified the collection and the for loop iterator
                break;
            }
        }

        // Signaling reconnection should be handled by the signaling interface implementation
        // if needed for specific use cases.

        // Check if we need to re-create the signaling client on-the-fly with retry mechanism
        BOOL needsRecreate = ATOMIC_LOAD_BOOL(&pSampleConfiguration->recreate_signaling_client);
        if (needsRecreate) {
            DLOGD("recreateSignalingClient flag is TRUE, checking conditions");
        }

        if (needsRecreate &&
            gWebRtcAppConfig.signaling_client_if != NULL &&
            gSignalingClientData != NULL) {

            // Static variables to track retry attempts and timing
            static UINT32 retryCount = 0;
            static UINT64 lastRetryTime = 0;
            static UINT64 connectionStartTime = 0;
            static BOOL connectionInProgress = FALSE;
            UINT64 currentTime = GETTIME();

            // Exponential backoff: 5s, 10s, 20s, 40s, 60s (max)
            UINT64 retryDelays[] = {
                5 * HUNDREDS_OF_NANOS_IN_A_SECOND,   // 5 seconds
                10 * HUNDREDS_OF_NANOS_IN_A_SECOND,  // 10 seconds
                20 * HUNDREDS_OF_NANOS_IN_A_SECOND,  // 20 seconds
                40 * HUNDREDS_OF_NANOS_IN_A_SECOND,  // 40 seconds
                60 * HUNDREDS_OF_NANOS_IN_A_SECOND   // 60 seconds (max)
            };
            UINT32 maxRetryIndex = ARRAY_SIZE(retryDelays) - 1;
            UINT32 currentRetryIndex = MIN(retryCount, maxRetryIndex);
            UINT64 retryDelay = retryDelays[currentRetryIndex];

            // Connection timeout: 15 seconds
            const UINT64 CONNECTION_TIMEOUT = 15 * HUNDREDS_OF_NANOS_IN_A_SECOND;

            // Check for connection timeout
            if (connectionInProgress && (currentTime - connectionStartTime >= CONNECTION_TIMEOUT)) {
                DLOGE("Connection attempt timed out after %llu seconds, marking as failed",
                      CONNECTION_TIMEOUT / HUNDREDS_OF_NANOS_IN_A_SECOND);
                connectionInProgress = FALSE;
                retryCount++;
                lastRetryTime = currentTime;
                // Keep recreate_signaling_client flag TRUE to continue retrying
            }

            // Check if enough time has passed since last retry attempt
            BOOL shouldRetry = !connectionInProgress &&
                             ((lastRetryTime == 0) || (currentTime - lastRetryTime >= retryDelay));

            if (shouldRetry) {
                DLOGI("Reconnecting signaling client (attempt %d, delay: %llu seconds)",
                      retryCount + 1, retryDelay / HUNDREDS_OF_NANOS_IN_A_SECOND);

                // Disconnect and reconnect
                CHK_STATUS(gWebRtcAppConfig.signaling_client_if->disconnect(gSignalingClientData));

                // Mark connection as starting
                connectionInProgress = TRUE;
                connectionStartTime = currentTime;

                retStatus = gWebRtcAppConfig.signaling_client_if->connect(gSignalingClientData);

                if (STATUS_FAILED(retStatus)) {
                    // Immediate failure - update retry tracking
                    connectionInProgress = FALSE;
                    retryCount++;
                    lastRetryTime = currentTime;
                    DLOGE("Failed to start signaling client connection: 0x%08x (attempt %d, next retry in %llu seconds)",
                          retStatus, retryCount,
                          retryDelays[MIN(retryCount, maxRetryIndex)] / HUNDREDS_OF_NANOS_IN_A_SECOND);

                    // Reset status to avoid breaking the loop
                    retStatus = STATUS_SUCCESS;
                } else {
                    // Connection started successfully - but don't reset retry tracking yet
                    // Wait for actual connection success in the next iteration
                    DLOGI("Signaling client connection started, waiting for completion...");
                }
            } else if (connectionInProgress) {
                DLOGD("Connection in progress for %llu seconds (timeout: %llu)",
                      (currentTime - connectionStartTime) / HUNDREDS_OF_NANOS_IN_A_SECOND,
                      CONNECTION_TIMEOUT / HUNDREDS_OF_NANOS_IN_A_SECOND);
            } else {
                // Not time to retry yet
                UINT64 timeUntilNextRetry = retryDelay - (currentTime - lastRetryTime);
                DLOGD("Waiting %llu more seconds before next reconnection attempt",
                      timeUntilNextRetry / HUNDREDS_OF_NANOS_IN_A_SECOND);
            }

            // Check if connection actually succeeded by checking signaling state
            if (connectionInProgress) {
                // TODO: Add proper signaling state check here
                // For now, we rely on the timeout mechanism above
                // The connection will be marked as failed if it times out
                // and successful connection should reset the recreate_signaling_client flag
                // via the signaling state change callback
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

STATUS submitPendingIceCandidate(PPendingMessageQueue pPendingMessageQueue, PAppWebRTCSession pAppWebRTCSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL noPendingSignalingMessageForClient = FALSE;
    webrtc_message_t* pWebRtcMessage = NULL;
    UINT64 hashValue;

    CHK(pPendingMessageQueue != NULL && pPendingMessageQueue->messageQueue != NULL && pAppWebRTCSession != NULL, STATUS_NULL_ARG);

    do {
        CHK_STATUS(stackQueueIsEmpty(pPendingMessageQueue->messageQueue, &noPendingSignalingMessageForClient));
        if (!noPendingSignalingMessageForClient) {
            hashValue = 0;
            CHK_STATUS(stackQueueDequeue(pPendingMessageQueue->messageQueue, &hashValue));
            pWebRtcMessage = (webrtc_message_t*) hashValue;
            CHK(pWebRtcMessage != NULL, STATUS_INTERNAL_ERROR);
            if (pWebRtcMessage->message_type == WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE) {
                // Use peer connection interface for queued ICE candidate handling if available
                if (gWebRtcAppConfig.peer_connection_if != NULL &&
                    gWebRtcAppConfig.peer_connection_if->send_message != NULL) {
                    ESP_LOGI(TAG, "Processing queued ICE candidate via peer connection interface for peer: %s",
                             pWebRtcMessage->peer_client_id);

                    // Send message through interface using stored session handle
                    STATUS interface_status = gWebRtcAppConfig.peer_connection_if->send_message(
                        pAppWebRTCSession->interface_session_handle, pWebRtcMessage);
                    CHK_STATUS(interface_status);
                }
            }
            // Clean up the webrtc message and its payload
            if (pWebRtcMessage != NULL) {
                if (pWebRtcMessage->payload != NULL) {
                    SAFE_MEMFREE(pWebRtcMessage->payload);
                }
                SAFE_MEMFREE(pWebRtcMessage);
            }
        }
    } while (!noPendingSignalingMessageForClient);

    CHK_STATUS(freeMessageQueue(pPendingMessageQueue));

CleanUp:

    // Clean up webrtc message if not processed
    if (pWebRtcMessage != NULL) {
        if (pWebRtcMessage->payload != NULL) {
            SAFE_MEMFREE(pWebRtcMessage->payload);
        }
        SAFE_MEMFREE(pWebRtcMessage);
    }
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

/**
 * @brief Wrapper function to adapt portable signaling interface to KVS SDK format
 *
 * This function handles bridge mode fast path and validates the message
 * before calling the actual signalingMessageReceived function directly.
 */
static WEBRTC_STATUS signalingMessageReceivedWrapper(uint64_t customData, webrtc_message_t* pWebRtcMessage)
{
    STATUS kvsStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    if (pWebRtcMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // BRIDGE MODE FAST PATH: If in bridge mode, forward message directly to bridge interface
    if (pSampleConfiguration != NULL && pSampleConfiguration->bridge_mode) {
        webrtc_peer_connection_if_t* pc_interface = gWebRtcAppConfig.peer_connection_if;
        if (pc_interface != NULL && pc_interface->send_message != NULL) {
            // Forward directly to bridge - no session management needed
            WEBRTC_STATUS bridge_status = pc_interface->send_message(NULL, pWebRtcMessage);
            if (bridge_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGE("app_webrtc", "Failed to forward message to bridge: 0x%08x", bridge_status);
                return WEBRTC_STATUS_INTERNAL_ERROR;
            }

            ESP_LOGD("app_webrtc", "Message forwarded to bridge successfully");
            return WEBRTC_STATUS_SUCCESS;
        } else {
            ESP_LOGE("app_webrtc", "Bridge mode enabled but no bridge interface available");
            return WEBRTC_STATUS_INVALID_OPERATION;
        }
    }

    // Normal processing when not in bridge mode - pass message directly
    // Validate peer_id in the message - check for empty string only
    if (pWebRtcMessage->peer_client_id[0] == '\0') {
        ESP_LOGW(TAG, "Empty peer_client_id in message - this will cause issues. Using default value.");
        // Use a default peer ID to avoid crashes
        STRNCPY((PCHAR)pWebRtcMessage->peer_client_id, "default", APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
        pWebRtcMessage->peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    }

    // Call the signaling function directly with the webrtc_message_t
    kvsStatus = signalingMessageReceived((UINT64)customData, pWebRtcMessage);

    // Check for errors and log them with more details
    if (kvsStatus != STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to process %s: 0x%08" PRIx32 ", peer_id=%s",
                 pWebRtcMessage->message_type == WEBRTC_MESSAGE_TYPE_OFFER ? "offer" :
                 pWebRtcMessage->message_type == WEBRTC_MESSAGE_TYPE_ANSWER ? "answer" :
                 pWebRtcMessage->message_type == WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE ? "ICE candidate" : "unknown message",
                 kvsStatus,
                 pWebRtcMessage->peer_client_id);
    }

    // Convert STATUS to WEBRTC_STATUS
    return (kvsStatus == STATUS_SUCCESS) ? WEBRTC_STATUS_SUCCESS : WEBRTC_STATUS_INTERNAL_ERROR;
}

STATUS signalingMessageReceived(UINT64 customData, webrtc_message_t* pWebRtcMessage)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = (PSampleConfiguration) customData;

    CHK(pSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Normal processing (bridge mode is handled in wrapper, never reaches here)
    BOOL peerConnectionFound = FALSE, locked = FALSE, freeStreamingSession = FALSE;
    UINT32 clientIdHash;
    UINT64 hashValue = 0;
    PPendingMessageQueue pPendingMessageQueue = NULL;
    PAppWebRTCSession pAppWebRTCSession = NULL;
    webrtc_message_t* pWebRtcMessageCopy = NULL;

    MUTEX_LOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = TRUE;

    clientIdHash = COMPUTE_CRC32((PBYTE) pWebRtcMessage->peer_client_id,
                                 (UINT32) STRLEN(pWebRtcMessage->peer_client_id));
    CHK_STATUS(hashTableContains(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &peerConnectionFound));
    if (peerConnectionFound) {
        CHK_STATUS(hashTableGet(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, &hashValue));
        pAppWebRTCSession = (PAppWebRTCSession) hashValue;
    }

    // Declare variables once at the top of function scope to avoid redefinition errors
    webrtc_peer_connection_if_t* pc_interface = NULL;
    WEBRTC_STATUS message_status = WEBRTC_STATUS_SUCCESS;

    switch (pWebRtcMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            // Check if we already have an ongoing master session with the same peer
            CHK_ERR(!peerConnectionFound, STATUS_INVALID_OPERATION, "Peer connection %s is in progress",
                    pWebRtcMessage->peer_client_id);

            /*
             * Create new streaming session for each offer, then insert the client id and streaming session into
             * pRtcPeerConnectionForRemoteClient for subsequent ice candidate messages. Lastly check if there is
             * any ice candidate messages queued in pPendingSignalingMessageForRemoteClient. If so then submit
             * all of them.
             */

            // Check session limits (bridge mode is handled in wrapper)
            if (pSampleConfiguration->streamingSessionCount >= ARRAY_SIZE(pSampleConfiguration->webrtcSessionList)) {
                DLOGW("Max simultaneous streaming session count reached.");

                // Need to remove the pending queue if any.
                // This is a simple optimization as the session cleanup will
                // handle the cleanup of pending message queue after a while
                CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                         &pPendingMessageQueue));

                CHK(FALSE, retStatus);
            }

            // =================================================================
            // UNIFIED SESSION CREATION: Interface OR Legacy
            // =================================================================

            // Check if we have a pluggable interface or need to use legacy functions directly
            pc_interface = gWebRtcAppConfig.peer_connection_if;
            if (pc_interface == NULL) {
                ESP_LOGI("app_webrtc", "No interface provided - using direct legacy functions for incoming peer: %s",
                         pWebRtcMessage->peer_client_id);
            } else {
                ESP_LOGI("app_webrtc", "Using pluggable peer connection interface for incoming peer: %s",
                         pWebRtcMessage->peer_client_id);
            }

            // g_kvs_webrtc_client should be initialized once in app_webrtc_runTask
            if (g_kvs_webrtc_client == NULL) {
                ESP_LOGE(TAG, "Peer connection client not initialized - should have been initialized in app_webrtc_runTask");
                CHK(FALSE, STATUS_INVALID_OPERATION);
            } else {
                ESP_LOGI(TAG, "Using pre-initialized peer connection client");
            }

            // Create session - either through interface or direct legacy call
            if (pc_interface != NULL) {

                /* Progressive ICE Optimization:
                 * Trigger non-blocking ICE server refresh using progressive mechanism
                 * This gets STUN servers immediately and triggers background TURN fetching
                 */
                app_webrtc_trigger_progressive_ice("new session", true);

                // Use pluggable interface
                void* session_handle = NULL;

                // Create the session
                WEBRTC_STATUS create_status = pc_interface->create_session(
                    g_kvs_webrtc_client,
                    pWebRtcMessage->peer_client_id,
                    FALSE,  // is_initiator = FALSE for master (responder)
                    NULL,  // Use client-level data channel config
                    &session_handle);

                if (create_status != WEBRTC_STATUS_SUCCESS) {
                    ESP_LOGE(TAG, "Failed to create peer connection session via interface: 0x%08x", create_status);
                    CHK(FALSE, STATUS_INTERNAL_ERROR);
                }


                // All interfaces: create compatibility structure that wraps the interface session
                pAppWebRTCSession = (PAppWebRTCSession) MEMCALLOC(1, SIZEOF(AppWebRTCSession));
                CHK(pAppWebRTCSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

                // Initialize essential fields that legacy code expects
                pAppWebRTCSession->firstFrame = TRUE;
                pAppWebRTCSession->offerReceiveTime = GETTIME();
                STRCPY(pAppWebRTCSession->peerId, pWebRtcMessage->peer_client_id);
                ATOMIC_STORE_BOOL(&pAppWebRTCSession->peerIdReceived, TRUE);
                pAppWebRTCSession->pSampleConfiguration = pSampleConfiguration;
                // Store interface session handle for ALL interfaces
                pAppWebRTCSession->interface_session_handle = session_handle;
                // CRITICAL: Set pPeerConnection to NULL to indicate interface mode
                pAppWebRTCSession->pPeerConnection = NULL;
            }

            freeStreamingSession = TRUE;

            // Set up callbacks for the session
            if (pc_interface != NULL && pc_interface->set_callbacks != NULL) {
                WEBRTC_STATUS cb_status = pc_interface->set_callbacks(
                    pAppWebRTCSession->interface_session_handle,
                    (uint64_t) pAppWebRTCSession,  // Pass specific session, not global config
                    peerOutboundMessageWrapper,
                    peerConnectionStateChangedWrapper);
                if (cb_status != WEBRTC_STATUS_SUCCESS) {
                    ESP_LOGW(TAG, "Failed to set peer connection callbacks: 0x%08x", cb_status);
                }
            }

            // =================================================================
            // UNIFIED MESSAGE PROCESSING: Interface OR Direct Legacy
            // =================================================================

            if (pc_interface != NULL) {
                // Use pluggable interface - we can pass the message directly since it's already in the right format
                message_status = pc_interface->send_message(pAppWebRTCSession->interface_session_handle, pWebRtcMessage);
                if (message_status != WEBRTC_STATUS_SUCCESS) {
                    ESP_LOGE(TAG, "Failed to process offer via interface: 0x%08x", message_status);
                    CHK(FALSE, STATUS_INTERNAL_ERROR);
                }
            }

            ESP_LOGI(TAG, "Successfully processed offer via unified interface for peer: %s",
                     pWebRtcMessage->peer_client_id);

            CHK_STATUS(hashTablePut(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, (UINT64) pAppWebRTCSession));

            // If there are any ice candidate messages in the queue for this client id, submit them now.
            CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                     &pPendingMessageQueue));
            if (pPendingMessageQueue != NULL) {
                CHK_STATUS(submitPendingIceCandidate(pPendingMessageQueue, pAppWebRTCSession));

                // NULL the pointer to avoid it being freed in the cleanup
                pPendingMessageQueue = NULL;
            }

            MUTEX_LOCK(pSampleConfiguration->streamingSessionListReadLock);
            pSampleConfiguration->webrtcSessionList[pSampleConfiguration->streamingSessionCount++] = pAppWebRTCSession;
            MUTEX_UNLOCK(pSampleConfiguration->streamingSessionListReadLock);
            freeStreamingSession = FALSE;

            // Note: ICE stats timer now managed by kvs_webrtc.c implementation
            break;

        case WEBRTC_MESSAGE_TYPE_ANSWER:
            /*
             * For viewer, session should already exist. Use unified interface to process the answer.
             */
            pAppWebRTCSession = pSampleConfiguration->webrtcSessionList[0];
            CHK(pAppWebRTCSession != NULL, STATUS_INVALID_OPERATION);

            // =================================================================
            // UNIFIED ANSWER PROCESSING: Interface OR Direct Legacy
            // =================================================================

            // Check if we have a pluggable interface or need to use legacy functions directly
            pc_interface = gWebRtcAppConfig.peer_connection_if;

            if (pc_interface != NULL) {
                // Use pluggable interface
                ESP_LOGD(TAG, "Using pluggable peer connection interface for answer from peer: %s",
                         pWebRtcMessage->peer_client_id);

                /* Progressive ICE Optimization:
                 * Trigger non-blocking ICE server refresh for answer processing
                 */
                app_webrtc_trigger_progressive_ice("answer processing", true);

                // Use the message directly since it's already in the right format
                message_status = pc_interface->send_message(pAppWebRTCSession->interface_session_handle, pWebRtcMessage);
                if (message_status != WEBRTC_STATUS_SUCCESS) {
                    ESP_LOGE(TAG, "Failed to process answer via interface: 0x%08x", message_status);
                    CHK(FALSE, STATUS_INTERNAL_ERROR);
                }
            }

            // Note: No need to update hash table for ANSWER - session already exists from OFFER

            // If there are any ice candidate messages in the queue for this client id, submit them now.
            CHK_STATUS(getPendingMessageQueueForHash(pSampleConfiguration->pPendingSignalingMessageForRemoteClient, clientIdHash, TRUE,
                                                     &pPendingMessageQueue));
            if (pPendingMessageQueue != NULL) {
                CHK_STATUS(submitPendingIceCandidate(pPendingMessageQueue, pAppWebRTCSession));

                // NULL the pointer to avoid it being freed in the cleanup
                pPendingMessageQueue = NULL;
            }

            // Note: ICE stats timer now managed by kvs_webrtc.c implementation
            // Skip metrics collection as we're removing KVS signaling dependencies
            // The metrics would be collected by the signaling interface if needed
            DLOGP("[Signaling offer sent to answer received]");
            break;

        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
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

                pWebRtcMessageCopy = (webrtc_message_t*) MEMCALLOC(1, SIZEOF(webrtc_message_t));
                CHK(pWebRtcMessageCopy != NULL, STATUS_NOT_ENOUGH_MEMORY);

                // Copy the webrtc_message_t structure and handle payload appropriately
                // Copy all fields except payload
                pWebRtcMessageCopy->version = pWebRtcMessage->version;
                pWebRtcMessageCopy->message_type = pWebRtcMessage->message_type;
                STRNCPY(pWebRtcMessageCopy->correlation_id, pWebRtcMessage->correlation_id, APP_WEBRTC_MAX_CORRELATION_ID_LEN);
                pWebRtcMessageCopy->correlation_id[APP_WEBRTC_MAX_CORRELATION_ID_LEN] = '\0';
                STRNCPY(pWebRtcMessageCopy->peer_client_id, pWebRtcMessage->peer_client_id, APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
                pWebRtcMessageCopy->peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
                pWebRtcMessageCopy->payload_len = pWebRtcMessage->payload_len;

                // Handle payload copying - always allocate separate memory for queue storage
                if (pWebRtcMessage->payload_len > 0 && pWebRtcMessage->payload != NULL) {
                    pWebRtcMessageCopy->payload = (PCHAR) MEMALLOC(pWebRtcMessage->payload_len + 1);
                    CHK(pWebRtcMessageCopy->payload != NULL, STATUS_NOT_ENOUGH_MEMORY);
                    MEMCPY(pWebRtcMessageCopy->payload, pWebRtcMessage->payload, pWebRtcMessage->payload_len);
                    pWebRtcMessageCopy->payload[pWebRtcMessage->payload_len] = '\0';  // Null terminate
                } else {
                    pWebRtcMessageCopy->payload = NULL;
                }

                CHK_STATUS(stackQueueEnqueue(pPendingMessageQueue->messageQueue, (UINT64) pWebRtcMessageCopy));

                // NULL the pointers to not free any longer
                pPendingMessageQueue = NULL;
                pWebRtcMessageCopy = NULL;
            } else {
                // =================================================================
                // UNIFIED ICE CANDIDATE PROCESSING: Interface OR Direct Legacy
                // =================================================================

                // Check if we have a pluggable interface or need to use legacy functions directly
                pc_interface = gWebRtcAppConfig.peer_connection_if;

                if (pc_interface != NULL) {
                    // Use pluggable interface
                    ESP_LOGD("app_webrtc", "Using pluggable peer connection interface for ICE candidate from peer: %s",
                             pWebRtcMessage->peer_client_id);

                    // For interface calls: pass the session that the interface expects
                    // If we have interface_session_handle, use it; otherwise pass the session directly
                    void* session_to_pass = pAppWebRTCSession;
                    if (pAppWebRTCSession->interface_session_handle != NULL) {
                        session_to_pass = pAppWebRTCSession->interface_session_handle;
                    }
                    // Use the message directly since it's already in the right format
                    message_status = pc_interface->send_message(session_to_pass, pWebRtcMessage);
                    if (message_status != WEBRTC_STATUS_SUCCESS) {
                        ESP_LOGE("app_webrtc", "Failed to process ICE candidate via interface: 0x%08x", message_status);
                        CHK(FALSE, STATUS_INTERNAL_ERROR);
                    }
                }
            }
            break;

        default:
            DLOGD("Unhandled signaling message type %u", pWebRtcMessage->message_type);
            break;
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    // Note: ICE stats timer management moved to kvs_webrtc.c where it belongs with the KVS implementation

CleanUp:

    // Clean up webrtc message copy if allocated
    if (pWebRtcMessageCopy != NULL) {
        // Free the payload if allocated separately
        if (pWebRtcMessageCopy->payload != NULL) {
            SAFE_MEMFREE(pWebRtcMessageCopy->payload);
        }
        // Free the message itself
        SAFE_MEMFREE(pWebRtcMessageCopy);
    }

    if (pPendingMessageQueue != NULL) {
        freeMessageQueue(pPendingMessageQueue);
    }

    if (freeStreamingSession && pAppWebRTCSession != NULL) {
        freeAppWebRTCSession(&pAppWebRTCSession);
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
WEBRTC_STATUS app_webrtc_init(app_webrtc_config_t *config)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;

    CHK(config != NULL, STATUS_NULL_ARG);
    CHK(config->signaling_client_if != NULL, STATUS_NULL_ARG);
    CHK(!gapp_webrtc_initialized, STATUS_INVALID_OPERATION);

    // Initialize flash wrapper first
    retStatus = flash_wrapper_init();
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to initialize flash wrapper");
        goto CleanUp;
    }

    // Initialize and run the work queue if not already done
    esp_work_queue_config_t work_queue_config = ESP_WORK_QUEUE_CONFIG_DEFAULT();
    work_queue_config.stack_size = 32 * 1024;

    if (esp_work_queue_init_with_config(&work_queue_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize work queue");
        retStatus = STATUS_INTERNAL_ERROR;
        goto CleanUp;
    }

    if (esp_work_queue_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start work queue");
        retStatus = STATUS_INTERNAL_ERROR;
        goto CleanUp;
    }

    // Store the user config in our global structure
    MEMCPY(&gWebRtcAppConfig, config, SIZEOF(app_webrtc_config_t));

    // Apply reasonable defaults for configuration not provided by user
    BOOL trickle_ice = TRUE;  // Always enabled for faster connection setup
    BOOL use_turn = TRUE;     // Always enabled for better NAT traversal
    UINT32 log_level = LOG_LEVEL_INFO;  // INFO level - good balance of information (can be changed via app_webrtc_set_log_level)
    webrtc_channel_role_type_t role_type = WEBRTC_CHANNEL_ROLE_TYPE_MASTER;  // Most common case for IoT devices

    DLOGI("WebRTC app initializing with reasonable defaults:");
    DLOGI("  - Role: %s", role_type == WEBRTC_CHANNEL_ROLE_TYPE_MASTER ? "MASTER" : "VIEWER");
    DLOGI("  - Trickle ICE: %s", trickle_ice ? "enabled" : "disabled");
    DLOGI("  - TURN servers: %s", use_turn ? "enabled" : "disabled");
    DLOGI("  - Log level: %d (INFO, can be changed via app_webrtc_set_log_level)", log_level);

    // Validate peer connection interface
    if (config->peer_connection_if == NULL) {
        DLOGE("Cannot initialize WebRTC - peer_connection_if is required");
        DLOGE("You must provide a valid peer_connection_if in app_webrtc_config_t");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    }

    DLOGI("WebRTC initialization will be handled by peer_connection_if in app_webrtc_runTask");

    // Initialize the event callback mutex if not already done
    if (!IS_VALID_MUTEX_VALUE(gEventCallbackLock)) {
        gEventCallbackLock = MUTEX_CREATE(FALSE);
    }

    // Create the sample configuration with reasonable defaults
    // We don't need signaling_only flag anymore as we're using the peer_connection_if
    BOOL signaling_only = FALSE; // This will be ignored by the implementation
    CHK_STATUS(createAppWebRTCContext(trickle_ice, use_turn, log_level, signaling_only, &pSampleConfiguration));

    // Store the sample configuration for later use
    gSampleConfiguration = pSampleConfiguration;

    // Set default role to MASTER (most common for IoT devices)
    // Note: This can be overridden later using app_webrtc_set_role() advanced API
    pSampleConfiguration->channel_role_type = WEBRTC_CHANNEL_ROLE_TYPE_MASTER;
    DLOGI("Default role set to: MASTER (can be changed via app_webrtc_set_role)");

    // Apply reasonable defaults for media configuration
    pSampleConfiguration->audioCodec = APP_WEBRTC_CODEC_OPUS;  // Most common audio codec
    pSampleConfiguration->videoCodec = APP_WEBRTC_CODEC_H264;  // Most common video codec

    // Auto-detect media type based on provided interfaces
    if (config->video_capture != NULL && config->audio_capture != NULL) {
        pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
        DLOGI("Media type: audio+video (auto-detected)");
    } else if (config->video_capture != NULL) {
        pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_VIDEO;
        DLOGI("Media type: video-only (auto-detected)");
    } else if (config->audio_capture != NULL) {
        // Note: There's no APP_WEBRTC_MEDIA_AUDIO_ONLY, so fallback to audio+video
        pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
        DLOGI("Media type: audio+video (fallback for audio-only)");
    } else {
        pSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;  // Default for signaling-only
        DLOGI("Media type: audio+video (default for signaling-only)");
    }

    // Configure media capture interfaces if provided
    if (config->video_capture != NULL) {
        pSampleConfiguration->video_capture = config->video_capture;
        DLOGI("Video capture interface configured");
    }

    if (config->audio_capture != NULL) {
        pSampleConfiguration->audio_capture = config->audio_capture;
        DLOGI("Audio capture interface configured");
    }

    // Configure media player interfaces if provided
    if (config->video_player != NULL) {
        pSampleConfiguration->video_player = config->video_player;
        DLOGI("Video player interface configured");
    }

    if (config->audio_player != NULL) {
        pSampleConfiguration->audio_player = config->audio_player;
        DLOGI("Audio player interface configured");
    }

    // Register our event handler with the peer connection interface if available
    if (config->peer_connection_if != NULL) {
        // The register_event_handler function might not be implemented in all interfaces
        // so we need to check if it exists before calling it
        WEBRTC_STATUS (*register_event_handler_fn)(void *, void (*)(app_webrtc_event_t, UINT32, PCHAR, PCHAR)) = NULL;

        // Get the function pointer from the interface struct
        register_event_handler_fn = (WEBRTC_STATUS (*)(void *, void (*)(app_webrtc_event_t, UINT32, PCHAR, PCHAR)))
            config->peer_connection_if->register_event_handler;

        if (register_event_handler_fn != NULL) {
            DLOGI("Registering event handler with peer connection interface");
            if (STATUS_FAILED(register_event_handler_fn(
                    (void*)pSampleConfiguration, raiseEvent))) {
                DLOGE("Failed to register event handler with peer connection interface");
                // Non-fatal error, continue initialization
            }
        }
    }

    // Default: disable media reception (most IoT devices are senders)
    pSampleConfiguration->receive_media = FALSE;

    // Set the WebRTC app as initialized
    gapp_webrtc_initialized = TRUE;

    // Raise the initialized event
    raiseEvent(APP_WEBRTC_EVENT_INITIALIZED, STATUS_SUCCESS, NULL, "WebRTC app initialized with reasonable defaults");

    DLOGI("WebRTC app initialized successfully with simplified configuration");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to initialize WebRTC app with status 0x%08x", retStatus);
        if (pSampleConfiguration != NULL) {
            freeAppWebRTCContext(&pSampleConfiguration);
        }
    }

    LEAVES();
    return retStatus;
}

/**
 * @brief Task function to run the WebRTC application
 * This task handles the WebRTC application main loop
 */
static void app_webrtc_runTask(void *pvParameters)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    ESP_LOGD(TAG, "Running WebRTC application in task");

    // Initialize peer connection interface if provided
    if (gWebRtcAppConfig.peer_connection_if != NULL) {
        // Detect if this is a bridge-only interface (create_session == NULL)
        gSampleConfiguration->bridge_mode = (gWebRtcAppConfig.peer_connection_if->create_session == NULL);
        if (gSampleConfiguration->bridge_mode) {
            ESP_LOGD(TAG, "Bridge-only mode detected (no real WebRTC sessions will be created)");
            // Set the callbacks to the bridge interface
            // NOTE: For bridge mode, we still use global config since there are no specific sessions
            gWebRtcAppConfig.peer_connection_if->set_callbacks(
                NULL, // No session_handle for bridge mode
                (uint64_t) gSampleConfiguration, // customData - keep global for bridge mode
                peerOutboundMessageWrapper, // From peer connection interface
                peerConnectionStateChangedWrapper); // From peer connection interface
        } else {
            ESP_LOGD(TAG, "Real WebRTC interface detected");
        }

        ESP_LOGD(TAG, "Initializing peer connection client via interface");

        // Create a generic peer connection config that includes media interfaces
        static webrtc_peer_connection_config_t generic_config = {0};

        // Pass media interfaces from app_webrtc
        generic_config.video_capture = gWebRtcAppConfig.video_capture;
        generic_config.audio_capture = gWebRtcAppConfig.audio_capture;
        generic_config.video_player = gWebRtcAppConfig.video_player;
        generic_config.audio_player = gWebRtcAppConfig.audio_player;

        // Pass codec configuration from app_webrtc (no conversion needed)
        generic_config.audio_codec = gSampleConfiguration->audioCodec;
        generic_config.video_codec = gSampleConfiguration->videoCodec;

        // Pass data channel configuration from app_webrtc
        generic_config.data_channel_config = gWebRtcAppConfig.data_channel_config;

        // Pass the implementation-specific config as opaque pointer
        generic_config.peer_connection_cfg = gWebRtcAppConfig.implementation_config;

        ESP_LOGD(TAG, "Created generic peer connection config with media interfaces");

        // Initialize the peer connection client
        WEBRTC_STATUS init_status = gWebRtcAppConfig.peer_connection_if->init(&generic_config, &g_kvs_webrtc_client);

        if (init_status != WEBRTC_STATUS_SUCCESS) {
            DLOGE("Failed to initialize peer connection client: 0x%08x", init_status);
            CHK(FALSE, STATUS_INTERNAL_ERROR);
        }

        ESP_LOGD(TAG, "Peer connection client initialized successfully via interface");

        // Apply early data channel callbacks if registered
        if (g_early_data_channel_callbacks.callbacks_registered &&
            gWebRtcAppConfig.peer_connection_if->set_data_channel_callbacks != NULL) {

            // Set the callbacks using the peer connection interface
            WEBRTC_STATUS cbStatus = gWebRtcAppConfig.peer_connection_if->set_data_channel_callbacks(
                                            g_kvs_webrtc_client,
                                            g_early_data_channel_callbacks.onOpen,
                                            g_early_data_channel_callbacks.onMessage,
                                            g_early_data_channel_callbacks.custom_data
                                        );
            if (STATUS_SUCCEEDED(cbStatus)) {
                DLOGI("Successfully applied early data channel callbacks");
            } else {
                DLOGW("Failed to apply early data channel callbacks: 0x%08x", cbStatus);
            }
        }
    } else {
        // No peer connection interface provided - assume legacy mode (real WebRTC)
        gSampleConfiguration->bridge_mode = FALSE;
        DLOGI("No peer connection interface provided - skipping peer connection initialization");
    }

    // Initialize signaling client first, then set ICE servers
    if (gWebRtcAppConfig.signaling_client_if != NULL && gWebRtcAppConfig.signaling_cfg != NULL) {
        DLOGD("Initializing signaling client using interface");

        // Initialize the signaling client using the interface and opaque config
        retStatus = gWebRtcAppConfig.signaling_client_if->init(
            gWebRtcAppConfig.signaling_cfg,
            &gSignalingClientData);

        if (STATUS_FAILED(retStatus)) {
            DLOGE("Failed to initialize signaling client: 0x%08x", retStatus);
            CHK(FALSE, retStatus);
        }

        DLOGI("Signaling client initialized successfully");

        // Set the role type for the signaling client
        if (gWebRtcAppConfig.signaling_client_if->set_role_type != NULL) {
            DLOGI("Setting signaling client role type: %d", gSampleConfiguration->channel_role_type);
            retStatus = gWebRtcAppConfig.signaling_client_if->set_role_type(
                gSignalingClientData,
                gSampleConfiguration->channel_role_type);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to set signaling client role type: 0x%08x", retStatus);
                CHK(FALSE, retStatus);
            }
        }

        // Set up callbacks
        if (gWebRtcAppConfig.signaling_client_if->set_callbacks != NULL) {
            DLOGI("Setting up signaling callbacks");
            retStatus = gWebRtcAppConfig.signaling_client_if->set_callbacks(
                gSignalingClientData,
                (uint64_t) gSampleConfiguration,
                signalingMessageReceivedWrapper,
                signalingClientStateChangedWrapper,
                signalingClientErrorWrapper);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to set signaling callbacks: 0x%08x", retStatus);
                CHK(FALSE, retStatus);
            }
        }

        // Set up progressive ICE server update callback if supported
        if (gWebRtcAppConfig.signaling_client_if->set_ice_update_callback != NULL) {
            DLOGI("Setting up progressive ICE update callback");
            retStatus = gWebRtcAppConfig.signaling_client_if->set_ice_update_callback(
                gSignalingClientData,
                (uint64_t) gSampleConfiguration,
                app_webrtc_on_ice_servers_updated);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Failed to set ICE update callback: 0x%08x", retStatus);
                // Non-fatal error - progressive ICE is optional
                DLOGW("Progressive ICE callback setup failed, falling back to traditional mode");
            } else {
                DLOGD("Progressive ICE callback registered successfully");
            }
        } else {
            DLOGW("Progressive ICE updates not supported by signaling interface");
        }

        // Connect the signaling client
        if (gWebRtcAppConfig.signaling_client_if->connect != NULL) {
            DLOGD("Connecting signaling client");
            retStatus = gWebRtcAppConfig.signaling_client_if->connect(gSignalingClientData);

            if (STATUS_FAILED(retStatus)) {
                DLOGE("Initial signaling client connection failed: 0x%08x", retStatus);
                DLOGI("Connection failure will be handled by retry logic in sessionCleanupWait");

                // Set the recreate flag to trigger retry logic in sessionCleanupWait
                ATOMIC_STORE_BOOL(&gSampleConfiguration->recreate_signaling_client, TRUE);

                // Reset status to continue to sessionCleanupWait - don't treat initial connection failure as fatal
                retStatus = STATUS_SUCCESS;
            } else {
                ESP_LOGD(TAG, "Initial signaling client connection successful");
            }
        }
    } else {
        DLOGI("No signaling client interface or config provided, running in streaming-only mode");
    }

    // Wait for termination
    sessionCleanupWait(gSampleConfiguration, FALSE);
    DLOGI("WebRTC app terminated");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app run failed with status 0x%08x", retStatus);
        raiseEvent(APP_WEBRTC_EVENT_ERROR, retStatus, NULL, "WebRTC application run failed");
    }

    // Terminate WebRTC application
    app_webrtc_terminate();
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
WEBRTC_STATUS app_webrtc_run(void)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, WEBRTC_STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, WEBRTC_STATUS_INTERNAL_ERROR);

    DLOGI("WebRTC app running");

    // Check if task is already running
    if (gWebRtcRunTaskHandle != NULL) {
        DLOGI("WebRTC task is already running");
        CHK(FALSE, WEBRTC_STATUS_INVALID_OPERATION);
    }

    // Create a task to run the WebRTC application
    // Use a higher stack size for the WebRTC task as signaling requires substantial stack
    DLOGI("Creating WebRTC run task");

#define WEBRTC_TASK_STACK_SIZE     (16 * 1024)
#define WEBRTC_TASK_PRIO           5
    static StaticTask_t *task_buffer = NULL;
    static void *task_stack = NULL;

    /* Check if we need to allocate or reuse existing buffers */
    if (task_buffer == NULL) {
        task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    }

    if (task_stack == NULL) {
        task_stack = heap_caps_calloc_prefer(1, WEBRTC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL);
    }

    if (!task_buffer || !task_stack) {
        DLOGE("Failed to allocate task buffers");
        CHK(FALSE, WEBRTC_STATUS_NOT_ENOUGH_MEMORY);
    }

    gWebRtcRunTaskHandle = xTaskCreateStatic(
        app_webrtc_runTask,
        "webrtc_run",
        WEBRTC_TASK_STACK_SIZE,
        NULL,
        WEBRTC_TASK_PRIO,
        task_stack,
        task_buffer
    );

    if (gWebRtcRunTaskHandle == NULL) {
        DLOGE("Failed to create WebRTC run task");
        CHK(FALSE, WEBRTC_STATUS_INTERNAL_ERROR);
    } else {
        DLOGI("WebRTC run task created successfully");
    }

CleanUp:
    if (WEBRTC_STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app run failed with status 0x%08x", retStatus);
    }

    LEAVES();
    // Convert STATUS to WEBRTC_STATUS for public API consistency
    return retStatus;
}

/**
 * @brief Terminate the WebRTC application
 */
WEBRTC_STATUS app_webrtc_terminate(void)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, WEBRTC_STATUS_INVALID_OPERATION);

    if (gSampleConfiguration != NULL) {
        // Kick off the termination sequence
        ATOMIC_STORE_BOOL(&gSampleConfiguration->appTerminateFlag, TRUE);

        // Disconnect signaling client if available
        if (gWebRtcAppConfig.signaling_client_if != NULL &&
            gSignalingClientData != NULL &&
            gWebRtcAppConfig.signaling_client_if->disconnect != NULL) {

            DLOGI("Disconnecting signaling client");
            retStatus = gWebRtcAppConfig.signaling_client_if->disconnect(gSignalingClientData);

            if (WEBRTC_STATUS_FAILED(retStatus)) {
                DLOGW("Failed to disconnect signaling client: 0x%08x", retStatus);
                // Continue with termination even if disconnect fails
                retStatus = WEBRTC_STATUS_SUCCESS;
            } else {
                DLOGI("Signaling client disconnected successfully");
            }
        }

        // Free the peer connection client if available
        if (g_kvs_webrtc_client != NULL) {
            webrtc_peer_connection_if_t* pc_interface = gWebRtcAppConfig.peer_connection_if;

            if (pc_interface != NULL && pc_interface->free != NULL) {
                // Use interface to free the client
                DLOGI("Freeing peer connection client via interface");
                WEBRTC_STATUS free_status = pc_interface->free(g_kvs_webrtc_client);

                if (free_status != WEBRTC_STATUS_SUCCESS) {
                    DLOGW("Failed to free peer connection client via interface: 0x%08x", free_status);
                } else {
                    DLOGI("Peer connection client freed successfully via interface");
                }
            }

            g_kvs_webrtc_client = NULL;
        }

        // Free signaling client if available
        if (gWebRtcAppConfig.signaling_client_if != NULL &&
            gSignalingClientData != NULL &&
            gWebRtcAppConfig.signaling_client_if->free != NULL) {

            DLOGI("Freeing signaling client");
            retStatus = gWebRtcAppConfig.signaling_client_if->free(gSignalingClientData);

            if (WEBRTC_STATUS_FAILED(retStatus)) {
                DLOGW("Failed to free signaling client: 0x%08x", retStatus);
                // Continue with termination even if free fails
                retStatus = WEBRTC_STATUS_SUCCESS;
            } else {
                DLOGI("Signaling client freed successfully");
            }
        }

        // Free sample configuration
        freeAppWebRTCContext(&gSampleConfiguration);
    }

    // Reset state
    gapp_webrtc_initialized = FALSE;
    gSignalingClientData = NULL;
    MEMSET(&gWebRtcAppConfig, 0, SIZEOF(app_webrtc_config_t));

    DLOGI("WebRTC app terminated successfully");

CleanUp:
    if (WEBRTC_STATUS_FAILED(retStatus)) {
        DLOGE("WebRTC app termination failed with status 0x%08x", retStatus);
    }

    LEAVES();
    // Convert STATUS to WEBRTC_STATUS for public API consistency
    return retStatus;
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
int app_webrtc_trigger_offer(char *pPeerId)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSampleConfiguration pSampleConfiguration = NULL;
    PAppWebRTCSession pAppWebRTCSession = NULL;
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
        pAppWebRTCSession = (PAppWebRTCSession) hashValue;
    } else {
        // Create new streaming session - check limits (bridge mode handled in wrapper)
        if (pSampleConfiguration->streamingSessionCount >= ARRAY_SIZE(pSampleConfiguration->webrtcSessionList)) {
            ESP_LOGE(TAG, "Max streaming sessions reached");
            CHK(FALSE, STATUS_INVALID_OPERATION);
        }

        // =================================================================
        // UNIFIED SESSION CREATION FOR TRIGGER OFFER: Interface OR Legacy
        // =================================================================

        // Determine which peer connection interface to use
        webrtc_peer_connection_if_t* pc_interface = gWebRtcAppConfig.peer_connection_if;
        BOOL use_direct_legacy = (pc_interface == NULL);

        // g_kvs_webrtc_client should be initialized once in app_webrtc_runTask
        if (g_kvs_webrtc_client == NULL) {
            ESP_LOGE(TAG, "Peer connection client not initialized - should have been initialized in app_webrtc_runTask");
            CHK(FALSE, STATUS_INVALID_OPERATION);
        } else if (!use_direct_legacy) {
            ESP_LOGI(TAG, "Using pre-initialized peer connection client for trigger offer");
        }

        /* Progressive ICE Optimization:
         * Trigger non-blocking ICE server refresh for offer creation
         */
        app_webrtc_trigger_progressive_ice("offer creation", true);

        // Use pluggable interface
        void* session_handle = NULL;
        // Create the session
        WEBRTC_STATUS create_status = pc_interface->create_session(
            g_kvs_webrtc_client,
            pPeerId,
            TRUE,  // is_initiator = TRUE for viewer (initiator)
            NULL,  // Use client-level data channel config
            &session_handle);

        if (create_status != WEBRTC_STATUS_SUCCESS) {
            ESP_LOGE("app_webrtc", "Failed to create peer connection session: 0x%08x", create_status);
            CHK(FALSE, STATUS_INTERNAL_ERROR);
        }

        // All interfaces: create compatibility structure that wraps the interface session
        pAppWebRTCSession = (PAppWebRTCSession) MEMCALLOC(1, SIZEOF(AppWebRTCSession));
            CHK(pAppWebRTCSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

        // Initialize essential fields that legacy code expects
        pAppWebRTCSession->firstFrame = TRUE;
        pAppWebRTCSession->offerReceiveTime = GETTIME();
        STRCPY(pAppWebRTCSession->peerId, pPeerId);
        ATOMIC_STORE_BOOL(&pAppWebRTCSession->peerIdReceived, TRUE);
        pAppWebRTCSession->pSampleConfiguration = pSampleConfiguration;

        // Store interface session handle for ALL interfaces
        pAppWebRTCSession->interface_session_handle = session_handle;
        // CRITICAL: Set pPeerConnection to NULL to indicate interface mode
        pAppWebRTCSession->pPeerConnection = NULL;

        // Wire callbacks through unified interface (only for interface mode)
        if (pc_interface->set_callbacks != NULL) {
            WEBRTC_STATUS cb_status = pc_interface->set_callbacks(
                pAppWebRTCSession->interface_session_handle,
                (uint64_t) pAppWebRTCSession,  // Pass specific session, not global config
                peerOutboundMessageWrapper,
                peerConnectionStateChangedWrapper);
            if (cb_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGW("app_webrtc", "Failed to set peer connection callbacks: 0x%08x", cb_status);
            }
        }

        // Add to the list and hash table
        pSampleConfiguration->webrtcSessionList[pSampleConfiguration->streamingSessionCount++] = pAppWebRTCSession;

        CHK_STATUS(hashTablePut(pSampleConfiguration->pRtcPeerConnectionForRemoteClient, clientIdHash, (UINT64) pAppWebRTCSession));
    }

    MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    locked = FALSE;

    // =================================================================
    // UNIFIED OFFER CREATION: Interface OR Legacy
    // =================================================================

    webrtc_peer_connection_if_t* pc_interface = gWebRtcAppConfig.peer_connection_if;

    if (pc_interface == NULL) {
        ESP_LOGE(TAG, "Direct legacy path requested but APP_WEBRTC_ENABLE_LEGACY_FALLBACKS is not enabled");
        ESP_LOGE(TAG, "You must provide a valid peer_connection_if in app_webrtc_config_t");
        CHK(FALSE, STATUS_INVALID_OPERATION);
    } else {
        // Interface path: Call interface to create session as initiator, which will auto-generate offer
        ESP_LOGI(TAG, "Creating offer using peer connection interface");

        // Check if session already exists for this peer
        if (pAppWebRTCSession->interface_session_handle != NULL) {
            ESP_LOGI(TAG, "Session already exists, using existing session");

            // Don't create a new session - this causes duplicate offers
            // Instead, use the existing session and just set the callbacks if needed

            // Verify message callback is properly set
            ESP_LOGI(TAG, "Using existing session with handle: %p", (void *)pAppWebRTCSession->interface_session_handle);

            // We should NOT trigger a new offer here - this is causing duplicate offers
            // The apprtc_signaling is already calling this function when it needs to send an offer
            // If we trigger another offer here, we'll end up with multiple offers being sent
            ESP_LOGI(TAG, "Skipping explicit offer creation to avoid duplicates");

            // Just raise the event to indicate we're using the existing session
            raiseEvent(APP_WEBRTC_EVENT_SENT_OFFER, 0, pPeerId, "Using existing session");
        } else {
            ESP_LOGI(TAG, "Creating new session as initiator, will auto-generate offer");

            // Create session as initiator - this will automatically generate and send offer
            void* session_handle = NULL;
            WEBRTC_STATUS create_status = pc_interface->create_session(
                g_kvs_webrtc_client, pPeerId, TRUE, NULL, &session_handle);  // TRUE = is_initiator, NULL = use client config

            if (create_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Failed to create initiator session via interface: 0x%08" PRIx32, (UINT32) create_status);
                CHK(FALSE, STATUS_INTERNAL_ERROR);
            }

            // Store the session handle
            pAppWebRTCSession->interface_session_handle = session_handle;

            // Set callbacks for this session - IMPORTANT: This must happen BEFORE auto-offer generation
            ESP_LOGI(TAG, "Setting callbacks for session with pAppWebRTCSession=%p", (void *)pAppWebRTCSession);
            WEBRTC_STATUS cb_status = pc_interface->set_callbacks(session_handle, (uint64_t)pAppWebRTCSession,
                                                  peerOutboundMessageWrapper, peerConnectionStateChangedWrapper);
            if (cb_status != WEBRTC_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Failed to set session callbacks: 0x%08" PRIx32, (UINT32) cb_status);
                CHK(FALSE, STATUS_INTERNAL_ERROR);
            }

            ESP_LOGI(TAG, "Initiator session created, offer should be auto-generated");

            // The KVS WebRTC implementation will automatically generate and send an offer
            // after the callbacks are set, so we don't need to explicitly call createOffer here

            // Verify message callback is properly set
            ESP_LOGI(TAG, "Message callback registered: %p", (void *) peerOutboundMessageWrapper);
            raiseEvent(APP_WEBRTC_EVENT_SENT_OFFER, 0, pPeerId, "Triggered offer creation via interface");
        }
    }

    ESP_LOGI(TAG, "Offer sent successfully to peer: %s", pPeerId);

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSampleConfiguration->sampleConfigurationObjLock);
    }

    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to create and send offer: 0x%08" PRIx32, (UINT32) retStatus);
    }

    return (int) retStatus;
}

/**
 * @brief Get ICE servers configuration from the WebRTC application
 */
WEBRTC_STATUS app_webrtc_get_ice_servers(PUINT32 pIceServerCount, PVOID pIceConfiguration)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    app_webrtc_ice_servers_payload_t ice_config;

    CHK(pIceServerCount != NULL && pIceConfiguration != NULL, STATUS_NULL_ARG);
    CHK(gSignalingClientData != NULL && gWebRtcAppConfig.signaling_client_if != NULL, STATUS_INVALID_OPERATION);

    // Initialize configuration
    MEMSET(&ice_config, 0x00, SIZEOF(app_webrtc_ice_servers_payload_t));

    // Get ICE servers from signaling client interface
    if (gWebRtcAppConfig.signaling_client_if->get_ice_servers != NULL) {
        // Pass the ice_servers array directly to keep the interface generic
        CHK_STATUS(gWebRtcAppConfig.signaling_client_if->get_ice_servers(
            gSignalingClientData,
            pIceServerCount,
            ice_config.ice_servers));  // Use app_webrtc ice_servers array
    } else {
        ESP_LOGW(TAG, "No get_ice_servers function available in signaling interface");
        *pIceServerCount = 0;
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Copy the ICE servers array to the output buffer
    // Caller expects app_webrtc_ice_server_t array format
    if (*pIceServerCount > 0) {
        ice_config.ice_server_count = *pIceServerCount;
        MEMCPY(pIceConfiguration, &ice_config.ice_servers, sizeof(ice_config.ice_servers));
        ESP_LOGI(TAG, "Retrieved %" PRIu32 " ICE servers for bridge forwarding", *pIceServerCount);

        // Log the ICE servers for debugging
        for (int i = 0; i < (int)*pIceServerCount; i++) {
            ESP_LOGI(TAG, "ICE Server %d: %s", i, ice_config.ice_servers[i].urls);
        }
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to get ICE servers: 0x%08" PRIx32, (UINT32) retStatus);
        if (pIceServerCount != NULL) {
            *pIceServerCount = 0;
        }
    }

    LEAVES();
    return retStatus;
}

/********************************************************************************
 *                      Advanced Configuration APIs Implementation               *
 ********************************************************************************/

WEBRTC_STATUS app_webrtc_set_role(webrtc_channel_role_type_t role)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_NULL_ARG);

    // Validate role type
    switch (role) {
        case WEBRTC_CHANNEL_ROLE_TYPE_MASTER:
            DLOGI("Setting WebRTC role to: MASTER");
            break;
        case WEBRTC_CHANNEL_ROLE_TYPE_VIEWER:
            DLOGI("Setting WebRTC role to: VIEWER");
            break;
        default:
            DLOGE("Invalid role type: %d", role);
            CHK(FALSE, STATUS_INVALID_ARG);
    }

    // Update the channel role type directly (no conversion needed)
    gSampleConfiguration->channel_role_type = role;

CleanUp:
    return retStatus;
}

WEBRTC_STATUS app_webrtc_set_ice_config(bool trickle_ice, bool use_turn)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_NULL_ARG);

    DLOGI("Setting ICE configuration: trickle_ice=%s, use_turn=%s",
          trickle_ice ? "enabled" : "disabled",
          use_turn ? "enabled" : "disabled");

    gSampleConfiguration->trickleIce = trickle_ice;
    gSampleConfiguration->useTurn = use_turn;

CleanUp:
    return retStatus;
}

WEBRTC_STATUS app_webrtc_set_log_level(uint32_t level)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(level <= 8, STATUS_INVALID_ARG);  // Valid levels: 0-8

    DLOGI("Setting log level to: %d", level);

    // Directly set the log level using kvs_utils logger
    SET_LOGGER_LOG_LEVEL(level);

    // Update in sample configuration if it exists
    if (gSampleConfiguration != NULL) {
        gSampleConfiguration->logLevel = level;
    }

CleanUp:
    return retStatus;
}

WEBRTC_STATUS app_webrtc_set_codecs(app_webrtc_rtc_codec_t audio_codec, app_webrtc_rtc_codec_t video_codec)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_NULL_ARG);

    DLOGI("Setting codecs: audio=%d, video=%d", audio_codec, video_codec);

    // Validate and set audio codec
    switch (audio_codec) {
        case APP_WEBRTC_CODEC_OPUS:
        case APP_WEBRTC_CODEC_MULAW:
        case APP_WEBRTC_CODEC_ALAW:
            gSampleConfiguration->audioCodec = audio_codec;
            break;
        default:
            DLOGW("Unsupported audio codec: %d, using OPUS", audio_codec);
            gSampleConfiguration->audioCodec = APP_WEBRTC_CODEC_OPUS;
            break;
    }

    // Validate and set video codec
    switch (video_codec) {
        case APP_WEBRTC_CODEC_H264:
        case APP_WEBRTC_CODEC_H265:
        case APP_WEBRTC_CODEC_VP8:
            gSampleConfiguration->videoCodec = video_codec;
            break;
        default:
            DLOGW("Unsupported video codec: %d, using H264", video_codec);
            gSampleConfiguration->videoCodec = APP_WEBRTC_CODEC_H264;
            break;
    }

CleanUp:
    return retStatus;
}

WEBRTC_STATUS app_webrtc_set_media_type(app_webrtc_streaming_media_t media_type)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_NULL_ARG);

    DLOGI("Setting media type to: %d", media_type);

    switch (media_type) {
        case APP_WEBRTC_MEDIA_VIDEO:
            gSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_VIDEO;
            break;
        case APP_WEBRTC_MEDIA_AUDIO_VIDEO:
            gSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
            break;
        default:
            DLOGW("Unsupported media type: %d, using audio+video", media_type);
            gSampleConfiguration->mediaType = APP_WEBRTC_MEDIA_AUDIO_VIDEO;
            break;
    }

CleanUp:
    return retStatus;
}

WEBRTC_STATUS app_webrtc_enable_media_reception(bool enable)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(gapp_webrtc_initialized, STATUS_INVALID_OPERATION);
    CHK(gSampleConfiguration != NULL, STATUS_NULL_ARG);

    DLOGI("Setting media reception: %s", enable ? "enabled" : "disabled");
    gSampleConfiguration->receive_media = enable;

CleanUp:
    return retStatus;
}

/**
 * @brief Update ICE servers for the peer connection client
 *
 * This function fetches fresh ICE servers from the signaling client and
 * updates the peer connection client with them using the set_ice_servers interface.
 */
 WEBRTC_STATUS app_webrtc_update_ice_servers(void)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    static app_webrtc_ice_servers_payload_t persistent_ice_cfg = {0};
    UINT32 ice_count = 0;

    ESP_LOGI(TAG, "Progressive ICE: Updating ICE servers for peer connection client");

    // Check if we have a valid peer connection client and interface
    CHK(g_kvs_webrtc_client != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.peer_connection_if != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.peer_connection_if->set_ice_servers != NULL, STATUS_NOT_IMPLEMENTED);

    // Get ICE servers from signaling interface if available
    CHK(gWebRtcAppConfig.signaling_client_if != NULL && gSignalingClientData != NULL &&
        gWebRtcAppConfig.signaling_client_if->get_ice_servers != NULL, STATUS_INVALID_OPERATION);

    ESP_LOGI(TAG, "Progressive ICE: Fetching fresh ICE servers (including new TURN servers)");
    STATUS ice_status = gWebRtcAppConfig.signaling_client_if->get_ice_servers(
        gSignalingClientData, &ice_count, persistent_ice_cfg.ice_servers);
    CHK(STATUS_SUCCEEDED(ice_status) && ice_count > 0, STATUS_INVALID_OPERATION);

    // Sanitize: keep only entries with non-empty URL
    UINT32 valid = 0;
    for (UINT32 i = 0; i < ice_count; i++) {
        if (persistent_ice_cfg.ice_servers[i].urls[0] != '\0') {
            if (i != valid) {
                MEMCPY(&persistent_ice_cfg.ice_servers[valid], &persistent_ice_cfg.ice_servers[i], SIZEOF(app_webrtc_ice_server_t));
            }
            valid++;
        }
    }

    CHK(valid > 0, STATUS_INVALID_OPERATION);

    // Update ICE servers in the peer connection client
    ESP_LOGI(TAG, "Progressive ICE: Setting %" PRIu32 " fresh ICE servers for peer connection client", valid);
    ESP_LOGI(TAG, "Progressive ICE: Calling kvs_pc_set_ice_servers() -> kvs_applyNewIceServersCallback() -> peerConnectionUpdateIceServers()");
    WEBRTC_STATUS update_status = gWebRtcAppConfig.peer_connection_if->set_ice_servers(
        g_kvs_webrtc_client, persistent_ice_cfg.ice_servers, valid);
    CHK(update_status == WEBRTC_STATUS_SUCCESS, STATUS_INTERNAL_ERROR);

    ESP_LOGI(TAG, "Progressive ICE: Successfully updated ICE servers for peer connection client");

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to update ICE servers: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

/**
 * @brief Callback for when new ICE servers become available asynchronously
 *
 * This callback is triggered when TURN servers are fetched in the background
 * and allows the peer connection to update its ICE servers dynamically.
 * This is a key part of the progressive ICE optimization that allows
 * connections to start immediately with STUN and get TURN servers later.
 */
static WEBRTC_STATUS app_webrtc_on_ice_servers_updated(uint64_t customData, uint32_t newServerCount)
{
    UNUSED_PARAM(customData);
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;

    ESP_LOGI(TAG, "Progressive ICE callback triggered! New server count: %" PRIu32, newServerCount);
    ESP_LOGI(TAG, "This means TURN servers are now available for improved NAT traversal");

    if (newServerCount == 0) {
        ESP_LOGW(TAG, "No new ICE servers available, skipping update");
        return WEBRTC_STATUS_SUCCESS;
    }

    // Update ICE servers for the peer connection client
    ESP_LOGI(TAG, "Updating peer connection client with fresh ICE servers...");
    retStatus = app_webrtc_update_ice_servers();
    if (retStatus == WEBRTC_STATUS_SUCCESS) {
        ESP_LOGD(TAG, "Progressive ICE update completed successfully!");
        ESP_LOGD(TAG, "   • Existing connections: Will use current servers (STUN usually sufficient)");
        ESP_LOGD(TAG, "   • New connections: Will use fresh servers including TURN");
        ESP_LOGD(TAG, "   • NAT traversal: Improved for future sessions");
    } else {
        ESP_LOGW(TAG, "Progressive ICE update failed: 0x%08" PRIx32, (UINT32) retStatus);
        ESP_LOGD(TAG, "   • Impact: New connections may have reduced NAT traversal capability");
    }

    return retStatus;
}

/**
 * @brief Trigger progressive ICE server refresh mechanism
 *
 * This helper function eliminates code duplication for the progressive ICE
 * trigger logic used in offer processing, answer processing, and offer creation.
 * Falls back to traditional ICE refresh when progressive mechanism is not available.
 *
 * For split architecture (streaming_only + signaling_only), this provides default
 * STUN servers immediately to start ICE gathering, then triggers async update for
 * full ICE servers (including TURN) from the signaling device.
 *
 * @param context Description of the context (for logging)
 * @param useTurn Whether to prioritize TURN servers
 * @return WEBRTC_STATUS Result of the operation
 */
static WEBRTC_STATUS app_webrtc_trigger_progressive_ice(const char* context, bool useTurn)
{
    ESP_LOGD(TAG, "Progressive ICE: Triggering ICE refresh for %s", context);

    // Check if signaling interface is available
    if (gWebRtcAppConfig.signaling_client_if == NULL || gSignalingClientData == NULL) {
        ESP_LOGW(TAG, "Progressive ICE: Signaling interface not available for %s", context);
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Check if progressive ICE mechanism is supported (get_ice_server_by_idx available)
    if (gWebRtcAppConfig.signaling_client_if->get_ice_server_by_idx != NULL) {
        ESP_LOGI(TAG, "Using progressive ICE mechanism for %s", context);

        // Call with index 0 to get servers immediately and trigger background fetching
        uint8_t *ice_data = NULL;
        int ice_len = 0;
        bool have_more = false;

        WEBRTC_STATUS trigger_status = gWebRtcAppConfig.signaling_client_if->get_ice_server_by_idx(
            gSignalingClientData, 0, useTurn, &ice_data, &ice_len, &have_more);

        if (trigger_status == WEBRTC_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Progressive ICE: Triggered for %s, have_more=%s",
                     context, have_more ? "true" : "false");
            if (have_more) {
                ESP_LOGD(TAG, "Progressive ICE: TURN servers will be fetched in background");
            }
            // Free the data if allocated
            if (ice_data != NULL) {
                SAFE_MEMFREE(ice_data);
            }
        } else {
            ESP_LOGW(TAG, "Progressive ICE: Failed to trigger ICE refresh for %s: 0x%08" PRIx32, context, (UINT32) trigger_status);
        }

        return trigger_status;
    } else {
        // Fallback path for interfaces without progressive ICE support (e.g., split architecture)
        ESP_LOGI(TAG, "Progressive ICE not supported for %s - using default STUN + async update strategy", context);

        // Provide default STUN server immediately so ICE gathering can start
        if (g_kvs_webrtc_client != NULL &&
            gWebRtcAppConfig.peer_connection_if != NULL &&
            gWebRtcAppConfig.peer_connection_if->set_ice_servers != NULL) {

            // Create default STUN server configuration
            static app_webrtc_ice_server_t default_ice_servers[1];
            STRNCPY(default_ice_servers[0].urls, APP_WEBRTC_DEFAULT_STUN_SERVER, APP_WEBRTC_MAX_ICE_CONFIG_URI_LEN);
            default_ice_servers[0].urls[APP_WEBRTC_MAX_ICE_CONFIG_URI_LEN] = '\0';
            default_ice_servers[0].username[0] = '\0';
            default_ice_servers[0].credential[0] = '\0';

            ESP_LOGI(TAG, "Setting default STUN server immediately: %s", default_ice_servers[0].urls);
            WEBRTC_STATUS stun_status = gWebRtcAppConfig.peer_connection_if->set_ice_servers(
                g_kvs_webrtc_client, default_ice_servers, 1);

            if (stun_status == WEBRTC_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Default STUN server set - ICE gathering can start immediately");
            } else {
                ESP_LOGW(TAG, "Failed to set default STUN server: 0x%08" PRIx32, (UINT32) stun_status);
            }
        }

        // Trigger async ICE refresh
        if (gWebRtcAppConfig.signaling_client_if->refresh_ice_configuration != NULL) {
            WEBRTC_STATUS refresh_status = gWebRtcAppConfig.signaling_client_if->refresh_ice_configuration(gSignalingClientData);

            if (refresh_status == WEBRTC_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Traditional ICE refresh triggered successfully for %s", context);
            } else {
                ESP_LOGW(TAG, "Failed to trigger async ICE refresh: 0x%08" PRIx32, (UINT32) refresh_status);
            }
        } else {
            ESP_LOGI(TAG, "No async ICE refresh mechanism available! Will use default STUN only");
        }

        return WEBRTC_STATUS_SUCCESS;
    }
}

WEBRTC_STATUS app_webrtc_set_data_channel_callbacks(const char *peer_id,
                                                   app_webrtc_rtc_on_open_t onOpen,
                                                   app_webrtc_rtc_on_message_t onMessage,
                                                   uint64_t custom_data)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    PAppWebRTCSession pStreamingSession = NULL;
    UINT32 i;
    bool peer_found = FALSE;

    ESP_LOGI(TAG, "Setting data channel callbacks for peer: %s, custom_data: 0x%" PRIx64,
             peer_id ? peer_id : "NULL", (uint64_t)custom_data);

    CHK(peer_id != NULL, STATUS_NULL_ARG);

    // Store the callbacks for future peer connections
    g_early_data_channel_callbacks.callbacks_registered = TRUE;
    g_early_data_channel_callbacks.onOpen = onOpen;
    g_early_data_channel_callbacks.onMessage = onMessage;
    g_early_data_channel_callbacks.custom_data = custom_data;
    ESP_LOGD(TAG, "Stored early data channel callbacks for future peer connections");

    // If gSampleConfiguration is not initialized yet, we'll apply these callbacks when peers connect
    if (gSampleConfiguration == NULL) {
        ESP_LOGD(TAG, "WebRTC not fully initialized yet, callbacks will be applied when peers connect");
        CHK(TRUE, retStatus);
    }

    // Try to find an existing streaming session with the given peer ID
    MUTEX_LOCK(gSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < gSampleConfiguration->streamingSessionCount; ++i) {
        if (gSampleConfiguration->webrtcSessionList[i] != NULL &&
            STRCMP(gSampleConfiguration->webrtcSessionList[i]->peerId, peer_id) == 0) {
            pStreamingSession = gSampleConfiguration->webrtcSessionList[i];
            ESP_LOGD(TAG, "Found streaming session for peer: %s at index %" PRIu32, peer_id, i);
            peer_found = TRUE;
            break;
        }
    }
    MUTEX_UNLOCK(gSampleConfiguration->streamingSessionListReadLock);

    // If peer not found, that's okay - we've stored the callbacks for future use
    if (!peer_found) {
        ESP_LOGD(TAG, "No streaming session found for peer: %s, callbacks will be applied when peer connects", peer_id);
        CHK(TRUE, retStatus);
    }

    // If we found a peer, apply the callbacks now
    if (peer_found) {
        // Make sure the peer connection interface is valid
        CHK(gWebRtcAppConfig.peer_connection_if != NULL, STATUS_INVALID_OPERATION);
        CHK(gWebRtcAppConfig.peer_connection_if->set_data_channel_callbacks != NULL, STATUS_NOT_IMPLEMENTED);

        ESP_LOGD(TAG, "Setting data channel callbacks via interface for peer: %s", peer_id);
        ESP_LOGD(TAG, "  - onOpen callback: %p", (void *) onOpen);
        ESP_LOGD(TAG, "  - onMessage callback: %p", (void *) onMessage);
        ESP_LOGD(TAG, "  - interface_session_handle: %p", pStreamingSession->interface_session_handle);

        // Set the callbacks using the peer connection interface
        retStatus = gWebRtcAppConfig.peer_connection_if->set_data_channel_callbacks(
            pStreamingSession->interface_session_handle,
            onOpen,
            onMessage,
            custom_data
        );

        if (STATUS_SUCCEEDED(retStatus)) {
            ESP_LOGD(TAG, "Successfully set data channel callbacks for peer: %s", peer_id);

            // Store the custom data in the streaming session for later use
            pStreamingSession->shutdownCallbackCustomData = custom_data;
            ESP_LOGD(TAG, "Stored custom_data 0x%" PRIx64 " in session", custom_data);
        } else {
            ESP_LOGE(TAG, "Failed to set data channel callbacks via interface: 0x%08" PRIx32, (UINT32) retStatus);
        }
    }

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to set data channel callbacks: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}

WEBRTC_STATUS app_webrtc_send_data_channel_message(const char *peer_id,
                                                   void *pDataChannel,
                                                   bool isBinary,
                                                   const uint8_t *pMessage,
                                                   uint32_t messageLen)
{
    ENTERS();
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    PAppWebRTCSession pStreamingSession = NULL;
    UINT32 i;

    CHK(peer_id != NULL && pMessage != NULL && messageLen > 0, STATUS_NULL_ARG);
    CHK(gSampleConfiguration != NULL, STATUS_INVALID_OPERATION);

    // Find the streaming session with the given peer ID
    MUTEX_LOCK(gSampleConfiguration->streamingSessionListReadLock);
    for (i = 0; i < gSampleConfiguration->streamingSessionCount; ++i) {
        if (gSampleConfiguration->webrtcSessionList[i] != NULL &&
            STRCMP(gSampleConfiguration->webrtcSessionList[i]->peerId, peer_id) == 0) {
            pStreamingSession = gSampleConfiguration->webrtcSessionList[i];
            ESP_LOGD(TAG, "Found streaming session for peer: %s at index %" PRIu32, peer_id, i);
            break;
        }
    }
    MUTEX_UNLOCK(gSampleConfiguration->streamingSessionListReadLock);

    if (pStreamingSession == NULL) {
        ESP_LOGE(TAG, "No streaming session found for peer: %s", peer_id);
        CHK(FALSE, STATUS_INVALID_ARG);
    }

    // Make sure the peer connection interface is valid
    CHK(gWebRtcAppConfig.peer_connection_if != NULL, STATUS_INVALID_OPERATION);
    CHK(gWebRtcAppConfig.peer_connection_if->send_data_channel_message != NULL, STATUS_NOT_IMPLEMENTED);

    ESP_LOGD(TAG, "Sending data channel message via interface for peer: %s", peer_id);
    ESP_LOGD(TAG, "  - interface_session_handle: %p", pStreamingSession->interface_session_handle);
    ESP_LOGD(TAG, "  - message length: %" PRIu32, messageLen);

    // Send the message using the peer connection interface
    retStatus = gWebRtcAppConfig.peer_connection_if->send_data_channel_message(
        pStreamingSession->interface_session_handle,
        pDataChannel,
        isBinary,
        pMessage,
        messageLen
    );

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        ESP_LOGE(TAG, "Failed to send data channel message: 0x%08" PRIx32, (UINT32) retStatus);
    }
    return retStatus;
}
