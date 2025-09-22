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
#include "video_capture.h"
#if CONFIG_IDF_TARGET_ESP32P4 || CONFIG_IDF_TARGET_ESP32S3
#include <esp_h264_types.h>
#else
/**
 * @brief  Enumerate video frame type
 */
 typedef enum {
    ESP_H264_FRAME_TYPE_INVALID = -1,  /*<! Encoder not ready or parameters are invalidate */
    ESP_H264_FRAME_TYPE_IDR     = 0,   /*<! Instantaneous decoding refresh (IDR) frame
                                            IDR frames are essentially I-frames and use intra-frame prediction.
                                            IDR frames refresh immediately
                                            So IDR frames assume the random access function, a new IDR frame starts,
                                            can recalculate a new GOP start encoding, the player can always play from an IDR frame,
                                            because after it no frame references the previous frame.
                                            If there is no IDR frame in a video, the video cannot be accessed randomly. */
    ESP_H264_FRAME_TYPE_I       = 1,   /*<! Intra frame(I frame) type. If output frame type is this,
                                            it means this frame is I-frame excpet IDR frame. */
    ESP_H264_FRAME_TYPE_P       = 2,   /*<! Predicted frame (P-frame) type */
} esp_h264_frame_type_t;
#endif

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
