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

#include "esp_heap_caps.h"
#include "webrtc_mem_utils.h"

#include "sdkconfig.h"

static const char *TAG = "H264FrameGrabber";

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#include "H264FrameGrabber.h"

#include "app_camera_esp.h"

#if CONFIG_IDF_TARGET_ESP32S3
#include "esp_h264_enc_single_sw.h"
#include "esp_h264_enc_single.h"
#else
#include "esp_h264_hw_enc.h"
#include "esp_h264_enc_single.h"
extern void esp32p4_frame_grabber_init(void);
extern esp_h264_out_buf_t *esp32p4_grab_one_frame();
#endif

static int frame_count = 0;
static esp_h264_enc_handle_t handle = NULL;
static esp_h264_enc_out_frame_t out_frame = { 0 };
static esp_h264_enc_in_frame_t in_frame = { 0 };
static esp_h264_enc_cfg_t cfg = {0};
static esp_h264_enc_handle_t initialize_h264_encoder();

static QueueHandle_t frame_queue = NULL;

#define QUEUE_RECEIVE_WAIT_MS  CONFIG_VIDEO_QUEUE_RECEIVE_WAIT_MS
#define QUEUE_SEND_WAIT_MS     CONFIG_VIDEO_QUEUE_SEND_WAIT_MS
static volatile int curr_cnt = 0;

static void video_encoder_task(void *arg)
{
    static uint8_t fill_val = 0;
    frame_count = 0;
    int one_image_size = cfg.res.height * cfg.res.width * 2;
    while(1) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            memcpy(in_frame.raw_data.buffer, fb->buf, one_image_size);
            esp_camera_fb_return(fb);
        } else {
            memset(in_frame.raw_data.buffer, fill_val++, one_image_size);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        in_frame.pts = frame_count++ * (1000 / cfg.fps);

        esp_h264_err_t ret = esp_h264_enc_process(handle, &in_frame, &out_frame);
        if (ret != ESP_H264_ERR_OK) {
            printf("Process failed. ret %d \r\n", ret);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        esp_h264_out_buf_t frame = {0};
        /* Calculate the frame length */
        frame.len = out_frame.length;
        frame.type = (esp_h264_frame_type_t)out_frame.frame_type;

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
        memcpy(frame.buffer, out_frame.raw_data.buffer, frame.len);

        /* Insert it into the queue */
        if (xQueueSend(frame_queue, &frame, pdMS_TO_TICKS(QUEUE_SEND_WAIT_MS)) != pdTRUE) {
            // ESP_LOGW(TAG, "Queue full, dropping frame");
            free(frame.buffer);
        }
    }
}

esp_err_t camera_and_encoder_init(video_capture_config_t *config)
{
    static bool camera_enc_init_done = false;
    if (camera_enc_init_done == true) {
        ESP_LOGW(TAG, "camera_and_encoder_init already done!");
        return ESP_OK;
    }
#if CONFIG_IDF_TARGET_ESP32P4
    esp32p4_frame_grabber_init();
    camera_enc_init_done = true;
#else
    app_camera_init();
    camera_enc_init_done = true;
    printf("camera init done\n");

    initialize_h264_encoder();
    webrtc_mem_utils_print_stats(TAG);

    frame_queue = xQueueCreate(CONFIG_VIDEO_FRAME_QUEUE_SIZE, sizeof(esp_h264_out_buf_t));
    if (!frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return ESP_ERR_NO_MEM;
    }

#define ENC_TASK_STACK_SIZE     CONFIG_VIDEO_ENCODER_TASK_STACK_SIZE
#define ENC_TASK_PRIO           CONFIG_VIDEO_ENCODER_TASK_PRIORITY
    StaticTask_t *task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    void *task_stack = heap_caps_malloc(ENC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    assert(task_buffer && task_stack);

    /* the task never exits, so do not bother to free the buffers */
    TaskHandle_t enc_task_handle = xTaskCreateStatic(video_encoder_task, "video_encoder", ENC_TASK_STACK_SIZE,
                                                     NULL, ENC_TASK_PRIO, task_stack, task_buffer);
    if (enc_task_handle == NULL) {
        ESP_LOGE(TAG, "failed to create encoder task!");
    }
    printf("encoder initialized\n");
#endif
    return ESP_OK;
}

esp_h264_out_buf_t *get_h264_encoded_frame()
{
#if CONFIG_IDF_TARGET_ESP32P4
    return esp32p4_grab_one_frame();
#else
    esp_h264_out_buf_t *frame_data = heap_caps_calloc(1, sizeof(esp_h264_out_buf_t), MALLOC_CAP_SPIRAM);
    if (xQueueReceive(frame_queue, frame_data, pdMS_TO_TICKS(QUEUE_RECEIVE_WAIT_MS)) != pdTRUE) {
        heap_caps_free(frame_data);
        return NULL;
    }
    return frame_data;
#endif
}
#else
void get_h264_encoded_frame(uint8_t *out_buf, uint32_t *frame_len)
{
    /* Dummy function which does nothing. Set the frame size to 0 */
    *frame_len = 0;
}
#endif

#if CONFIG_IDF_TARGET_ESP32S3
static esp_h264_enc_handle_t initialize_h264_encoder()
{
    esp_h264_err_t ret = ESP_H264_ERR_OK;
    int one_image_size = 0;
    cfg.fps = 10;//DEFAULT_FPS_VALUE;
    cfg.gop = 30;
    cfg.res.width = 320;
    cfg.res.height = 240;
    cfg.rc.bitrate = cfg.res.width * cfg.res.height * cfg.fps / 20;
    cfg.rc.qp_min = 30;
    cfg.rc.qp_max = 30;
    cfg.pic_type = ESP_H264_RAW_FMT_YUYV;
    one_image_size = cfg.res.height * cfg.res.width * 2; // 1.5 : Pixel is 1.5 on ESP_H264_RAW_FMT_I420.
    in_frame.raw_data.buffer = (uint8_t *) heap_caps_aligned_alloc(16, one_image_size, MALLOC_CAP_SPIRAM);
    if (in_frame.raw_data.buffer == NULL) {
        printf("in_frame.raw_data.buffer allocation failed\n");
        goto h264_example_exit;
    }
    out_frame.raw_data.len = one_image_size;
    out_frame.raw_data.buffer = (uint8_t *) heap_caps_aligned_alloc(16, one_image_size, MALLOC_CAP_SPIRAM);
    if (out_frame.raw_data.buffer == NULL) {
        printf("out_frame.raw_data.buffer allocation failed\n");
        goto h264_example_exit;
    }
#ifdef CONFIG_IDF_TARGET_ESP32P4
    ret = esp_h264_enc_hw_new(&cfg, &handle);
#else
    ret = esp_h264_enc_sw_new(&cfg, &handle);
#endif
    if (ret != ESP_H264_ERR_OK) {
        printf("esp_h264_enc_sw_new failed ret %d, handle %p\n", (int) ret, (void *) handle);
        goto h264_example_exit;
    }

    ret = esp_h264_enc_open(handle);
    if (ret != ESP_H264_ERR_OK) {
        printf("Open failed. ret %d, handle %p\n", (int) ret, (void *) handle);
        goto h264_example_exit;
    }

    return handle;

h264_example_exit:
    if (in_frame.raw_data.buffer) {
        heap_caps_free(in_frame.raw_data.buffer);
        in_frame.raw_data.buffer = NULL;
    }

    esp_h264_enc_close(handle);
    esp_h264_enc_del(handle);
    return NULL;
}
#endif
