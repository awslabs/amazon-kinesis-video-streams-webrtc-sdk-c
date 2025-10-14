/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "hal/mipi_csi_types.h"
#include "hal/isp_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV5647_MIPI_IDI_CLOCK_RATE_720P_56FPS   (82031250ULL)
#define OV5647_MIPI_CSI_LINE_RATE_720P_56FPS    (OV5647_MIPI_IDI_CLOCK_RATE_720P_56FPS * 4)

#define OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS   (74000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_720P_50FPS    (OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS * 4)

#define OV5647_MIPI_IDI_CLOCK_RATE_1080P_22FPS  (98437500ULL)
#define OV5647_MIPI_CSI_LINE_RATE_1080P_22FPS   (OV5647_MIPI_IDI_CLOCK_RATE_1080P_22FPS * 4)

#define SC2336_MIPI_IDI_CLOCK_RATE_800x800_30FPS  (84000000ULL)
#define SC2336_MIPI_CSI_LINE_RATE_800x800_30FPS   (SC2336_MIPI_IDI_CLOCK_RATE_800x800_30FPS * 4)

#define SC2336_MIPI_IDI_CLOCK_RATE_1920x1080_30FPS  (84000000ULL)
#define SC2336_MIPI_CSI_LINE_RATE_1920x1080_30FPS   (SC2336_MIPI_IDI_CLOCK_RATE_1920x1080_30FPS * 4)

esp_err_t sensor_ov5647_init(uint16_t hor_res, uint16_t ver_res, isp_color_t color_mode, uint32_t clock_rate);

esp_err_t sensor_sc2336_init(uint16_t hor_res, uint16_t ver_res, isp_color_t color_mode, uint32_t clock_rate);

#ifdef __cplusplus
}
#endif
