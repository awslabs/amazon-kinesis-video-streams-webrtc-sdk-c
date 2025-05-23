/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#pragma once
#include "esp_wifi.h"
#include "esp_hosted_mock.h"
#include "esp_wifi_remote.h"

#define ESP_HOSTED_CHANNEL_CONFIG_DEFAULT() {}
#define ESP_SERIAL_IF   0
#define ESP_STA_IF      1
#define ESP_AP_IF       2

struct esp_remote_channel_config {
    int if_type;
    bool secure;
};

esp_remote_channel_t esp_hosted_add_channel(struct esp_remote_channel_config *config, esp_remote_channel_tx_fn_t *tx_cb, esp_remote_channel_rx_fn_t rx_cb);
