/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WEBRTC_BRIDGE_SIGNALING_H__
#define __WEBRTC_BRIDGE_SIGNALING_H__

#include "webrtc_signaling_if.h"
#include "signaling_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for bridge signaling client
 */
typedef struct {
    // Client ID for signaling
    char *client_id;

    // Log level
    uint32_t log_level;
} bridge_signaling_config_t;

/**
 * @brief Get the bridge signaling client interface
 *
 * This returns a pointer to the bridge signaling interface that can be used
 * with webrtcAppInit to enable bridge-based signaling.
 *
 * @return Pointer to the bridge signaling client interface
 */
WebRtcSignalingClientInterface* getBridgeSignalingClientInterface(void);

/**
 * @brief Common utility function to send a signaling message via webrtc_bridge
 *
 * This function takes a signaling_msg_t, serializes it, and sends it via the webrtc_bridge.
 * It can be used by both the bridge signaling interface and external components that need
 * to send messages via the bridge.
 *
 * @param pMessage Pointer to the signaling message to send
 * @return WEBRTC_STATUS code
 */
WEBRTC_STATUS bridge_signaling_send_message_via_bridge(signaling_msg_t* pMessage);

/**
 * @brief Handle messages received from webrtc_bridge for bridge signaling client
 *
 * This function is called by the central router to deliver messages to the bridge signaling client.
 *
 * @param data Pointer to the received message data
 * @param len Length of the received message data
 */
void bridge_message_handler(const void* data, int len);

#ifdef __cplusplus
}
#endif

#endif /* __WEBRTC_BRIDGE_SIGNALING_H__ */
