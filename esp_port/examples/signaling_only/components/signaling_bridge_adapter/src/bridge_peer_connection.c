/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bridge_peer_connection.c
 * @brief Bridge-only peer connection implementation
 *
 * This file implements a minimal peer connection interface that doesn't
 * initialize the KVS WebRTC SDK or create actual peer connections. It's
 * designed for signaling-only devices that just forward messages to a
 * streaming device via the bridge.
 */

#include "esp_log.h"
#include "app_webrtc.h"
#include <stdlib.h>
#include <string.h>
#include "bridge_peer_connection.h"
#include "signaling_bridge_adapter.h"

static const char *TAG = "bridge_pc";

// Forward declaration of signaling_bridge_adapter functions
extern int signaling_bridge_adapter_send_message(webrtc_message_t *signalingMessage);

/**
 * @brief Bridge peer connection client data
 * Contains minimal state for the bridge peer connection
 */
typedef struct {
    bool initialized;
} bridge_pc_client_t;

/**
 * @brief Set or update ICE servers for a bridge peer connection client
 *
 * This is a minimal implementation that just logs the ICE server update
 * since bridge peer connections don't use actual ICE servers.
 */
static WEBRTC_STATUS bridge_pc_set_ice_servers(void *pPeerConnectionClient, void *ice_servers, uint32_t ice_count)
{
    ESP_LOGI(TAG, "Setting ICE servers for bridge peer connection (ignored, count=%" PRIu32 ")", ice_count);

    if (pPeerConnectionClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    bridge_pc_client_t *client = (bridge_pc_client_t *)pPeerConnectionClient;
    if (!client->initialized) {
        return WEBRTC_STATUS_INVALID_OPERATION;
    }

    // Bridge peer connection doesn't use ICE servers, so we just log and succeed
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Initialize bridge peer connection client
 * This is a minimal implementation that doesn't call initKvsWebRtc
 */
static WEBRTC_STATUS bridge_pc_init(void *pc_cfg, void **ppPeerConnectionClient)
{
    ESP_LOGI(TAG, "Initializing bridge peer connection client");

    // Configure and initialize signaling bridge adapter (handles all bridge communication)
    signaling_bridge_adapter_config_t adapter_config = {
        .user_ctx = NULL,
        .auto_register_callbacks = false
    };

    WEBRTC_STATUS adapter_status = signaling_bridge_adapter_init(&adapter_config);
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to initialize signaling bridge adapter: 0x%08" PRIx32, (uint32_t) adapter_status);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    // Start the signaling bridge adapter (starts bridge and handles all communication)
    adapter_status = signaling_bridge_adapter_start();
    if (adapter_status != WEBRTC_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Failed to start signaling bridge adapter: 0x%08" PRIx32, (uint32_t) adapter_status);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    if (ppPeerConnectionClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Create client data structure
    bridge_pc_client_t *client = (bridge_pc_client_t *)calloc(1, sizeof(bridge_pc_client_t));
    if (client == NULL) {
        return WEBRTC_STATUS_NOT_ENOUGH_MEMORY;
    }

    // Copy configuration if provided
    if (pc_cfg != NULL) {
        ESP_LOGI(TAG, "Bridge peer connection configuration provided");
    }

    client->initialized = true;
    *ppPeerConnectionClient = client;

    ESP_LOGI(TAG, "Bridge peer connection client initialized (no WebRTC SDK initialization)");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Send a WebRTC message via bridge
 * This forwards messages to the bridge without actual peer connection processing
 */
static WEBRTC_STATUS bridge_pc_send_message(void *pSession, webrtc_message_t *pMessage)
{
    if (pMessage == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    ESP_LOGD(TAG, "Bridge peer connection forwarding message type %d to peer %s",
             pMessage->message_type, pMessage->peer_client_id);

    // Use the adapter's send message function to send via bridge
    int result = signaling_bridge_adapter_send_message(pMessage);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to send message via bridge adapter: %d", result);
        return WEBRTC_STATUS_INTERNAL_ERROR;
    }

    ESP_LOGD(TAG, "Sent message via bridge adapter");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Destroy a bridge peer connection session
 * Since bridge doesn't create real sessions, this is a no-op
 */
static WEBRTC_STATUS bridge_pc_destroy_session(void *pSession)
{
    ESP_LOGD(TAG, "Bridge peer connection destroy session called (no-op)");
    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Free bridge peer connection client resources
 */
static WEBRTC_STATUS bridge_pc_free(void *pPeerConnectionClient)
{
    if (pPeerConnectionClient == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    bridge_pc_client_t *client = (bridge_pc_client_t *)pPeerConnectionClient;
    ESP_LOGI(TAG, "Freeing bridge peer connection client");

    // Free client
    free(client);

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Set callbacks for bridge peer connection events
 */
static WEBRTC_STATUS bridge_pc_set_callbacks(void *pSession,
                                           uint64_t custom_data,
                                           WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*),
                                           WEBRTC_STATUS (*on_peer_state_changed)(uint64_t, webrtc_peer_state_t))
{
    ESP_LOGI(TAG, "Setting bridge peer connection callbacks");

    // Register the on_message_received callback with signaling_bridge_adapter
    // This allows the adapter to directly call this callback when messages come from the bridge
    signaling_bridge_adapter_register_callbacks(custom_data, on_message_received);
    ESP_LOGI(TAG, "Registered callbacks with signaling_bridge_adapter (on_message_received: %s)",
             on_message_received ? "provided" : "NULL");

    return WEBRTC_STATUS_SUCCESS;
}

/**
 * @brief Get the bridge peer connection interface
 */
webrtc_peer_connection_if_t* bridge_peer_connection_if_get(void)
{
    static webrtc_peer_connection_if_t bridge_peer_connection_if = {
        .init = bridge_pc_init,
        .set_ice_servers = bridge_pc_set_ice_servers,
        .create_session = NULL,  // Bridge doesn't create real sessions - this flags it as a bridge interface
        .send_message = bridge_pc_send_message,
        .destroy_session = bridge_pc_destroy_session,
        .free = bridge_pc_free,
        .set_callbacks = bridge_pc_set_callbacks,
    };

    return &bridge_peer_connection_if;
}
