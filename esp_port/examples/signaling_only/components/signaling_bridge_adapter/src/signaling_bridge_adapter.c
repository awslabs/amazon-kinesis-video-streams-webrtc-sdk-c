/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "signaling_bridge_adapter.h"
#include "network_coprocessor.h"
#include "webrtc_bridge.h"
#include "app_webrtc.h"
#include "esp_log.h"
#include "string.h"
#include "stdlib.h"

static const char *TAG = "signaling_bridge_adapter";

// Static configuration storage
static signaling_bridge_adapter_config_t g_config = {0};
static bool g_initialized = false;

/**
 * @brief Handle signaling messages received from the streaming device via webrtc_bridge
 */
static void handle_bridged_message(const void* data, int len)
{
    ESP_LOGI(TAG, "Received bridged message from streaming device (%d bytes)", len);

    signaling_msg_t signalingMsg = {0};
    esp_err_t ret = deserialize_signaling_message(data, len, &signalingMsg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize signaling message");
        return;
    }

    ESP_LOGI(TAG, "Deserialized message type %d from peer %s",
             signalingMsg.messageType, signalingMsg.peerClientId);

    // Note: ICE requests are now handled via synchronous RPC, not bridge messages
    // This provides much better performance (89ms vs 1.4s) by bypassing the async queue

    // Send other messages to signaling server
    int result = app_webrtc_send_msg_to_signaling(&signalingMsg);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to send message to signaling server: %d", result);
    } else {
        ESP_LOGI(TAG, "Successfully sent message to signaling server");
    }

    // Clean up
    if (signalingMsg.payload != NULL) {
        free(signalingMsg.payload);
    }
}

WEBRTC_STATUS signaling_bridge_adapter_init(const signaling_bridge_adapter_config_t *config)
{
    if (g_initialized) {
        ESP_LOGW(TAG, "Signaling bridge adapter already initialized");
        return WEBRTC_STATUS_SUCCESS;
    }

    if (config) {
        // Store configuration
        memcpy(&g_config, config, sizeof(signaling_bridge_adapter_config_t));
    }

    // Register the message sending callback for split mode (sends to streaming device)
    app_webrtc_register_msg_callback(signaling_bridge_adapter_send_message);
    ESP_LOGI(TAG, "Registered bridge message sending callback");

    // Register the bridge message handler to receive messages from streaming device
    webrtc_bridge_register_handler(handle_bridged_message);
    ESP_LOGI(TAG, "Registered bridge message handler");

    // Register RPC handler with network coprocessor for ICE server queries
    network_coprocessor_register_ice_server_query_callback(signaling_bridge_adapter_rpc_handler);
    ESP_LOGI(TAG, "Registered ICE server RPC handler");

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

int signaling_bridge_adapter_send_message(signaling_msg_t *signalingMessage)
{
    if (!signalingMessage) {
        ESP_LOGE(TAG, "Invalid signaling message");
        return -1;
    }

    ESP_LOGI(TAG, "Sending signaling message type %d to streaming device", signalingMessage->messageType);

    size_t serializedMsgLen = 0;
    char *serializedMsg = serialize_signaling_message(signalingMessage, &serializedMsgLen);
    if (serializedMsg == NULL) {
        ESP_LOGE(TAG, "Failed to serialize signaling message");
        return -1;
    }

    // ownership of serializedMsg is transferred to webrtc_bridge_send_message
    webrtc_bridge_send_message(serializedMsg, serializedMsgLen);

    ESP_LOGI(TAG, "Successfully sent message type %d via bridge", signalingMessage->messageType);
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
        network_coprocessor_register_ice_server_query_callback(NULL);

        // Clear configuration
        memset(&g_config, 0, sizeof(signaling_bridge_adapter_config_t));
        g_initialized = false;

        ESP_LOGI(TAG, "Signaling bridge adapter deinitialized");
    }
}