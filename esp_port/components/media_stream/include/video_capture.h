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

typedef enum {
    PIXFMT_YUV422,    // 2BPP/YUV422
    PIXFMT_YUV420,    // 1.5BPP/YUV420
    PIXFMT_OTHER,
} video_frame_pixformat_t;

typedef struct {
    uint8_t *buffer;
    uint16_t height;
    uint16_t width;
    size_t len;
    video_frame_pixformat_t pixfmt;
} video_frame_raw_t;

/**
 * @brief Callback to preprocess raw frame before h264 encoding
 *
 * @param frame_raw Raw frame data
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
typedef esp_err_t (*video_frame_preprocess_fn_t)(video_frame_raw_t frame_raw);

/**
 * @brief Video capture configuration
 */
typedef struct {
    video_codec_type_t codec;
    video_resolution_t resolution;
    uint8_t quality;            /* 0-100, higher is better quality */
    uint32_t bitrate;           /* Target bitrate in kbps */
    video_frame_preprocess_fn_t
        frame_preprocess_fn;    /* Callback to preprocess raw frame */
    void *codec_specific;       /* Codec-specific parameters if needed */
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
 * @brief Set the video bitrate dynamically
 *
 * @param handle Video capture handle
 * @param bitrate_kbps New bitrate in kbps
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_set_bitrate(video_capture_handle_t handle, uint32_t bitrate_kbps);

/**
 * @brief Get the current video bitrate
 *
 * @param handle Video capture handle
 * @param bitrate_kbps Pointer to store current bitrate in kbps
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_get_bitrate(video_capture_handle_t handle, uint32_t *bitrate_kbps);

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

/**
 * @brief Capture a single JPEG snapshot
 *
 * When H264 streaming is active, intercepts the next raw frame from the
 * encoder pipeline and JPEG-encodes it (no camera sharing issues).
 * When streaming is NOT active but camera is initialized, directly grabs
 * a raw frame from the camera and JPEG-encodes it.
 * When camera is not initialized, temporarily initializes it, captures
 * one frame, encodes, and cleans up.
 *
 * Caller must free the returned buffer with video_capture_snapshot_free().
 *
 * @param jpeg_buf[out]  Pointer to store the allocated JPEG buffer
 * @param jpeg_len[out]  Pointer to store the JPEG data length
 * @param quality        JPEG quality (1-100, higher is better)
 * @param timeout_ms     Maximum time to wait for a frame in milliseconds
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_capture_get_snapshot(uint8_t **jpeg_buf, size_t *jpeg_len,
                                     uint8_t quality, uint32_t timeout_ms);

/**
 * @brief Free a JPEG snapshot buffer allocated by video_capture_get_snapshot()
 *
 * @param jpeg_buf Pointer to the JPEG buffer to free
 */
void video_capture_snapshot_free(uint8_t *jpeg_buf);

/**
 * @brief Check if the H264 encoder is currently running (actively encoding)
 *
 * @return true if the encoder task is running, false otherwise
 */
bool h264_encoder_is_running(void);

/**
 * @brief Check if the H264 encoder has been initialized
 *
 * @return true if initialized, false otherwise
 */
bool h264_encoder_is_initialized(void);

#ifdef __cplusplus
}
#endif
