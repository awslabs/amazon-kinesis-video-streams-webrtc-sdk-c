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
#include "esp_heap_caps.h"
#include "video_capture.h"
#include "H264FrameGrabber.h"
#include "MJPEGFrameGrabber.h"

#if CONFIG_IDF_TARGET_ESP32P4
#include "driver/jpeg_encode.h"
#include "esp_video_if.h"

/* Extern declarations for P4 snapshot interceptor functions */
extern esp_err_t esp32p4_snapshot_intercept_frame(uint8_t *buf, size_t buf_size,
                                                   size_t *len, uint16_t *width,
                                                   uint16_t *height,
                                                   video_frame_pixformat_t *pixfmt,
                                                   uint32_t timeout_ms);
extern esp_err_t esp32p4_snapshot_direct_grab(uint8_t *buf, size_t buf_size,
                                               size_t *len, uint16_t *width,
                                               uint16_t *height,
                                               video_frame_pixformat_t *pixfmt,
                                               uint32_t timeout_ms);
#endif

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
    } else if (ctx->codec_type == VIDEO_CODEC_H264) {
        // Deinitialize H264 encoder and camera hardware
        h264_encoder_deinit();
    }

    // Free our context
    free(ctx);
    return ESP_OK;
}

/* -------------------------------------------------------------------------- */
/*  JPEG Snapshot                                                             */
/* -------------------------------------------------------------------------- */

#if CONFIG_IDF_TARGET_ESP32P4

/**
 * One-shot JPEG encode: creates a HW JPEG encoder, encodes one frame, and
 * deletes the encoder. Suitable for infrequent snapshot captures.
 *
 * @param yuv_buf     Input YUV420 buffer
 * @param yuv_len     Length of the input buffer
 * @param width       Image width
 * @param height      Image height
 * @param quality     JPEG quality (1-100)
 * @param jpeg_out    Pointer to store allocated JPEG buffer (caller frees)
 * @param jpeg_len    Pointer to store JPEG data length
 * @return ESP_OK on success
 */
static esp_err_t jpeg_one_shot_encode(uint8_t *yuv_buf, size_t yuv_len,
                                      uint16_t width, uint16_t height,
                                      uint8_t quality,
                                      uint8_t **jpeg_out, size_t *jpeg_len)
{
    if (!yuv_buf || !jpeg_out || !jpeg_len || width == 0 || height == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_OK;

    /* Create a temporary JPEG encoder engine */
    jpeg_encoder_handle_t encoder = NULL;
    jpeg_encode_engine_cfg_t enc_cfg = {
        .intr_priority = 0,
        .timeout_ms = 3000,
    };

    ret = jpeg_new_encoder_engine(&enc_cfg, &encoder);
    if (ret != ESP_OK || !encoder) {
        ESP_LOGE(TAG, "Failed to create JPEG encoder engine: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Allocate output buffer. JPEG is always smaller than raw; W*H is a safe
     * upper bound and avoids SPIRAM fragmentation issues with W*H*2. */
    size_t max_jpeg_size = width * height;
    uint8_t *out_buf = heap_caps_aligned_calloc(64, 1, max_jpeg_size, MALLOC_CAP_SPIRAM);
    if (!out_buf) {
        ESP_LOGE(TAG, "Failed to allocate JPEG output buffer (%zu bytes)", max_jpeg_size);
        jpeg_del_encoder_engine(encoder);
        return ESP_ERR_NO_MEM;
    }

    /* Configure and encode */
    jpeg_encode_cfg_t config = {
        .width = width,
        .height = height,
        .src_type = JPEG_ENCODE_IN_FORMAT_YUV422,
        .sub_sample = JPEG_DOWN_SAMPLING_YUV422,
        .image_quality = quality,
    };

    uint32_t out_len = 0;
    ret = jpeg_encoder_process(encoder, &config, yuv_buf, yuv_len,
                               out_buf, max_jpeg_size, &out_len);

    /* Clean up the encoder */
    jpeg_del_encoder_engine(encoder);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG encoding failed: %s", esp_err_to_name(ret));
        free(out_buf);
        return ret;
    }

    *jpeg_out = out_buf;
    *jpeg_len = (size_t)out_len;

    ESP_LOGI(TAG, "JPEG snapshot encoded: %ux%u quality=%u size=%zu",
             width, height, quality, (size_t)out_len);
    return ESP_OK;
}

esp_err_t video_capture_get_snapshot(uint8_t **jpeg_buf, size_t *jpeg_len,
                                     uint8_t quality, uint32_t timeout_ms)
{
    if (!jpeg_buf || !jpeg_len) {
        return ESP_ERR_INVALID_ARG;
    }

    *jpeg_buf = NULL;
    *jpeg_len = 0;

    if (quality == 0 || quality > 100) {
        quality = 80;  /* Default quality */
    }

    esp_err_t ret;
    uint16_t width = 0, height = 0;
    size_t raw_len = 0;
    video_frame_pixformat_t pixfmt = PIXFMT_YUV420;

    /* Allocate a raw frame buffer large enough for the maximum expected resolution.
     * YUV420 = 1.5 bytes/pixel. For 1920x1080 that's ~3.1 MB. */
    size_t raw_buf_size = 1920 * 1080 * 3 / 2;  /* Max expected YUV420 frame size */
    uint8_t *raw_buf = heap_caps_aligned_calloc(64, 1, raw_buf_size, MALLOC_CAP_SPIRAM);
    if (!raw_buf) {
        ESP_LOGE(TAG, "Failed to allocate raw frame buffer for snapshot");
        return ESP_ERR_NO_MEM;
    }

    if (h264_encoder_is_running()) {
        /* Path 1: Encoder is running - intercept a frame from the pipeline */
        ESP_LOGI(TAG, "Snapshot: intercepting frame from active encoder pipeline");
        ret = esp32p4_snapshot_intercept_frame(raw_buf, raw_buf_size,
                                               &raw_len, &width, &height,
                                               &pixfmt, timeout_ms);
    } else {
        /* Path 2: Encoder is NOT running - directly grab from camera */
        ESP_LOGI(TAG, "Snapshot: directly grabbing frame from camera");
        ret = esp32p4_snapshot_direct_grab(raw_buf, raw_buf_size,
                                           &raw_len, &width, &height,
                                           &pixfmt, timeout_ms);
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to capture raw frame for snapshot: %s", esp_err_to_name(ret));
        free(raw_buf);
        return ret;
    }

    ESP_LOGI(TAG, "Raw frame captured: %ux%u len=%zu pixfmt=%d", width, height, raw_len, pixfmt);

    /* JPEG-encode the raw frame */
    ret = jpeg_one_shot_encode(raw_buf, raw_len, width, height, quality,
                               jpeg_buf, jpeg_len);

    /* Free the raw frame buffer */
    free(raw_buf);

    return ret;
}

void video_capture_snapshot_free(uint8_t *jpeg_buf)
{
    if (jpeg_buf) {
        free(jpeg_buf);
    }
}

#else /* !CONFIG_IDF_TARGET_ESP32P4 */

esp_err_t video_capture_get_snapshot(uint8_t **jpeg_buf, size_t *jpeg_len,
                                     uint8_t quality, uint32_t timeout_ms)
{
    (void)quality;
    (void)timeout_ms;
    if (jpeg_buf) *jpeg_buf = NULL;
    if (jpeg_len) *jpeg_len = 0;
    ESP_LOGW(TAG, "JPEG snapshot not supported on this target");
    return ESP_ERR_NOT_SUPPORTED;
}

void video_capture_snapshot_free(uint8_t *jpeg_buf)
{
    if (jpeg_buf) {
        free(jpeg_buf);
    }
}

#endif /* CONFIG_IDF_TARGET_ESP32P4 */
