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

#ifdef __cplusplus
}
#endif

#endif /* __BRIDGE_PEER_CONNECTION_H__ */
