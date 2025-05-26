/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "signaling_serializer.h"

/**
 * @brief Serialize a SignalingMessage using raw binary format
 *
 * The raw format is:
 * [SignalingMessage struct][payload data]
 *
 * @param pSignalingMessage Message to serialize
 * @param outLen Length of the serialized data
 * @return char* Serialized message (must be freed by caller), NULL on error
 */
char* serialize_signaling_message_raw(signaling_msg_t *pSignalingMessage, size_t* outLen);

/**
 * @brief Deserialize raw binary data into a SignalingMessage
 *
 * @param data Raw binary data
 * @param len Length of data
 * @param pSignalingMessage Output message structure
 * @return STATUS STATUS_SUCCESS on success
 */
esp_err_t deserialize_signaling_message_raw(const char* data, size_t len, signaling_msg_t *pSignalingMessage);
