/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Generic audio capture interface for microphone input and encoding
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio codec type enumeration
 */
typedef enum {
    AUDIO_CODEC_OPUS,
    AUDIO_CODEC_PCM,
    AUDIO_CODEC_AAC,
    /* Add more codecs as needed */
} audio_codec_type_t;

/**
 * @brief Audio format configuration
 */
typedef struct {
    uint32_t sample_rate;    /* Sample rate in Hz (e.g., 16000, 44100) */
    uint8_t channels;        /* Number of channels (1=mono, 2=stereo) */
    uint16_t bits_per_sample; /* Bits per sample (8, 16, 24, 32) */
} audio_format_t;

/**
 * @brief Audio capture configuration
 */
typedef struct {
    audio_codec_type_t codec;     /* Audio codec to use */
    audio_format_t format;        /* Audio format */
    uint32_t bitrate;             /* Target bitrate in kbps (for compressed formats) */
    uint16_t frame_duration_ms;   /* Frame duration in milliseconds */
    void *codec_specific;         /* Codec-specific parameters if needed */
} audio_capture_config_t;

/**
 * @brief Audio frame buffer structure
 */
typedef struct {
    uint8_t *buffer;        /* Data buffer */
    uint32_t len;           /* Buffer length in bytes */
    uint64_t timestamp;     /* Timestamp in microseconds */
} audio_frame_t;

/**
 * @brief Audio capture handle
 */
typedef void* audio_capture_handle_t;

/**
 * @brief Initialize audio capture with specified configuration
 *
 * @param config Audio capture configuration
 * @param ret_handle Pointer to store the created handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_capture_init(audio_capture_config_t *config, audio_capture_handle_t *ret_handle);

/**
 * @brief Start audio capture
 *
 * @param handle Audio capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_capture_start(audio_capture_handle_t handle);

/**
 * @brief Stop audio capture
 *
 * @param handle Audio capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_capture_stop(audio_capture_handle_t handle);

/**
 * @brief Get the next captured audio frame
 *
 * @param handle Audio capture handle
 * @param frame Pointer to store the audio frame
 * @param wait_ms Time to wait for a frame in milliseconds (0 for non-blocking)
 * @return esp_err_t ESP_OK on success, ESP_ERR_TIMEOUT if no frame available, otherwise an error code
 */
esp_err_t audio_capture_get_frame(audio_capture_handle_t handle, audio_frame_t **frame, uint32_t wait_ms);

/**
 * @brief Release an audio frame when no longer needed
 *
 * @param handle Audio capture handle
 * @param frame Frame to release
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_capture_release_frame(audio_capture_handle_t handle, audio_frame_t *frame);

/**
 * @brief Deinitialize audio capture and free resources
 *
 * @param handle Audio capture handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_capture_deinit(audio_capture_handle_t handle);

#ifdef __cplusplus
}
#endif
