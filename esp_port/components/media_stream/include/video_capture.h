/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Generic video capture interface for camera input and encoding
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video codec type enumeration
 */
typedef enum {
    VIDEO_CODEC_H264,
    VIDEO_CODEC_MJPEG,
    VIDEO_CODEC_RAW,
    /* Add more codecs as needed */
} video_codec_type_t;

/**
 * @brief Video resolution configuration
 */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t fps;
} video_resolution_t;

/**
 * @brief Frame type for encoded video
 */
typedef enum {
    VIDEO_FRAME_TYPE_I,    /* I-frame */
    VIDEO_FRAME_TYPE_P,    /* P-frame */
    VIDEO_FRAME_TYPE_B,    /* B-frame */
    VIDEO_FRAME_TYPE_OTHER /* Other frame types */
} video_frame_type_t;

/**
 * @brief Video capture configuration
 */
typedef struct {
    video_codec_type_t codec;
    video_resolution_t resolution;
    uint8_t quality;       /* 0-100, higher is better quality */
    uint32_t bitrate;      /* Target bitrate in kbps */
    void *codec_specific;  /* Codec-specific parameters if needed */
} video_capture_config_t;

/**
 * @brief Video frame buffer structure
 */
typedef struct {
    uint8_t *buffer;        /* Data buffer */
    uint32_t len;           /* Buffer length in bytes */
    uint64_t timestamp;     /* Timestamp in microseconds */
    video_frame_type_t type; /* Frame type */
} video_frame_t;

/**
 * @brief Video capture handle
 */
typedef void* video_capture_handle_t;

/**
 * @brief Initialize video capture with specified configuration
 *
 * @param config Video capture configuration
 * @param ret_handle Pointer to store the created handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_init(video_capture_config_t *config, video_capture_handle_t *ret_handle);

/**
 * @brief Start video capture
 *
 * @param handle Video capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_start(video_capture_handle_t handle);

/**
 * @brief Stop video capture
 *
 * @param handle Video capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_stop(video_capture_handle_t handle);

/**
 * @brief Get the next captured video frame
 *
 * @param handle Video capture handle
 * @param frame Pointer to store the video frame
 * @param wait_ms Time to wait for a frame in milliseconds (0 for non-blocking)
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT if no frame available, otherwise an error code
 */
esp_err_t video_capture_get_frame(video_capture_handle_t handle, video_frame_t **frame, uint32_t wait_ms);

/**
 * @brief Release a video frame when no longer needed
 *
 * @param handle Video capture handle
 * @param frame Frame to release
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_release_frame(video_capture_handle_t handle, video_frame_t *frame);

/**
 * @brief Deinitialize video capture and free resources
 *
 * @param handle Video capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_deinit(video_capture_handle_t handle);

#ifdef __cplusplus
}
#endif
