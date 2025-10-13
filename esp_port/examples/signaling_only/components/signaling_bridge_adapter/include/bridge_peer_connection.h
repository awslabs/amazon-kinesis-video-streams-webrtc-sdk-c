/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BRIDGE_PEER_CONNECTION_H__
#define __BRIDGE_PEER_CONNECTION_H__

#include "app_webrtc_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for bridge peer connection
 *
 * This is a minimal configuration for the bridge-only peer connection interface.
 * It doesn't require any media interfaces or WebRTC SDK initialization.
 */
typedef struct {
    void *bridge_ctx;                      // Optional context for bridge operations
} bridge_peer_connection_config_t;

/**
 * @brief Get the bridge peer connection interface
 *
 * This returns a peer connection interface that implements a bridge-only
 * peer connection. It doesn't initialize the KVS WebRTC SDK or any media
 * components, making it ideal for signaling-only devices.
 *
 * The implementation in bridge_peer_connection.c provides:
 * - No WebRTC SDK initialization (no initKvsWebRtc call)
 * - No peer connection creation (just minimal session tracking)
 * - Message forwarding to bridge for all SDP/ICE messages
 * - Proper callback registration for receiving messages from bridge
 *
 * This is used for:
 * - signaling_only mode (with bridge)
 * - Any scenario requiring pure message passing without peer connections
 *
 * @return Pointer to the bridge peer connection interface
 */
webrtc_peer_connection_if_t* bridge_peer_connection_if_get(void);

/**
 * @brief Trigger P4 wake-up and queue subsequent messages
 *
 * This function sets the message queue state to WAITING_FOR_WAKEUP, causing
 * subsequent messages to be queued until P4 sends a READY signal.
 *
 * @note This function is called automatically by signaling_bridge_adapter_send_message()
 * when an OFFER is received. Applications typically don't need to call this directly.
 *
 * Simple and Robust Approach (Always Wake on OFFER):
 * 1. OFFER arrives → ALWAYS transition to WAITING_FOR_WAKEUP state
 * 2. Call wakeup_host() - physically wake P4 if sleeping
 * 3. Send READY_QUERY - ask P4 for readiness (critical!)
 * 4. Enqueue OFFER and subsequent messages
 * 5. P4 responds with READY signal
 * 6. Queue is flushed automatically when READY received
 *
 * Benefits:
 * - No race conditions (don't trust power state detection)
 * - P4 handler always ready when messages arrive
 * - Works whether P4 is sleeping, waking, or already awake
 * - READY_QUERY ensures response even if P4 already initialized
 */
void signaling_bridge_adapter_trigger_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif /* __BRIDGE_PEER_CONNECTION_H__ */
