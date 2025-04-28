/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <esp_log.h>
#include <esp_rmaker_core.h>
#include <esp_rmaker_standard_types.h>
#include <esp_rmaker_standard_params.h>
#include "app_rainmaker.h"
#include "esp_rmaker_console.h"

static const char *TAG = "app_rainmaker";

// Device name prefix
static char *device_name = "ESP_WebRTC";
static const char *param_type = "esp.param.channel";
static const char *device_type = "esp.device.camera";

esp_err_t app_rainmaker_init(void)
{
    esp_rmaker_config_t config = {
        .enable_time_sync = false,
    };

    esp_rmaker_node_t *node = esp_rmaker_node_init(&config, device_name, "esp_webrtc");
    if (!node) {
        ESP_LOGE(TAG, "Failed to initialize RainMaker node");
        return ESP_FAIL;
    }

    // Add your device(s) and parameters here
    esp_rmaker_device_t *device = esp_rmaker_device_create("WebRTC_Device", device_type, NULL);
    if (!device) {
        ESP_LOGE(TAG, "Failed to create device");
        return ESP_FAIL;
    }

    if (esp_rmaker_node_add_device(node, device) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add device to node");
        return ESP_FAIL;
    }

    const char *node_id = esp_rmaker_get_node_id();
    esp_rmaker_param_t *channel_param = esp_rmaker_param_create("Channel", param_type, esp_rmaker_str(node_id), PROP_FLAG_READ);
    // esp_rmaker_param_add_ui_type(channel_param, ESP_RMAKER_UI_TRIGGER);
    esp_rmaker_device_add_param(device, channel_param);

    // Start RainMaker
    if (esp_rmaker_start() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start RainMaker");
        return ESP_FAIL;
    }

    // init rainmaker console and add commands to it
    esp_rmaker_console_init();

    return ESP_OK;
}
