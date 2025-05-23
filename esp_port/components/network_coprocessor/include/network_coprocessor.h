
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Function pointer type for WebRTC message callback
 *
 * @param data Pointer to the received message data
 * @param len Length of the received message data
 */
typedef void (*webrtc_message_callback_t)(const void *data, int len);

/**
 * @brief Register a callback for WebRTC messages
 *
 * @param callback Function to call when a WebRTC message is received
 */
void network_coprocessor_register_webrtc_callback(webrtc_message_callback_t callback);

/**
 * @brief Initialize the network coprocessor
 */
void network_coprocessor_init(void);

#ifdef __cplusplus
}
#endif
