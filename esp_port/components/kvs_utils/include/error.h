/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_ERROR__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_ERROR__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
// #include "platform_esp32.h"

#ifndef UINT32
typedef uint32_t UINT32;
#endif

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define STATUS UINT32

#define STATUS_SUCCESS      ((STATUS) 0x00000000)
#define STATUS_FAILED(x)    (((STATUS)(x)) != STATUS_SUCCESS)
#define STATUS_SUCCEEDED(x) (!STATUS_FAILED(x))
/******************************************************************************
 * Error codes base
 ******************************************************************************/
#define STATUS_BASE                0x00000000
#define STATUS_JSON_BASE           0x13000000
#define STATUS_AWS_SIGNER_BASE     0x14200000
#define STATUS_NET_BASE            0x15000000
#define STATUS_SOCKET_CONN_BASE    0x15100000
#define STATUS_CRYPTO_BASE         0x16000000
#define STATUS_TLS_BASE            0x16200000
#define STATUS_HTTP_BASE           0x17000000
#define STATUS_WSS_API_BASE        0x17100000
#define STATUS_WSS_CLIENT_BASE     0x17200000

#define STATUS_PEER_CONN_BASE      0x20000000
#define STATUS_METRICS_BASE        0x21000000

/******************************************************************************
 * Common error codes
 ******************************************************************************/
#define STATUS_NULL_ARG                          STATUS_BASE + 0x00000001
#define STATUS_INVALID_ARG                       STATUS_BASE + 0x00000002
#define STATUS_INVALID_ARG_LEN                   STATUS_BASE + 0x00000003
#define STATUS_NOT_ENOUGH_MEMORY                 STATUS_BASE + 0x00000004
#define STATUS_BUFFER_TOO_SMALL                  STATUS_BASE + 0x00000005
#define STATUS_UNEXPECTED_EOF                    STATUS_BASE + 0x00000006
#define STATUS_FORMAT_ERROR                      STATUS_BASE + 0x00000007
#define STATUS_INVALID_HANDLE_ERROR              STATUS_BASE + 0x00000008
#define STATUS_OPEN_FILE_FAILED                  STATUS_BASE + 0x00000009
#define STATUS_READ_FILE_FAILED                  STATUS_BASE + 0x0000000A
#define STATUS_WRITE_TO_FILE_FAILED              STATUS_BASE + 0x0000000B
#define STATUS_INTERNAL_ERROR                    STATUS_BASE + 0x0000000C
#define STATUS_INVALID_OPERATION                 STATUS_BASE + 0x0000000D
#define STATUS_NOT_IMPLEMENTED                   STATUS_BASE + 0x0000000E
#define STATUS_OPERATION_TIMED_OUT               STATUS_BASE + 0x0000000F
#define STATUS_NOT_FOUND                         STATUS_BASE + 0x00000010
#define STATUS_CREATE_THREAD_FAILED              STATUS_BASE + 0x00000011
#define STATUS_THREAD_NOT_ENOUGH_RESOURCES       STATUS_BASE + 0x00000012
#define STATUS_THREAD_INVALID_ARG                STATUS_BASE + 0x00000013
#define STATUS_THREAD_PERMISSIONS                STATUS_BASE + 0x00000014
#define STATUS_THREAD_DEADLOCKED                 STATUS_BASE + 0x00000015
#define STATUS_THREAD_DOES_NOT_EXIST             STATUS_BASE + 0x00000016
#define STATUS_JOIN_THREAD_FAILED                STATUS_BASE + 0x00000017
#define STATUS_WAIT_FAILED                       STATUS_BASE + 0x00000018
#define STATUS_CANCEL_THREAD_FAILED              STATUS_BASE + 0x00000019
#define STATUS_THREAD_IS_NOT_JOINABLE            STATUS_BASE + 0x0000001A
#define STATUS_DETACH_THREAD_FAILED              STATUS_BASE + 0x0000001B
#define STATUS_THREAD_ATTR_INIT_FAILED           STATUS_BASE + 0x0000001C
#define STATUS_THREAD_ATTR_SET_STACK_SIZE_FAILED STATUS_BASE + 0x0000001D
#define STATUS_MEMORY_NOT_FREED                  STATUS_BASE + 0x0000001E

/**
 * Common Utils error values
 */
#define STATUS_UTILS_BASE                            0x40000000
#define STATUS_INVALID_BASE64_ENCODE                 STATUS_UTILS_BASE + 0x00000001
#define STATUS_INVALID_BASE                          STATUS_UTILS_BASE + 0x00000002
#define STATUS_INVALID_DIGIT                         STATUS_UTILS_BASE + 0x00000003
#define STATUS_INT_OVERFLOW                          STATUS_UTILS_BASE + 0x00000004
#define STATUS_EMPTY_STRING                          STATUS_UTILS_BASE + 0x00000005
#define STATUS_DIRECTORY_OPEN_FAILED                 STATUS_UTILS_BASE + 0x00000006
#define STATUS_PATH_TOO_LONG                         STATUS_UTILS_BASE + 0x00000007
#define STATUS_UNKNOWN_DIR_ENTRY_TYPE                STATUS_UTILS_BASE + 0x00000008
#define STATUS_REMOVE_DIRECTORY_FAILED               STATUS_UTILS_BASE + 0x00000009
#define STATUS_REMOVE_FILE_FAILED                    STATUS_UTILS_BASE + 0x0000000a
#define STATUS_REMOVE_LINK_FAILED                    STATUS_UTILS_BASE + 0x0000000b
#define STATUS_DIRECTORY_ACCESS_DENIED               STATUS_UTILS_BASE + 0x0000000c
#define STATUS_DIRECTORY_MISSING_PATH                STATUS_UTILS_BASE + 0x0000000d
#define STATUS_DIRECTORY_ENTRY_STAT_ERROR            STATUS_UTILS_BASE + 0x0000000e
#define STATUS_STRFTIME_FALIED                       STATUS_UTILS_BASE + 0x0000000f
#define STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED STATUS_UTILS_BASE + 0x00000010
#define STATUS_UTIL_MAX_TAG_COUNT                    STATUS_UTILS_BASE + 0x00000011
#define STATUS_UTIL_INVALID_TAG_VERSION              STATUS_UTILS_BASE + 0x00000012
#define STATUS_UTIL_TAGS_COUNT_NON_ZERO_TAGS_NULL    STATUS_UTILS_BASE + 0x00000013
#define STATUS_UTIL_INVALID_TAG_NAME_LEN             STATUS_UTILS_BASE + 0x00000014
#define STATUS_UTIL_INVALID_TAG_VALUE_LEN            STATUS_UTILS_BASE + 0x00000015
#define STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE     STATUS_UTILS_BASE + 0x0000002a
#define STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED STATUS_UTILS_BASE + 0x0000002b
#define STATUS_THREADPOOL_MAX_COUNT                  STATUS_UTILS_BASE + 0x0000002c
#define STATUS_THREADPOOL_INTERNAL_ERROR             STATUS_UTILS_BASE + 0x0000002d

/**
 * Semaphore error values starting from 0x41200000
 */
#define STATUS_SEMAPHORE_BASE                     STATUS_UTILS_BASE + 0x01200000
#define STATUS_SEMAPHORE_OPERATION_AFTER_SHUTDOWN STATUS_SEMAPHORE_BASE + 0x00000001
#define STATUS_SEMAPHORE_ACQUIRE_WHEN_LOCKED      STATUS_SEMAPHORE_BASE + 0x00000002

////////////////////////////////////////////////////
// Status return codes
////////////////////////////////////////////////////
#define STATUS_STATE_BASE                     0x52000000
#define STATUS_INVALID_STREAM_STATE           STATUS_STATE_BASE + 0x0000000e
#define STATUS_STATE_MACHINE_STATE_NOT_FOUND  STATUS_STATE_BASE + 0x00000056
#define STATUS_STATE_MACHINE_NAME_LEN_INVALID STATUS_STATE_BASE + 0x0000009a // 0x00000057 to 0x0000008f used with STATUS_CLIENT_BASE

////////////////////////////////////////////////////
/// Common producer base return codes
////////////////////////////////////////////////////

/*! \addtogroup CommonProducerBaseStatusCodes
 *  @{
 */

/**
 * This section is done for backward compat. We shouldn't add to it. New status should be added to common base section
 */
#define STATUS_COMMON_PRODUCER_BASE                         0x15000000
#define STATUS_INVALID_AWS_CREDENTIALS_VERSION              STATUS_COMMON_PRODUCER_BASE + 0x00000008
#define STATUS_MAX_REQUEST_HEADER_COUNT                     STATUS_COMMON_PRODUCER_BASE + 0x00000009
#define STATUS_MAX_REQUEST_HEADER_NAME_LEN                  STATUS_COMMON_PRODUCER_BASE + 0x0000000a
#define STATUS_MAX_REQUEST_HEADER_VALUE_LEN                 STATUS_COMMON_PRODUCER_BASE + 0x0000000b
#define STATUS_INVALID_API_CALL_RETURN_JSON                 STATUS_COMMON_PRODUCER_BASE + 0x0000000c
#define STATUS_CURL_INIT_FAILED                             STATUS_COMMON_PRODUCER_BASE + 0x0000000d
#define STATUS_CURL_LIBRARY_INIT_FAILED                     STATUS_COMMON_PRODUCER_BASE + 0x0000000e
#define STATUS_HMAC_GENERATION_ERROR                        STATUS_COMMON_PRODUCER_BASE + 0x00000010
#define STATUS_IOT_FAILED                                   STATUS_COMMON_PRODUCER_BASE + 0x00000011
#define STATUS_MAX_ROLE_ALIAS_LEN_EXCEEDED                  STATUS_COMMON_PRODUCER_BASE + 0x00000012
#define STATUS_INVALID_USER_AGENT_LENGTH                    STATUS_COMMON_PRODUCER_BASE + 0x00000015
#define STATUS_IOT_EXPIRATION_OCCURS_IN_PAST                STATUS_COMMON_PRODUCER_BASE + 0x00000017
#define STATUS_IOT_EXPIRATION_PARSING_FAILED                STATUS_COMMON_PRODUCER_BASE + 0x00000018
#define STATUS_MAX_IOT_THING_NAME_LENGTH                    STATUS_COMMON_PRODUCER_BASE + 0x0000001e
#define STATUS_IOT_CREATE_LWS_CONTEXT_FAILED                STATUS_COMMON_PRODUCER_BASE + 0x0000001f
#define STATUS_INVALID_CA_CERT_PATH                         STATUS_COMMON_PRODUCER_BASE + 0x00000020
#define STATUS_FILE_CREDENTIAL_PROVIDER_OPEN_FILE_FAILED    STATUS_COMMON_PRODUCER_BASE + 0x00000022
#define STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_LENGTH STATUS_COMMON_PRODUCER_BASE + 0x00000023
#define STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_FORMAT STATUS_COMMON_PRODUCER_BASE + 0x00000024
#define STATUS_INVALID_AUTH_LEN                             STATUS_COMMON_PRODUCER_BASE + 0x00000025

/*!@} */

/**
 * Timer queue error values starting from 0x41100000
 */
#define STATUS_TIMER_QUEUE_BASE            STATUS_UTILS_BASE + 0x01100000
#define STATUS_TIMER_QUEUE_STOP_SCHEDULING STATUS_TIMER_QUEUE_BASE + 0x00000001
#define STATUS_INVALID_TIMER_COUNT_VALUE   STATUS_TIMER_QUEUE_BASE + 0x00000002
#define STATUS_INVALID_TIMER_PERIOD_VALUE  STATUS_TIMER_QUEUE_BASE + 0x00000003
#define STATUS_MAX_TIMER_COUNT_REACHED     STATUS_TIMER_QUEUE_BASE + 0x00000004
#define STATUS_TIMER_QUEUE_SHUTDOWN        STATUS_TIMER_QUEUE_BASE + 0x00000005

/**
 * Hash table error values starting from 0x40100000
 */
#define STATUS_HASH_TABLE_BASE            STATUS_UTILS_BASE + 0x00100000
#define STATUS_HASH_KEY_NOT_PRESENT       STATUS_HASH_TABLE_BASE + 0x00000001
#define STATUS_HASH_KEY_ALREADY_PRESENT   STATUS_HASH_TABLE_BASE + 0x00000002
#define STATUS_HASH_ENTRY_ITERATION_ABORT STATUS_HASH_TABLE_BASE + 0x00000003

/******************************************************************************
 * File logger error codes
 ******************************************************************************/
/**
 * File logger error values starting from 0x41300000
 */
#define STATUS_FILE_LOGGER_BASE                    STATUS_UTILS_BASE + 0x01300000
#define STATUS_FILE_LOGGER_INDEX_FILE_INVALID_SIZE STATUS_FILE_LOGGER_BASE + 0x00000001

/******************************************************************************
 * Network error codes
 ******************************************************************************/
#define STATUS_NET_GET_LOCAL_IP_ADDRESSES_FAILED      STATUS_NET_BASE + 0x00000001
#define STATUS_NET_CREATE_UDP_SOCKET_FAILED           STATUS_NET_BASE + 0x00000002
#define STATUS_NET_BINDING_SOCKET_FAILED              STATUS_NET_BASE + 0x00000003
#define STATUS_NET_GET_PORT_NUMBER_FAILED             STATUS_NET_BASE + 0x00000004
#define STATUS_NET_SEND_DATA_FAILED                   STATUS_NET_BASE + 0x00000005
#define STATUS_NET_RESOLVE_HOSTNAME_FAILED            STATUS_NET_BASE + 0x00000006
#define STATUS_NET_HOSTNAME_NOT_FOUND                 STATUS_NET_BASE + 0x00000007
#define STATUS_NET_SOCKET_CONNECT_FAILED              STATUS_NET_BASE + 0x00000008
#define STATUS_NET_SOCKET_SET_SEND_BUFFER_SIZE_FAILED STATUS_NET_BASE + 0x00000009
#define STATUS_NET_GET_SOCKET_FLAG_FAILED             STATUS_NET_BASE + 0x0000000A
#define STATUS_NET_SET_SOCKET_FLAG_FAILED             STATUS_NET_BASE + 0x0000000B
#define STATUS_NET_CLOSE_SOCKET_FAILED                STATUS_NET_BASE + 0x0000000C
#define STATUS_NET_RECV_DATA_FAILED                   STATUS_NET_BASE + 0x0000000D
/******************************************************************************
 * Socket error codes
 ******************************************************************************/
#define STATUS_SOCKET_CONN_NULL_ARG          STATUS_SOCKET_CONN_BASE + 0x00000001
#define STATUS_SOCKET_CONN_INVALID_ARG       STATUS_SOCKET_CONN_BASE + 0x00000002
#define STATUS_SOCKET_CONN_NOT_ENOUGH_MEMORY STATUS_SOCKET_CONN_BASE + 0x00000003
#define STATUS_SOCKET_CONN_INVALID_OPERATION STATUS_SOCKET_CONN_BASE + 0x00000004
#define STATUS_SOCKET_CONN_CLOSED_ALREADY    STATUS_SOCKET_CONN_BASE + 0x00000005


/******************************************************************************
 * TLS error codes
 ******************************************************************************/
#define STATUS_TLS_NULL_ARG                     STATUS_TLS_BASE + 0x00000001
#define STATUS_TLS_NOT_ENOUGH_MEMORY            STATUS_TLS_BASE + 0x00000002
#define STATUS_TLS_CREATE_SSL_FAILED            STATUS_TLS_BASE + 0x00000003
#define STATUS_TLS_INVALID_CA_CERT_PATH         STATUS_TLS_BASE + 0x00000004
#define STATUS_TLS_SSL_CTX_SETUP_FAILED         STATUS_TLS_BASE + 0x00000005
#define STATUS_TLS_SSL_HANDSHAKE_FAILED         STATUS_TLS_BASE + 0x00000006
#define STATUS_TLS_SSL_CTX_CREATION_FAILED      STATUS_TLS_BASE + 0x00000007
#define STATUS_TLS_SOCKET_READ_FAILED           STATUS_TLS_BASE + 0x00000008
#define STATUS_TLS_CONNECTION_NOT_READY_TO_SEND STATUS_TLS_BASE + 0x00000009

/******************************************************************************
 * ICE fsm error codes
 ******************************************************************************/
#define STATUS_ICE_FSM_NULL_ARG                             STATUS_ICE_FSM_BASE + 0x00000001
#define STATUS_ICE_FSM_FAILED_TO_RECOVER_FROM_DISCONNECTION STATUS_ICE_FSM_BASE + 0x00000002
#define STATUS_ICE_FSM_NO_CONNECTED_CANDIDATE_PAIR          STATUS_ICE_FSM_BASE + 0x00000003
#define STATUS_ICE_FSM_INVALID_STATE                        STATUS_ICE_FSM_BASE + 0x00000004
#define STATUS_ICE_FSM_NO_AVAILABLE_ICE_CANDIDATE_PAIR      STATUS_ICE_FSM_BASE + 0x00000005
/******************************************************************************
 * ICE utils error codes
 ******************************************************************************/
#define STATUS_ICE_UTILS_URL_INVALID_PREFIX          STATUS_ICE_UTILS_BASE + 0x00000001
#define STATUS_ICE_UTILS_URL_TURN_MISSING_USERNAME   STATUS_ICE_UTILS_BASE + 0x00000002
#define STATUS_ICE_UTILS_URL_TURN_MISSING_CREDENTIAL STATUS_ICE_UTILS_BASE + 0x00000003
#define STATUS_ICE_UTILS_NOT_ENOUGH_MEMORY           STATUS_ICE_UTILS_BASE + 0x00000004
#define STATUS_ICE_UTILS_NULL_ARG                    STATUS_ICE_UTILS_BASE + 0x00000005
#define STATUS_ICE_UTILS_EMPTY_STUN_SEND_BUF         STATUS_ICE_UTILS_BASE + 0x00000006

/******************************************************************************
 * Peer connection error codes
 ******************************************************************************/
#define STATUS_PEER_CONN_NULL_ARG                                 STATUS_PEER_CONN_BASE + 0x00000001
#define STATUS_PEER_CONN_CREATE_ANSWER_WITHOUT_REMOTE_DESCRIPTION STATUS_PEER_CONN_BASE + 0x00000002
#define STATUS_PEER_CONN_NO_CONNECTION                            STATUS_PEER_CONN_BASE + 0x00000003
#define STATUS_PEER_CONN_NO_SCTP_SESSION                          STATUS_PEER_CONN_BASE + 0x00000004
#define STATUS_PEER_CONN_NO_ON_MESSAGE                            STATUS_PEER_CONN_BASE + 0x00000005
#define STATUS_PEER_CONN_NOT_ENOUGH_MEMORY                        STATUS_PEER_CONN_BASE + 0x00000006
/******************************************************************************
 * Sctp error codes
 ******************************************************************************/
// #define STATUS_SCTP_SESSION_SETUP_FAILED    STATUS_SCTP_BASE + 0x00000001
// #define STATUS_SCTP_INVALID_DCEP_PACKET     STATUS_SCTP_BASE + 0x00000002
// #define STATUS_SCTP_SO_NON_BLOCKING_FAILED  STATUS_SCTP_BASE + 0x00000003
// #define STATUS_SCTP_SO_CREATE_FAILED        STATUS_SCTP_BASE + 0x00000004
// #define STATUS_SCTP_SO_BIND_FAILED          STATUS_SCTP_BASE + 0x00000005
// #define STATUS_SCTP_SO_CONNECT_FAILED       STATUS_SCTP_BASE + 0x00000006
// #define STATUS_SCTP_SO_LINGER_FAILED        STATUS_SCTP_BASE + 0x00000007
// #define STATUS_SCTP_SO_NODELAY_FAILED       STATUS_SCTP_BASE + 0x00000008
// #define STATUS_SCTP_EVENT_FAILED            STATUS_SCTP_BASE + 0x00000009
// #define STATUS_SCTP_INITMSG_FAILED          STATUS_SCTP_BASE + 0x0000000A
// #define STATUS_SCTP_PEER_ADDR_PARAMS_FAILED STATUS_SCTP_BASE + 0x0000000B
/******************************************************************************
 * Rtcp error codes
 ******************************************************************************/
// #define STATUS_RTCP_INPUT_PACKET_TOO_SMALL       STATUS_RTCP_BASE + 0x00000001
// #define STATUS_RTCP_INPUT_PACKET_INVALID_VERSION STATUS_RTCP_BASE + 0x00000002
// #define STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH    STATUS_RTCP_BASE + 0x00000003
// #define STATUS_RTCP_INPUT_NACK_LIST_INVALID      STATUS_RTCP_BASE + 0x00000004
// #define STATUS_RTCP_INPUT_SSRC_INVALID           STATUS_RTCP_BASE + 0x00000005
// #define STATUS_RTCP_INPUT_PARTIAL_PACKET         STATUS_RTCP_BASE + 0x00000006
// #define STATUS_RTCP_INPUT_REMB_TOO_SMALL         STATUS_RTCP_BASE + 0x00000007
// #define STATUS_RTCP_INPUT_REMB_INVALID           STATUS_RTCP_BASE + 0x00000008
// #define STATUS_RTCP_NULL_ARG                     STATUS_RTCP_BASE + 0x00000009
/******************************************************************************
 * Rolling buffer error codes
 ******************************************************************************/
// #define STATUS_ROLLING_BUFFER_NOT_IN_RANGE STATUS_ROLLING_BUFFER_BASE + 0x00000001
/******************************************************************************
 * Aws signature error codes
 ******************************************************************************/
#define STATUS_AWS_SIGNER_FAIL_TO_CALCULATE_HASH STATUS_AWS_SIGNER_BASE + 0x00000001
/******************************************************************************
 * Json error codes
 ******************************************************************************/
#define STATUS_JSON_PARSE_ERROR             STATUS_JSON_BASE + 0x00000001
#define STATUS_JSON_API_CALL_INVALID_RETURN STATUS_JSON_BASE + 0x00000002
/******************************************************************************
 * Http error codes
 ******************************************************************************/
#define STATUS_HTTP_RES_NOT_FOUND_ERROR       STATUS_HTTP_BASE + 0x00000001
#define STATUS_HTTP_REST_EXCEPTION_ERROR      STATUS_HTTP_BASE + 0x00000002
#define STATUS_HTTP_REST_NOT_AUTHORIZED_ERROR STATUS_HTTP_BASE + 0x00000003
#define STATUS_HTTP_REST_UNKNOWN_ERROR        STATUS_HTTP_BASE + 0x00000004
#define STATUS_HTTP_STATUS_CODE_ERROR         STATUS_HTTP_BASE + 0x00000005
#define STATUS_HTTP_PARSER_ERROR              STATUS_HTTP_BASE + 0x00000006
#define STATUS_HTTP_RSP_ERROR                 STATUS_HTTP_BASE + 0x00000007
#define STATUS_HTTP_NOT_ENOUGH_MEMORY         STATUS_HTTP_BASE + 0x00000008
#define STATUS_HTTP_BUF_OVERFLOW              STATUS_HTTP_BASE + 0x00000009
#define STATUS_HTTP_IOT_FAILED                STATUS_HTTP_BASE + 0x0000000A
/******************************************************************************
 * Wss api error codes
 ******************************************************************************/
#define STATUS_WSS_API_NULL_ARG                 STATUS_WSS_API_BASE + 0x00000001
#define STATUS_WSS_API_PARSE_RSP                STATUS_WSS_API_BASE + 0x00000002
#define STATUS_WSS_API_NOT_ENOUGH_MEMORY        STATUS_WSS_API_BASE + 0x00000003
#define STATUS_WSS_API_BUF_OVERFLOW             STATUS_WSS_API_BASE + 0x00000004
#define STATUS_WSS_API_MISSING_SIGNALING_CLIENT STATUS_WSS_API_BASE + 0x00000005
#define STATUS_WSS_API_MISSING_CONTEXT          STATUS_WSS_API_BASE + 0x00000006
/******************************************************************************
 * Wss client error codes
 ******************************************************************************/
#define STATUS_WSS_CLIENT_NULL_ARG              STATUS_WSS_CLIENT_BASE + 0x00000001
#define STATUS_WSS_CLIENT_INVALID_ARG           STATUS_WSS_CLIENT_BASE + 0x00000002
#define STATUS_WSS_CLIENT_SEND_FAILED           STATUS_WSS_CLIENT_BASE + 0x00000003
#define STATUS_WSS_CLIENT_SEND_QUEUE_MSG_FAILED STATUS_WSS_CLIENT_BASE + 0x00000004
#define STATUS_WSS_CLIENT_RECV_FAILED           STATUS_WSS_CLIENT_BASE + 0x00000005
#define STATUS_WSS_UPGRADE_CONNECTION_ERROR     STATUS_WSS_CLIENT_BASE + 0x00000006
#define STATUS_WSS_UPGRADE_PROTOCOL_ERROR       STATUS_WSS_CLIENT_BASE + 0x00000007
#define STATUS_WSS_ACCEPT_KEY_ERROR             STATUS_WSS_CLIENT_BASE + 0x00000008
#define STATUS_WSS_GENERATE_CLIENT_KEY_ERROR    STATUS_WSS_CLIENT_BASE + 0x00000009
#define STATUS_WSS_GENERATE_ACCEPT_KEY_ERROR    STATUS_WSS_CLIENT_BASE + 0x0000000A
#define STATUS_WSS_VALIDATE_ACCEPT_KEY_ERROR    STATUS_WSS_CLIENT_BASE + 0x0000000B
#define STATUS_WSS_GENERATE_RANDOM_NUM_ERROR    STATUS_WSS_CLIENT_BASE + 0x0000000C
#define STATUS_WSS_CLIENT_PING_FAILED           STATUS_WSS_CLIENT_BASE + 0x0000000D
/******************************************************************************
 * Metrics error codes
 ******************************************************************************/
#define STATUS_METRICS_NULL_ARG STATUS_METRICS_BASE + 0x00000001

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_WEBRTC_CLIENT_ERROR__ */
