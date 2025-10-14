/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_dma_utils.h"
#include "esp_cache.h"
#include "esp_cam_ctlr_csi.h"
#include "esp_cam_ctlr.h"
#include "driver/isp.h"
#include "hal/color_hal.h"

#include "sensor.h"

#include "sdkconfig.h"
#include "bsp_err_check.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/camera.h"
#include "bsp/esp-bsp.h"

#define CAMERA_MAX_FB_NUM         3 // maximum supported frame buffer number

#define TEST_CSI_OV5647      (1)
#define TEST_CSI_SC2336      (0)

static const char *TAG = "bsp_camera";

static bsp_camera_trans_done_cb_t trans_done = NULL;
static uint8_t num_fbs = 0;
static size_t fb_size = 0;
static void *fb_act = NULL;
static void *fbs[CAMERA_MAX_FB_NUM] = { NULL };

static void isp_init_task(void *arg);
static bool on_cam_ctlr_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);
static bool on_cam_ctlr_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data);

esp_err_t bsp_camera_new(const bsp_camera_config_t *camera_config, const isp_config_t *isp_config, esp_cam_ctlr_handle_t *ret_handle)
{
    ESP_RETURN_ON_FALSE(camera_config && isp_config, ESP_ERR_INVALID_ARG, TAG, "Invalid configuration");
    ESP_RETURN_ON_FALSE((camera_config->flags.use_external_fb == 0) ||
                        ((camera_config->fbs != NULL) && (camera_config->fb_size > 0)), ESP_ERR_INVALID_ARG, TAG,
                        "Invalid buffer configuration");
    ESP_RETURN_ON_FALSE(camera_config->num_fbs > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid number of frame buffers");
    ESP_RETURN_ON_FALSE(camera_config->csi_lane_rate_mbps  > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid CSI lane rate");
    ESP_RETURN_ON_FALSE(camera_config->clock_rate_hz > 0, ESP_ERR_INVALID_ARG, TAG, "Invalid IDI clock rate");

    bsp_ldo_power_on();

    fb_size = camera_config->fb_size;
    if (!camera_config->flags.use_external_fb) {
        color_space_pixel_format_t out_color_format = {
            .color_type_id = camera_config->output_data_color_type,
        };
        int out_bits_per_pixel = color_hal_pixel_format_get_bit_depth(out_color_format);
        uint32_t csi_block_bytes = camera_config->hor_res * camera_config->ver_res * out_bits_per_pixel / 8;

        esp_dma_mem_info_t dma_mem_info = {
            .extra_heap_caps = MALLOC_CAP_SPIRAM,
        };
        for (int i = 0; i < camera_config->num_fbs; i++) {
            ESP_RETURN_ON_ERROR(esp_dma_capable_calloc(1, csi_block_bytes, &dma_mem_info, &fbs[i], &fb_size), TAG,
                                                       "Allocate camera buffer failed");
            // ESP_RETURN_ON_ERROR(esp_cache_msync(fbs[i], fb_size, ESP_CACHE_MSYNC_FLAG_DIR_C2M), TAG, "flush cache buffer failed");
            ESP_LOGI(TAG, "Allocate buffer at %p, size: %d", fbs[i], fb_size);
        }

        if (camera_config->fb_size_ptr != NULL) {
            *camera_config->fb_size_ptr = fb_size;
        }
    } else {
        for (int i = 0; i < camera_config->num_fbs; i++) {
            fbs[i] = camera_config->fbs[i];
            ESP_LOGI(TAG, "Use external buffer at %p, size: %d", fbs[i], fb_size);
        }
    }
    num_fbs = camera_config->num_fbs;
    fb_act = fbs[0];

    esp_cam_ctlr_handle_t cam_ctlr_handle = NULL;
    esp_cam_ctlr_csi_config_t csi_config = {
        .ctlr_id = 0,
        .clk_src = MIPI_CSI_PHY_CLK_SRC_DEFAULT,
        .h_res = camera_config->hor_res,
        .v_res = camera_config->ver_res,
        .data_lane_num = 2,
#if TEST_CSI_SC2336
        .lane_bit_rate_mbps = SC2336_MIPI_CSI_LINE_RATE_1920x1080_30FPS / 1000 / 1000,
#else
        .lane_bit_rate_mbps = camera_config->csi_lane_rate_mbps,
#endif
        .input_data_color_type = camera_config->input_data_color_type,
        .output_data_color_type = camera_config->output_data_color_type,
        .byte_swap_en = false,
        .bk_buffer_dis = false,
        .queue_items = 1,
    };
    ESP_RETURN_ON_ERROR(esp_cam_new_csi_ctlr(&csi_config, &cam_ctlr_handle), TAG, "Create new CSI controller failed");
    const esp_cam_ctlr_evt_cbs_t cbs = {
        .on_get_new_trans = on_cam_ctlr_get_new_trans,
        .on_trans_finished = on_cam_ctlr_trans_finished,
    };
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_register_event_callbacks(cam_ctlr_handle, &cbs, NULL), TAG, "Register event callbacks failed");
    ESP_RETURN_ON_ERROR(esp_cam_ctlr_enable(cam_ctlr_handle), TAG, "Enable CSI controller failed");

    ESP_LOGI(TAG, "Initialize ISP");
    isp_proc_handle_t isp_proc = NULL;


    esp_isp_processor_cfg_t isp_cfg = {
#if TEST_CSI_SC2336
        .clk_hz = SC2336_MIPI_IDI_CLOCK_RATE_1920x1080_30FPS,
#else
        .clk_hz = camera_config->clock_rate_hz,
#endif
        .input_data_source = ISP_INPUT_DATA_SOURCE_CSI,
        .input_data_color_type = isp_config->input_data_color_type,
        .output_data_color_type = isp_config->output_data_color_type,
        .has_line_start_packet = false,
        .has_line_end_packet = false,
        .h_res = isp_config->hor_res,
        .v_res = isp_config->ver_res,
    };

    ESP_ERROR_CHECK(esp_isp_new_processor(&isp_cfg, &isp_proc));
    ESP_ERROR_CHECK(esp_isp_enable(isp_proc));

    ESP_RETURN_ON_ERROR(esp_cam_ctlr_start(cam_ctlr_handle), TAG, "Start CSI controller failed");

    ESP_RETURN_ON_ERROR(bsp_i2c_init(), TAG, "Initialize I2C failed");
#if TEST_CSI_OV5647
    ESP_RETURN_ON_ERROR(sensor_ov5647_init(camera_config->hor_res, camera_config->ver_res, camera_config->input_data_color_type,
                                           camera_config->clock_rate_hz), TAG, "Initialize OV5647 failed");
#elif TEST_CSI_SC2336
    uint32_t clock_rate = SC2336_MIPI_IDI_CLOCK_RATE_1920x1080_30FPS;
    ESP_RETURN_ON_ERROR(sensor_sc2336_init(camera_config->hor_res, camera_config->ver_res, camera_config->input_data_color_type,
                                           clock_rate), TAG, "Initialize SC2336 failed");
#endif

    ESP_LOGI(TAG, "Camera initialized");

    if (ret_handle != NULL) {
        *ret_handle = cam_ctlr_handle;
    }

    return ESP_OK;
}

esp_err_t bsp_camera_register_trans_done_callback(bsp_camera_trans_done_cb_t callback)
{
    trans_done = callback;

    return ESP_OK;
}

esp_err_t bsp_camera_set_frame_buffer(void *buffer)
{
    ESP_RETURN_ON_FALSE(buffer != NULL, ESP_ERR_INVALID_ARG, TAG, "Invalid fbs");

    fb_act = buffer;

    return ESP_OK;
}

esp_err_t bsp_camera_get_frame_buffer(uint32_t fb_num, void **fb0, ...)
{
    ESP_RETURN_ON_FALSE(fb_num && fb_num <= num_fbs, ESP_ERR_INVALID_ARG, TAG, "invalid frame buffer number");
    void **fb_itor = fb0;
    va_list args;
    va_start(args, fb0);
    for (int i = 0; i < fb_num; i++) {
        if (fb_itor) {
            *fb_itor = fbs[i];
            fb_itor = va_arg(args, void **);
        }
    }
    va_end(args);
    return ESP_OK;
}

esp_err_t bsp_camera_del(void)
{

    return ESP_OK;
}

static void isp_init_task(void *arg)
{
    const isp_config_t *config = (const isp_config_t *)arg;

    ESP_LOGI(TAG, "Initialize ISP");
    // ESP_ERROR_CHECK(isp_init(config));

    vTaskDelete(NULL);
}

IRAM_ATTR static bool on_cam_ctlr_get_new_trans(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    bool need_yield = false;

    if (trans_done) {
        need_yield = trans_done();
    }

    trans->buffer = fb_act;
    trans->buflen = fb_size;

    return need_yield;
}

IRAM_ATTR static bool on_cam_ctlr_trans_finished(esp_cam_ctlr_handle_t handle, esp_cam_ctlr_trans_t *trans, void *user_data)
{
    return false;
}
