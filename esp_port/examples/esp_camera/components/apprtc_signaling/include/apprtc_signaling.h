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
#include "app_webrtc_if.h"

typedef void (*apprtc_signaling_message_handler_t)(const char *message, size_t message_len, void *user_data);
typedef void (*apprtc_signaling_state_change_handler_t)(int state, void *user_data);

typedef enum {
    APPRTC_SIGNALING_STATE_DISCONNECTED = 0,
    APPRTC_SIGNALING_STATE_CONNECTING,
    APPRTC_SIGNALING_STATE_CONNECTED,
    APPRTC_SIGNALING_STATE_ERROR
} apprtc_signaling_state_t;

/**
 * @brief AppRTC Signaling configuration structure
 */
typedef struct {
    // Server configuration
    char *serverUrl;                    // AppRTC server URL (optional, uses default if NULL)
    char *roomId;                       // Room ID to join (NULL to create new room)

    // Connection options
    bool autoConnect;                   // Whether to auto-connect on init
    uint32_t connectionTimeout;         // Connection timeout in milliseconds

    // Logging
    uint32_t logLevel;                  // Log level for AppRTC signaling
} apprtc_signaling_config_t;

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
 * @param room_id Room ID to connect to
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
 * @brief Get the current state of the AppRTC signaling client
 *
 * @return apprtc_signaling_state_t Current state
 */
apprtc_signaling_state_t apprtc_signaling_get_state(void);

/**
 * @brief Get the room ID of the current connection
 *
 * @return const char* Room ID or NULL if not connected
 */
const char* apprtc_signaling_get_room_id(void);

/**
 * @brief Send a WebRTC message through the AppRTC signaling server
 *
 * @param pWebrtcMessage WebRTC message to send
 * @return int 0 on success, non-zero on failure
 */
int apprtc_signaling_send_webrtc_message(webrtc_message_t *pWebrtcMessage);

/**
 * @brief Deinitialize the AppRTC signaling client
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_deinit(void);

/**
 * @brief Get the AppRTC signaling client interface
 *
 * @return webrtc_signaling_client_if_t* Pointer to the AppRTC signaling interface
 */
webrtc_signaling_client_if_t* apprtc_signaling_client_if_get(void);

#endif /* APPRTC_SIGNALING_H */
