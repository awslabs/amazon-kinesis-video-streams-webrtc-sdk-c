/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "signaling_serializer.h"

/**
 * @brief Serialize a SignalingMsg using JSON format
 *
 * @param pSignalingMessage Message to serialize
 * @param outLen Length of the serialized data
 * @return char* Serialized message (must be freed by caller), NULL on error
 */
char* serialize_signaling_message_json(signaling_msg_t *pSignalingMessage, size_t* outLen);

/**
 * @brief Deserialize JSON data into a SignalingMsg
 *
 * @param data JSON data
 * @param len Length of data
 * @param pSignalingMessage Output message structure
 * @return STATUS STATUS_SUCCESS on success
 */
esp_err_t deserialize_signaling_message_json(const char* data, size_t len, signaling_msg_t *pSignalingMessage);
