/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "app_webrtc_if.h"

#ifdef __cplusplus
extern "C" {
#endif

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
);

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
int signaling_message_to_apprtc_json(
    webrtc_message_t *pWebrtcMessage,
    char **ppJsonMessage,
    size_t *pJsonMessageLen
);

#ifdef __cplusplus
}
#endif
