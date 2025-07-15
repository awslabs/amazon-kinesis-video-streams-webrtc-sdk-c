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
#include "signaling_serializer.h"

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
    signaling_msg_t *pSignalingMessage
)
{
    int status = 0;

    // Validate parameters
    if (json_message == NULL || pSignalingMessage == NULL) {
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
    pSignalingMessage->version = 0;
    pSignalingMessage->payload = NULL;
    pSignalingMessage->payloadLen = 0;

    // Extract clientId if present
    cJSON *clientIdNode = cJSON_GetObjectItem(jsonMessage, "clientId");
    if (clientIdNode && cJSON_IsString(clientIdNode)) {
        strncpy(pSignalingMessage->peerClientId, cJSON_GetStringValue(clientIdNode), SS_MAX_SIGNALING_CLIENT_ID_LEN);
        pSignalingMessage->peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
    } else {
        pSignalingMessage->peerClientId[0] = '\0';
    }

    // Extract correlationId if present
    cJSON *correlationIdNode = cJSON_GetObjectItem(jsonMessage, "correlationId");
    if (correlationIdNode && cJSON_IsString(correlationIdNode)) {
        strncpy(pSignalingMessage->correlationId, cJSON_GetStringValue(correlationIdNode), SS_MAX_CORRELATION_ID_LEN);
        pSignalingMessage->correlationId[SS_MAX_CORRELATION_ID_LEN] = '\0';
    } else {
        pSignalingMessage->correlationId[0] = '\0';
    }

    // Convert message type to KVS WebRTC SDK message type
    if (strcmp(messageType, "offer") == 0) {
        pSignalingMessage->messageType = SIGNALING_MSG_TYPE_OFFER;
    } else if (strcmp(messageType, "answer") == 0) {
        pSignalingMessage->messageType = SIGNALING_MSG_TYPE_ANSWER;
    } else if (strcmp(messageType, "iceCandidate") == 0 || strcmp(messageType, "candidate") == 0) {
        pSignalingMessage->messageType = SIGNALING_MSG_TYPE_ICE_CANDIDATE;
    } else if (strcmp(messageType, "customized") == 0) {
        // Handle customized messages
        cJSON *dataNode = cJSON_GetObjectItem(jsonMessage, "data");
        if (dataNode && cJSON_IsString(dataNode)) {
            const char *data = cJSON_GetStringValue(dataNode);
            pSignalingMessage->payloadLen = (uint32_t) strlen(data);
            pSignalingMessage->payload = (char*) heap_caps_calloc(1, pSignalingMessage->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!pSignalingMessage->payload) {
                ESP_LOGE(TAG, "Failed to allocate memory for custom message");
                cJSON_Delete(jsonMessage);
                return -1;
            }
            strcpy(pSignalingMessage->payload, data);
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

    // Store the full JSON message in the payload for non-custom messages
    if (pSignalingMessage->messageType != SIGNALING_MSG_TYPE_ICE_CANDIDATE &&
        strcmp(messageType, "customized") != 0) {
        // Use the original full JSON message as the payload
        pSignalingMessage->payloadLen = (uint32_t) json_message_len;
        pSignalingMessage->payload = (char*) heap_caps_calloc(1, pSignalingMessage->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (pSignalingMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for full JSON payload");
            cJSON_Delete(jsonMessage);
            return -1;
        }
        memcpy(pSignalingMessage->payload, json_message, json_message_len);
        pSignalingMessage->payload[json_message_len] = '\0';
        ESP_LOGI(TAG, "Using full JSON as payload: %s", pSignalingMessage->payload);
    }
    // For ICE candidates, also use the full JSON
    else if (pSignalingMessage->messageType == SIGNALING_MSG_TYPE_ICE_CANDIDATE) {
        pSignalingMessage->payloadLen = (uint32_t) json_message_len;
        pSignalingMessage->payload = (char*) heap_caps_calloc(1, pSignalingMessage->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (pSignalingMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for ICE candidate");
            cJSON_Delete(jsonMessage);
            return -1;
        }
        memcpy(pSignalingMessage->payload, json_message, json_message_len);
        pSignalingMessage->payload[json_message_len] = '\0';
        ESP_LOGI(TAG, "Using full JSON as ICE candidate payload: %s", pSignalingMessage->payload);
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
 * @param pSignalingMessage The signaling message to convert
 * @param ppJsonMessage Output JSON message (caller must free)
 * @param pJsonMessageLen Output JSON message length
 * @return 0 on success, non-zero on failure
 */
int signaling_message_to_apprtc_json(signaling_msg_t *pSignalingMessage,
                                        char **ppJsonMessage, size_t *pJsonMessageLen)
{
    int status = 0;

    // Validate parameters
    if (pSignalingMessage == NULL || ppJsonMessage == NULL || pJsonMessageLen == NULL) {
        ESP_LOGE(TAG, "Invalid parameters");
        return -1;
    }

    // Check if the payload is already a JSON message
    if (pSignalingMessage->payload != NULL && pSignalingMessage->payloadLen > 0 &&
        pSignalingMessage->payload[0] == '{') {
        ESP_LOGI(TAG, "Payload appears to be a JSON message already, using as-is");

        // Just duplicate the payload
        *ppJsonMessage = heap_caps_calloc(1, pSignalingMessage->payloadLen + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (*ppJsonMessage == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for JSON message");
            return -1;
        }
        memcpy(*ppJsonMessage, pSignalingMessage->payload, pSignalingMessage->payloadLen);
        *pJsonMessageLen = pSignalingMessage->payloadLen;
        return 0;
    }

    // Create the JSON message
    cJSON *jsonMessage = cJSON_CreateObject();
    if (jsonMessage == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return -1;
    }

    // Add client ID if provided
    if (pSignalingMessage->peerClientId[0] != '\0') {
        cJSON_AddStringToObject(jsonMessage, "clientId", pSignalingMessage->peerClientId);
    }

    // Convert message type and add appropriate fields
    switch (pSignalingMessage->messageType) {
        case SIGNALING_MSG_TYPE_OFFER:
            cJSON_AddStringToObject(jsonMessage, "type", "offer");
            if (pSignalingMessage->payload != NULL && pSignalingMessage->payloadLen > 0) {
                cJSON_AddStringToObject(jsonMessage, "sdp", pSignalingMessage->payload);
                ESP_LOGI(TAG, "Converting offer with SDP: %s", pSignalingMessage->payload);
            } else {
                ESP_LOGW(TAG, "No SDP payload in offer message");
            }
            break;

        case SIGNALING_MSG_TYPE_ANSWER:
            cJSON_AddStringToObject(jsonMessage, "type", "answer");
            if (pSignalingMessage->payload != NULL && pSignalingMessage->payloadLen > 0) {
                cJSON_AddStringToObject(jsonMessage, "sdp", pSignalingMessage->payload);
                ESP_LOGI(TAG, "Converting answer with SDP: %s", pSignalingMessage->payload);
            } else {
                ESP_LOGW(TAG, "No SDP payload in answer message");
            }
            break;

        case SIGNALING_MSG_TYPE_ICE_CANDIDATE:
            cJSON_AddStringToObject(jsonMessage, "type", "iceCandidate");
            if (pSignalingMessage->payload != NULL && pSignalingMessage->payloadLen > 0) {
                cJSON_AddStringToObject(jsonMessage, "candidate", pSignalingMessage->payload);
                ESP_LOGI(TAG, "Converting ICE candidate: %s", pSignalingMessage->payload);
            } else {
                ESP_LOGW(TAG, "No candidate payload in ICE candidate message");
            }
            break;

        default:
            ESP_LOGE(TAG, "Unsupported message type: %d", pSignalingMessage->messageType);
            cJSON_Delete(jsonMessage);
            return -1;
    }

    // Add correlation ID if present
    if (pSignalingMessage->correlationId[0] != '\0') {
        cJSON_AddStringToObject(jsonMessage, "correlationId", pSignalingMessage->correlationId);
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
