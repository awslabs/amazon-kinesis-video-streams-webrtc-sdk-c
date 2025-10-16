/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __KVS_PEER_CONNECTION_H__
#define __KVS_PEER_CONNECTION_H__

#include "app_webrtc_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration for KVS peer connection
 *
 * This wraps the existing KVS SDK peer connection functionality
 * with our new interface. Used for classic mode and any scenario
 * where full WebRTC peer connections are needed.
 */
typedef struct {
    uint32_t log_level;                    // Log level for debugging
    bool trickle_ice;                      // Enable trickle ICE
    bool use_turn;                         // Use TURN servers

    // Media configuration
    void* video_capture;                   // Video capture interface
    void* audio_capture;                   // Audio capture interface
    void* video_player;                    // Video player interface
    void* audio_player;                    // Audio player interface
    bool receive_media;                    // Whether to receive media

    // Codec configuration
    app_webrtc_rtc_codec_t audio_codec;    // Audio codec (KVS codec type)
    app_webrtc_rtc_codec_t video_codec;    // Video codec (KVS codec type)

    // ICE server configuration (from signaling)
    void* ice_servers;                     // Pointer to array of RtcIceServer
    uint32_t ice_server_count;             // Number of ICE servers
} kvs_peer_connection_config_t;

/**
 * @brief Get the KVS peer connection interface
 *
 * This returns a peer connection interface that implements the actual
 * KVS SDK peer connection functionality. It handles real WebRTC
 * peer connections with media streaming, SDP processing, and ICE handling.
 *
 * The implementation is in kvs_webrtc.c and provides full KVS WebRTC
 * functionality including:
 * - Peer connection lifecycle management
 * - SDP offer/answer processing
 * - ICE candidate handling
 * - Media stream setup
 * - RTP transceiver management
 *
 * This is used for:
 * - webrtc_classic mode (full functionality on one device)
 * - esp_camera mode (with custom signaling)
 * - streaming_only mode (with bridge signaling)
 * - Any scenario requiring actual peer connections
 *
 * @return Pointer to the KVS peer connection interface
 */
webrtc_peer_connection_if_t* kvs_peer_connection_if_get(void);

#ifdef __cplusplus
}
#endif

#endif /* __KVS_PEER_CONNECTION_H__ */
