/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Stub adapter implementation for video_player.h
 * Note: This is a placeholder implementation since no actual video player exists yet
 */

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "video_player.h"

static const char *TAG = "video_player_adapter";

typedef struct {
    video_player_config_t config;
    bool initialized;
    bool running;
} video_player_context_t;

esp_err_t video_player_init(video_player_config_t *config, video_player_handle_t *ret_handle)
{
    if (config == NULL || ret_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // This is a stub implementation with no actual functionality
    ESP_LOGW(TAG, "video_player_init: Stub implementation, no actual playback will occur");

    video_player_context_t *ctx = calloc(1, sizeof(video_player_context_t));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Save configuration
    memcpy(&ctx->config, config, sizeof(video_player_config_t));
    ctx->initialized = true;
    ctx->running = false;

    *ret_handle = ctx;
    return ESP_OK;
}

esp_err_t video_player_start(video_player_handle_t handle)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "video_player_start: Stub implementation, no actual playback will occur");
    ctx->running = true;
    return ESP_OK;
}

esp_err_t video_player_stop(video_player_handle_t handle)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "video_player_stop: Stub implementation");
    ctx->running = false;
    return ESP_OK;
}

esp_err_t video_player_play_frame(video_player_handle_t handle, const uint8_t *data, uint32_t len, bool is_keyframe)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || !ctx->running || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // In a real implementation, this would decode and display the frame
    ESP_LOGD(TAG, "video_player_play_frame: Stub implementation, frame of %d bytes %s",
             (int) len, is_keyframe ? "(keyframe)" : "");
    return ESP_OK;
}

esp_err_t video_player_clear_buffer(video_player_handle_t handle)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "video_player_clear_buffer: Stub implementation");
    return ESP_OK;
}

esp_err_t video_player_get_buffer_status(video_player_handle_t handle, uint32_t *available_frames)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || available_frames == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Always report empty buffer
    *available_frames = 0;
    return ESP_OK;
}

esp_err_t video_player_deinit(video_player_handle_t handle)
{
    video_player_context_t *ctx = (video_player_context_t *)handle;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGW(TAG, "video_player_deinit: Stub implementation");
    free(ctx);
    return ESP_OK;
}
