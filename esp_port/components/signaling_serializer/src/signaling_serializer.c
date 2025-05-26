/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>

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
