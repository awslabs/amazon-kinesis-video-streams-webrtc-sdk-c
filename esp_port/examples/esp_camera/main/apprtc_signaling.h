/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APPRTC_SIGNALING_H
#define APPRTC_SIGNALING_H

#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>
#include "signaling_serializer.h"
#include "message_queue.h"
#include "signaling_conversion.h"

// Forward declaration for the message handler
typedef void (*apprtc_signaling_message_handler_t)(const char *message, size_t message_len, void *user_data);

// Forward declaration for the state change handler
typedef void (*apprtc_signaling_state_change_handler_t)(int state, void *user_data);

// Define missing types
// typedef int STATUS;
// typedef struct IceConfigInfo {
//     // Add necessary fields here
//     int dummy;
// } IceConfigInfo, *PIceConfigInfo;

// Signaling states
typedef enum {
    APPRTC_SIGNALING_STATE_DISCONNECTED = 0,
    APPRTC_SIGNALING_STATE_CONNECTING,
    APPRTC_SIGNALING_STATE_CONNECTED,
    APPRTC_SIGNALING_STATE_ERROR
} apprtc_signaling_state_t;

/**
 * @brief Initialize the AppRTC signaling client
 *
 * @param message_handler Callback for received messages
 * @param state_handler Callback for state changes
 * @param user_data User data to pass to callbacks
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_init(apprtc_signaling_message_handler_t message_handler,
                               apprtc_signaling_state_change_handler_t state_handler,
                               void* user_data);

/**
 * @brief Connect to the AppRTC signaling server
 *
 * @param room_id Room ID to join, or NULL to create a new room
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_connect(const char *room_id);

/**
 * @brief Disconnect from the AppRTC signaling server
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_disconnect(void);

/**
 * @brief Send a message through the AppRTC signaling server
 *
 * @param message Message to send
 * @param message_len Length of the message
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_send_message(const char *message, size_t message_len);

/**
 * @brief Get the current room ID
 *
 * @return const char* Current room ID or NULL if not connected
 */
const char* apprtc_signaling_get_room_id(void);

/**
 * @brief Get the current signaling state
 *
 * @return apprtc_signaling_state_t Current signaling state
 */
apprtc_signaling_state_t apprtc_signaling_get_state(void);

/**
 * @brief Get ICE servers for WebRTC configuration
 *
 * @param unused Unused parameter (to match required function signature)
 * @param pIceConfigInfo Output ICE configuration
 * @return STATUS STATUS_SUCCESS on success
 */
// STATUS apprtc_signaling_get_ice_servers(uint64_t unused, PIceConfigInfo pIceConfigInfo);

/**
 * @brief Send a custom command through the signaling channel
 *
 * @param cmd The custom command to send
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_send_custom_command(const char *cmd);

/**
 * @brief Process a received message from AppRTC and forward it to WebRTC SDK
 *
 * This function extracts the inner message if needed, converts it to signaling_msg_t format,
 * and forwards it to webrtcAppSignalingMessageReceived.
 *
 * @param message Message from AppRTC signaling
 * @param message_len Length of the message
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_process_message(const char *message, size_t message_len);

/**
 * @brief Register the send callback with WebRTC SDK
 *
 * This function registers the apprtc_signaling_send_callback with the WebRTC SDK
 * to handle sending messages from the SDK via AppRTC signaling.
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_register_with_webrtc(void);

/**
 * @brief Queue a message to be sent when the signaling channel is connected
 *
 * @param data The message data to queue
 * @param len The length of the message data
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_queue_message(const void *data, int len);

/**
 * @brief Process any queued messages that were stored while disconnected
 *
 * @return esp_err_t ESP_OK on success, error code otherwise
 */
esp_err_t apprtc_signaling_process_queued_messages(void);

/**
 * @brief AppRTC signaling send callback function
 *
 * This function is used to send signaling messages via AppRTC.
 * Return values: 0 = success (sent immediately), 1 = success (queued), -1 = error
 *
 * @param pSignalingMsg The signaling message to send
 * @return int Status code: 0/1 for success, negative for error
 */
int apprtc_signaling_send_callback(signaling_msg_t *pSignalingMsg);

#endif /* APPRTC_SIGNALING_H */
