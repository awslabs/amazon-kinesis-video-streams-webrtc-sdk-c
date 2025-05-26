/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include "signaling_serializer.h"
#include "protobuf_serializer.h"
#include "signaling.pb-c.h"
#include "serializer_internal.h"

static const char *TAG = "protobuf_serializer";

char* serialize_signaling_message_protobuf(signaling_msg_t *pSignalingMessage, size_t* outLen)
{
    Kvs__Signaling__SignalingMessage msg = KVS__SIGNALING__SIGNALING_MESSAGE__INIT;

    // Pack the message
    msg.version = pSignalingMessage->version;
    msg.messagetype = pSignalingMessage->messageType;
    msg.peerclientid = pSignalingMessage->peerClientId;
    msg.correlationid = pSignalingMessage->correlationId;

    // Handle payload
    msg.payload.data = (uint8_t*)pSignalingMessage->payload;
    msg.payload.len = pSignalingMessage->payloadLen;
    msg.payloadlen = pSignalingMessage->payloadLen;

    // Get serialized size
    size_t len = kvs__signaling__signaling_message__get_packed_size(&msg);
    *outLen = len;

    // Allocate buffer for serialized data
    char* buffer = serializer_memalloc(len);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer for size %d", (int) len);
        return NULL;
    }

    // Pack message into buffer
    size_t packed_size = kvs__signaling__signaling_message__pack(&msg, (uint8_t*)buffer);
    if (packed_size != len) {
        ESP_LOGE(TAG, "Packed size mismatch: expected %d, got %d", (int) len, (int) packed_size);
        free(buffer);
        return NULL;
    }

    return buffer;
}

esp_err_t deserialize_signaling_message_protobuf(const char* data, size_t len, signaling_msg_t *pSignalingMessage)
{
    esp_err_t retStatus = ESP_OK;

    // Unpack the message
    Kvs__Signaling__SignalingMessage* msg =
        kvs__signaling__signaling_message__unpack(NULL, len, (uint8_t*)data);

    if (msg == NULL) {
        ESP_LOGE(TAG, "Failed to unpack message");
        retStatus = ESP_ERR_INVALID_ARG;
        goto CleanUp;
    }

    // Copy data to SignalingMsg structure
    pSignalingMessage->version = msg->version;
    pSignalingMessage->messageType = msg->messagetype;
    strncpy(pSignalingMessage->peerClientId, msg->peerclientid,
            SS_MAX_SIGNALING_CLIENT_ID_LEN);
    strncpy(pSignalingMessage->correlationId, msg->correlationid,
            SS_MAX_CORRELATION_ID_LEN);

    // Allocate and copy payload
    if (msg->payload.len > 0) {
        pSignalingMessage->payload = serializer_memalloc(msg->payload.len + 1);
        if (pSignalingMessage->payload == NULL) {
            ESP_LOGE(TAG, "Failed to allocate %d bytes", (int) msg->payload.len);
            retStatus = ESP_ERR_NO_MEM;
            goto CleanUp;
        }
        memcpy(pSignalingMessage->payload, msg->payload.data, msg->payload.len);
        pSignalingMessage->payload[msg->payload.len] = '\0';
        pSignalingMessage->payloadLen = msg->payload.len;
    }

CleanUp:
    kvs__signaling__signaling_message__free_unpacked(msg, NULL);
    return retStatus;
}
