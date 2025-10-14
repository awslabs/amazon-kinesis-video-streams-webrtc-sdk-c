/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdlib.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Structure to hold received message data
 */
typedef struct received_msg {
    char *buf;           /*!< Buffer to store message data */
    int capacity;        /*!< Total capacity of the buffer */
    int data_size;       /*!< Current size of data in the buffer */
} received_msg_t;

/**
 * @brief Creates a buffer for received messages
 *
 * @param capacity Size of the buffer to create
 * @return Pointer to the created buffer structure or NULL if failed
 */
received_msg_t *esp_webrtc_create_buffer_for_msg(int capacity);

/**
 * @brief Appends message data to an existing buffer
 *
 * @param dst_msg Destination message buffer
 * @param data_ptr Pointer to the data to append
 * @param data_len Length of the data to append
 * @param is_fin Flag indicating if this is the final fragment
 * @return ESP_OK if message is complete, ESP_ERR_NOT_FINISHED if more fragments expected, ESP_FAIL on error
 */
esp_err_t esp_webrtc_append_msg_to_existing(received_msg_t *dst_msg, void *data_ptr, int data_len, bool is_fin);

#ifdef __cplusplus
}
#endif
