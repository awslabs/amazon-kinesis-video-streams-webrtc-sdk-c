
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for WebRTC signaling message callback
 *
 * @param data Pointer to the received message data
 * @param len Length of the received message data
 */
typedef void (*webrtc_message_callback_t)(const void *data, int len);

/**
 * @brief Function pointer type for ICE server query callback
 *
 * @param index ICE server index to query
 * @param data Output pointer to receive ICE server data (caller takes ownership)
 * @param len Output pointer to receive data length
 * @param have_more Output pointer to indicate if more servers are available
 * @return 0 on success, -1 on failure
 */
typedef int (*ice_server_query_callback_t)(int index, uint8_t **data, int *len, bool *have_more);

/**
 * @brief Register a callback for WebRTC signaling messages
 *
 * @param callback Function to call when a WebRTC signaling message is received
 */
void network_coprocessor_register_webrtc_callback(webrtc_message_callback_t callback);

/**
 * @brief Register a callback for ICE server queries via RPC
 *
 * This callback is invoked when the streaming device requests ICE servers
 * via RPC_ID__Req_USR3. The callback should populate ICE server data for
 * the given index and indicate if more servers are available.
 *
 * @param callback Function to call for ICE server queries (NULL to unregister)
 */
void network_coprocessor_register_ice_server_query_callback(ice_server_query_callback_t callback);

/**
 * @brief Initialize the network coprocessor
 */
void network_coprocessor_init(void);

#ifdef __cplusplus
}
#endif
