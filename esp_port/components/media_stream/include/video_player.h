/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Generic video player interface for decoding and playback
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Video codec type enumeration for playback
 */
typedef enum {
    VIDEO_PLAYER_CODEC_H264,
    VIDEO_PLAYER_CODEC_MJPEG,
    /* Add more codecs as needed */
} video_player_codec_t;

/**
 * @brief Video output format configuration
 */
typedef struct {
    uint16_t width;          /* Width of output video */
    uint16_t height;         /* Height of output video */
    uint8_t framerate;       /* Target framerate for playback */
} video_player_format_t;

/**
 * @brief Video player configuration
 */
typedef struct {
    video_player_codec_t codec;  /* Video codec to use for decoding */
    video_player_format_t format; /* Output video format */
    uint16_t buffer_frames;      /* Number of frames to buffer */
    void *codec_specific;        /* Codec-specific parameters if needed */
    void *display_handle;        /* Display handle for output (optional) */
} video_player_config_t;

/**
 * @brief Video player handle
 */
typedef void* video_player_handle_t;

/**
 * @brief Initialize video player with specified configuration
 *
 * @param config Video player configuration
 * @param ret_handle Pointer to store the created handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_init(video_player_config_t *config, video_player_handle_t *ret_handle);

/**
 * @brief Start video playback
 *
 * @param handle Video player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_start(video_player_handle_t handle);

/**
 * @brief Stop video playback
 *
 * @param handle Video player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_stop(video_player_handle_t handle);

/**
 * @brief Decode and queue a video frame for playback
 *
 * @param handle Video player handle
 * @param data Pointer to encoded video data
 * @param len Length of encoded video data in bytes
 * @param is_keyframe Whether this frame is a keyframe (I-frame)
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_play_frame(video_player_handle_t handle, const uint8_t *data, uint32_t len, bool is_keyframe);

/**
 * @brief Clear the video playback buffer
 *
 * @param handle Video player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_clear_buffer(video_player_handle_t handle);

/**
 * @brief Get current buffer status (how many frames are queued)
 *
 * @param handle Video player handle
 * @param available_frames Pointer to store available frames count
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_get_buffer_status(video_player_handle_t handle, uint32_t *available_frames);

/**
 * @brief Deinitialize video player and free resources
 *
 * @param handle Video player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t video_player_deinit(video_player_handle_t handle);

#ifdef __cplusplus
}
#endif
