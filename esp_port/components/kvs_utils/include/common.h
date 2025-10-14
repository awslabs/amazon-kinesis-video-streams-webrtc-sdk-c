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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "platform_esp32.h"
#include "error.h"

#ifndef JSMN_HEADER
#define JSMN_HEADER
#endif
#include <jsmn.h>

#ifndef MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80
/**
 * Copied straight from mbedtls/include/mbedtls/ssl.h
 * When not using DTLS (i.e., in signaling only mode), we can disable DTLS,
 * but the build system still tries to include the DTLS headers.
 * This macro keeps the build system happy.
 */
#define MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80     ((uint16_t) 0x0001)
#define MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_32     ((uint16_t) 0x0002)
#endif

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/**
 * Max device name length in chars
 */
#define MAX_DEVICE_NAME_LEN 128

/**
 * Max stream count for sanity validation - 1M
 */
#define MAX_STREAM_COUNT 1000000

/**
 * Max stream name length chars
 */
#define MAX_STREAM_NAME_LEN 256

/**
 * Max update version length in chars
 * https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_DeleteSignalingChannel.html#KinesisVideo-DeleteSignalingChannel-request-CurrentVersion
 */
#define MAX_UPDATE_VERSION_LEN 64

/**
 * Max ARN len in chars
 * https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_DescribeSignalingChannel.html#API_DescribeSignalingChannel_RequestSyntax
 * https://docs.aws.amazon.com/kinesisvideostreams/latest/dg/API_CreateStream.html#KinesisVideo-CreateStream-request-KmsKeyId
 */
#define MAX_ARN_LEN 2048

/**
 * Max len of the auth data (STS or Cert) in bytes
 */
#define MAX_AUTH_LEN 10000

/**
 * Max len of the fully qualified URI
 */
#define MAX_URI_CHAR_LEN 8000

/**
 * Min streaming token expiration duration. Currently defined as 30 seconds.
 */
#define MIN_STREAMING_TOKEN_EXPIRATION_DURATION (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * The max streaming token expiration duration after which the ingestion host will force terminate the connection.
 */
#define MAX_ENFORCED_TOKEN_EXPIRATION_DURATION (40 * HUNDREDS_OF_NANOS_IN_A_MINUTE)

/**
 * Grace period for the streaming token expiration - 3 seconds
 */
#define STREAMING_TOKEN_EXPIRATION_GRACE_PERIOD (3 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Service call default timeout - 5 seconds
 */
#define SERVICE_CALL_DEFAULT_TIMEOUT (10 * HUNDREDS_OF_NANOS_IN_A_SECOND)

/**
 * Service call infinite timeout for streaming
 */
#define SERVICE_CALL_INFINITE_TIMEOUT MAX_UINT64

/**
 * Default service call retry count
 */
#define SERVICE_CALL_MAX_RETRY_COUNT 5

/**
 * This is a sentinel indicating an invalid timestamp value
 */
#ifndef INVALID_TIMESTAMP_VALUE
#define INVALID_TIMESTAMP_VALUE ((UINT64) 0xFFFFFFFFFFFFFFFFULL)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_TIMESTAMP
#define IS_VALID_TIMESTAMP(h) ((h) != INVALID_TIMESTAMP_VALUE)
#endif

/**
 * Environment variable to enable file logging. Run export AWS_ENABLE_FILE_LOGGING=TRUE to enable file
 * logging
 */
#define ENABLE_FILE_LOGGING ((PCHAR) "AWS_ENABLE_FILE_LOGGING")

/**
 * Max region name
 */
#define MAX_REGION_NAME_LEN 128

/**
 * Max user agent string
 */
#define MAX_USER_AGENT_LEN 256

/**
 * Max custom user agent string
 */
#define MAX_CUSTOM_USER_AGENT_LEN 128

/**
 * Max custom user agent name postfix string
 */
#define MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN 32

/**
 * Default Video track ID to be used
 */
#define DEFAULT_VIDEO_TRACK_ID 1

/**
 * Default Audio track ID to be used
 */
#define DEFAULT_AUDIO_TRACK_ID 2

/*
 * Max access key id length https://docs.aws.amazon.com/STS/latest/APIReference/API_Credentials.html
 */
#define MAX_ACCESS_KEY_LEN 128

/*
 * Max secret access key length
 */
#define MAX_SECRET_KEY_LEN 128

/*
 * Max session token string length
 */
#define MAX_SESSION_TOKEN_LEN 2048

/*
 * Max expiration string length
 */
#define MAX_EXPIRATION_LEN 128

/*
 * Max role alias length https://docs.aws.amazon.com/iot/latest/apireference/API_UpdateRoleAlias.html
 */
#define MAX_ROLE_ALIAS_LEN 128

/*
 * Max AWS region length
 */
#define MAX_AWS_REGION_LEN 16

/**
 * Max string length for IoT thing name
 */
#define MAX_IOT_THING_NAME_LEN MAX_DEVICE_NAME_LEN

/**
 * Default period for the cached endpoint update
 */
#define DEFAULT_ENDPOINT_CACHE_UPDATE_PERIOD (40 * HUNDREDS_OF_NANOS_IN_A_MINUTE)

/**
 * Sentinel value indicating to use default update period
 */
#define ENDPOINT_UPDATE_PERIOD_SENTINEL_VALUE 0

/**
 * Max period for the cached endpoint update
 */
#define MAX_ENDPOINT_CACHE_UPDATE_PERIOD (24 * HUNDREDS_OF_NANOS_IN_AN_HOUR)

/**
 * AWS credential environment variable name
 */
#define ACCESS_KEY_ENV_VAR      ((PCHAR) "AWS_ACCESS_KEY_ID")
#define SECRET_KEY_ENV_VAR      ((PCHAR) "AWS_SECRET_ACCESS_KEY")
#define SESSION_TOKEN_ENV_VAR   ((PCHAR) "AWS_SESSION_TOKEN")
#define DEFAULT_REGION_ENV_VAR  ((PCHAR) "AWS_DEFAULT_REGION")
#define CACERT_PATH_ENV_VAR     ((PCHAR) "AWS_KVS_CACERT_PATH")
#define DEBUG_LOG_LEVEL_ENV_VAR ((PCHAR) "AWS_KVS_LOG_LEVEL")

// #ifdef CMAKE_DETECTED_CACERT_PATH
// #define DEFAULT_KVS_CACERT_PATH KVS_CA_CERT_PATH
// #else
// #ifdef KVS_PLAT_ESP_FREERTOS
#define DEFAULT_KVS_CACERT_PATH "/spiffs/certs/cacert.pem"
// #else
// #define DEFAULT_KVS_CACERT_PATH EMPTY_STRING
// #endif
// #endif

// Protocol scheme names
#define HTTPS_SCHEME_NAME "https"
#define WSS_SCHEME_NAME   "wss"

// Max header name length in chars
#define MAX_REQUEST_HEADER_NAME_LEN 128

// Max header value length in chars
#define MAX_REQUEST_HEADER_VALUE_LEN 2048

// Max header count
#define MAX_REQUEST_HEADER_COUNT 200

// Max delimiter characters when packing headers into a string for printout
#define MAX_REQUEST_HEADER_OUTPUT_DELIMITER 5

// Max request header length in chars including the name/value, delimiter and null terminator
#define MAX_REQUEST_HEADER_STRING_LEN (MAX_REQUEST_HEADER_NAME_LEN + MAX_REQUEST_HEADER_VALUE_LEN + 3)

// Literal definitions of the request verbs
#define HTTP_REQUEST_VERB_GET_STRING  (PCHAR) "GET"
#define HTTP_REQUEST_VERB_PUT_STRING  (PCHAR) "PUT"
#define HTTP_REQUEST_VERB_POST_STRING (PCHAR) "POST"

// Schema delimiter string
#define SCHEMA_DELIMITER_STRING (PCHAR) "://"

// Default canonical URI if we fail to get anything from the parsing
#define DEFAULT_CANONICAL_URI_STRING (PCHAR) "/"

// HTTP status OK
#define HTTP_STATUS_CODE_OK 200

// HTTP status Request timed out
#define HTTP_STATUS_CODE_REQUEST_TIMEOUT 408

/**
 * Maximal length of the credentials file
 */
#define MAX_CREDENTIAL_FILE_LEN MAX_AUTH_LEN

/**
 * Default AWS region
 */
#define DEFAULT_AWS_REGION "us-east-1"

/**
 * Control plane prefix
 */
#define CONTROL_PLANE_URI_PREFIX "https://"

/**
 * KVS service name
 */
#define KINESIS_VIDEO_SERVICE_NAME "kinesisvideo"

/**
 * Control plane postfix
 */
#define CONTROL_PLANE_URI_POSTFIX ".amazonaws.com"

/**
 * Default user agent name
 */
#define DEFAULT_USER_AGENT_NAME "AWS-SDK-KVS"

/**
 * Max number of tokens in the API return JSON
 */
#define MAX_JSON_TOKEN_COUNT 100

/**
 * Current versions for the public structs
 */
#define AWS_CREDENTIALS_CURRENT_VERSION 0

/**
 * Buffer length for the error to be stored in
 */
#define CALL_INFO_ERROR_BUFFER_LEN 256

/**
 * Parameterized string for each tag pair
 */
#define TAG_PARAM_JSON_TEMPLATE "\n\t\t\"%s\": \"%s\","

/**
 * Low speed limits in bytes per duration
 */
#define DEFAULT_LOW_SPEED_LIMIT 30

/**
 * Low speed limits in 100ns for the amount of bytes per this duration
 */
#define DEFAULT_LOW_SPEED_TIME_LIMIT (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

// Header delimiter for requests and it's size
#define REQUEST_HEADER_DELIMITER      ((PCHAR) ": ")
#define REQUEST_HEADER_DELIMITER_SIZE (2 * SIZEOF(CHAR))

/*
 * Default SSL port
 */
#define DEFAULT_SSL_PORT_NUMBER 443

/*
 * Default non-SSL port
 */
#define DEFAULT_NON_SSL_PORT_NUMBER 8080

/**
 * AWS service Request id header name
 */
#define KVS_REQUEST_ID_HEADER_NAME "x-amzn-RequestId"

/**
 * Service call result
 */
typedef enum {
    // Not defined
    SERVICE_CALL_RESULT_NOT_SET = 0,

    // All OK
    SERVICE_CALL_RESULT_OK = 200,

    // Invalid params error
    SERVICE_CALL_INVALID_ARG = 406,

    // Resource not found exception
    SERVICE_CALL_RESOURCE_NOT_FOUND = 404,

    // Client limit exceeded error
    SERVICE_CALL_CLIENT_LIMIT = 10000,

    // Device limit exceeded error
    SERVICE_CALL_DEVICE_LIMIT = 10001,

    // Stream limit exception
    SERVICE_CALL_STREAM_LIMIT = 10002,

    // Resource in use exception
    SERVICE_CALL_RESOURCE_IN_USE = 10003,

    // Bad request
    SERVICE_CALL_BAD_REQUEST = 400,

    // Forbidden
    SERVICE_CALL_FORBIDDEN = 403,

    // Security Credentials Expired
    SERVICE_CALL_SIGNATURE_EXPIRED = 10008,

    // device time ahead of server
    SERVICE_CALL_SIGNATURE_NOT_YET_CURRENT = 10009,

    // Device not provisioned
    SERVICE_CALL_DEVICE_NOT_PROVISIONED = 10004,

    // Device not found
    SERVICE_CALL_DEVICE_NOT_FOUND = 10005,

    // Security error
    SERVICE_CALL_NOT_AUTHORIZED = 401,

    // Request timeout
    SERVICE_CALL_REQUEST_TIMEOUT = 408,

    // Gateway timeout
    SERVICE_CALL_GATEWAY_TIMEOUT = 504,

    // Network read timeout
    SERVICE_CALL_NETWORK_READ_TIMEOUT = 598,

    // Network connection timeout
    SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT = 599,

    // Resource deleted exception
    SERVICE_CALL_RESOURCE_DELETED = 10400,

    // The stream authorization is in a grace period
    SERVICE_CALL_STREAM_AUTH_IN_GRACE_PERIOD = 10401,

    // Not implemented
    SERVICE_CALL_NOT_IMPLEMENTED = 501,

    // Internal server error
    SERVICE_CALL_INTERNAL_ERROR = 500,

    // Service unavailable
    SERVICE_CALL_SERVICE_UNAVAILABLE = 503,

    // Other errors
    SERVICE_CALL_UNKNOWN = 10006,

    // Auth failure we don't know the specific reason at this layer
    SERVICE_CALL_AUTH_FAILURE = 10007,

    // ACK errors
    // Error when reading the input stream
    SERVICE_CALL_RESULT_STREAM_READ_ERROR = 4000,

    // Fragment size is greater than the limit
    SERVICE_CALL_RESULT_FRAGMENT_SIZE_REACHED = 4001,

    // Fragment duration is greater than the limit
    SERVICE_CALL_RESULT_FRAGMENT_DURATION_REACHED = 4002,

    // Connection duration is greater than allowed threshold
    SERVICE_CALL_RESULT_CONNECTION_DURATION_REACHED = 4003,

    // Fragment timecode is not monotonically increasing
    SERVICE_CALL_RESULT_FRAGMENT_TIMECODE_NOT_MONOTONIC = 4004,

    // Multi-track MKV is not supported
    SERVICE_CALL_RESULT_MULTI_TRACK_MKV = 4005,

    // MKV parsing error
    SERVICE_CALL_RESULT_INVALID_MKV_DATA = 4006,

    // Invalid producer timestamp
    SERVICE_CALL_RESULT_INVALID_PRODUCER_TIMESTAMP = 4007,

    // Inactive stream
    SERVICE_CALL_RESULT_STREAM_NOT_ACTIVE = 4008,

    // Fragment metadata name/value/count limit reached
    SERVICE_CALL_RESULT_FRAGMENT_METADATA_LIMIT_REACHED = 4009,

    // Track number in simple block doesn't match the track number in TrackInfo
    SERVICE_CALL_RESULT_TRACK_NUMBER_MISMATCH = 4010,

    // Frames (simple block) missing for at least of one of the Tracks specified in TrackInfo
    SERVICE_CALL_RESULT_FRAMES_MISSING_FOR_TRACK = 4011,

    // KVS doesn't accept MKV input with more than 3 tracks (limit can be changed via SDC config)
    SERVICE_CALL_RESULT_MORE_THAN_ALLOWED_TRACKS_FOUND = 4012,

    // KMS specific error - KMS access denied while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_ACCESS_DENIED = 4500,

    // KMS specific error - KMS key is disabled while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_DISABLED = 4501,

    // KMS specific error - KMS throws validation error while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_VALIDATION_ERROR = 4502,

    // KMS specific error - KMS key unavailable while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_UNAVAILABLE = 4503,

    // KMS specific error - Invalid usage of the KMS key while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_INVALID_USAGE = 4504,

    // KMS specific error - Invalid state of the KMS key while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_INVALID_STATE = 4505,

    // KMS specific error - KMS key not found while encrypting data
    SERVICE_CALL_RESULT_KMS_KEY_NOT_FOUND = 4506,

    // Stream has been/is being deleted
    SERVICE_CALL_RESULT_STREAM_DELETED = 4507,

    // Internal error
    SERVICE_CALL_RESULT_ACK_INTERNAL_ERROR = 5000,

    // Fragment archiving error in the persistent storage
    SERVICE_CALL_RESULT_FRAGMENT_ARCHIVAL_ERROR = 5001,

    // Go Away result
    SERVICE_CALL_RESULT_SIGNALING_GO_AWAY = 6000,

    // Reconnect ICE Server
    SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE = 6001,

    // Unknown ACK error
    SERVICE_CALL_RESULT_UNKNOWN_ACK_ERROR = 7000,
} SERVICE_CALL_RESULT;

////////////////////////////////////////////////////
/// Main enum declarations
////////////////////////////////////////////////////
/*! \addtogroup PubicEnums
 *
 * @{
 */

/**
 * @brief Types of verbs
 */
typedef enum {
    HTTP_REQUEST_VERB_GET,  //!< Indicates GET type of HTTP request
    HTTP_REQUEST_VERB_POST, //!< Indicates POST type of HTTP request
    HTTP_REQUEST_VERB_PUT   //!< Indicates PUT type of HTTP request
} HTTP_REQUEST_VERB;

/**
 * @brief Request SSL certificate type Not specified, "DER", "PEM", "ENG"
 */
typedef enum {
    SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, //!< Default enum when type of certificate is not specified
    SSL_CERTIFICATE_TYPE_DER,           //!< Use DER type of SSL certificate if certificate to use is *.der
    SSL_CERTIFICATE_TYPE_PEM,           //!< Use PEM type of SSL certificate if certificate to use is *.pem
    SSL_CERTIFICATE_TYPE_ENG,           //!< Use ENG type of SSL certificate if certificate to use is *.eng
} SSL_CERTIFICATE_TYPE;
/*!@} */

/////////////////////////////////////////////////////
/// Structures available for use by applications
/////////////////////////////////////////////////////

/*! \addtogroup PublicStructures
 *
 * @{
 */

/**
 * @brief AWS Credentials declaration
 */
typedef struct __AwsCredentials AwsCredentials;
struct __AwsCredentials {
    UINT32 version;         //!< Version of structure
    UINT32 size;            //!< Size of the entire structure in bytes including the struct itself
    PCHAR accessKeyId;      //!< Access Key ID - NULL terminated
    UINT32 accessKeyIdLen;  //!< Length of the access key id - not including NULL terminator
    PCHAR secretKey;        //!< Secret Key - NULL terminated
    UINT32 secretKeyLen;    //!< Length of the secret key - not including NULL terminator
    PCHAR sessionToken;     //!< Session token - NULL terminated
    UINT32 sessionTokenLen; //!< Length of the session token - not including NULL terminator
    UINT64 expiration;      //!< Expiration in absolute time in 100ns.
    //!< The rest of the data might follow the structure
};
typedef struct __AwsCredentials* PAwsCredentials;

/**
 * @brief Abstract base for the credential provider
 */
typedef struct __AwsCredentialProvider* PAwsCredentialProvider;

/*! \addtogroup Callbacks
 * Callback definitions
 *  @{
 */

/**
 * @brief Function returning AWS credentials
 */
typedef STATUS (*GetCredentialsFunc)(PAwsCredentialProvider, PAwsCredentials*);
/*!@} */

typedef struct __AwsCredentialProvider AwsCredentialProvider;
struct __AwsCredentialProvider {
    GetCredentialsFunc getCredentialsFn; //!< Get credentials function which will be overwritten by different implementations
};
/*!@} */

////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////

#include <util.h>
#include <version.h>

#ifdef __cplusplus
}
#endif
