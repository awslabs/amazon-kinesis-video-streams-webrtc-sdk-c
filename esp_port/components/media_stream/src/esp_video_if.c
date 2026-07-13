/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: ESPRESSIF MIT
 */

#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32P4
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <sys/select.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "linux/videodev2.h"

#include "esp_video_init.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "esp_cam_sensor.h"
#include "esp_video_if.h"
#include "esp_h264_hw_enc.h"
#include "bsp/esp32_p4_function_ev_board.h"

#if CONFIG_IDF_TARGET_ESP32P4
/* Forward declaration of internal I2C init function */
extern esp_err_t media_stream_i2c_init_safe(void);
#endif

#define ESP_VIDEO_MIPI_CSI_DEVICE_NAME      "/dev/video0"
#define CAM_DEV_PATH        ESP_VIDEO_MIPI_CSI_DEVICE_NAME
#define BUFFER_COUNT        3
#define USE_V4L2_USERPTR    1
#define USERPTR_ALIGNMENT   64
#define USERPTR_HEAP_CAPS   (MALLOC_CAP_SPIRAM | MALLOC_CAP_DMA)

#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_PORT       (1)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SCL_PIN    (GPIO_NUM_8)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_SDA_PIN    (GPIO_NUM_7)
#define CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ       (400000)
#define CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN (-1)
#define CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN (-1)

typedef struct v4l2 {
    int cap_fd;
    uint8_t             *cap_buffer[BUFFER_COUNT];
    size_t              buffer_size[BUFFER_COUNT];  /* Store sizes separately for USERPTR fd reopen */
    bool                fb_used[BUFFER_COUNT];
    struct v4l2_buffer  v4l2_buf[BUFFER_COUNT];
    bool                buffers_allocated;
    video_fb_t fb;
} v4l2_src_t;

static v4l2_src_t *g_v4l2 = NULL;
static video_resolution_t g_current_resolution = {.width = 0, .height = 0, .fps = 0};

/* Global variable to pre-configure resolution before init (set by video_capture_adapter) */
video_resolution_t g_desired_resolution = {.width = 0, .height = 0, .fps = 0};

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
    if (fd < 0) {
        ESP_LOGE(TAG, "Failed to open camera device %s, errno: %d", CAM_DEV_PATH, errno);
        return ESP_FAIL;
    }

    if (ioctl(fd, VIDIOC_QUERYCAP, &capability) < 0) {
        ESP_LOGE(TAG, "Failed to query capabilities, errno: %d", errno);
        close(fd);  /* Close file descriptor on error */
        return ESP_FAIL;
    }
    print_video_device_info(&capability);

    struct v4l2_format format;

    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = type;
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        ESP_LOGE(TAG, "failed to get format");
        close(fd);  /* Close file descriptor on error */
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Default: width=%" PRIu32 " height=%" PRIu32, format.fmt.pix.width, format.fmt.pix.height);

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

static void requeue_used_buffers(v4l2_src_t *v4l2)
{
    if (!v4l2) {
        return;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (v4l2->fb_used[i]) {
            v4l2->fb_used[i] = false;
#if USE_V4L2_USERPTR
            /* For USERPTR, always set the pointer and length before requeueing */
            v4l2->v4l2_buf[i].m.userptr = (unsigned long)v4l2->cap_buffer[i];
            /* length should already be set from QUERYBUF */
#endif
            ioctl(v4l2->cap_fd, VIDIOC_QBUF, &v4l2->v4l2_buf[i]);
        }
    }
}

static esp_err_t queue_all_buffers(v4l2_src_t *v4l2)
{
    if (!v4l2) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        v4l2->v4l2_buf[i].type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2->v4l2_buf[i].memory = USE_V4L2_USERPTR ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;
        v4l2->v4l2_buf[i].index = i;
        v4l2->fb_used[i] = false;

        if (USE_V4L2_USERPTR) {
            v4l2->v4l2_buf[i].m.userptr = (unsigned long)v4l2->cap_buffer[i];
            v4l2->v4l2_buf[i].length = v4l2->buffer_size[i];  /* Restore from saved size */
            if (v4l2->buffer_size[i] == 0) {
                ESP_LOGE(TAG, "USERPTR buffer size not set for buffer %d", i);
                return ESP_FAIL;
            }
            v4l2->v4l2_buf[i].bytesused = 0;
        }

        if (ioctl(v4l2->cap_fd, VIDIOC_QBUF, &v4l2->v4l2_buf[i]) < 0) {
            ESP_LOGE(TAG, "Failed to requeue buffer %d, errno: %d", i, errno);
            /* Buffers 0..i-1 are already queued to the driver. Flush them with
             * STREAMOFF (legal before STREAMON; returns every queued buffer to
             * the dequeued state) so the driver is not left half-queued and a
             * later start attempt begins from a clean queue. */
            int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if (i > 0 && ioctl(v4l2->cap_fd, VIDIOC_STREAMOFF, &type) < 0) {
                ESP_LOGE(TAG, "STREAMOFF after partial queue failed, errno: %d (full re-init required)", errno);
            }
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static video_fb_t *video_fb_get_cb(void *cb_ctx)
{
    int64_t us;
    v4l2_src_t *v4l2 = (v4l2_src_t *)cb_ctx;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (v4l2->fb_used[i] == false) {
            struct v4l2_buffer buf = {
                .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                .memory = USE_V4L2_USERPTR ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP,
            };
            int ret = ioctl(v4l2->cap_fd, VIDIOC_DQBUF, &buf);
            if (ret != 0) {
                ESP_LOGE(TAG, "failed to receive video frame ret %d", ret);
                return NULL;
            }

            /* Guard the driver-supplied index before it touches fb_used[] /
             * cap_buffer[] / v4l2_buf[] (all sized BUFFER_COUNT): a buggy or
             * racing driver returning an unexpected index must not become an
             * out-of-bounds write. */
            if (buf.index >= BUFFER_COUNT) {
                ESP_LOGE(TAG, "DQBUF returned out-of-range buffer index %u", (unsigned) buf.index);
                return NULL;
            }

            v4l2->fb_used[buf.index] = true;
            v4l2->fb.buf = v4l2->cap_buffer[buf.index];
            v4l2->fb.len = buf.bytesused;
            v4l2->v4l2_buf[buf.index] = buf;

            esp_cache_msync(v4l2->fb.buf, v4l2->fb.len, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

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
        if (v4l2->fb_used[i] && v4l2->cap_buffer[i] == fb->buf) {
            v4l2->fb_used[i] = false;
#if USE_V4L2_USERPTR
            /* For USERPTR, always set the pointer and length before requeueing (per ESP-BSP example) */
            v4l2->v4l2_buf[i].m.userptr = (unsigned long)v4l2->cap_buffer[i];
            /* length should already be set from QUERYBUF, but as fallback use fb length */
            if (v4l2->v4l2_buf[i].length == 0) {
                v4l2->v4l2_buf[i].length = fb->len;
            }
#endif
            ioctl(v4l2->cap_fd, VIDIOC_QBUF, &v4l2->v4l2_buf[i]);
            return;
        }
    }
}

static void free_mapped_buffers(v4l2_src_t *v4l2)
{
    if (!v4l2 || !v4l2->buffers_allocated) {
        return;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (v4l2->cap_buffer[i]) {
            if (USE_V4L2_USERPTR) {
                heap_caps_free(v4l2->cap_buffer[i]);
            } else {
                munmap(v4l2->cap_buffer[i], v4l2->buffer_size[i]);
            }
            v4l2->cap_buffer[i] = NULL;
        }
        v4l2->buffer_size[i] = 0;
        v4l2->fb_used[i] = false;
    }

    v4l2->buffers_allocated = false;
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
        requeue_used_buffers(g_v4l2);
        video_stop_cb(g_v4l2);
        /* Keep buffers allocated to avoid fragmentation - they'll be reused on next start */
        ESP_LOGD(TAG, "Streaming stopped (buffers kept allocated for reuse)");
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t esp_video_if_deinit(void)
{
    if (!g_v4l2) {
        ESP_LOGD(TAG, "Video interface not initialized, nothing to deinitialize");
        return ESP_OK;
    }

    ESP_LOGD(TAG, "Deinitializing camera hardware (keeping buffers for reuse)");

    // Stop streaming first (stops frame capture and reduces power consumption)
    esp_video_if_stop();

#if USE_V4L2_USERPTR
    /* For USERPTR: Close fd to power down hardware, but keep buffer memory */
    if (g_v4l2->cap_fd >= 0) {
        close(g_v4l2->cap_fd);
        g_v4l2->cap_fd = -1;
        ESP_LOGD(TAG, "Closed camera fd (hardware powered down, USERPTR buffers retained)");
    }
#else
    /* For MMAP: Keep fd open so buffers remain valid */
    ESP_LOGD(TAG, "Kept fd open (MMAP buffers remain valid)");
#endif

    return ESP_OK;
}

/* Helper: Reopen camera for USERPTR after fd was closed */
static esp_err_t reopen_camera_for_userptr(v4l2_src_t *v4l2)
{
#if USE_V4L2_USERPTR
    if (v4l2->cap_fd >= 0) {
        return ESP_OK;  /* Already open */
    }

    ESP_LOGD(TAG, "USERPTR buffers allocated but fd closed, reopening camera");

    /* Reopen camera device */
    if (init_camera(v4l2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reopen camera for USERPTR");
        return ESP_FAIL;
    }

    /* Set format again */
    struct v4l2_format format = {0};
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    format.fmt.pix.width = g_current_resolution.width;
    format.fmt.pix.height = g_current_resolution.height;
    format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
    if (ioctl(v4l2->cap_fd, VIDIOC_S_FMT, &format) != 0) {
        ESP_LOGE(TAG, "Failed to set format on reopen");
        return ESP_FAIL;
    }

    /* Re-request USERPTR buffers */
    struct v4l2_requestbuffers req = {0};
    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_USERPTR;
    if (ioctl(v4l2->cap_fd, VIDIOC_REQBUFS, &req) < 0) {
        ESP_LOGE(TAG, "Failed to re-request USERPTR buffers, errno: %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Camera reopened, reusing %d USERPTR buffers", BUFFER_COUNT);
#endif
    return ESP_OK;
}

/* Helper: Apply flip controls and start streaming */
static esp_err_t start_streaming(v4l2_src_t *v4l2)
{
#if CONFIG_ESP_VIDEO_IF_HOR_FLIP || CONFIG_ESP_VIDEO_IF_VER_FLIP
    struct v4l2_ext_controls ext_ctrls = {0};
    struct v4l2_ext_control ctrls[2] = {0};
    int ctrl_count = 0;

#if CONFIG_ESP_VIDEO_IF_HOR_FLIP
    ctrls[ctrl_count].id = V4L2_CID_HFLIP;
    ctrls[ctrl_count].value = 1;
    ctrl_count++;
#endif

#if CONFIG_ESP_VIDEO_IF_VER_FLIP
    ctrls[ctrl_count].id = V4L2_CID_VFLIP;
    ctrls[ctrl_count].value = 1;
    ctrl_count++;
#endif

    ext_ctrls.controls = ctrls;
    ext_ctrls.count = ctrl_count;

    if (ioctl(v4l2->cap_fd, VIDIOC_S_EXT_CTRLS, &ext_ctrls) < 0) {
        ESP_LOGW(TAG, "Failed to set flip controls, errno: %d", errno);
    }
#endif

    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2->cap_fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGE(TAG, "Failed to stream on, errno: %d", errno);
        /* Cleanup all buffers on stream start failure */
        for (int i = 0; i < BUFFER_COUNT; i++) {
            if (v4l2->cap_buffer[i]) {
                if (USE_V4L2_USERPTR) {
                    heap_caps_free(v4l2->cap_buffer[i]);
                } else {
                    munmap(v4l2->cap_buffer[i], v4l2->buffer_size[i]);
                }
                v4l2->cap_buffer[i] = NULL;
            }
        }
        v4l2->buffers_allocated = false;
        return ESP_FAIL;
    }

    return ESP_OK;
}

/* Helper: Allocate and queue buffers for first-time setup */
static esp_err_t allocate_and_queue_buffers(v4l2_src_t *v4l2)
{
    struct v4l2_buffer buf;
    struct v4l2_requestbuffers req = {0};

    req.count = BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = USE_V4L2_USERPTR ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;

    if (ioctl(v4l2->cap_fd, VIDIOC_REQBUFS, &req) < 0) {
        ESP_LOGE(TAG, "Failed to require buffers, errno: %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Allocating %d %s buffers (first time or after cleanup)",
             BUFFER_COUNT, USE_V4L2_USERPTR ? "USERPTR" : "MMAP");

    for (int i = 0; i < BUFFER_COUNT; i++) {
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = USE_V4L2_USERPTR ? V4L2_MEMORY_USERPTR : V4L2_MEMORY_MMAP;
        buf.index = i;

        /* Query buffer to get the required length from driver */
        if (ioctl(v4l2->cap_fd, VIDIOC_QUERYBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to query buffer, errno: %d", errno);
#if USE_V4L2_USERPTR
            for (int j = 0; j < i; j++) {
                if (v4l2->cap_buffer[j]) {
                    heap_caps_free(v4l2->cap_buffer[j]);
                    v4l2->cap_buffer[j] = NULL;
                }
            }
#else
            for (int j = 0; j < i; j++) {
                if (v4l2->cap_buffer[j]) {
                    munmap(v4l2->cap_buffer[j], v4l2->buffer_size[j]);
                    v4l2->cap_buffer[j] = NULL;
                }
            }
#endif
            return ESP_FAIL;
        }

        /* Store buffer info */
        v4l2->v4l2_buf[i] = buf;
        v4l2->buffer_size[i] = buf.length;

#if USE_V4L2_USERPTR
        /* Allocate user buffer */
        v4l2->cap_buffer[i] = heap_caps_aligned_alloc(USERPTR_ALIGNMENT, buf.length, USERPTR_HEAP_CAPS);
        if (!v4l2->cap_buffer[i]) {
            ESP_LOGE(TAG, "Failed to allocate USERPTR buffer %d size=%u (caps=0x%x)",
                     i, (unsigned)buf.length, USERPTR_HEAP_CAPS);
            for (int j = 0; j < i; j++) {
                if (v4l2->cap_buffer[j]) {
                    heap_caps_free(v4l2->cap_buffer[j]);
                    v4l2->cap_buffer[j] = NULL;
                }
            }
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "Allocated USERPTR buffer %d addr=%p size=%zu caps=0x%x",
                 i, v4l2->cap_buffer[i], v4l2->buffer_size[i], USERPTR_HEAP_CAPS);
        buf.m.userptr = (unsigned long)v4l2->cap_buffer[i];
#else
        /* Map buffer */
        v4l2->cap_buffer[i] = (uint8_t *)mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                                                MAP_SHARED, v4l2->cap_fd, buf.m.offset);
        if (!v4l2->cap_buffer[i]) {
            ESP_LOGE(TAG, "Failed to map buffer, errno: %d", errno);
            for (int j = 0; j < i; j++) {
                if (v4l2->cap_buffer[j]) {
                    munmap(v4l2->cap_buffer[j], v4l2->buffer_size[j]);
                    v4l2->cap_buffer[j] = NULL;
                }
            }
            return ESP_FAIL;
        }
#endif

        /* Queue buffer */
        if (ioctl(v4l2->cap_fd, VIDIOC_QBUF, &buf) < 0) {
            ESP_LOGE(TAG, "Failed to queue buffer %d, errno: %d userptr=%p len=%u bytesused=%u",
                     i, errno, (void *)buf.m.userptr, (unsigned)buf.length, (unsigned)buf.bytesused);
            for (int j = 0; j <= i; j++) {
                if (v4l2->cap_buffer[j]) {
                    if (USE_V4L2_USERPTR) {
                        heap_caps_free(v4l2->cap_buffer[j]);
                    } else {
                        munmap(v4l2->cap_buffer[j], v4l2->buffer_size[j]);
                    }
                    v4l2->cap_buffer[j] = NULL;
                }
            }
            return ESP_FAIL;
        }

        v4l2->v4l2_buf[i] = buf;
    }

    v4l2->buffers_allocated = true;
    return ESP_OK;
}

/* Helper: Configure camera format with fallback resolution support */
static esp_err_t configure_camera_format(v4l2_src_t *v4l2, uint32_t pixelformat)
{
    typedef struct {
        uint32_t width;
        uint32_t height;
    } resolution_t;

    resolution_t fallback_resolutions[] = {
        {g_desired_resolution.width ? g_desired_resolution.width : WIDTH,
         g_desired_resolution.height ? g_desired_resolution.height : HEIGHT},
        {1280, 720},
        {800, 600},
        {640, 480},
        {320, 240}
    };
    int num_fallbacks = sizeof(fallback_resolutions) / sizeof(fallback_resolutions[0]);

    for (int i = 0; i < num_fallbacks; i++) {
        /* Skip duplicate attempts */
        if (i > 0 && fallback_resolutions[i].width == fallback_resolutions[i - 1].width &&
            fallback_resolutions[i].height == fallback_resolutions[i - 1].height) {
            continue;
        }

        struct v4l2_format format = {0};
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = fallback_resolutions[i].width;
        format.fmt.pix.height = fallback_resolutions[i].height;
        format.fmt.pix.pixelformat = pixelformat;

        if (i == 0) {
            ESP_LOGD(TAG, "Attempting to set format: %dx%d", (int)format.fmt.pix.width, (int)format.fmt.pix.height);
        } else {
            ESP_LOGW(TAG, "Retrying with fallback resolution: %dx%d", (int)format.fmt.pix.width, (int)format.fmt.pix.height);
        }

        if (ioctl(v4l2->cap_fd, VIDIOC_S_FMT, &format) == 0) {
            ESP_LOGI(TAG, "Successfully set format: %dx%d", (int)format.fmt.pix.width, (int)format.fmt.pix.height);
            /* Track actual resolution that was set (V4L2 may adjust it) */
            g_current_resolution.width = format.fmt.pix.width;
            g_current_resolution.height = format.fmt.pix.height;
            g_current_resolution.fps = g_desired_resolution.fps ? g_desired_resolution.fps : 30;
            return ESP_OK;
        } else {
            ESP_LOGW(TAG, "Failed to set format %dx%d, errno: %d",
                     (int)format.fmt.pix.width, (int)format.fmt.pix.height, errno);
        }
    }

    ESP_LOGE(TAG, "Failed to set any supported format. Check the camera resolution in menuconfig");
    return ESP_FAIL;
}

/* Helper: Restart streaming with already-allocated buffers */
static esp_err_t restart_streaming_with_existing_buffers(v4l2_src_t *v4l2)
{
    ESP_LOGD(TAG, "Buffers already allocated (%d buffers in %s mode), skipping reallocation",
             BUFFER_COUNT, USE_V4L2_USERPTR ? "USERPTR" : "MMAP");

    /* For USERPTR, reopen fd if needed */
    if (reopen_camera_for_userptr(v4l2) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Requeue all buffers */
    if (queue_all_buffers(v4l2) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to requeue buffers for restart");
        return ESP_FAIL;
    }

    /* Start streaming */
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(v4l2->cap_fd, VIDIOC_STREAMON, &type) < 0) {
        ESP_LOGE(TAG, "Failed to stream on, errno: %d", errno);
        return ESP_FAIL;
    }

    ESP_LOGD(TAG, "Video streaming restarted successfully (no reallocation)");
    return ESP_OK;
}

esp_err_t esp_video_if_start(void)
{
    if (!g_v4l2) {
        ESP_LOGE(TAG, "Camera not initialized");
        return ESP_FAIL;
    }

    v4l2_src_t *v4l2 = g_v4l2;

    ESP_LOGD(TAG, "Starting video (buffers_allocated=%d, fd=%d, mode=%s)",
             v4l2->buffers_allocated, v4l2->cap_fd, USE_V4L2_USERPTR ? "USERPTR" : "MMAP");

    /* Fast path: buffers already allocated, just restart streaming */
    if (v4l2->buffers_allocated) {
        return restart_streaming_with_existing_buffers(v4l2);
    }

    /* Slow path: First-time setup or after cleanup */
    uint32_t capture_fmt = V4L2_PIX_FMT_YUV420;

    /* Configure camera format with fallback resolution support */
    if (configure_camera_format(v4l2, capture_fmt) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Allocate and queue buffers */
    if (allocate_and_queue_buffers(v4l2) != ESP_OK) {
        return ESP_FAIL;
    }

    /* Apply flip controls and start streaming */
    return start_streaming(v4l2);
}

esp_err_t esp_video_if_init(void)
{
    if (g_v4l2) {
        ESP_LOGD(TAG, "video interface already initialized, restarting streaming");
        return esp_video_if_start();
    }

#if CONFIG_CODEC_I2C_BACKWARD_COMPATIBLE
    ESP_LOGE(TAG, "esp_video is not compatible with old I2C driver");
    return ESP_FAIL;
#endif

    /* Ensure I2C is initialized using the safe initialization function */
    esp_err_t i2c_ret = media_stream_i2c_init_safe();
    if (i2c_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2C: %s", esp_err_to_name(i2c_ret));
        return ESP_FAIL;
    }

    v4l2_src_t *v4l2 = heap_caps_calloc(1, sizeof(v4l2_src_t), MALLOC_CAP_SPIRAM);
    if (!v4l2) {
        ESP_LOGE(TAG, "Failed to allocate memory for v4l2");
        return ESP_FAIL;
    }

    esp_video_init_csi_config_t csi_config[] = {
        {
            .sccb_config = {
                .init_sccb = false,
                .i2c_handle = bsp_i2c_get_handle(),
                .freq = CONFIG_EXAMPLE_MIPI_CSI_SCCB_I2C_FREQ,
            },
            .reset_pin = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_RESET_PIN,
            .pwdn_pin  = CONFIG_EXAMPLE_MIPI_CSI_CAM_SENSOR_PWDN_PIN,
        },
    };

    // Check if video device already exists (from previous initialization)
    // If it exists, esp_video_init() is not needed because ISP is already registered
    // NOTE: This handles the case where esp_video_if_deinit() was called but ISP device
    // registration persists (no esp_video_deinit() API exists)
    int test_fd = open(CAM_DEV_PATH, O_RDONLY);
    if (test_fd >= 0) {
        close(test_fd);
        ESP_LOGD(TAG, "Video device already exists (ISP already registered), skipping esp_video_init");
    } else {
        // Device doesn't exist - need to initialize esp_video to register ISP device
        esp_video_init_config_t cam_config = {
            .csi      = csi_config,
        };

        esp_err_t video_init_ret = esp_video_init(&cam_config);
        if (video_init_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize video");
            free(v4l2);
            return ESP_FAIL;
        }
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
        if (v4l2->cap_fd >= 0) {
            close(v4l2->cap_fd);  /* Close file descriptor before freeing */
        }
        free(v4l2);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t esp_video_if_get_resolution(video_resolution_t *resolution)
{
    if (!resolution) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_v4l2 || g_current_resolution.width == 0) {
        return ESP_ERR_INVALID_STATE;
    }

    *resolution = g_current_resolution;
    return ESP_OK;
}

esp_err_t esp_video_if_cleanup(void)
{
    /* This function explicitly frees mapped buffers and closes the camera fd.
     * Call this when you want a full cleanup (releasing mmap memory).
     */

    if (!g_v4l2) {
        ESP_LOGD(TAG, "No buffers to clean up");
        return ESP_OK;
    }

    video_stop_cb(g_v4l2);
    free_mapped_buffers(g_v4l2);

    if (g_v4l2->cap_fd >= 0) {
        close(g_v4l2->cap_fd);
        g_v4l2->cap_fd = -1;
    }

    heap_caps_free(g_v4l2);
    g_v4l2 = NULL;

    g_current_resolution.width = 0;
    g_current_resolution.height = 0;
    g_current_resolution.fps = 0;

    ESP_LOGI(TAG, "Buffers and camera fd cleaned up");
    return ESP_OK;
}
#endif
