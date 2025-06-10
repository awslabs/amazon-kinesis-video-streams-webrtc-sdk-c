/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_check.h"

#include "bsp/esp-bsp.h"
#include "sensor.h"
#include "priv_include/sc2336_settings.h"

#include "esp_rom_sys.h"
#include "esp_codec_dev_defaults.h"

#if USE_NEW_I2C_DRIVER
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;
#endif

#define I2C_MASTER_TIMEOUT_MS       (1000)
static audio_codec_ctrl_if_t *i2c_ctrl_if = NULL;

static const char *TAG = "sc2336";

static void delay_us(uint32_t t)
{
#if TEST_CSI_FPGA
    for (uint32_t tu = 0; tu < t; tu++);
#else
    // ets_delay_us(t);
    esp_rom_delay_us(t);
#endif
}

static int sc2336_read(uint16_t addr, uint8_t *read_buf)
{
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xff};
#if USE_NEW_I2C_DRIVER
    return i2c_master_transmit_receive(dev_handle, addr_buf, sizeof(addr_buf), read_buf, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#else
    return i2c_master_write_read_device(BSP_I2C_NUM, SC2336_SCCB_ADDR, addr_buf, sizeof(addr_buf), read_buf, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#endif
}

static int sc2336_write(uint16_t addr, uint8_t data)
{
    uint8_t write_buf[3] = {addr >> 8, addr & 0xff, data};
#if USE_NEW_I2C_DRIVER
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#else
    return i2c_master_write_to_device(BSP_I2C_NUM, SC2336_SCCB_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#endif
}

/* write a array of registers  */
static esp_err_t sc2336_write_array(const reginfo_t *regarray)
{
    int i = 0;
    while (regarray[i].reg != SC2336_REG_END) {
        ESP_RETURN_ON_ERROR(sc2336_write(regarray[i].reg, regarray[i].val), TAG, "Write register failed");
        i++;
    }

    return ESP_OK;
}

static int set_colorbar(int enable)
{
    int ret = 0;
    uint8_t temp = 0;
    sc2336_read(0x4501, &temp);

    if (enable) {
        temp |= 0x08;
    } else {
        temp &= 0xF7;
    }
    sc2336_write(0x4501, temp);
    return ret;
}

esp_err_t sensor_sc2336_init(uint16_t hor_res, uint16_t ver_res, isp_color_t color_mode, uint32_t clock_rate)
{
    bsp_i2c_init(); // init i2c if not already initialized
#if USE_NEW_I2C_DRIVER
    i2c_bus_handle = bsp_get_i2c_bus_handle();
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SC2336_SCCB_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
#endif

    uint8_t pid_h = 0, pid_l = 0;
    sc2336_read(SC2336_REG_SENSOR_ID_H, &pid_h);
    sc2336_read(SC2336_REG_SENSOR_ID_L, &pid_l);
    uint16_t PID = (pid_h << 8) | pid_l;
    if (SC2336_PID == PID) {
        ESP_LOGI(TAG, "sc2336 init");
    } else {
        ESP_LOGW(TAG, "Mismatch PID=0x%x", PID);
    }

    if (hor_res == 1920 && ver_res == 1080) {
        ESP_RETURN_ON_ERROR(sc2336_write_array(init_reglist_MIPI_2lane_1080p_raw8_30fps), TAG, "Write array failed for res (1920x1080)");
    } else if (hor_res == 1280 && ver_res == 720) {
        ESP_RETURN_ON_ERROR(sc2336_write_array(init_reglist_MIPI_2lane_720p_raw8_30fps), TAG, "Write array failed for res (1280x720)");
    } else if (hor_res == 800 && ver_res == 800) {
        ESP_RETURN_ON_ERROR(sc2336_write_array(init_reglist_MIPI_2lane_8bit_800x800_30fps), TAG, "Write array failed for res (800x800)");
    } else {
        ESP_LOGE(TAG, "Unsupported resolution: %dx%d", hor_res, ver_res);
        return ESP_ERR_NOT_SUPPORTED;
    }

    // set_colorbar(1);
    return ESP_OK;
}
