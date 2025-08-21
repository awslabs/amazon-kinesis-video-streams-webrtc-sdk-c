/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief grab camera frames and encode them with h264 encoder
 *
 */

#pragma once

#include <stdint.h>
#include <esp_h264_types.h>
#include "video_capture.h"

typedef struct {
    uint8_t *buffer; /*<! Data buffer */
    uint32_t len;    /*<! It is buffer length in byte */
    esp_h264_frame_type_t type; /* Frame type */
} esp_h264_out_buf_t;

esp_h264_out_buf_t *get_h264_encoded_frame();

/* Explicitly initialize camera and encoder */
esp_err_t camera_and_encoder_init(video_capture_config_t *config);

/* Start/stop/deinit functions for H264 encoder */
esp_err_t h264_encoder_start(void);
esp_err_t h264_encoder_stop(void);
esp_err_t h264_encoder_deinit(void);
