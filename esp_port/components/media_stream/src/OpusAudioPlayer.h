/**
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_err.h"
#include "inttypes.h"

#pragma once

/**
 * @brief Initialize OPUS audio player
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t OpusAudioPlayerInit();

/**
 * @brief Deinitialize OPUS audio player
 *
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t OpusAudioPlayerDeinit();

/**
 * @brief Decode one frame of OPUS to PCM
 *
 * @param data Pointer to encoded OPUS data
 * @param size Size of encoded OPUS data
 * @return esp_err_t ESP_OK on success, otherwise an error code
 */
esp_err_t OpusAudioPlayerDecode(uint8_t *data, size_t size);
