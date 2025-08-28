/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "bsp/esp-bsp.h"
#include "bsp/bsp_board_extra.h"

static const char *TAG = "bsp_extra_board";

static esp_codec_dev_handle_t play_dev_handle;

static bool _is_audio_init = false;
static int _vloume_intensity = CODEC_DEFAULT_VOLUME;
static SemaphoreHandle_t _audio_init_mutex = NULL;

esp_err_t bsp_extra_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_read(play_dev_handle, audio_buffer, len);
    *bytes_read = len;
    return ret;
}

esp_err_t bsp_extra_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_write(play_dev_handle, audio_buffer, len);
    *bytes_written = len;
    return ret;
}

esp_err_t bsp_extra_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    esp_err_t ret = ESP_OK;

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = rate,
        .channel = ch,
        .bits_per_sample = bits_cfg,
    };

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_close(record_dev_handle);
    //     ret |= esp_codec_dev_set_in_gain(record_dev_handle, CODEC_DEFAULT_ADC_VOLUME);
    // }

    if (play_dev_handle) {
        ret |= esp_codec_dev_open(play_dev_handle, &fs);
    }
    // if (record_dev_handle) {
    //     ret |= esp_codec_dev_open(record_dev_handle, &fs);
    // }
    return ret;
}

esp_err_t bsp_extra_codec_volume_set(int volume, int *volume_set)
{
    ESP_RETURN_ON_ERROR(esp_codec_dev_set_out_vol(play_dev_handle, volume), TAG, "Set Codec volume failed");
    _vloume_intensity = volume;

    ESP_LOGI(TAG, "Setting volume: %d", volume);

    return ESP_OK;
}

int bsp_extra_codec_volume_get(void)
{
    return _vloume_intensity;
}

esp_err_t bsp_extra_codec_mute_set(bool enable)
{
    esp_err_t ret = ESP_OK;
    ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
    return ret;
}

esp_err_t bsp_extra_codec_dev_stop(void)
{
    esp_err_t ret = ESP_OK;

    if (play_dev_handle) {
        ret = esp_codec_dev_close(play_dev_handle);
    }

    return ret;
}

esp_err_t bsp_extra_codec_dev_resume(void)
{
    return bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CODEC_DEFAULT_CHANNEL);
}

esp_err_t bsp_extra_codec_init()
{
    // Create mutex on first call
    if (_audio_init_mutex == NULL) {
        _audio_init_mutex = xSemaphoreCreateMutex();
        if (_audio_init_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create audio init mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    // Thread-safe initialization
    if (xSemaphoreTake(_audio_init_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take audio init mutex");
        return ESP_ERR_TIMEOUT;
    }

    // Double-check pattern inside mutex
    if (_is_audio_init) {
        ESP_LOGD(TAG, "Audio codec already initialized, skipping");
        xSemaphoreGive(_audio_init_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing BSP audio codec (thread-safe)");

    play_dev_handle = bsp_audio_codec_speaker_init();
    if (!play_dev_handle) {
        ESP_LOGE(TAG, "Failed to initialize audio codec speaker");
        xSemaphoreGive(_audio_init_mutex);
        return ESP_FAIL;
    }

    // Only set FS if this is the first initialization
    esp_err_t ret = bsp_extra_codec_set_fs(CODEC_DEFAULT_SAMPLE_RATE, CODEC_DEFAULT_BIT_WIDTH, CONFIG_BSP_I2S_NUM);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set codec FS during init: %s", esp_err_to_name(ret));
        xSemaphoreGive(_audio_init_mutex);
        return ret;
    }

    _is_audio_init = true;
    ESP_LOGI(TAG, "BSP audio codec initialized successfully");

    xSemaphoreGive(_audio_init_mutex);
    return ESP_OK;
}
