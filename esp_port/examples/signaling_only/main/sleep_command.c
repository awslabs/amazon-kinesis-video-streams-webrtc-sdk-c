/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <esp_console.h>
#include <esp_log.h>
#if CONFIG_IDF_TARGET_ESP32C6
#include "host_power_save.h"
#endif

static const char *TAG = "sleep_cmd";

static int wakeup_cli_handler(int argc, char *argv[])
{
#if CONFIG_IDF_TARGET_ESP32C6
    ESP_LOGI(TAG, "Asking P4 to wake-up...");
    wakeup_host(500);
#endif
    return 0;
}

static esp_console_cmd_t sleep_cmds[] = {
    {
        .command = "wake-up",
        .help = "Wake-up P4 from deep sleep",
        .func = wakeup_cli_handler,
    }
};

int sleep_command_register_cli()
{
    int cmds_num = sizeof(sleep_cmds) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", sleep_cmds[i].command);
        esp_console_cmd_register(&sleep_cmds[i]);
    }
    return 0;
}
