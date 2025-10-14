/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Frame type definitions for MJPEG frames
 */
typedef enum {
    ESP_MJPEG_FRAME_TYPE_JPEG,  // Every frame is a keyframe in MJPEG
} esp_mjpeg_frame_type_t;

/**
 * @brief Structure for MJPEG output buffer
 */
typedef struct {
    uint8_t *buffer;             /*!< Pointer to the encoded data */
    uint32_t len;                /*!< Length of the encoded data */
    esp_mjpeg_frame_type_t type; /*!< Type of the frame */
} esp_mjpeg_out_buf_t;

/**
 * @brief Initialize the camera and MJPEG encoder
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t mjpeg_camera_and_encoder_init(uint16_t width, uint16_t height, uint8_t quality);

/**
 * @brief Get an MJPEG encoded frame
 *
 * @return esp_mjpeg_out_buf_t* Pointer to the MJPEG frame, or NULL if no frame is available
 */
esp_mjpeg_out_buf_t* get_mjpeg_encoded_frame(void);

/**
 * @brief Clean up MJPEG encoder resources
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t mjpeg_encoder_deinit(void);

#ifdef __cplusplus
}
#endif
