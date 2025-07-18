/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __WEBRTC_SIGNALING_IF_H__
#define __WEBRTC_SIGNALING_IF_H__

#include <stdint.h>
#include <stdbool.h>
#include "signaling_serializer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generic status type for signaling operations
 */
typedef uint32_t WEBRTC_STATUS;

#define WEBRTC_STATUS_SUCCESS           0x00000000
#define WEBRTC_STATUS_NULL_ARG          0x00000001
#define WEBRTC_STATUS_INVALID_ARG       0x00000002
#define WEBRTC_STATUS_NOT_ENOUGH_MEMORY 0x00000004
#define WEBRTC_STATUS_INVALID_OPERATION 0x0000000D
#define WEBRTC_STATUS_INTERNAL_ERROR    0x0000000C

#define WEBRTC_STATUS_FAILED(x)    (((WEBRTC_STATUS)(x)) != WEBRTC_STATUS_SUCCESS)
#define WEBRTC_STATUS_SUCCEEDED(x) (!WEBRTC_STATUS_FAILED(x))

/**
 * @brief Generic signaling client states
 */
typedef enum {
    WEBRTC_SIGNALING_CLIENT_STATE_NEW = 0,
    WEBRTC_SIGNALING_CLIENT_STATE_CONNECTING,
    WEBRTC_SIGNALING_CLIENT_STATE_CONNECTED,
    WEBRTC_SIGNALING_CLIENT_STATE_DISCONNECTED,
    WEBRTC_SIGNALING_CLIENT_STATE_FAILED,
} webrtc_signaling_client_state_t;

/**
 * @brief Generic signaling channel role types
 */
typedef enum {
    WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_MASTER = 0,
    WEBRTC_SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,
} webrtc_signaling_channel_role_type_t;

/**
 * @brief Signaling state enum
 */
typedef enum {
    ESP_SIGNALING_STATE_NEW,
    ESP_SIGNALING_STATE_CONNECTING,
    ESP_SIGNALING_STATE_CONNECTED,
    ESP_SIGNALING_STATE_DISCONNECTED,
    ESP_SIGNALING_STATE_FAILED,
} esp_webrtc_signaling_state_t;

/**
 * @brief Message types for signaling
 */
typedef enum {
    ESP_SIGNALING_MESSAGE_TYPE_OFFER,
    ESP_SIGNALING_MESSAGE_TYPE_ANSWER,
    ESP_SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE,
    ESP_SIGNALING_MESSAGE_TYPE_GO_AWAY,
    ESP_SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER,
} esp_webrtc_signaling_message_type_t;

/**
 * @brief Signaling message structure
 */
typedef struct {
    esp_webrtc_signaling_message_type_t message_type;
    char peer_client_id[SS_MAX_SIGNALING_CLIENT_ID_LEN + 1];
    char correlation_id[SS_MAX_CORRELATION_ID_LEN + 1];
    char *payload;
    uint32_t payload_len;
    uint32_t version;
} esp_webrtc_signaling_message_t;

/**
 * @brief Forward declaration for signaling client
 */
typedef struct esp_webrtc_signaling_client_s esp_webrtc_signaling_client_t;

/**
 * @brief Callback for signaling state changes
 */
typedef void (*esp_webrtc_signaling_state_callback_t)(esp_webrtc_signaling_client_t *client,
                                                 esp_webrtc_signaling_state_t state,
                                                 void *user_data);

/**
 * @brief Callback for signaling message received
 */
typedef void (*esp_webrtc_signaling_message_callback_t)(esp_webrtc_signaling_client_t *client,
                                                   esp_webrtc_signaling_message_t *message,
                                                   void *user_data);

/**
 * @brief Callback for signaling error
 */
typedef void (*esp_webrtc_signaling_error_callback_t)(esp_webrtc_signaling_client_t *client,
                                                 WEBRTC_STATUS error_status,
                                                 const char *error_message,
                                                 void *user_data);

/**
 * @brief Signaling client interface
 */
struct esp_webrtc_signaling_client_s {
    // Client-specific data
    void *client_data;

    // User context
    void *user_data;

    // Callbacks
    esp_webrtc_signaling_state_callback_t on_state_change;
    esp_webrtc_signaling_message_callback_t on_message;
    esp_webrtc_signaling_error_callback_t on_error;

    // Interface functions

    /**
     * @brief Initialize the signaling client
     *
     * @param client The signaling client
     * @param config Client-specific configuration
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*init)(esp_webrtc_signaling_client_t *client, void *config);

    /**
     * @brief Connect to the signaling service
     *
     * @param client The signaling client
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*connect)(esp_webrtc_signaling_client_t *client);

    /**
     * @brief Disconnect from the signaling service
     *
     * @param client The signaling client
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*disconnect)(esp_webrtc_signaling_client_t *client);

    /**
     * @brief Send a message through the signaling service
     *
     * @param client The signaling client
     * @param message The message to send
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*send_message)(esp_webrtc_signaling_client_t *client, esp_webrtc_signaling_message_t *message);

    /**
     * @brief Get the current state of the signaling client
     *
     * @param client The signaling client
     * @param state Pointer to store the state
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*get_state)(esp_webrtc_signaling_client_t *client, esp_webrtc_signaling_state_t *state);

    /**
     * @brief Free the signaling client
     *
     * @param client The signaling client
     * @return WEBRTC_STATUS code
     */
    WEBRTC_STATUS (*free)(esp_webrtc_signaling_client_t *client);
};

/**
 * @brief Create a signaling client instance
 *
 * This function is implemented by each signaling provider
 *
 * @param client_type Type of client to create
 * @param pp_client Pointer to store the created client
 * @return WEBRTC_STATUS code
 */
typedef WEBRTC_STATUS (*create_signaling_client_func_t)(const char *client_type, esp_webrtc_signaling_client_t **pp_client);

/**
 * @brief WebRTC Signaling Client Interface
 *
 * This interface defines the common operations that any signaling client
 * implementation must provide. It abstracts away the specific signaling
 * protocol (KVS, bridge, custom, etc.) from the WebRTC application layer.
 */
typedef struct {
    // Initialize the signaling client with configuration
    WEBRTC_STATUS (*init)(void *pSignalingConfig, void **ppSignalingClient);

    // Connect to signaling service
    WEBRTC_STATUS (*connect)(void *pSignalingClient);

    // Disconnect from signaling service
    WEBRTC_STATUS (*disconnect)(void *pSignalingClient);

    // Send a signaling message
    WEBRTC_STATUS (*sendMessage)(void *pSignalingClient, esp_webrtc_signaling_message_t *pMessage);

    // Free resources
    WEBRTC_STATUS (*free)(void *pSignalingClient);

    // Set callbacks for signaling events
    WEBRTC_STATUS (*setCallbacks)(void *pSignalingClient,
                                 uint64_t customData,
                                 WEBRTC_STATUS (*onMessageReceived)(uint64_t, esp_webrtc_signaling_message_t*),
                                 WEBRTC_STATUS (*onStateChanged)(uint64_t, webrtc_signaling_client_state_t),
                                 WEBRTC_STATUS (*onError)(uint64_t, WEBRTC_STATUS, char*, uint32_t));

    // Set the role type for the signaling client
    WEBRTC_STATUS (*setRoleType)(void *pSignalingClient, webrtc_signaling_channel_role_type_t roleType);

    // Get ICE server configuration
    WEBRTC_STATUS (*getIceServers)(void *pSignalingClient, uint32_t *pIceConfigCount, void *pRtcConfiguration);
} WebRtcSignalingClientInterface;

#ifdef __cplusplus
}
#endif

#endif /* __WEBRTC_SIGNALING_IF_H__ */
