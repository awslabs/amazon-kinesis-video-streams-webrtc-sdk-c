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
#include <inttypes.h>
#include "ice_bridge_client.h"

static const char *TAG = "bridge_signaling";

/**
 * @brief Bridge signaling client data structure
 */
typedef struct {
    // Configuration
    bridge_signaling_config_t config;
    bool initialized;
    webrtc_signaling_state_t state;

    // Callbacks matching webrtc_signaling_client_if_t
    WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*);
    WEBRTC_STATUS (*on_signaling_state_changed)(uint64_t, webrtc_signaling_state_t);
    WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t);
    uint64_t user_data;

    // ICE servers received from signaling_only device
    bool ice_servers_received;
    ss_ice_servers_payload_t ice_servers_config;
} BridgeSignalingClientData;

// Global client instance (bridge is typically singleton)
static BridgeSignalingClientData* g_bridge_client = NULL;

/**
 * @brief Handle messages received from webrtc_bridge
 */
void bridge_message_handler(const void* data, int len)
{
    ESP_LOGD(TAG, "bridge_message_handler called with %d bytes", len);

    if (g_bridge_client == NULL || g_bridge_client->on_msg_received == NULL) {
        ESP_LOGW(TAG, "Bridge message received but no client or callback registered");
        return;
    }

    // Deserialize the message
    signaling_msg_t signaling_msg;
    ESP_LOGD(TAG, "Processing bridge message (%d bytes)", len);
    esp_err_t deserialize_result = deserialize_signaling_message(data, len, &signaling_msg);
    if (deserialize_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize bridge message with error: %d", deserialize_result);
        return;
    }

    ESP_LOGD(TAG, "Deserialized message: type=%d, correlation_id=%s, peer_id=%s, payload_len=%d",
             (int) signaling_msg.messageType, signaling_msg.correlationId, signaling_msg.peerClientId, (int) signaling_msg.payloadLen);

    // Convert to standardized webrtc_message_t structure
    webrtc_message_t webrtc_msg;
    memset(&webrtc_msg, 0, sizeof(webrtc_message_t));

    // Handle ICE servers message separately (not forwarded to WebRTC)
    if (signaling_msg.messageType == SIGNALING_MSG_TYPE_ICE_SERVERS) {
        ESP_LOGI(TAG, "Received ICE servers configuration from signaling device");

        if (g_bridge_client && extract_ice_servers_from_message(&signaling_msg, &g_bridge_client->ice_servers_config) == ESP_OK) {
            g_bridge_client->ice_servers_received = true;
            ESP_LOGI(TAG, "Stored %d ICE servers for WebRTC connection", (int) g_bridge_client->ice_servers_config.ice_server_count);

            // Log the ICE servers for debugging
            for (int i = 0; i < g_bridge_client->ice_servers_config.ice_server_count; i++) {
                ESP_LOGI(TAG, "ICE Server %d: %s (user: %s)", i,
                         g_bridge_client->ice_servers_config.ice_servers[i].urls,
                         g_bridge_client->ice_servers_config.ice_servers[i].username);
            }
        } else {
            ESP_LOGE(TAG, "Failed to extract ICE servers from message");
        }
        goto cleanup;  // Don't forward ICE servers to WebRTC layer
    }

    // Handle ICE server response message separately (for index-based requests)
    if (signaling_msg.messageType == SIGNALING_MSG_TYPE_ICE_SERVER_RESPONSE) {
        ESP_LOGI(TAG, "Received ICE server response from signaling device");

        // Extract the ICE server response
        ss_ice_server_response_t ice_server_response;
        esp_err_t extract_result = extract_ice_server_response_from_message(&signaling_msg, &ice_server_response);
        if (extract_result == ESP_OK) {
            ESP_LOGI(TAG, "Successfully extracted ICE server response: %s (have_more: %s)",
                     ice_server_response.urls, ice_server_response.have_more ? "true" : "false");

            // Notify ice_bridge_client.c about the response
            ice_bridge_client_set_ice_server_response(&ice_server_response);
        } else {
            ESP_LOGE(TAG, "Failed to extract ICE server response from message");
        }

        goto cleanup;  // Don't forward ICE server responses to WebRTC layer
    }

    // Convert message type for regular signaling messages
    switch (signaling_msg.messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            webrtc_msg.message_type = WEBRTC_MESSAGE_TYPE_OFFER;
            break;
        case SIGNALING_MSG_TYPE_ANSWER:
            webrtc_msg.message_type = WEBRTC_MESSAGE_TYPE_ANSWER;
            break;
        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            webrtc_msg.message_type = WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE;
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
    WEBRTC_STATUS callback_result = g_bridge_client->on_msg_received((uint64_t)g_bridge_client->user_data, &webrtc_msg);
    if (callback_result != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "g_bridge_client->on_msg_received returned error: 0x%08" PRIx32, (uint32_t) callback_result);
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

    // Debug: preview the serialized JSON we send to the bridge (payload is base64 already)
    const int preview_len = (serialized_len < 1600) ? (int)serialized_len : 1600;
    ESP_LOGD(TAG, "Sending serialized message (%d bytes) via webrtc_bridge", (int)serialized_len);
    ESP_LOGD(TAG, "serialized preview: %.*s", preview_len, serialized_data);

    // Send via webrtc_bridge (it takes ownership of the data)
    webrtc_bridge_send_message(serialized_data, (int)serialized_len);

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling init implementation
 */
static WEBRTC_STATUS bridgeInit(void *signaling_cfg, void **ppSignalingClient)
{
    ESP_LOGI(TAG, "Initializing bridge signaling client");

    if (signaling_cfg == NULL || ppSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    bridge_signaling_config_t* config = (bridge_signaling_config_t*)signaling_cfg;

    // Allocate client data
    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)calloc(1, sizeof(BridgeSignalingClientData));
    if (client_data == NULL) {
        return WEBRTC_STATUS_NOT_ENOUGH_MEMORY;
    }

    // Copy configuration
    memcpy(&client_data->config, config, sizeof(bridge_signaling_config_t));
    client_data->initialized = true;
    client_data->state = WEBRTC_SIGNALING_STATE_NEW;

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
    client_data->state = WEBRTC_SIGNALING_STATE_CONNECTED;

    // Notify state change using portable state type
    if (client_data->on_signaling_state_changed) {
        client_data->on_signaling_state_changed((uint64_t)client_data->user_data, WEBRTC_SIGNALING_STATE_CONNECTED);
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
    client_data->state = WEBRTC_SIGNALING_STATE_DISCONNECTED;

    // Notify state change using portable state type
    if (client_data->on_signaling_state_changed) {
        client_data->on_signaling_state_changed((uint64_t)client_data->user_data, WEBRTC_SIGNALING_STATE_DISCONNECTED);
    }

    ESP_LOGI(TAG, "Bridge signaling client disconnected");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling send message implementation
 */
static WEBRTC_STATUS bridgeSendMessage(void *pSignalingClient, webrtc_message_t* pMessage)
{
    ESP_LOGI(TAG, "Sending message via bridge signaling");

    if (pSignalingClient == NULL || pMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    if (!client_data->initialized || client_data->state != WEBRTC_SIGNALING_STATE_CONNECTED) {
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Convert webrtc_message_t to signaling_msg_t
    signaling_msg_t signaling_msg;
    signaling_msg.version = pMessage->version;

    // Convert message type
    switch (pMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            signaling_msg.messageType = SIGNALING_MSG_TYPE_OFFER;
            break;
        case WEBRTC_MESSAGE_TYPE_ANSWER:
            signaling_msg.messageType = SIGNALING_MSG_TYPE_ANSWER;
            break;
        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
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
                                        WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*),
                                        WEBRTC_STATUS (*on_signaling_state_changed)(uint64_t, webrtc_signaling_state_t),
                                        WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t))
{
    if (pSignalingClient == NULL) {
        ESP_LOGE(TAG, "pSignalingClient is NULL");
        return WEBRTC_STATUS_NULL_ARG;
    }

    BridgeSignalingClientData* client_data = (BridgeSignalingClientData*)pSignalingClient;

    // Set the callbacks
    client_data->on_msg_received = on_msg_received;
    client_data->on_signaling_state_changed = on_signaling_state_changed;
    client_data->on_error = on_error;
    client_data->user_data = customData;

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling set role type implementation (stub)
 */
static WEBRTC_STATUS bridgeSetRoleType(void *pSignalingClient, webrtc_channel_role_type_t role_type)
{
    ESP_LOGI(TAG, "Setting bridge signaling role type: %d", role_type);

    if (pSignalingClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Bridge signaling doesn't need to handle role type differently
    // The role is handled by the actual WebRTC connection
    ESP_LOGI(TAG, "Bridge signaling role type set");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Bridge signaling get ICE servers implementation
 *
 * This function uses the new index-based ICE server retrieval approach,
 * requesting servers one-by-one from the signaling device via bridge.
 *
 * @param pSignalingClient - Bridge signaling client instance
 * @param pIceConfigCount - IN/OUT - Number of ICE servers
 * @param pIceServersArray - OUT - Array of ICE servers (abstraction layer passes the iceServers array directly)
 */
static WEBRTC_STATUS bridgeGetIceServers(void *pSignalingClient, uint32_t *pIceConfigCount, void *pIceServersArray)
{
    ESP_LOGI(TAG, "Getting ICE servers via bridge signaling (index-based)");

    if (pSignalingClient == NULL || pIceConfigCount == NULL || pIceServersArray == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    ESP_LOGI(TAG, "Using generic ICE servers array (abstraction layer handles structure conversion)");

    // Use the new ice_bridge_client to get servers via index-based requests
    // The abstraction layer passes the iceServers array directly, not the full RtcConfiguration
    WEBRTC_STATUS status = ice_bridge_client_get_servers(
        NULL,  // pAppSignaling (unused in streaming-only mode)
        pIceServersArray,  // ICE servers array (compatible with RtcIceServer format)
        pIceConfigCount
    );

    if (status == WEBRTC_STATUS_SUCCESS && *pIceConfigCount > 0) {
        ESP_LOGI(TAG, "Bridge signaling successfully retrieved %" PRIu32 " ICE servers", *pIceConfigCount);
        return WEBRTC_STATUS_SUCCESS;
    } else {
        ESP_LOGW(TAG, "Bridge signaling failed to retrieve ICE servers, status: 0x%08" PRIx32, (uint32_t) status);
        *pIceConfigCount = 0;
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }
}

/**
 * @brief Get the bridge signaling client interface
 */
webrtc_signaling_client_if_t* getBridgeSignalingClientInterface(void)
{
    static webrtc_signaling_client_if_t bridge_interface = {
        .init = bridgeInit,
        .connect = bridgeConnect,
        .disconnect = bridgeDisconnect,
        .send_message = bridgeSendMessage,
        .free = bridgeFree,
        .set_callbacks = bridgeSetCallbacks,
        .set_role_type = bridgeSetRoleType,
        .get_ice_servers = bridgeGetIceServers
    };

    return &bridge_interface;
}
