/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __APP_WEBRTC_IF_H__
#define __APP_WEBRTC_IF_H__

#include <stdint.h>
#include <stdbool.h>
// #include "signaling_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WebRTC status codes using structured enum ranges
 *
 * Range allocation:
 * - Basic errors: 0-127
 * - Signaling errors: 128-255
 * - Peer connection errors: 256-383 (reserved for future)
 * - Media errors: 384-511 (reserved for future)
 * - Implementation-specific: 512+ (reserved for future)
 */
typedef enum {
    // === Basic Errors (0-127) ===
    WEBRTC_STATUS_SUCCESS = 0,
    WEBRTC_STATUS_NULL_ARG = 1,
    WEBRTC_STATUS_INVALID_ARG = 2,
    WEBRTC_STATUS_NOT_ENOUGH_MEMORY = 4,
    WEBRTC_STATUS_NOT_IMPLEMENTED = 5,
    WEBRTC_STATUS_INTERNAL_ERROR = 12,
    WEBRTC_STATUS_INVALID_OPERATION = 13,

    // Room for additional basic errors (6-127)

    // === Signaling Errors (128-255) ===
    WEBRTC_STATUS_SIGNALING_BASE = 128,
    WEBRTC_STATUS_SIGNALING_ICE_REFRESH_FAILED = 129,
    WEBRTC_STATUS_SIGNALING_RECONNECT_FAILED = 130,
    WEBRTC_STATUS_SIGNALING_CONNECTION_LOST = 131,
    WEBRTC_STATUS_SIGNALING_AUTH_FAILED = 132,
    WEBRTC_STATUS_SIGNALING_TIMEOUT = 133,
    WEBRTC_STATUS_SIGNALING_CHANNEL_NOT_FOUND = 134,
    WEBRTC_STATUS_SIGNALING_INVALID_MESSAGE = 135,
    WEBRTC_STATUS_SIGNALING_RATE_LIMITED = 136,

    // Room for additional signaling errors (137-255)

    // === Peer Connection Errors (256-383) - Reserved ===
    WEBRTC_STATUS_PEER_CONNECTION_BASE = 256,
    // Future peer connection specific errors...

    // === Media Errors (384-511) - Reserved ===
    WEBRTC_STATUS_MEDIA_BASE = 384,
    // Future media specific errors...

    // === Implementation-specific (512+) - Reserved ===
    WEBRTC_STATUS_IMPLEMENTATION_BASE = 512
} WEBRTC_STATUS;

// Must match with the defines in the signaling protocol defined in kvs
#define APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN     256
#define APP_WEBRTC_MAX_CORRELATION_ID_LEN          256

// ICE Server configuration limits (matching main SDK)
#define APP_WEBRTC_MAX_ICE_CONFIG_URI_LEN          127
#define APP_WEBRTC_MAX_ICE_CONFIG_USER_NAME_LEN    256
#define APP_WEBRTC_MAX_ICE_CONFIG_CREDENTIAL_LEN   256
#define APP_WEBRTC_MAX_ICE_SERVERS_COUNT           5

// Default fallback STUN server (Google's public STUN server)
#define APP_WEBRTC_DEFAULT_STUN_SERVER             "stun:stun.l.google.com:19302"

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

// Status check macros for the enum-based error codes
#define WEBRTC_STATUS_FAILED(x)    (((WEBRTC_STATUS)(x)) != WEBRTC_STATUS_SUCCESS)
#define WEBRTC_STATUS_SUCCEEDED(x) (!WEBRTC_STATUS_FAILED(x))

// Status category check macros
#define WEBRTC_STATUS_IS_SIGNALING_ERROR(x)    (((WEBRTC_STATUS)(x)) >= WEBRTC_STATUS_SIGNALING_BASE && ((WEBRTC_STATUS)(x)) < WEBRTC_STATUS_PEER_CONNECTION_BASE)
#define WEBRTC_STATUS_IS_PEER_CONNECTION_ERROR(x) (((WEBRTC_STATUS)(x)) >= WEBRTC_STATUS_PEER_CONNECTION_BASE && ((WEBRTC_STATUS)(x)) < WEBRTC_STATUS_MEDIA_BASE)
#define WEBRTC_STATUS_IS_MEDIA_ERROR(x)        (((WEBRTC_STATUS)(x)) >= WEBRTC_STATUS_MEDIA_BASE && ((WEBRTC_STATUS)(x)) < WEBRTC_STATUS_IMPLEMENTATION_BASE)

/**
 * @brief Signaling client states
 * These represent the connection state to the signaling service
 */
typedef enum {
    WEBRTC_SIGNALING_STATE_NEW = 0,
    WEBRTC_SIGNALING_STATE_CONNECTING = 1,
    WEBRTC_SIGNALING_STATE_CONNECTED = 2,
    WEBRTC_SIGNALING_STATE_DISCONNECTED = 3,
    WEBRTC_SIGNALING_STATE_FAILED = 4,
} webrtc_signaling_state_t;

/**
 * @brief Peer connection states
 * These represent the WebRTC peer-to-peer connection state
 */
typedef enum {
    WEBRTC_PEER_STATE_NEW = 0,
    WEBRTC_PEER_STATE_CONNECTING = 1,
    WEBRTC_PEER_STATE_CONNECTED = 2,
    WEBRTC_PEER_STATE_DISCONNECTED = 3,
    WEBRTC_PEER_STATE_FAILED = 4,
    WEBRTC_PEER_STATE_MEDIA_STARTING = 5,  // Internal state to signal media thread startup needed
} webrtc_peer_state_t;

/**
 * @brief Channel role types
 */
typedef enum {
    WEBRTC_CHANNEL_ROLE_TYPE_MASTER = 0,
    WEBRTC_CHANNEL_ROLE_TYPE_VIEWER,
} webrtc_channel_role_type_t;

/**
 * @brief Unified WebRTC message types
 *
 * This covers both signaling messages and peer connection messages
 */
typedef enum {
    WEBRTC_MESSAGE_TYPE_OFFER,
    WEBRTC_MESSAGE_TYPE_ANSWER,
    WEBRTC_MESSAGE_TYPE_ICE_CANDIDATE,
    WEBRTC_MESSAGE_TYPE_GO_AWAY,
    WEBRTC_MESSAGE_TYPE_RECONNECT_ICE_SERVER,
    WEBRTC_MESSAGE_TYPE_STATE_CHANGE,
    WEBRTC_MESSAGE_TYPE_ERROR,
    WEBRTC_MESSAGE_TYPE_TRIGGER_OFFER,
} webrtc_message_type_t;

/**
 * @brief Unified WebRTC message structure
 *
 * This single message type is used for both signaling and peer connection communication.
 * Pure message format without additional state/error fields.
 */
typedef struct {
    webrtc_message_type_t message_type;
    char peer_client_id[APP_WEBRTC_MAX_SIGNALING_CLIENT_ID_LEN + 1];
    char correlation_id[APP_WEBRTC_MAX_CORRELATION_ID_LEN + 1];
    char *payload;
    uint32_t payload_len;
    uint32_t version;
} webrtc_message_t;

/**
 * @brief WebRTC Signaling Client Interface
 *
 * This interface defines the common operations that any signaling client
 * implementation must provide. It abstracts away the specific signaling
 * protocol (KVS, bridge, custom, etc.) from the WebRTC application layer.
 */
typedef struct {
    // Initialize the signaling client with configuration
    WEBRTC_STATUS (*init)(void *signaling_cfg, void **ppSignalingClient);

    // Connect to signaling service
    WEBRTC_STATUS (*connect)(void *pSignalingClient);

    // Disconnect from signaling service
    WEBRTC_STATUS (*disconnect)(void *pSignalingClient);

    // Send a signaling message
    WEBRTC_STATUS (*send_message)(void *pSignalingClient, webrtc_message_t *pMessage);

    // Free resources
    WEBRTC_STATUS (*free)(void *pSignalingClient);

    // Set callbacks for signaling events
    WEBRTC_STATUS (*set_callbacks)(void *pSignalingClient,
                                   uint64_t customData,
                                   WEBRTC_STATUS (*on_msg_received)(uint64_t, webrtc_message_t*),
                                   WEBRTC_STATUS (*on_signaling_state_changed)(uint64_t, webrtc_signaling_state_t),
                                   WEBRTC_STATUS (*on_error)(uint64_t, WEBRTC_STATUS, char*, uint32_t));

    // Set the role type for the signaling client
    WEBRTC_STATUS (*set_role_type)(void *pSignalingClient, webrtc_channel_role_type_t role_type);

    // Get ICE server configuration (expects iceServers array, not full RtcConfiguration)
    WEBRTC_STATUS (*get_ice_servers)(void *pSignalingClient, uint32_t *pIceConfigCount, void *pIceServersArray);

    // Query ICE server by index (for bridge/RPC pattern)
    WEBRTC_STATUS (*get_ice_server_by_idx)(void *pSignalingClient, int index, bool useTurn, uint8_t **data, int *len, bool *have_more);

    // Check if ICE configuration refresh is needed (immediate, non-blocking check)
    WEBRTC_STATUS (*is_ice_refresh_needed)(void *pSignalingClient, bool *refreshNeeded);

    // Trigger ICE configuration refresh (background operation)
    WEBRTC_STATUS (*refresh_ice_configuration)(void *pSignalingClient);

    // Progressive ICE server callbacks (for async TURN server delivery)
    WEBRTC_STATUS (*set_ice_update_callback)(void *pSignalingClient,
                                              uint64_t customData,
                                              WEBRTC_STATUS (*on_ice_servers_updated)(uint64_t, uint32_t));
} webrtc_signaling_client_if_t;

/**
 * @brief Data channel callbacks
 * These are the function signatures for data channel callbacks
 */
typedef void (*app_webrtc_rtc_on_open_t)(uint64_t customData, void *pDataChannel, const char *peer_id);
typedef void (*app_webrtc_rtc_on_message_t)(uint64_t customData, void *pDataChannel, const char *peer_id, bool isBinary, uint8_t *pMessage, uint32_t messageLen);

/**
 * @brief Generic WebRTC peer connection configuration
 *
 * This configuration can be used by any peer connection implementation
 * to configure basic WebRTC settings.
 */
typedef struct {
    // Media interfaces
    void* video_capture;                  // Video capture interface
    void* audio_capture;                  // Audio capture interface
    void* video_player;                   // Video player interface
    void* audio_player;                   // Audio player interface

    // Codec configuration
    app_webrtc_rtc_codec_t audio_codec;   // Audio codec to use (OPUS, MULAW, ALAW, etc.)
    app_webrtc_rtc_codec_t video_codec;   // Video codec to use (H264, H265, VP8, etc.)

    // Implementation-specific configuration
    void* peer_connection_cfg;          // Implementation-specific configuration (opaque pointer)
} webrtc_peer_connection_config_t;

/**
 * @brief WebRTC Peer Connection Interface
 *
 * This interface provides a minimal abstraction for peer connection operations.
 * It focuses on essential message passing and lifecycle management while keeping
 * the state management in app_webrtc.c
 */
typedef struct {
    // Initialize peer connection client with basic configuration (without ICE servers)
    WEBRTC_STATUS (*init)(void *pc_cfg, void **ppPeerConnectionClient);

    // Set or update ICE servers for the peer connection client
    // This allows updating ICE servers separately from initialization
    WEBRTC_STATUS (*set_ice_servers)(void *pPeerConnectionClient, void *ice_servers, uint32_t ice_count);

    // Create a new peer connection session
    WEBRTC_STATUS (*create_session)(void *pPeerConnectionClient,
                                    const char *peer_id,
                                    bool is_initiator,
                                    void **ppSession);

    // Send a WebRTC message (SDP offer/answer, ICE candidate)
    WEBRTC_STATUS (*send_message)(void *pSession, webrtc_message_t *pMessage);

    // Destroy a peer connection session
    WEBRTC_STATUS (*destroy_session)(void *pSession);

    // Free peer connection client resources
    WEBRTC_STATUS (*free)(void *pPeerConnectionClient);

    // Set callbacks for peer connection events
    WEBRTC_STATUS (*set_callbacks)(void *pSession,
                                  uint64_t custom_data,
                                  WEBRTC_STATUS (*on_message_received)(uint64_t, webrtc_message_t*),
                                  WEBRTC_STATUS (*on_peer_state_changed)(uint64_t, webrtc_peer_state_t));

    // Trigger an offer creation for an existing session
    WEBRTC_STATUS (*trigger_offer)(void *pSession);

    // Data channel operations

    // Create a data channel for a session
    WEBRTC_STATUS (*create_data_channel)(void *pSession,
                                        const char *channelName,
                                        void *pDataChannelInit,
                                        void **ppDataChannel);

    // Event handler registration
    WEBRTC_STATUS (*register_event_handler)(void *pPeerConnectionClient,
                                          void (*eventHandler)(app_webrtc_event_t event_id,
                                                               uint32_t status_code,
                                                               char *peer_id,
                                                               char *message));

    // Set callbacks for data channel events
    WEBRTC_STATUS (*set_data_channel_callbacks)(void *pSession,
                                              app_webrtc_rtc_on_open_t onOpen,
                                              app_webrtc_rtc_on_message_t onMessage,
                                              uint64_t customData);

    // Send a message through the data channel
    WEBRTC_STATUS (*send_data_channel_message)(void *pSession,
                                               void *pDataChannel,
                                               bool isBinary,
                                               const uint8_t *pMessage,
                                               uint32_t messageLen);
} webrtc_peer_connection_if_t;

#ifdef __cplusplus
}
#endif

#endif /* __APP_WEBRTC_IF_H__ */
