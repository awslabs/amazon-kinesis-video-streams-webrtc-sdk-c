/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "signaling_serializer.h"
#include "raw_serializer.h"
#include "serializer_internal.h"

static const char *TAG = "RawSerializer";

char* serialize_signaling_message_raw(signaling_msg_t *pSignalingMessage, size_t* outLen)
{
    char* serialized = NULL;

    if (pSignalingMessage == NULL || outLen == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    // Calculate total size needed
    *outLen = sizeof(signaling_msg_t) + pSignalingMessage->payloadLen;

    // Allocate buffer
    serialized = serializer_memalloc(*outLen);
    if (serialized == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", (int) *outLen);
        return NULL;
    }

    // Copy the SignalingMsg structure
    memcpy(serialized, pSignalingMessage, sizeof(signaling_msg_t));

    // Copy the payload if present
    if (pSignalingMessage->payload != NULL && pSignalingMessage->payloadLen > 0) {
        memcpy(serialized + sizeof(signaling_msg_t),
               pSignalingMessage->payload,
               pSignalingMessage->payloadLen);
    }

    ESP_LOGD(TAG, "Serialized message of size %d bytes", (int) *outLen);
    return serialized;
}

esp_err_t deserialize_signaling_message_raw(const char* data, size_t len, signaling_msg_t *pSignalingMessage)
{
    esp_err_t retStatus = ESP_OK;

    if(data == NULL || pSignalingMessage == NULL) {
        ESP_LOGE(TAG, "Invalid arguments");
        retStatus = ESP_ERR_INVALID_ARG;
        goto CleanUp;
    }

    // Validate minimum size
    if(len < sizeof(signaling_msg_t)) {
        ESP_LOGE(TAG, "Invalid message size");
        retStatus = ESP_ERR_INVALID_ARG;
        goto CleanUp;
    }

    // Copy the SignalingMsg structure
    memcpy(pSignalingMessage, data, sizeof(signaling_msg_t));

    // Calculate payload size
    size_t payloadLen = len - sizeof(signaling_msg_t);

    // Validate payload length matches the structure
    if(payloadLen != pSignalingMessage->payloadLen) {
        ESP_LOGE(TAG, "Invalid payload length");
        retStatus = ESP_ERR_INVALID_ARG;
        goto CleanUp;
    }

    if (payloadLen > 0) {
        // Allocate and copy payload
        pSignalingMessage->payload = serializer_memalloc(payloadLen);
        if (pSignalingMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate %d bytes", (int) payloadLen);
            retStatus = ESP_ERR_NO_MEM;
            goto CleanUp;
        }

        memcpy(pSignalingMessage->payload,
               data + sizeof(signaling_msg_t),
               payloadLen);
    } else {
        pSignalingMessage->payload = NULL;
    }

    ESP_LOGD(TAG, "Deserialized message with %d bytes payload", (int) payloadLen);

CleanUp:
    if (retStatus != ESP_OK) {
        ESP_LOGE(TAG, "Failed to deserialize message with status %d", (int) retStatus);
        if (pSignalingMessage != NULL) {
            if (pSignalingMessage->payload != NULL) {
                free(pSignalingMessage->payload);
                pSignalingMessage->payload = NULL;
            }
            pSignalingMessage->payloadLen = 0;
        }
    }
    return retStatus;
}
