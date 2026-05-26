/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * CLI command to capture a JPEG snapshot from the streaming device (P4)
 * over the existing webrtc_bridge transport using a small binary framing
 * (1-byte cmd_id + payload). No JSON / signaling_serializer involved.
 *
 * Usage:  query-snapshot [quality]
 *   quality: JPEG quality 1-100 (default: 80)
 *
 * Single in-flight request at a time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <esp_console.h>
#include <esp_log.h>

#include "webrtc_bridge.h"

static const char *TAG = "query_snapshot_cmd";

static uint8_t s_last_quality = 80;

static void snapshot_response_handler(uint8_t cmd_id, const uint8_t *data, size_t len)
{
    (void)cmd_id;

    if (len == 0) {
        ESP_LOGE(TAG, "Snapshot capture failed on streaming device (empty response)");
        return;
    }

    ESP_LOGI(TAG, "Snapshot received: %zu bytes (quality=%u)", len, s_last_quality);
    /* The first 4 bytes of a valid JPEG are FF D8 FF E0 (or similar) */
    if (len >= 4) {
        ESP_LOGI(TAG, "JPEG header: %02X %02X %02X %02X", data[0], data[1], data[2], data[3]);
    }
}

static int query_snapshot_cli_handler(int argc, char *argv[])
{
    uint8_t quality = 80;
    if (argc >= 2) {
        int q = atoi(argv[1]);
        if (q >= 1 && q <= 100) {
            quality = (uint8_t)q;
        } else {
            ESP_LOGW(TAG, "Invalid quality %d, using default %u", q, quality);
        }
    }

    s_last_quality = quality;
    ESP_LOGI(TAG, "Requesting JPEG snapshot from streaming device (quality=%u)...", quality);

    webrtc_bridge_send_binary_cmd(BRIDGE_CMD_SNAPSHOT_REQUEST, &quality, 1);
    printf("Request sent. Snapshot size will be printed on response.\n");
    return 0;
}

static esp_console_cmd_t query_snapshot_cmds[] = {
    {
        .command = "query-snapshot",
        .help = "Capture JPEG snapshot from streaming device. Usage: query-snapshot [quality]",
        .hint = NULL,
        .func = query_snapshot_cli_handler,
    }
};

int query_snapshot_command_register_response_handler(void)
{
    webrtc_bridge_register_binary_handler(BRIDGE_CMD_SNAPSHOT_RESPONSE, snapshot_response_handler);
    return 0;
}

int query_snapshot_command_register_cli(void)
{
    int cmds_num = sizeof(query_snapshot_cmds) / sizeof(esp_console_cmd_t);
    for (int i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", query_snapshot_cmds[i].command);
        esp_err_t result = esp_console_cmd_register(&query_snapshot_cmds[i]);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register command %s: %d",
                     query_snapshot_cmds[i].command, result);
            return -1;
        }
    }
    return 0;
}
