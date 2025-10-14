/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

static QueueHandle_t frame_queue = NULL;

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

static void audio_encoder_task(void *arg)
{
    void *enc_handle = arg;
    uint8_t *inbuf = NULL;
    uint8_t *outbuf = NULL;
    int insize = 0;
    int outsize = 0;

#if CONFIG_IDF_TARGET_ESP32P4
    // esp_codec_dev_handle_t audio_handle = bsp_audio_codec_speaker_init();
    bsp_extra_codec_init();
#else
    i2s_init();
#endif
    esp_opus_enc_get_frame_size(enc_handle, &insize, &outsize);
#if READ_SAMPLE_SIZE_30
    inbuf = heap_caps_calloc(1, insize * 2, MALLOC_CAP_SPIRAM); // 20ms mono
#else
    inbuf = heap_caps_calloc(1, insize, MALLOC_CAP_SPIRAM); // 20ms mono
#endif
    outbuf = heap_caps_calloc(1, outsize, MALLOC_CAP_SPIRAM);
    while (1) {
        // Encode process
        esp_audio_enc_in_frame_t in_frame = { 0 };
        esp_audio_enc_out_frame_t out_frame = { 0 };

        in_frame.buffer = inbuf;
        in_frame.len = insize;
        out_frame.buffer = outbuf;
        out_frame.len = outsize;

        size_t bytes_read = 0;

#define I2S_READ_WAIT_MS CONFIG_AUDIO_QUEUE_WAIT_MS
#if CONFIG_IDF_TARGET_ESP32P4
        // esp_codec_dev_read(audio_handle, (void*)inbuf, insize);
        bsp_extra_i2s_read(inbuf, insize, &bytes_read, I2S_READ_WAIT_MS);
#else
#if READ_SAMPLE_SIZE_30
        i2s_read(i2s_port, (void*)inbuf, insize * 2, &bytes_read, pdMS_TO_TICKS(I2S_READ_WAIT_MS));
        // rescale the data (Actual data is 30 bits, use higher 16 bits out of those)
        for (int i = 0; i < bytes_read / 4; ++i) {
            ((uint16_t *) inbuf)[i] = (((uint32_t *) inbuf)[i] >> 14) & 0xffff;
        }
#else
        i2s_read(i2s_port, (void*)inbuf, insize, &bytes_read, pdMS_TO_TICKS(I2S_READ_WAIT_MS));
#endif
#endif

#define OPUS_ENCODE_WAIT_MS CONFIG_AUDIO_QUEUE_WAIT_MS

        esp_opus_out_buf_t opus_frame = { 0 };
        esp_audio_err_t ret = ESP_AUDIO_ERR_OK;
        ret = esp_opus_enc_process(enc_handle, &in_frame, &out_frame);
        if (ret != ESP_AUDIO_ERR_OK) {
            printf("audio encoder process failed.\n");
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
        if (xQueueSend(frame_queue, &opus_frame, pdMS_TO_TICKS(OPUS_ENCODE_WAIT_MS)) != pdTRUE) {
            heap_caps_free(opus_frame.buffer);
        }
    }
}

/* returns handle */
void *opus_encoder_init_internal(audio_capture_config_t *config)
{
    if (config == NULL) {
        return NULL;
    }

    void *enc_handle = NULL;
    esp_audio_err_t ret = ESP_AUDIO_ERR_OK;
    esp_opus_enc_config_t enc_config = ESP_OPUS_ENC_CONFIG_DEFAULT();
    enc_config.sample_rate = config->format.sample_rate;
    enc_config.channel = config->format.channels;
    enc_config.bitrate = config->bitrate;
    ret = esp_opus_enc_open(&enc_config, sizeof(esp_opus_enc_config_t), &enc_handle);
    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "Failed to initialize Opus encoder");
        return NULL;
    }

    frame_queue = xQueueCreate(CONFIG_AUDIO_FRAME_QUEUE_SIZE, sizeof(esp_opus_out_buf_t));
    if (!frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return NULL;
    }

#define ENC_TASK_STACK_SIZE     CONFIG_AUDIO_ENCODER_TASK_STACK_SIZE
#define ENC_TASK_PRIO           CONFIG_AUDIO_ENCODER_TASK_PRIORITY
    StaticTask_t *task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    void *task_stack = heap_caps_calloc(1, ENC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    assert(task_buffer && task_stack);

    /* the task never exits, so do not bother to free the buffers */
    xTaskCreateStatic(audio_encoder_task, "audio_encoder", ENC_TASK_STACK_SIZE,
                      enc_handle, ENC_TASK_PRIO, task_stack, task_buffer);

    printf("audio_encoder initialized\n");
    webrtc_mem_utils_print_stats(TAG);

    return enc_handle;
}

esp_opus_out_buf_t *get_opus_encoded_frame()
{
    esp_opus_out_buf_t *opus_frame = heap_caps_calloc(1, sizeof(esp_opus_out_buf_t), MALLOC_CAP_SPIRAM);
    if (!opus_frame) {
        ESP_LOGE(TAG, "Failed to allocate opus_frame");
        return NULL;
    }

    if (xQueueReceive(frame_queue, opus_frame, pdMS_TO_TICKS(CONFIG_AUDIO_QUEUE_WAIT_MS)) != pdTRUE) {
        heap_caps_free(opus_frame);
        return NULL;
    }

    return opus_frame;
}
#else
esp_opus_out_buf_t *get_opus_encoded_frame()
{
  return NULL;
}
#endif
