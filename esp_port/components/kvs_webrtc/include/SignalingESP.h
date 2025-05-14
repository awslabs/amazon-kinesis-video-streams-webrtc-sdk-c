/*
 * ESP-IDF WebSocket implementation for KVS WebRTC signaling
 */
#ifndef __KINESIS_VIDEO_WEBRTC_SIGNALING_ESP__
#define __KINESIS_VIDEO_WEBRTC_SIGNALING_ESP__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_websocket_client.h>
#ifndef LWS_PRE
#define LWS_PRE 16 // To keep definitions from LwsApiCalls.h happy
#include "Signaling/LwsApiCalls.h"
#endif
#include "Include_i.h"

// Define logging tag for ESP implementation
#define ESP_SIGNALING_TAG "ESP_SIGNALING"

// Structure to hold ESP WebSocket client context
typedef struct __EspSignalingClientWrapper {
    PSignalingClient signalingClient;
    esp_websocket_client_handle_t wsClient;
    MUTEX wsClientLock;
    BOOL isConnected;
    BOOL connectionAwaitingConfirmation; // Track pending connections
} EspSignalingClientWrapper, *PEspSignalingClientWrapper;

// Function declarations for ESP WebSocket implementation
STATUS terminateEspSignalingClient(PSignalingClient pSignalingClient);
STATUS connectEspSignalingClient(PSignalingClient pSignalingClient);
STATUS sendEspSignalingMessage(PSignalingClient pSignalingClient, PSignalingMessage pSignalingMessage);

// Declare API call functions
STATUS describeChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS createChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS getChannelEndpointEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS getIceConfigEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS deleteChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS connectSignalingChannelEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS joinStorageSessionEsp(PSignalingClient pSignalingClient, UINT64 time);
STATUS describeMediaStorageConfEsp(PSignalingClient pSignalingClient, UINT64 time);

#ifdef __cplusplus
}
#endif

#endif // __KINESIS_VIDEO_WEBRTC_SIGNALING_ESP__