/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/select.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "linux/videodev2.h"

#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_if.h"
#include "esp_h264_hw_enc.h"
#include "bsp/esp32_p4_function_ev_board.h"
#include "bsp/bsp_board_extra.h"

#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME      "/dev/video0"
#define CAM_DEV_PATH        ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define BUFFER_COUNT        3

#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT       (1)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN    (GPIO_NUM_8)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN    (GPIO_NUM_7)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ       (400000)
#define CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN (-1)
#define CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN (-1)

typedef struct v4l2 {
    int cap_fd;
    uint8_t             *cap_buffer[BUFFER_COUNT];
    bool                fb_used[BUFFER_COUNT];
    struct v4l2_buffer  v4l2_buf[BUFFER_COUNT];
    video_fb_t fb;
} v4l2_src_t;

static v4l2_src_t *g_v4l2 = NULL;

static const char *TAG = "esp_video_if";


static void print_video_device_info(const struct v4l2_capability *capability)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", (uint16_t)(capability->version >> 16),
             (uint8_t)(capability->version >> 8),
             (uint8_t)capability->version);
    ESP_LOGI(TAG, "driver:  %s", capability->driver);
    ESP_LOGI(TAG, "card:    %s", capability->card);
    ESP_LOGI(TAG, "bus:     %s", capability->bus_info);
    ESP_LOGI(TAG, "capabilities:");
    if (capability->capabilities & V4L2_CAP_VIDEO_CAPTURE) {
        ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
    }
    if (capability->capabilities & V4L2_CAP_READWRITE) {
        ESP_LOGI(TAG, "\tREADWRITE");
    }
    if (capability->capabilities & V4L2_CAP_ASYNCIO) {
        ESP_LOGI(TAG, "\tASYNCIO");
    }
    if (capability->capabilities & V4L2_CAP_STREAMING) {
        ESP_LOGI(TAG, "\tSTREAMING");
    }
    if (capability->capabilities & V4L2_CAP_META_OUTPUT) {
        ESP_LOGI(TAG, "\tMETA_OUTPUT");
    }
    if (capability->capabilities & V4L2_CAP_DEVICE_CAPS) {
        ESP_LOGI(TAG, "device capabilities:");
        if (capability->device_caps & V4L2_CAP_VIDEO_CAPTURE) {
            ESP_LOGI(TAG, "\tVIDEO_CAPTURE");
        }
        if (capability->device_caps & V4L2_CAP_READWRITE) {
            ESP_LOGI(TAG, "\tREADWRITE");
        }
        if (capability->device_caps & V4L2_CAP_ASYNCIO) {
            ESP_LOGI(TAG, "\tASYNCIO");
        }
        if (capability->device_caps & V4L2_CAP_STREAMING) {
            ESP_LOGI(TAG, "\tSTREAMING");
        }
        if (capability->device_caps & V4L2_CAP_META_OUTPUT) {
            ESP_LOGI(TAG, "\tMETA_OUTPUT");
        }
    }
}

static esp_err_t init_camera(v4l2_src_t *v4l2)
{
    int fd;
    struct v4l2_capability capability;
    const int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    fd = open(CAM_DEV_PATH, O_RDONLY);
    assert(fd >= 0);

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) {
        ESP_LOGE(TAG, "Failed to query capabilities, errno: %d", errno);
        return ESP_FAIL;
    }
    print_video_device_info(&capability);

    struct v4l2_format format;

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Deafult: width=%" PRIu32 " height=%" PRIu32, format.fmt.pix.width, format.fmt.pix.height);

    v4l2->cap_fd = fd;

    ESP_LOGI(TAG, "Camera capture initialized and streaming started");
    return ESP_OK;
}

static void video_stop_cb(void *cb_ctx)
{
    int type;
    v4l2_src_t *v4l2 = (v4l2_src_t *)cb_ctx;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(v4l2->cap_fd, VIDIOC_STREAMOFF, &type);
}

static video_fb_t *video_fb_get_cb(void *cb_ctx)
{
    int64_t us;
    v4l2_src_t *v4l2 = (v4l2_src_t *)cb_ctx;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (v4l2->fb_used[i] == false) {
            // uint64_t start_time = esp_timer_get_time();
            struct v4l2_buffer *buf = &v4l2->v4l2_buf[i];
            buf->type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf->memory = V4L2_MEMORY_MMAP;
            int ret = ioctl(v4l2->cap_fd, VIDIOC_DQBUF, buf);
            if (ret != 0) {
                ESP_LOGE(TAG, "failed to receive video frame ret %d", ret);
                return NULL;
            }
            v4l2->fb_used[i] = true;
            v4l2->fb.buf = v4l2->cap_buffer[buf->index];
            // printf("Captured buffer num: %d\n", (int) buf->index);
            v4l2->fb.len = buf->bytesused;
            // v4l2->fb.width = format.fmt.pix.width;
            // v4l2->fb.height = format.fmt.pix.height;
            us = esp_timer_get_time();
            v4l2->fb.timestamp.tv_sec = us / 1000000UL;
            v4l2->fb.timestamp.tv_usec = us % 1000000UL;

            // uint64_t end_time = us;
            // printf("Frame Grab FPS: %d\n", (int) (1000000 / (end_time - start_time)));

            return &v4l2->fb;
        }
    }
    return NULL;
}

static void video_fb_return_cb(video_fb_t *fb, void *cb_ctx)
{
    v4l2_src_t *v4l2 = (v4l2_src_t *)cb_ctx;

    ESP_LOGD(TAG, "Returning encoder buffer");

    for (int i = 0; i < BUFFER_COUNT; i++) {
        struct v4l2_buffer *buf = &v4l2->v4l2_buf[i];
        if (v4l2->fb_used[i] && v4l2->cap_buffer[buf->index] == fb->buf) {
            v4l2->fb_used[i] = false;
            ioctl(v4l2->cap_fd, VIDIOC_QBUF, buf);
            return;
        }
    }
}

video_fb_t *esp_video_if_get_frame(void)
{
    video_fb_t *fb = NULL;
    if (g_v4l2) {
        fb = video_fb_get_cb(g_v4l2);
        if (!fb) {
            ESP_LOGE(TAG, "Failed to get frame");
            return NULL;
        }
        return fb;  // Return the raw frame without encoding
    }
    ESP_LOGE(TAG, "Camera not initialized");
    return NULL;
}

void esp_video_if_release_frame(video_fb_t *fb)
{
    if (g_v4l2 && fb) {
        video_fb_return_cb(fb, g_v4l2);
    }
}

esp_err_t esp_video_if_stop(void)
{
    if (g_v4l2) {
        video_stop_cb(g_v4l2);
        /* Free the mapped buffers */
        for (int i = 0; i < BUFFER_COUNT; i++) {
            munmap(g_v4l2->cap_buffer[i], g_v4l2->v4l2_buf[i].length);
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t esp_video_if_start(void)
{
    if (!g_v4l2) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_FAIL;
    }

    struct v4l2_buffer buf;
    struct v4l2_format format;
    struct v4l2_requestbuffers req;
    v4l2_src_t *v4l2 = g_v4l2;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    uint32_t capture_fmt = V4L2_PIX_FMT_YUV420;

    /* Configure camera interface capture stream */
    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = WIDTH;
    format.fmt.pix.height = HEIGHT;
    format.fmt.pix.pixelformat = capture_fmt;
    if (ioctl(v4l2->cap_fd, VIDIOC_S_FMT, &format) < 0) {
        ESP_LOGE(TAG, "Failed to set format, errno: %d. Check the camera resolution in menuconfig", errno);
        return ESP_FAIL;
    }

    memset(&req, 0, sizeof(req));
    req.count  = BUFFER_COUNT;
    req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(v4l2->cap_fd, VIDIOC_REQBUFS, &req) < 0) {
        ESP_LOGE(TAG, "Failed to require buffers, errno: %d", errno);
        return ESP_FAIL;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory      = V4L2_MEMORY_MMAP;
        buf.index       = i;
        if (ioctl(v4l2->cap_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to query buffer, errno: %d", errno);
            return ESP_FAIL;
        }

        v4l2->cap_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                MAP_SHARED, v4l2->cap_fd, buf.m.offset);
        if (!v4l2->cap_buffer[i]) {
            ESP_LOGE(TAG, "Failed to map buffer, errno: %d", errno);
            return ESP_FAIL;
        }

        if (ioctl(v4l2->cap_fd, VIDIOC_QBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to queue buffer, errno: %d", errno);
            return ESP_FAIL;
        }
    }

#if CONFIG_ESP_VIDEO_IF_HOR_FLIP
    // Set horizontal flip using extended controls since VIDIOC_S_CTRL is not supported
    struct v4l2_ext_controls ext_ctrls;
    struct v4l2_ext_control ctrl;
    memset(&ext_ctrls, 0, sizeof(ext_ctrls));
    memset(&ctrl, 0, sizeof(ctrl));

    ctrl.id = V4L2_CID_HFLIP;
    ctrl.value = 1; // 1 to enable horizontal flip, 0 to disable
    ext_ctrls.controls = &ctrl;
    ext_ctrls.count = 1;

    if (ioctl(v4l2->cap_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
        ESP_LOGW(TAG, "Failed to set horizontal flip, errno: %d", errno);
    }
#endif

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2->cap_fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGE(TAG, "Failed to stream on, errno: %d", errno);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_video_if_init(void)
{
    if (g_v4l2) {
        ESP_LOGI(TAG, "video interface already initialized");
        return ESP_OK;
    }

#if CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
    ESP_LOGE(TAG, "esp_video is not compatible with old I2C driver");
    return ESP_FAIL;
#endif

    /* Initialize I2C bus */
    bsp_i2c_init();

    v4l2_src_t *v4l2 = heap_caps_calloc(1, sizeof(v4l2_src_t), MALLOC_CAP_SPIRAM);
    if (!v4l2) {
        ESP_LOGE(TAG, "Failed to allocate memory for v4l2");
        return ESP_FAIL;
    }

    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = bsp_get_i2c_bus_handle(),
                .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
            },
            .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
            .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
        },
    };

    esp_video_init_config_t cam_config = {
        .csi      = csi_config,
    };

    if (esp_video_init(&cam_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize video");
        free(v4l2);
        return ESP_FAIL;
    }

    if (init_camera(v4l2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize capture video");
        free(v4l2);
        return ESP_FAIL;
    }
    g_v4l2 = v4l2;

    if (esp_video_if_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start video");
        g_v4l2 = NULL;
        free(v4l2);
        return ESP_FAIL;
    }

    return ESP_OK;
}
#endif
