/**
 * Minimal stub of producer-c common/Include.h for self-contained builds (signaling OFF).
 * Provides only the definitions needed by non-signaling WebRTC code.
 * AWS-specific types and macros are guarded by ENABLE_SIGNALING.
 */
#ifndef __KINESIS_VIDEO_COMMON_INCLUDE__
#define __KINESIS_VIDEO_COMMON_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/client/Include.h>
#ifndef JSMN_HEADER
#define JSMN_HEADER
#endif
#include <com/amazonaws/kinesis/video/common/jsmn.h>

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
/*!@} */

#define DEBUG_LOG_LEVEL_ENV_VAR ((PCHAR) "AWS_KVS_LOG_LEVEL")

////////////////////////////////////////////////////
// Lengths used by non-signaling code
////////////////////////////////////////////////////
#define MAX_JSON_TOKEN_COUNT    100

#ifdef __cplusplus
}
#endif

#endif /* __KINESIS_VIDEO_COMMON_INCLUDE__ */
