/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

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
 * @brief Function pointer type for binary command callback
 *
 * @param cmd_id command identifier (must match the registration cmd_id)
 * @param data pointer to the payload bytes (excludes the leading cmd_id byte)
 * @param len length of the payload
 */
typedef void (*webrtc_bridge_binary_cb_t) (uint8_t cmd_id, const uint8_t *data, size_t len);

/**
 * @brief Reserved binary command identifiers carried in the leading byte of a bridge message.
 *
 * Values 0x7B ('{') and 0x5B ('[') are avoided so the dispatcher can fall back
 * to the JSON signaling path by sniffing the first byte.
 */
#define BRIDGE_CMD_SNAPSHOT_REQUEST   0x01  /* payload: 1 byte JPEG quality (1-100) */
#define BRIDGE_CMD_SNAPSHOT_RESPONSE  0x02  /* payload: raw JPEG bytes */

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

/**
 * @brief Send a binary command via the webrtc bridge.
 *
 * Direction-agnostic: the initiator uses this to send a REQUEST, the responder uses it
 * to send the matching RESPONSE. The wire payload is `[cmd_id][data...]` so the receiver
 * can route by cmd_id without colliding with the JSON signaling path (cmd_ids 0x7B/0x5B
 * are reserved for `{` and `[`).
 *
 * cmd_id is the routing key the peer's registered handler will match on. By convention
 * request/response pairs use distinct ids — see `BRIDGE_CMD_SNAPSHOT_REQUEST` (0x01) and
 * `BRIDGE_CMD_SNAPSHOT_RESPONSE` (0x02) for the snapshot demo.
 *
 * @param cmd_id command identifier (1..255, avoid 0x7B and 0x5B)
 * @param data pointer to the payload bytes (may be NULL if len == 0)
 * @param len length of the payload
 */
void webrtc_bridge_send_binary_cmd(uint8_t cmd_id, const uint8_t *data, size_t len);

/**
 * @brief Register a callback for a binary command id.
 *
 * Direction-agnostic: the responder registers for the REQUEST id to handle incoming
 * requests; the initiator registers for the RESPONSE id to handle replies. See
 * `BRIDGE_CMD_SNAPSHOT_REQUEST` / `BRIDGE_CMD_SNAPSHOT_RESPONSE` for the snapshot demo
 * convention. Pass NULL to unregister.
 *
 * @param cmd_id command identifier to dispatch on
 * @param cb callback invoked with payload (cmd_id byte already stripped)
 */
void webrtc_bridge_register_binary_handler(uint8_t cmd_id, webrtc_bridge_binary_cb_t cb);

#ifdef __cplusplus
}
#endif
