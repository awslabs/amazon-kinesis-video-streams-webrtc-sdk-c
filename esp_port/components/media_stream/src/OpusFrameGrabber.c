/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "esp_err.h"

#include "driver/i2s.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_opus_enc.h"
#include "webrtc_mem_utils.h"

#include "OpusFrameGrabber.h"

#if CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4
#if CONFIG_IDF_TARGET_ESP32P4
#include "bsp/esp-bsp.h"
#include "bsp/bsp_board_extra.h"
#endif
static const char *TAG = "OpusFrameGrabber";

#define SAMPLE_RATE 16000
#define CHANNELS 1
#define BITRATE 16000

typedef struct {
    QueueHandle_t frame_queue;
    TaskHandle_t encoder_task_handle;
    void *encoder_handle;
    StaticTask_t *task_buffer;
    void *task_stack;
    bool encoder_initialized;
    bool running;
    SemaphoreHandle_t run_semaphore;
    uint8_t *inbuf;
    uint8_t *outbuf;
    int insize;
    int outsize;
} opus_encoder_data_t;

static opus_encoder_data_t s_enc_data = {0};

#ifndef CONFIG_IDF_TARGET_ESP32P4
static i2s_port_t i2s_port = I2S_NUM_0;
static void i2s_init(void)
{
    // Start listening for audio: MONO @ 16KHz
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = (i2s_bits_per_sample_t) 16,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 3,
        .dma_buf_len = 300,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = -1,
    };
#if CONFIG_IDF_TARGET_ESP32S3
    i2s_pin_config_t pin_config = {
        .bck_io_num = 41,    // IIS_SCLK
        .ws_io_num = 42,     // IIS_LCLK
        .data_out_num = -1,  // IIS_DSIN
        .data_in_num = 2,   // IIS_DOUT
    };
// #define READ_SAMPLE_SIZE_30  1
#if READ_SAMPLE_SIZE_30
    i2s_config.bits_per_sample = (i2s_bits_per_sample_t) 32;
#endif
#elif CONFIG_IDF_TARGET_ESP32P4
    i2s_pin_config_t pin_config = {
        .mck_io_num = 30,
        .bck_io_num = 29,    // IIS_SCLK
        .ws_io_num = 27,     // IIS_LCLK
        .data_out_num = -1,  // IIS_DSIN
        .data_in_num = 28,   // IIS_DOUT
    };
#else
    i2s_pin_config_t pin_config = {
        .bck_io_num = 26,    // IIS_SCLK
        .ws_io_num = 32,     // IIS_LCLK
        .data_out_num = -1,  // IIS_DSIN
        .data_in_num = 33,   // IIS_DOUT
    };
    i2s_port = I2S_NUM_1; // for esp32-eye
#endif

    esp_err_t ret = 0;
    ret = i2s_driver_install(i2s_port, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in i2s_driver_install");
    }
    ret = i2s_set_pin(i2s_port, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in i2s_set_pin");
    }

    ret = i2s_zero_dma_buffer(i2s_port);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error in initializing dma buffer with 0");
    }
}
#endif

static void audio_encoder_task(void *arg)
{
    void *enc_handle = arg;
    ESP_LOGD(TAG, "Audio encoder task started (singleton mode - runs continuously)");

    while (1) {
        // Check if encoding should be running
        if (!s_enc_data.running) {
            ESP_LOGD(TAG, "Audio encoder paused, waiting for start signal...");
            // Wait for start signal (blocking)
            xSemaphoreTake(s_enc_data.run_semaphore, portMAX_DELAY);
            ESP_LOGD(TAG, "Audio encoder resumed");
        }

        // Encode process
        esp_audio_enc_in_frame_t in_frame = { 0 };
        esp_audio_enc_out_frame_t out_frame = { 0 };

        in_frame.buffer = s_enc_data.inbuf;
        in_frame.len = s_enc_data.insize;
        out_frame.buffer = s_enc_data.outbuf;
        out_frame.len = s_enc_data.outsize;

        size_t bytes_read = 0;

#define I2S_READ_WAIT_MS CONFIG_AUDIO_QUEUE_WAIT_MS
#if CONFIG_IDF_TARGET_ESP32P4
        // esp_codec_dev_read(audio_handle, (void*)s_enc_data.inbuf, s_enc_data.insize);
        esp_err_t read_ret = bsp_extra_i2s_read(s_enc_data.inbuf, s_enc_data.insize, &bytes_read, I2S_READ_WAIT_MS);
        if (read_ret != ESP_OK) {
            ESP_LOGE(TAG, "bsp_extra_i2s_read error: %s", esp_err_to_name(read_ret));
            vTaskDelay(pdMS_TO_TICKS(I2S_READ_WAIT_MS));
            continue;
        }
#else
        esp_err_t i2s_ret = ESP_OK;
#if READ_SAMPLE_SIZE_30
        i2s_ret = i2s_read(i2s_port, (void*)s_enc_data.inbuf, s_enc_data.insize * 2, &bytes_read, pdMS_TO_TICKS(I2S_READ_WAIT_MS));
        if (i2s_ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(i2s_ret));
            vTaskDelay(pdMS_TO_TICKS(I2S_READ_WAIT_MS));
            continue;
        }
        // rescale the data (Actual data is 30 bits, use higher 16 bits out of those)
        for (int i = 0; i < bytes_read / 4; ++i) {
            ((uint16_t *) s_enc_data.inbuf)[i] = (((uint32_t *) s_enc_data.inbuf)[i] >> 14) & 0xffff;
        }
#else
        i2s_ret = i2s_read(i2s_port, (void*)s_enc_data.inbuf, s_enc_data.insize, &bytes_read, pdMS_TO_TICKS(I2S_READ_WAIT_MS));
        if (i2s_ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S read error: %s", esp_err_to_name(i2s_ret));
            vTaskDelay(pdMS_TO_TICKS(I2S_READ_WAIT_MS));
            continue;
        }
#endif
#endif

#define OPUS_ENCODE_WAIT_MS CONFIG_AUDIO_QUEUE_WAIT_MS

        esp_opus_out_buf_t opus_frame = { 0 };
        esp_audio_err_t ret = ESP_AUDIO_ERR_OK;
        ret = esp_opus_enc_process(enc_handle, &in_frame, &out_frame);
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "audio encoder process failed.");
            vTaskDelay(pdMS_TO_TICKS(OPUS_ENCODE_WAIT_MS));
            continue;
        }
        opus_frame.len = out_frame.encoded_bytes;
        opus_frame.buffer = heap_caps_calloc(1, opus_frame.len, MALLOC_CAP_SPIRAM);
        if (!opus_frame.buffer) {
            ESP_LOGE(TAG, "Failed to alloc opus_frame.buffer(size %d)", (int) opus_frame.len);
            vTaskDelay(pdMS_TO_TICKS(OPUS_ENCODE_WAIT_MS));
            continue;
        }
        memcpy(opus_frame.buffer, out_frame.buffer, opus_frame.len);

        /* Insert it into the queue */
        if (xQueueSend(s_enc_data.frame_queue, &opus_frame, pdMS_TO_TICKS(OPUS_ENCODE_WAIT_MS)) != pdTRUE) {
            heap_caps_free(opus_frame.buffer);
        }
    }

    // This point should never be reached in singleton mode
    ESP_LOGE(TAG, "Audio encoder task unexpectedly exited!");
}

/* returns handle */
void *opus_encoder_init_internal(audio_capture_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    // Singleton pattern: return existing handle if already initialized
    if (s_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "Opus encoder already initialized (singleton), returning existing handle");
        return s_enc_data.encoder_handle;
    }

    esp_audio_err_t ret = ESP_AUDIO_ERR_OK;
    esp_opus_enc_config_t enc_config = ESP_OPUS_ENC_CONFIG_DEFAULT();
    enc_config.sample_rate = config->format.sample_rate;
    enc_config.channel = config->format.channels;
    enc_config.bitrate = config->bitrate;
    ret = esp_opus_enc_open(&enc_config, sizeof(esp_opus_enc_config_t), &s_enc_data.encoder_handle);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Opus encoder");
        goto cleanup;
    }

    s_enc_data.frame_queue = xQueueCreate(CONFIG_AUDIO_FRAME_QUEUE_SIZE, sizeof(esp_opus_out_buf_t));
    if (!s_enc_data.frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        goto cleanup;
    }

    // Create semaphore for start/stop control
    s_enc_data.run_semaphore = xSemaphoreCreateBinary();
    if (!s_enc_data.run_semaphore) {
        ESP_LOGE(TAG, "Failed to create run semaphore");
        goto cleanup;
    }

    // Get frame sizes and allocate buffers
    esp_opus_enc_get_frame_size(s_enc_data.encoder_handle, &s_enc_data.insize, &s_enc_data.outsize);
#if READ_SAMPLE_SIZE_30
    s_enc_data.inbuf = heap_caps_calloc(1, s_enc_data.insize * 2, MALLOC_CAP_SPIRAM); // 20ms mono
#else
    s_enc_data.inbuf = heap_caps_calloc(1, s_enc_data.insize, MALLOC_CAP_SPIRAM); // 20ms mono
#endif
    s_enc_data.outbuf = heap_caps_calloc(1, s_enc_data.outsize, MALLOC_CAP_SPIRAM);

    if (!s_enc_data.inbuf || !s_enc_data.outbuf) {
        ESP_LOGE(TAG, "Failed to allocate audio buffers");
        goto cleanup;
    }

    // Initialize audio hardware
#if CONFIG_IDF_TARGET_ESP32P4
    bsp_extra_codec_init();
    // Give some time for the I2S channel to be properly enabled
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGD(TAG, "ESP32P4 audio codec initialized");
#else
    i2s_init();
#endif

#define ENC_TASK_STACK_SIZE     CONFIG_AUDIO_ENCODER_TASK_STACK_SIZE
#define ENC_TASK_PRIO           CONFIG_AUDIO_ENCODER_TASK_PRIORITY
    s_enc_data.task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    s_enc_data.task_stack = heap_caps_calloc(1, ENC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_enc_data.task_buffer || !s_enc_data.task_stack) {
        ESP_LOGE(TAG, "Failed to allocate task buffers");
        goto cleanup;
    }

    s_enc_data.running = false;  // Start in stopped state

    s_enc_data.encoder_task_handle =
    xTaskCreateStatic(audio_encoder_task, "audio_encoder", ENC_TASK_STACK_SIZE,
        s_enc_data.encoder_handle, ENC_TASK_PRIO, s_enc_data.task_stack, s_enc_data.task_buffer);

    s_enc_data.encoder_initialized = true;

    ESP_LOGD(TAG, "Opus encoder initialized as singleton (stopped, use start() to begin)");
    webrtc_mem_utils_print_stats(TAG);

    return s_enc_data.encoder_handle;

cleanup:
    // Conditional cleanup based on what was allocated
    if (s_enc_data.task_buffer != NULL) {
        heap_caps_free(s_enc_data.task_buffer);
        s_enc_data.task_buffer = NULL;
    }
    if (s_enc_data.task_stack != NULL) {
        heap_caps_free(s_enc_data.task_stack);
        s_enc_data.task_stack = NULL;
    }
    if (s_enc_data.inbuf != NULL) {
        heap_caps_free(s_enc_data.inbuf);
        s_enc_data.inbuf = NULL;
    }
    if (s_enc_data.outbuf != NULL) {
        heap_caps_free(s_enc_data.outbuf);
        s_enc_data.outbuf = NULL;
    }
    s_enc_data.insize = 0;
    s_enc_data.outsize = 0;

    if (s_enc_data.run_semaphore != NULL) {
        vSemaphoreDelete(s_enc_data.run_semaphore);
        s_enc_data.run_semaphore = NULL;
    }
    if (s_enc_data.frame_queue != NULL) {
        vQueueDelete(s_enc_data.frame_queue);
        s_enc_data.frame_queue = NULL;
    }
    if (s_enc_data.encoder_handle != NULL) {
        esp_opus_enc_close(s_enc_data.encoder_handle);
        s_enc_data.encoder_handle = NULL;
    }

    return NULL;
}

esp_opus_out_buf_t *get_opus_encoded_frame()
{
    esp_opus_out_buf_t *opus_frame = heap_caps_calloc(1, sizeof(esp_opus_out_buf_t), MALLOC_CAP_SPIRAM);
    if (!opus_frame) {
        ESP_LOGE(TAG, "Failed to allocate opus_frame");
        return NULL;
    }

    if (xQueueReceive(s_enc_data.frame_queue, opus_frame, pdMS_TO_TICKS(CONFIG_AUDIO_QUEUE_WAIT_MS)) != pdTRUE) {
        heap_caps_free(opus_frame);
        return NULL;
    }

    return opus_frame;
}

esp_err_t opus_encoder_start_internal(void)
{
    if (!s_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "Opus encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_enc_data.running) {
        ESP_LOGD(TAG, "Opus encoder already running");
        return ESP_OK;
    }

    s_enc_data.running = true;
    xSemaphoreGive(s_enc_data.run_semaphore);  // Signal encoder to start
    ESP_LOGD(TAG, "Opus encoder started");

    return ESP_OK;
}

esp_err_t opus_encoder_stop_internal(void)
{
    if (!s_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "Opus encoder not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_enc_data.running) {
        ESP_LOGD(TAG, "Opus encoder already stopped");
        return ESP_OK;
    }

    s_enc_data.running = false;
    ESP_LOGD(TAG, "Opus encoder stopped");

    return ESP_OK;
}

esp_err_t opus_encoder_deinit_internal(void)
{
    if (!s_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "Opus encoder not initialized, nothing to deinitialize");
        return ESP_OK;
    }

    // Stop encoding first
    opus_encoder_stop_internal();

    // Drain the frame queue with limited iterations to prevent infinite loop
    if (s_enc_data.frame_queue != NULL) {
        ESP_LOGD(TAG, "Draining frame queue...");
        esp_opus_out_buf_t opus_frame;
        int drained_count = 0;
        const int max_drain_iterations = CONFIG_AUDIO_FRAME_QUEUE_SIZE;  // Prevent infinite loop

        while (xQueueReceive(s_enc_data.frame_queue, &opus_frame, 0) == pdTRUE &&
               drained_count < max_drain_iterations) {
            if (opus_frame.buffer) {
                heap_caps_free(opus_frame.buffer);
            }
            drained_count++;
        }

        if (drained_count > 0) {
            ESP_LOGD(TAG, "Drained %d frames from queue", drained_count);
        }
        if (drained_count >= max_drain_iterations) {
            ESP_LOGI(TAG, "Reached max drain limit, queue may still contain frames");
        }
    }

    // Singleton pattern: encoder task remains running but paused
    ESP_LOGD(TAG, "Opus encoder is singleton - task remains paused until start() is called");

    return ESP_OK;
}
#else
void *opus_encoder_init_internal(audio_capture_config_t *config)
{
    (void) config;
    return NULL;
}

esp_opus_out_buf_t *get_opus_encoded_frame()
{
  return NULL;
}

esp_err_t opus_encoder_start_internal(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}

esp_err_t opus_encoder_stop_internal(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}

esp_err_t opus_encoder_deinit_internal(void)
{
    // No-op for unsupported targets
    return ESP_OK;
}
#endif
