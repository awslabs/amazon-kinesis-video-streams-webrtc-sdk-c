/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APPRTC_SIGNALING_INTERNAL_H
#define APPRTC_SIGNALING_INTERNAL_H

#include "app_webrtc_if.h"
#include "apprtc_signaling.h"
#include <stdbool.h>
#include <stdint.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Process a received message from AppRTC
 *
 * @param message Message to process
 * @param message_len Length of the message
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_process_message(const char *message, size_t message_len);

/**
 * @brief Send a message through the AppRTC signaling server
 *
 * @param message Message to send
 * @param message_len Length of the message
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_send_message(const char *message, size_t message_len);

/**
 * @brief Process queued messages
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t apprtc_signaling_process_queued_messages(void);

/**
 * @brief Helper function to check if AppRTC signaling is connected
 *
 * @return bool true if connected, false otherwise
 */
bool is_apprtc_signaling_connected(void);

/**
 * @brief Helper function to get the current room ID
 *
 * @return char* Current room ID or NULL if not connected
 */
char* apprtc_room_id_get(void);

#ifdef __cplusplus
}
#endif

#endif /* APPRTC_SIGNALING_INTERNAL_H */
