/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(LINUX_BUILD) || defined(KVS_PLAT_LINUX_UNIX)
// For Linux builds
// #include <stdio.h>
// #include <stdlib.h>
// #include "esp_err_compat.h"
typedef int esp_err_t;
#else
// For ESP builds
#include <esp_err.h>
#endif

typedef enum {
    SERIALIZER_TYPE_JSON = 0,
    SERIALIZER_TYPE_PROTOBUF,
    SERIALIZER_TYPE_RAW
} signaling_serializer_type;

/**
 * @brief Enum defining the type of signaling message
 *
 * This intentionally matches the enum defined in the signaling protocol defined in kvs
 * https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-websocket-apis3.html
 */
typedef enum {
    SIGNALING_MSG_TYPE_OFFER,
    SIGNALING_MSG_TYPE_ANSWER,
    SIGNALING_MSG_TYPE_ICE_CANDIDATE,
} signaling_msg_type;

// Must match with the defines in the signaling protocol defined in kvs
#define SS_MAX_CORRELATION_ID_LEN          256
#define SS_MAX_SIGNALING_CLIENT_ID_LEN     256
/**
 * @brief Structure defining the signaling message
 *
 * This intentionally matches the structure defined in the signaling protocol defined in kvs
 * https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-websocket-apis3.html
 */
typedef struct {
    uint32_t version; //!< Current version of the structure
    signaling_msg_type messageType; //!< Type of signaling message.
    char correlationId[SS_MAX_CORRELATION_ID_LEN + 1]; //!< Correlation Id string
    char peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN + 1]; //!< Sender client id
    uint32_t payloadLen; //!< Optional payload length. If 0, the length will be calculated
    char *payload; //!< Actual signaling message payload
} signaling_msg_t;

/**
 * @brief Serialize a SignalingMsg to a string format
 *
 * @param pSignalingMessage Message to serialize
 * @param outLen Length of the serialized data
 * @return char* Serialized message (must be freed by caller), NULL on error
 */
char* serialize_signaling_message(signaling_msg_t *pSignalingMessage, size_t* outLen);

/**
 * @brief Deserialize a string into a SignalingMsg
 *
 * @param data Input data to deserialize
 * @param len Length of input data
 * @param pSignalingMessage Output message structure
 * @return STATUS STATUS_SUCCESS on success
 */
esp_err_t deserialize_signaling_message(const char* data, size_t len, signaling_msg_t *pSignalingMessage);

/**
 * @brief Initialize the signaling serializer
 */
void signaling_serializer_init(void);

#ifdef __cplusplus
}
#endif
