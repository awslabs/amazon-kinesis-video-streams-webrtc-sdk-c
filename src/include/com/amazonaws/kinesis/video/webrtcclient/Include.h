/**
 * Main public include file
 */
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE__
#define __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE__

#pragma once

#ifdef  __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/client/Include.h>
#include <com/amazonaws/kinesis/video/common/Include.h>

// For tight packing
#pragma pack(push, include, 1) // for byte alignment

////////////////////////////////////////////////////
// Status return codes
////////////////////////////////////////////////////
#define STATUS_WEBRTC_BASE                                                          0x55000000
#define STATUS_SESSION_DESCRIPTION_INIT_NOT_OBJECT                                  STATUS_WEBRTC_BASE + 0x00000001
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP_OR_TYPE_MEMBER                  STATUS_WEBRTC_BASE + 0x00000002
#define STATUS_SESSION_DESCRIPTION_INIT_INVALID_TYPE                                STATUS_WEBRTC_BASE + 0x00000003
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_SDP                                 STATUS_WEBRTC_BASE + 0x00000004
#define STATUS_SESSION_DESCRIPTION_INIT_MISSING_TYPE                                STATUS_WEBRTC_BASE + 0x00000005
#define STATUS_SESSION_DESCRIPTION_INIT_MAX_SDP_LEN_EXCEEDED                        STATUS_WEBRTC_BASE + 0x00000006
#define STATUS_SESSION_DESCRIPTION_INVALID_SESSION_DESCRIPTION                      STATUS_WEBRTC_BASE + 0x00000007
#define STATUS_SESSION_DESCRIPTION_MISSING_ICE_VALUES                               STATUS_WEBRTC_BASE + 0x00000008
#define STATUS_SESSION_DESCRIPTION_MISSING_CERTIFICATE_FINGERPRINT                  STATUS_WEBRTC_BASE + 0x00000009

//
// SDP related errors starting from 0x56000000
//
#define STATUS_SDP_BASE                                                             STATUS_WEBRTC_BASE + 0x01000000
#define STATUS_SDP_MISSING_ITEMS                                                    STATUS_SDP_BASE + 0x00000001
#define STATUS_SDP_ATTRIBUTES_ERROR                                                 STATUS_SDP_BASE + 0x00000002
#define STATUS_SDP_BANDWIDTH_ERROR                                                  STATUS_SDP_BASE + 0x00000003
#define STATUS_SDP_CONNECTION_INFORMATION_ERROR                                     STATUS_SDP_BASE + 0x00000004
#define STATUS_SDP_EMAIL_ADDRESS_ERROR                                              STATUS_SDP_BASE + 0x00000005
#define STATUS_SDP_ENCYRPTION_KEY_ERROR                                             STATUS_SDP_BASE + 0x00000006
#define STATUS_SDP_INFORMATION_ERROR                                                STATUS_SDP_BASE + 0x00000007
#define STATUS_SDP_MEDIA_NAME_ERROR                                                 STATUS_SDP_BASE + 0x00000008
#define STATUS_SDP_ORIGIN_ERROR                                                     STATUS_SDP_BASE + 0x00000009
#define STATUS_SDP_PHONE_NUMBER_ERROR                                               STATUS_SDP_BASE + 0x0000000A
#define STATUS_SDP_TIME_DECRYPTION_ERROR                                            STATUS_SDP_BASE + 0x0000000B
#define STATUS_SDP_TIMEZONE_ERROR                                                   STATUS_SDP_BASE + 0x0000000C
#define STATUS_SDP_URI_ERROR                                                        STATUS_SDP_BASE + 0x0000000D
#define STATUS_SDP_VERSION_ERROR                                                    STATUS_SDP_BASE + 0x0000000E
#define STATUS_SDP_ATTRIBUTE_MAX_EXCEEDED                                           STATUS_SDP_BASE + 0x0000000F

//
// STUN related errors starting from 0x57000000
//
#define STATUS_STUN_BASE                                                            STATUS_SDP_BASE + 0x01000000
#define STATUS_STUN_MESSAGE_INTEGRITY_NOT_LAST                                      STATUS_STUN_BASE + 0x00000001
#define STATUS_STUN_MESSAGE_INTEGRITY_SIZE_ALIGNMENT                                STATUS_STUN_BASE + 0x00000002
#define STATUS_STUN_FINGERPRINT_NOT_LAST                                            STATUS_STUN_BASE + 0x00000003
#define STATUS_STUN_MAGIC_COOKIE_MISMATCH                                           STATUS_STUN_BASE + 0x00000004
#define STATUS_STUN_INVALID_ADDRESS_ATTRIBUTE_LENGTH                                STATUS_STUN_BASE + 0x00000005
#define STATUS_STUN_INVALID_USERNAME_ATTRIBUTE_LENGTH                               STATUS_STUN_BASE + 0x00000006
#define STATUS_STUN_INVALID_MESSAGE_INTEGRITY_ATTRIBUTE_LENGTH                      STATUS_STUN_BASE + 0x00000007
#define STATUS_STUN_INVALID_FINGERPRINT_ATTRIBUTE_LENGTH                            STATUS_STUN_BASE + 0x00000008
#define STATUS_STUN_MULTIPLE_MESSAGE_INTEGRITY_ATTRIBUTES                           STATUS_STUN_BASE + 0x00000009
#define STATUS_STUN_MULTIPLE_FINGERPRINT_ATTRIBUTES                                 STATUS_STUN_BASE + 0x0000000A
#define STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY                  STATUS_STUN_BASE + 0x0000000B
#define STATUS_STUN_MESSAGE_INTEGRITY_AFTER_FINGERPRINT                             STATUS_STUN_BASE + 0x0000000C
#define STATUS_STUN_MAX_ATTRIBUTE_COUNT                                             STATUS_STUN_BASE + 0x0000000D
#define STATUS_STUN_MESSAGE_INTEGRITY_MISMATCH                                      STATUS_STUN_BASE + 0x0000000E
#define STATUS_STUN_FINGERPRINT_MISMATCH                                            STATUS_STUN_BASE + 0x0000000F
#define STATUS_STUN_INVALID_PRIORITY_ATTRIBUTE_LENGTH                               STATUS_STUN_BASE + 0x00000010
#define STATUS_STUN_INVALID_USE_CANDIDATE_ATTRIBUTE_LENGTH                          STATUS_STUN_BASE + 0x00000011
#define STATUS_STUN_INVALID_LIFETIME_ATTRIBUTE_LENGTH                               STATUS_STUN_BASE + 0x00000012
#define STATUS_STUN_INVALID_REQUESTED_TRANSPORT_ATTRIBUTE_LENGTH                    STATUS_STUN_BASE + 0x00000013
#define STATUS_STUN_INVALID_REALM_ATTRIBUTE_LENGTH                                  STATUS_STUN_BASE + 0x00000014
#define STATUS_STUN_INVALID_NONCE_ATTRIBUTE_LENGTH                                  STATUS_STUN_BASE + 0x00000015
#define STATUS_STUN_INVALID_ERROR_CODE_ATTRIBUTE_LENGTH                             STATUS_STUN_BASE + 0x00000016
#define STATUS_STUN_INVALID_ICE_CONTROL_ATTRIBUTE_LENGTH                            STATUS_STUN_BASE + 0x00000017
#define STATUS_STUN_INVALID_CHANNEL_NUMBER_ATTRIBUTE_LENGTH                         STATUS_STUN_BASE + 0x00000018

//
// Networking related errors starting from 0x58000000
//
#define STATUS_NETWORKING_BASE                                                      STATUS_STUN_BASE + 0x01000000
#define STATUS_GET_LOCAL_IP_ADDRESSES_FAILED                                        STATUS_NETWORKING_BASE + 0x00000016
#define STATUS_CREATE_UDP_SOCKET_FAILED                                             STATUS_NETWORKING_BASE + 0x00000017
#define STATUS_BINDING_SOCKET_FAILED                                                STATUS_NETWORKING_BASE + 0x00000018
#define STATUS_GET_PORT_NUMBER_FAILED                                               STATUS_NETWORKING_BASE + 0x00000019
#define STATUS_SEND_DATA_FAILED                                                     STATUS_NETWORKING_BASE + 0x0000001a
#define STATUS_RESOLVE_HOSTNAME_FAILED                                              STATUS_NETWORKING_BASE + 0x0000001b
#define STATUS_HOSTNAME_NOT_FOUND                                                   STATUS_NETWORKING_BASE + 0x0000001c
#define STATUS_SOCKET_CONNECT_FAILED                                                STATUS_NETWORKING_BASE + 0x0000001d
#define STATUS_CREATE_SSL_FAILED                                                    STATUS_NETWORKING_BASE + 0x0000001e
#define STATUS_SSL_CONNECTION_FAILED                                                STATUS_NETWORKING_BASE + 0x0000001f
#define STATUS_SECURE_SOCKET_READ_FAILED                                            STATUS_NETWORKING_BASE + 0x00000020
#define STATUS_SECURE_SOCKET_CONNECTION_NOT_READY                                   STATUS_NETWORKING_BASE + 0x00000021
#define STATUS_SOCKET_CONNECTION_CLOSED_ALREADY                                     STATUS_NETWORKING_BASE + 0x00000022

//
// DTLS related errors starting from 0x59000000
//
#define STATUS_DTLS_BASE                                                            STATUS_NETWORKING_BASE + 0x01000000
#define STATUS_CERTIFICATE_GENERATION_FAILED                                        STATUS_DTLS_BASE + 0x00000001
#define STATUS_SSL_CTX_CREATION_FAILED                                              STATUS_DTLS_BASE + 0x00000002
#define STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED                           STATUS_DTLS_BASE + 0x00000003
#define STATUS_SSL_PACKET_BEFORE_DTLS_READY                                         STATUS_DTLS_BASE + 0x00000004
#define STATUS_SSL_UNKNOWN_SRTP_PROFILE                                             STATUS_DTLS_BASE + 0x00000005

//
// ICE related errors starting from 0x5a000000
//
#define STATUS_ICE_BASE                                                             STATUS_DTLS_BASE + 0x01000000
#define STATUS_ICE_AGENT_NO_SELECTED_CANDIDATE_AVAILABLE                            STATUS_ICE_BASE  + 0x00000001
#define STATUS_ICE_CANDIDATE_STRING_MISSING_PORT                                    STATUS_ICE_BASE  + 0x00000002
#define STATUS_ICE_CANDIDATE_STRING_MISSING_IP                                      STATUS_ICE_BASE  + 0x00000003
#define STATUS_ICE_CANDIDATE_STRING_INVALID_IP                                      STATUS_ICE_BASE  + 0x00000004
#define STATUS_ICE_CANDIDATE_STRING_IS_TCP                                          STATUS_ICE_BASE  + 0x00000005
#define STATUS_ICE_FAILED_TO_COMPUTE_MD5_FOR_LONG_TERM_CREDENTIAL                   STATUS_ICE_BASE  + 0x00000006
#define STATUS_ICE_URL_INVALID_PREFIX                                               STATUS_ICE_BASE  + 0x00000007
#define STATUS_ICE_URL_TURN_MISSING_USERNAME                                        STATUS_ICE_BASE  + 0x00000008
#define STATUS_ICE_URL_TURN_MISSING_CREDENTIAL                                      STATUS_ICE_BASE  + 0x00000009
#define STATUS_ICE_AGENT_STATE_CHANGE_FAILED                                        STATUS_ICE_BASE  + 0x0000000a
#define STATUS_ICE_NO_LOCAL_CANDIDATE_AVAILABLE                                     STATUS_ICE_BASE  + 0x0000000b
#define STATUS_ICE_AGENT_TERMINATED_ALREADY                                         STATUS_ICE_BASE  + 0x0000000c
#define STATUS_ICE_NO_CONNECTED_CANDIDATE_PAIR                                      STATUS_ICE_BASE  + 0x0000000d
#define STATUS_ICE_CANDIDATE_PAIR_LIST_EMPTY                                        STATUS_ICE_BASE  + 0x0000000e
#define STATUS_ICE_NOMINATED_CANDIDATE_NOT_CONNECTED                                STATUS_ICE_BASE  + 0x00000010
#define STATUS_ICE_CANDIDATE_INIT_MALFORMED                                         STATUS_ICE_BASE  + 0x00000011
#define STATUS_ICE_CANDIDATE_MISSING_CANDIDATE                                      STATUS_ICE_BASE  + 0x00000012
#define STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR                                STATUS_ICE_BASE  + 0x00000013
#define STATUS_ICE_MAX_REMOTE_CANDIDATE_COUNT_EXCEEDED                              STATUS_ICE_BASE  + 0x00000014
#define STATUS_ICE_INVALID_GATHERING_STATE                                          STATUS_ICE_BASE  + 0x00000015
#define STATUS_ICE_INVALID_CHECK_CONNECTION_STATE                                   STATUS_ICE_BASE  + 0x00000016
#define STATUS_ICE_INVALID_CONNECTED_STATE                                          STATUS_ICE_BASE  + 0x00000017
#define STATUS_ICE_INVALID_NOMINATING_STATE                                         STATUS_ICE_BASE  + 0x00000018
#define STATUS_ICE_INVALID_READY_STATE                                              STATUS_ICE_BASE  + 0x00000019
#define STATUS_ICE_INVALID_DISCONNECTED_STATE                                       STATUS_ICE_BASE  + 0x0000001a
#define STATUS_ICE_INVALID_FAILED_STATE                                             STATUS_ICE_BASE  + 0x0000001b
#define STATUS_ICE_INVALID_NEW_STATE                                                STATUS_ICE_BASE  + 0x0000001c
#define STATUS_ICE_NO_LOCAL_HOST_CANDIDATE_AVAILABLE                                STATUS_ICE_BASE  + 0x0000001d
#define STATUS_ICE_NO_NOMINATED_VALID_CANDIDATE_PAIR_AVAILABLE                      STATUS_ICE_BASE  + 0x0000001e
#define STATUS_TURN_CONNECTION_NO_HOST_INTERFACE_FOUND                              STATUS_ICE_BASE  + 0x0000001f
#define STATUS_TURN_CONNECTION_STATE_TRANSITION_TIMEOUT                             STATUS_ICE_BASE  + 0x00000020
#define STATUS_TURN_CONNECTION_FAILED_TO_CREATE_PERMISSION                          STATUS_ICE_BASE  + 0x00000021
#define STATUS_TURN_CONNECTION_FAILED_TO_BIND_CHANNEL                               STATUS_ICE_BASE  + 0x00000022
#define STATUS_TURN_NEW_DATA_CHANNEL_MSG_HEADER_BEFORE_PREVIOUS_MSG_FINISH          STATUS_ICE_BASE  + 0x00000023
#define STATUS_TURN_MISSING_CHANNEL_DATA_HEADER                                     STATUS_ICE_BASE  + 0x00000024
#define STATUS_ICE_FAILED_TO_RECOVER_FROM_DISCONNECTION                             STATUS_ICE_BASE  + 0x00000025
//
// SRTP related errors starting from 0x5b000000
//
#define STATUS_SRTP_BASE                                                            STATUS_ICE_BASE + 0x01000000
#define STATUS_SRTP_DECRYPT_FAILED                                                  STATUS_SRTP_BASE + 0x00000001
#define STATUS_SRTP_ENCRYPT_FAILED                                                  STATUS_SRTP_BASE + 0x00000002
#define STATUS_SRTP_TRANSMIT_SESSION_CREATION_FAILED                                STATUS_SRTP_BASE + 0x00000003
#define STATUS_SRTP_RECEIVE_SESSION_CREATION_FAILED                                 STATUS_SRTP_BASE + 0x00000004
#define STATUS_SRTP_INIT_FAILED                                                     STATUS_SRTP_BASE + 0x00000005

//
// RTP related errors starting from 0x5c000000
//
#define STATUS_RTP_BASE                                                             STATUS_SRTP_BASE + 0x01000000
#define STATUS_RTP_INPUT_PACKET_TOO_SMALL                                           STATUS_RTP_BASE + 0x00000001
#define STATUS_RTP_INPUT_MTU_TOO_SMALL                                              STATUS_RTP_BASE + 0x00000002
#define STATUS_RTP_INVALID_NALU                                                     STATUS_RTP_BASE + 0x00000003
#define STATUS_RTP_INVALID_EXTENSION_LEN                                            STATUS_RTP_BASE + 0x00000004

//
// Signaling related errors starting from 0x5d000000
//
#define STATUS_SIGNALING_BASE                                                       STATUS_RTP_BASE + 0x01000000
#define STATUS_SIGNALING_INVALID_READY_STATE                                        STATUS_SIGNALING_BASE + 0x00000001
#define STATUS_SIGNALING_GET_TOKEN_CALL_FAILED                                      STATUS_SIGNALING_BASE + 0x00000002
#define STATUS_SIGNALING_DESCRIBE_CALL_FAILED                                       STATUS_SIGNALING_BASE + 0x00000003
#define STATUS_SIGNALING_CREATE_CALL_FAILED                                         STATUS_SIGNALING_BASE + 0x00000004
#define STATUS_SIGNALING_GET_ENDPOINT_CALL_FAILED                                   STATUS_SIGNALING_BASE + 0x00000005
#define STATUS_SIGNALING_GET_ICE_CONFIG_CALL_FAILED                                 STATUS_SIGNALING_BASE + 0x00000006
#define STATUS_SIGNALING_READY_CALLBACK_FAILED                                      STATUS_SIGNALING_BASE + 0x00000007
#define STATUS_SIGNALING_CONNECT_CALL_FAILED                                        STATUS_SIGNALING_BASE + 0x00000008
#define STATUS_SIGNALING_CONNECTED_CALLBACK_FAILED                                  STATUS_SIGNALING_BASE + 0x00000009
#define STATUS_SIGNALING_INVALID_CHANNEL_INFO_VERSION                               STATUS_SIGNALING_BASE + 0x0000000A
#define STATUS_SIGNALING_INVALID_SIGNALING_CALLBACKS_VERSION                        STATUS_SIGNALING_BASE + 0x0000000B
#define STATUS_SIGNALING_INVALID_CHANNEL_NAME_LENGTH                                STATUS_SIGNALING_BASE + 0x0000000C
#define STATUS_SIGNALING_INVALID_CHANNEL_ARN_LENGTH                                 STATUS_SIGNALING_BASE + 0x0000000D
#define STATUS_SIGNALING_INVALID_REGION_LENGTH                                      STATUS_SIGNALING_BASE + 0x0000000E
#define STATUS_SIGNALING_INVALID_CPL_LENGTH                                         STATUS_SIGNALING_BASE + 0x0000000F
#define STATUS_SIGNALING_INVALID_CERTIFICATE_PATH_LENGTH                            STATUS_SIGNALING_BASE + 0x00000010
#define STATUS_SIGNALING_INVALID_AGENT_POSTFIX_LENGTH                               STATUS_SIGNALING_BASE + 0x00000011
#define STATUS_SIGNALING_INVALID_AGENT_LENGTH                                       STATUS_SIGNALING_BASE + 0x00000012
#define STATUS_SIGNALING_INVALID_KMS_KEY_LENGTH                                     STATUS_SIGNALING_BASE + 0x00000013
#define STATUS_SIGNALING_LWS_CREATE_CONTEXT_FAILED                                  STATUS_SIGNALING_BASE + 0x00000014
#define STATUS_SIGNALING_LWS_CLIENT_CONNECT_FAILED                                  STATUS_SIGNALING_BASE + 0x00000015
#define STATUS_SIGNALING_CHANNEL_BEING_DELETED                                      STATUS_SIGNALING_BASE + 0x00000016
#define STATUS_SIGNALING_INVALID_CLIENT_INFO_VERSION                                STATUS_SIGNALING_BASE + 0x00000017
#define STATUS_SIGNALING_INVALID_CLIENT_INFO_CLIENT_LENGTH                          STATUS_SIGNALING_BASE + 0x00000018
#define STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT                                       STATUS_SIGNALING_BASE + 0x00000019
#define STATUS_SIGNALING_MAX_ICE_URI_COUNT                                          STATUS_SIGNALING_BASE + 0x0000001A
#define STATUS_SIGNALING_MAX_ICE_URI_LEN                                            STATUS_SIGNALING_BASE + 0x0000001B
#define STATUS_SIGNALING_NO_CONFIG_SPECIFIED                                        STATUS_SIGNALING_BASE + 0x0000001C
#define STATUS_SIGNALING_INVALID_ICE_CONFIG_INFO_VERSION                            STATUS_SIGNALING_BASE + 0x0000001D
#define STATUS_SIGNALING_NO_CONFIG_URI_SPECIFIED                                    STATUS_SIGNALING_BASE + 0x0000001E
#define STATUS_SIGNALING_NO_ARN_RETURNED_ON_CREATE                                  STATUS_SIGNALING_BASE + 0x0000001F
#define STATUS_SIGNALING_MISSING_ENDPOINTS_IN_GET_ENDPOINT                          STATUS_SIGNALING_BASE + 0x00000020
#define STATUS_SIGNALING_INVALID_MESSAGE_TYPE                                       STATUS_SIGNALING_BASE + 0x00000021
#define STATUS_SIGNALING_INVALID_SIGNALING_MESSAGE_VERSION                          STATUS_SIGNALING_BASE + 0x00000022
#define STATUS_SIGNALING_NO_PEER_CLIENT_ID_IN_MESSAGE                               STATUS_SIGNALING_BASE + 0x00000023
#define STATUS_SIGNALING_MESSAGE_DELIVERY_FAILED                                    STATUS_SIGNALING_BASE + 0x00000024
#define STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING                             STATUS_SIGNALING_BASE + 0x00000025
#define STATUS_SIGNALING_RECEIVE_BINARY_DATA_NOT_SUPPORTED                          STATUS_SIGNALING_BASE + 0x00000026
#define STATUS_SIGNALING_RECEIVE_EMPTY_DATA_NOT_SUPPORTED                           STATUS_SIGNALING_BASE + 0x00000027
#define STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN                  STATUS_SIGNALING_BASE + 0x00000028
#define STATUS_SIGNALING_INVALID_PAYLOAD_LEN_IN_MESSAGE                             STATUS_SIGNALING_BASE + 0x00000029
#define STATUS_SIGNALING_NO_PAYLOAD_IN_MESSAGE                                      STATUS_SIGNALING_BASE + 0x0000002A
#define STATUS_SIGNALING_DUPLICATE_MESSAGE_BEING_SENT                               STATUS_SIGNALING_BASE + 0x0000002B
#define STATUS_SIGNALING_ICE_TTL_LESS_THAN_GRACE_PERIOD                             STATUS_SIGNALING_BASE + 0x0000002C
#define STATUS_SIGNALING_DISCONNECTED_CALLBACK_FAILED                               STATUS_SIGNALING_BASE + 0x0000002D

//
// PeerConnection related errors starting from 0x5e000000
//
#define STATUS_PEERCONNECTION_BASE                                                  STATUS_SIGNALING_BASE + 0x01000000
#define STATUS_PEERCONNECTION_CREATE_ANSWER_WITHOUT_REMOTE_DESCRIPTION              STATUS_PEERCONNECTION_BASE + 0x00000001
#define STATUS_PEERCONNECTION_CODEC_INVALID                                         STATUS_PEERCONNECTION_BASE + 0x00000002
#define STATUS_PEERCONNECTION_CODEC_MAX_EXCEEDED                                    STATUS_PEERCONNECTION_BASE + 0x00000003

//
// SCTP related errors starting from 0x5f000000
//
#define STATUS_SCTP_BASE                                                            STATUS_PEERCONNECTION_BASE + 0x01000000
#define STATUS_SCTP_SESSION_SETUP_FAILED                                            STATUS_SCTP_BASE + 0x00000001
#define STATUS_SCTP_INVALID_DCEP_PACKET                                             STATUS_SCTP_BASE + 0x00000002

//
// RTCP related errors starting from 0x60000000
//
#define STATUS_RTCP_BASE                                                            STATUS_SCTP_BASE + 0x01000000
#define STATUS_RTCP_INPUT_PACKET_TOO_SMALL                                          STATUS_RTCP_BASE + 0x00000001
#define STATUS_RTCP_INPUT_PACKET_INVALID_VERSION                                    STATUS_RTCP_BASE + 0x00000002
#define STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH                                       STATUS_RTCP_BASE + 0x00000003
#define STATUS_RTCP_INPUT_NACK_LIST_INVALID                                         STATUS_RTCP_BASE + 0x00000004
#define STATUS_RTCP_INPUT_SSRC_INVALID                                              STATUS_RTCP_BASE + 0x00000005

//
// RollingBuffer related errors starting from 0x61000000
//
#define STATUS_ROLLING_BUFFER_BASE                                                  STATUS_RTCP_BASE + 0x01000000
#define STATUS_ROLLING_BUFFER_NOT_IN_RANGE                                          STATUS_ROLLING_BUFFER_BASE + 0x00000001

////////////////////////////////////////////////////
// Main defines
////////////////////////////////////////////////////

/**
 * Max channel name length
 */
#define MAX_CHANNEL_NAME_LEN                                                        256

/**
 * Max client id length
 */
#define MAX_SIGNALING_CLIENT_ID_LEN                                                 256

/**
 * Max Ice config user name length
 */
#define MAX_ICE_CONFIG_USER_NAME_LEN                                                512

/**
 * Max Ice config password length
 */
#define MAX_ICE_CONFIG_CREDENTIAL_LEN                                               512

/**
 * Max Signaling endpoint Uri length
 */
#define MAX_SIGNALING_ENDPOINT_URI_LEN                                              512

/**
 * Max Ice Uri length
 */
#define MAX_ICE_CONFIG_URI_LEN                                                      256

/**
 * Max number of URIs in the Ice configuration
 */
#define MAX_ICE_CONFIG_URI_COUNT                                                    4

/**
 * Max Ice configuration object count
 */
#define MAX_ICE_CONFIG_COUNT                                                        5

/**
 * Max correlation id string length
 */
#define MAX_CORRELATION_ID_LEN                                                      256

/**
 * Max error type string length
 */
#define MAX_ERROR_TYPE_STRING_LEN                                                   256

/**
 * Max status code string length
 */
#define MAX_STATUS_CODE_STRING_LEN                                                  256

/**
 * Max message description string length
 */
#define MAX_MESSAGE_DESCRIPTION_LEN                                                 1024

/**
 * Max ice servers for a RtcPeerConnection
 *
 * Max number of configurations returned by get ICE config times the max number of server URIs per config plus single STUN
 */
#define MAX_ICE_SERVERS_COUNT                                                       (MAX_ICE_CONFIG_COUNT * MAX_ICE_CONFIG_URI_COUNT + 1)

// Parameterized string for KVS STUN Server
#define KINESIS_VIDEO_STUN_URL                                                     "stun:stun.kinesisvideo.%s.amazonaws.com:443"

// Default SSL port
#define SIGNALING_DEFAULT_SSL_PORT                                                  DEFAULT_SSL_PORT_NUMBER

// Default non-SSL port
#define SIGNALING_DEFAULT_NON_SSL_PORT                                              DEFAULT_NON_SSL_PORT_NUMBER

// Default timeout for the signaling client create
#define SIGNALING_CREATE_TIMEOUT                                                    (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Default timeout for the overall connect sync API
#define SIGNALING_CONNECT_STATE_TIMEOUT                                             (15 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Default timeout for the connection establishment
#define SIGNALING_CONNECT_TIMEOUT                                                   (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Default timeout for sending data
#define SIGNALING_SEND_TIMEOUT                                                      (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Default message ttl value
#define SIGNALING_DEFAULT_MESSAGE_TTL_VALUE                                         (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Max signaling message payload length
#define MAX_SIGNALING_MESSAGE_LEN                                                   (10 * 1024)

/**
 * Maximum size of SDP member in RtcSessionDescriptionInit
 */
#define MAX_SESSION_DESCRIPTION_INIT_SDP_LEN                                        25000

/*
 * Maximum size of a MediaStream's ID
 */
#define MAX_MEDIA_STREAM_ID_LEN                                                     255

/*
 * Maximum size of a MediaStream's Track ID
 */
#define MAX_MEDIA_STREAM_TRACK_ID_LEN                                               255

/*
 * Maximum size of candidate member of ICECandidateInit
 */
#define MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN                                        255

/*
 * Maximum size of DataChannel name
 */
#define MAX_DATA_CHANEL_NAME_LEN                                                    255

/**
 * Current versions of the structures
 */
#define PEER_CONNECTION_CURRENT_VERSION                                             0
#define CHANNEL_INFO_CURRENT_VERSION                                                0
#define SIGNALING_CLIENT_INFO_CURRENT_VERSION                                       0
#define SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION                                  0
#define SIGNALING_CLIENT_CURRENT_VERSION                                            0
#define SIGNALING_CHANNEL_DESCRIPTION_CURRENT_VERSION                               0
#define SIGNALING_ICE_CONFIG_INFO_CURRENT_VERSION                                   0
#define SIGNALING_MESSAGE_CURRENT_VERSION                                           0

/**
 * Max sequence number in rtp packet/jitter buffer
 */
#define MAX_SEQUENCE_NUM                                                            ((UINT32) MAX_UINT16)

/**
 * Default jitter buffer tolerated latency, frame will be dropped if it is out of window
 */
#define DEFAULT_JITTER_BUFFER_MAX_LATENCY                                           (2000L * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

////////////////////////////////////////////////////
// Extra callbacks definitions
////////////////////////////////////////////////////
/*
 * RtcOnFrame is fired everytime a frame is received from
 * the remote peer. It is available via the RtpRec
 *
 * RtcOnFrame is a KVS specific method
 *
 */
typedef VOID (*RtcOnFrame)(UINT64, PFrame);

/*
 * RtcOnMessage is fired when a message is received for the DataChannel
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcdatachannel-onmessage
 */
typedef VOID (*RtcOnMessage)(UINT64, BOOL, PBYTE, UINT32);

/*
 * RtcOnDataChannel is fired when the remote PeerConnection
 * creates a new DataChannel
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-ondatachannel
 */
typedef struct __RtcDataChannel RtcDataChannel;
typedef VOID (*RtcOnDataChannel)(UINT64, RtcDataChannel*);

/*
 * RtcOnIceCandidate is fired when new iceCandidate is found. if PCHAR is NULL then candidate gathering is done.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-ondatachannel
 */
typedef VOID (*RtcOnIceCandidate)(UINT64, PCHAR);

/*
 * https://www.w3.org/TR/webrtc/#rtcpeerconnectionstate-enum
 */
typedef enum {
    RTC_PEER_CONNECTION_STATE_NONE          = 0, // dummy state
    RTC_PEER_CONNECTION_STATE_NEW           = 1,
    RTC_PEER_CONNECTION_STATE_CONNECTING    = 2,
    RTC_PEER_CONNECTION_STATE_CONNECTED     = 3,
    RTC_PEER_CONNECTION_STATE_DISCONNECTED  = 4,
    RTC_PEER_CONNECTION_STATE_FAILED        = 5,
    RTC_PEER_CONNECTION_STATE_CLOSED        = 6,
} RTC_PEER_CONNECTION_STATE;

/*
 * RtcOnConnectionStateChange is fired to report a change in peer connection state.
 *
 * https://www.w3.org/TR/webrtc/#event-iceconnectionstatechange
 */
typedef VOID (*RtcOnConnectionStateChange)(UINT64, RTC_PEER_CONNECTION_STATE);


////////////////////////////////////////////////////
// Main structure declarations
////////////////////////////////////////////////////
/**
 * Definition of the signaling client handle
 */
typedef UINT64 SIGNALING_CLIENT_HANDLE;
typedef SIGNALING_CLIENT_HANDLE* PSIGNALING_CLIENT_HANDLE;

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_SIGNALING_CLIENT_HANDLE_VALUE
#define INVALID_SIGNALING_CLIENT_HANDLE_VALUE ((SIGNALING_CLIENT_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_SIGNALING_CLIENT_HANDLE
#define IS_VALID_SIGNALING_CLIENT_HANDLE(h) ((h) != INVALID_SIGNALING_CLIENT_HANDLE_VALUE)
#endif

typedef enum {
    // SessionDescription is type offer
    SDP_TYPE_OFFER = 1,

    // SessionDescription is type answer
    SDP_TYPE_ANSWER = 2,
} SDP_TYPE;

typedef enum {
    MEDIA_STREAM_TRACK_KIND_AUDIO = 1,
    MEDIA_STREAM_TRACK_KIND_VIDEO = 2,
} MEDIA_STREAM_TRACK_KIND;

typedef enum {
    RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE = 1,
    RTC_CODEC_OPUS = 2,
    RTC_CODEC_VP8 = 3,
    RTC_CODEC_MULAW = 4,
    RTC_CODEC_ALAW = 5,
} RTC_CODEC;

/* ICE_TRANSPORT_POLICY restrict which ICE candidates are used in a session.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcicetransportpolicy
 *
 */
typedef enum {
    // The ICE Agent uses only media relay candidates
    // such as candidates passing through a TURN server.
    ICE_TRANSPORT_POLICY_RELAY = 1,

    // The ICE Agent can use any type of candidate when this value is specified.
    ICE_TRANSPORT_POLICY_ALL = 2,
} ICE_TRANSPORT_POLICY;

/* RTC_RTP_TRANSCEIVER_DIRECTION indicates direction of a transceiver
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverdirection
 *
 */
typedef enum {
    RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1,
    RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2,
    RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3,
} RTC_RTP_TRANSCEIVER_DIRECTION;


/**
 * An RtcPeerConnection instance allows an application to establish peer-to-peer
 * communications with another RtcPeerConnection, or to another endpoint implementing
 * the required protocols
 *
 *  https://www.w3.org/TR/webrtc/#introduction
 */
typedef struct {
    UINT32 version;
} RtcPeerConnection, *PRtcPeerConnection;

/*
 * RtcMediaStreamTrack represents a single track in a MediaStream
 *
 * https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
 */
typedef struct {
    RTC_CODEC codec;                            // non-standard, codec that this track is using
    CHAR trackId[MAX_MEDIA_STREAM_ID_LEN + 1];  // non-standard, id of this individual track
    CHAR streamId[MAX_MEDIA_STREAM_ID_LEN + 1]; // non-standard, id of the MediaStream this track belongs too
    MEDIA_STREAM_TRACK_KIND kind;
} RtcMediaStreamTrack, *PRtcMediaStreamTrack;

/* RTCRtpReceiver allows an application to inspect the
 * receipt of a MediaStreamTrack.
 *
 * Kvs extends this interface allowing users to receive
 * complete frames from the remote connection.
 *
 * https://www.w3.org/TR/webrtc/#rtcrtpreceiver-interface
 */
typedef struct {
    RtcMediaStreamTrack track;
} RtcRtpReceiver, *PRtcRtpReceiver;

/**
 *
 * The RTCRtpTransceiver represents a combination of an RTCRtpSender and an RTCRtpReceiver
 * that share a common mid.
 *
 *  https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiver
 */
typedef struct {
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
    RtcRtpReceiver receiver;
} RtcRtpTransceiver, *PRtcRtpTransceiver;

/*
 * RtcIceServer is used to describe the STUN and TURN servers that
 * can be used by the ICE Agent to establish a connection with a peer.
 *
 * https://www.w3.org/TR/webrtc/#rtciceserver-dictionary
 */
typedef struct {
    CHAR urls[MAX_ICE_CONFIG_URI_LEN + 1];
    CHAR username[MAX_ICE_CONFIG_USER_NAME_LEN + 1];
    CHAR credential[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
} RtcIceServer, *PRtcIceServer;

/**
 *  The Configuration defines a set of parameters to configure how the peer-to-peer
 *  communication established via RtcPeerConnection is established or re-established.
 *  https://www.w3.org/TR/webrtc/#rtcconfiguration-dictionary
 */
typedef struct {
    // Indicates which candidates the ICE Agent is allowed to use.
    ICE_TRANSPORT_POLICY iceTransportPolicy;

    RtcIceServer iceServers[MAX_ICE_SERVERS_COUNT];
} RtcConfiguration, *PRtcConfiguration;

/*
 * SessionDescription is used by RtcPeerConnection to expose local
 * and remote session descriptions.
 *
 * https://www.w3.org/TR/webrtc/#rtcsessiondescription-class
 */
typedef struct {
    SDP_TYPE type;
    CHAR     sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1];
} RtcSessionDescriptionInit, *PRtcSessionDescriptionInit;

/*
 * Rtc ICE candidate interface.
 *
 * https://www.w3.org/TR/webrtc/#rtcicecandidate-interface
 */
typedef struct {
    CHAR candidate[MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN + 1];
} RtcIceCandidateInit, *PRtcIceCandidateInit;

/**
 * Channel status as reported by the service
 */
typedef enum {
    // Channel is being created
    SIGNALING_CHANNEL_STATUS_CREATING,

    // Channel has been created and is active
    SIGNALING_CHANNEL_STATUS_ACTIVE,

    // Channel is being updated
    SIGNALING_CHANNEL_STATUS_UPDATING,

    // Channel is being deleted
    SIGNALING_CHANNEL_STATUS_DELETING,
} SIGNALING_CHANNEL_STATUS;

/**
 * Signaling message type enum
 */
typedef enum {
    // Type offer
    SIGNALING_MESSAGE_TYPE_OFFER,

    // Type answer
    SIGNALING_MESSAGE_TYPE_ANSWER,

    // ICE candidate
    SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE,

    // Go away message
    SIGNALING_MESSAGE_TYPE_GO_AWAY,

    // Reconnect with new ICE server
    SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER,

    // Status response for a message
    SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE,

    // Unknown message type
    SIGNALING_MESSAGE_TYPE_UNKNOWN,
} SIGNALING_MESSAGE_TYPE;

/**
 * Signaling message type enum
 */
typedef enum {
    // Unknown state
    SIGNALING_CLIENT_STATE_UNKNOWN,

    // New state
    SIGNALING_CLIENT_STATE_NEW,

    // Get security credentials
    SIGNALING_CLIENT_STATE_GET_CREDENTIALS,

    // Describe channel state
    SIGNALING_CLIENT_STATE_DESCRIBE,

    // Create channel state
    SIGNALING_CLIENT_STATE_CREATE,

    // Get channel endpoint state
    SIGNALING_CLIENT_STATE_GET_ENDPOINT,

    // Get ICE configuration
    SIGNALING_CLIENT_STATE_GET_ICE_CONFIG,

    // Ready state
    SIGNALING_CLIENT_STATE_READY,

    // Connecting state
    SIGNALING_CLIENT_STATE_CONNECTING,

    // Connected state
    SIGNALING_CLIENT_STATE_CONNECTED,

    // Disconnected state
    SIGNALING_CLIENT_STATE_DISCONNECTED,
} SIGNALING_CLIENT_STATE;

/**
 * Channel type as reported by the service
 */
typedef enum {
    // Channel is unknown
    SIGNALING_CHANNEL_TYPE_UNKNOWN,

    // Channel is master/viewer
    SIGNALING_CHANNEL_TYPE_SINGLE_MASTER,
} SIGNALING_CHANNEL_TYPE;

/**
 * Channel role type
 */
typedef enum {
    // Channel is unknown
    SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN,

    // Channel role is master
    SIGNALING_CHANNEL_ROLE_TYPE_MASTER,

    // Channel role is master
    SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,
} SIGNALING_CHANNEL_ROLE_TYPE;

/**
 * Signaling message structure
 */
typedef struct {
    // Current version of the structure
    UINT32 version;

    // Message type
    SIGNALING_MESSAGE_TYPE messageType;

    // Correlation Id string
    CHAR correlationId[MAX_CHANNEL_NAME_LEN + 1];

    // Sender client id
    CHAR peerClientId[MAX_SIGNALING_CLIENT_ID_LEN + 1];

    // Payload length. Optional. Will calculate the length if 0
    UINT32 payloadLen;

    // Payload
    CHAR payload[MAX_SIGNALING_MESSAGE_LEN + 1];
} SignalingMessage, *PSignalingMessage;

/**
 * Received Signaling message structure
 */
typedef struct {
    // The signaling message
    SignalingMessage signalingMessage;

    // Response status code
    SERVICE_CALL_RESULT statusCode;

    // Error type if any
    CHAR errorType[MAX_ERROR_TYPE_STRING_LEN + 1];

    // Description if any
    CHAR description[MAX_MESSAGE_DESCRIPTION_LEN + 1];
} ReceivedSignalingMessage, *PReceivedSignalingMessage;

/**
 * Client info struct
 */
typedef struct {
    // Version of the structure
    UINT32 version;

    // Client id to use
    CHAR clientId[MAX_SIGNALING_CLIENT_ID_LEN + 1];

    // Verbosity level for the logging. One of LOG_LEVEL_XXX values or the default verbosity will be assumed
    UINT32 loggingLevel;
} SignalingClientInfo, *PSignalingClientInfo;

/**
 * Channel info struct
 */
typedef struct {
    // Version of the structure
    UINT32 version;

    // Channel name
    PCHAR pChannelName; // [MAX_CHANNEL_NAME_LEN + 1]

    // Optional ARN.
    PCHAR pChannelArn; // [MAX_ARN_LEN + 1];

    // AWS Region. Empty for default
    PCHAR pRegion; // [MAX_REGION_NAME_LEN + 1]

    // Optional fully qualified control plane URI
    PCHAR pControlPlaneUrl; // [MAX_URI_CHAR_LEN + 1]

    // Optional certificate path
    PCHAR pCertPath; // [MAX_PATH_LEN + 1]

    // Optional user agent post-fix
    PCHAR pUserAgentPostfix; // [MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN + 1]

    // Optional custom user agent name
    PCHAR pCustomUserAgent; // [MAX_CUSTOM_USER_AGENT_LEN + 1]

    // Combined user agent
    PCHAR pUserAgent; // [MAX_USER_AGENT_LEN + 1]

    // Optional KMS key id ARN
    PCHAR pKmsKeyId; // [MAX_ARN_LEN + 1]

    // Channel type when creating
    SIGNALING_CHANNEL_TYPE channelType;

    // Channel role type for the endpoint
    SIGNALING_CHANNEL_ROLE_TYPE channelRoleType;

    // Whether to cache the endpoint
    BOOL cachingEndpoint;

    // Endpoint caching TTL
    UINT64 endpointCachingPeriod;

    // Whether to retry the network calls on errors up to max retry times
    BOOL retry;

    // Whether to reconnect on connection dropped
    BOOL reconnect;

    // Number of tags associated with the stream
    UINT32 tagCount;

    // Stream tags array
    PTag pTags;
} ChannelInfo, *PChannelInfo;

/**
 * ICE configuration information struct
 *
 * NOTE: Each ICE configuration has an array of ICE URIs.
 * The actual URI count is specified in uriCount member.
 *
 * NOTE:TTL is given in default - 100ns duration
 */
typedef struct {
    // Version of the struct
    UINT32 version;

    // TTL of the configuration in 100ns
    UINT64 ttl;

    // Number of Ice Uri objects
    UINT32 uriCount;

    // Ice server Uris
    CHAR uris[MAX_ICE_CONFIG_URI_COUNT][MAX_ICE_CONFIG_URI_LEN + 1];

    // Username for the server
    CHAR userName[MAX_ICE_CONFIG_USER_NAME_LEN + 1];

    // Password for the server
    CHAR password[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1];
} IceConfigInfo, *PIceConfigInfo;

/**
 * Callback that is fired when Signalling client receives an Offer
 *
 * NOTE: Returning non-success status will terminate the internal event loop and will force
 * the state machinery to roll back to the state corresponding to the type of status returned.
 *
 * @param - UINT64 - Custom data passed in to the signaling client
 * @param - PReceivedSignalingMessage - Pointer to the received message
 *
 * @return - STATUS code of the operation
 */
typedef STATUS (*SignalingClientMessageReceivedFunc)(UINT64, PReceivedSignalingMessage);

/**
 * Callback that is fired on error.
 *
 * NOTE: This callback is optional and can be set to NULL.
 *
 * NOTE: Returning non-success status will terminate the internal event loop and will force
 * the state machinery to roll back to the state corresponding to the type of status returned.
 *
 * @param - UINT64 - Custom data passed in to the signaling client
 * @param - STATUS - The status code of the error
 * @param - PCHAR - Variable - can point to an error string or other information
 * @param - UINT32 - Length of the message
 *
 * @return - STATUS code of the operation
 */
typedef STATUS (*SignalingClientErrorReportFunc)(UINT64, STATUS, PCHAR, UINT32);

/**
 * Callback that is fired on signaling client state change.
 *
 * NOTE: This callback is optional and can be set to NULL.
 *
 * NOTE: Returning non-success status will terminate the internal event loop and will force
 * the state machinery to roll back to the state corresponding to the type of status returned.
 *
 * @param - UINT64 - Custom data passed in to the signaling client
 * @param - SIGNALING_CLIENT_STATE - The new state
 *
 * @return - STATUS code of the operation
 */
typedef STATUS (*SignalingClientStateChangedFunc)(UINT64, SIGNALING_CLIENT_STATE);

/**
 * Signaling client callbacks
 */
typedef struct {
    // Current version of the structure
    UINT32 version;

    // Custom data passed by the caller
    UINT64 customData;

    // SDP received callback
    SignalingClientMessageReceivedFunc messageReceivedFn;

    // Error reporting function - Optional
    SignalingClientErrorReportFunc errorReportFn;

    // Signaling client state change callback
    SignalingClientStateChangedFunc stateChangeFn;
} SignalingClientCallbacks, *PSignalingClientCallbacks;

/**
 * Signaling channel description returned from the service
 */
typedef struct {
    // Version of the struct
    UINT32 version;

    // Channel ARN
    CHAR channelArn[MAX_ARN_LEN + 1];

    // Channel name - human readable. Null terminated.
    // Should be unique per AWS account.
    CHAR channelName[MAX_CHANNEL_NAME_LEN + 1];

    // Current channel status
    SIGNALING_CHANNEL_STATUS channelStatus;

    // Channel type
    SIGNALING_CHANNEL_TYPE channelType;

    // Update version.
    CHAR updateVersion[MAX_UPDATE_VERSION_LEN + 1];

    // Message TTL in 100ns
    UINT64 messageTtl;

    // Channel creation time
    UINT64 creationTime;
} SignalingChannelDescription, *PSignalingChannelDescription;

/*
 * RtcRtpTransceiverInit is used to configure a transceiver when creating it
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverinit
 */
typedef struct {
    RTC_RTP_TRANSCEIVER_DIRECTION direction;
} RtcRtpTransceiverInit, *PRtcRtpTransceiverInit;

/*
 * RtcDataChannel represents a bi-directional data channel between two peers.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcdatachannel
 */
typedef struct __RtcDataChannel {
    CHAR name[MAX_DATA_CHANEL_NAME_LEN];
} RtcDataChannel, *PRtcDataChannel;

////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////

/**
 * Initialize a RtcPeerConnection with the provided Configuration
 * https://www.w3.org/TR/webrtc/#constructor
 *
 * @param - PConfiguration - IN - Configuration to initialize provided RtcPeerConnection
 * @param - PRtcPeerConnection - IN/OUT - Initialized and configured RtcPeerConnection
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createPeerConnection(PRtcConfiguration, PRtcPeerConnection*);

/**
 * Free a RtcPeerConnection
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection that will be freed
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freePeerConnection(PRtcPeerConnection*);

/**
 * Set a callback when new Ice collects new local candidate. When IceAgent is done with collecting candidates,
 * RtcOnIceCandidate will be called with NULL.
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection struct
 * @param - UINT64 - IN - User customData that will be passed along when RtcOnIceCandidate is called
 * @param - RtcOnIceCandidate - IN - User callback when new local candidate is found
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS peerConnectionOnIceCandidate(PRtcPeerConnection, UINT64, RtcOnIceCandidate);

/**
 * Set a callback for data channel
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection struct
 * @param - UINT64 - IN - User customData that will be passed along when RtcOnDataChannel is called
 * @param - RtcOnIceCandidate - IN - User RtcOnDataChannel callback
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS peerConnectionOnDataChannel(PRtcPeerConnection, UINT64, RtcOnDataChannel);

/**
 * Set a callback for connection state change
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection struct
 * @param - UINT64 - IN - User customData that will be passed along when RtcOnDataChannel is called
 * @param - RtcOnIceCandidate - IN - User RtcOnConnectionStateChange callback
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS peerConnectionOnConnectionStateChange(PRtcPeerConnection, UINT64, RtcOnConnectionStateChange);

/**
 * Load the sdp field of PRtcSessionDescriptionInit with latest local session description
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection struct
 * @param - PRtcSessionDescriptionInit - IN/OUT - PRtcSessionDescriptionInit whose sdp field will be modified.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS peerConnectionGetCurrentLocalDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * createOffer method populates the provided answer that contains an RFC 3264 offer
 * with the supported configurations for the session.
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-createoffer
 *
 * @param - PRtcPeerConnection - IN - Initialized RtcPeerConnection
 * @param - PRtcSessionDescriptionInit - IN/OUT - answer that describes the supported configurations of the RtcPeerConnection
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createOffer(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * createAnswer method populates the provided answer that contains an RFC 3264 answer
 * with the supported configurations for the session.
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-createanswer
 *
 * @param - PRtcPeerConnection - IN - Initialized RtcPeerConnection
 * @param - PRtcSessionDescriptionInit - IN/OUT - answer that describes the supported configurations of the RtcPeerConnection
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createAnswer(PRtcPeerConnection, PRtcSessionDescriptionInit);


/**
 * serializeSessionDescriptionInit creates a JSON string from RtcSessionDescriptionInit

 * @param - PRtcSessionDescriptionInit - IN - Source RtcSessionDescriptionInit that will become JSON string
 * @param - PCHAR - OUT - JSON string generated from PRtcSessionDescriptionInit
 * @param - PUINT32 - OUT - If PCHAR is null this is the required buffer size. If PCHAR is non-NULL this is the length of the output
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS serializeSessionDescriptionInit(PRtcSessionDescriptionInit, PCHAR, PUINT32);


/**
 * deserializeSessionDescriptionInit parses a JSON string and returns a
 * allocated PSessionDescriptionInit

 * @param - PCHAR - IN - JSON String of a RtcSessionDescriptionInit
 * @param - UINT32 - IN - Length of JSON String
 * @param - PRtcSessionDescriptionInit - OUT - RtcSessionDescriptionInit populated from JSON String
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS deserializeSessionDescriptionInit(PCHAR, UINT32, PRtcSessionDescriptionInit);

/**
 * deserializeRtcIceCandidateInit parses a JSON string and populates a PRtcIceCandidateInit

 * @param - PCHAR - IN - JSON String of a PRtcIceCandidateInit
 * @param - UINT32 - IN - Length of JSON String
 * @param - PRtcIceCandidateInit - OUT - PRtcIceCandidateInit populated from JSON String
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS deserializeRtcIceCandidateInit(PCHAR, UINT32, PRtcIceCandidateInit);

/*
 * setLocalDescription method instructs the RtcPeerConnection to apply
 * the supplied RtcSessionDescriptionInit as the local description.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-setlocaldescription
 *
 * @param - PRtcPeerConnection - IN - Initialized RtcPeerConnection
 * @param - PRtcSessionDescriptionInit - IN/OUT - SessionDescriptionInit that becomes our new local description
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setLocalDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/*
 * setRemoteDescription method instructs the RtcPeerConnection to apply
 * the supplied RtcSessionDescriptionInit as the remote description.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-setremotedescription
 *
 * @param - PRtcPeerConnection - IN - Initialized RtcPeerConnection
 * @param - PRtcSessionDescriptionInit - IN/OUT - RtcSessionDescriptionInit that becomes our new remote description
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setRemoteDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/*
 * addTransceiver create a new RtcRtpTransceiver and add it to the set of transceivers.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-addtransceiver
 * @param - PRtcPeerConnection - IN - Initialized RtcPeerConnection
 * @param - PRtcMediaStreamTrack - IN - RtcMediaStreamTrack that is the codec we are sending, or NULL for RECVONLY
 * @param - PRtcRtpTransceiverInit - IN - PRtcRtpTransceiverInit that may configure our new Transceiver
 * @param - *PRtcRtpTransceiver - IN/OUT - Initialized and configured RtcRtpTransceiver
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS addTransceiver(PRtcPeerConnection, PRtcMediaStreamTrack, PRtcRtpTransceiverInit, PRtcRtpTransceiver*);

/**
 * Set a callback for transceiver frame
 *
 * @param - PRtcRtpTransceiver* - IN - RtcRtpTransceiver struct
 * @param - UINT64 - IN - User customData that will be passed along when RtcOnFrame is called
 * @param - RtcOnFrame - IN - User RtcOnFrame callback
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS transceiverOnFrame(PRtcRtpTransceiver, UINT64, RtcOnFrame);

/*
 * freeTransceiver frees the previously created transceiver object
 *
 * @param - *PRtcRtpTransceiver - IN/OUT/OPT - RtcRtpTransceiver to be freed
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freeTransceiver(PRtcRtpTransceiver*);

/*
 * initKvsWebRtc initializes global state needed for all RtcPeerConnection's it must only be called once
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS initKvsWebRtc(VOID);

/*
 * addSupportedCodec adds to the list of codecs we support receiving. The remote MUST only send codecs we declare
 *
 * @param - RTC_CODEC - IN - Codec that we support receiving.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS addSupportedCodec(PRtcPeerConnection, RTC_CODEC);

/*
 * writeFrame packetizes and sends media via the configuration specified by the RtcRtpTransceiver
 *
 * @param - PRtcRtpTransceiver - IN - Configured and connected RtcRtpTransceiver to send media via
 * @param - PFrame - IN - Frame of media that will be sent
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS writeFrame(PRtcRtpTransceiver, PFrame);

/*
 * addIceCandidate method provides a remote candidate to the ICE Agent. This method can also be used to
 * indicate the end of remote candidates when called with an empty string for the candidate member.
 *
 * https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-addicecandidate
 */
STATUS addIceCandidate(PRtcPeerConnection, PCHAR);

/**
 * Set a callback for data channel message
 *
 * @param - PRtcPeerConnection* - IN - RtcPeerConnection struct
 * @param - UINT64 - IN - User customData that will be passed along when RtcOnMessage is called
 * @param - RtcOnMessage - IN - User RtcOnMessage callback
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS dataChannelOnMessage(PRtcDataChannel, UINT64, RtcOnMessage);

/*
 * dataChannelSend send data via the PRtcDataChannel
 *
 * @param - PRtcDataChannel - IN - Configured and connected PRtcDataChannel
 * @param - BOOL - IN - Is message binary, if false will be delivered as a string
 * @param - PBYTE - IN - Data that you wish to send
 * @param - UINT32 - IN - Length of the PBYTE you wish to send
 *
 * https://www.w3.org/TR/webrtc/#dfn-send
 */
STATUS dataChannelSend(PRtcDataChannel, BOOL, PBYTE, UINT32);

/*
 * Creates a Signaling client and returns a handle to it
 *
 * @param - PSignalingClientInfo - IN - Signaling client info
 * @param - PChannelInfo - IN - Signaling channel info to use/create a channel
 * @param - PSignalingClientCallbacks - IN - Signaling callbacks for event notifications
 * @param - PAwsCredentialProvider - IN - Credential provider for auth integration
 * @param - PSIGNALING_CLIENT_HANDLE - OUT - Returned signaling client handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createSignalingClientSync(PSignalingClientInfo, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSIGNALING_CLIENT_HANDLE);

/*
 * Frees the Signaling client object
 *
 * NOTE: The call is idempotent.
 *
 * @param - PSIGNALING_CLIENT_HANDLE - IN/OUT/OPT - Signaling client handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freeSignalingClient(PSIGNALING_CLIENT_HANDLE);

/*
 * Send a message through a Signaling client.
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 * NOTE: This is a synchronous call. It will block and wait for sending the data and await for the ACK from the service.
 *
 * @param - SIGNALING_CLIENT_HANDLE - IN - Signaling client handle
 * @param - PSignalingMessage - IN - Message to send.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientSendMessageSync(SIGNALING_CLIENT_HANDLE, PSignalingMessage);

/*
 * Gets the retrieved ICE configuration information object count
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 *
 * @param - SIGNALING_CLIENT_HANDLE - IN - Signaling client handle
 * @param - PUINT32 - OUT - The count of the ICE configuration information objects
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientGetIceConfigInfoCout(SIGNALING_CLIENT_HANDLE, PUINT32);

/*
 * Gets the ICE configuration information object given its index
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 * IMPORTANT: The returned pointer to the ICE configuration information object points to internal structures
 * and its contents should not be modified.
 *
 * @param - SIGNALING_CLIENT_HANDLE - IN - Signaling client handle
 * @param - UINT32 - INT - The index of the ICE configuration information objects to retrieve
 * @param - PIceConfigInfo* - OUT - The pointer to the ICE configuration information object
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientGetIceConfigInfo(SIGNALING_CLIENT_HANDLE, UINT32, PIceConfigInfo*);

/*
 * Connects the signaling client to the socket in order to send/receive messages.
 *
 * NOTE: The call will succeed only when the signaling client is in a ready state.
 *
 * @param - SIGNALING_CLIENT_HANDLE - IN - Signaling client handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientConnectSync(SIGNALING_CLIENT_HANDLE);

#pragma pack(pop, include)

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE__ */
