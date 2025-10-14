/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "apprtc_signaling_interface.h"
#include "apprtc_signaling.h"
#include "signaling_conversion.h"
#include "webrtc_signaling_if.h"
#include "esp_log.h"

static const char *TAG = "apprtc_signaling_if";

// Global state for the AppRTC signaling adapter
typedef struct {
    AppRtcSignalingConfig config;
    bool initialized;
    bool connected;
    void *customData;
    WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*);
    WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t);
    WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t);
} AppRtcSignalingClientData;

static AppRtcSignalingClientData gAppRtcClientData = {0};

// Forward declarations for AppRTC callbacks
static void apprtc_message_handler(const char *message, size_t message_len, void *user_data);
static void apprtc_state_change_handler(int state, void *user_data);

// Convert AppRTC state to portable signaling client state
static webrtc_signaling_client_state_t convertAppRtcState(apprtc_signaling_state_t apprtc_state)
{
    switch (apprtc_state) {
        case APPRTC_SIGNALING_STATE_DISCONNECTED:
            return WEBRTC_SIGNALING_CLIENT_STATE_NEW;
        case APPRTC_SIGNALING_STATE_CONNECTING:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTING;
        case APPRTC_SIGNALING_STATE_CONNECTED:
            return WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED;
        case APPRTC_SIGNALING_STATE_ERROR:
            return WEBRTC_SIGNALING_CLIENT_STATE_FAILED;
        default:
            return WEBRTC_SIGNALING_CLIENT_STATE_FAILED;
    }
}

// AppRTC message handler callback
static void apprtc_message_handler(const char *message, size_t message_len, void *user_data)
{
    ESP_LOGI(TAG, "AppRTC message received: %.*s", (int)message_len, message);

    if (gAppRtcClientData.onMessageReceived == NULL) {
        ESP_LOGW(TAG, "No message callback registered");
        return;
    }

    // First convert AppRTC JSON to signaling_msg_t format
    signaling_msg_t signaling_msg = {0};
    int result = apprtc_json_to_signaling_message(message, message_len, &signaling_msg);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to convert AppRTC message to signaling format");
        return;
    }

    // Then convert from signaling_msg_t to esp_webrtc_signaling_message_t
    esp_webrtc_signaling_message_t webrtc_msg = {0};

    // Copy version
    webrtc_msg.version = signaling_msg.version;

    // Convert message type
    switch (signaling_msg.messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            webrtc_msg.message_type = ESP_SIGNALING_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            webrtc_msg.message_type = ESP_SIGNALING_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            webrtc_msg.message_type = ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown signaling message type: %d", signaling_msg.messageType);
            // Clean up allocated payload from signaling_msg
            if (signaling_msg.payload) {
                free(signaling_msg.payload);
            }
            return;
    }

    // Copy correlation ID
    strncpy(webrtc_msg.correlation_id, signaling_msg.correlationId, sizeof(webrtc_msg.correlation_id) - 1);
    webrtc_msg.correlation_id[sizeof(webrtc_msg.correlation_id) - 1] = '\0';

    // Copy peer client ID
    strncpy(webrtc_msg.peer_client_id, signaling_msg.peerClientId, sizeof(webrtc_msg.peer_client_id) - 1);
    webrtc_msg.peer_client_id[sizeof(webrtc_msg.peer_client_id) - 1] = '\0';

    // Copy payload (transfer ownership)
    webrtc_msg.payload = signaling_msg.payload;
    webrtc_msg.payload_len = signaling_msg.payloadLen;

    // Call the registered callback
    gAppRtcClientData.onMessageReceived((uint64_t)(uintptr_t)gAppRtcClientData.customData, &webrtc_msg);

    // Clean up the payload (it was allocated by apprtc_json_to_signaling_message)
    if (webrtc_msg.payload) {
        free(webrtc_msg.payload);
    }
}

static void apprtc_state_change_handler(int state, void *user_data)
{
    ESP_LOGI(TAG, "AppRTC state changed to: %d", state);

    // Update our internal state
    gAppRtcClientData.connected = (state == APPRTC_SIGNALING_STATE_CONNECTED);

    // Forward state change to WebRTC app if callback is set
    if (gAppRtcClientData.onStateChanged != NULL) {
        webrtc_signaling_client_state_t clientState = convertAppRtcState((apprtc_signaling_state_t)state);
        gAppRtcClientData.onStateChanged((uint64_t)(uintptr_t)gAppRtcClientData.customData, clientState);
    }
}

// Interface implementation functions
static WEBRTC_STATUS apprtcInit(void *pSignalingConfig, void **ppSignalingClient)
{
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    PAppRtcSignalingConfig pConfig = (PAppRtcSignalingConfig)pSignalingConfig;

    ESP_LOGI(TAG, "Initializing AppRTC signaling interface");

    if (pConfig == NULL || ppSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Copy configuration
    memcpy(&gAppRtcClientData.config, pConfig, sizeof(AppRtcSignalingConfig));

    // Initialize AppRTC signaling with our callbacks
    esp_err_t ret = apprtc_signaling_init(apprtc_message_handler, apprtc_state_change_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize AppRTC signaling: %s", esp_err_to_name(ret));
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    gAppRtcClientData.initialized = true;
    *ppSignalingClient = &gAppRtcClientData;

    ESP_LOGI(TAG, "AppRTC signaling interface initialized successfully");
    return retStatus;
}

static WEBRTC_STATUS apprtcConnect(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Connecting AppRTC signaling");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Connect to AppRTC using the room ID from config
    esp_err_t ret = apprtc_signaling_connect(gAppRtcClientData.config.roomId);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to connect AppRTC signaling: %s", esp_err_to_name(ret));
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS apprtcDisconnect(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Disconnecting AppRTC signaling");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    esp_err_t ret = apprtc_signaling_disconnect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect AppRTC signaling: %s", esp_err_to_name(ret));
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    gAppRtcClientData.connected = false;
    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS apprtcSendMessage(void *pSignalingClient, esp_webrtc_signaling_message_t *pMessage)
{
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    signaling_msg_t signalingMsg;

    ESP_LOGI(TAG, "Sending message via AppRTC signaling interface");

    if (pSignalingClient == NULL || pMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Convert from esp_webrtc_signaling_message_t to signaling_msg_t
    signalingMsg.version = pMessage->version;

    // Convert message type
    switch (pMessage->message_type) {
        case ESP_SIGNALING_MESSAGE_TYPE_OFFER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_OFFER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ANSWER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ANSWER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type: %d", pMessage->message_type);
            return WEBRTC_STATUS_INVALID_ARG;
    }

    // Copy correlation ID
    strncpy(signalingMsg.correlationId, pMessage->correlation_id, SS_MAX_CORRELATION_ID_LEN);
    signalingMsg.correlationId[SS_MAX_CORRELATION_ID_LEN] = '\0';

    // Copy peer client ID
    strncpy(signalingMsg.peerClientId, pMessage->peer_client_id, SS_MAX_SIGNALING_CLIENT_ID_LEN);
    signalingMsg.peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';

    // Copy payload
    signalingMsg.payload = pMessage->payload;
    signalingMsg.payloadLen = pMessage->payload_len;

    // Send the message using the AppRTC signaling callback
    int result = apprtc_signaling_send_callback(&signalingMsg);

    // Handle the callback return value:
    // 0 = success (message sent immediately)
    // 1 = success (message queued for later sending)
    // any other value = error
    if (result == 0 || result == 1) {
        retStatus = WEBRTC_STATUS_SUCCESS;
        ESP_LOGI(TAG, "Message sent successfully via AppRTC signaling interface");
    } else {
        retStatus = WEBRTC_STATUS_INTERNAL_ERROR;
        ESP_LOGE(TAG, "Failed to send message via AppRTC signaling interface, result: %d", result);
    }

    return retStatus;
}

static WEBRTC_STATUS apprtcFree(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Freeing AppRTC signaling interface");

    if (gAppRtcClientData.connected) {
        apprtc_signaling_disconnect();
    }

    memset(&gAppRtcClientData, 0, sizeof(AppRtcSignalingClientData));
    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS apprtcSetCallbacks(void *pSignalingClient,
                                void *customData,
                                WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*),
                                WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t),
                                WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    ESP_LOGI(TAG, "Setting AppRTC signaling callbacks");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Store the callbacks
    gAppRtcClientData.customData = customData;
    gAppRtcClientData.onMessageReceived = onMessageReceived;
    gAppRtcClientData.onStateChanged = onStateChanged;
    gAppRtcClientData.onError = onError;

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS apprtcSetRoleType(void *pSignalingClient, webrtc_signaling_channel_role_type_t roleType)
{
    ESP_LOGI(TAG, "Setting AppRTC signaling role type: %d", roleType);

    // AppRTC doesn't have a explicit role type concept like KVS
    // The role is determined by who creates the room vs who joins
    // For now, we just log it

    return WEBRTC_STATUS_SUCCESS;
}

static WEBRTC_STATUS apprtcGetIceServers(void *pSignalingClient, uint32_t *pIceConfigCount, void *pRtcConfiguration)
{
    ESP_LOGI(TAG, "Getting ICE servers from AppRTC");

    // AppRTC typically provides ICE servers through the signaling process
    // For now, we return success and let the default ICE servers be used
    // This could be enhanced to get ICE servers from AppRTC server

    if (pIceConfigCount != NULL) {
        *pIceConfigCount = 0;  // No additional ICE servers
    }

    return WEBRTC_STATUS_SUCCESS;
}

// Interface instance
static WebRtcSignalingClientInterface apprtcSignalingInterface = {
    .init = apprtcInit,
    .connect = apprtcConnect,
    .disconnect = apprtcDisconnect,
    .sendMessage = apprtcSendMessage,
    .free = apprtcFree,
    .setCallbacks = apprtcSetCallbacks,
    .setRoleType = apprtcSetRoleType,
    .getIceServers = apprtcGetIceServers
};

// Public interface functions
WebRtcSignalingClientInterface* getAppRtcSignalingClientInterface(void)
{
    return &apprtcSignalingInterface;
}

bool isAppRtcSignalingConnected(void)
{
    return gAppRtcClientData.connected;
}

char* getAppRtcRoomId(void)
{
    return (char*)apprtc_signaling_get_room_id();
}
