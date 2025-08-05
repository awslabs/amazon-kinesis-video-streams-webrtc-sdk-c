/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "app_webrtc.h"
#include "signaling_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for signaling bridge adapter
 */
typedef struct {
    void *user_ctx;
} signaling_bridge_adapter_config_t;

/**
 * @brief Initialize the signaling bridge adapter
 *
 * This function initializes the bridge adapter and automatically:
 * - Registers RPC handler for ICE server queries
 * - Sets up bridge message handlers
 * - Configures WebRTC application callbacks
 *
 * @param config Configuration structure
 * @return WEBRTC_STATUS_SUCCESS on success, error code otherwise
 */
WEBRTC_STATUS signaling_bridge_adapter_init(const signaling_bridge_adapter_config_t *config);

/**
 * @brief Start the signaling bridge adapter
 *
 * This function starts the WebRTC bridge and begins handling messages.
 *
 * @return WEBRTC_STATUS_SUCCESS on success, error code otherwise
 */
WEBRTC_STATUS signaling_bridge_adapter_start(void);

/**
 * @brief Send message callback for bridge communication
 *
 * This function sends signaling messages to the streaming device via bridge.
 *
 * @param signalingMessage Message to send
 * @return 0 on success, negative on error
 */
int signaling_bridge_adapter_send_message(signaling_msg_t *signalingMessage);

/**
 * @brief RPC callback handler for ICE server queries (Internal)
 *
 * This function is automatically registered with the network coprocessor
 * as an ICE server query callback for RPC_ID__Req_USR3.
 *
 * @param index ICE server index to query
 * @param data Output pointer to receive ICE server data (caller takes ownership)
 * @param len Output pointer to receive data length
 * @param have_more Output pointer to indicate if more servers are available
 * @return 0 on success, -1 on failure
 */
int signaling_bridge_adapter_rpc_handler(int index, uint8_t **data, int *len, bool *have_more);

/**
 * @brief Deinitialize the signaling bridge adapter
 */
void signaling_bridge_adapter_deinit(void);

#ifdef __cplusplus
}
#endif
