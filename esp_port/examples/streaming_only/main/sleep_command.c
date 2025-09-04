/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <esp_console.h>
#include <esp_log.h>

#if CONFIG_IDF_TARGET_ESP32P4
#include "power_save_drv.h"
#endif

static const char *TAG = "sleep_command";

static int deep_sleep_cli_handler(int argc, char *argv[])
{
#if CONFIG_IDF_TARGET_ESP32P4
    ESP_LOGI(TAG, "Putting ESP32-P4 into deep sleep...");
    start_host_power_save();
#else
    ESP_LOGI(TAG, "Deep sleep not supported for this arch");
#endif
    return 0;
}

static esp_console_cmd_t sleep_cmd[] = {
    {
        .command = "deep-sleep",
        .help = "Put P4 into deep sleep",
        .func = deep_sleep_cli_handler,
    }
};

int sleep_command_register_cli()
{
    int cmds_num = sizeof(sleep_cmd) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", sleep_cmd[i].command);
        esp_console_cmd_register(&sleep_cmd[i]);
    }
    return 0;
}
