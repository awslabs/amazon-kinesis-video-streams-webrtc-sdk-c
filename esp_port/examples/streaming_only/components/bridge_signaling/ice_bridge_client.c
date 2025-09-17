/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Implementation of ICE server bridge client for streaming-only mode
 *
 * This component requests ICE servers from the signaling device via bridge
 * communication using an index-based request-response pattern.
 */

#include "ice_bridge_client.h"
#include "signaling_serializer.h"
#include "webrtc_bridge.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#if CONFIG_ESP_HOSTED_ENABLED
#include "rpc_wrap.h"
#endif

#define LOG_CLASS "ice_bridge_client"
static const char *TAG = "ice_bridge_client";

#define ICE_REQUEST_TIMEOUT_MS 5000  // 5 second total timeout (adaptive per-attempt)

// Constants from main SDK (to avoid header dependency)
#define MAX_ICE_SERVERS_COUNT 4
#define MAX_ICE_CONFIG_URI_LEN 127
#define MAX_ICE_CONFIG_USER_NAME_LEN 256
#define MAX_ICE_CONFIG_CREDENTIAL_LEN 256

// Global variables for ICE server response handling (single server)
// These will be set by bridge_signaling.c when ICE responses are received
static ss_ice_server_response_t g_received_ice_server = {0};
static bool g_ice_servers_received = false;

/**
 * @brief Callback function for ICE server responses from bridge_signaling
 * This is called from bridge_signaling.c when SIGNALING_MSG_TYPE_ICE_SERVER_RESPONSE is received
 */
void ice_bridge_client_set_ice_server_response(const ss_ice_server_response_t* ice_server_response)
{
    ESP_LOGI(TAG, "ice_bridge_client_set_ice_server_response called!");

    if (ice_server_response != NULL) {
        memcpy(&g_received_ice_server, ice_server_response, sizeof(ss_ice_server_response_t));
        g_ice_servers_received = true;
        ESP_LOGI(TAG, "Set g_ice_servers_received = true! Response: %s (have_more: %s)",
                 g_received_ice_server.urls, g_received_ice_server.have_more ? "true" : "false");
    } else {
        ESP_LOGE(TAG, "ice_bridge_client_set_ice_server_response called with NULL response!");
    }
}

/*
 * For streaming-only mode, we request ICE servers from the signaling device via bridge
 * Using synchronous RPC calls to avoid queue delays
 */
WEBRTC_STATUS ice_bridge_client_get_servers(void* pAppSignaling, void* pIceServers, uint32_t* pServerNum)
{
    WEBRTC_STATUS retStatus = WEBRTC_STATUS_SUCCESS;
    uint32_t uriCount = 0;
    uint32_t max_tries = 10;
    bool have_more = true;

    // pAppSignaling is unused in streaming-only mode
    (void)pAppSignaling;

    if (pIceServers == NULL || pServerNum == NULL) {
        return WEBRTC_STATUS_NULL_ARG;
    }

    // Cast to RtcIceServer array (we know the layout matches ss_ice_server_t)
    ss_ice_server_t* ice_servers_array = (ss_ice_server_t*)pIceServers;

    ESP_LOGI(TAG, "Requesting ICE servers via synchronous RPC (bypasses event queue!)");

#if CONFIG_ESP_HOSTED_ENABLED
    // Use synchronous RPC calls - this completely bypasses the async event queue!
    for (int i = 0; i < max_tries; i++) {
        if (uriCount >= MAX_ICE_SERVERS_COUNT || !have_more) {
            ESP_LOGW(TAG, "Done or Max ICE URI count reached (try %d/%d)", i, (int) max_tries);
            break;
        }

        ESP_LOGI(TAG, "RPC: Requesting ICE server at index %d (try %d/%d)", (int) uriCount, i, (int) max_tries);

        // Prepare RPC request
        rpc_usr_t req = {0};
        rpc_usr_t resp = {0};

        req.int_1 = (int32_t)uriCount; // ICE server index
        req.int_2 = 1; // use_turn = true
        req.uint_1 = 0; // unused
        req.uint_2 = 0; // unused
        req.data_len = 0; // No additional data in request

        // Send synchronous RPC request (this blocks until response arrives!)
        esp_err_t rpc_result = rpc_send_usr_request(3, &req, &resp); // usr_req_num = 3 for RPC_ID__Req_USR3

        if (rpc_result != ESP_OK) {
            ESP_LOGE(TAG, "RPC request failed: %d", rpc_result);
            retStatus = WEBRTC_STATUS_INTERNAL_ERROR;
            break;
        }
        have_more = resp.uint_1;

        // Parse response
        if (resp.data_len > 0) {
            // The response data is serialized ss_ice_server_response_t - deserialize it
            ss_ice_server_response_t ice_response = {0};

            // For now, assume the data is directly the structure (since it's same-device RPC)
            if (resp.data_len >= sizeof(ss_ice_server_t)) {
                memcpy(&ice_response, resp.data, sizeof(ss_ice_server_t));

                // Copy the ICE server data
                strncpy(ice_servers_array[uriCount].urls, ice_response.urls, MAX_ICE_CONFIG_URI_LEN);
                ice_servers_array[uriCount].urls[MAX_ICE_CONFIG_URI_LEN] = '\0';

                strncpy(ice_servers_array[uriCount].username, ice_response.username, MAX_ICE_CONFIG_USER_NAME_LEN);
                ice_servers_array[uriCount].username[MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';

                strncpy(ice_servers_array[uriCount].credential, ice_response.credential, MAX_ICE_CONFIG_CREDENTIAL_LEN);
                ice_servers_array[uriCount].credential[MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';

                ESP_LOGI(TAG, "RPC: Received ICE server %" PRIu32 ": %s (user: %s, have_more: %s)",
                         uriCount, ice_servers_array[uriCount].urls,
                         ice_servers_array[uriCount].username[0] ? ice_servers_array[uriCount].username : "none",
                         have_more ? "true" : "false");
                uriCount++;
            } else {
                ESP_LOGE(TAG, "RPC: Invalid response data size: %d (expected: %d)", resp.data_len, sizeof(ss_ice_server_t));
                have_more = false;
            }
            if (have_more) {
                ESP_LOGI(TAG, "More ICE servers available, requesting next index %" PRIu32, uriCount);
            } else {
                ESP_LOGI(TAG, "No more ICE servers, completed with %" PRIu32 " servers", uriCount);
            }
        } else if (resp.data_len == 0 && have_more) {
            ESP_LOGI(TAG, "RPC: ICE refresh in progress for index %" PRIu32, uriCount);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

#else
    ESP_LOGW(TAG, "RPC not available, falling back to STUN only");
#endif

    // Fallback if no servers received
    if (uriCount == 0) {
        ESP_LOGW(TAG, "No servers received from signaling device - using fallback STUN");
        snprintf(ice_servers_array[0].urls, MAX_ICE_CONFIG_URI_LEN, "stun:stun.l.google.com:19302");
        ice_servers_array[0].urls[MAX_ICE_CONFIG_URI_LEN] = '\0';
        ice_servers_array[0].username[0] = '\0';
        ice_servers_array[0].credential[0] = '\0';
        uriCount = 1;
    }

    *pServerNum = uriCount;
    ESP_LOGI(TAG, "RPC: Successfully configured %" PRIu32 " ICE servers via synchronous calls!", uriCount);

    return retStatus;
}
