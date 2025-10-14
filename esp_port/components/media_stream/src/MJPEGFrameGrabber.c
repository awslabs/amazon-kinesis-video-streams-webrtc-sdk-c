/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "MJPEGFrameGrabber.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "esp_camera.h"
#include "app_camera_esp.h"

#elif CONFIG_IDF_TARGET_ESP32P4
#include "driver/jpeg_encode.h"
#include "esp_video_if.h"
#include "esp_heap_caps.h"
#endif

static const char *TAG = "MJPEGFrameGrabber";

// Queue for storing encoded frames
static QueueHandle_t mjpeg_frame_queue = NULL;
static uint16_t frame_width = 640;
static uint16_t frame_height = 480;
static uint8_t jpeg_quality = 80;
static bool encoder_initialized = false;
static TaskHandle_t encoder_task_handle = NULL;

#define MAX_MJPEG_QUEUE_SIZE 3

#if CONFIG_IDF_TARGET_ESP32P4
// JPEG encoder handle
static jpeg_encoder_handle_t jpeg_encoder = NULL;

/**
 * @brief Wrapper function for JPEG encoding
 *
 * @param input_buf Input YUV buffer
 * @param input_size Size of input buffer
 * @param width Image width
 * @param height Image height
 * @param quality JPEG quality (1-100)
 * @param output_buf Pointer to store the output buffer address
 * @param output_size Pointer to store the output size
 * @return esp_err_t ESP_OK on success
 */
static esp_err_t esp_jpeg_encode(uint8_t *input_buf, size_t input_size,
                                 uint16_t width, uint16_t height,
                                 uint8_t quality, uint8_t **output_buf, size_t *output_size)
{
    esp_err_t ret = ESP_OK;

    // Check if the encoder is initialized
    if (jpeg_encoder == NULL) {
        ESP_LOGE(TAG, "JPEG encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Allocate output buffer - make it large enough for the worst case
    size_t max_jpeg_size = width * height * 2;  // Conservative estimate
    uint8_t *jpeg_buf = heap_caps_aligned_calloc(64, 1, max_jpeg_size, MALLOC_CAP_SPIRAM);
    if (!jpeg_buf) {
        ESP_LOGE(TAG, "Failed to allocate JPEG buffer");
        return ESP_ERR_NO_MEM;
    }

    // Configure the encoder
    jpeg_encode_cfg_t config = {
        .width = width,
        .height = height,
        .src_type = JPEG_ENCODE_IN_FORMAT_YUV422,  // YUV422 input format
        .sub_sample = JPEG_DOWN_SAMPLING_YUV422,   // YUV422 subsampling
        .image_quality = quality,
    };

    // Encode the image
    uint32_t jpeg_len = 0;
    ret = jpeg_encoder_process(jpeg_encoder, &config, input_buf, input_size,
                             jpeg_buf, max_jpeg_size, &jpeg_len);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "JPEG encoding failed: %d", ret);
        free(jpeg_buf);
        return ret;
    }

    // Set the output parameters
    *output_buf = jpeg_buf;
    *output_size = jpeg_len;

    return ESP_OK;
}
#endif

// Task for camera frame capture and JPEG encoding
static void mjpeg_encoder_task(void *arg)
{
    ESP_LOGI(TAG, "MJPEG encoder task started");

    while (1) {
        // Capture a frame
#if CONFIG_IDF_TARGET_ESP32S3
        // ESP32-S3 uses esp_camera
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // For ESP32-S3, the camera can directly output JPEG
        esp_mjpeg_out_buf_t *frame = malloc(sizeof(esp_mjpeg_out_buf_t));
        if (!frame) {
            ESP_LOGE(TAG, "Failed to allocate frame structure");
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Allocate buffer for the JPEG data and copy it
        frame->buffer = malloc(fb->len);
        if (!frame->buffer) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer");
            free(frame);
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        memcpy(frame->buffer, fb->buf, fb->len);
        frame->len = fb->len;
        frame->type = ESP_MJPEG_FRAME_TYPE_JPEG;

        // Return the camera frame buffer
        esp_camera_fb_return(fb);

#elif CONFIG_IDF_TARGET_ESP32P4
        // Get a frame using the ESP video interface
        video_fb_t *video_frame = esp_video_if_get_frame();
        if (!video_frame) {
            // No frame available, try again later
            vTaskDelay(pdMS_TO_TICKS(33));
            continue;
        }

        // Allocate an MJPEG frame
        esp_mjpeg_out_buf_t *frame = malloc(sizeof(esp_mjpeg_out_buf_t));
        if (!frame) {
            ESP_LOGE(TAG, "Failed to allocate frame structure");
            esp_video_if_release_frame(video_frame);
            vTaskDelay(pdMS_TO_TICKS(33));
            continue;
        }

        // Encode the raw frame to JPEG
        size_t jpeg_len = 0;
        uint8_t *jpeg_buf = NULL;
        esp_err_t ret = esp_jpeg_encode(video_frame->buf, video_frame->len,
                                        frame_width, frame_height,
                                        jpeg_quality, &jpeg_buf, &jpeg_len);

        // Release the raw frame as we're done with it
        esp_video_if_release_frame(video_frame);

        if (ret != ESP_OK || !jpeg_buf) {
            ESP_LOGE(TAG, "JPEG encoding failed");
            free(frame);
            vTaskDelay(pdMS_TO_TICKS(33));
            continue;
        }

        frame->buffer = jpeg_buf;
        frame->len = jpeg_len;
        frame->type = ESP_MJPEG_FRAME_TYPE_JPEG;
#endif

        // Send frame to queue, with timeout
        if (xQueueSend(mjpeg_frame_queue, &frame, pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full, dropping frame");
            free(frame->buffer);
            free(frame);
        }

        // Limit frame rate
        vTaskDelay(pdMS_TO_TICKS(33)); // ~30fps
    }
}

esp_err_t mjpeg_camera_and_encoder_init(uint16_t width, uint16_t height, uint8_t quality)
{
    if (encoder_initialized) {
        ESP_LOGW(TAG, "MJPEG encoder already initialized");
        return ESP_OK;
    }

    // Store parameters
    frame_width = width;
    frame_height = height;
    jpeg_quality = quality;

    // Create frame queue
    mjpeg_frame_queue = xQueueCreate(MAX_MJPEG_QUEUE_SIZE, sizeof(esp_mjpeg_out_buf_t*));
    if (!mjpeg_frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return ESP_ERR_NO_MEM;
    }

#if CONFIG_IDF_TARGET_ESP32S3
    // Initialize camera for ESP32-S3
    camera_config_t camera_config = {
        .pin_pwdn = CAMERA_PIN_PWDN,
        .pin_reset = CAMERA_PIN_RESET,
        .pin_xclk = CAMERA_PIN_XCLK,
        .pin_sccb_sda = CAMERA_PIN_SIOD,
        .pin_sccb_scl = CAMERA_PIN_SIOC,

        .pin_d7 = CAMERA_PIN_D7,
        .pin_d6 = CAMERA_PIN_D6,
        .pin_d5 = CAMERA_PIN_D5,
        .pin_d4 = CAMERA_PIN_D4,
        .pin_d3 = CAMERA_PIN_D3,
        .pin_d2 = CAMERA_PIN_D2,
        .pin_d1 = CAMERA_PIN_D1,
        .pin_d0 = CAMERA_PIN_D0,
        .pin_vsync = CAMERA_PIN_VSYNC,
        .pin_href = CAMERA_PIN_HREF,
        .pin_pclk = CAMERA_PIN_PCLK,

        .xclk_freq_hz = 20000000,  // 20MHz XCLK frequency
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,

        .pixel_format = PIXFORMAT_JPEG,  // Use JPEG format for direct output
        .frame_size = FRAMESIZE_VGA,     // Default to VGA, will adjust based on requested size

        .jpeg_quality = quality,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
        .fb_location = CAMERA_FB_IN_PSRAM
    };

    // Determine the best frame size based on requested dimensions
    if (width <= 160 && height <= 120) {
        camera_config.frame_size = FRAMESIZE_QQVGA;
    } else if (width <= 240 && height <= 176) {
        camera_config.frame_size = FRAMESIZE_HQVGA;
    } else if (width <= 320 && height <= 240) {
        camera_config.frame_size = FRAMESIZE_QVGA;
    } else if (width <= 640 && height <= 480) {
        camera_config.frame_size = FRAMESIZE_VGA;
    } else if (width <= 800 && height <= 600) {
        camera_config.frame_size = FRAMESIZE_SVGA;
    }

    // Initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        vQueueDelete(mjpeg_frame_queue);
        mjpeg_frame_queue = NULL;
        return err;
    }

    // Additional camera settings if needed
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        // Flip the image if needed
        // s->set_vflip(s, 1);
        // s->set_hmirror(s, 1);
    }

#elif CONFIG_IDF_TARGET_ESP32P4
    // Initialize video interface
    esp_err_t ret = esp_video_if_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Video initialization failed: %d", ret);
        vQueueDelete(mjpeg_frame_queue);
        mjpeg_frame_queue = NULL;
        return ret;
    }

    // Initialize the JPEG encoder
    jpeg_encode_engine_cfg_t enc_cfg = {
        .intr_priority = 0,
        .timeout_ms = 1000,  // 1 second timeout
    };

    ret = jpeg_new_encoder_engine(&enc_cfg, &jpeg_encoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create JPEG encoder: %d", ret);
        esp_video_if_stop();
        vQueueDelete(mjpeg_frame_queue);
        mjpeg_frame_queue = NULL;
        return ret;
    }
    ESP_LOGI(TAG, "JPEG encoder created successfully");

    // Start video capture with error recovery
    ret = esp_video_if_start();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Video start failed: %d - will retry in encoder task", ret);
        // We'll continue anyway and let the encoder task retry
    }
#endif

    // Create encoder task
    BaseType_t xReturn = xTaskCreate(mjpeg_encoder_task, "mjpeg_encoder", 4096, NULL, 5, &encoder_task_handle);
    if (xReturn != pdPASS) {
        ESP_LOGE(TAG, "Failed to create MJPEG encoder task");
#if CONFIG_IDF_TARGET_ESP32P4
        if (jpeg_encoder) {
            jpeg_del_encoder_engine(jpeg_encoder);
            jpeg_encoder = NULL;
        }
        esp_video_if_stop();
#endif
        vQueueDelete(mjpeg_frame_queue);
        mjpeg_frame_queue = NULL;
        return ESP_FAIL;
    }

    encoder_initialized = true;
    ESP_LOGI(TAG, "MJPEG encoder initialized: %dx%d, quality=%d", frame_width, frame_height, jpeg_quality);
    return ESP_OK;
}

esp_mjpeg_out_buf_t* get_mjpeg_encoded_frame(void)
{
    if (!encoder_initialized || !mjpeg_frame_queue) {
        ESP_LOGW(TAG, "MJPEG encoder not initialized");
        return NULL;
    }

    esp_mjpeg_out_buf_t *frame = NULL;
    if (xQueueReceive(mjpeg_frame_queue, &frame, 0) != pdTRUE) {
        // No frame available
        return NULL;
    }

    return frame;
}

esp_err_t mjpeg_encoder_deinit(void)
{
    if (!encoder_initialized) {
        return ESP_OK;
    }

    // Stop the encoder task
    if (encoder_task_handle != NULL) {
        vTaskDelete(encoder_task_handle);
        encoder_task_handle = NULL;
    }

#if CONFIG_IDF_TARGET_ESP32P4
    // Clean up the JPEG encoder
    if (jpeg_encoder != NULL) {
        jpeg_del_encoder_engine(jpeg_encoder);
        jpeg_encoder = NULL;
    }

    // Stop video interface
    esp_video_if_stop();
#endif

    // Free any frames in the queue
    if (mjpeg_frame_queue != NULL) {
        esp_mjpeg_out_buf_t *frame = NULL;
        while (xQueueReceive(mjpeg_frame_queue, &frame, 0) == pdTRUE) {
            if (frame) {
                if (frame->buffer) {
                    free(frame->buffer);
                }
                free(frame);
            }
        }
        vQueueDelete(mjpeg_frame_queue);
        mjpeg_frame_queue = NULL;
    }

    encoder_initialized = false;
    return ESP_OK;
}
