/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "esp_wifi_remote.h"

void run_all_wifi_apis(void);
void run_all_wifi_remote_apis(void);

void app_main(void)
{
    // manual init and deinit
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_deinit();

    run_all_wifi_apis();
    run_all_wifi_remote_apis();
}
