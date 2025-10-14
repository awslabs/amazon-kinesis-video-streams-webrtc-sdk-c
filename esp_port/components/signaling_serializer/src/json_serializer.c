/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "signaling_serializer.h"
#include "json_serializer.h"
#include "serializer_internal.h"
#include "jsmn.h"
#include "base64_compat.h"

static const char *TAG = "json_serializer";

// Helper function to compare JSON token with a string
static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

char* serialize_signaling_message_json(signaling_msg_t *pSignalingMessage, size_t *outLen)
{
    // Calculate base64 encoded payload length
    size_t encodedPayloadLen = 0;
    if (pSignalingMessage->payload != NULL) {
        mbedtls_base64_encode(NULL, 0, &encodedPayloadLen, (unsigned char*)pSignalingMessage->payload, pSignalingMessage->payloadLen);
        ESP_LOGD(TAG, "base64 encoded payload length: %d", (int)encodedPayloadLen);
    }

    // Calculate total JSON string length including base64 payload
    int jsonStringLen = snprintf(NULL, 0,
                               "{\"version\":%d,\"messageType\":%d,\"peerClientId\":\"%s\",\"correlationId\":\"%s\", \"payloadLen\":%d,\"payload\":\"",
                               (int)pSignalingMessage->version,
                               (int)pSignalingMessage->messageType,
                               pSignalingMessage->peerClientId,
                               pSignalingMessage->correlationId,
                               (int)pSignalingMessage->payloadLen) +
                               encodedPayloadLen + 3; // Add 3 for closing ""}

    // Allocate memory for the complete JSON string
    char* jsonString = serializer_memalloc(jsonStringLen);
    if (jsonString == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", (int) jsonStringLen);
        return NULL;
    }
    ESP_LOGD(TAG, "jsonStringLen: %d, payloadLen: %d", (int) jsonStringLen, (int)encodedPayloadLen);

    // Write the JSON prefix
    int written = snprintf(jsonString, jsonStringLen,
             "{\"version\":%d,\"messageType\":%d,\"peerClientId\":\"%s\",\"correlationId\":\"%s\",\"payload\":\"",
             (int)pSignalingMessage->version,
             (int)pSignalingMessage->messageType,
             pSignalingMessage->peerClientId,
             pSignalingMessage->correlationId);

    // Base64 encode the payload if it exists
    if (pSignalingMessage->payload != NULL) {
        ESP_LOGD(TAG, "encoding payload: %.*s", (int)pSignalingMessage->payloadLen, pSignalingMessage->payload);
        size_t remainingLen = jsonStringLen - written;
        size_t actualEncodedLen = 0;
        mbedtls_base64_encode((unsigned char *) (jsonString + written), remainingLen, &actualEncodedLen, (unsigned char*)pSignalingMessage->payload, pSignalingMessage->payloadLen);
        written += actualEncodedLen;
        ESP_LOGD(TAG, "base64 encoded payload length: %d", (int)actualEncodedLen);
    }

    // Add the closing quotes and bracket
    written += snprintf(jsonString + written, jsonStringLen - written, "\"}");

    ESP_LOGD(TAG, "Encoded jsonString: %.*s", written, jsonString);
    *outLen = written;

    return jsonString;
}

esp_err_t deserialize_signaling_message_json(const char* jsonString, size_t jsonStringLen, signaling_msg_t *pSignalingMessage)
{
    esp_err_t retStatus = ESP_OK;
    jsmn_parser parser;
    jsmntok_t* tokens = NULL;
    const int maxTokenCount = 20;
    int tokenCount;

    tokens = serializer_memalloc(maxTokenCount * sizeof(jsmntok_t));
    if (tokens == NULL) {
        ESP_LOGE(TAG, "Failed to allocate %d bytes", (int) (maxTokenCount * sizeof(jsmntok_t)));
        retStatus = ESP_ERR_NO_MEM;
        goto CleanUp;
    }

    jsmn_init(&parser);
    ESP_LOGD(TAG, "jsonString: %.*s", (int) jsonStringLen, jsonString);
    tokenCount = jsmn_parse(&parser, jsonString, jsonStringLen, tokens, maxTokenCount);
    ESP_LOGD(TAG, "tokenCount: %d", tokenCount);
    if(tokenCount <= 1) {
        ESP_LOGE(TAG, "Invalid token count");
        retStatus = ESP_ERR_INVALID_ARG;
        goto CleanUp;
    }

    // Initialize strings to empty
    pSignalingMessage->peerClientId[0] = '\0';
    pSignalingMessage->correlationId[0] = '\0';
    pSignalingMessage->payload = NULL;
    pSignalingMessage->payloadLen = 0;

    for (int i = 1; i < tokenCount; i++) {
        if (jsoneq(jsonString, &tokens[i], "version") == 0) {
            pSignalingMessage->version = atoi(jsonString + tokens[i + 1].start);
            ESP_LOGD(TAG, "version: %d", (int)pSignalingMessage->version);
            i++;
        } else if (jsoneq(jsonString, &tokens[i], "payloadLen") == 0) {
            pSignalingMessage->payloadLen = atoi(jsonString + tokens[i + 1].start);
            ESP_LOGD(TAG, "payloadLen: %d", (int)pSignalingMessage->payloadLen);
            i++;
        } else if (jsoneq(jsonString, &tokens[i], "messageType") == 0) {
            pSignalingMessage->messageType = atoi(jsonString + tokens[i + 1].start);
            ESP_LOGD(TAG, "messageType: %d", pSignalingMessage->messageType);
            i++;
        } else if (jsoneq(jsonString, &tokens[i], "peerClientId") == 0) {
            int len = tokens[i + 1].end - tokens[i + 1].start;
            if (len > 0) {
                strncpy(pSignalingMessage->peerClientId, jsonString + tokens[i + 1].start, len);
                pSignalingMessage->peerClientId[len] = '\0';
            }
            ESP_LOGD(TAG, "peerClientId: %s", pSignalingMessage->peerClientId);
            i++;
        } else if (jsoneq(jsonString, &tokens[i], "correlationId") == 0) {
            int len = tokens[i + 1].end - tokens[i + 1].start;
            if (len > 0) {
                strncpy(pSignalingMessage->correlationId, jsonString + tokens[i + 1].start, len);
                pSignalingMessage->correlationId[len] = '\0';
            }
            ESP_LOGD(TAG, "correlationId: %s", pSignalingMessage->correlationId);
            i++;
        } else if (jsoneq(jsonString, &tokens[i], "payload") == 0) {
            int len = tokens[i + 1].end - tokens[i + 1].start;
            if (len > 0) {
                size_t payloadLen = 0;
                if (pSignalingMessage->payloadLen == 0) {
                    mbedtls_base64_decode(NULL, 0, &payloadLen, (unsigned char*)jsonString + tokens[i + 1].start, len);
                    pSignalingMessage->payloadLen = payloadLen;
                }
                pSignalingMessage->payload = serializer_memalloc(pSignalingMessage->payloadLen + 1);
                if(pSignalingMessage->payload != NULL) {
                    mbedtls_base64_decode((unsigned char *) pSignalingMessage->payload, pSignalingMessage->payloadLen, &payloadLen, (unsigned char*)jsonString + tokens[i + 1].start, len);
                    ESP_LOGD(TAG, "payload: %.*s", (int)payloadLen, pSignalingMessage->payload);
                    pSignalingMessage->payload[payloadLen] = '\0';
                }
            }
            i++;
        }
    }

    ESP_LOGD(TAG, "Decoded payload: %.*s", (int)pSignalingMessage->payloadLen, pSignalingMessage->payload);

CleanUp:
    if (tokens != NULL) {
        free(tokens);
    }
    return retStatus;
}
