/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#if CONFIG_USE_ESP_VIDEO_IF
#define USE_ESP_VIDEO_IF 1
#endif

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "esp_dma_utils.h"

#include "bsp/esp32_p4_function_ev_board.h"
#if !USE_ESP_VIDEO_IF
/* Old camera API path - only available with local component override */
/* Define camera clock rate constants if not using esp_video_if */
#ifndef OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS
#define OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS   (74000000ULL)
#define OV5647_MIPI_CSI_LINE_RATE_720P_50FPS   (OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS * 4)
#define OV5647_MIPI_IDI_CLOCK_RATE_1080P_22FPS  (98437500ULL)
#define OV5647_MIPI_CSI_LINE_RATE_1080P_22FPS  (OV5647_MIPI_IDI_CLOCK_RATE_1080P_22FPS * 4)
#endif
/* Note: bsp/camera.h is not available in component manager version */
/* This path requires local component override */
#include "bsp/camera.h"
#endif
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"

#include "webrtc_mem_utils.h"
#include "esp_h264_alloc.h"
#if USE_ESP_VIDEO_IF
#include "esp_video_if.h"
#endif

static const char *TAG = "esp32p4_frame_grabber";
#define H264_ENCODE     1
// #define SDCARD_SAVE     1

#if H264_ENCODE
#include "esp_h264_hw_enc.h"
#include "esp_h264_alloc.h"
#include "esp_cache.h"
#endif

#if SDCARD_SAVE
static FILE* f264 = NULL;
#endif

static volatile int frames_received_cnt = 0;

#ifndef USE_ESP_VIDEO_IF
static esp_h264_out_buf_t h264_out_data;
static uint8_t *jpeg_last_buf;
static uint8_t *jpeg_next_buf;
static uint8_t *camera_next_buf;
static uint8_t *camera_last_buf;

static bool camera_trans_done(void)
{
    jpeg_next_buf = camera_last_buf;

    bsp_camera_set_frame_buffer(camera_next_buf);
    camera_last_buf = camera_next_buf;

    frames_received_cnt++; // Update the frames received count
    return true;
}

static esp_err_t camera_init(void)
{
    size_t camera_fb_size = 0;
    bsp_camera_config_t camera_cfg = {
        .hor_res = WIDTH,
        .ver_res = HEIGHT,
        .fb_size_ptr = &camera_fb_size,
        .num_fbs = 3,
        .input_data_color_type = ISP_COLOR_RAW8,
        .output_data_color_type = ISP_COLOR_YUV420,
#if WIDTH > 1280
        .clock_rate_hz = OV5647_MIPI_IDI_CLOCK_RATE_1080P_22FPS,
        .csi_lane_rate_mbps = OV5647_MIPI_CSI_LINE_RATE_1080P_22FPS / 1000 / 1000,
#else
        .clock_rate_hz = OV5647_MIPI_IDI_CLOCK_RATE_720P_50FPS,
        .csi_lane_rate_mbps = OV5647_MIPI_CSI_LINE_RATE_720P_50FPS / 1000 / 1000,
#endif
        .flags = {
            .use_external_fb = 0,
        },
    };
    const isp_config_t isp_cfg = ISP_CONFIG_DEFAULT(camera_cfg.hor_res, camera_cfg.ver_res,
                                                    ISP_COLOR_RAW8, ISP_COLOR_YUV420);
    if(bsp_camera_new(&camera_cfg, &isp_cfg, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "camera initialization failed");
        webrtc_mem_utils_print_stats(TAG);
        return ESP_FAIL;
    }
    ESP_ERROR_CHECK(bsp_camera_register_trans_done_callback(camera_trans_done));
    ESP_ERROR_CHECK(bsp_camera_get_frame_buffer(3, (void **) &camera_last_buf,
                                                (void **) &camera_next_buf, (void **) &jpeg_last_buf));
    jpeg_next_buf = jpeg_last_buf;

    return ESP_OK;
}

#if H264_ENCODE

// Data read callback to read raw data
static void data_read_callback(void *ctx, esp_h264_buf_t *in_data)
{
    if (jpeg_next_buf != jpeg_last_buf) {
        /* New frame not available */
        camera_next_buf = jpeg_last_buf;
        jpeg_last_buf = jpeg_next_buf;
        in_data->buffer = NULL;
    } else {
        in_data->buffer = jpeg_next_buf;
    }
}

// Data write callback to output encoded frames
static void data_write_callback(void *ctx, esp_h264_out_buf_t *out_data)
{
    h264_out_data.buffer = out_data->buffer;
    h264_out_data.len = out_data->len;
    h264_out_data.type = out_data->type;

    frames_received_cnt++;

#if SDCARD_SAVE
    if (f264) {
        esp_cache_msync(out_data->buffer, out_data->len, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        int wr_len = fwrite(out_data->buffer, 1, out_data->len, f264);
        if (wr_len != out_data->len) {
            ESP_LOGW(TAG, "expected wr: %" PRIu32 " actual: %d", out_data->len, wr_len);
        }
    }
#endif
}
#endif
#endif

static void init_chip()
{
    asm volatile("li t0, 0x2000\n"
                 "csrrs t0, mstatus, t0\n"); /* FPU_state = 1 (initial) */
    asm volatile("li t0, 0x1\n"
                 "csrrs t0, 0x7F1, t0\n"); /* HWLP_state = 1 (initial) */
    asm volatile("li t0, 0x1\n"
                 "csrrs t0, 0x7F2, t0\n"); /* AIA_state = 1 (initial) */
}

void init_clock(void)
{
#include "soc/hp_sys_clkrst_reg.h"

    uint32_t rd;

    REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL02_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_CLK_SRC_SEL, 1);
    REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_CSI_DPHY_CLK_SRC_SEL, 1);
    REG_CLR_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_CFG_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_CFG_CLK_EN);
    REG_CLR_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_PLL_REFCLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_PLL_REFCLK_EN);
    REG_CLR_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_CSI_DPHY_CFG_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_CSI_DPHY_CFG_CLK_EN);
    REG_CLR_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_PLL_REFCLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPHY_PLL_REFCLK_EN);

    REG_CLR_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_DSI_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_DSI_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_DSI_BRG);
    REG_CLR_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_DSI_BRG);

    REG_CLR_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_CSI_HOST_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_CSI_HOST_SYS_CLK_EN);
    REG_CLR_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_CSI_BRG_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_CSI_BRG_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_CSI_HOST);
    REG_CLR_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_CSI_HOST);
    REG_SET_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_CSI_BRG);
    REG_CLR_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_CSI_BRG);

    // REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPICLK_DIV_NUM, (480000000 / MIPI_DPI_CLOCK_RATE) - 1);
    REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPICLK_SRC_SEL, 1);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL03_REG, HP_SYS_CLKRST_REG_MIPI_DSI_DPICLK_EN);

    REG_CLR_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_GDMA_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, HP_SYS_CLKRST_REG_GDMA_SYS_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_GDMA);
    REG_CLR_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_GDMA);

    REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL26_REG, HP_SYS_CLKRST_REG_ISP_CLK_DIV_NUM, 1 - 1);
    REG_SET_FIELD(HP_SYS_CLKRST_PERI_CLK_CTRL25_REG, HP_SYS_CLKRST_REG_ISP_CLK_SRC_SEL, 1);
    REG_CLR_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL25_REG, HP_SYS_CLKRST_REG_ISP_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_PERI_CLK_CTRL25_REG, HP_SYS_CLKRST_REG_ISP_CLK_EN);
    REG_SET_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_ISP);
    REG_CLR_BIT(HP_SYS_CLKRST_HP_RST_EN0_REG, HP_SYS_CLKRST_REG_RST_EN_ISP);

    rd = REG_READ(HP_SYS_CLKRST_REF_CLK_CTRL1_REG);
    REG_WRITE(HP_SYS_CLKRST_REF_CLK_CTRL1_REG, rd | HP_SYS_CLKRST_REG_REF_240M_CLK_EN);

    rd = REG_READ(HP_SYS_CLKRST_REF_CLK_CTRL2_REG);
    REG_WRITE(HP_SYS_CLKRST_REF_CLK_CTRL2_REG, rd | HP_SYS_CLKRST_REG_REF_160M_CLK_EN);

    rd = REG_READ(HP_SYS_CLKRST_HP_RST_EN2_REG);
    REG_WRITE(HP_SYS_CLKRST_HP_RST_EN2_REG, rd & (~HP_SYS_CLKRST_REG_RST_EN_H264));

    rd = REG_READ(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG);
    rd = rd | HP_SYS_CLKRST_REG_H264_SYS_CLK_EN | HP_SYS_CLKRST_REG_GPSPI3_SYS_CLK_EN | HP_SYS_CLKRST_REG_AXI_PDMA_SYS_CLK_EN | HP_SYS_CLKRST_REG_GDMA_SYS_CLK_EN;
    rd = rd | HP_SYS_CLKRST_REG_CSI_HOST_SYS_CLK_EN | HP_SYS_CLKRST_REG_CSI_BRG_SYS_CLK_EN;
    REG_WRITE(HP_SYS_CLKRST_SOC_CLK_CTRL1_REG, rd);
    // H264_DMA
    rd = REG_READ(HP_SYS_CLKRST_PERI_CLK_CTRL26_REG);
    rd |= HP_SYS_CLKRST_REG_H264_CLK_EN;
    rd |= HP_SYS_CLKRST_REG_H264_CLK_SRC_SEL;
    REG_WRITE(HP_SYS_CLKRST_PERI_CLK_CTRL26_REG, rd);
}

void esp32p4_frame_grabber_cleanup(void)
{
#if SDCARD_SAVE
    if (f264) {
        fflush(f264);
        fclose(f264);
    }
#endif
}

typedef struct {
    QueueHandle_t frame_queue;
    TaskHandle_t encoder_task_handle;
    StaticTask_t *task_buffer;
    void *task_stack;
    bool encoder_initialized;
    bool running;
    SemaphoreHandle_t run_semaphore;
} esp32p4_encoder_data_t;

static esp32p4_encoder_data_t s_p4_enc_data = {0};

#define QUEUE_RECEIVE_WAIT_MS  CONFIG_VIDEO_QUEUE_RECEIVE_WAIT_MS
#define QUEUE_SEND_WAIT_MS     CONFIG_VIDEO_QUEUE_SEND_WAIT_MS

esp_h264_out_buf_t *esp32p4_grab_one_frame()
{
    esp_h264_out_buf_t *frame_data = heap_caps_calloc(1, sizeof(esp_h264_out_buf_t), MALLOC_CAP_SPIRAM);
    if (xQueueReceive(s_p4_enc_data.frame_queue, frame_data, pdMS_TO_TICKS(QUEUE_RECEIVE_WAIT_MS)) != pdTRUE) {
        heap_caps_free(frame_data);
        return NULL;
    }
    return frame_data;
}

extern void *get_buffer();
extern esp_err_t esp_h264_hw_enc_set_reset_request();
static void video_encoder_task(void *arg)
{
    ESP_LOGD(TAG, "Video encoder task started (singleton mode - runs continuously)");

    while (1) {
        // Check if encoding should be running
        if (!s_p4_enc_data.running) {
            ESP_LOGD(TAG, "Video encoder paused, waiting for start signal...");
            // Wait for start signal (blocking)
            xSemaphoreTake(s_p4_enc_data.run_semaphore, portMAX_DELAY);
            ESP_LOGD(TAG, "Video encoder resumed");
        }

#if USE_ESP_VIDEO_IF
        // Get raw frame
        video_fb_t *raw_frame = esp_video_if_get_frame();
        if (!raw_frame) {
            vTaskDelay(pdMS_TO_TICKS(QUEUE_RECEIVE_WAIT_MS));
            continue;
        }

        // Encode the raw frame
        esp_h264_out_buf_t *frame = esp_h264_hw_enc_encode_frame(raw_frame->buf, raw_frame->len);

        // Release the raw frame as we're done with it
        esp_video_if_release_frame(raw_frame);

        // If encoding failed, continue to next frame
        if (!frame) {
            ESP_LOGW(TAG, "Frame encoding failed");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
#else
        static int frames_cnt = 0;
        esp_err_t ret = esp_h264_hw_enc_process_one_frame();
        if (ret != ESP_OK) {
            if (ret != ESP_ERR_NOT_FOUND) {
                ESP_LOGE(TAG, "Encoding failed");
            }
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        frames_cnt++;
        if (frames_cnt % 100 == 0) {
            ESP_LOGI(TAG, "frames_received_cnt %d, curr frame len %" PRIu32, frames_received_cnt, h264_out_data.len);
            print_mem_stats(TAG);
            if (frames_cnt % 250 == 0) {
                // Keep setting the bitrate periodically
                esp_h264_hw_enc_set_reset_request();
            }
        }

        // frame copy
        esp_h264_out_buf_t *frame = calloc(1, sizeof(esp_h264_out_buf_t));
        if (!frame) {
            ESP_LOGE(TAG, "Failed to alloc frame");
        }
        frame->len = h264_out_data.len;
        frame->buffer = heap_caps_aligned_calloc(64, 1, frame->len, MALLOC_CAP_SPIRAM);
        if (!frame->buffer) {
            ESP_LOGE(TAG, "Failed to alloc buffer. size %d", (int) frame->len);
        }
        esp_cache_msync(h264_out_data.buffer, (frame->len + 63) & ~63, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
        memcpy(frame->buffer, h264_out_data.buffer, frame->len);
        frame->type = h264_out_data.type;
#endif
        if (xQueueSend(s_p4_enc_data.frame_queue, frame, pdMS_TO_TICKS(QUEUE_SEND_WAIT_MS)) != pdTRUE) {
            free(frame->buffer);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        free(frame);
    }

    ESP_LOGE(TAG, "Video encoder task unexpectedly exited!");
}

void esp32p4_frame_grabber_init(void)
{
    // Singleton pattern: return if already initialized
    if (s_p4_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "ESP32P4 frame grabber already initialized (singleton)");
        return;
    }

    init_chip();
    init_clock();

    s_p4_enc_data.frame_queue = xQueueCreate(CONFIG_VIDEO_FRAME_QUEUE_SIZE, sizeof(esp_h264_out_buf_t));
    if (!s_p4_enc_data.frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        goto cleanup;
    }

    // Create semaphore for start/stop control
    s_p4_enc_data.run_semaphore = xSemaphoreCreateBinary();
    if (!s_p4_enc_data.run_semaphore) {
        ESP_LOGE(TAG, "Failed to create run semaphore");
        goto cleanup;
    }
#if USE_ESP_VIDEO_IF
    esp_err_t ret = esp_video_if_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize video interface");
        goto cleanup;
    }
    ESP_LOGD(TAG, "video interface initialized");
#else
    if (camera_init() != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed");
        goto cleanup;
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(10));
#if SDCARD_SAVE
    sdmmc_card_t *sdcard = bsp_sdcard_mount();
    if (sdcard == NULL) {
        ESP_LOGE(TAG, "sdcard initialization failed!");
        goto cleanup;
    }

    printf("Filesystem mounted\n");
    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, sdcard);

    char h264_name[100] = "/sdcard/encoded_file.h264"; // CONFIG_BSP_SD_MOUNT_POINT
    // sprintf(h264_name, "/eMMC/res_%d_%d.264", 1920, 1080);
    printf("h264_name %s \n", h264_name);

    f264 = fopen(h264_name, "wb+");
    if (f264 == NULL) {
        printf("H264 file create failed\n");
        esp32p4_frame_grabber_cleanup();
        goto cleanup;
    }
#endif

#if H264_ENCODE
    h264_enc_user_cfg_t cfg = {
        .enc_cfg = DEFAULT_ENCODER_CFG()
    };

    /* Get the ACTUAL camera resolution (which may differ from desired due to fallback) */
#if USE_ESP_VIDEO_IF
    video_resolution_t actual_resolution = {0};
    if (esp_video_if_get_resolution(&actual_resolution) == ESP_OK &&
        actual_resolution.width != 0 && actual_resolution.height != 0) {
        cfg.enc_cfg.res.width = actual_resolution.width;
        cfg.enc_cfg.res.height = actual_resolution.height;
        if (actual_resolution.fps != 0) {
            cfg.enc_cfg.fps = actual_resolution.fps;
        }
        ESP_LOGI(TAG, "Configuring encoder with actual camera resolution: %dx%d@%d",
                 cfg.enc_cfg.res.width, cfg.enc_cfg.res.height, cfg.enc_cfg.fps);
    }

    cfg.enc_cfg.fps = 27; /* used to distribute the bitrate*/
    cfg.enc_cfg.rc.qp_min = 32;
    cfg.enc_cfg.rc.qp_max = 36;
#else
    cfg.read_cb = &data_read_callback,
    cfg.write_cb = &data_write_callback,
#endif

    esp_h264_setup_encoder(&cfg);

#define ENC_TASK_STACK_SIZE     CONFIG_VIDEO_ENCODER_TASK_STACK_SIZE
#define ENC_TASK_PRIO           CONFIG_VIDEO_ENCODER_TASK_PRIORITY
    s_p4_enc_data.task_buffer = heap_caps_calloc(1, sizeof(StaticTask_t), MALLOC_CAP_INTERNAL);
    s_p4_enc_data.task_stack = heap_caps_calloc(ENC_TASK_STACK_SIZE, 1, MALLOC_CAP_SPIRAM);
    if (!s_p4_enc_data.task_buffer || !s_p4_enc_data.task_stack) {
        ESP_LOGE(TAG, "Failed to allocate task buffers");
        goto cleanup;
    }

    s_p4_enc_data.running = false;  // Start in stopped state
    s_p4_enc_data.encoder_task_handle = xTaskCreateStatic(video_encoder_task, "video_encoder", ENC_TASK_STACK_SIZE,
                                                          NULL, ENC_TASK_PRIO, s_p4_enc_data.task_stack, s_p4_enc_data.task_buffer);

    if (s_p4_enc_data.encoder_task_handle == NULL) {
        ESP_LOGE(TAG, "failed to create encoder task!");
        goto cleanup;
    }
#endif

    s_p4_enc_data.encoder_initialized = true;

    ESP_LOGD(TAG, "ESP32P4 frame grabber initialized as singleton (stopped, use start() to begin)");
    return;

cleanup:
    // Conditional cleanup based on what was allocated
    if (s_p4_enc_data.task_buffer != NULL) {
        heap_caps_free(s_p4_enc_data.task_buffer);
        s_p4_enc_data.task_buffer = NULL;
    }
    if (s_p4_enc_data.task_stack != NULL) {
        heap_caps_free(s_p4_enc_data.task_stack);
        s_p4_enc_data.task_stack = NULL;
    }
    if (s_p4_enc_data.run_semaphore != NULL) {
        vSemaphoreDelete(s_p4_enc_data.run_semaphore);
        s_p4_enc_data.run_semaphore = NULL;
    }
    if (s_p4_enc_data.frame_queue != NULL) {
        vQueueDelete(s_p4_enc_data.frame_queue);
        s_p4_enc_data.frame_queue = NULL;
    }

    ESP_LOGE(TAG, "ESP32P4 frame grabber initialization failed");
}

esp_err_t esp32p4_frame_grabber_start(void)
{
    if (!s_p4_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "ESP32P4 frame grabber not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_p4_enc_data.running) {
        ESP_LOGD(TAG, "ESP32P4 frame grabber already running");
        return ESP_OK;
    }

    s_p4_enc_data.running = true;
    xSemaphoreGive(s_p4_enc_data.run_semaphore);  // Signal encoder to start
    ESP_LOGD(TAG, "ESP32P4 frame grabber started");

    return ESP_OK;
}

esp_err_t esp32p4_frame_grabber_stop(void)
{
    if (!s_p4_enc_data.encoder_initialized) {
        ESP_LOGE(TAG, "ESP32P4 frame grabber not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_p4_enc_data.running) {
        ESP_LOGD(TAG, "ESP32P4 frame grabber already stopped");
        return ESP_OK;
    }

    s_p4_enc_data.running = false;
    ESP_LOGD(TAG, "ESP32P4 frame grabber stopped");

    return ESP_OK;
}

esp_err_t esp32p4_frame_grabber_deinit(void)
{
    if (!s_p4_enc_data.encoder_initialized) {
        ESP_LOGD(TAG, "ESP32P4 frame grabber not initialized, nothing to deinitialize");
        return ESP_OK;
    }

    // Stop encoding first
    esp32p4_frame_grabber_stop();

    // Drain the frame queue with limited iterations to prevent infinite loop
    if (s_p4_enc_data.frame_queue != NULL) {
        ESP_LOGD(TAG, "Draining video frame queue...");
        esp_h264_out_buf_t h264_frame;
        int drained_count = 0;
        const int max_drain_iterations = CONFIG_VIDEO_FRAME_QUEUE_SIZE;  // Prevent infinite loop

        while (xQueueReceive(s_p4_enc_data.frame_queue, &h264_frame, 0) == pdTRUE &&
               drained_count < max_drain_iterations) {
            if (h264_frame.buffer) {
                heap_caps_free(h264_frame.buffer);
            }
            drained_count++;
        }

        if (drained_count > 0) {
            ESP_LOGD(TAG, "Drained %d video frames from queue", drained_count);
        }
        if (drained_count >= max_drain_iterations) {
            ESP_LOGI(TAG, "Reached max drain limit, queue may still contain frames");
        }
    }

    // Singleton pattern: encoder task remains running but paused
    ESP_LOGD(TAG, "ESP32P4 frame grabber is singleton - task remains paused until start() is called");

    return ESP_OK;
}
#endif
