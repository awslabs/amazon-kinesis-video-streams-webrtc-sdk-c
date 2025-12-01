/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#if CONFIG_IDF_CMAKE
#include <esp_err.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

static inline void* serializer_memalloc(size_t size) {
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, MALLOC_CAP_DEFAULT);
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <time.h>

static uint32_t get_timestamp(void)
{
    struct timespec current_time;
    int result = clock_gettime(CLOCK_MONOTONIC, &current_time);
    uint32_t milliseconds = current_time.tv_sec * 1000 + current_time.tv_nsec / 1000000;
    return milliseconds;
}
typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102

#define ESP_LOGX(level, tag, fmt, ...) printf("%s (%" PRIu32 ") %s: " fmt "\n", level, get_timestamp(), tag, ##__VA_ARGS__)

// ESP logging macros for Linux
#define ESP_LOGI(tag, fmt, ...) ESP_LOGX("I", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) ESP_LOGX("E", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) ESP_LOGX("W", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ESP_LOGX("D", tag, fmt, ##__VA_ARGS__)
#define ESP_LOGV(tag, fmt, ...) ESP_LOGX("V", tag, fmt, ##__VA_ARGS__)

static inline void* serializer_memalloc(size_t size) {
    return malloc(size);
}
#endif
