/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "string.h"
#include "stdlib.h"
#include "webrtc_bridge.h"
#include "app_webrtc.h"
#include "signaling_bridge_adapter.h"
#include "bridge_peer_connection.h"
#include "signaling_serializer.h"
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
#include "network_coprocessor.h"
#endif

static const char *TAG = "signaling_bridge_adapter";

// Static configuration storage
static signaling_bridge_adapter_config_t g_config = {0};
static bool g_initialized = false;

// Global reference for bridge peer connection session
static uint64_t g_custom_data = 0;
static WEBRTC_STATUS (*g_on_message_received)(uint64_t, webrtc_message_t*) = NULL;

/**
 * @brief Handle signaling messages received from the streaming device via webrtc_bridge
 */
static void handle_bridged_message(const void* data, int len)
{
    ESP_LOGD(TAG, "Received bridged message from streaming device (%d bytes)", len);

    signaling_msg_t signalingMsg = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message");
        return;
    }

    ESP_LOGD(TAG, "Deserialized message type %d from peer %s",
             signalingMsg.messageType, signalingMsg.peerClientId);

    // Note: ICE requests are now handled via synchronous RPC, not bridge messages
    // This provides much better performance (89ms vs 1.4s) by bypassing the async queue

    // Convert to webrtc_message_t format for potential direct handling by bridge_peer_connection
    webrtc_message_t webrtcMsg = {0};
    webrtcMsg.version = signalingMsg.version;

    // Map message types
    switch (signalingMsg.messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE;
            break;
        default:
            ESP_LOGW(TAG, "Unknown signaling message type: %d", signalingMsg.messageType);
            if (signalingMsg.payload != NULL) {
                free(signalingMsg.payload);
            }
            return;
    }

    // Copy message fields
    strncpy(webrtcMsg.correlation_id, signalingMsg.correlationId, sizeof(webrtcMsg.correlation_id) - 1);
    webrtcMsg.correlation_id[sizeof(webrtcMsg.correlation_id) - 1] = '\0';

    strncpy(webrtcMsg.peer_client_id, signalingMsg.peerClientId, sizeof(webrtcMsg.peer_client_id) - 1);
    webrtcMsg.peer_client_id[sizeof(webrtcMsg.peer_client_id) - 1] = '\0';

    webrtcMsg.payload = signalingMsg.payload;
    webrtcMsg.payload_len = signalingMsg.payloadLen;

    // If we have a registered message callback, use it directly
    // if (g_on_message_received != NULL) {
    ESP_LOGD(TAG, "Using registered callback for message type %d", webrtcMsg.message_type);
    WEBRTC_STATUS status = g_on_message_received(g_custom_data, &webrtcMsg);
    if (status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to process message via callback: 0x%08x", status);
    } else {
        ESP_LOGD(TAG, "Successfully processed message via callback");
    }

    // Clean up
    if (signalingMsg.payload != NULL) {
        free(signalingMsg.payload);
    }
}

/**
 * @brief Register callbacks for bridge peer connection
 *
 * This function registers callbacks for the bridge peer connection interface
 * to use when receiving messages from the bridge.
 */
WEBRTC_STATUS signaling_bridge_adapter_register_callbacks(
    uint64_t custom_data,
    WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*))
{
    ESP_LOGI(TAG, "Registering bridge peer connection callbacks");

    g_custom_data = custom_data;
    g_on_message_received = on_message_received; // Callback to be called when meesages from bridge are received

    return WEBRTC_STATUS_SUCCESS;
}

WEBRTC_STATUS signaling_bridge_adapter_init(const signaling_bridge_adapter_config_t *config)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Signaling bridge adapter already initialized");
        return WEBRTC_STATUS_SUCCESS;
    }

    // Initialize signaling serializer
    signaling_serializer_init();

    if (config) {
        // Store configuration
        memcpy(&g_config, config, sizeof(signaling_bridge_adapter_config_t));
    }

    // Register the bridge message handler to receive messages from streaming device
    webrtc_bridge_register_handler(handle_bridged_message);
    ESP_LOGI(TAG, "Registered bridge message handler");

    // Register RPC handler with network coprocessor for ICE server queries
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
    network_coprocessor_register_ice_server_query_callback(signaling_bridge_adapter_rpc_handler);
    ESP_LOGI(TAG, "Registered ICE server RPC handler");
#endif

    g_initialized = true;
    ESP_LOGI(TAG, "Signaling bridge adapter initialized successfully");
    return WEBRTC_STATUS_SUCCESS;
}

WEBRTC_STATUS signaling_bridge_adapter_start(void)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Signaling bridge adapter not initialized");
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Start the WebRTC bridge
    webrtc_bridge_start();
    ESP_LOGI(TAG, "WebRTC bridge started");

    return WEBRTC_STATUS_SUCCESS;
}

int signaling_bridge_adapter_send_message(webrtc_message_t *signalingMessage)
{
    if (!signalingMessage) {
        ESP_LOGE(TAG, "Invalid signaling message");
        return -1;
    }

    // Convert to signaling_msg_t format for bridge
    signaling_msg_t signalingMsg = {0};
    signalingMsg.version = signalingMessage->version;

    // Map message types between webrtc_message_type_t and signaling_msg_type
    switch (signalingMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_OFFER;
            break;
        case WEBRTC_MESSAGE_TYPE_ANSWER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ANSWER;
            break;
        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
            break;
        case WEBRTC_MESSAGE_TYPE_TRIGGER_OFFER:
            signalingMsg.messageType = SIGNALING_MSG_TYPE_TRIGGER_OFFER;
            break;
        default:
            ESP_LOGE(TAG, "Unsupported message type: %d", signalingMessage->message_type);
            return -1;
    }

    strncpy(signalingMsg.peerClientId, signalingMessage->peer_client_id, sizeof(signalingMsg.peerClientId) - 1);
    strncpy(signalingMsg.correlationId, signalingMessage->correlation_id, sizeof(signalingMsg.correlationId) - 1);
    signalingMsg.payload = signalingMessage->payload;
    signalingMsg.payloadLen = signalingMessage->payload_len;

    ESP_LOGD(TAG, "Sending signaling message type %d to streaming device", signalingMessage->message_type);

    size_t serializedMsgLen = 0;
    char *serializedMsg = serialize_signaling_message(&signalingMsg, &serializedMsgLen);
    if (serializedMsg == NULL) {
        ESP_LOGE(TAG, "Failed to serialize signaling message");
        return -1;
    }

    // ownership of serializedMsg is transferred to webrtc_bridge_send_message
    webrtc_bridge_send_message(serializedMsg, serializedMsgLen);

    ESP_LOGD(TAG, "Successfully sent message type %d via bridge", signalingMessage->message_type);
    return 0;
}

int signaling_bridge_adapter_rpc_handler(int index, uint8_t **data, int *len, bool *have_more)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Signaling bridge adapter not initialized");
        return -1;
    }

    ESP_LOGI(TAG, "🚀 RPC: ICE server query for index %d", index);

    // Delegate to WebRTC abstraction layer
    bool use_turn = true; // Always request TURN servers via RPC
    uint8_t* raw_data = NULL;
    int raw_len = 0;
    WEBRTC_STATUS status = app_webrtc_get_server_by_idx(index, use_turn, &raw_data, &raw_len, have_more);

    if (status == WEBRTC_STATUS_SUCCESS && raw_data != NULL && raw_len > 0) {
        // Transfer ownership to caller
        *data = raw_data;
        *len = raw_len;

        ESP_LOGI(TAG, "✅ RPC: Successfully retrieved ICE server for index %d (len: %d, have_more: %s)",
                 index, raw_len, *have_more ? "true" : "false");
        return 0; // Success
    } else if (*have_more == true) {
        ESP_LOGI(TAG, "🔄 RPC: ICE server query for index %d is still in progress", index);
        *data = NULL;
        *len = 0;
        return 0; // Success
    } else {
        ESP_LOGE(TAG, "❌ RPC: Failed to retrieve ICE server for index %d: 0x%08x", index, status);
        if (raw_data) {
            free(raw_data); // Clean up on failure
        }
        *data = NULL;
        *len = 0;
        *have_more = false;
        return -1; // Failure
    }
}

void signaling_bridge_adapter_deinit(void)
{
    if (g_initialized) {
        // Unregister RPC handler
#if CONFIG_ESP_WEBRTC_BRIDGE_HOSTED
        network_coprocessor_register_ice_server_query_callback(NULL);
#endif

        // Clear configuration
        memset(&g_config, 0, sizeof(signaling_bridge_adapter_config_t));
        g_initialized = false;

        ESP_LOGI(TAG, "Signaling bridge adapter deinitialized");
    }
}
