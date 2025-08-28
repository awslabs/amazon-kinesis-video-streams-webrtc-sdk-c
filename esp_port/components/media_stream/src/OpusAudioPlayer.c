/**
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "esp_err.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "esp_audio_dec_default.h"
#include "esp_audio_dec.h"
#include "ringbuf.h"

#include "sdkconfig.h"
#if CONFIG_IDF_TARGET_ESP32P4
#include "bsp/bsp_board_extra.h"
#include "bsp/esp32_p4_function_ev_board.h"
#endif

static const char *TAG = "OPUS_AUDIO_PLAYER";

typedef struct {
    bool                      use_common_api;
    esp_audio_dec_handle_t    decoder;
    esp_audio_dec_out_frame_t out_frame;
    bool                      decode_err;
} write_ctx_t;

static write_ctx_t write_ctx;
static rb_handle_t rb_handle;

#define OPUS_PLAYER_TASK        (1)
#define OPUS_SAMPLE_RATE        (48000)
#define OPUS_CHANNELS           (2)
#define OPUS_OUTPUT_BUFFER_SIZE (4096)
#define CODEC_SAMPLE_RATE       (16000)
#define OPUS_PLAYER_OUT_RB_SIZE (32000)
#define CODEC_CHANNELS          (1)

static int decode_one_frame(uint8_t *data, int size)
{
    esp_audio_dec_in_raw_t raw = {
        .buffer = data,
        .len = size,
    };
    esp_audio_dec_out_frame_t *out_frame = &write_ctx.out_frame;

    int ret = 0;
    // Input data may contain multiple frames, each call of process decode only one frame
    while (raw.len) {
        if (write_ctx.use_common_api) {
            ret = esp_audio_dec_process(write_ctx.decoder, &raw, out_frame);
        } else {
            esp_audio_dec_info_t aud_info;
            ret = esp_opus_dec_decode(write_ctx.decoder, &raw, out_frame, &aud_info);
        }
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
            ESP_LOGW(TAG, "Output buffer not enough, reallocating to %d", (int)out_frame->needed_size);
            // When output buffer for pcm is not enough, need reallocate it according reported `needed_size` and retry
            uint8_t *new_buf = heap_caps_realloc(out_frame->buffer, out_frame->needed_size, MALLOC_CAP_SPIRAM);
            if (new_buf == NULL) {
                ESP_LOGE(TAG, "Failed to reallocate buffer for pcm");
                break; // skips this frame
            }
            out_frame->buffer = new_buf;
            out_frame->len = out_frame->needed_size;
            continue;
        }
        if (ret != ESP_AUDIO_ERR_OK) {
            ESP_LOGE(TAG, "Failed to decode data ret %d", ret);
            write_ctx.decode_err = true;
            break;
        } else {
            ESP_LOGD(TAG, "Decoded the frame. Output buffer size: %d", (int) out_frame->decoded_size);
            raw.len -= raw.consumed;
            raw.buffer += raw.consumed;
        }

        if (raw.len != 0) {
            ESP_LOGW(TAG, "More data to decode %d", (int) raw.len);
            break;
        }
    }
    return ret;
}

#if CONFIG_IDF_TARGET_ESP32P4
#if PLAY_TEST_AUDIO
#define EXAMPLE_PDM_TX_FREQ_HZ          16000           // I2S PDM TX frequency
#define EXAMPLE_WAVE_AMPLITUDE          (1000.0)        // 1~32767
#define CONST_PI                        (3.1416f)
#define EXAMPLE_SINE_WAVE_LEN(tone)     (uint32_t)((EXAMPLE_PDM_TX_FREQ_HZ / (float)tone) + 0.5) // The sample point number per sine wave to generate the tone
#define EXAMPLE_TONE_LAST_TIME_MS       500
#define EXAMPLE_BYTE_NUM_EVERY_TONE     (EXAMPLE_TONE_LAST_TIME_MS * EXAMPLE_PDM_TX_FREQ_HZ / 1000)

/* The frequency of tones: do, re, mi, fa, so, la, si, in Hz. */
static const uint32_t tone[3][7] = {
    {262, 294, 330, 349, 392, 440, 494},        // bass
    {523, 587, 659, 698, 784, 880, 988},        // alto
    {1046, 1175, 1318, 1397, 1568, 1760, 1976}, // treble
};
/* Numbered musical notation of 'twinkle twinkle little star' */
static const uint8_t song[28] = {1, 1, 5, 5, 6, 6, 5,
                                 4, 4, 3, 3, 2, 2, 1,
                                 5, 5, 4, 4, 3, 3, 2,
                                 5, 5, 4, 4, 3, 3, 2
                                };
/* Rhythm of 'twinkle twinkle little star', it's repeated in four sections */
static const uint8_t rhythm[7] = {1, 1, 1, 1, 1, 1, 2};

static const char *tone_name[3] = {"bass", "alto", "treble"};

#define EXAMPLE_BUFF_SIZE               2048

void i2s_write_task(void *args)
{
    int16_t *w_buf = (int16_t *)calloc(1, EXAMPLE_BUFF_SIZE);
    assert(w_buf);
    size_t w_bytes = 0;
    i2s_chan_handle_t tx_chan = bsp_audio_get_tx_channel();
    uint8_t cnt = 0;            // The current index of the song
    uint8_t tone_select = 0;    // To selecting the tone level

    printf("Playing %s `twinkle twinkle little star`\n", tone_name[tone_select]);
    while (1) {
        int tone_point = EXAMPLE_SINE_WAVE_LEN(tone[tone_select][song[cnt] - 1]);
        /* Generate the tone buffer */
        for (int i = 0; i < tone_point; i++) {
            w_buf[i] = (int16_t)((sin(2 * (float)i * CONST_PI / tone_point)) * EXAMPLE_WAVE_AMPLITUDE);
        }
        for (int tot_bytes = 0; tot_bytes < EXAMPLE_BYTE_NUM_EVERY_TONE * rhythm[cnt % 7]; tot_bytes += w_bytes) {
            /* Play the tone */
            bsp_extra_i2s_write(w_buf, tone_point * sizeof(int16_t), &w_bytes, 100);
            // i2s_channel_write(tx_chan, w_buf, tone_point * sizeof(int16_t), &w_bytes, 1000)
            if (w_bytes != tone_point * sizeof(int16_t)) {
                ESP_LOGE(TAG, "Failed to write to I2S");
                continue;
            }
        }
        cnt++;
        /* If finished playing, switch the tone level */
        if (cnt == sizeof(song)) {
            cnt = 0;
            tone_select++;
            tone_select %= 3;
            printf("Playing %s `twinkle twinkle little star`\n", tone_name[tone_select]);
        }
        /* Gap between the tones */
        vTaskDelay(15);
    }
    free(w_buf);
    vTaskDelete(NULL);
}
#else
#include "resampling.h"
static void i2s_write_task(void *arg)
{
    audio_resample_config_t resample_cfg = {0};

    // 20 ms of 48K stereo audio
    static int samples_to_read = OPUS_SAMPLE_RATE / 1000 * 20;
    uint16_t resampled_buf[OPUS_CHANNELS * samples_to_read]; // enough to hold stereo to mono

    // i2s_chan_handle_t i2s_tx_chan = bsp_audio_get_tx_channel();

    while (1) {
        /**
         * The received OPUS audio is 48K/2 but we want to play it at 16K/1
         * Resample the audio 48K/2 to 16K/1 in-place and write to I2S
         */
        int bytes_read = rb_read(rb_handle, (void *) resampled_buf, sizeof(resampled_buf), portMAX_DELAY);
        if (bytes_read <= 0) {
            ESP_LOGE(TAG, "Failed to read from ringbuffer");
            continue;
        }

        int outnum = audio_resample_down_channel(
                (short *)resampled_buf, (short *)resampled_buf, OPUS_SAMPLE_RATE, CODEC_SAMPLE_RATE,
                samples_to_read * OPUS_CHANNELS, samples_to_read * CODEC_CHANNELS, 0, &resample_cfg);
        ESP_LOGV(TAG, "Resampled %d samples to %d", samples_to_read * OPUS_CHANNELS, outnum);

        size_t bytes_to_write = outnum * sizeof(int16_t);
        size_t bytes_written = 0;
        bsp_extra_i2s_write(resampled_buf, bytes_to_write, &bytes_written, 100);
        // i2s_channel_write(i2s_tx_chan, resampled_buf, bytes_to_write, &bytes_written, 100);
        if (bytes_written != bytes_to_write) {
            // This should be very rare
            ESP_LOGW(TAG, "Failed to write to I2S");
            continue;
        }
    }
}
#endif
#else
static void i2s_write_task(void *arg)
{
    while (1) {
        ESP_LOGW(TAG, "I2S write task not implemented for this target");
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
    }
}
#endif
esp_err_t opus_player_decode_and_play_one_frame(uint8_t *data, size_t size)
{
    // printf("Decoding and playing one frame %p of size %zu\n", data, size);

    decode_one_frame(data, size);

#if 0
    size_t bytes_written = 0;
    size_t bytes_to_write = write_ctx.out_frame.decoded_size;

    bsp_extra_i2s_write(write_ctx.out_frame.buffer, write_ctx.out_frame.decoded_size, &bytes_written, 10);
    if (bytes_written != bytes_to_write) {
        ESP_LOGW(TAG, "Failed to write to I2S");
        return ESP_FAIL;
    }
#else
    rb_write(rb_handle, write_ctx.out_frame.buffer, write_ctx.out_frame.decoded_size, 10);
#endif
    return ESP_OK;
}

static esp_err_t init_decoder()
{
    // Register OPUS decoder, or you can call `esp_audio_dec_register_default` to register all supported decoder
    if (esp_opus_dec_register() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPUS decoder");
        return ESP_FAIL;
    }

    // Configuration for OPUS decoder
    esp_opus_dec_cfg_t opus_cfg = {.sample_rate = OPUS_SAMPLE_RATE, .channel = OPUS_CHANNELS, .self_delimited = false};
    memset(&write_ctx, 0, sizeof(write_ctx));
    write_ctx.use_common_api = true;
    // Allocate buffer to hold output PCM data
    write_ctx.out_frame.len = OPUS_OUTPUT_BUFFER_SIZE;
    write_ctx.out_frame.buffer = heap_caps_malloc(write_ctx.out_frame.len, MALLOC_CAP_SPIRAM);
    if (write_ctx.out_frame.buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for OPUS decoder");
        return ESP_FAIL;
    }
    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(opus_cfg),
    };

    // Open decoder
    if (esp_audio_dec_open(&dec_cfg, &write_ctx.decoder) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open OPUS decoder");
        return ESP_FAIL;
    }
    return ESP_OK;
}

#if OPUS_PLAYER_TASK
typedef struct frame_data_t {
    void *data;
    size_t len; // size of the data
} frame_data_t;

#define MAX_QUEUE_SIZE 10
#define QUEUE_RECV_TIMEOUT pdMS_TO_TICKS(20)
#define QUEUE_SEND_TIMEOUT pdMS_TO_TICKS(10)

static QueueHandle_t frame_queue = NULL;

static void opus_player_task(void *arg)
{
    frame_data_t frame = {0};
    while(1) {
        if (xQueueReceive(frame_queue, &frame, QUEUE_RECV_TIMEOUT) != pdTRUE) {
            ESP_LOGV(TAG, "No frame to decode");
            continue;
        }
        opus_player_decode_and_play_one_frame(frame.data, frame.len);
        heap_caps_free(frame.data);
    }
}
#endif

esp_err_t OpusAudioPlayerInit()
{
    static bool initialized = false; // allow only one initialization
    if (initialized) {
        ESP_LOGI(TAG, "OPUS audio player already initialized");
        return ESP_OK;
    }

#if OPUS_PLAYER_TASK
    frame_queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(frame_data_t));
    if (!frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return ESP_FAIL;
    }
#endif

#if CONFIG_IDF_TARGET_ESP32P4
    bsp_extra_codec_init();
    bsp_extra_codec_mute_set(false);
    int volume_set = 0;
    bsp_extra_codec_volume_set(60, &volume_set);
    // ESP_LOGI(TAG, "Volume set: %d", volume_set);
    (void) volume_set;
    bsp_extra_codec_set_fs(16000, 16, I2S_SLOT_MODE_MONO);
    // bsp_extra_codec_dev_resume();
#endif

    if (init_decoder() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize decoder");
        return ESP_FAIL;
    }

#if OPUS_PLAYER_TASK
    rb_handle = rb_init("opus_player", OPUS_PLAYER_OUT_RB_SIZE);

#define OPUS_TASK_STACK_SIZE     (16 * 1024)
#define OPUS_TASK_PRIO           (4) // A bit higher than grabber tasks
    StaticTask_t *task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    void *task_stack = heap_caps_calloc(1, OPUS_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    assert(task_buffer && task_stack);

    /* the task never exits, so do not bother to free the buffers */
    TaskHandle_t opus_task_handle = xTaskCreateStatic(opus_player_task, "opus_player", OPUS_TASK_STACK_SIZE,
                                                     NULL, OPUS_TASK_PRIO, task_stack, task_buffer);
    if (opus_task_handle == NULL) {
        ESP_LOGE(TAG, "failed to create opus player task!");
    }

#define I2S_SYNC_TASK_STACK_SIZE     (10 * 1024)
#define I2S_SYNC_TASK_PRIO           (6) // IO sensitive task
    StaticTask_t *i2s_write_task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    void *i2s_write_task_stack = heap_caps_calloc(1, I2S_SYNC_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM);
    assert(i2s_write_task_buffer && i2s_write_task_stack);

    /* the task never exits, so do not bother to free the buffers */
    TaskHandle_t i2s_write_task_handle = xTaskCreateStatic(i2s_write_task, "i2s_write", I2S_SYNC_TASK_STACK_SIZE,
                                                     NULL, I2S_SYNC_TASK_PRIO, i2s_write_task_stack, i2s_write_task_buffer);
    if (i2s_write_task_handle == NULL) {
        ESP_LOGE(TAG, "failed to create i2s sync task!");
    }
#endif


    ESP_LOGI(TAG, "OPUS audio player initialized");

    initialized = true;
    return ESP_OK;
}

esp_err_t OpusAudioPlayerDeinit()
{
    // Clear up resources
    esp_audio_dec_close(write_ctx.decoder);
    free(write_ctx.out_frame.buffer);
    esp_audio_dec_unregister(ESP_AUDIO_TYPE_OPUS);
    return ESP_OK;
}

esp_err_t OpusAudioPlayerDecode(uint8_t *data, size_t size)
{
#if OPUS_PLAYER_TASK
    uint8_t *data_dup = heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM);
    if (data_dup == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for OPUS decoder");
        return ESP_FAIL;
    }
    memcpy(data_dup, data, size);
    frame_data_t frame = {0};
    frame.data = data_dup;
    frame.len = size;
    if (xQueueSend(frame_queue, &frame, QUEUE_SEND_TIMEOUT) != pdTRUE) {
        ESP_LOGV(TAG, "Failed to insert frame into queue");
        heap_caps_free(data_dup);
        return ESP_FAIL;
    }
#else
    return opus_player_decode_and_play_one_frame(data, size);
#endif
    return ESP_OK;
}
