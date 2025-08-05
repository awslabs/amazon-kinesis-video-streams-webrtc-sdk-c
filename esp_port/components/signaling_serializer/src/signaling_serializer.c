/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#if defined(LINUX_BUILD) || defined(KVS_PLAT_LINUX_UNIX)
#include <stdio.h>
#define ESP_LOGE(tag, format, ...) printf("[ERROR] " format "\n", ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) printf("[INFO] " format "\n", ##__VA_ARGS__)
#else
#include "esp_log.h"
#endif

#include "signaling_serializer.h"
#include "json_serializer.h"
#include "protobuf_serializer.h"
#include "raw_serializer.h"
#include "serializer_internal.h"

static const char *TAG = "signaling_serializer";

typedef char* (*SerializeFunc)(signaling_msg_t *pSignalingMessage, size_t* outLen);
typedef esp_err_t (*DeserializeFunc)(const char* data, size_t len, signaling_msg_t *pSignalingMessage);

static SerializeFunc gSerializeFunc = NULL;
static DeserializeFunc gDeserializeFunc = NULL;

void signaling_serializer_init(void) {
#ifdef CONFIG_KVS_USE_PROTOBUF_SERIALIZATION
    gSerializeFunc = serialize_signaling_message_protobuf;
    gDeserializeFunc = deserialize_signaling_message_protobuf;
#elif CONFIG_KVS_USE_JSON_SERIALIZATION
    gSerializeFunc = serialize_signaling_message_json;
    gDeserializeFunc = deserialize_signaling_message_json;
#else
    gSerializeFunc = serialize_signaling_message_raw;
    gDeserializeFunc = deserialize_signaling_message_raw;
#endif
}

char* serialize_signaling_message(signaling_msg_t *pSignalingMessage, size_t* outLen) {
    if (gSerializeFunc == NULL) {
        ESP_LOGE(TAG, "Serializer not initialized");
        return NULL;
    }
    return gSerializeFunc(pSignalingMessage, outLen);
}

esp_err_t deserialize_signaling_message(const char* data, size_t len, signaling_msg_t *pSignalingMessage) {
    if (gDeserializeFunc == NULL) {
        ESP_LOGE(TAG, "Deserializer not initialized");
        return ESP_ERR_INVALID_ARG;
    }
    return gDeserializeFunc(data, len, pSignalingMessage);
}

/**
 * @brief Create ICE servers message from RtcConfiguration
 */
esp_err_t create_ice_servers_message(const void* pRtcConfiguration, uint32_t ice_server_count, signaling_msg_t* pSignalingMessage) {
    if (pRtcConfiguration == NULL || pSignalingMessage == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for create_ice_servers_message");
        return ESP_ERR_INVALID_ARG;
    }

    // Temporary workaround: Use void* to avoid including the main SDK headers
    // We'll assume the structure layout: RtcIceServer iceServers[MAX_ICE_SERVERS_COUNT]
    // Each RtcIceServer has: urls[128], username[257], credential[257]
    const char* ice_servers_ptr = (const char*)pRtcConfiguration;

    // Calculate size of one RtcIceServer (128 + 257 + 257 = 642 bytes)
    const size_t rtc_ice_server_size = (SS_MAX_ICE_CONFIG_URI_LEN + 1) +
                                       (SS_MAX_ICE_CONFIG_USER_NAME_LEN + 1) +
                                       (SS_MAX_ICE_CONFIG_CREDENTIAL_LEN + 1);

    // Create ICE servers payload
    ss_ice_servers_payload_t* payload = (ss_ice_servers_payload_t*)malloc(sizeof(ss_ice_servers_payload_t));
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ICE servers payload");
        return ESP_ERR_NO_MEM;
    }

    // Limit to our maximum
    payload->ice_server_count = (ice_server_count > SS_MAX_ICE_SERVERS_COUNT) ? SS_MAX_ICE_SERVERS_COUNT : ice_server_count;

    // Copy ICE servers data
    for (uint32_t i = 0; i < payload->ice_server_count; i++) {
        const char* src_server = ice_servers_ptr + (i * rtc_ice_server_size);

        // Copy urls (first field)
        strncpy(payload->ice_servers[i].urls, src_server, SS_MAX_ICE_CONFIG_URI_LEN);
        payload->ice_servers[i].urls[SS_MAX_ICE_CONFIG_URI_LEN] = '\0';

        // Copy username (second field)
        strncpy(payload->ice_servers[i].username, src_server + (SS_MAX_ICE_CONFIG_URI_LEN + 1), SS_MAX_ICE_CONFIG_USER_NAME_LEN);
        payload->ice_servers[i].username[SS_MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';

        // Copy credential (third field)
        strncpy(payload->ice_servers[i].credential, src_server + (SS_MAX_ICE_CONFIG_URI_LEN + 1) + (SS_MAX_ICE_CONFIG_USER_NAME_LEN + 1), SS_MAX_ICE_CONFIG_CREDENTIAL_LEN);
        payload->ice_servers[i].credential[SS_MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';

        ESP_LOGI(TAG, "ICE Server %d: %s (user: %s)", i, payload->ice_servers[i].urls, payload->ice_servers[i].username);
    }

    // Setup signaling message
    memset(pSignalingMessage, 0, sizeof(signaling_msg_t));
    pSignalingMessage->version = 1;
    pSignalingMessage->messageType = SIGNALING_MSG_TYPE_ICE_SERVERS;
    strcpy(pSignalingMessage->correlationId, "ice-servers-config");
    strcpy(pSignalingMessage->peerClientId, "signaling-device");
    pSignalingMessage->payload = (char*)payload;
    pSignalingMessage->payloadLen = sizeof(ss_ice_servers_payload_t);

    ESP_LOGI(TAG, "Created ICE servers message with %d servers", payload->ice_server_count);
    return ESP_OK;
}

/**
 * @brief Extract ICE servers from signaling message
 */
esp_err_t extract_ice_servers_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_servers_payload_t* pIceServersPayload) {
    if (pSignalingMessage == NULL || pIceServersPayload == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for extract_ice_servers_from_message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->messageType != SIGNALING_MSG_TYPE_ICE_SERVERS) {
        ESP_LOGE(TAG, "Message is not an ICE servers message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->payload == NULL || pSignalingMessage->payloadLen != sizeof(ss_ice_servers_payload_t)) {
        ESP_LOGE(TAG, "Invalid ICE servers payload");
        return ESP_ERR_INVALID_ARG;
    }

    // Copy the payload
    memcpy(pIceServersPayload, pSignalingMessage->payload, sizeof(ss_ice_servers_payload_t));

    ESP_LOGI(TAG, "Extracted %d ICE servers from message", pIceServersPayload->ice_server_count);
    return ESP_OK;
}

/**
 * @brief Create ICE request message to ask signaling device for ICE server by index
 */
esp_err_t create_ice_request_message(uint32_t index, bool use_turn, signaling_msg_t* pSignalingMessage)
{
    if (pSignalingMessage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create request payload
    ss_ice_request_payload_t* payload = (ss_ice_request_payload_t*)malloc(sizeof(ss_ice_request_payload_t));
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ICE request payload");
        return ESP_ERR_NO_MEM;
    }

    payload->index = index;
    payload->use_turn = use_turn;

    // Initialize the message structure
    memset(pSignalingMessage, 0, sizeof(signaling_msg_t));

    // Set message type to ICE request
    pSignalingMessage->messageType = SIGNALING_MSG_TYPE_ICE_REQUEST;

    // Set correlation ID for tracking
    snprintf(pSignalingMessage->correlationId, sizeof(pSignalingMessage->correlationId), "ice-req-%" PRIu32, index);

    // Set sender ID
    strncpy(pSignalingMessage->peerClientId, "streaming-device", sizeof(pSignalingMessage->peerClientId) - 1);

    // Set payload
    pSignalingMessage->payload = (char*)payload;
    pSignalingMessage->payloadLen = sizeof(ss_ice_request_payload_t);

    ESP_LOGI(TAG, "Created ICE request message for index %d (use_turn: %s)", index, use_turn ? "true" : "false");
    return ESP_OK;
}

/**
 * @brief Extract ICE request from signaling message
 */
esp_err_t extract_ice_request_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_request_payload_t* pIceRequestPayload)
{
    if (pSignalingMessage == NULL || pIceRequestPayload == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for extract_ice_request_from_message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->messageType != SIGNALING_MSG_TYPE_ICE_REQUEST) {
        ESP_LOGE(TAG, "Message is not an ICE request message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->payload == NULL || pSignalingMessage->payloadLen != sizeof(ss_ice_request_payload_t)) {
        ESP_LOGE(TAG, "Invalid ICE request payload");
        return ESP_ERR_INVALID_ARG;
    }

    // Copy the payload
    memcpy(pIceRequestPayload, pSignalingMessage->payload, sizeof(ss_ice_request_payload_t));

    ESP_LOGI(TAG, "Extracted ICE request: index=%d, use_turn=%s",
             pIceRequestPayload->index, pIceRequestPayload->use_turn ? "true" : "false");
    return ESP_OK;
}

/**
 * @brief Create ICE server response message
 */
esp_err_t create_ice_server_response_message(const void* pIceServer, bool have_more, signaling_msg_t* pSignalingMessage)
{
    if (pSignalingMessage == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create response payload
    ss_ice_server_response_t* payload = (ss_ice_server_response_t*)malloc(sizeof(ss_ice_server_response_t));
    if (payload == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ICE server response payload");
        return ESP_ERR_NO_MEM;
    }

    memset(payload, 0, sizeof(ss_ice_server_response_t));
    payload->have_more = have_more;

    if (pIceServer != NULL) {
        // Assume pIceServer is RtcIceServer structure
        const char* ice_server_ptr = (const char*)pIceServer;

        // Copy URLs, username, credential from RtcIceServer structure
        // RtcIceServer layout: urls[128], username[257], credential[257]
        strncpy(payload->urls, ice_server_ptr, SS_MAX_ICE_CONFIG_URI_LEN);
        payload->urls[SS_MAX_ICE_CONFIG_URI_LEN] = '\0';

        strncpy(payload->username, ice_server_ptr + (SS_MAX_ICE_CONFIG_URI_LEN + 1), SS_MAX_ICE_CONFIG_USER_NAME_LEN);
        payload->username[SS_MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';

        strncpy(payload->credential, ice_server_ptr + (SS_MAX_ICE_CONFIG_URI_LEN + 1) + (SS_MAX_ICE_CONFIG_USER_NAME_LEN + 1), SS_MAX_ICE_CONFIG_CREDENTIAL_LEN);
        payload->credential[SS_MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';

        ESP_LOGI(TAG, "ICE Server Response: %s (user: %s) [have_more: %s]",
                 payload->urls, payload->username[0] ? payload->username : "none", have_more ? "true" : "false");
    }

    // Initialize the message structure
    memset(pSignalingMessage, 0, sizeof(signaling_msg_t));

    // Set message type to ICE server response
    pSignalingMessage->messageType = SIGNALING_MSG_TYPE_ICE_SERVER_RESPONSE;

    // Set correlation ID for tracking
    strncpy(pSignalingMessage->correlationId, "ice-response", sizeof(pSignalingMessage->correlationId) - 1);

    // Set sender ID
    strncpy(pSignalingMessage->peerClientId, "signaling-device", sizeof(pSignalingMessage->peerClientId) - 1);

    // Set payload
    pSignalingMessage->payload = (char*)payload;
    pSignalingMessage->payloadLen = sizeof(ss_ice_server_response_t);

    ESP_LOGI(TAG, "Created ICE server response message (have_more: %s)", have_more ? "true" : "false");
    return ESP_OK;
}

/**
 * @brief Extract ICE server response from signaling message
 */
esp_err_t extract_ice_server_response_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_server_response_t* pIceServerResponse)
{
    if (pSignalingMessage == NULL || pIceServerResponse == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for extract_ice_server_response_from_message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->messageType != SIGNALING_MSG_TYPE_ICE_SERVER_RESPONSE) {
        ESP_LOGE(TAG, "Message is not an ICE server response message");
        return ESP_ERR_INVALID_ARG;
    }

    if (pSignalingMessage->payload == NULL || pSignalingMessage->payloadLen != sizeof(ss_ice_server_response_t)) {
        ESP_LOGE(TAG, "Invalid ICE server response payload");
        return ESP_ERR_INVALID_ARG;
    }

    // Copy the payload
    memcpy(pIceServerResponse, pSignalingMessage->payload, sizeof(ss_ice_server_response_t));

    ESP_LOGI(TAG, "Extracted ICE server response: %s (have_more: %s)",
             pIceServerResponse->urls, pIceServerResponse->have_more ? "true" : "false");
    return ESP_OK;
}
