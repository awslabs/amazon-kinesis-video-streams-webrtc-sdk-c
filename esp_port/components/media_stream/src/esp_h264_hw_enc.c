/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include "dirent.h"
#include <stdio.h>
#include <errno.h>

#include <esp_log.h>

#include "esp_h264_enc_single_hw.h"
#include "esp_h264_enc_single.h"
#include "esp_h264_alloc.h"

#include "esp_dma_utils.h"
#include "esp_cache.h"

#include "esp_h264_hw_enc.h"
// #include "allocators.h"

static char *TAG = "h264_hw_enc";
static void audio_mem_print(const char *tag, int line, const char *func)
{
#ifdef CONFIG_SPIRAM_BOOT_INIT
    ESP_LOGI(tag, "Func:%s, Line:%d, MEM Total:%" PRIu32 " Bytes, SPI:%d Bytes, SPI:%d Bytes\r\n",
             func, line, esp_get_free_heap_size(), (int) heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
             (int) heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
    ESP_LOGI(tag, "Func:%s, Line:%d, MEM Total:%" PRIu32 " Bytes\r\n", func, line, esp_get_free_heap_size());
#endif
}

// #define CPU_TEST
#ifdef CPU_TEST
#include "esp_cpu.h"
#endif

typedef struct {
    esp_h264_enc_t *enc;
    data_read_cb_t *data_read_cb;
    data_write_cb_t *data_write_cb;
    esp_h264_enc_cfg_t cfg;
    esp_h264_enc_in_frame_t in_frame;
    esp_h264_enc_out_frame_t out_frame;
    bool reset_requested;
    uint32_t delay_ms;
} enc_data_t;

static enc_data_t enc_data;

esp_err_t esp_h264_hw_enc_process_one_frame()
{
    esp_h264_err_t ret = ESP_H264_ERR_OK;
    if (enc_data.reset_requested) {
        esp_h264_enc_param_hw_handle_t param_hd = NULL;
        ret = esp_h264_enc_hw_get_param_hd(enc_data.enc, &param_hd);
        if (ret == ESP_H264_ERR_OK) {
            printf("Setting new bitrate %d...\n", (int) enc_data.cfg.rc.bitrate);
            ret = esp_h264_enc_set_bitrate(&param_hd->base, enc_data.cfg.rc.bitrate);
            if (ret != ESP_H264_ERR_OK) {
                printf("esp_h264_enc_set_bitrate failed .line %d \n", __LINE__);
            }
            vTaskDelay(pdMS_TO_TICKS(enc_data.delay_ms));
        }
        enc_data.reset_requested = false;
    }
#ifdef CPU_TEST
    static int index_c = 0;
    uint32_t start = 0;
    uint32_t stop = 0;
    double sum = 0.0;
    int frame = 0;
    while (1) {
        index_c++;
        // if (index_c > COLOR_NUM) {
        if (index_c > 100) {
            index_c = 0;
            break;
        }
#else
    {
#endif
        esp_h264_buf_t read_data = {
            .buffer = enc_data.in_frame.raw_data.buffer,
            .len = enc_data.in_frame.raw_data.len
        };
        if (enc_data.data_read_cb) {
            enc_data.data_read_cb(NULL, &read_data);
            if (read_data.buffer == NULL) {
                return ESP_ERR_NOT_FOUND;
            }
            enc_data.in_frame.raw_data.buffer = read_data.buffer;
        }

#ifdef CPU_TEST
        start = esp_cpu_get_cycle_count();
#endif
        ret = esp_h264_enc_process(enc_data.enc, &enc_data.in_frame, &enc_data.out_frame);

#ifdef CPU_TEST
        stop = esp_cpu_get_cycle_count();
        frame++;
        sum += (double)((stop - start));
#endif
        if (ret != ESP_H264_ERR_OK) {
            printf("process failed. line %d \n", __LINE__);
            return ESP_FAIL;
        }
        esp_h264_out_buf_t write_data = {
            .buffer = enc_data.out_frame.raw_data.buffer,
            .len = enc_data.out_frame.length,
            .type = enc_data.out_frame.frame_type
        };

        if (enc_data.data_write_cb) {
            // esp_h264_cache_check_and_writeback(enc_data.out_frame.raw_data.buffer, enc_data.out_frame.length);
            enc_data.data_write_cb(NULL, &write_data);
        }
    }
#ifdef CPU_TEST
    sum /= 1000 * CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    printf("res %d * %d sum %.4f, frame %d, sum/frame %.4f fps %.4f \n",
           enc_data.cfg.res.width, enc_data.cfg.res.height, sum, frame, sum / frame, 1000 * frame / sum);
#endif
    return ESP_OK;
}

esp_h264_out_buf_t *esp_h264_hw_enc_encode_frame(uint8_t *frame, size_t frame_len)
{
    esp_h264_err_t ret = ESP_H264_ERR_OK;
    if (enc_data.reset_requested) {
        esp_h264_enc_param_hw_handle_t param_hd = NULL;
        ret = esp_h264_enc_hw_get_param_hd(enc_data.enc, &param_hd);
        if (ret == ESP_H264_ERR_OK) {
            printf("Setting new bitrate %d...\n", (int) enc_data.cfg.rc.bitrate);
            ret = esp_h264_enc_set_bitrate(&param_hd->base, enc_data.cfg.rc.bitrate);
            if (ret != ESP_H264_ERR_OK) {
                printf("esp_h264_enc_set_bitrate failed .line %d \n", __LINE__);
            }
            vTaskDelay(pdMS_TO_TICKS(enc_data.delay_ms));
        }
        enc_data.reset_requested = false;
    }

    {
        enc_data.in_frame.raw_data.buffer = frame;
        enc_data.in_frame.raw_data.len = frame_len;

        // uint64_t start_time = esp_timer_get_time();
        ret = esp_h264_enc_process(enc_data.enc, &enc_data.in_frame, &enc_data.out_frame);
        // uint64_t end_time = esp_timer_get_time();
        // printf("Encode FPS: %d\n", (int) (1000000 / (end_time - start_time)));

        if (ret != ESP_H264_ERR_OK) {
            ESP_LOGE(TAG, "esp_h264_enc_process failed. line %d", __LINE__);
            return NULL;
        }
        esp_h264_out_buf_t *out_buf = calloc(1, sizeof(esp_h264_out_buf_t));
        if (!out_buf) {
            ESP_LOGE(TAG, "Allocation failed for esp_h264_out_buf_t");
            return NULL;
        }
        out_buf->buffer = heap_caps_aligned_calloc(64, 1, enc_data.out_frame.length, MALLOC_CAP_SPIRAM);
        if (!out_buf->buffer) {
            ESP_LOGE(TAG, "mem allocation failed for frame_buffer. line %d", __LINE__);
            free(out_buf);
            return NULL;
        }
        // Good place to add cache flush
        esp_cache_msync(enc_data.out_frame.raw_data.buffer, (enc_data.out_frame.length + 63) & ~63, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

        memcpy(out_buf->buffer, enc_data.out_frame.raw_data.buffer, enc_data.out_frame.length);
        out_buf->len = enc_data.out_frame.length;
        out_buf->type = enc_data.out_frame.frame_type;

        return out_buf;
    }
    return NULL;
}

void esp_h264_destroy_encoder()
{
    esp_h264_enc_close(enc_data.enc);
    esp_h264_enc_del(enc_data.enc);
    enc_data.enc = NULL;

    if (enc_data.out_frame.raw_data.buffer) {
        heap_caps_free(enc_data.out_frame.raw_data.buffer);
        enc_data.out_frame.raw_data.buffer = NULL;
    }
}

esp_err_t esp_h264_setup_encoder(h264_enc_user_cfg_t *user_cfg)
{
    esp_h264_err_t ret = ESP_H264_ERR_OK;

    esp_h264_enc_cfg_t cfg = DEFAULT_ENCODER_CFG();

    if (user_cfg) {
        enc_data.data_read_cb = user_cfg->read_cb;
        enc_data.data_write_cb = user_cfg->write_cb;
        cfg = user_cfg->enc_cfg;
    }

    uint16_t width = ((cfg.res.width + 15) >> 4 << 4);
    uint16_t height = ((cfg.res.height + 15) >> 4 << 4);
    audio_mem_print("H264 HW", __LINE__, __func__);
    enc_data.in_frame.raw_data.len = (width * height + (width * height >> 1));

    enc_data.out_frame.raw_data.len = enc_data.in_frame.raw_data.len;
    // uint32_t actual_size = 0;
    // enc_data.out_frame.raw_data.buffer = esp_h264_aligned_calloc(64, 1, enc_data.out_frame.raw_data.len, &actual_size, MALLOC_CAP_SPIRAM);
    enc_data.out_frame.raw_data.buffer = heap_caps_aligned_calloc(64, 1, enc_data.out_frame.raw_data.len, MALLOC_CAP_SPIRAM);

    if (!enc_data.out_frame.raw_data.buffer) {
        printf("mem allocation failed.line %d \n", __LINE__);
        audio_mem_print("H264 HW", __LINE__, __func__);
        return ESP_FAIL;
    }
    esp_cache_msync(enc_data.out_frame.raw_data.buffer, enc_data.out_frame.raw_data.len,
                    ESP_CACHE_MSYNC_FLAG_DIR_M2C);

    ret = esp_h264_enc_hw_new(&cfg, &enc_data.enc);
    if (ret != ESP_H264_ERR_OK) {
        printf("new failed. line %d \n", __LINE__);
        esp_h264_destroy_encoder();
        return ESP_FAIL;
    }

    ret = esp_h264_enc_open(enc_data.enc);
    if (ret != ESP_H264_ERR_OK) {
        esp_h264_destroy_encoder();
        return ESP_FAIL;
    }
    enc_data.cfg = cfg;
    return ESP_OK;
}

esp_err_t esp_h264_hw_enc_set_reset_request()
{
    enc_data.delay_ms = 0;
    enc_data.reset_requested = true;
    return ESP_OK;
}

#define MIN_BITRTATE_DELTA  (10 * 1024)
#define MIN_BITRATE         (500 * 1024)
#define MAX_BITRATE         (3 * 1024 * 1024)
esp_err_t esp_h264_hw_enc_set_bitrate(uint32_t bitrate)
{
    // ESP_LOGI(TAG, "received bitrate suggestion: %d", (int) bitrate);
    int new_bitrate = (int) bitrate;
    int curr_bitrate = (int) enc_data.cfg.rc.bitrate;

    if (new_bitrate < MIN_BITRATE) {
        new_bitrate = MIN_BITRATE;
    }
    if (new_bitrate > MAX_BITRATE) {
        new_bitrate = MAX_BITRATE;
    }
    if ((new_bitrate > curr_bitrate + MIN_BITRTATE_DELTA) || (new_bitrate < curr_bitrate - MIN_BITRTATE_DELTA)) {
        enc_data.cfg.rc.bitrate = new_bitrate;
        enc_data.reset_requested = true;
        enc_data.delay_ms = 0;
        if (new_bitrate < curr_bitrate) {
            enc_data.delay_ms = 50;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}
#endif
