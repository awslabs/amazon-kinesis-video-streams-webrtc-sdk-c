/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include <esp_console.h>
#include <esp_log.h>
#include "app_webrtc_if.h"

static const char *TAG = "trigger_offer_cmd";

extern int signaling_bridge_adapter_send_message(webrtc_message_t *signalingMessage);

/**
 * @brief Send trigger offer message via signaling bridge adapter
 *
 * @param peerClientId The peer client ID to send the trigger offer to
 * @return int 0 on success, -1 on error
 */
static int send_trigger_offer_message(const char* peerClientId)
{
    if (peerClientId == NULL || strlen(peerClientId) == 0) {
        ESP_LOGE(TAG, "Invalid peer client ID");
        return -1;
    }

    /* Create webrtc message with TRIGGER_OFFER type */
    webrtc_message_t webrtcMsg = {0};
    webrtcMsg.version = 1;  /* Use version 1 */
    webrtcMsg.message_type = WEBRTC_MESSAGE_TYPE_TRIGGER_OFFER;

    /* Set the peer client ID */
    strncpy(webrtcMsg.peer_client_id, peerClientId, sizeof(webrtcMsg.peer_client_id) - 1);
    webrtcMsg.peer_client_id[sizeof(webrtcMsg.peer_client_id) - 1] = '\0';

    /* No correlation ID needed for trigger offer */
    webrtcMsg.correlation_id[0] = '\0';

    /* No payload needed for trigger offer */
    webrtcMsg.payload = NULL;
    webrtcMsg.payload_len = 0;

    ESP_LOGI(TAG, "Sending TRIGGER_OFFER message to peer: %s", peerClientId);

    /* Send via signaling bridge adapter (handles serialization and bridge communication) */
    int result = signaling_bridge_adapter_send_message(&webrtcMsg);
    if (result != 0) {
        ESP_LOGE(TAG, "Failed to send trigger offer message via bridge adapter");
        return -1;
    }

    ESP_LOGI(TAG, "Successfully sent TRIGGER_OFFER message to streaming device for peer: %s", peerClientId);
    return 0;
}

/**
 * @brief Console command handler for trigger offer
 *
 * @param argc Number of arguments
 * @param argv Array of argument strings
 * @return int 0 on success
 */
static int trigger_offer_cli_handler(int argc, char *argv[])
{
    if (argc != 2) {
        ESP_LOGE(TAG, "Usage: trigger-offer <peer_client_id>");
        return -1;
    }

    const char* peerClientId = argv[1];
    ESP_LOGI(TAG, "Triggering offer for peer: %s", peerClientId);

    int result = send_trigger_offer_message(peerClientId);
    if (result == 0) {
        ESP_LOGI(TAG, "Trigger offer command completed successfully");
    } else {
        ESP_LOGE(TAG, "Trigger offer command failed");
    }

    return result;
}

/* Console command definition */
static esp_console_cmd_t trigger_offer_cmds[] = {
    {
        .command = "trigger-offer",
        .help = "Send trigger offer message to streaming device for specified peer",
        .hint = "<peer_client_id>",
        .func = trigger_offer_cli_handler,
    }
};

/**
 * @brief Register trigger offer commands with the console
 *
 * @return int 0 on success
 */
int trigger_offer_command_register_cli(void)
{
    int cmds_num = sizeof(trigger_offer_cmds) / sizeof(esp_console_cmd_t);
    int i;
    for (i = 0; i < cmds_num; i++) {
        ESP_LOGI(TAG, "Registering command: %s", trigger_offer_cmds[i].command);
        esp_err_t result = esp_console_cmd_register(&trigger_offer_cmds[i]);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register command %s: %d", trigger_offer_cmds[i].command, result);
            return -1;
        }
    }
    return 0;
}
