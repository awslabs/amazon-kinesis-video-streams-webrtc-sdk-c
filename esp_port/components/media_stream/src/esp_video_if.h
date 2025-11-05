/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#pragma once

#include "esp_err.h"
#include "H264FrameGrabber.h"
#include "video_capture.h"
#include <sys/time.h>

/**
 * @brief Frame buffer structure
 */
typedef struct {
    uint8_t *buf;               /*!< Pointer to the frame data */
    size_t len;                 /*!< Length of the buffer in bytes */
    size_t width;               /*!< Width of the image frame in pixels */
    size_t height;              /*!< Height of the image frame in pixels */
    struct timeval timestamp;   /*!< Timestamp since boot of the frame */
} video_fb_t;

/**
 * @brief Initialize the video interface
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t esp_video_if_init(void);

/**
 * @brief Stop the video interface
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t esp_video_if_stop(void);

/**
 * @brief Start the video interface
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t esp_video_if_start(void);

/**
 * @brief Get a raw video frame
 *
 * @return video_fb_t* Pointer to the raw frame, or NULL if no frame is available
 */
video_fb_t *esp_video_if_get_frame(void);

/**
 * @brief Release a video frame when done with it
 *
 * @param fb Pointer to the frame to release
 */
void esp_video_if_release_frame(video_fb_t *fb);

/**
 * @brief Get the current video resolution
 *
 * @param resolution Pointer to store the current resolution
 * @return esp_err_t
 *  - ESP_OK: Successfully retrieved resolution
 *  - ESP_ERR_INVALID_ARG: resolution pointer is NULL
 *  - ESP_ERR_INVALID_STATE: Video interface not initialized
 */
esp_err_t esp_video_if_get_resolution(video_resolution_t *resolution);
