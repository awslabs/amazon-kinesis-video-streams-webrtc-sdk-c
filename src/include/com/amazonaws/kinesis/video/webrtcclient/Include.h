/*
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#include <com/amazonaws/kinesis/video/client/Include.h>
#include <com/amazonaws/kinesis/video/common/Include.h>
#pragma clang diagnostic pop

/*===========================================================================================*/
/*=================SESSION DESCRIPTION INIT RELATED STATUS ERROR CODES=== ===================*/
/*===========================================================================================*/
/*! \addtogroup SessionDescriptionInit
   * WEBRTC Session Description initialization related codes. Values are derived
   * from STATUS_WEBRTC_BASE (0x55000000)
   *  @{
*/
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
/*!@} */

/*===========================================================================================*/
/*==================================SDP RELATED STATUS ERROR CODES===========================*/
/*===========================================================================================*/
/*! \addtogroup SDP
   * WEBRTC SDP packet related error codes. Values are derived from STATUS_SDP_BASE
   * (0x56000000)
   *  @{
*/
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
/*!@} */

/*===========================================================================================*/
/*=================================STUN RELATED STATUS ERROR CODES===========================*/
/*===========================================================================================*/
/*! \addtogroup STUN
   * WEBRTC STUN related error codes. Values are derived from STATUS_STUN_BASE (0x57000000)
   *  @{
*/
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
/*!@} */

/*===========================================================================================*/
/*==============================NETWORKING RELATED STATUS ERROR CODES========================*/
/*===========================================================================================*/
/*! \addtogroup Networking
   * WEBRTC Networking related error codes. Values are derived from STATUS_NETWORKING_BASE (0x58000000)
   *  @{
*/
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
#define STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND                                  STATUS_NETWORKING_BASE + 0x00000021
#define STATUS_SOCKET_CONNECTION_CLOSED_ALREADY                                     STATUS_NETWORKING_BASE + 0x00000022
#define STATUS_SOCKET_SET_SEND_BUFFER_SIZE_FAILED                                   STATUS_NETWORKING_BASE + 0x00000023
#define STATUS_GET_SOCKET_FLAG_FAILED                                               STATUS_NETWORKING_BASE + 0x00000024
#define STATUS_SET_SOCKET_FLAG_FAILED                                               STATUS_NETWORKING_BASE + 0x00000025
/*!@} */

/*===========================================================================================*/
/*=================================DTLS RELATED STATUS ERROR CODES===========================*/
/*===========================================================================================*/
/*! \addtogroup DTLS
   * WEBRTC DTLS related error codes. Values are derived from STATUS_DTLS_BASE (0x59000000)
   *  @{
*/
#define STATUS_DTLS_BASE                                                            STATUS_NETWORKING_BASE + 0x01000000
#define STATUS_CERTIFICATE_GENERATION_FAILED                                        STATUS_DTLS_BASE + 0x00000001
#define STATUS_SSL_CTX_CREATION_FAILED                                              STATUS_DTLS_BASE + 0x00000002
#define STATUS_SSL_REMOTE_CERTIFICATE_VERIFICATION_FAILED                           STATUS_DTLS_BASE + 0x00000003
#define STATUS_SSL_PACKET_BEFORE_DTLS_READY                                         STATUS_DTLS_BASE + 0x00000004
#define STATUS_SSL_UNKNOWN_SRTP_PROFILE                                             STATUS_DTLS_BASE + 0x00000005
#define STATUS_SSL_INVALID_CERTIFICATE_BITS                                         STATUS_DTLS_BASE + 0x00000006

/*!@} */

/*===========================================================================================*/
/*===============================ICE/TURN RELATED STATUS ERROR CODES=========================*/
/*===========================================================================================*/
/*! \addtogroup ICE
   * WEBRTC ICE related error codes. Values are derived from STATUS_ICE_BASE (0x5a000000)
   *  @{
*/
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
#define STATUS_ICE_NO_LOCAL_CANDIDATE_AVAILABLE_AFTER_GATHERING_TIMEOUT             STATUS_ICE_BASE  + 0x0000000b
#define STATUS_ICE_AGENT_TERMINATED_ALREADY                                         STATUS_ICE_BASE  + 0x0000000c
#define STATUS_ICE_NO_CONNECTED_CANDIDATE_PAIR                                      STATUS_ICE_BASE  + 0x0000000d
#define STATUS_ICE_CANDIDATE_PAIR_LIST_EMPTY                                        STATUS_ICE_BASE  + 0x0000000e
#define STATUS_ICE_NOMINATED_CANDIDATE_NOT_CONNECTED                                STATUS_ICE_BASE  + 0x00000010
#define STATUS_ICE_CANDIDATE_INIT_MALFORMED                                         STATUS_ICE_BASE  + 0x00000011
#define STATUS_ICE_CANDIDATE_MISSING_CANDIDATE                                      STATUS_ICE_BASE  + 0x00000012
#define STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR                                STATUS_ICE_BASE  + 0x00000013
#define STATUS_ICE_MAX_REMOTE_CANDIDATE_COUNT_EXCEEDED                              STATUS_ICE_BASE  + 0x00000014
#define STATUS_ICE_INVALID_STATE                                                	STATUS_ICE_BASE  + 0x0000001c
#define STATUS_ICE_NO_LOCAL_HOST_CANDIDATE_AVAILABLE                                STATUS_ICE_BASE  + 0x0000001d
#define STATUS_ICE_NO_NOMINATED_VALID_CANDIDATE_PAIR_AVAILABLE                      STATUS_ICE_BASE  + 0x0000001e
#define STATUS_TURN_CONNECTION_NO_HOST_INTERFACE_FOUND                              STATUS_ICE_BASE  + 0x0000001f
#define STATUS_TURN_CONNECTION_STATE_TRANSITION_TIMEOUT                             STATUS_ICE_BASE  + 0x00000020
#define STATUS_TURN_CONNECTION_FAILED_TO_CREATE_PERMISSION                          STATUS_ICE_BASE  + 0x00000021
#define STATUS_TURN_CONNECTION_FAILED_TO_BIND_CHANNEL                               STATUS_ICE_BASE  + 0x00000022
#define STATUS_TURN_NEW_DATA_CHANNEL_MSG_HEADER_BEFORE_PREVIOUS_MSG_FINISH          STATUS_ICE_BASE  + 0x00000023
#define STATUS_TURN_MISSING_CHANNEL_DATA_HEADER                                     STATUS_ICE_BASE  + 0x00000024
#define STATUS_ICE_FAILED_TO_RECOVER_FROM_DISCONNECTION                             STATUS_ICE_BASE  + 0x00000025
#define STATUS_ICE_NO_AVAILABLE_ICE_CANDIDATE_PAIR                                  STATUS_ICE_BASE  + 0x00000026
#define STATUS_TURN_CONNECTION_PEER_NOT_USABLE                                      STATUS_ICE_BASE  + 0x00000027
/*!@} */

/*===========================================================================================*/
/*===============================SRTP RELATED STATUS ERROR CODES=============================*/
/*===========================================================================================*/
/*! \addtogroup SRTP
   * WEBRTC SRTP related error codes. Values are derived from STATUS_SRTP_BASE (0x5b000000)
   *  @{
*/
#define STATUS_SRTP_BASE                                                            STATUS_ICE_BASE + 0x01000000
#define STATUS_SRTP_DECRYPT_FAILED                                                  STATUS_SRTP_BASE + 0x00000001
#define STATUS_SRTP_ENCRYPT_FAILED                                                  STATUS_SRTP_BASE + 0x00000002
#define STATUS_SRTP_TRANSMIT_SESSION_CREATION_FAILED                                STATUS_SRTP_BASE + 0x00000003
#define STATUS_SRTP_RECEIVE_SESSION_CREATION_FAILED                                 STATUS_SRTP_BASE + 0x00000004
#define STATUS_SRTP_INIT_FAILED                                                     STATUS_SRTP_BASE + 0x00000005
/*!@} */

/*===========================================================================================*/
/*================================RTP RELATED STATUS ERROR CODES=============================*/
/*===========================================================================================*/
/*! \addtogroup RTP
   * WEBRTC RTP related error codes. Values are derived from STATUS_RTP_BASE (0x5c000000)
   *  @{
*/
#define STATUS_RTP_BASE                                                             STATUS_SRTP_BASE + 0x01000000
#define STATUS_RTP_INPUT_PACKET_TOO_SMALL                                           STATUS_RTP_BASE + 0x00000001
#define STATUS_RTP_INPUT_MTU_TOO_SMALL                                              STATUS_RTP_BASE + 0x00000002
#define STATUS_RTP_INVALID_NALU                                                     STATUS_RTP_BASE + 0x00000003
#define STATUS_RTP_INVALID_EXTENSION_LEN                                            STATUS_RTP_BASE + 0x00000004
/*!@} */

/*===========================================================================================*/
/*===============================SIGNALING RELATED STATUS ERROR CODES========================*/
/*===========================================================================================*/
/*! \addtogroup Signaling
   * WEBRTC Signaling related error codes. Values are derived from STATUS_SIGNALING_BASE (0x5d000000)
   *  @{
*/
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
#define STATUS_SIGNALING_INVALID_MESSAGE_TTL_VALUE                                  STATUS_SIGNALING_BASE + 0x0000002E
#define STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED                                  STATUS_SIGNALING_BASE + 0x0000002F
#define STATUS_SIGNALING_RECONNECT_FAILED                                           STATUS_SIGNALING_BASE + 0x00000030

/*!@} */
/*===========================================================================================*/
/*=========================PEER CONENCTION RELATED STATUS ERROR CODES========================*/
/*===========================================================================================*/
/*! \addtogroup PeerConnection
   * WEBRTC STUN related error codes. Values are derived from STATUS_PEERCONNECTION_BASE
   * (0x5e000000)
   *  @{
*/
#define STATUS_PEERCONNECTION_BASE                                                  STATUS_SIGNALING_BASE + 0x01000000
#define STATUS_PEERCONNECTION_CREATE_ANSWER_WITHOUT_REMOTE_DESCRIPTION              STATUS_PEERCONNECTION_BASE + 0x00000001
#define STATUS_PEERCONNECTION_CODEC_INVALID                                         STATUS_PEERCONNECTION_BASE + 0x00000002
#define STATUS_PEERCONNECTION_CODEC_MAX_EXCEEDED                                    STATUS_PEERCONNECTION_BASE + 0x00000003
/*!@} */

/*===========================================================================================*/
/*=============================SCTP RELATED STATUS ERROR CODES===============================*/
/*===========================================================================================*/
/*! \addtogroup SCTP
   * WEBRTC SCTP related error codes. Values are derived from STATUS_STUN_BASE (0x5f000000)
   *  @{
*/
#define STATUS_SCTP_BASE                                                            STATUS_PEERCONNECTION_BASE + 0x01000000
#define STATUS_SCTP_SESSION_SETUP_FAILED                                            STATUS_SCTP_BASE + 0x00000001
#define STATUS_SCTP_INVALID_DCEP_PACKET                                             STATUS_SCTP_BASE + 0x00000002
/*!@} */

/*===========================================================================================*/
/*=================================RTCP RELATED STATUS ERROR CODES===========================*/
/*===========================================================================================*/
/*! \addtogroup RTCP
   * WEBRTC RTCP related error codes. Values are derived from STATUS_RTCP_BASE (0x60000000)
   *  @{
*/
#define STATUS_RTCP_BASE                                                            STATUS_SCTP_BASE + 0x01000000
#define STATUS_RTCP_INPUT_PACKET_TOO_SMALL                                          STATUS_RTCP_BASE + 0x00000001
#define STATUS_RTCP_INPUT_PACKET_INVALID_VERSION                                    STATUS_RTCP_BASE + 0x00000002
#define STATUS_RTCP_INPUT_PACKET_LEN_MISMATCH                                       STATUS_RTCP_BASE + 0x00000003
#define STATUS_RTCP_INPUT_NACK_LIST_INVALID                                         STATUS_RTCP_BASE + 0x00000004
#define STATUS_RTCP_INPUT_SSRC_INVALID                                              STATUS_RTCP_BASE + 0x00000005
#define STATUS_RTCP_INPUT_PARTIAL_PACKET                                            STATUS_RTCP_BASE + 0x00000006
#define STATUS_RTCP_INPUT_REMB_TOO_SMALL                                            STATUS_RTCP_BASE + 0x00000007
#define STATUS_RTCP_INPUT_REMB_INVALID                                              STATUS_RTCP_BASE + 0x00000008
/*!@} */

/*===========================================================================================*/
/*=======================ROLLING BUFFER RELATED STATUS ERROR CODES===========================*/
/*===========================================================================================*/
/*! \addtogroup RollingBuffer
   * WEBRTC Rolling Buffer related error codes. Values are derived from STATUS_ROLLING_BUFFER_BASE
   * (0x61000000)
   *  @{
*/
#define STATUS_ROLLING_BUFFER_BASE                                                  STATUS_RTCP_BASE + 0x01000000
#define STATUS_ROLLING_BUFFER_NOT_IN_RANGE                                          STATUS_ROLLING_BUFFER_BASE + 0x00000001
/*!@} */


/*===========================================================================================*/
/*==========================LENGTHS OF DIFFERENT CHARACTER ARRAYS============================*/
/*===========================================================================================*/
/*! \addtogroup NameLengths
   * Lengths of some string members of different structures
   *  @{
*/

/**
 * Maximum allowed channel name length
 */
#define MAX_CHANNEL_NAME_LEN                                                        256

/**
 * Maximum allowed signaling client ID length
 */
#define MAX_SIGNALING_CLIENT_ID_LEN                                                 256

/**
 * Maximum allowed ICE configuration user name length
 */
#define MAX_ICE_CONFIG_USER_NAME_LEN                                                512

/**
 * Maximum allowed ICE configuration password length
 */
#define MAX_ICE_CONFIG_CREDENTIAL_LEN                                               512

/**
 * Maximum allowed signaling URI length
 */
#define MAX_SIGNALING_ENDPOINT_URI_LEN                                              512

/**
 * Maximum allowed ICE URI length
 */
#define MAX_ICE_CONFIG_URI_LEN                                                      256

/**
 * Maximum allowed correlation ID length
 */
#define MAX_CORRELATION_ID_LEN                                                      256

/**
 * Maximum allowed error string length
 */
#define MAX_ERROR_TYPE_STRING_LEN                                                   256

/**
 * Maximum allowed code string length
 */
#define MAX_STATUS_CODE_STRING_LEN                                                  256

/**
 * Maximum allowed message description length
 */
#define MAX_MESSAGE_DESCRIPTION_LEN                                                 1024

/**
 * Maximum length of SDP member in RtcSessionDescriptionInit
 */
#define MAX_SESSION_DESCRIPTION_INIT_SDP_LEN                                        25000

/**
 * Maximum length of a MediaStream's ID
 */
#define MAX_MEDIA_STREAM_ID_LEN                                                     255


/**
 * Max certificates an RtcConfiguration can accept
 */
#define MAX_RTCCONFIGURATION_CERTIFICATES                                           3

/**
 * Maximum length of a MediaStream's Track ID
 */
#define MAX_MEDIA_STREAM_TRACK_ID_LEN                                               255

/**
 * Maximum length of candidate member of ICECandidateInit
 */
#define MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN                                        255

/**
 * Maximum length of DataChannel name
 */
#define MAX_DATA_CHANNEL_NAME_LEN                                                    255

/**
 * Maximum length of DataChannel Protocol
 */
#define MAX_DATA_CHANNEL_PROTOCOL_LEN                                               255

/**
 * Maximum length of signaling message
 */
#define MAX_SIGNALING_MESSAGE_LEN                                                   (10 * 1024)
/*!@} */

/*===========================================================================================*/
/*=================================STRUCTURE VERSIONS MACROS=================================*/
/*===========================================================================================*/
/*! \addtogroup StructureVersions
   * Lengths of some string members of different structures
   *  @{
*/
/**
 * Version of RtcPeerConnection structure
 */
#define PEER_CONNECTION_CURRENT_VERSION                                             0

/**
 * Version of ChannelInfo structure
 */
#define CHANNEL_INFO_CURRENT_VERSION                                                0

/**
 * Version of SignalingClientInfo structure
 */
#define SIGNALING_CLIENT_INFO_CURRENT_VERSION                                       0

/**
 * Version of SignalingClientCallbacks structure
 */
#define SIGNALING_CLIENT_CALLBACKS_CURRENT_VERSION                                  0

/**
 * Signaling states default retry count. This will evaluate to the last call being made 20 seconds in which will hit a timeout first.
 */
#define SIGNALING_STATES_DEFAULT_RETRY_COUNT                                        10

/**
 * Version of signaling client
 */
#define SIGNALING_CLIENT_CURRENT_VERSION                                            0

/**
 * Version of SignalingChannelDescription structure
 */
#define SIGNALING_CHANNEL_DESCRIPTION_CURRENT_VERSION                               0

/**
 * Version of IceConfigInfo structure
 */
#define SIGNALING_ICE_CONFIG_INFO_CURRENT_VERSION                                   0

/**
 * Version of SignalingMessage structure
 */
#define SIGNALING_MESSAGE_CURRENT_VERSION                                           0
/*!@} */

/*===========================================================================================*/
/*=======================================COUNT RELATED MACROS================================*/
/*===========================================================================================*/
/*! \addtogroup Counts
   * Counts of different structure members
   *  @{
*/
/**
 * Maximum number of ICE config URI allowed
 */
#define MAX_ICE_CONFIG_URI_COUNT                                                    4

/**
 * Maximum number of ICE configs allowed
 */
#define MAX_ICE_CONFIG_COUNT                                                        5

/**
 * Max ice servers for a RtcPeerConnection.
 * It is calculated as product of maximum number of ICE configurations and
 * maximum number of server URIs plus single STUN (1)
 */
#define MAX_ICE_SERVERS_COUNT                                                       (MAX_ICE_CONFIG_COUNT * MAX_ICE_CONFIG_URI_COUNT + 1)
/*!@} */

/*===========================================================================================*/
/*=====================================TIMOUTS RELATED MACROS================================*/
/*===========================================================================================*/
/*! \addtogroup Timeouts
   * Timeouts for different activities in the stack
   *  @{
*/
/**
 * Default signaling creation timeout
 */
#define SIGNALING_CREATE_TIMEOUT                                                    (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Default connect sync API timeout
 */
#define SIGNALING_CONNECT_STATE_TIMEOUT                                             (15 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Default signaling connection establishment timeout
 */
#define SIGNALING_CONNECT_TIMEOUT                                                   (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Default timeout for sending data
 */
#define SIGNALING_SEND_TIMEOUT                                                      (5 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Default signaling message alive time
 */
#define SIGNALING_DEFAULT_MESSAGE_TTL_VALUE                                         (60 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Default jitter buffer tolerated latency, frame will be dropped if it is out of window
 */
#define DEFAULT_JITTER_BUFFER_MAX_LATENCY                                           (2000L * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)
/*!@} */

/**
 * Valid ASCII characters for signaling channel name
 */
#define SIGNALING_VALID_NAME_CHARS                                                  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.-"


/**
 * Maximum sequence number in rtp packet/jitter buffer
 */
#define MAX_SEQUENCE_NUM                                                            ((UINT32) MAX_UINT16)

/**
 * Parameterized string for KVS STUN Server
 */
#define KINESIS_VIDEO_STUN_URL                                                     "stun:stun.kinesisvideo.%s.amazonaws.com:443"

/**
 * Default signaling SSL port
 */
#define SIGNALING_DEFAULT_SSL_PORT                                                  DEFAULT_SSL_PORT_NUMBER

/**
 * Default signaling non SSL port
 */
#define SIGNALING_DEFAULT_NON_SSL_PORT                                              DEFAULT_NON_SSL_PORT_NUMBER

/* CHK_LOG_ERR_NV has been replaced with CHK_LOG_ERR. */
#define CHK_LOG_ERR_NV(condition) \
    DLOGE("CHK_LOG_ERR_NV has been replaced with CHK_LOG_ERR");

/**
 * @brief Definition of the signaling client handle
 */
typedef UINT64 SIGNALING_CLIENT_HANDLE;
typedef SIGNALING_CLIENT_HANDLE* PSIGNALING_CLIENT_HANDLE;

/**
 * @brief This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_SIGNALING_CLIENT_HANDLE_VALUE
#define INVALID_SIGNALING_CLIENT_HANDLE_VALUE ((SIGNALING_CLIENT_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * @brief Checks for the handle validity
 */
#ifndef IS_VALID_SIGNALING_CLIENT_HANDLE
#define IS_VALID_SIGNALING_CLIENT_HANDLE(h) ((h) != INVALID_SIGNALING_CLIENT_HANDLE_VALUE)
#endif

////////////////////////////////////////////////////
/// Extra callbacks definitions
////////////////////////////////////////////////////
/**
 * @brief RtcOnFrame is fired everytime a frame is received from
 * the remote peer. It is available via the RtpRec
 *
 * NOTE: RtcOnFrame is a KVS specific method
 *
 */
typedef VOID (*RtcOnFrame)(UINT64, PFrame);

/**
 * @brief RtcOnBandwidthEstimation is fired everytime a bandwidth estimation value
 * is computed. This will be fired for sender or receiver side estimation
 *
 * NOTE: RtcOnBandwidthEstimation is a KVS specific method
 *
 */
typedef VOID (*RtcOnBandwidthEstimation)(UINT64, DOUBLE);

/**
 * @brief RtcOnPictureLoss is fired everytime a Picture Loss Indication (PLI)
 * feedback message is received. Receiving such message normally indicates that
 * you sent a video frame which receiver could not decode.
 * It may happen either because of packet loss or for any other reason.
 * Generating an intra frame (aka keyframe, aka I-frame) in response to such message is considered a good strategy.
 *
 * See https://tools.ietf.org/html/rfc4585#section-6.3 for more details
 */
typedef VOID (*RtcOnPictureLoss)(UINT64);


/**
 * @brief RtcOnMessage is fired when a message is received for the DataChannel
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcdatachannel-onmessage
 */
typedef VOID (*RtcOnMessage)(UINT64, BOOL, PBYTE, UINT32);

/**
 * RtcOnOpen is fired when the DataChannel has opened
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcdatachannel-onopen
 */
typedef VOID (*RtcOnOpen)(UINT64);

/**
 * @brief RtcOnDataChannel is fired when the remote PeerConnection
 * creates a new DataChannel
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-ondatachannel
 */
struct __RtcDataChannel;
typedef VOID (*RtcOnDataChannel)(UINT64, struct __RtcDataChannel*);

/**
 * @brief RtcOnIceCandidate is fired when new iceCandidate is found. if PCHAR is NULL then candidate gathering is done.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-ondatachannel
 */
typedef VOID (*RtcOnIceCandidate)(UINT64, PCHAR);

/**
 * @brief IceSetInterfaceFilterFunc is fired when a callback function to filter network interfaces is assigned.
 * The callback function is expected to check for specific interface names to be whitelisted/blacklisted
 */
typedef BOOL (*IceSetInterfaceFilterFunc) (UINT64, PCHAR);

////////////////////////////////////////////////
/// ENUMS
////////////////////////////////////////////////

/**
 * @brief
 * Reference: https://www.w3.org/TR/webrtc/#rtcpeerconnectionstate-enum
 */
typedef enum {
    RTC_PEER_CONNECTION_STATE_NONE          = 0, //!< Starting state of peer connection
    RTC_PEER_CONNECTION_STATE_NEW           = 1, //!< This state is set when ICE Agent is waiting for remote credential
    RTC_PEER_CONNECTION_STATE_CONNECTING    = 2, //!< This state is set when ICE agent checks connection
    RTC_PEER_CONNECTION_STATE_CONNECTED     = 3, //!< This state is set when CIE Agent is ready
    RTC_PEER_CONNECTION_STATE_DISCONNECTED  = 4, //!< This state is set when ICE Agent is disconnected
    RTC_PEER_CONNECTION_STATE_FAILED        = 5, //!< This state is set when ICE Agent transitions to fail state
    RTC_PEER_CONNECTION_STATE_CLOSED        = 6, //!< This state leads to termination of streaming session
    RTC_PEER_CONNECTION_TOTAL_STATE_COUNT   = 7, //!< This state indicates maximum number of Peer connection states
} RTC_PEER_CONNECTION_STATE;

/**
 * @brief RtcOnConnectionStateChange is fired to report a change in peer connection state.
 *
 * Reference: https://www.w3.org/TR/webrtc/#event-iceconnectionstatechange
 */
typedef VOID (*RtcOnConnectionStateChange)(UINT64, RTC_PEER_CONNECTION_STATE);


/**
 * The enum specifies the type of SDP being exchanged
 */
typedef enum {
    SDP_TYPE_OFFER = 1,  //!< SessionDescription is type offer
    SDP_TYPE_ANSWER = 2, //!< SessionDescription is type answer
} SDP_TYPE;

/**
 * @brief The enum specifies the type of track in the stream
 */
typedef enum {
    MEDIA_STREAM_TRACK_KIND_AUDIO = 1, //!< Audio track. Track information is set before add transceiver
    MEDIA_STREAM_TRACK_KIND_VIDEO = 2, //!< Video track. Track information is set before add transceiver
} MEDIA_STREAM_TRACK_KIND;

/**
 * @brief The enum specifies the codec types for audio and video tracks
 */
typedef enum {
    RTC_CODEC_H264_PROFILE_42E01F_LEVEL_ASYMMETRY_ALLOWED_PACKETIZATION_MODE = 1, //!< H264 video codec
    RTC_CODEC_OPUS = 2,                //!< OPUS audio codec
    RTC_CODEC_VP8 = 3,                 //!< VP8 video codec.
    RTC_CODEC_MULAW = 4,               //!< MULAW audio codec
    RTC_CODEC_ALAW = 5,                //!< ALAW audio codec
} RTC_CODEC;

/**
 * @brief ICE_TRANSPORT_POLICY restrict which ICE candidates are used in a session.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcicetransportpolicy
 */
typedef enum {
    ICE_TRANSPORT_POLICY_RELAY = 1, //!< The ICE Agent uses only media relay candidates such as candidates
                                    //!< passing through a TURN server

    ICE_TRANSPORT_POLICY_ALL = 2,   //!< The ICE Agent can use any type of candidate when this value is specified.
} ICE_TRANSPORT_POLICY;

/**
 * @brief RTC_RTP_TRANSCEIVER_DIRECTION indicates direction of a transceiver
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverdirection
 */
typedef enum {
    RTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV = 1, //!< This indicates that peer can send and receive data
    RTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY = 2, //!< This indicates that the peer can only send information
    RTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY = 3, //!< This indicates that the peer can only receive information
} RTC_RTP_TRANSCEIVER_DIRECTION;

/**
 * @brief Defines channel status as reported by the service
 */
typedef enum {
    SIGNALING_CHANNEL_STATUS_CREATING, //!< Signaling channel is being created
    SIGNALING_CHANNEL_STATUS_ACTIVE,   //!< Signaling channel is active
    SIGNALING_CHANNEL_STATUS_UPDATING, //!< Signaling channel is being updated
    SIGNALING_CHANNEL_STATUS_DELETING, //!< Signaling channel is being deleted
} SIGNALING_CHANNEL_STATUS;

/**
 * @brief Defines different signaling messages
 */
typedef enum {
    SIGNALING_MESSAGE_TYPE_OFFER, //!< This message type leads to checks in existence of peer id and payload in the message
    SIGNALING_MESSAGE_TYPE_ANSWER, //!< This message type leads to checks in length/existence of payload in the message
    SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE, //!< This message type leads to checks in length/existence of payload in the message
    SIGNALING_MESSAGE_TYPE_GO_AWAY, //!< This message moves signaling back to describe state
    SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER, //!< This message moves signaling state back to get ICE config
    SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE, //!< This message notifies the awaiting send after checking for failure in message delivery
    SIGNALING_MESSAGE_TYPE_UNKNOWN, //!< This message type is set when the type of message received is unknown
} SIGNALING_MESSAGE_TYPE;

/**
 * @brief Defines different states a signaling client traverses
 */
typedef enum {
    SIGNALING_CLIENT_STATE_UNKNOWN,          //!< Starting state of signaling client
    SIGNALING_CLIENT_STATE_NEW,              //!< This state indicates a new client state
    SIGNALING_CLIENT_STATE_GET_CREDENTIALS,  //!< This state involves getting a token using AWS credentials
    SIGNALING_CLIENT_STATE_DESCRIBE,         //!< This state is set to get the most current information about the channel
                                             //!< Channel name or ARN needs to be provided t get the information
    SIGNALING_CLIENT_STATE_CREATE,           //!< This state is set to create the channel with the information supplied
                                             //!< in the describe state
    SIGNALING_CLIENT_STATE_GET_ENDPOINT,     //!< This state is set to provide an endpoint for sending/receiving messages
    SIGNALING_CLIENT_STATE_GET_ICE_CONFIG,   //!< This state gets ICE related details such as server list, username,
                                             //!< and passwords
    SIGNALING_CLIENT_STATE_READY,            //!< On setting this state, if continueOnReady flag is set, a transition is
                                             //!< to the next state is made
    SIGNALING_CLIENT_STATE_CONNECTING,       //!< In this state, if already connected, nothing needs to be done. This can happen when
                                             //!< we get to this state after ICE refresh
    SIGNALING_CLIENT_STATE_CONNECTED,        //!< On transitioning to this state, the timeout on the state machine is reset
    SIGNALING_CLIENT_STATE_DISCONNECTED,     //!< This state transition happens either from connect or connected state
    SIGNALING_CLIENT_STATE_MAX_VALUE,        //!< This state indicates maximum number of signaling client states
} SIGNALING_CLIENT_STATE, *PSIGNALING_CLIENT_STATE;

/**
 * @brief Channel type as reported by the service
 */
typedef enum {
    SIGNALING_CHANNEL_TYPE_UNKNOWN,       //!< Channel type is unknown
    SIGNALING_CHANNEL_TYPE_SINGLE_MASTER, //!< Channel type is master
} SIGNALING_CHANNEL_TYPE;

/**
 * @brief Channel role type
 */
typedef enum {
    SIGNALING_CHANNEL_ROLE_TYPE_UNKNOWN, //!< Channel role is unknown
    SIGNALING_CHANNEL_ROLE_TYPE_MASTER,  //!< Channel role is master
    SIGNALING_CHANNEL_ROLE_TYPE_VIEWER,  //!< Channel role is viewer
} SIGNALING_CHANNEL_ROLE_TYPE;



/**
 * @brief An RtcPeerConnection instance allows an application to establish peer-to-peer
 * communications with another RtcPeerConnection, or to another endpoint implementing
 * the required protocols
 *
 * Reference: https://www.w3.org/TR/webrtc/#introduction
 */
typedef struct {
    UINT32 version; //!< Version of peer connection structure
} RtcPeerConnection, *PRtcPeerConnection;

/**
 * @brief Represents a single track in a MediaStream
 *
 * Reference: https://www.w3.org/TR/mediacapture-streams/#mediastreamtrack
 */
typedef struct {
    RTC_CODEC codec; //!< non-standard, codec that the track is using
    CHAR trackId[MAX_MEDIA_STREAM_ID_LEN + 1]; //!< non-standard, id of this individual track
    CHAR streamId[MAX_MEDIA_STREAM_ID_LEN + 1]; //!< non-standard, id of the MediaStream this track belongs too
    MEDIA_STREAM_TRACK_KIND kind; //!< Kind of track - audio or video
} RtcMediaStreamTrack, *PRtcMediaStreamTrack;

/**
 * @brief RTCRtpReceiver allows an application to inspect the
 * receipt of a MediaStreamTrack.
 *
 * NOTE: KVS extends this interface allowing users to receive
 * complete frames from the remote connection.
 *
 * Reference: https://www.w3.org/TR/webrtc/#rtcrtpreceiver-interface
 */
typedef struct {
    RtcMediaStreamTrack track; //!< Track with details of codec, trackId, streamId and track kind
} RtcRtpReceiver, *PRtcRtpReceiver;

/**
 * @brief The RTCRtpTransceiver represents a combination of an RTCRtpSender
 * and an RTCRtpReceiver that share a common mid.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiver
 */
typedef struct {
    RTC_RTP_TRANSCEIVER_DIRECTION direction; //!< Transceiver direction
    RtcRtpReceiver receiver; //!< RtcRtpReceiver that has track specific information
} RtcRtpTransceiver, *PRtcRtpTransceiver;

/**
 * @brief RtcIceServer is used to describe the STUN and TURN servers that
 * can be used by the ICE Agent to establish a connection with a peer.
 *
 * https://www.w3.org/TR/webrtc/#rtciceserver-dictionary
 */
typedef struct {
    CHAR urls[MAX_ICE_CONFIG_URI_LEN + 1]; //!< URL of STUN/TURN Server
    CHAR username[MAX_ICE_CONFIG_USER_NAME_LEN + 1]; //!< Username to be used with TURN server
    CHAR credential[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1]; //!< Password to be used with TURN server
} RtcIceServer, *PRtcIceServer;

/**
 * @brief Specifies the certificate and the private key used by the certificate.
 * The Certificates are in the form of x509 certs
 */
typedef struct {
    // The certificate bits and the size
    PBYTE pCertificate; //!< Certificate bits
    UINT32 certificateSize; //!< Size of certificate

    // The private key bits and the size in bytes
    PBYTE pPrivateKey; //!< Private key bit
    UINT32 privateKeySize; //!< Size of private key in bytes
} RtcCertificate, *PRtcCertificate;

/**
 *  KvsRtcConfiguration is a collection of non-standard extensions to RTCConfiguration
 *  these exist to serve use cases that currently aren't being served by the W3C standard
 *
 *  NOTE: These options will be removed/modified as the WebRTC standard changes, and exist to unblock
 *  issues that we have today.
 */
typedef struct {
    //!< Controls the size of the largest packet the WebRTC SDK will send
    //!< Some networks may drop packets if they exceed a certain size, and is useful in those conditions.
    //!< A smaller MTU will incur higher bandwidth usage however since more packets will be generated with
    //!< smaller payloads. If unset DEFAULT_MTU_SIZE will be used
    UINT16 maximumTransmissionUnit;

    //!< Maximum time ice will wait for gathering STUN and RELAY candidates. Once
    //!< it's reached, ice will proceed with whatever candidate it current has. Use default value if 0.
    UINT32 iceLocalCandidateGatheringTimeout;

    //!< Maximum time allowed waiting for at least one ice candidate pair to receive
    //!< binding response from the peer. Use default value if 0.
    UINT32 iceConnectionCheckTimeout;

    //!< If client is ice controlling, this is the timeout for receiving bind response of requests that has USE_CANDIDATE
    //!< attribute. If client is ice controlled, this is the timeout for receiving binding request that has USE_CANDIDATE
    //!< attribute after connection check is done. Use default value if 0.
    UINT32 iceCandidateNominationTimeout;

    //!< Ta in https://tools.ietf.org/html/rfc8445
    //!< rate at which binding request packets are sent during connection check. Use default interval if 0.
    UINT32 iceConnectionCheckPollingInterval;

    //!< GeneratedCertificateBits controls the amount of bits the locally generated self-signed certificate uses
    //!< A smaller amount of bits may result in less CPU usage on startup, but will cause a weaker certificate to be generated
    //!< If unset GENERATED_CERTIFICATE_BITS will be used
    INT32 generatedCertificateBits;

    //!< GenerateRSACertificate controls if an ECDSA or RSA certificate is generated.
    //!< By default we generate an ECDSA certificate but some platforms may not support them.
    BOOL generateRSACertificate;

    UINT32 sendBufSize; //!< Socket send buffer length. Item larger then this size will get dropped. Use system default if 0.

    UINT64 filterCustomData; //!< Custom Data that can be populated by the developer while developing filter function

    IceSetInterfaceFilterFunc iceSetInterfaceFilterFunc; //!< Filter function callback to be set when the developer
                                                         //!< would like to whitelist/blacklist specific network interfaces
} KvsRtcConfiguration, *PKvsRtcConfiguration;

/**
 *  @brief The Configuration defines a set of parameters to configure how the peer-to-peer
 *  communication established via RtcPeerConnection is established or re-established.
 *
 *  Reference: https://www.w3.org/TR/webrtc/#rtcconfiguration-dictionary
 */
typedef struct {
    ICE_TRANSPORT_POLICY iceTransportPolicy; //!< Indicates which candidates the ICE Agent is allowed to use.
    RtcIceServer iceServers[MAX_ICE_SERVERS_COUNT]; //!< Servers available to be used by ICE, such as STUN and TURN servers.
    KvsRtcConfiguration kvsRtcConfiguration; //!< Non-standard configuration options

    //!< Set of certificates that the RtcPeerConnection uses to authenticate.
    //!< Although any given DTLS connection will use only one certificate, this
    //!< attribute allows the caller to provide multiple certificates that support
    //!< different algorithms.
    //!<
    //!< If this value is absent, then a default set of certificates is generated
    //!< for each RtcPeerConnection.
    //!<
    //!< An absent value is determined by the certificate pointing to NULL
    //!<
    //!< Doc: https://www.w3.org/TR/webrtc/#dom-rtcconfiguration-certificates
    //!<
    //!< !!!!!!!!!! IMPORTANT !!!!!!!!!!
    //!< It is recommended to rotate the certificates often - preferably for every peer connection
    //!< to avoid a compromised client weakening the security of the new connections.
    //!<
    //!< NOTE: The certificates, if specified, can be freed after the peer connection create call
    //!<
    RtcCertificate certificates[MAX_RTCCONFIGURATION_CERTIFICATES];
} RtcConfiguration, *PRtcConfiguration;

/**
 * @brief SessionDescription is used by RtcPeerConnection to expose local
 * and remote session descriptions.
 *
 * Reference: https://www.w3.org/TR/webrtc/#rtcsessiondescription-class
 */
typedef struct {
    SDP_TYPE type; //!< Indicates an offer/anser SDP type
    CHAR sdp[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1]; //!< SDP Data containing media capabilities, transport addresses
                                                        //!< and related metadata in a transport agnostic manner
} RtcSessionDescriptionInit, *PRtcSessionDescriptionInit;

/**
 * @brief Rtc ICE candidate interface.
 *
 * Reference: https://www.w3.org/TR/webrtc/#rtcicecandidate-interface
 */
typedef struct {
    CHAR candidate[MAX_ICE_CANDIDATE_INIT_CANDIDATE_LEN + 1]; //!< Candidate information contaiing details such as protocol
                                                              //!< (udp/tcp), IP Address, priority and port
} RtcIceCandidateInit, *PRtcIceCandidateInit;

/**
 * @brief Structure defining the basic signaling message
 */
typedef struct {
    UINT32 version; //!< Current version of the structure

    SIGNALING_MESSAGE_TYPE messageType; //!< Type of signaling message.

    CHAR correlationId[MAX_CORRELATION_ID_LEN + 1]; //!< Correlation Id string

    CHAR peerClientId[MAX_SIGNALING_CLIENT_ID_LEN + 1]; //!< Sender client id

    UINT32 payloadLen; //!< Optional payload length. If 0, the length will be calculated

    CHAR payload[MAX_SIGNALING_MESSAGE_LEN + 1]; //!< Actual signaling message payload
} SignalingMessage, *PSignalingMessage;

/**
 * @brief Structure defining the signaling message to be received
 */
typedef struct {
    SignalingMessage signalingMessage; //!< The signaling message with details such as message type, correlation ID,
                                       //!< peer client ID and payload

    SERVICE_CALL_RESULT statusCode; //!< Response status code

    CHAR errorType[MAX_ERROR_TYPE_STRING_LEN + 1]; //!< Error type of the signaling message

    CHAR description[MAX_MESSAGE_DESCRIPTION_LEN + 1]; //!< Optional description of the message
} ReceivedSignalingMessage, *PReceivedSignalingMessage;

/**
 * @brief Populate Signaling client with client ID and application log level
 */
typedef struct {
    UINT32 version; //!< Version of the structure
    CHAR clientId[MAX_SIGNALING_CLIENT_ID_LEN + 1]; //!< Client id to use. Defines if the client is a producer/consumer
    UINT32 loggingLevel; //!< Verbosity level for the logging. One of LOG_LEVEL_XXX
                         //!< values or the default verbosity will be assumed. Currently,
                         //!< default value is LOG_LEVEL_WARNING
} SignalingClientInfo, *PSignalingClientInfo;

/**
 * @brief Contains all signaling channel related information
 */
typedef struct {
    UINT32 version; //!< Version of the structure
    PCHAR pChannelName; //!< Name of the signaling channel name. Maximum length is defined by MAX_CHANNEL_NAME_LEN + 1

    PCHAR pChannelArn; //!< Channel Amazon Resource Name (ARN). This is an optional parameter
                       //!< Maximum length is defined by MAX_ARN_LEN+1

    PCHAR pRegion; //!< AWS Region in which the channel is to be opened. Can be empty for default
                   //!< Maximum length is defined by MAX_REGION_NAME_LEN+1

    PCHAR pControlPlaneUrl; //!< Optional fully qualified control plane URL
                            //!< Maximum length is defined by MAX_ARN_LEN+1

    PCHAR pCertPath; //!< Optional certificate path. Maximum length is defined by MAX_PATH_LEN+1

    PCHAR pUserAgentPostfix; //!<Optional user agent post-fix. Maximum length is defined by
                             //!< MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN+1

    PCHAR pCustomUserAgent; //!< Optional custom user agent name. Maximum length is defined by MAX_USER_AGENT_LEN+1

    PCHAR pUserAgent; //!< Combined user agent.  Maximum length is defined by MAX_USER_AGENT_LEN+1

    PCHAR pKmsKeyId; //!< Optional KMS key id ARN. Maximum length is defined by MAX_ARN_LEN+1

    SIGNALING_CHANNEL_TYPE channelType; //!< Channel type when creating.

    SIGNALING_CHANNEL_ROLE_TYPE channelRoleType; //!< Channel role type for the endpoint - master/viewer

    BOOL cachingEndpoint; //!< Flag determines if the endpoint is to be cached

    UINT64 endpointCachingPeriod; //!< Endpoint caching TTL

    BOOL retry; //!< Flag determines if a retry of the network calls is to be done on errors up to max retry times

    BOOL reconnect; //!< Flag determines if reconnection should be attempted on connection drop

    UINT64 messageTtl; //!< The message TTL. Must be in the range of 5ns and 120ns.
                       //!< Specifying zero will default to 60ns

    UINT32 tagCount; //!< Number of tags associated with the stream

    PTag pTags; //!< Stream tags array
} ChannelInfo, *PChannelInfo;

/**
 * @brief ICE configuration information struct
 *
 * NOTE: Each ICE configuration has an array of ICE URIs.
 * The actual URI count is specified in uriCount member.
 *
 * NOTE:TTL is given in default which is 100ns duration
 */
typedef struct {
    UINT32 version; //!< Version of the struct
    UINT64 ttl; //!< TTL of the configuration is 100ns
    UINT32 uriCount; //!<  Number of Ice URI objects
    CHAR uris[MAX_ICE_CONFIG_URI_COUNT][MAX_ICE_CONFIG_URI_LEN + 1]; //!< List of Ice server URIs
    CHAR userName[MAX_ICE_CONFIG_USER_NAME_LEN + 1]; //!< Username for the server
    CHAR password[MAX_ICE_CONFIG_CREDENTIAL_LEN + 1]; //!< Password for the server
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
 * @brief Register Signaling client callbacks
 */
typedef struct {
    UINT32 version; //!< Current version of the structure
    UINT64 customData; //!< Custom data passed by the caller
    SignalingClientMessageReceivedFunc messageReceivedFn; //!< Callback registeration for received SDP
    SignalingClientErrorReportFunc errorReportFn; //!<  Error reporting function. This is an optional member
    SignalingClientStateChangedFunc stateChangeFn; //!< Signaling client state change callback
} SignalingClientCallbacks, *PSignalingClientCallbacks;

/**
 * @brief Signaling channel description returned from the service
 */
typedef struct {
    UINT32 version; //!< Version of the SignalingChannelDescription struct
    CHAR channelArn[MAX_ARN_LEN + 1]; //!< Channel Amazon Resource Name (ARN)
    CHAR channelName[MAX_CHANNEL_NAME_LEN + 1]; //!< Signaling channel name. Should be unique per AWS account
    SIGNALING_CHANNEL_STATUS channelStatus; //!< Current channel status as reported by the service
    SIGNALING_CHANNEL_TYPE channelType; //!< Channel type as reported by the service
    CHAR updateVersion[MAX_UPDATE_VERSION_LEN + 1]; //!< A random number generated on every update while describing
                                                    //!< signaling channel
    UINT64 messageTtl; //!< The period of time a signaling channel retains underlived messages before they are discarded
                       //!< The values are in the range of 5 and 120 seconds
    UINT64 creationTime; //!< Timestamp of when the channel gets created
} SignalingChannelDescription, *PSignalingChannelDescription;

/**
 * @brief RtcRtpTransceiverInit is used to configure a transceiver when creating it
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcrtptransceiverinit
 */
typedef struct {
    RTC_RTP_TRANSCEIVER_DIRECTION direction; //!< Transceiver direction - SENDONLY, RECVONLY, SENDRECV
} RtcRtpTransceiverInit, *PRtcRtpTransceiverInit;

/**
 * @brief RtcDataChannel represents a bi-directional data channel between two peers.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcdatachannel
 */
typedef struct __RtcDataChannel {
    CHAR name[MAX_DATA_CHANNEL_NAME_LEN + 1]; //!< Define name of data channel. Max length is 256 characters
} RtcDataChannel, *PRtcDataChannel;

/**
 * @brief RtcDataChannelInit dictionary used to configure properties of the
 * underlying channel such as data reliability
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcdatachannelinit
 */
typedef struct {
    BOOL ordered; //!< Decides the order in which data is sent. If true, data is sent in order
    UINT16 maxPacketLifeTime; //!< Limits the time (in milliseconds) during which the channel will (re)transmit
                              //!< data if not acknowledged. This value may be clamped if it exceeds the maximum
                              //!< value supported by the user agent.
    UINT16 maxRetransmits;    //!< Control number of times a channel retransmits data if not delivered successfully
    CHAR protocol[MAX_DATA_CHANNEL_PROTOCOL_LEN + 1]; //!< Sub protocol name for the channel
    BOOL negotiated; //!< If set to true, it is up to the application to negotiate the channel and create an
                     //!< RTCDataChannel object with the same id as the other peer.
} RtcDataChannelInit, *PRtcDataChannelInit;

////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////

/**
 * @brief Initialize a RtcPeerConnection with the provided Configuration
 *
 * Reference: https://www.w3.org/TR/webrtc/#constructor
 *
 * @param[in] PConfiguration Configuration to initialize provided RtcPeerConnection
 * @param[in,out] PRtcPeerConnection Uninitialized RtcPeerConnection
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createPeerConnection(PRtcConfiguration, PRtcPeerConnection*);

/**
 * @brief Free a RtcPeerConnection
 *
 * @param[in] PRtcPeerConnection* RtcPeerConnection that is to be freed
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freePeerConnection(PRtcPeerConnection*);

/**
 * @brief Set a callback when new Ice collects new local candidate.
 *
 * NOTE: When IceAgent is done with collecting candidates,
 * RtcOnIceCandidate will be called with NULL.
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] UINT64 User customData that will be passed along when RtcOnIceCandidate is called
 * @param[in] RtcOnIceCandidate User callback when new local candidate is found
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS peerConnectionOnIceCandidate(PRtcPeerConnection, UINT64, RtcOnIceCandidate);

/**
 * Set a callback for data channel
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] UINT64 User customData that will be passed along when RtcOnDataChannel is called
 * @param[in] RtcOnDataChannel User RtcOnDataChannel callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS peerConnectionOnDataChannel(PRtcPeerConnection, UINT64, RtcOnDataChannel);

/**
 * Set a callback for connection state change
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] UINT64 User customData that will be passed along when RtcOnDataChannel is called
 * @param[in] RtcOnIceCandidate User RtcOnConnectionStateChange callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS peerConnectionOnConnectionStateChange(PRtcPeerConnection, UINT64, RtcOnConnectionStateChange);

/**
 * Load the sdp field of PRtcSessionDescriptionInit with latest local session description
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in,out] PRtcSessionDescriptionInit IN/PRtcSessionDescriptionInit whose sdp field will be modified.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS peerConnectionGetCurrentLocalDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * @brief Populate the provided answer that contains an RFC 3264 offer
 * with the supported configurations for the session.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-createoffer
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in,out] PRtcSessionDescriptionInit IN/answer that describes the supported configurations of the RtcPeerConnection
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createOffer(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * @brief Populate the provided answer that contains an RFC 3264 answer
 * with the supported configurations for the session.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-createanswer
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in,out] PRtcSessionDescriptionInit IN/answer that describes the supported configurations of the RtcPeerConnection
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createAnswer(PRtcPeerConnection, PRtcSessionDescriptionInit);


/**
 * @brief Create a JSON string from RtcSessionDescriptionInit

 * @param[in] PRtcSessionDescriptionInit Source RtcSessionDescriptionInit that will become JSON string
 * @param[out] PCHAR JSON string generated from PRtcSessionDescriptionInit
 * @param[out] PUINT32 If PCHAR is null this is the required buffer size. If PCHAR is non-NULL this is the length of the output
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS serializeSessionDescriptionInit(PRtcSessionDescriptionInit, PCHAR, PUINT32);


/**
 * @brief Parses a JSON string and returns an allocated PSessionDescriptionInit
 *
 * @param[in] PCHAR JSON String of a RtcSessionDescriptionInit
 * @param[in] UINT32 Length of JSON String
 * @param[out] PRtcSessionDescriptionInit RtcSessionDescriptionInit populated from JSON String
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS deserializeSessionDescriptionInit(PCHAR, UINT32, PRtcSessionDescriptionInit);

/**
 * @brief Parses a JSON string and populates a PRtcIceCandidateInit

 * @param[in] PCHAR JSON String of a PRtcIceCandidateInit
 * @param[in] UINT32 Length of JSON String
 * @param[out] PRtcIceCandidateInit PRtcIceCandidateInit populated from JSON String
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS deserializeRtcIceCandidateInit(PCHAR, UINT32, PRtcIceCandidateInit);

/**
 * @brief Instructs the RtcPeerConnection to apply the supplied RtcSessionDescriptionInit
 * as the local description.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-setlocaldescription
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param [in,out]PRtcSessionDescriptionInit IN/SessionDescriptionInit that becomes our new local description
 *
 * @return - STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS setLocalDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * @brief Instructs the RtcPeerConnection to apply
 * the supplied RtcSessionDescriptionInit as the remote description.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-setremotedescription
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in,out] PRtcSessionDescriptionInit IN/RtcSessionDescriptionInit that becomes our new remote description
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS setRemoteDescription(PRtcPeerConnection, PRtcSessionDescriptionInit);

/**
 * @brief Create a new RtcRtpTransceiver and add it to the set of transceivers.
 *
 * Reference https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-addtransceiver
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] PRtcMediaStreamTrack Stream track information for the codec appropriate codec, or NULL for RECVONLY
 * @param[in] PRtcRtpTransceiverInit PRtcRtpTransceiverInit that may configure our new Transceiver
 * @param[in,out] PRtcRtpTransceiver* IN/Initialized and configured RtcRtpTransceiver
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS addTransceiver(PRtcPeerConnection, PRtcMediaStreamTrack, PRtcRtpTransceiverInit, PRtcRtpTransceiver*);

/**
 * @brief Set a callback for transceiver frame
 *
 * @param[in] PRtcRtpTransceiver Populated RtcRtpTransceiver struct
 * @param[in] UINT64 User customData that will be passed along when RtcOnFrame is called
 * @param[in] RtcOnFrame User RtcOnFrame callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS transceiverOnFrame(PRtcRtpTransceiver, UINT64, RtcOnFrame);

/**
 * @brief Set a callback for bandwidth estimation results
 *
 * @param[in] PRtcRtpTransceiver Populated RtcRtpTransceiver struct
 * @param[in] UINT64 User customData that will be passed along when RtcOnBandwidthEstimation is called
 * @param[in] RtcOnBandwidthEstimation User RtcOnBandwidthEstimation callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS transceiverOnBandwidthEstimation(PRtcRtpTransceiver, UINT64, RtcOnBandwidthEstimation);

/**
 * @brief Frees the previously created transceiver object
 *
 * @param[in/out/opt] PRtcRtpTransceiver* IN/OUT/OPT RtcRtpTransceiver to be freed
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeTransceiver(PRtcRtpTransceiver*);

/**
 * @brief Initializes global state needed for all RtcPeerConnections. It must only be called once
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS initKvsWebRtc(VOID);

/**
 * @brief Deinitializes global state needed for all RtcPeerConnections. It must only be called once
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS deinitKvsWebRtc(VOID);

/**
 * @brief Adds to the list of codecs we support receiving.
 *
 * NOTE: The remote MUST only send codecs we declare
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] RTC_CODEC Codec that we support receiving.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS addSupportedCodec(PRtcPeerConnection, RTC_CODEC);

/**
 * @brief Packetizes and sends media via the configuration specified by the RtcRtpTransceiver
 *
 * @param[in] PRtcRtpTransceiver Configured and connected RtcRtpTransceiver to send media
 * @param[in] PFrame Frame of media that will be sent
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS writeFrame(PRtcRtpTransceiver, PFrame);

/**
 * @brief Provides a remote candidate to the ICE Agent.
 *
 * This method can also be used to indicate the end of remote candidates
 * when called with an empty string for the candidate member.
 *
 * Reference: https://www.w3.org/TR/webrtc/#dom-rtcpeerconnection-addicecandidate
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] PCHAR New remote ICE candidate to add
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
STATUS addIceCandidate(PRtcPeerConnection, PCHAR);

/**
 * @brief createDataChannel creates a new RtcDataChannel object with the given label.
 *
 * NOTE: The RtcDataChannelInit dictionary can be used to configure properties of the underlying
 * channel such as data reliability.
 * NOTE: Data channel can be created only before signaling for now
 *
 * Reference: https://www.w3.org/TR/webrtc/#methods-11
 *
 * @param[in] PRtcPeerConnection Initialized RtcPeerConnection
 * @param[in] PCHAR Data channel Name
 * @param[in] PRtcDataChannelInit Allowed to be NULL/defines underlying channel properties
 * @param[out] PRtcDataChannel* Created data channel with supplied channel name
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
STATUS createDataChannel(PRtcPeerConnection, PCHAR, PRtcDataChannelInit, PRtcDataChannel*);

/**
 * @brief Set a callback for data channel message
 *
 * @param[in] PRtcDataChannel Data channel struct created by createDataChannel()
 * @param[in] UINT64 User customData that will be passed along when RtcOnMessage is called
 * @param[in] RtcOnMessage User RtcOnMessage callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS dataChannelOnMessage(PRtcDataChannel, UINT64, RtcOnMessage);

/**
 * @brief Set a callback for data channel open
 *
 * @param[in] PRtcDataChannel Data channel struct created by createDataChannel()
 * @param[in] UINT64 User customData that will be passed along when RtcOnOpen is called
 * @param[in] RtcOnOpen User RtcOnOpen callback
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS dataChannelOnOpen(PRtcDataChannel, UINT64, RtcOnOpen);

/**
 * @brief Send data via the PRtcDataChannel
 *
 * Reference: https://www.w3.org/TR/webrtc/#dfn-send
 *
 * @param[in] PRtcDataChannel Configured and connected PRtcDataChannel
 * @param[in] BOOL Is message binary, if false will be delivered as a string
 * @param[in] PBYTE Data that you wish to send
 * @param[in] UINT32 Length of the PBYTE you wish to send
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 *
 */
STATUS dataChannelSend(PRtcDataChannel, BOOL, PBYTE, UINT32);

/**
 * @brief Creates a Signaling client and returns a handle to it
 *
 * @param[in] PSignalingClientInfo Signaling client info
 * @param[in] PChannelInfo Signaling channel info to use/create a channel
 * @param[in] PSignalingClientCallbacks Signaling callbacks for event notifications
 * @param[in] PAwsCredentialProvider Credential provider for auth integration
 * @param[out] PSIGNALING_CLIENT_HANDLE Returned signaling client handle
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createSignalingClientSync(PSignalingClientInfo, PChannelInfo, PSignalingClientCallbacks, PAwsCredentialProvider, PSIGNALING_CLIENT_HANDLE);

/**
 * @brief Frees the Signaling client object
 *
 * NOTE: The call is idempotent.
 *
 * @param[in/out/opt] PSIGNALING_CLIENT_HANDLE Signaling client handle to free
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeSignalingClient(PSIGNALING_CLIENT_HANDLE);

/**
 * @brief Send a message through a Signaling client.
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 * NOTE: This is a synchronous call. It will block and wait for sending the data and await for the ACK from the service.
 *
 * @param[in] SIGNALING_CLIENT_HANDLE Signaling client handle
 * @param[in] PSignalingMessage Message to send.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signalingClientSendMessageSync(SIGNALING_CLIENT_HANDLE, PSignalingMessage);

/**
 * @brief Gets the retrieved ICE configuration information object count
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 *
 * @param[in] SIGNALING_CLIENT_HANDLE Signaling client handle
 * @param[out] PUINT32 The count of the ICE configuration information objects
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signalingClientGetIceConfigInfoCount(SIGNALING_CLIENT_HANDLE, PUINT32);

/**
 * @brief Gets the ICE configuration information object given its index
 *
 * NOTE: The call will fail if the client is not in the CONNECTED state.
 * IMPORTANT: The returned pointer to the ICE configuration information object points to internal structures
 * and its contents should not be modified.
 *
 * @param SIGNALING_CLIENT_HANDLE
 * @param[in] UINT32 Index of the ICE configuration information object to retrieve
 * @param[out] PIceConfigInfo The pointer to the ICE configuration information object
 *
 * @return STATUS code of execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signalingClientGetIceConfigInfo(SIGNALING_CLIENT_HANDLE, UINT32, PIceConfigInfo*);


/**
 * @brief Connects the signaling client to the socket in order to send/receive messages.
 *
 * NOTE: The call will succeed only when the signaling client is in a ready state.
 * @param SIGNALING_CLIENT_HANDLE
 *
 * @return STATUS function execution status. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signalingClientConnectSync(SIGNALING_CLIENT_HANDLE);

/*
 * Gets the Signaling client current state.
 *
 * @param - SIGNALING_CLIENT_HANDLE - IN - Signaling client handle
 * @param - PSIGNALING_CLIENT_STATE - OUT - Current state of the signaling client as an UINT32 enum
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientGetCurrentState(SIGNALING_CLIENT_HANDLE, PSIGNALING_CLIENT_STATE);

/*
 * Gets a literal string representing a Signaling client state.
 *
 * @param - SIGNALING_CLIENT_STATE - IN - Signaling client state
 * @param - PCHAR* - OUT - Read only string representing the state
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS signalingClientGetStateString(SIGNALING_CLIENT_STATE, PCHAR*);

#ifdef  __cplusplus
}
#endif
#endif  /* __KINESIS_VIDEO_WEBRTC_CLIENT_INCLUDE__ */
