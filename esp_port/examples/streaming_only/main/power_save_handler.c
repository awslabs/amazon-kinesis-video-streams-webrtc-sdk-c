/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "power_save_handler.h"
#include "esp_log.h"
#include <inttypes.h>

#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
#include "power_save_drv.h"
#include "freertos/portmacro.h"
#endif

#include <stdio.h>
#include <esp_console.h>

static const char *TAG = "power_save_handler";

#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
/* Idle deep-sleep timer timeout - device sleeps if no activity for this duration */
#define IDLE_DEEP_SLEEP_TIMEOUT_MS (20 * 1000)
/* Post-streaming deep-sleep timer timeout - shorter timeout after streaming stops */
#define POST_STREAMING_DEEP_SLEEP_TIMEOUT_MS (10 * 1000)
/* Track active peer connection count - keep device active while any peer is connected */
static uint32_t g_active_peer_connections = 0;
static portMUX_TYPE g_peer_connections_lock = portMUX_INITIALIZER_UNLOCKED;

/* Helper function to safely increment peer connection count */
static uint32_t increment_peer_connection(void)
{
    uint32_t count = 0;
    portENTER_CRITICAL(&g_peer_connections_lock);
    g_active_peer_connections++;
    count = g_active_peer_connections;
    portEXIT_CRITICAL(&g_peer_connections_lock);
    return count;
}

/* Helper function to safely decrement peer connection count */
static uint32_t decrement_peer_connection(void)
{
    uint32_t count = 0;
    portENTER_CRITICAL(&g_peer_connections_lock);
    if (g_active_peer_connections > 0) {
        g_active_peer_connections--;
    }
    count = g_active_peer_connections;
    portEXIT_CRITICAL(&g_peer_connections_lock);
    return count;
}
#endif

#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
/**
 * @brief Power save event handler for WebRTC events
 */
static void power_save_event_handler(app_webrtc_event_data_t *event_data, void *user_ctx)
{
    if (event_data == NULL) {
        return;
    }

    switch (event_data->event_id) {
        case APP_WEBRTC_EVENT_INITIALIZED:
            /* Start idle timer when WebRTC is initialized */
            portENTER_CRITICAL(&g_peer_connections_lock);
            g_active_peer_connections = 0;
            portEXIT_CRITICAL(&g_peer_connections_lock);
            ESP_LOGI(TAG, "WebRTC initialized, starting idle deep-sleep timer");
            host_power_save_timer_start(IDLE_DEEP_SLEEP_TIMEOUT_MS);
            ESP_LOGI(TAG, "Started idle deep-sleep timer (%d ms)", IDLE_DEEP_SLEEP_TIMEOUT_MS);
            break;
        case APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED:
            ESP_LOGI(TAG, "Peer connection requested, resetting host power save timer");
            host_power_save_timer_start(IDLE_DEEP_SLEEP_TIMEOUT_MS);
            break;

        case APP_WEBRTC_EVENT_PEER_CONNECTED:
            /* Peer connected - increment count and stop timer */
            {
                uint32_t count = increment_peer_connection();
                ESP_LOGI(TAG, "Peer connected: %s (active peers: %" PRIu32 "), stopping host power save timer",
                         event_data->peer_id ? event_data->peer_id : "unknown", count);
                host_power_save_timer_stop();
            }
            break;

        case APP_WEBRTC_EVENT_PEER_DISCONNECTED:
            /* Peer disconnected - decrement count and start timer if no peers left */
            {
                uint32_t count = decrement_peer_connection();
                ESP_LOGI(TAG, "Peer disconnected: %s (active peers: %" PRIu32 ")",
                         event_data->peer_id ? event_data->peer_id : "unknown", count);
                if (count == 0) {
                    ESP_LOGI(TAG, "All peer connections closed, starting host power save timer");
                    /* Give host few seconds, as it might still get some packets */
                    host_power_save_timer_start(POST_STREAMING_DEEP_SLEEP_TIMEOUT_MS);
                } else {
                    ESP_LOGI(TAG, "Other peers still connected (%" PRIu32 "), keeping timer stopped", count);
                }
            }
            break;

        case APP_WEBRTC_EVENT_RECEIVED_OFFER:
            ESP_LOGI(TAG, "Received offer, resetting host power save timer");
            host_power_save_timer_start(IDLE_DEEP_SLEEP_TIMEOUT_MS);
            break;

        case APP_WEBRTC_EVENT_SENT_ANSWER:
            /* Answer sent means connection and streaming will happen, stop timer */
            ESP_LOGI(TAG, "Sent answer, stopping host power save timer");
            host_power_save_timer_stop();
            break;

        default:
            /* Other events don't affect power save */
            break;
    }
}
#endif

int32_t power_save_enable(void)
{
#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
    int32_t ret = app_webrtc_register_event_callback(power_save_event_handler, NULL);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to enable power save");
        return ret;
    }
    ESP_LOGI(TAG, "Power save enabled successfully");
    return 0;
#else
    /* Power save not enabled or not supported for this target */
    return 0;
#endif
}

#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
/**
 * @brief CLI handler for manual deep sleep command
 */
static int deep_sleep_cli_handler(int argc, char *argv[])
{
    ESP_LOGI(TAG, "Putting ESP32-P4 into deep sleep...");
    start_host_power_save();
    return 0;
}

static esp_console_cmd_t sleep_cmd[] = {
    {
        .command = "deep-sleep",
        .help = "Put P4 into deep sleep",
        .func = deep_sleep_cli_handler,
    }
};
#endif

int power_save_cli_register(void)
{
#if CONFIG_IDF_TARGET_ESP32P4 && CONFIG_EXAMPLE_ENABLE_POWER_SAVE
    int cmds_num = sizeof(sleep_cmd) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", sleep_cmd[i].command);
        esp_console_cmd_register(&sleep_cmd[i]);
    }
    return 0;
#else
    /* Power save CLI not enabled or not supported for this target */
    return 0;
#endif
}
