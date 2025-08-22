/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include "signaling_conversion.h"
#include "app_webrtc_if.h"

static const char *TAG = "signaling_conversion";

/**
 * @brief Convert AppRTC JSON message to signaling_msg_t format
 *
 * This function parses an AppRTC signaling message in JSON format and
 * converts it to the signaling_msg_t structure used by the WebRTC SDK.
 *
 * @param json_message The AppRTC JSON message to convert
 * @param json_message_len Length of the JSON message
 * @param pSignalingMessage Output signaling message structure
 * @return 0 on success, non-zero on failure
 */
int apprtc_json_to_signaling_message(
    const char *json_message,
    size_t json_message_len,
    webrtc_message_t *pWebrtcMessage
)
{
    int status = 0;

    // Validate parameters
    if (json_message == NULL || pWebrtcMessage == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    // Parse the incoming JSON message
    cJSON *jsonMessage = cJSON_Parse(json_message);
    if (jsonMessage == NULL) {
        ESP_LOGE(TAG, "Failed to parse signaling message as JSON");
        return -1;
    }

    // Extract message type
    cJSON *typeNode = cJSON_GetObjectItem(jsonMessage, "type");
    if (typeNode == NULL || !cJSON_IsString(typeNode)) {
        ESP_LOGE(TAG, "Missing or invalid 'type' field in message");
        cJSON_Delete(jsonMessage);
        return -1;
    }

    const char *messageType = cJSON_GetStringValue(typeNode);
    ESP_LOGI(TAG, "Converting signaling message type: %s", messageType);

    // Initialize the output SignalingMessage
    pWebrtcMessage->version = 0;
    pWebrtcMessage->payload = NULL;
    pWebrtcMessage->payload_len = 0;

    // Extract clientId if present
    cJSON *clientIdNode = cJSON_GetObjectItem(jsonMessage, "clientId");
    if (clientIdNode && cJSON_IsString(clientIdNode)) {
        strncpy(pWebrtcMessage->peer_client_id, cJSON_GetStringValue(clientIdNode), APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN);
        pWebrtcMessage->peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    } else {
        pWebrtcMessage->peer_client_id[0] = '\0';
    }

    // Extract correlationId if present
    cJSON *correlationIdNode = cJSON_GetObjectItem(jsonMessage, "correlationId");
    if (correlationIdNode && cJSON_IsString(correlationIdNode)) {
        strncpy(pWebrtcMessage->correlation_id, cJSON_GetStringValue(correlationIdNode), APP_WEBRTC_MAX_CORRELATION_ID_LEN);
        pWebrtcMessage->correlation_id[APP_WEBRTC_MAX_CORRELATION_ID_LEN] = '\0';
    } else {
        pWebrtcMessage->correlation_id[0] = '\0';
    }

    // Convert message type to KVS WebRTC SDK message type
    if (strcmp(messageType, "offer") == 0) {
        pWebrtcMessage->message_type = WEBRTC_MESSAGE_TYPE_OFFER;
    } else if (strcmp(messageType, "answer") == 0) {
        pWebrtcMessage->message_type = WEBRTC_MESSAGE_TYPE_ANSWER;
    } else if (strcmp(messageType, "iceCandidate") == 0 || strcmp(messageType, "candidate") == 0) {
        pWebrtcMessage->message_type = WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE;
    } else if (strcmp(messageType, "customized") == 0) {
        // Handle customized messages
        cJSON *dataNode = cJSON_GetObjectItem(jsonMessage, "data");
        if (dataNode && cJSON_IsString(dataNode)) {
            const char *data = cJSON_GetStringValue(dataNode);
            pWebrtcMessage->payload_len = (uint32_t) strlen(data);
            pWebrtcMessage->payload = (char*) heap_caps_calloc(1, pWebrtcMessage->payload_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!pWebrtcMessage->payload) {
                ESP_LOGE(TAG, "Failed to allocate memory for custom message");
                cJSON_Delete(jsonMessage);
                return -1;
            }
            strcpy(pWebrtcMessage->payload, data);
        } else {
            ESP_LOGW(TAG, "Invalid custom message format");
            cJSON_Delete(jsonMessage);
            return -1;
        }
    } else {
        ESP_LOGW(TAG, "Unknown message type: %s", messageType);
        cJSON_Delete(jsonMessage);
        return -1;
    }

        // For all message types, store the full JSON message in the payload
    if (pWebrtcMessage->message_type != WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE &&
        strcmp(messageType, "customized") != 0) {
        // Store the full JSON message for non-custom messages
        pWebrtcMessage->payload_len = (uint32_t) json_message_len;
        pWebrtcMessage->payload = (char*) heap_caps_calloc(1, pWebrtcMessage->payload_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (pWebrtcMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for full JSON payload");
            cJSON_Delete(jsonMessage);
            return -1;
        }
        memcpy(pWebrtcMessage->payload, json_message, json_message_len);
        pWebrtcMessage->payload[json_message_len] = '\0';
        ESP_LOGI(TAG, "Using full JSON as payload for %s", messageType);
    }
    // For ICE candidates, also use the full JSON
    else if (pWebrtcMessage->message_type == WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE) {
        pWebrtcMessage->payload_len = (uint32_t) json_message_len;
        pWebrtcMessage->payload = (char*) heap_caps_calloc(1, pWebrtcMessage->payload_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (pWebrtcMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for ICE candidate");
            cJSON_Delete(jsonMessage);
            return -1;
        }
        memcpy(pWebrtcMessage->payload, json_message, json_message_len);
        pWebrtcMessage->payload[json_message_len] = '\0';
        ESP_LOGI(TAG, "Using full JSON as ICE candidate payload");
    }

    // Clean up
    cJSON_Delete(jsonMessage);

    return status;
}

/**
 * @brief Convert signaling_msg_t to AppRTC JSON format
 *
 * This function converts a signaling_msg_t structure to an AppRTC
 * signaling message in JSON format.
 *
 * @param pWebrtcMessage The signaling message to convert
 * @param ppJsonMessage Output JSON message (caller must free)
 * @param pJsonMessageLen Output JSON message length
 * @return 0 on success, non-zero on failure
 */
int signaling_message_to_apprtc_json(webrtc_message_t *pWebrtcMessage,
                                     char **ppJsonMessage, size_t *pJsonMessageLen)
{
    int status = 0;

    // Validate parameters
    if (pWebrtcMessage == NULL || ppJsonMessage == NULL || pJsonMessageLen == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    // Check if the payload is already a JSON message
    if (pWebrtcMessage->payload != NULL && pWebrtcMessage->payload_len > 0 &&
        pWebrtcMessage->payload[0] == '{') {
        ESP_LOGI(TAG, "Payload appears to be a JSON message already, using as-is");

        // Just duplicate the payload
        *ppJsonMessage = heap_caps_calloc(1, pWebrtcMessage->payload_len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (*ppJsonMessage == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for JSON message");
            return -1;
        }
        memcpy(*ppJsonMessage, pWebrtcMessage->payload, pWebrtcMessage->payload_len);
        *pJsonMessageLen = pWebrtcMessage->payload_len;
        return 0;
    }

    // Create the JSON message
    cJSON *jsonMessage = cJSON_CreateObject();
    if (jsonMessage == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return -1;
    }

    // Add client ID if provided
    if (pWebrtcMessage->peer_client_id[0] != '\0') {
        cJSON_AddStringToObject(jsonMessage, "clientId", pWebrtcMessage->peer_client_id);
    }

    // Convert message type and add appropriate fields
    switch (pWebrtcMessage->message_type) {
        case WEBRTC_MESSAGE_TYPE_OFFER:
            cJSON_AddStringToObject(jsonMessage, "type", "offer");
            if (pWebrtcMessage->payload != NULL && pWebrtcMessage->payload_len > 0) {
                cJSON_AddStringToObject(jsonMessage, "sdp", pWebrtcMessage->payload);
                ESP_LOGI(TAG, "Converting offer with SDP: %s", pWebrtcMessage->payload);
            } else {
                ESP_LOGW(TAG, "No SDP payload in offer message");
            }
            break;

        case WEBRTC_MESSAGE_TYPE_ANSWER:
            cJSON_AddStringToObject(jsonMessage, "type", "answer");
            if (pWebrtcMessage->payload != NULL && pWebrtcMessage->payload_len > 0) {
                cJSON_AddStringToObject(jsonMessage, "sdp", pWebrtcMessage->payload);
                ESP_LOGI(TAG, "Converting answer with SDP: %s", pWebrtcMessage->payload);
            } else {
                ESP_LOGW(TAG, "No SDP payload in answer message");
            }
            break;

        case WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE:
            cJSON_AddStringToObject(jsonMessage, "type", "iceCandidate");
            if (pWebrtcMessage->payload != NULL && pWebrtcMessage->payload_len > 0) {
                cJSON_AddStringToObject(jsonMessage, "candidate", pWebrtcMessage->payload);
                ESP_LOGI(TAG, "Converting ICE candidate: %s", pWebrtcMessage->payload);
            } else {
                ESP_LOGW(TAG, "No candidate payload in ICE candidate message");
            }
            break;

        default:
            ESP_LOGE(TAG, "Unsupported message type: %d", pWebrtcMessage->message_type);
            cJSON_Delete(jsonMessage);
            return -1;
    }

    // Add correlation ID if present
    if (pWebrtcMessage->correlation_id[0] != '\0') {
        cJSON_AddStringToObject(jsonMessage, "correlationId", pWebrtcMessage->correlation_id);
    }

    // Serialize to string
    char *jsonString = cJSON_PrintUnformatted(jsonMessage);
    if (jsonString == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON message");
        cJSON_Delete(jsonMessage);
        return -1;
    }

    // Set output parameters
    *ppJsonMessage = jsonString;
    *pJsonMessageLen = strlen(jsonString);

    // Clean up
    cJSON_Delete(jsonMessage);

    return status;
}
