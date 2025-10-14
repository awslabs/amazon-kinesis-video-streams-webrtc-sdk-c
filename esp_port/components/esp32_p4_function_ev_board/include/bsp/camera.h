/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdio.h>
#include "esp_err.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_csi.h"
#include "driver/isp.h"
#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t hor_res;
    uint16_t ver_res;
    void **fbs;
    union {
        size_t fb_size;
        size_t *fb_size_ptr;
    };
    uint8_t num_fbs;
    isp_color_t input_data_color_type;
    isp_color_t output_data_color_type;
    uint32_t clock_rate_hz;
    uint32_t csi_lane_rate_mbps;
    struct {
        uint32_t use_external_fb: 1;
    } flags;
} bsp_camera_config_t;

typedef struct {
    uint16_t hor_res;
    uint16_t ver_res;
    int update_timeout_ms;
    isp_color_t input_data_color_type;
    isp_color_t output_data_color_type;
    struct {
        uint8_t core_id;
        uint16_t stack_size;
        uint16_t priority;
        uint16_t loop_delay_ms;
    } task;
    struct {
        uint32_t enable_awb : 1;
    } flags;
} isp_config_t;

typedef bool (*bsp_camera_trans_done_cb_t)(void);

#define ISP_TASK_CORE_DEFALUT (1)
#define ISP_CONFIG_DEFAULT(width, height, in_color, out_color) { \
    .hor_res = width,                       \
    .ver_res = height,                      \
    .update_timeout_ms = 10,                \
    .input_data_color_type = in_color,      \
    .output_data_color_type = out_color,    \
    .task = {                               \
        .core_id = ISP_TASK_CORE_DEFALUT,   \
        .stack_size = 4 * 1024,             \
        .priority = 10,                     \
        .loop_delay_ms = 10,                \
    },                                      \
    .flags = {                              \
        .enable_awb = 0,                    \
    },                                      \
}

esp_err_t bsp_camera_new(const bsp_camera_config_t *camera_config, const isp_config_t *isp_config, esp_cam_ctlr_handle_t *ret_handle);

esp_err_t bsp_camera_register_trans_done_callback(bsp_camera_trans_done_cb_t callback);

esp_err_t bsp_camera_set_frame_buffer(void *buffer);

esp_err_t bsp_camera_get_frame_buffer(uint32_t fb_num, void **fb0, ...);

#ifdef __cplusplus
}
#endif
