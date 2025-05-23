/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "H264FrameGrabber.h"
#include "esp_h264_types.h"

#define WIDTH               (1920)
#define HEIGHT              (1080)

typedef struct {
    uint8_t *buffer; /*<! Data buffer */
    uint32_t len;    /*<! It is buffer length in byte */
} esp_h264_buf_t;

// Data read callback to read raw data
typedef void data_read_cb_t(void *ctx, esp_h264_buf_t *in_data);

// Data write callback to output encoded frames
typedef void data_write_cb_t(void *ctx, esp_h264_out_buf_t *out_data);

#define DEFAULT_ENCODER_CFG() { \
    .gop = 10, \
    .fps = 22, \
    .res = { \
        .width = WIDTH, \
        .height = HEIGHT, \
    }, \
    .rc.bitrate = 1 * 1024 * 1024, \
    .rc.qp_min = 30, \
    .rc.qp_max = 40, \
    .pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY, \
}

typedef struct {
    data_read_cb_t *read_cb;
    data_write_cb_t *write_cb;
    esp_h264_enc_cfg_t enc_cfg;
} h264_enc_user_cfg_t;

/* setup encoder with given parameters */
esp_err_t esp_h264_setup_encoder(h264_enc_user_cfg_t *cfg);
esp_err_t esp_h264_hw_enc_process_one_frame();
esp_h264_out_buf_t *esp_h264_hw_enc_encode_frame(uint8_t *frame, size_t frame_len);
esp_err_t esp_h264_hw_enc_set_bitrate(uint32_t bitrate);
void esp_h264_destroy_encoder();
