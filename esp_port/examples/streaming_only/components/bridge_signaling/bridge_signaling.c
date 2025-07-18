/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "webrtc_bridge_signaling.h"
#include "webrtc_bridge.h"
#include "signaling_serializer.h"
#include "esp_log.h"
#include "string.h"
#include "stdlib.h"

static const char *TAG = "bridge_signaling";

/**
 * @brief Bridge signaling client data structure
 */
typedef struct {
    // Configuration
    bridge_signaling_config_t config;
    bool initialized;
    esp_webrtc_signaling_state_t state;

    // Callbacks matching WebRtcSignalingClientInterface
    WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*);
    WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t);
    WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t);
    uint64_t user_data;
} BridgeSignalingClientData;

// Global client instance (bridge is typically singleton)
static BridgeSignalingClientData* g_bridge_client = NULL;

/**
 * @brief Handle messages received from webrtc_bridge
 */
void bridge_message_handler(const void* data, int len)
{
    if (g_bridge_client == NULL || g_bridge_client->onMessageReceived == NULL) {
        ESP_LOGW(TAG, "Bridge message received but no client or callback registered");
        return;
    }

    // Deserialize the message
    signaling_msg_t signaling_msg;
    esp_err_t deserialize_result = deserialize_signaling_message(data, len, &signaling_msg);
    if (deserialize_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize bridge message with error: %d", deserialize_result);
        return;
    }

    // Convert to standardized esp_webrtc_signaling_message_t structure
    esp_webrtc_signaling_message_t webrtc_msg;
    memset(&webrtc_msg, 0, sizeof(esp_webrtc_signaling_message_t));

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
            ESP_LOGW(TAG, "Unknown message type from bridge: %d", signaling_msg.messageType);
            goto cleanup;
    }

    // Copy message data
    webrtc_msg.version = signaling_msg.version;
    strncpy(webrtc_msg.correlation_id, signaling_msg.correlationId, SS_MAX_CORRELATION_ID_LEN);
    webrtc_msg.correlation_id[SS_MAX_CORRELATION_ID_LEN] = '\0';
    strncpy(webrtc_msg.peer_client_id, signaling_msg.peerClientId, SS_MAX_SIGNALING_CLIENT_ID_LEN);
    webrtc_msg.peer_client_id[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';

    // Copy payload
    if (signaling_msg.payload && signaling_msg.payloadLen > 0) {
        webrtc_msg.payload = signaling_msg.payload;
        webrtc_msg.payload_len = signaling_msg.payloadLen;
    }

    // Call the callback with standardized message format
    WEBRTC_STATUS callback_result = g_bridge_client->onMessageReceived((uint64_t)g_bridge_client->user_data, &webrtc_msg);
    if (callback_result != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "g_bridge_client->onMessageReceived returned error: 0x%08" PRIx32, callback_result);
    }

cleanup:
    // Clean up deserialized message
    if (signaling_msg.payload) {
        free(signaling_msg.payload);
    }
}

/**
 * @brief Common utility function to send a signaling message via webrtc_bridge
 */
WEBRTC_STATUS bridge_signaling_send_message_via_bridge(signaling_msg_t* pMessage)
{
    if (pMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Serialize the message
    size_t serialized_len = 0;
    char* serialized_data = serialize_signaling_message(pMessage, &serialized_len);
    if (serialized_data == NULL) {
        ESP_LOGE(TAG, "Failed to serialize message for bridge");
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    ESP_LOGI(TAG, "Sending serialized message (%d bytes) via webrtc_bridge", (int)serialized_len);

    // Send via webrtc_bridge (it takes ownership of the data)
    webrtc_bridge_send_message(serialized_data, (int)serialized_len);

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling init implementation
 */
static WEBRTC_STATUS bridgeInit(void *pSignalingConfig, void **ppSignalingClient)
{
    ESP_LOGI(TAG, "Initializing bridge signaling client");

    if (pSignalingConfig == NULL || ppSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    bridge_signaling_config_t* config = (bridge_signaling_config_t*)pSignalingConfig;

    // Allocate client data
    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)calloc(1, sizeof(BridgeSignalingClientData));
    if (client_data == NULL) {
        return WEBRTC_STATUS_NOT_ENOUGH_MEMORY;
    }

    // Copy configuration
    memcpy(&client_data->config, config, sizeof(bridge_signaling_config_t));
    client_data->initialized = true;
    client_data->state = ESP_SIGNALING_STATE_NEW;

    // Set global client reference
    g_bridge_client = client_data;

    // Register the central router for this device (streaming_only)
    webrtc_bridge_register_handler(&bridge_message_handler);

    // Return the client data through the output parameter
    *ppSignalingClient = (void*)client_data;

    ESP_LOGI(TAG, "Bridge signaling client initialized");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling connect implementation
 */
static WEBRTC_STATUS bridgeConnect(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Connecting bridge signaling client");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    if (!client_data->initialized) {
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Set connected state
    client_data->state = ESP_SIGNALING_STATE_CONNECTED;

    // Notify state change using portable state type
    if (client_data->onStateChanged) {
        client_data->onStateChanged((uint64_t)client_data->user_data, WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED);
    }

    ESP_LOGI(TAG, "Bridge signaling client connected");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling disconnect implementation
 */
static WEBRTC_STATUS bridgeDisconnect(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Disconnecting bridge signaling client");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    if (!client_data->initialized) {
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Set disconnected state
    client_data->state = ESP_SIGNALING_STATE_DISCONNECTED;

    // Notify state change using portable state type
    if (client_data->onStateChanged) {
        client_data->onStateChanged((uint64_t)client_data->user_data, WEBRTC_SIGNALING_CLIENT_STATE_DISCONNECTED);
    }

    ESP_LOGI(TAG, "Bridge signaling client disconnected");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling send message implementation
 */
static WEBRTC_STATUS bridgeSendMessage(void *pSignalingClient, esp_webrtc_signaling_message_t* pMessage)
{
    ESP_LOGI(TAG, "Sending message via bridge signaling");

    if (pSignalingClient == NULL || pMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    if (!client_data->initialized || client_data->state != ESP_SIGNALING_STATE_CONNECTED) {
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Convert esp_webrtc_signaling_message_t to signaling_msg_t
    signaling_msg_t signaling_msg;
    signaling_msg.version = pMessage->version;

    // Convert message type
    switch (pMessage->message_type) {
        case ESP_SIGNALING_MESSAGE_TYPE_OFFER:
            signaling_msg.messageType = SIGNALING_MSG_TYPE_OFFER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ANSWER:
            signaling_msg.messageType = SIGNALING_MSG_TYPE_ANSWER;
            break;
        case ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            signaling_msg.messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown message type for bridge: %d", pMessage->message_type);
            return WEBRTC_STATUS_INVALID_ARG;
    }

    // Copy strings
    strncpy(signaling_msg.correlationId, pMessage->correlation_id, SS_MAX_CORRELATION_ID_LEN);
    signaling_msg.correlationId[SS_MAX_CORRELATION_ID_LEN] = '\0';
    strncpy(signaling_msg.peerClientId, pMessage->peer_client_id, SS_MAX_SIGNALING_CLIENT_ID_LEN);
    signaling_msg.peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';

    // Copy payload
    signaling_msg.payload = pMessage->payload;
    signaling_msg.payloadLen = pMessage->payload_len;

    // Use the common function to send via bridge
    return bridge_signaling_send_message_via_bridge(&signaling_msg);
}

/**
 * @brief Bridge signaling free implementation
 */
static WEBRTC_STATUS bridgeFree(void *pSignalingClient)
{
    ESP_LOGI(TAG, "Freeing bridge signaling client");

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    // Clear global reference
    if (g_bridge_client == client_data) {
        g_bridge_client = NULL;
    }

    // Free the client data
    free(client_data);

    ESP_LOGI(TAG, "Bridge signaling client freed");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling set callbacks implementation
 */
static WEBRTC_STATUS bridgeSetCallbacks(void *pSignalingClient,
                                        uint64_t customData,
                                        WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*),
                                        WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t),
                                        WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    if (pSignalingClient == NULL) {
        ESP_LOGE(TAG, "pSignalingClient is NULL");
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    // Set the callbacks
    client_data->onMessageReceived = onMessageReceived;
    client_data->onStateChanged = onStateChanged;
    client_data->onError = onError;
    client_data->user_data = customData;

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling set role type implementation (stub)
 */
static WEBRTC_STATUS bridgeSetRoleType(void *pSignalingClient, webrtc_signaling_channel_role_type_t roleType)
{
    ESP_LOGI(TAG, "Setting bridge signaling role type: %d", roleType);

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Bridge signaling doesn't need to handle role type differently
    // The role is handled by the actual WebRTC connection
    ESP_LOGI(TAG, "Bridge signaling role type set");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling get ICE servers implementation (stub)
 */
static WEBRTC_STATUS bridgeGetIceServers(void *pSignalingClient, uint32_t *pIceConfigCount, void *pRtcConfiguration)
{
    ESP_LOGI(TAG, "Getting ICE servers for bridge signaling");

    if (pSignalingClient == NULL || pIceConfigCount == NULL || pRtcConfiguration == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Bridge signaling doesn't provide ICE servers
    // The actual signaling client (KVS) will provide them
    *pIceConfigCount = 0;

    ESP_LOGI(TAG, "Bridge signaling ICE servers retrieved (none)");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Get the bridge signaling client interface
 */
WebRtcSignalingClientInterface* getBridgeSignalingClientInterface(void)
{
    static WebRtcSignalingClientInterface bridge_interface = {
        .init = bridgeInit,
        .connect = bridgeConnect,
        .disconnect = bridgeDisconnect,
        .sendMessage = bridgeSendMessage,
        .free = bridgeFree,
        .setCallbacks = bridgeSetCallbacks,
        .setRoleType = bridgeSetRoleType,
        .getIceServers = bridgeGetIceServers
    };

    return &bridge_interface;
}
