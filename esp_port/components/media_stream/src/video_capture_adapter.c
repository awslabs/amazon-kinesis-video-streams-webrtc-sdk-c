/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief Adapter implementation for video_capture.h using H264FrameGrabber
 */

#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "video_capture.h"
#include "H264FrameGrabber.h"
#include "MJPEGFrameGrabber.h"

static const char *TAG = "video_capture_adapter";

typedef struct {
    video_capture_config_t config;
    bool initialized;
    bool running;
    video_codec_type_t codec_type;
} video_capture_context_t;

esp_err_t video_capture_init(video_capture_config_t *config, video_capture_handle_t *ret_handle)
{
    if (config == NULL || ret_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Check supported codecs
    if (config->codec != VIDEO_CODEC_H264 && config->codec != VIDEO_CODEC_MJPEG) {
        ESP_LOGE(TAG, "Only H264 and MJPEG codecs are supported");
        return ESP_ERR_NOT_SUPPORTED;
    }

    video_capture_context_t *ctx = calloc(1, sizeof(video_capture_context_t));
    if (ctx == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Save configuration
    memcpy(&ctx->config, config, sizeof(video_capture_config_t));
    ctx->codec_type = config->codec;
    ctx->initialized = true;
    ctx->running = false;

    // Initialize the camera and encoder based on codec type
    esp_err_t ret = ESP_OK;
    if (config->codec == VIDEO_CODEC_H264) {
        // Initialize H264 encoder with configuration
        ret = camera_and_encoder_init(config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize H264 camera and encoder: %d", ret);
            free(ctx);
            return ret;
        }
    } else if (config->codec == VIDEO_CODEC_MJPEG) {
        // Initialize MJPEG encoder
        ret = mjpeg_camera_and_encoder_init(
            config->resolution.width,
            config->resolution.height,
            config->quality
        );
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize MJPEG camera and encoder: %d", ret);
            free(ctx);
            return ret;
        }
    }

    *ret_handle = ctx;
    return ESP_OK;
}

esp_err_t video_capture_start(video_capture_handle_t handle)
{
    video_capture_context_t *ctx = (video_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    // The camera and encoder are already running after initialization
    ctx->running = true;
    if (ctx->codec_type == VIDEO_CODEC_H264) {
        esp_err_t ret = h264_encoder_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start H264 encoder: %d", ret);
            return ret;
        }
    }
    return ESP_OK;
}

esp_err_t video_capture_stop(video_capture_handle_t handle)
{
    video_capture_context_t *ctx = (video_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    if (ctx->codec_type == VIDEO_CODEC_H264) {
        esp_err_t ret = h264_encoder_stop();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop H264 encoder: %d", ret);
            return ret;
        }
    }
    ctx->running = false;
    return ESP_OK;
}

esp_err_t video_capture_get_frame(video_capture_handle_t handle, video_frame_t **frame, uint32_t wait_ms)
{
    video_capture_context_t *ctx = (video_capture_context_t *)handle;
    if (ctx == NULL || !ctx->initialized || !ctx->running || frame == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate a video_frame_t to return
    video_frame_t *output_frame = calloc(1, sizeof(video_frame_t));
    if (output_frame == NULL) {
        return ESP_ERR_NO_MEM;
    }

    if (ctx->codec_type == VIDEO_CODEC_H264) {
        // Get a frame using the H264 API
        esp_h264_out_buf_t *h264_frame = get_h264_encoded_frame();
        if (h264_frame == NULL) {
            free(output_frame);
            return ESP_ERR_TIMEOUT;
        }

        // Fill in the frame data
        output_frame->buffer = h264_frame->buffer;
        output_frame->len = h264_frame->len;
        output_frame->timestamp = esp_timer_get_time(); // Use current time as timestamp

        // Convert frame type
        switch (h264_frame->type) {
            case ESP_H264_FRAME_TYPE_IDR:
            case ESP_H264_FRAME_TYPE_I:
                output_frame->type = VIDEO_FRAME_TYPE_I;
                break;
            case ESP_H264_FRAME_TYPE_P:
                output_frame->type = VIDEO_FRAME_TYPE_P;
                break;
            default:
                output_frame->type = VIDEO_FRAME_TYPE_OTHER;
                break;
        }
        free(h264_frame);
    } else if (ctx->codec_type == VIDEO_CODEC_MJPEG) {
        // Get a frame using the MJPEG API
        esp_mjpeg_out_buf_t *mjpeg_frame = get_mjpeg_encoded_frame();
        if (mjpeg_frame == NULL) {
            free(output_frame);
            return ESP_ERR_TIMEOUT;
        }

        // Fill in the frame data
        output_frame->buffer = mjpeg_frame->buffer;
        output_frame->len = mjpeg_frame->len;
        output_frame->timestamp = esp_timer_get_time(); // Use current time as timestamp
        output_frame->type = VIDEO_FRAME_TYPE_I; // All MJPEG frames are keyframes

        // Free the mjpeg_frame structure (but not the buffer which is now owned by output_frame)
        free(mjpeg_frame);
    }

    *frame = output_frame;
    return ESP_OK;
}

__attribute__((weak)) esp_err_t video_capture_set_bitrate(video_capture_handle_t handle, uint32_t bitrate_kbps)
{
    return ESP_OK;
}

__attribute__((weak)) esp_err_t video_capture_get_bitrate(video_capture_handle_t handle, uint32_t *bitrate_kbps)
{
    if (bitrate_kbps != NULL) {
        *bitrate_kbps = 500; /* Default bitrate */
    }
    return ESP_OK;
}

esp_err_t video_capture_release_frame(video_capture_handle_t handle, video_frame_t *frame)
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

esp_err_t video_capture_deinit(video_capture_handle_t handle)
{
    video_capture_context_t *ctx = (video_capture_context_t *)handle;
    if (ctx == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Clean up based on codec type
    if (ctx->codec_type == VIDEO_CODEC_MJPEG) {
        mjpeg_encoder_deinit();
    }
    // For H264, there's no explicit deinit function

    // Free our context
    free(ctx);
    return ESP_OK;
}
