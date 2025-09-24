/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "esp_log.h"
#include "esp_err.h"

#include "esp_heap_caps.h"
#include "webrtc_mem_utils.h"

#include "sdkconfig.h"

static const char *TAG = "H264FrameGrabber";

#include "H264FrameGrabber.h"

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#if CONFIG_IDF_TARGET_ESP32S3
#include "app_camera_esp.h"
#include "esp_h264_enc_single_sw.h"
#include "esp_h264_enc_single.h"
#else
#include "esp_h264_hw_enc.h"
#include "esp_h264_enc_single.h"
extern void esp32p4_frame_grabber_init(void);
extern esp_err_t esp32p4_frame_grabber_start(void);
extern esp_err_t esp32p4_frame_grabber_stop(void);
extern esp_err_t esp32p4_frame_grabber_deinit(void);
extern esp_h264_out_buf_t *esp32p4_grab_one_frame();
#endif

#if CONFIG_IDF_TARGET_ESP32S3

typedef struct {
    QueueHandle_t frame_queue;
    TaskHandle_t encoder_task_handle;
    StaticTask_t *task_buffer;
    void *task_stack;
    bool encoder_initialized;
    bool running;
    SemaphoreHandle_t run_semaphore;
    volatile int curr_cnt;
    int frame_count;
    esp_h264_enc_handle_t handle;
    esp_h264_enc_out_frame_t out_frame;
    esp_h264_enc_in_frame_t in_frame;
    esp_h264_enc_cfg_t cfg;
} h264_encoder_data_t;

static h264_encoder_data_t s_h264_enc_data = {0};
static esp_h264_enc_handle_t initialize_h264_encoder();

#define QUEUE_RECEIVE_WAIT_MS  CONFIG_VIDEO_QUEUE_RECEIVE_WAIT_MS
#define QUEUE_SEND_WAIT_MS     CONFIG_VIDEO_QUEUE_SEND_WAIT_MS

static void video_encoder_task(void *arg)
{
    static uint8_t fill_val = 0;
    ESP_LOGD(TAG, "H264 encoder task started (singleton mode - runs continuously)");

    s_h264_enc_data.frame_count = 0;
    int one_image_size = s_h264_enc_data.cfg.res.height * s_h264_enc_data.cfg.res.width * 2;

    while(1) {
        // Check if encoding should be running
        if (!s_h264_enc_data.running) {
            ESP_LOGD(TAG, "H264 encoder paused, waiting for start signal...");
            // Wait for start signal (blocking)
            xSemaphoreTake(s_h264_enc_data.run_semaphore, portMAX_DELAY);
            ESP_LOGD(TAG, "H264 encoder resumed");
        }
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            memcpy(s_h264_enc_data.in_frame.raw_data.buffer, fb->buf, one_image_size);
            esp_camera_fb_return(fb);
        } else {
            memset(s_h264_enc_data.in_frame.raw_data.buffer, fill_val++, one_image_size);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        s_h264_enc_data.in_frame.pts = s_h264_enc_data.frame_count++ * (1000 / s_h264_enc_data.cfg.fps);

        esp_h264_err_t ret = esp_h264_enc_process(s_h264_enc_data.handle, &s_h264_enc_data.in_frame, &s_h264_enc_data.out_frame);
        if (ret != ESP_H264_ERR_OK) {
            printf("Process failed. ret %d \r\n", ret);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        esp_h264_out_buf_t frame = {0};
        /* Calculate the frame length */
        frame.len = s_h264_enc_data.out_frame.length;
        frame.type = (esp_h264_frame_type_t)s_h264_enc_data.out_frame.frame_type;

        /* allocate the memory of size *frame_len */
        frame.buffer = (uint8_t *) heap_caps_calloc(1, frame.len, MALLOC_CAP_SPIRAM);

        if (!frame.buffer) {
            ESP_LOGE(TAG, "frame.buffer alloc failed, size %d", (int) frame.len);
            webrtc_mem_utils_print_stats(TAG);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        } else {
            // ESP_LOGI(TAG, "frame.len %d", (int) frame.len);
        }

        /* Copy the frame */
        memcpy(frame.buffer, s_h264_enc_data.out_frame.raw_data.buffer, frame.len);

        /* Insert it into the queue */
        if (xQueueSend(s_h264_enc_data.frame_queue, &frame, pdMS_TO_TICKS(QUEUE_SEND_WAIT_MS)) != pdTRUE) {
            // ESP_LOGW(TAG, "Queue full, dropping frame");
            free(frame.buffer);
        }
    }

    ESP_LOGE(TAG, "H264 encoder task unexpectedly exited!");
}

esp_err_t camera_and_encoder_init(video_capture_config_t *config)
{
    // Singleton pattern: return if already initialized
    if (s_h264_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "H264 encoder already initialized (singleton)");
        return ESP_OK;
    }

    app_camera_init();
    printf("camera init done\n");

    if (initialize_h264_encoder() == NULL) {
        ESP_LOGE(TAG, "Failed to initialize H264 encoder");
        goto cleanup;
    }
    webrtc_mem_utils_print_stats(TAG);

    s_h264_enc_data.frame_queue = xQueueCreate(CONFIG_VIDEO_FRAME_QUEUE_SIZE, sizeof(esp_h264_out_buf_t));
    if (!s_h264_enc_data.frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        goto cleanup;
    }

    // Create semaphore for start/stop control
    s_h264_enc_data.run_semaphore = xSemaphoreCreateBinary();
    if (!s_h264_enc_data.run_semaphore) {
        ESP_LOGE(TAG, "Failed to create run semaphore");
        goto cleanup;
    }

#define ENC_TASK_STACK_SIZE     CONFIG_VIDEO_ENCODER_TASK_STACK_SIZE
#define ENC_TASK_PRIO           CONFIG_VIDEO_ENCODER_TASK_PRIORITY
    s_h264_enc_data.task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    s_h264_enc_data.task_stack = heap_caps_malloc(ENC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_h264_enc_data.task_buffer || !s_h264_enc_data.task_stack) {
        ESP_LOGE(TAG, "Failed to allocate task buffers");
        goto cleanup;
    }

    s_h264_enc_data.running = false;  // Start in stopped state
    s_h264_enc_data.encoder_task_handle = xTaskCreateStatic(video_encoder_task, "video_encoder", ENC_TASK_STACK_SIZE,
                                                            NULL, ENC_TASK_PRIO, s_h264_enc_data.task_stack, s_h264_enc_data.task_buffer);
    if (s_h264_enc_data.encoder_task_handle == NULL) {
        ESP_LOGE(TAG, "failed to create encoder task!");
        goto cleanup;
    }

    s_h264_enc_data.encoder_initialized = true;

    ESP_LOGD(TAG, "H264 encoder initialized as singleton (stopped, use start() to begin)");
    return ESP_OK;

cleanup:
    // Conditional cleanup based on what was allocated
    if (s_h264_enc_data.task_buffer != NULL) {
        heap_caps_free(s_h264_enc_data.task_buffer);
        s_h264_enc_data.task_buffer = NULL;
    }
    if (s_h264_enc_data.task_stack != NULL) {
        heap_caps_free(s_h264_enc_data.task_stack);
        s_h264_enc_data.task_stack = NULL;
    }
    if (s_h264_enc_data.run_semaphore != NULL) {
        vSemaphoreDelete(s_h264_enc_data.run_semaphore);
        s_h264_enc_data.run_semaphore = NULL;
    }
    if (s_h264_enc_data.frame_queue != NULL) {
        vQueueDelete(s_h264_enc_data.frame_queue);
        s_h264_enc_data.frame_queue = NULL;
    }

    ESP_LOGE(TAG, "H264 encoder initialization failed");
    return ESP_ERR_NO_MEM;
}

esp_h264_out_buf_t *get_h264_encoded_frame()
{
    esp_h264_out_buf_t *frame_data = heap_caps_calloc(1, sizeof(esp_h264_out_buf_t), MALLOC_CAP_SPIRAM);
    if (xQueueReceive(s_h264_enc_data.frame_queue, frame_data, pdMS_TO_TICKS(QUEUE_RECEIVE_WAIT_MS)) != pdTRUE) {
        heap_caps_free(frame_data);
        return NULL;
    }
    return frame_data;
}

static esp_h264_enc_handle_t initialize_h264_encoder()
{
    esp_h264_err_t ret = ESP_H264_ERR_OK;
    int one_image_size = 0;
    s_h264_enc_data.cfg.fps = 10;//DEFAULT_FPS_VALUE;
    s_h264_enc_data.cfg.gop = 30;
    s_h264_enc_data.cfg.res.width = 320;
    s_h264_enc_data.cfg.res.height = 240;
    s_h264_enc_data.cfg.rc.bitrate = s_h264_enc_data.cfg.res.width * s_h264_enc_data.cfg.res.height * s_h264_enc_data.cfg.fps / 20;
    s_h264_enc_data.cfg.rc.qp_min = 30;
    s_h264_enc_data.cfg.rc.qp_max = 30;
    s_h264_enc_data.cfg.pic_type = ESP_H264_RAW_FMT_YUYV;
    one_image_size = s_h264_enc_data.cfg.res.height * s_h264_enc_data.cfg.res.width * 2; // 1.5 : Pixel is 1.5 on ESP_H264_RAW_FMT_I420.
    s_h264_enc_data.in_frame.raw_data.buffer = (uint8_t *) heap_caps_aligned_alloc(16, one_image_size, MALLOC_CAP_SPIRAM);
    if (s_h264_enc_data.in_frame.raw_data.buffer == NULL) {
        printf("in_frame.raw_data.buffer allocation failed\n");
        goto h264_example_exit;
    }
    s_h264_enc_data.out_frame.raw_data.len = one_image_size;
    s_h264_enc_data.out_frame.raw_data.buffer = (uint8_t *) heap_caps_aligned_alloc(16, one_image_size, MALLOC_CAP_SPIRAM);
    if (s_h264_enc_data.out_frame.raw_data.buffer == NULL) {
        printf("out_frame.raw_data.buffer allocation failed\n");
        goto h264_example_exit;
    }
    ret = esp_h264_enc_sw_new(&s_h264_enc_data.cfg, &s_h264_enc_data.handle);
    if (ret != ESP_H264_ERR_OK) {
        printf("esp_h264_enc_sw_new failed ret %d, handle %p\n", (int) ret, (void *) s_h264_enc_data.handle);
        goto h264_example_exit;
    }

    ret = esp_h264_enc_open(s_h264_enc_data.handle);
    if (ret != ESP_H264_ERR_OK) {
        printf("Open failed. ret %d, handle %p\n", (int) ret, (void *) s_h264_enc_data.handle);
        goto h264_example_exit;
    }

    return s_h264_enc_data.handle;

h264_example_exit:
    if (s_h264_enc_data.in_frame.raw_data.buffer) {
        heap_caps_free(s_h264_enc_data.in_frame.raw_data.buffer);
        s_h264_enc_data.in_frame.raw_data.buffer = NULL;
    }

    esp_h264_enc_close(s_h264_enc_data.handle);
    esp_h264_enc_del(s_h264_enc_data.handle);
    return NULL;
}

esp_err_t h264_encoder_start(void)
{
    if (!s_h264_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "H264 encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_h264_enc_data.running) {
        ESP_LOGD(TAG, "H264 encoder already running");
        return ESP_OK;
    }

    s_h264_enc_data.running = true;
    xSemaphoreGive(s_h264_enc_data.run_semaphore);  // Signal encoder to start
    ESP_LOGD(TAG, "H264 encoder started");

    return ESP_OK;
}

esp_err_t h264_encoder_stop(void)
{
    if (!s_h264_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "H264 encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_h264_enc_data.running) {
        ESP_LOGD(TAG, "H264 encoder already stopped");
        return ESP_OK;
    }

    s_h264_enc_data.running = false;
    ESP_LOGD(TAG, "H264 encoder stopped");

    return ESP_OK;
}

esp_err_t h264_encoder_deinit(void)
{
    if (!s_h264_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "H264 encoder not initialized, nothing to deinitialize");
        return ESP_OK;
    }

    // Stop encoding first
    h264_encoder_stop();

    // Drain the frame queue with limited iterations to prevent infinite loop
    if (s_h264_enc_data.frame_queue != NULL) {
        ESP_LOGD(TAG, "Draining H264 frame queue...");
        esp_h264_out_buf_t h264_frame;
        int drained_count = 0;
        const int max_drain_iterations = CONFIG_VIDEO_FRAME_QUEUE_SIZE;  // Prevent infinite loop

        while (xQueueReceive(s_h264_enc_data.frame_queue, &h264_frame, 0) == pdTRUE &&
               drained_count < max_drain_iterations) {
            if (h264_frame.buffer) {
                heap_caps_free(h264_frame.buffer);
            }
            drained_count++;
        }

        if (drained_count > 0) {
            ESP_LOGD(TAG, "Drained %d H264 frames from queue", drained_count);
        }
        if (drained_count >= max_drain_iterations) {
            ESP_LOGI(TAG, "Reached max drain limit, queue may still contain frames");
        }
    }

    // Singleton pattern: encoder task remains running but paused
    ESP_LOGD(TAG, "H264 encoder is singleton - task remains paused until start() is called");

    return ESP_OK;
}
#else /* CONFIG_IDF_TARGET_ESP32P4 */
esp_err_t camera_and_encoder_init(video_capture_config_t *config)
{
    static bool camera_enc_init_done = false;
    if (camera_enc_init_done == true) {
        ESP_LOGI(TAG, "camera_and_encoder_init already done!");
        return ESP_OK;
    }

    esp32p4_frame_grabber_init();
    camera_enc_init_done = true;
    return ESP_OK;
}

esp_h264_out_buf_t *get_h264_encoded_frame()
{
    return esp32p4_grab_one_frame();
}

esp_err_t video_capture_set_bitrate(video_capture_handle_t handle, uint32_t bitrate_kbps)
{
    esp_h264_hw_enc_set_bitrate(bitrate_kbps * 1000);
    return ESP_OK;
}

esp_err_t h264_encoder_start(void)
{
    return esp32p4_frame_grabber_start();
}

esp_err_t h264_encoder_stop(void)
{
    return esp32p4_frame_grabber_stop();
}

esp_err_t h264_encoder_deinit(void)
{
    return esp32p4_frame_grabber_deinit();
}
#endif
#else /* all other targets */
esp_err_t camera_and_encoder_init(video_capture_config_t *config)
{
    (void) config;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_h264_out_buf_t * get_h264_encoded_frame(uint8_t *out_buf, uint32_t *frame_len)
{
    /* Dummy function which does nothing. Set the frame size to 0 */
    *frame_len = 0;
    return NULL;
}

esp_err_t video_capture_set_bitrate(video_capture_handle_t handle, uint32_t bitrate_kbps)
{
    (void) handle;
    (void) bitrate_kbps;
    return ESP_OK;
}

esp_err_t h264_encoder_start(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}

esp_err_t h264_encoder_stop(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}

esp_err_t h264_encoder_deinit(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}
#endif
