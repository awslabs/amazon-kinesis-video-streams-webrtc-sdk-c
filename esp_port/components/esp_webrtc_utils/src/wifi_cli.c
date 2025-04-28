/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

#include <esp_wifi.h>
#include <esp_console.h>
#include <esp_log.h>

static const char *TAG = "wifi_cli";

static int wifi_set_cli_handler(int argc, char *argv[])
{
    /* Just to go to the next line */
    printf("\n");
    if (argc != 3) {
        printf("%s: Incorrect arguments\n", TAG);
        return 0;
    }

    wifi_config_t wifi_cfg = {
        .sta = {
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
            * However these modes are deprecated and not advisable to be used. Incase your Access point
            * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    snprintf((char*)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", argv[1]);
    snprintf((char*)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s", argv[2]);

    /* Configure WiFi station with provided host credentials */
    if (esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_cfg) != ESP_OK) {
        printf("%s: Failed to set WiFi configuration\n", TAG);
        return 0;
    }

    ESP_LOGI(TAG, "Rebooting with new wi-fi configuration...");
    vTaskDelay(pdMS_TO_TICKS(2 * 1000));
    esp_restart(); // for now we simply re-start the device
    return 0;
}

static esp_console_cmd_t wifi_cmds[] = {
    {
        .command = "wifi-set",
        .help = "wifi-set <ssid> <passphrase>",
        .func = wifi_set_cli_handler,
    }
};

int wifi_register_cli()
{
    int cmds_num = sizeof(wifi_cmds) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", wifi_cmds[i].command);
        esp_console_cmd_register(&wifi_cmds[i]);
    }
    return 0;
}
