/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for WebRTC bridge message callback
 *
 * @param data Pointer to the received message data
 * @param len Length of the received message data
 */
typedef void (*webrtc_bridge_msg_cb_t) (const void *data, int len);

/**
 * @brief Start the webrtc bridge
 */
void webrtc_bridge_start(void);

/**
 * @brief Send message via webrtc bridge
 *
 * @param data pointer to the data to send
 * @param len length of the data to send
 *
 * @note the data is freed by the webrtc bridge, hence do not free it in the caller
 */
void webrtc_bridge_send_message(const char *data, int len);

/**
 * @brief Register a message handler for the webrtc bridge
 *
 * @param handler function pointer to the message handler
 */
void webrtc_bridge_register_handler(webrtc_bridge_msg_cb_t handler);

#ifdef __cplusplus
}
#endif
