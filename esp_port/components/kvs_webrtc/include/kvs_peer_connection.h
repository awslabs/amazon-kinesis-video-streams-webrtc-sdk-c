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
 * @brief Initialize the global KVS peer connection client
 *
 * This should be called once during app initialization to set up
 * the KVS peer connection system.
 *
 * @param config KVS peer connection configuration
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_global_init(kvs_peer_connection_config_t* config);

/**
 * @brief Create a KVS peer connection session
 *
 * This is the main function that app_webrtc.c should call instead of
 * createSampleStreamingSession. It creates a new peer connection session
 * using the KVS SDK with proper media setup.
 *
 * @param peer_id Unique identifier for the peer
 * @param is_initiator Whether this side initiates the connection
 * @param session_handle Output pointer to the created session handle
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_create_session(const char* peer_id, bool is_initiator, void** session_handle);

/**
 * @brief Send a WebRTC message through the peer connection
 *
 * Handles SDP offers/answers and ICE candidates through the KVS SDK.
 *
 * @param session_handle Session handle from kvs_webrtc_create_session
 * @param message WebRTC message to send
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_send_message(void* session_handle, webrtc_message_t* message);

/**
 * @brief Destroy a KVS peer connection session
 *
 * @param session_handle Session handle to destroy
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_destroy_session(void* session_handle);

/**
 * @brief Set callbacks for peer connection events
 *
 * @param session_handle Session handle
 * @param custom_data Custom data passed to callbacks
 * @param on_message_received Callback for received messages
 * @param on_peer_state_changed Callback for state changes
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_set_callbacks(void* session_handle,
                                      uint64_t custom_data,
                                      WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*),
                                      WEBRTC_STATUS (*on_peer_state_changed)(uint64_t, webrtc_peer_state_t));

/**
 * @brief Create and send an SDP offer
 *
 * This is used when the local side initiates the connection. The offer
 * will be sent through the on_message_received callback.
 *
 * @param session_handle Session handle (must be created as initiator)
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_create_offer(void* session_handle);

/**
 * @brief Global cleanup for KVS peer connection client
 *
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS kvs_webrtc_global_deinit(void);

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
