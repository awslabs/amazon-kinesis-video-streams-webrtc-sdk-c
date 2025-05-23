/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Adapter implementation for audio_player.h using OpusAudioPlayer
 */

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "audio_player.h"
#include "OpusAudioPlayer.h"

static const char *TAG = "audio_player_adapter";

typedef struct {
    audio_player_config_t config;
    bool initialized;
    bool running;
} audio_player_context_t;

esp_err_t audio_player_init(audio_player_config_t *config, audio_player_handle_t *ret_handle)
{
    if (config == NULL || ret_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Currently only OPUS is supported
    if (config->codec != AUDIO_PLAYER_CODEC_OPUS) {
        ESP_LOGE(TAG, "Only OPUS codec is supported");
        return ESP_ERR_NOT_SUPPORTED;
    }

    audio_player_context_t *ctx = calloc(1, sizeof(audio_player_context_t));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Save configuration
    memcpy(&ctx->config, config, sizeof(audio_player_config_t));
    ctx->initialized = false;
    ctx->running = false;

    // Initialize the OPUS audio player
    esp_err_t ret = OpusAudioPlayerInit();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OPUS audio player: %d", ret);
        free(ctx);
        return ret;
    }

    ctx->initialized = true;
    *ret_handle = ctx;
    return ESP_OK;
}

esp_err_t audio_player_start(audio_player_handle_t handle)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // The audio player is automatically started when frames are decoded
    ctx->running = true;
    return ESP_OK;
}

esp_err_t audio_player_stop(audio_player_handle_t handle)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // There's no explicit stop in the current implementation
    ctx->running = false;
    return ESP_OK;
}

esp_err_t audio_player_play_frame(audio_player_handle_t handle, const uint8_t *data, uint32_t len)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || !ctx->running || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Use the existing API to decode and play the frame
    return OpusAudioPlayerDecode((uint8_t *)data, len);
}

esp_err_t audio_player_clear_buffer(audio_player_handle_t handle)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // The current implementation doesn't have a clear buffer function
    // This is a no-op for now
    return ESP_OK;
}

esp_err_t audio_player_get_buffer_status(audio_player_handle_t handle, uint32_t *available_ms)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || available_ms == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // The current implementation doesn't provide buffer status
    // Return a default value
    *available_ms = 0;
    return ESP_OK;
}

esp_err_t audio_player_deinit(audio_player_handle_t handle)
{
    audio_player_context_t *ctx = (audio_player_context_t *)handle;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->initialized) {
        // Deinitialize the OPUS audio player
        esp_err_t ret = OpusAudioPlayerDeinit();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to deinitialize OPUS audio player: %d", ret);
            // Continue anyway to clean up our resources
        }
    }

    free(ctx);
    return ESP_OK;
}
