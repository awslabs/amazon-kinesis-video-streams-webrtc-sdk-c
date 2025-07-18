/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef APPRTC_SIGNALING_INTERFACE_H
#define APPRTC_SIGNALING_INTERFACE_H

#include "webrtc_signaling_if.h"
#include "apprtc_signaling.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief AppRTC Signaling configuration structure
 */
typedef struct {
    // Server configuration
    char *serverUrl;                    // AppRTC server URL (optional, uses default if NULL)
    char *roomId;                       // Room ID to join (NULL to create new room)

    // Connection options
    bool autoConnect;                   // Whether to auto-connect on init
    uint32_t connectionTimeout;         // Connection timeout in milliseconds

    // Logging
    uint32_t logLevel;                  // Log level for AppRTC signaling
} AppRtcSignalingConfig, *PAppRtcSignalingConfig;

/**
 * @brief Get the AppRTC signaling client interface
 *
 * @return WebRtcSignalingClientInterface* Pointer to the AppRTC signaling interface
 */
WebRtcSignalingClientInterface* getAppRtcSignalingClientInterface(void);

/**
 * @brief Helper function to check if AppRTC signaling is connected
 *
 * @return bool true if connected, false otherwise
 */
bool isAppRtcSignalingConnected(void);

/**
 * @brief Helper function to get the current room ID
 *
 * @return char* Current room ID or NULL if not connected
 */
char* getAppRtcRoomId(void);

#ifdef __cplusplus
}
#endif

#endif /* APPRTC_SIGNALING_INTERFACE_H */
