/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(LINUX_BUILD) || defined(KVS_PLAT_LINUX_UNIX)
// For Linux builds
// #include <stdio.h>
// #include <stdlib.h>
// #include "esp_err_compat.h"
typedef int esp_err_t;
#else
// For ESP builds
#include <esp_err.h>
#endif

typedef enum {
    SERIALIZER_TYPE_JSON = 0,
    SERIALIZER_TYPE_PROTOBUF,
    SERIALIZER_TYPE_RAW
} signaling_serializer_type;

/**
 * @brief Enum defining the type of signaling message
 *
 * This intentionally matches the enum defined in the signaling protocol defined in kvs
 * https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-websocket-apis3.html
 */
typedef enum {
    SIGNALING_MSG_TYPE_OFFER,
    SIGNALING_MSG_TYPE_ANSWER,
    SIGNALING_MSG_TYPE_ICE_CANDIDATE,
    SIGNALING_MSG_TYPE_ICE_SERVERS,          // ← Transfer ICE server configuration (bulk response)
    SIGNALING_MSG_TYPE_ICE_REQUEST,          // ← Request ICE server by index from signaling device
    SIGNALING_MSG_TYPE_ICE_SERVER_RESPONSE,  // ← Single ICE server response with have_more flag
} signaling_msg_type;

// Must match with the defines in the signaling protocol defined in kvs
#define SS_MAX_CORRELATION_ID_LEN          256
#define SS_MAX_SIGNALING_CLIENT_ID_LEN     256

// ICE Server configuration limits (matching main SDK)
#define SS_MAX_ICE_CONFIG_URI_LEN          127
#define SS_MAX_ICE_CONFIG_USER_NAME_LEN    256
#define SS_MAX_ICE_CONFIG_CREDENTIAL_LEN   256
#define SS_MAX_ICE_SERVERS_COUNT           16

/**
 * @brief Structure defining a single ICE server configuration
 * This matches the RtcIceServer structure from the main SDK
 */
typedef struct {
    char urls[SS_MAX_ICE_CONFIG_URI_LEN + 1];                    //!< URL of STUN/TURN Server
    char username[SS_MAX_ICE_CONFIG_USER_NAME_LEN + 1];         //!< Username to be used with TURN server
    char credential[SS_MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];      //!< Password to be used with TURN server
} ss_ice_server_t;

/**
 * @brief Structure defining ICE servers configuration message payload
 */
typedef struct {
    uint32_t ice_server_count;                                  //!< Number of ICE servers in the array
    ss_ice_server_t ice_servers[SS_MAX_ICE_SERVERS_COUNT];     //!< Array of ICE server configurations
} ss_ice_servers_payload_t;

/**
 * @brief Structure defining the signaling message
 *
 * This intentionally matches the structure defined in the signaling protocol defined in kvs
 * https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/kvswebrtc-websocket-apis3.html
 */
typedef struct {
    uint32_t version; //!< Current version of the structure
    signaling_msg_type messageType; //!< Type of signaling message.
    char correlationId[SS_MAX_CORRELATION_ID_LEN + 1]; //!< Correlation Id string
    char peerClientId[SS_MAX_SIGNALING_CLIENT_ID_LEN + 1]; //!< Sender client id
    uint32_t payloadLen; //!< Optional payload length. If 0, the length will be calculated
    char *payload; //!< Actual signaling message payload
} signaling_msg_t;

/**
 * @brief Serialize a SignalingMsg to a string format
 *
 * @param pSignalingMessage Message to serialize
 * @param outLen Length of the serialized data
 * @return char* Serialized message (must be freed by caller), NULL on error
 */
char* serialize_signaling_message(signaling_msg_t *pSignalingMessage, size_t* outLen);

/**
 * @brief Deserialize a string into a SignalingMsg
 *
 * @param data Input data to deserialize
 * @param len Length of input data
 * @param pSignalingMessage Output message structure
 * @return STATUS STATUS_SUCCESS on success
 */
esp_err_t deserialize_signaling_message(const char* data, size_t len, signaling_msg_t *pSignalingMessage);

/**
 * @brief Initialize the signaling serializer
 */
void signaling_serializer_init(void);

/**
 * @brief Create ICE servers message from RtcConfiguration
 *
 * @param pRtcConfiguration Source RTC configuration with ICE servers
 * @param ice_server_count Number of ICE servers to include
 * @param pSignalingMessage Output signaling message structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t create_ice_servers_message(const void* pRtcConfiguration, uint32_t ice_server_count, signaling_msg_t* pSignalingMessage);

/**
 * @brief Extract ICE servers from signaling message
 *
 * @param pSignalingMessage Input signaling message with ICE servers
 * @param pIceServersPayload Output ICE servers payload structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t extract_ice_servers_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_servers_payload_t* pIceServersPayload);

// ICE request payload structure
typedef struct {
    uint32_t index;           // Index of ICE server to request (0 = STUN, 1+ = TURN)
    bool use_turn;           // Whether TURN servers are needed
} ss_ice_request_payload_t;

// ICE response payload structure (single server)
typedef struct {
    char urls[SS_MAX_ICE_CONFIG_URI_LEN + 1];
    char username[SS_MAX_ICE_CONFIG_USER_NAME_LEN + 1];
    char credential[SS_MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
    bool have_more;          // Indicates if more servers are available
} ss_ice_server_response_t;

/**
 * @brief Create ICE request message to ask signaling device for ICE server by index
 *
 * @param index Index of ICE server to request (0 = STUN, 1+ = TURN)
 * @param use_turn Whether TURN servers are needed
 * @param pSignalingMessage Output signaling message structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t create_ice_request_message(uint32_t index, bool use_turn, signaling_msg_t* pSignalingMessage);

/**
 * @brief Extract ICE request from signaling message
 *
 * @param pSignalingMessage Input signaling message with ICE request
 * @param pIceRequestPayload Output ICE request payload structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t extract_ice_request_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_request_payload_t* pIceRequestPayload);

/**
 * @brief Create ICE server response message
 *
 * @param pIceServer ICE server data to send
 * @param have_more Whether more servers are available
 * @param pSignalingMessage Output signaling message structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t create_ice_server_response_message(const void* pIceServer, bool have_more, signaling_msg_t* pSignalingMessage);

/**
 * @brief Extract ICE server response from signaling message
 *
 * @param pSignalingMessage Input signaling message with ICE server response
 * @param pIceServerResponse Output ICE server response structure
 * @return esp_err_t ESP_OK on success
 */
esp_err_t extract_ice_server_response_from_message(const signaling_msg_t* pSignalingMessage, ss_ice_server_response_t* pIceServerResponse);

#ifdef __cplusplus
}
#endif
