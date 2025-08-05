/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file app_webrtc.h
 * @brief App WebRTC API file
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "signaling_serializer.h"
#include "webrtc_signaling_if.h"

// Default Client IDs for signaling
#define DEFAULT_MASTER_CLIENT_ID "ProducerMaster"
#define DEFAULT_VIEWER_CLIENT_ID "ConsumerViewer"

/**
 * @brief WebRTC application event types
 */
typedef enum {
    // Initialization / Deinitialization
    APP_WEBRTC_EVENT_INITIALIZED,                   //!< WebRTC application has been initialized
    APP_WEBRTC_EVENT_DEINITIALIZING,                //!< WebRTC application is being deinitialized

    // Signaling Client States
    APP_WEBRTC_EVENT_SIGNALING_CONNECTING,          //!< Attempting to connect to signaling server
    APP_WEBRTC_EVENT_SIGNALING_CONNECTED,           //!< Successfully connected to signaling server
    APP_WEBRTC_EVENT_SIGNALING_DISCONNECTED,        //!< Disconnected from signaling server
    APP_WEBRTC_EVENT_SIGNALING_DESCRIBE,            //!< Describing signaling channel
    APP_WEBRTC_EVENT_SIGNALING_GET_ENDPOINT,        //!< Getting signaling endpoint
    APP_WEBRTC_EVENT_SIGNALING_GET_ICE,             //!< Getting ICE server configuration
    APP_WEBRTC_EVENT_PEER_CONNECTION_REQUESTED,     //!< Peer connection has been requested
    APP_WEBRTC_EVENT_PEER_CONNECTED,                //!< Peer connection established successfully
    APP_WEBRTC_EVENT_PEER_DISCONNECTED,             //!< Peer connection terminated
    APP_WEBRTC_EVENT_STREAMING_STARTED,             //!< Media streaming threads started for a peer
    APP_WEBRTC_EVENT_STREAMING_STOPPED,             //!< Media streaming threads stopped for a peer
    APP_WEBRTC_EVENT_RECEIVED_OFFER,                //!< Received WebRTC offer from peer
    APP_WEBRTC_EVENT_SENT_ANSWER,                   //!< Sent WebRTC answer to peer
    APP_WEBRTC_EVENT_SENT_OFFER,                    //!< Sent WebRTC offer to peer
    APP_WEBRTC_EVENT_RECEIVED_ICE_CANDIDATE,        //!< Received ICE candidate from peer
    APP_WEBRTC_EVENT_SENT_ICE_CANDIDATE,            //!< Sent ICE candidate to peer
    APP_WEBRTC_EVENT_ICE_GATHERING_COMPLETE,        //!< ICE candidate gathering completed

    // Peer Connection States
    APP_WEBRTC_EVENT_PEER_CONNECTING,               //!< Peer connection attempt started
    APP_WEBRTC_EVENT_PEER_CONNECTION_FAILED,        //!< Peer connection failed

    // Error Events
    APP_WEBRTC_EVENT_ERROR,                         //!< General WebRTC error occurred
    APP_WEBRTC_EVENT_SIGNALING_ERROR,               //!< Signaling-specific error occurred
} app_webrtc_event_t;

/**
 * @brief Media streaming types supported by WebRTC application
 */
typedef enum {
    APP_WEBRTC_MEDIA_VIDEO,         //!< Video only streaming
    APP_WEBRTC_MEDIA_AUDIO_VIDEO,   //!< Both audio and video streaming
} app_webrtc_streaming_media_t;

/**
 * @brief Event data structure passed to event callbacks
 */
typedef struct {
    app_webrtc_event_t event_id;                        //!< Type of event that occurred
    uint32_t status_code;                               //!< Status code associated with the event (0 = success)
    char peer_id[SS_MAX_SIGNALING_CLIENT_ID_LEN + 1];   //!< Peer ID associated with the event (if applicable)
    char message[256];                                  //!< Human-readable message describing the event
} app_webrtc_event_data_t;

/**
 * @brief Event callback function type
 *
 * @param event_data Pointer to event data structure containing event details
 * @param user_ctx User context pointer passed during callback registration
 */
typedef void (*app_webrtc_event_callback_t) (app_webrtc_event_data_t *event_data, void *user_ctx);

/**
 * @brief Message sending callback function type for bridge communication
 *
 * @param signalingMessage Pointer to signaling message to be sent to bridge
 * @return 0 on success, non-zero on failure
 */
typedef int (*app_webrtc_send_msg_cb_t) (signaling_msg_t *signalingMessage);

/**
 * @brief Register a callback for WebRTC events
 *
 * @param callback Function to call when events occur
 * @param user_ctx User context pointer passed to the callback
 *
 * @return 0 on success, non-zero on failure
 */
int32_t app_webrtc_register_event_callback(app_webrtc_event_callback_t callback, void *user_ctx);

/**
 * @brief The enum specifies the codec types for audio and video tracks
 */
 typedef enum {
    APP_WEBRTC_CODEC_H264 = 1, //!< H264 video codec
    APP_WEBRTC_CODEC_OPUS = 2,                                                  //!< OPUS audio codec
    APP_WEBRTC_CODEC_VP8 = 3,                                                   //!< VP8 video codec.
    APP_WEBRTC_CODEC_MULAW = 4,                                                 //!< MULAW audio codec
    APP_WEBRTC_CODEC_ALAW = 5,                                                  //!< ALAW audio codec
    APP_WEBRTC_CODEC_UNKNOWN = 6,
    APP_WEBRTC_CODEC_H265 = 7, //!< H265 video codec
    APP_WEBRTC_CODEC_MAX //!< Placeholder for max number of supported codecs
} app_webrtc_rtc_codec_t;

/**
 * @brief WebRTC application configuration structure (simplified for ease of use)
 *
 * This configuration focuses on the essentials that 90% of users need.
 * Advanced configurations can be set using dedicated APIs after initialization.
 */
typedef struct {
    // Essential - what you MUST provide
    webrtc_signaling_client_if_t *signaling_client_if;      //!< Signaling client interface implementation
    void *signaling_cfg;                                    //!< Signaling-specific configuration (opaque pointer)

    // Media interfaces (optional - set to NULL for signaling-only applications)
    void* video_capture;                                    //!< Video capture interface (media_stream_video_capture_t*)
    void* audio_capture;                                    //!< Audio capture interface (media_stream_audio_capture_t*)
    void* video_player;                                     //!< Video player interface (media_stream_video_player_t*)
    void* audio_player;                                     //!< Audio player interface (media_stream_audio_player_t*)
} app_webrtc_config_t;

/**
 * @brief Default WebRTC application configuration initializer
 *
 * Initialize with reasonable defaults for most use cases.
 * Advanced settings use built-in defaults and can be changed via dedicated APIs.
 */
#define APP_WEBRTC_CONFIG_DEFAULT() \
{ \
    .signaling_client_if = NULL, \
    .signaling_cfg = NULL, \
    .video_capture = NULL, \
    .audio_capture = NULL, \
    .video_player = NULL, \
    .audio_player = NULL, \
}

/**
 * @brief Initialize WebRTC application with the given configuration
 *
 * This function creates and initializes the WebRTC configuration with reasonable defaults:
 * - Role: MASTER (initiates connections)
 * - Trickle ICE: enabled (faster connection setup)
 * - TURN servers: enabled (better NAT traversal)
 * - Log level: INFO (good balance of information)
 * - Audio codec: OPUS (most common)
 * - Video codec: H264 (most common)
 * - Media reception: disabled (most IoT devices are senders)
 * - Signaling-only: auto-detected (based on provided media interfaces)
 *
 * Advanced settings can be changed using dedicated APIs after initialization.
 *
 * @param[in] config Configuration for the WebRTC application
 *
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_init(app_webrtc_config_t *config);

/**
 * @brief Run the WebRTC application and wait for termination
 *
 * This function starts the WebRTC streaming session and waits until it's terminated.
 *
 * @return STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_run(void);

/**
 * @brief Terminate the WebRTC application
 *
 * This function cleans up resources and terminates the WebRTC application.
 *
 * @return STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_terminate(void);

/********************************************************************************
 *                      Advanced Configuration APIs                             *
 ********************************************************************************/

/**
 * @brief Set the WebRTC role type
 *
 * By default, the application starts as MASTER (initiates connections).
 * Call this API to change to VIEWER role if needed.
 *
 * @param[in] role Role type (MASTER or VIEWER)
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_role(webrtc_signaling_channel_role_type_t role);

/**
 * @brief Configure ICE connection behavior
 *
 * By default, trickle ICE and TURN servers are both enabled for optimal connectivity.
 *
 * @param[in] trickle_ice Whether to use trickle ICE for faster connection setup
 * @param[in] use_turn Whether to use TURN servers for NAT traversal
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_ice_config(bool trickle_ice, bool use_turn);

/**
 * @brief Set logging level
 *
 * By default, log level is set to INFO (level 3).
 * Levels: 0=PROFILE, 1=VERBOSE, 2=DEBUG, 3=INFO, 4=WARN, 5=ERROR, 6=FATAL
 *
 * @param[in] level Log level
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_log_level(uint32_t level);

/**
 * @brief Configure media codecs
 *
 * By default, OPUS is used for audio and H264 for video.
 *
 * @param[in] audio_codec Audio codec to use
 * @param[in] video_codec Video codec to use
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_codecs(app_webrtc_rtc_codec_t audio_codec, app_webrtc_rtc_codec_t video_codec);

/**
 * @brief Set media streaming type
 *
 * By default, media type is auto-detected based on provided interfaces.
 *
 * @param[in] media_type Media type (video-only or audio+video)
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_media_type(app_webrtc_streaming_media_t media_type);

/**
 * @brief Enable or disable media reception from remote peers
 *
 * By default, media reception is disabled (most IoT devices are senders).
 *
 * @param[in] enable Whether to enable receiving media from remote peers
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_enable_media_reception(bool enable);

/**
 * @brief Force signaling-only mode
 *
 * By default, signaling-only mode is auto-detected (enabled if no media interfaces are provided).
 * Use this API to explicitly force signaling-only mode even with media interfaces present.
 *
 * @param[in] enable Whether to enable signaling-only mode
 * @return WEBRTC_STATUS code of the execution
 */
WEBRTC_STATUS app_webrtc_set_signaling_only_mode(bool enable);

/********************************************************************************
 *                          Granular APIs for advanced use cases                *
 ********************************************************************************/

/**
 * @brief Register a callback for sending signaling messages to bridge (used in split mode)
 *
 * @param callback Function to call when messages need to be sent to bridge
 * @return 0 on success, non-zero on failure
 */
int app_webrtc_register_msg_callback(app_webrtc_send_msg_cb_t callback);

/**
 * @brief Send a message from bridge to signaling server (used in split mode)
 * This function sends signaling messages received from the streaming device
 * to the signaling server.
 * @param signalingMessage The signaling message to send to signaling server
 * @return 0 on success, non-zero on failure
 */
int app_webrtc_send_msg_to_signaling(signaling_msg_t *signalingMessage);

/**
 * @brief Create and send WebRTC offer to a specific peer
 *
 * This function creates a WebRTC offer and sends it to the specified peer through
 * the signaling channel. Used to initiate a WebRTC connection when operating as
 * the viewer/initiator role.
 *
 * @param pPeerId Peer ID to send the offer to
 * @return 0 on success, non-zero on failure
 */
int app_webrtc_trigger_offer(char *pPeerId);

/**
 * @brief Get ICE servers configuration from the WebRTC application
 *
 * This function retrieves the ICE server configuration that was obtained from
 * the signaling client. Used in split mode to forward ICE configuration from
 * signaling_only device to streaming_only device.
 *
 * @param[out] pIceServerCount Number of ICE servers returned
 * @param[out] pIceConfiguration Buffer to store ICE server configuration
 *                               (RtcConfiguration.iceServers array format)
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS app_webrtc_get_ice_servers(uint32_t *pIceServerCount, void *pIceConfiguration);

/**
 * @brief Query ICE server by index through signaling abstraction
 *
 * This function delegates to the appropriate signaling implementation via callbacks.
 * The signaling_only application should use this instead of direct KVS calls.
 *
 * @param[in] index ICE server index (0 = STUN, 1+ = TURN servers)
 * @param[in] useTurn Whether TURN servers are requested
 * @param[out] data Pointer to allocated ICE server data (caller must free)
 * @param[out] len Size of allocated data
 * @param[out] have_more Whether more servers are available for subsequent requests
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS app_webrtc_get_server_by_idx(int index, bool useTurn, uint8_t **data, int *len, bool *have_more);

/**
 * @brief Check if ICE configuration refresh is needed through signaling abstraction
 *
 * This function delegates to the appropriate signaling implementation to check
 * if ICE server configuration needs to be refreshed (immediate, non-blocking check).
 *
 * @param[out] refreshNeeded Whether ICE refresh is needed
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS app_webrtc_is_ice_refresh_needed(bool *refreshNeeded);

/**
 * @brief Trigger ICE configuration refresh through signaling abstraction
 *
 * This function delegates to the appropriate signaling implementation to trigger
 * a background refresh of ICE server configuration (non-blocking operation).
 *
 * @return WEBRTC_STATUS_SUCCESS on success, error code on failure
 */
WEBRTC_STATUS app_webrtc_refresh_ice_configuration(void);

#ifdef __cplusplus
}
#endif
