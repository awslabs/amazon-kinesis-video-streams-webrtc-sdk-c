/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Generic audio player interface for decoding and playback
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Audio codec type enumeration for playback
 */
typedef enum {
    AUDIO_PLAYER_CODEC_OPUS,
    AUDIO_PLAYER_CODEC_PCM,
    AUDIO_PLAYER_CODEC_AAC,
    /* Add more codecs as needed */
} audio_player_codec_t;

/**
 * @brief Audio output format configuration
 */
typedef struct {
    uint32_t sample_rate;     /* Sample rate in Hz (e.g., 16000, 44100) */
    uint8_t channels;         /* Number of channels (1=mono, 2=stereo) */
    uint16_t bits_per_sample; /* Bits per sample (8, 16, 24, 32) */
} audio_player_format_t;

/**
 * @brief Audio player configuration
 */
typedef struct {
    audio_player_codec_t codec;  /* Audio codec to use for decoding */
    audio_player_format_t format; /* Output audio format */
    uint16_t buffer_ms;          /* Internal buffer size in milliseconds */
    void *codec_specific;        /* Codec-specific parameters if needed */
} audio_player_config_t;

/**
 * @brief Audio player handle
 */
typedef void* audio_player_handle_t;

/**
 * @brief Initialize audio player with specified configuration
 *
 * @param config Audio player configuration
 * @param ret_handle Pointer to store the created handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_init(audio_player_config_t *config, audio_player_handle_t *ret_handle);

/**
 * @brief Start audio playback
 *
 * @param handle Audio player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_start(audio_player_handle_t handle);

/**
 * @brief Stop audio playback
 *
 * @param handle Audio player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_stop(audio_player_handle_t handle);

/**
 * @brief Decode and queue an encoded audio frame for playback
 *
 * @param handle Audio player handle
 * @param data Pointer to encoded audio data
 * @param len Length of encoded audio data in bytes
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_play_frame(audio_player_handle_t handle, const uint8_t *data, uint32_t len);

/**
 * @brief Clear the audio playback buffer
 *
 * @param handle Audio player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_clear_buffer(audio_player_handle_t handle);

/**
 * @brief Get current buffer status (how much audio is queued)
 *
 * @param handle Audio player handle
 * @param available_ms Pointer to store available playback time in milliseconds
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_get_buffer_status(audio_player_handle_t handle, uint32_t *available_ms);

/**
 * @brief Deinitialize audio player and free resources
 *
 * @param handle Audio player handle
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t audio_player_deinit(audio_player_handle_t handle);

#ifdef __cplusplus
}
#endif
