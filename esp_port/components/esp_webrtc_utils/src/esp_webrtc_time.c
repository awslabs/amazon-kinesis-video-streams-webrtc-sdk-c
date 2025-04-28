/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#else
#include "esp_sntp.h"
#endif

#if ENABLE_STREAMING_ONLY && CONFIG_IDF_TARGET_ESP32P4
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "rpc_wrap.h"
static rpc_usr_t req = {0};
static rpc_usr_t resp = {0};
#endif

static const char *TAG = "esp_webrtc_time";
static bool time_sync_done = false;

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
    time_sync_done = true;
}

bool kvswebrtc_is_time_sync_done()
{
    return time_sync_done;
}

static const char *server_list[] = {
    "stratum2.iad01.publicntp.org",
    "pool.ntp.org",
    "time.google.com",
    "time.cloudflare.com"
};
static const int num_servers = sizeof(server_list) / sizeof(server_list[0]);

static void initialize_sntp(void)
{
#if ENABLE_STREAMING_ONLY && CONFIG_IDF_TARGET_ESP32P4
    // Define the date and time: 1 July 2024, 00:00:00
    struct tm date = {0};
    date.tm_year = 2024 - 1900; // tm_year is years since 1900
    date.tm_mon = 7 - 1;        // tm_mon is months since January (0-11)
    date.tm_mday = 30;           // Day of the month

    // Convert to time_t
    time_t ref_time = mktime(&date);

    int retry_attempts = 5;
#define SOME_STR "Req GetTimeOfTheDay"
    struct timeval tv = {};
    // Set time obtained from coprocessor first
    req.data_len = sizeof(SOME_STR);
    memcpy(req.data, SOME_STR, sizeof(SOME_STR));

retry_sync:
    int correction_usec = 0;
    rpc_send_usr_request(2, &req, &resp);
    struct timeval time_start, time_end;
    gettimeofday(&time_start, NULL);
    memcpy(&tv, resp.data, sizeof(struct timeval));
    if (tv.tv_sec < ref_time) {
        ESP_LOGI(TAG, "Retrying time sync from coprocessor...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        goto retry_sync;
    } else {
        time_sync_done = true;
        gettimeofday(&time_end, NULL);
        int rtt_us = (time_end.tv_sec - time_start.tv_sec) * 1000000 + time_end.tv_usec - time_start.tv_usec;
        correction_usec = rtt_us / 2;
        tv.tv_sec += correction_usec / 1000000;
        tv.tv_usec += correction_usec % 1000000;
    }
    // Set the last obtained time and go ahead
    settimeofday(&tv, NULL);
    ESP_LOGI(TAG, "New time set with %dus correction", correction_usec);

    if (time_sync_done) {
        return;
    }
#endif

    ESP_LOGI(TAG, "Initializing SNTP");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    // First deinit in case of reinitialization
    esp_netif_sntp_deinit();

    esp_sntp_config_t config = {
        .smooth_sync = false,  // Changed to false for faster initial sync
        .server_from_dhcp = false,
        .wait_for_sync = true,
        .start = true,
        .sync_cb = time_sync_notification_cb,
        .renew_servers_after_new_IP = false,  // Don't change servers after IP
        .ip_event_to_renew = IP_EVENT_STA_GOT_IP,
        .index_of_first_server = 0,
        .num_of_servers = 1,  // Try one server at a time
        .servers = {
            server_list[0],  // Start with most reliable
        }
    };
    esp_netif_sntp_init(&config);
#else
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, server_list[0]);
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
#endif
    sntp_init();
#endif
}

void esp_webrtc_time_sntp_time_sync_no_wait()
{
    if (time_sync_done){
        ESP_LOGI(TAG, "Time sync already done");
        return;
    }

    initialize_sntp();
}

void esp_webrtc_time_sntp_time_sync_and_wait()
{
    if (time_sync_done){
        ESP_LOGI(TAG, "Time sync already done");
        return;
    }

    initialize_sntp();

    // wait for time to be set
    int retry = 0;
    const int retry_count = num_servers * 3;
    const int retry_delay_ms = 2000;
    int current_server = 0;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (time_sync_done) {
        ESP_LOGI(TAG, "Time sync completed successfully");
        return;
    }
    while (retry < retry_count) {
        esp_err_t ret = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(retry_delay_ms));
        if (ret == ESP_OK) {
            time_sync_done = true;
            ESP_LOGI(TAG, "Time sync completed successfully");
            break;
        }
        retry++;
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);

        // Try next server after 3 failures
        if (retry % 3 == 0) {
            current_server = (current_server + 1) % num_servers;
            ESP_LOGW(TAG, "Switching to NTP server: %s", server_list[current_server]);

            esp_netif_sntp_deinit();
            esp_sntp_config_t new_config = {
                .smooth_sync = false,
                .server_from_dhcp = false,
                .wait_for_sync = true,
                .start = true,
                .sync_cb = time_sync_notification_cb,
                .renew_servers_after_new_IP = false,
                .ip_event_to_renew = IP_EVENT_STA_GOT_IP,
                .index_of_first_server = 0,
                .num_of_servers = 1,
                .servers = {
                    server_list[current_server],
                }
            };
            esp_netif_sntp_init(&new_config);
        }
    }
#else
    while (retry < retry_count) {
        if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
            time_sync_done = true;
            ESP_LOGI(TAG, "Time sync completed successfully");
            break;
        }
        retry++;
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        if (retry % 3 == 0) {
            current_server = (current_server + 1) % num_servers;
            ESP_LOGW(TAG, "Switching to NTP server: %s", server_list[current_server]);
            sntp_stop();
            vTaskDelay(pdMS_TO_TICKS(100));
            sntp_setservername(0, server_list[current_server]);
            sntp_set_time_sync_notification_cb(time_sync_notification_cb);
            sntp_init();
        }
        vTaskDelay(pdMS_TO_TICKS(retry_delay_ms));
    }
#endif
    if (!time_sync_done) {
        ESP_LOGE(TAG, "Failed to sync time after %d attempts", retry_count);
    }
}
