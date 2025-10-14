/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_check.h"
#include "bsp/esp-bsp.h"
#include "sensor.h"
#include "esp_rom_sys.h"

#define I2C_MASTER_TIMEOUT_MS       (1000)
#define OV_ADDR                     (0x36)

static const char *TAG = "sensor.ov5647";

typedef struct {
    uint16_t reg;
    uint8_t val;
} reginfo_t;

#define SEQUENCE_END         0xFFFF
#define BIT(nr)              (1UL << (nr))

#define MIPI_IDI_CLOCK_RATE     (65000000)

static reginfo_t ov5647_720p_init_data[] = {
    {0x3034, 0x18},
    {0x3035, 0x21},
    {0x3036, MIPI_IDI_CLOCK_RATE / 1000000},
    //[4]=0 PLL root divider /1, [3:0]=5 PLL pre-divider /1.5
    {0x3037, 0x05},
    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3821, 0x07},
    {0x3820, 0x41},
    {0x3827, 0xec},
    {0x370c, 0x0f},
    {0x3612, 0x59},
    {0x3618, 0x00},
    {0x5000, 0x06},
    {0x5002, 0x41},
    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x80},
    {0x3b07, 0x0c},
    //HTS line exposure time in # of pixels
    {0x380c, (1896 >> 8) & 0x1F},
    {0x380d, 1896 & 0xFF},
    //VTS frame exposure time in # lines
    {0x380e, (984 >> 8) & 0xFF},
    {0x380f, 984 & 0xFF},
    {0x3814, 0x31},
    {0x3815, 0x31},
    {0x3708, 0x64},
    {0x3709, 0x52},

    // resolution
    {0x3808, (1280 >> 8) & 0x0F},
    //[7:0] Output horizontal width low byte
    {0x3809, 1280 & 0xFF},
    //[2:0] Output vertical height high byte
    {0x380a, (720 >> 8) & 0x7F},
    //[7:0] Output vertical height low byte
    {0x380b, 720 & 0xFF},

    //[3:0]=0 X address start high byte
    {0x3800, (0 >> 8) & 0x0F},
    //[7:0]=0 X address start low byte
    {0x3801, 0 & 0xFF},
    //[2:0]=0 Y address start high byte
    {0x3802, (8 >> 8) & 0x07},
    //[7:0]=0 Y address start low byte
    {0x3803, 8 & 0xFF},
    //[3:0] X address end high byte
    {0x3804, ((2624 - 1) >> 8) & 0x0F},
    //[7:0] X address end low byte
    {0x3805, (2624 - 1) & 0xFF},
    //[2:0] Y address end high byte
    {0x3806, ((1954 - 1) >> 8) & 0x07},
    //[7:0] Y address end low byte
    {0x3807, (1954 - 1) & 0xFF},
    //[3:0]=0 timing hoffset high byte
    {0x3810, (8 >> 8) & 0x0F},
    //[7:0]=0 timing hoffset low byte
    {0x3811, 8 & 0xFF},
    //[2:0]=0 timing voffset high byte
    {0x3812, (0 >> 8) & 0x07},
    //[7:0]=0 timing voffset low byte
    {0x3813, 0 & 0xFF},

#define GAIN_MAN  (64775)
    {0x350a, (GAIN_MAN >> 8) & 0xff}, //AGC
    {0x350b, (GAIN_MAN >> 0) & 0xff},

#define EXPOSURE_MAN (0x0fffff)
    {0x3500, ((EXPOSURE_MAN >> 16) & 0xff)},
    {0x3501, ((EXPOSURE_MAN >> 8) & 0xff)},
    {0x3502, ((EXPOSURE_MAN >> 0) & 0xff)},

#define MIN_EXPORE (0x01)
#define MAX_EXPORT (0x100)
    {0x3a01, MIN_EXPORE},
    {0x3a02, (MAX_EXPORT >> 8)},
    {0x3a03, MAX_EXPORT & 0xff},
    // aec auto step
    {0x3a05, 0x20 | 0x10},

// WPT2 > WPT > BPT > BPT2
#define REG_WPT   (0x3A0F)
#define REG_BPT   (0x3A10)
#define REG_WPT2  (0x3A1B)
#define REG_BPT2  (0x3A1E)
    {0X3A05, 0X22}, //step_auto_en
    {REG_WPT2, 0xA0},
    {REG_WPT,  0x8E},
    {REG_BPT,  0x60},
    {REG_BPT2, 0x50},
    {0x3A11, 0xf9},
    {0x3A1F, 0xc0},
    {0x3a14, 0x00},// gain celling
    {0x3a15, 0x40},
    {0x3a18, 0x00},// gain celling
    {0x3a19, 0x22},

#define AG_MAN_EN (1) //1
#define AE_MAN_EN (0) //0
#define MENU_AG_AE ((AG_MAN_EN<<1)|AE_MAN_EN)
    {0x3503, (0x3 << 5) | MENU_AG_AE}, //menu AEC,AGC

    // {0x3a00, 0x78 | (0<<2)}, // Night mode disable

// auto AGC　模式配置 aec gain
#if AG_MAN_EN == 0
    {0x3a18, 0x00},
    {0x3a19, 0x00}, // 148
#endif

    {0x5000, 0xFF}, //LENC en
    {0x583e, 0xff}, // max gain
    {0x583f, 0x80}, // min gain
    {0x5840, 0xff}, // min gain

    {0x4000, 0x00}, //  blc
    {0x4050, 0xff}, //  blc
    {0x4051, 0x0F}, //  blc
    {0x4001, 0x02},
    {0x4004, 0x04},
    {0x4007, (0x0 << 3) | 0x0},

    // {0x3630, 0x2e},
    // {0x3632, 0xe2},
    // {0x3633, 0x23},
    // {0x3634, 0x44},
    // {0x3636, 0x06},
    // {0x3620, 0x64},
    // {0x3621, 0xe0},
    // {0x3600, 0x37},
    // {0x3704, 0xa0},
    // {0x3703, 0x5a},
    // {0x3715, 0x78},
    // {0x3717, 0x01},
    // {0x3731, 0x02},
    // {0x370b, 0x60},
    // {0x3705, 0x1a},
    // {0x3f05, 0x02},
    // {0x3f06, 0x10},
    // {0x3f01, 0x0a},
    // {0x3a08, 0x01},
    // {0x3a09, 0x27},
    // {0x3a0a, 0x00},
    // {0x3a0b, 0xf6},
    // {0x3a0d, 0x04},
    // {0x3a0e, 0x03},
    // {0x3a0f, 0x58},
    // {0x3a10, 0x50},
    // {0x3a1b, 0x58},
    // {0x3a1e, 0x50},
    // {0x3a11, 0x60},
    // {0x3a1f, 0x28},
    // {0x4001, 0x02},
    // {0x4004, 0x02},
    //little MIPI shit: global timing unit, period of PCLK in ns * 2(depends on # of lanes)
    {0x4837, (1000000000 / MIPI_IDI_CLOCK_RATE) * 2}, // 1/40M*2
    {0x4000, 0x09},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
#if (TEST_CSI_PATTERN)
    {0x503D, 0xA0},
#endif
    {SEQUENCE_END, 0x00}
};

static reginfo_t ov5647_1080p_init_data[] = {
    {0x0100, 0x00},
    {0x0103, 0x01},
    // {0x3034, 0x18},
    {0x3035, 0x41},
    // {0x3036, ((MIPI_IDI_CLOCK_RATE * 8 * 4 * 2) / 25000000)},

    {0x303c, 0x11},
    {0x3106, 0xf5},
    {0x3820, 0x40},//flip, binning
    {0x3821, 0x07},//mirror, binning
    {0x3822, 0x11},
    {0x3827, 0xec},
    {0x370c, 0x0f},
    {0x3612, 0x59},// 0x5b?
    {0x3618, 0x00},// 0x04?
    {0x5000, 0x06},

    {0x5025, 0x01},
    {0x5001, 0x01}, // AWB
    // {0x5002, 0x41},
#define GAIN_MAN_EN (1)
    {0X5180, GAIN_MAN_EN << 3}, // gain_man_en
    {0x5181, 0x10}, // awb delta (default: 0x20)

#define rg  (0x590-20)// + 200)
#define gg  (0x530-160)// + 100)
#define bg  (0x5fc-10)// + 200)
    {0X5186, (rg >> 8) & 0xff},
    {0X5187, (rg >> 0) & 0xff},
    {0X5188, (gg >> 8) & 0xff},
    {0X5189, (gg >> 0) & 0xff},
    {0X518a, (bg >> 8) & 0xff},
    {0X518b, (bg >> 0) & 0xff},

    {0x5003, 0x08},
    {0x5a00, 0x08},
    {0x3000, 0x00},
    {0x3001, 0x00},
    {0x3002, 0x00},
    {0x3016, 0x08},
    {0x3017, 0xe0},
    {0x3018, 0x44},
    {0x301c, 0xf8},
    {0x301d, 0xf0},
    {0x3a18, 0x00},
    {0x3a19, 0xf8},
    {0x3c01, 0x80},
    {0x3b07, 0x08}, // 0x0c?

#define HSTA (512)
#define VSTA (248)
#define HWIN (2110-1)
#define VWIN (1448-1)
#define HEND (HSTA + HWIN)
#define VEND (VSTA + VWIN)

#define THS  (HWIN+285)
#define TVS  (VWIN+285)

    // h-start
    {0x3800, (HSTA >> 8)},
    {0x3801, (HSTA & 0xff)},
    // v-start
    {0x3802, (VSTA >> 8)},
    {0x3803, (VSTA & 0xff)},

    // h-end
    {0x3804, (HEND >> 8)},
    {0x3805, (HEND & 0xff)},
    // v-end
    {0x3806, (VEND >> 8)},
    {0x3807, (VEND & 0xff)},

    // // resolution
    // {0x3808, (1920 >> 8) & 0x0F},
    // //[7:0] Output horizontal width low byte
    // {0x3809, 1920 & 0xFF},
    // //[2:0] Output vertical height high byte
    // {0x380a, (1080 >> 8) & 0x7F},
    // //[7:0] Output vertical height low byte
    // {0x380b, 1080 & 0xFF},

    // total h size
    {0x380c, (THS >> 8)},
    {0x380d, (THS & 0xff)},
    // total v size
    {0x380e, (TVS >> 8)},
    {0x380f, (TVS & 0xff)},

    // {0x3503, 0x10},
    {0x350c, 0x00},
    {0x350d, 0xff},

    // isp window
    // {0x3810, 0x00},
    // {0x3811, 0x02},
    // {0x3812, 0x00},
    // {0x3813, 0x02},

    // h inc
    {0x3814, 0x11},
    // y inc
    {0x3815, 0x11},

#define GAIN_MAN  (0x3ff)
    {0x350a, (GAIN_MAN >> 8) & 0xff}, //AGC
    {0x350b, (GAIN_MAN >> 0) & 0xff},

#define EXPOSURE_MAN (0x0fffff)
    {0x3500, ((EXPOSURE_MAN >> 16) & 0xff)},
    {0x3501, ((EXPOSURE_MAN >> 8) & 0xff)},
    {0x3502, ((EXPOSURE_MAN >> 0) & 0xff)},

#define MIN_EXPORE (0xff)
#define MAX_EXPORT (0xffff)
    {0x3a01, MIN_EXPORE},
    {0x3a02, (MAX_EXPORT >> 8)},
    {0x3a03, MAX_EXPORT & 0xff},
    // aec auto step
    {0x3a05, 0x20 | 0x10},

// WPT2 > WPT > BPT > BPT2
#define REG_WPT   (0x3A0F)
#define REG_BPT   (0x3A10)
#define REG_WPT2  (0x3A1B)
#define REG_BPT2  (0x3A1E)
    {0X3A05, 0X22}, //step_auto_en
    {REG_WPT2, 0xA0},
    {REG_WPT,  0x8E},
    {REG_BPT,  0x60},
    {REG_BPT2, 0x50},
    {0x3A11, 0xf9},
    {0x3A1F, 0xc0},
    {0x3a14, 0xff},// gain celling
    {0x3a15, 0xff},
    {0x3a18, 0x03},// gain celling
    {0x3a19, 0xff},

#define AG_MAN_EN (0) //1
#define AE_MAN_EN (1) //0
#define MENU_AG_AE ((AG_MAN_EN<<1)|AE_MAN_EN)
    {0x3503, (0x3 << 5) | MENU_AG_AE}, //menu AEC,AGC

    // {0x3a00, 0x78 | (0<<2)}, // Night mode disable

// auto AGC　模式配置 aec gain
#if AG_MAN_EN == 0
    {0x3a18, 0x00},
    {0x3a19, 0xff}, // 148
#endif

    {0x5000, 0xFF}, //LENC en
    {0x583e, 0xff}, // max gain
    {0x583f, 0x80}, // min gain
    {0x5840, 0xff}, // min gain

    {0x4000, 0x00}, //  blc
    {0x4050, 0xff}, //  blc
    {0x4051, 0x0F}, //  blc
    {0x4001, 0x02},
    {0x4004, 0x04},
    {0x4007, (0x0 << 3) | 0x0},

    {0x3630, 0x2e},
    {0x3632, 0xe2},
    {0x3633, 0x23},
    {0x3634, 0x44},
    {0x3636, 0x06},
    {0x3620, 0x64},
    {0x3621, 0xe0},
    {0x3600, 0x37},
    {0x3704, 0xa0},
    {0x3703, 0x5a},
    {0x3715, 0x78},
    {0x3717, 0x01},
    {0x3731, 0x02},
    {0x370b, 0x60},
    {0x3705, 0x1a},
    {0x3f05, 0x02},
    {0x3f06, 0x10},
    {0x3f01, 0x0a},
    {0x3a08, 0x01},
    {0x3a09, 0x27},
    {0x3a0a, 0x00},
    {0x3a0b, 0xf6},

    {0x3a0d, 0x04},
    {0x3a0e, 0x03},
    {0x3a0f, 0x58},
    {0x3a10, 0x50},
    {0x3a1b, 0x58},
    {0x3a1e, 0x50},
    {0x3a11, 0x60},
    {0x3a1f, 0x28},

    {0x4000, 0x00},
    {0x4001, 0x02},
    {0x4004, 0x04},
    {0x4050, 0x6e},
    {0x4051, 0x8f},
    {0x5903, 0xff},

    // {0x503f, 0xaa},
    // {0x3009, 0xff},
    // {0x3008, 0xff},
    // {0x470a, 0xff},
    // {0x470b, 0xff},

    // {0x503D, 0x93},
    // {0x480a, 0x06},
    // {0x503D, 0x93},
#if (TEST_CSI_PATTERN)
    {0x503D, 0xA1},
#endif
    // {0x0100, 0x01},
    {SEQUENCE_END, 0x00}
};

void sccb_init(int rate, int io_scl, int io_sda);
void sccb_write_reg16(uint8_t addr, uint16_t reg, int len, uint8_t data);
void sccb_read_reg16(uint8_t addr, uint16_t reg, int len, uint8_t* data);

#if USE_NEW_I2C_DRIVER
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;
#endif

esp_err_t ov5647_read(uint16_t addr, uint8_t *read_buf)
{
    uint8_t addr_buf[2] = {addr >> 8, addr & 0xff};
#if USE_NEW_I2C_DRIVER
    return i2c_master_transmit_receive(dev_handle, addr_buf, sizeof(addr_buf), read_buf, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#else
    return i2c_master_write_read_device(BSP_I2C_NUM, OV_ADDR, addr_buf, sizeof(addr_buf), read_buf, 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#endif
}

esp_err_t ov5647_write(uint16_t addr, uint8_t data)
{
    uint8_t write_buf[3] = {addr >> 8, addr & 0xff, data};
#if USE_NEW_I2C_DRIVER
    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#else
    return i2c_master_write_to_device(BSP_I2C_NUM, OV_ADDR, write_buf, sizeof(write_buf), pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
#endif
}

/* write a array of registers  */
static esp_err_t ov5647_write_array(reginfo_t *regarray)
{
    int i = 0;
    while (regarray[i].reg != SEQUENCE_END) {
        ESP_RETURN_ON_ERROR(ov5647_write(regarray[i].reg, regarray[i].val), TAG, "Write register failed");
        i++;
    }

    return ESP_OK;
}

static void ov5647_s_autogain(uint32_t val)
{
    uint8_t reg;
    ov5647_read(0x3503, &reg);
    ov5647_write(0x3503, val ? (reg & (~0x2)) : (reg | 0x2));
}

static void ov5647_s_exposure_auto(int val)
{
    uint8_t reg;
    ov5647_read(0x3503, &reg);
    ov5647_write(0x3503, val == -1 ? (reg | 0x1) : (reg & (~0x1)));
}

void ov5647_s_exposure(uint32_t val)
{
    ov5647_write(0x3500, (val >> 12) & 0xf);
    ov5647_write(0x3501, (val >> 4) & 0xff);
    ov5647_write(0x3502, (val & 0xf) << 4);
}

void ov5647_s_gain(uint32_t val)
{
    ov5647_write(0x350a, (val >> 8) & 0xf);
    ov5647_write(0x350b, val & 0xff);

    // ov5647_write(0x3a18, val >> 8);
    // ov5647_write(0x3a19, val & 0xff);
}

uint8_t read_gain(void)
{
    uint8_t reg = 0;
    ov5647_read(0x5693, &reg);
    printf("gain: %x\n", reg);
    return reg;
}

static esp_err_t ov5647_write_config(uint16_t hor_res, uint16_t ver_res, isp_color_t color_mode,
                                     uint32_t clock_rate)
{
    uint8_t reg_val_1 = ((uint64_t)clock_rate) * 8 * 4 * 2 / 25000000ULL;
    uint8_t reg_val_2 = 1000000000ULL * 2 / clock_rate;

    ESP_LOGI(TAG, "0x3036, 0x4837: %d, %d", reg_val_1, reg_val_2);

    ESP_RETURN_ON_ERROR(ov5647_write(0x3034, 0x18), TAG,
                        "Write color mode failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x3036, reg_val_1), TAG,
                        "Write color mode failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x3808, (hor_res >> 8) & 0x0F), TAG, "Write hor_res high bits failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x3809, hor_res & 0xFF), TAG, "Write hor_res low bits failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x380a, (ver_res >> 8) & 0x0F), TAG, "Write hor_res high bits failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x380b, ver_res & 0xFF), TAG, "Write hor_res low bits failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x4837, reg_val_2), TAG, "Write hor_res low bits failed");

    return ESP_OK;
}

static void delay_us(uint32_t t)
{
    esp_rom_delay_us(t);
}

esp_err_t sensor_ov5647_init(uint16_t hor_res, uint16_t ver_res, isp_color_t color_mode, uint32_t clock_rate)
{
    // sccb_init(100000, 34, 31);

    bsp_i2c_init(); // init i2c if not already initialized
#if USE_NEW_I2C_DRIVER
    i2c_bus_handle = bsp_get_i2c_bus_handle();
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = OV_ADDR,
        .scl_speed_hz = 400000,
    };
    i2c_master_bus_add_device(i2c_bus_handle, &dev_cfg, &dev_handle);
#endif
    uint8_t sensor_id[2] = {0, 0};
    int sensor_ready = 0;

    printf("ADDR: 0x%x\n", OV_ADDR);

    for (int i = 0; i < 10; i++) {
        ov5647_write(0x0100, 0x00);
        ov5647_write(0x0103, 0x01);
        delay_us(5000);
        ov5647_read(0x300a, &sensor_id[0]);
        ov5647_read(0x300b, &sensor_id[1]);
        printf("read: id, %x %x\r\n", sensor_id[0], sensor_id[1]);
        if (sensor_id[0] != 0x56 || sensor_id[1] != 0x47) {
            continue;
        }
        sensor_ready = 1;
        break;
    }
    if (sensor_ready == 0) {
        printf("Read sensor ID fail\n");
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(ov5647_write(0x0100, 0x00), TAG, "Write register failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x0103, 0x01), TAG, "Write register failed");
    delay_us(5000);
    // Ensure streaming off to make clock lane go into LP-11 state.
    ESP_RETURN_ON_ERROR(ov5647_write(0x4800, BIT(0)), TAG, "Write register failed");

    if (ver_res <= 960) {
        ESP_RETURN_ON_ERROR(ov5647_write_array(ov5647_720p_init_data), TAG, "Write array failed");
    } else {
        ESP_RETURN_ON_ERROR(ov5647_write_array(ov5647_1080p_init_data), TAG, "Write array failed");
    }

    ESP_RETURN_ON_ERROR(ov5647_write_config(hor_res, ver_res, color_mode, clock_rate), TAG, "Write config failed");

    ESP_RETURN_ON_ERROR(ov5647_write(0x4800, 0x14), TAG, "Write register failed");
    // ESP_RETURN_ON_ERROR(ov5647_write(0x4800, TEST_CSI_LINESYNC ? 0x14 : 0x04), TAG, "Write register failed");
#if TEST_CSI_AE
    ESP_RETURN_ON_ERROR(ov5647_write(0x3503, 0x3), TAG, "Write register failed");
#endif
#if 1
    // AF DW5714 XSD <-> set OV5647 GPIO0 to high
    ESP_RETURN_ON_ERROR(ov5647_write(0x3002, 0x01), TAG, "Write register failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x3010, 0x01), TAG, "Write register failed");
    ESP_RETURN_ON_ERROR(ov5647_write(0x300D, 0x01), TAG, "Write register failed");
    vTaskDelay(pdMS_TO_TICKS(10));
#endif
    printf("start camera\n");
    // ov5647_set_piex_arry_size(2590, 1940);
    // ov5647_set_output_size(128, 96, 0, 0);

    // ov5647_write_array(res_416x240_data);

    // ov5647_write(0x3814, 0x13);
    // ov5647_write(0x3815, 0x35);
    ESP_RETURN_ON_ERROR(ov5647_write(0x0100, 0x01), TAG, "Write register failed");
    uint8_t v = 0;
    ov5647_read(0x5005, &v);
    printf("IO: %x\n", v);

    // ov5647_s_exposure_auto(0);
    // ov5647_s_exposure(1000);
    return ESP_OK;
}

#include "driver/gpio.h"

// extern void ets_delay_us(uint32_t us);

static int scl_io = 0;
static int sda_io = 0;

void sccb_init(int rate, int io_scl, int io_sda)
{
    scl_io = io_scl;
    sda_io = io_sda;
    printf("init SCCB: SCL: %d, SDA: %d\n", scl_io, sda_io);
#if 0

    lpgpio_ll_attach_to(io_scl, 0);
    PIN_FUNC_SELECT(IOMUX_REG(io_scl), PIN_FUNC_GPIO);
    gpio_ll_output_enable(&GPIO, io_scl);
    lpgpio_ll_attach_to(io_sda, 0);
    PIN_FUNC_SELECT(IOMUX_REG(io_sda), PIN_FUNC_GPIO);
    gpio_ll_output_enable(&GPIO, io_sda);
    gpio_ll_pullup_en(&GPIO, io_scl);
    gpio_ll_pullup_en(&GPIO, io_sda);
#endif
    // gpio_set_direction(io_scl, GPIO_MODE_OUTPUT);
    // gpio_set_direction(io_sda, GPIO_MODE_OUTPUT);
    // gpio_set_pull_mode(io_scl, GPIO_PULLUP_ONLY);
    // gpio_set_pull_mode(io_sda, GPIO_PULLUP_ONLY);
    gpio_set_direction(io_scl, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(io_sda, GPIO_MODE_OUTPUT_OD);
}

void sccb_start(void)
{
    gpio_set_direction(sda_io, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(scl_io, GPIO_MODE_OUTPUT_OD);

    gpio_set_level(scl_io, 1);
    gpio_set_level(sda_io, 1);
    delay_us(10);
    gpio_set_level(sda_io, 0);
    delay_us(4);
    gpio_set_level(scl_io, 0);
    delay_us(4);
}

void sccb_stop(void)
{
    gpio_set_direction(sda_io, GPIO_MODE_OUTPUT_OD);
    gpio_set_direction(scl_io, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(scl_io, 0);
    gpio_set_level(sda_io, 0);
    delay_us(5);
    gpio_set_level(scl_io, 1);
    delay_us(3);
    gpio_set_level(sda_io, 1);
    delay_us(5);
}

void sccb_write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        gpio_set_level(sda_io, (byte & (0x80 >> i)) > 0);
        delay_us(2);
        gpio_set_level(scl_io, 1);
        delay_us(2);
        gpio_set_level(scl_io, 0);
        delay_us(2);
    }
    delay_us(2);
    gpio_set_level(sda_io, 1);
    gpio_set_level(scl_io, 1);
    delay_us(2);
    gpio_set_level(scl_io, 0);
    delay_us(2);
}

void sccb_read_byte(uint8_t* byte)
{
    uint8_t val = 0;
    // gpio_set_level(sda_io, 1);
    // gpio_set_level(scl_io, 1);
    // gpio_ll_output_disable(&GPIO, sda_io);
    // gpio_ll_input_enable(&GPIO, sda_io);

    gpio_set_direction(sda_io, GPIO_MODE_INPUT);

    delay_us(2);
    for (int i = 0; i < 8; i++) {
        val = val << 1;
        gpio_set_level(scl_io, 1);
        delay_us(2);
        val |= gpio_get_level(sda_io);
        delay_us(2);
        gpio_set_level(scl_io, 0);
        delay_us(2);
    }
    delay_us(2);
    gpio_set_level(sda_io, 1);
    // gpio_ll_output_enable(&GPIO, sda_io);
    // gpio_ll_input_disable(&GPIO, sda_io);
    gpio_set_direction(sda_io, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(scl_io, 1);
    delay_us(2);
    gpio_set_level(scl_io, 0);
    delay_us(2);
    *byte = val;
}

void sccb_write_reg16(uint8_t addr, uint16_t reg, int len, uint8_t data)
{
    sccb_start();
    sccb_write_byte(addr << 1);
    sccb_write_byte(0xff & (reg >> 8));
    sccb_write_byte(reg & 0xff);
    sccb_write_byte(data);
    sccb_stop();
}

void sccb_read_reg16(uint8_t addr, uint16_t reg, int len, uint8_t* data)
{
    sccb_start();
    sccb_write_byte((addr << 1));
    sccb_write_byte(0xff & (reg >> 8));
    sccb_write_byte(reg & 0xff);
    sccb_stop();
    sccb_start();
    sccb_write_byte((addr << 1) | 1);
    sccb_read_byte(data);
    sccb_stop();
}
