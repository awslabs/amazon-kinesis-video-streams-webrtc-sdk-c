/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Adapter implementation for audio_capture.h using OpusFrameGrabber
 */

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "audio_capture.h"
#include "OpusFrameGrabber.h"

static const char *TAG = "audio_capture_adapter";

typedef struct {
    audio_capture_config_t config;
    bool initialized;
    bool running;
} audio_capture_context_t;

esp_err_t audio_capture_init(audio_capture_config_t *config, audio_capture_handle_t *ret_handle)
{
    if (config == NULL || ret_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Currently only OPUS is supported
    if (config->codec != AUDIO_CODEC_OPUS) {
        ESP_LOGE(TAG, "Only OPUS codec is supported");
        return ESP_ERR_NOT_SUPPORTED;
    }

    audio_capture_context_t *ctx = calloc(1, sizeof(audio_capture_context_t));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Save configuration
    memcpy(&ctx->config, config, sizeof(audio_capture_config_t));
    ctx->initialized = true;
    ctx->running = false;

    opus_encoder_init_internal(config);

    *ret_handle = ctx;
    return ESP_OK;
}

esp_err_t audio_capture_start(audio_capture_handle_t handle)
{
    audio_capture_context_t *ctx = (audio_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // The microphone and encoder are already running when frames are requested
    ctx->running = true;
    return ESP_OK;
}

esp_err_t audio_capture_stop(audio_capture_handle_t handle)
{
    audio_capture_context_t *ctx = (audio_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // There's no explicit stop in the current implementation
    // We'll just mark it as not running
    ctx->running = false;
    return ESP_OK;
}

esp_err_t audio_capture_get_frame(audio_capture_handle_t handle, audio_frame_t **frame, uint32_t wait_ms)
{
    audio_capture_context_t *ctx = (audio_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || !ctx->running || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Get a frame using the existing API
    esp_opus_out_buf_t *opus_frame = get_opus_encoded_frame();
    if (opus_frame == NULL) {
        return ESP_ERR_TIMEOUT;
    }

    // Allocate an audio_frame_t to return
    audio_frame_t *output_frame = calloc(1, sizeof(audio_frame_t));
    if (output_frame == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Fill in the frame data
    output_frame->buffer = opus_frame->buffer;
    output_frame->len = opus_frame->len;
    output_frame->timestamp = esp_timer_get_time(); // Use current time as timestamp

    *frame = output_frame;
    return ESP_OK;
}

esp_err_t audio_capture_release_frame(audio_capture_handle_t handle, audio_frame_t *frame)
{
    if (handle == NULL || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (frame->buffer != NULL) {
        free(frame->buffer);
    }
    free(frame);
    return ESP_OK;
}

esp_err_t audio_capture_deinit(audio_capture_handle_t handle)
{
    audio_capture_context_t *ctx = (audio_capture_context_t *)handle;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // The current implementation doesn't have a deinit function
    // So we'll just free our context
    free(ctx);
    return ESP_OK;
}
