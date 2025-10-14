#pragma once


/**
 * Main public include file
 */
#ifndef __KINESIS_VIDEO_COMMON_INCLUDE__
#define __KINESIS_VIDEO_COMMON_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include "common_defs.h"
#include <com/amazonaws/kinesis/video/client/Include.h>
#ifndef JSMN_HEADER
#define JSMN_HEADER
#endif
#include "jsmn.h"

////////////////////////////////////////////////////
/// New common base status code.
/// All common library status codes defined
/// should continue from the STATUS_COMMON_BASE
////////////////////////////////////////////////////

/*! \addtogroup NewCommonBaseStatusCode
 *  @{
 */

/**
 * Continue errors from the new common base
 */
#define STATUS_COMMON_BASE                          0x16000000
#define STATUS_CURL_PERFORM_FAILED                  STATUS_COMMON_BASE + 0x00000001
#define STATUS_IOT_INVALID_RESPONSE_LENGTH          STATUS_COMMON_BASE + 0x00000002
#define STATUS_IOT_NULL_AWS_CREDS                   STATUS_COMMON_BASE + 0x00000003
#define STATUS_IOT_INVALID_URI_LEN                  STATUS_COMMON_BASE + 0x00000004
#define STATUS_TIMESTAMP_STRING_UNRECOGNIZED_FORMAT STATUS_COMMON_BASE + 0x00000005
/*!@} */

/**
 * Macro for checking whether the status code should be retried by the continuous retry logic
 */
#define IS_RETRIABLE_COMMON_LIB_ERROR(error)                                                                                                         \
    ((error) == STATUS_INVALID_API_CALL_RETURN_JSON || (error) == STATUS_CURL_INIT_FAILED || (error) == STATUS_CURL_LIBRARY_INIT_FAILED ||           \
     (error) == STATUS_HMAC_GENERATION_ERROR || (error) == STATUS_CURL_PERFORM_FAILED || (error) == STATUS_IOT_INVALID_RESPONSE_LENGTH ||            \
     (error) == STATUS_IOT_NULL_AWS_CREDS || (error) == STATUS_IOT_INVALID_URI_LEN || (error) == STATUS_IOT_EXPIRATION_OCCURS_IN_PAST ||             \
     (error) == STATUS_IOT_EXPIRATION_PARSING_FAILED || (error) == STATUS_IOT_CREATE_LWS_CONTEXT_FAILED ||                                           \
     (error) == STATUS_FILE_CREDENTIAL_PROVIDER_OPEN_FILE_FAILED || (error) == STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_LENGTH ||                \
     (error) == STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_FORMAT)

/////////////////////////////////////////////////////
/// Lengths of different character arrays
/////////////////////////////////////////////////////

/*! \addtogroup NameLengths
 * Lengths of some string members of different structures
 *  @{
 */

/**
 * Maximum allowed region name length
 */
#define MAX_REGION_NAME_LEN 128

/**
 * Maximum allowed user agent string length
 */
#define MAX_USER_AGENT_LEN 256

/**
 * Maximum allowed custom user agent string length
 */
#define MAX_CUSTOM_USER_AGENT_LEN 128

/**
 * Maximum allowed custom user agent name postfix string length
 */
#define MAX_CUSTOM_USER_AGENT_NAME_POSTFIX_LEN 32

/**
 * Maximum allowed access key id length https://docs.aws.amazon.com/STS/latest/APIReference/API_Credentials.html
 */
#define MAX_ACCESS_KEY_LEN 128

/**
 * Maximum allowed secret access key length
 */
#define MAX_SECRET_KEY_LEN 128

/**
 * Maximum allowed session token string length
 */
#define MAX_SESSION_TOKEN_LEN 2048

/**
 * Maximum allowed expiration string length
 */
#define MAX_EXPIRATION_LEN 128

/**
 * Maximum allowed role alias length https://docs.aws.amazon.com/iot/latest/apireference/API_UpdateRoleAlias.html
 */
#define MAX_ROLE_ALIAS_LEN 128

/**
 * Maximum allowed request header length
 */
#define MAX_REQUEST_HEADER_NAME_LEN 128

/**
 * Maximum allowed header value length
 */
#define MAX_REQUEST_HEADER_VALUE_LEN 2048

/**
 * Maximum request header length in chars including the name/value, delimiter and null terminator
 */
#define MAX_REQUEST_HEADER_STRING_LEN (MAX_REQUEST_HEADER_NAME_LEN + MAX_REQUEST_HEADER_VALUE_LEN + 3)

/**
 * Maximum length of the credentials file
 */
#define MAX_CREDENTIAL_FILE_LEN MAX_AUTH_LEN

/**
 * Buffer length for the error to be stored in
 */
#define CALL_INFO_ERROR_BUFFER_LEN 256

/**
 * Max parameter JSON string len which will be used for preparing the parameterized strings for the API calls.
 */
#define MAX_JSON_PARAMETER_STRING_LEN (4 * 1024)
/*!@} */

/**
 * Default Video track ID to be used
 */
#define DEFAULT_VIDEO_TRACK_ID 1

/**
 * Default Audio track ID to be used
 */
#define DEFAULT_AUDIO_TRACK_ID 2

/**
 * Default Audio only track ID to be used
 */
#define DEFAULT_AUDIO_ONLY_TRACK_ID 1

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

/////////////////////////////////////////////////////
/// Environment variables
/////////////////////////////////////////////////////

/*! \addtogroup EnvironmentVariables
 * Environment variable name
 *  @{
 */

/**
 * AWS Access Key value. Run `export AWS_ACCESS_KEY_ID=<value>` to provide AWS access key
 */
#define ACCESS_KEY_ENV_VAR ((PCHAR) "AWS_ACCESS_KEY_ID")

/**
 * AWS Secret Key value. Run `export AWS_SECRET_ACCESS_KEY=<value>` to provide AWS secret key
 */
#define SECRET_KEY_ENV_VAR ((PCHAR) "AWS_SECRET_ACCESS_KEY")

/**
 * AWS Session token value. Run `export AWS_SESSION_TOKEN=<value>` to provide AWS session token
 */
#define SESSION_TOKEN_ENV_VAR ((PCHAR) "AWS_SESSION_TOKEN")

/**
 * Closest AWS region to run Producer SDK. Run `export AWS_DEFAULT_REGION=<value>` to provide AWS region
 */
#define DEFAULT_REGION_ENV_VAR ((PCHAR) "AWS_DEFAULT_REGION")

/**
 * KVS CA Cert path. Provide this path if a cert is available in a path other than default. Run
 * `export AWS_KVS_CACERT_PATH=<value>` to provide Cert path
 */
#define CACERT_PATH_ENV_VAR ((PCHAR) "AWS_KVS_CACERT_PATH")

/**
 * KVS log level. KVS provides 7 log levels. Run `export AWS_KVS_LOG_LEVEL=<value>` to select log level
 */
#define DEBUG_LOG_LEVEL_ENV_VAR ((PCHAR) "AWS_KVS_LOG_LEVEL")

/**
 * Environment variable to enable file logging. Run export AWS_ENABLE_FILE_LOGGING=TRUE to enable file
 * logging
 */
#define ENABLE_FILE_LOGGING ((PCHAR) "AWS_ENABLE_FILE_LOGGING")
/*!@} */

/////////////////////////////////////////////////////
/// String constants
/////////////////////////////////////////////////////

/*! \addtogroup StringConstants
 * Fixed string defines
 *  @{
 */

/**
 * HTTPS Protocol scheme name
 */
#define HTTPS_SCHEME_NAME "https"

/**
 * WSS Protocol scheme name
 */
#define WSS_SCHEME_NAME "wss"

/**
 * HTTP GET request string
 */
#define HTTP_REQUEST_VERB_GET_STRING (PCHAR) "GET"
/**
 * HTTP PUT request string
 */
#define HTTP_REQUEST_VERB_PUT_STRING (PCHAR) "PUT"
/**
 * HTTP POST request string
 */
#define HTTP_REQUEST_VERB_POST_STRING (PCHAR) "POST"

/**
 * Schema delimiter string
 */
#define SCHEMA_DELIMITER_STRING (PCHAR) "://"

/**
 * Default canonical URI if we fail to get anything from the parsing
 */
#define DEFAULT_CANONICAL_URI_STRING (PCHAR) "/"

/**
 * Control plane prefix
 */
#define CONTROL_PLANE_URI_PREFIX "https://"

/**
 * KVS service name
 */
#define KINESIS_VIDEO_SERVICE_NAME "kinesisvideo"

#define AWS_KVS_FIPS_ENDPOINT_POSTFIX "-fips"

/**
 * Control plane postfix
 */
#define CONTROL_PLANE_URI_POSTFIX ".amazonaws.com"

#define CONTROL_PLANE_URI_POSTFIX_CN ".amazonaws.com.cn"

#define CONTROL_PLANE_URI_POSTFIX_ISO ".c2s.ic.gov"

#define CONTROL_PLANE_URI_POSTFIX_ISO_B ".sc2s.sgov.gov"

#define AWS_ISO_B_REGION_PREFIX "us-isob-"

#define AWS_ISO_REGION_PREFIX "us-iso-"

#define AWS_GOV_CLOUD_REGION_PREFIX "us-gov-"

#define AWS_CN_REGION_PREFIX "cn-"

/**
 * Default user agent name
 */
#define DEFAULT_USER_AGENT_NAME "AWS-SDK-KVS"

/**
 * Parameterized string for each tag pair
 */
#define TAG_PARAM_JSON_TEMPLATE "\n\t\t\"%s\": \"%s\","

/**
 * Header delimiter for requests and it's size
 */
#define REQUEST_HEADER_DELIMITER ((PCHAR) ": ")

/**
 * AWS service Request id header name
 */
#define KVS_REQUEST_ID_HEADER_NAME "x-amzn-RequestId"
/*!@} */

/////////////////////////////////////////////////////
/// Limits and counts
/////////////////////////////////////////////////////

/*! \addtogroup Limits
 * Limits and count macros
 *  @{
 */

// Max header count
#define MAX_REQUEST_HEADER_COUNT 200

// Max delimiter characters when packing headers into a string for printout
#define MAX_REQUEST_HEADER_OUTPUT_DELIMITER 5

// HTTP status OK
#define HTTP_STATUS_CODE_OK 200

// HTTP status Request timed out
#define HTTP_STATUS_CODE_REQUEST_TIMEOUT 408

/**
 * Max number of tokens in the API return JSON
 */
#define MAX_JSON_TOKEN_COUNT 100

/**
 * Low speed limits in bytes per duration
 */
#define DEFAULT_LOW_SPEED_LIMIT 30

/**
 * Low speed limits in 100ns for the amount of bytes per this duration
 */
#define DEFAULT_LOW_SPEED_TIME_LIMIT (30 * HUNDREDS_OF_NANOS_IN_A_SECOND)

#define REQUEST_HEADER_DELIMITER_SIZE (2 * SIZEOF(CHAR))
/*!@} */

/////////////////////////////////////////////////////
/// Miscellaneous
/////////////////////////////////////////////////////

/*! \addtogroup Miscellaneous
 * Miscellaneous macros
 *  @{
 */

/**
 * Current versions for the public structs
 */
#define AWS_CREDENTIALS_CURRENT_VERSION 0

/**
 * Default SSL port
 */
#define DEFAULT_SSL_PORT_NUMBER 443

/**
 * Default non-SSL port
 */
#define DEFAULT_NON_SSL_PORT_NUMBER 8080
/*!@} */


////////////////////////////////////////////////////
/// Public functions
////////////////////////////////////////////////////

/*! \addtogroup PublicMemberFunctions
 * @{
 */

/**
 * @brief Creates an AWS credentials object
 *
 * @param[in] PCHAR Access Key Id
 * @param[in] UINT32 Access Key Id Length excluding NULL terminator or 0 to calculate
 * @param[in] PCHAR Secret Key
 * @param[in] UINT32 Secret Key Length excluding NULL terminator or 0 to calculate
 * @param[in,opt] PCHAR Session Token
 * @param[in,opt] UINT32 Session Token Length excluding NULL terminator or 0 to calculate
 * @param[in] UINT64 Expiration in 100ns absolute time
 * @param[out] PAwsCredentials* Constructed object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createAwsCredentials(PCHAR, UINT32, PCHAR, UINT32, PCHAR, UINT32, UINT64, PAwsCredentials*);

/**
 * @brief Frees an Aws credentials object
 *
 * @param[in,out] PAwsCredentials* Credentials object to be destroyed.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeAwsCredentials(PAwsCredentials*);

/**
 * @ brief Deserialize an AWS credentials object, adapt the accessKey/secretKey/sessionToken pointer
 * to offset following the AwsCredential structure
 *
 * @param[in] PBYTE Token to be deserialized.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS deserializeAwsCredentials(PBYTE);

/**
 * @brief Creates a Static AWS credential provider object
 *
 * @param[in] PCHAR AWS Access Key Id
 * @param[in] UINT32 Access Key Id Length excluding NULL terminator or 0 to calculate
 * @param[in] PCHAR AWS Secret Key
 * @param[in] UINT32 Secret Key Length excluding NULL terminator or 0 to calculate
 * @param[in,opt] PCHAR Session Token
 * @param[in,opt] UINT32 Session Token Length excluding NULL terminator or 0 to calculate
 * @param[in] UINT64 Expiration in 100ns absolute time
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createStaticCredentialProvider(PCHAR, UINT32, PCHAR, UINT32, PCHAR, UINT32, UINT64, PAwsCredentialProvider*);

/**
 * @brief Frees a Static Aws credential provider object
 *
 * @param[in,out] PAwsCredentialProvider* Object to be destroyed.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeStaticCredentialProvider(PAwsCredentialProvider*);

/**
 * @brief Creates an IoT based AWS credential provider object using libCurl
 *
 * @param[in] PCHAR iot endpoint
 * @param[in] PCHAR cert file path
 * @param[in] PCHAR private key file path
 * @param[in,opt] PCHAR ca cert file path
 * @param[in] PCHAR role alias
 * @param[in] PCHAR iot thing name
 * @param[out] PAwsCredentialProvider* constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createCurlIotCredentialProvider(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PAwsCredentialProvider*);

/**
 * @brief Creates an IoT based AWS credential provider object using libWebSockets
 *
 * @param[in] PCHAR IoT endpoint
 * @param[in] PCHAR Cert file path
 * @param[in] PCHAR Private key file path
 * @param[in,opt] PCHAR CA cert file path
 * @param[in] PCHAR Role alias
 * @param[in] PCHAR IoT thing name
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createLwsIotCredentialProvider(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PAwsCredentialProvider*);

/**
 * @brief Creates an IoT based AWS credential provider object with time function which is based on libCurl
 *
 * @param[in] PCHAR IoT endpoint
 * @param[in] PCHAR Cert file path
 * @param[in] PCHAR Private key file path
 * @param[in] PCHAR CA cert file path
 * @param[in] PCHAR Role alias
 * @param[in] PCHAR IoT thing name
 * @param[in] UINT64 connection timeout
 * @param[in] UINT64 completion timeout
 * @param[in] GetCurrentTimeFunc Custom current time function
 * @param[in] UINT64 Time function custom data
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createCurlIotCredentialProviderWithTimeAndTimeout(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, UINT64, UINT64, GetCurrentTimeFunc,
                                                                    UINT64, PAwsCredentialProvider*);

/**
 * @brief Creates an IoT based AWS credential provider object with time function which is based on libCurl
 *
 * @param[in] PCHAR IoT endpoint
 * @param[in] PCHAR Cert file path
 * @param[in] PCHAR Private key file path
 * @param[in] PCHAR CA cert file path
 * @param[in] PCHAR Role alias
 * @param[in] PCHAR IoT thing name
 * @param[in] GetCurrentTimeFunc Custom current time function
 * @param[in] UINT64 Time function custom data
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createCurlIotCredentialProviderWithTime(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, GetCurrentTimeFunc, UINT64,
                                                          PAwsCredentialProvider*);

/**
 * @brief Creates an IoT based AWS credential provider object with time function which is based on libWebSockets
 *
 * @param[in] PCHAR IoT endpoint
 * @param[in] PCHAR Cert file path
 * @param[in] PCHAR Private key file path
 * @param[in] PCHAR CA cert file path
 * @param[in] PCHAR Role alias
 * @param[in] PCHAR IoT thing name
 * @param[in] GetCurrentTimeFunc Custom current time function
 * @param[in] UINT64 function custom data
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createLwsIotCredentialProviderWithTime(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, GetCurrentTimeFunc, UINT64,
                                                         PAwsCredentialProvider*);

/**
 * @brief Frees an IoT based Aws credential provider object
 *
 * @param[in,out] PAwsCredentialProvider* Object to be destroyed.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeIotCredentialProvider(PAwsCredentialProvider*);

/**
 * @brief Creates a File based AWS credential provider object
 *
 * @param[in] PCHAR Credentials file path
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createFileCredentialProvider(PCHAR, PAwsCredentialProvider*);

/**
 * @brief Creates a File based AWS credential provider object
 *
 * @param[in] PCHAR Credentials file path
 * @param[in] GetCurrentTimeFunc Current time function
 * @param[in] UINT64 Time function custom data
 * @param[out] PAwsCredentialProvider* Constructed AWS credentials provider object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createFileCredentialProviderWithTime(PCHAR, GetCurrentTimeFunc, UINT64, PAwsCredentialProvider*);

/**
 * @brief Frees a File based Aws credential provider object
 *
 * @param[in,out] PAwsCredentialProvider* Object to be destroyed.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeFileCredentialProvider(PAwsCredentialProvider*);


/**
 * @brief Signs a request by appending SigV4 headers
 *
 * @param[in,out] PRequestInfo* request info for signing
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signAwsRequestInfo(PRequestInfo);

/**
 * @brief Signs a request by appending SigV4 query param
 *
 * @param[in,out] PRequestInfo* Request info for signing
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS signAwsRequestInfoQueryParam(PRequestInfo);

/**
 * @brief Gets a request host string
 *
 * @param[in] PCHAR Request URL
 * @param[out] PCHAR* The request host start character. NULL on error.
 * @param[out] PCHAR* The request host end character. NULL on error.
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS getRequestHost(PCHAR, PCHAR*, PCHAR*);

/**
 * @brief Compares JSON strings taking into account the type
 *
 * @param[in] PCHAR JSON string being parsed
 * @param[in] jsmntok_t* Jsmn token to match
 * @param[in] jsmntype_t Jsmn token type to match
 * @param[in] PCHAR Token name to match
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API BOOL compareJsonString(PCHAR, jsmntok_t*, jsmntype_t, PCHAR);

/**
 * @brief Converts the timestamp string to time
 *
 * @param[in] PCHAR String to covert (MUST be null terminated)
 * @param[in] UINT64 Current time
 * @param[in,out] PUINT64 Converted time
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS convertTimestampToEpoch(PCHAR, UINT64, PUINT64);

/**
 * @brief Creates a user agent string
 *
 * @param[in] PCHAR User agent name
 * @param[in] PCHAR Custom user agent string
 * @param[in] UINT32 Length of the string
 * @param[out] PCHAR Combined user agent string
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS getUserAgentString(PCHAR, PCHAR, UINT32, PCHAR);

/**
 * @brief Releases the CallInfo allocations
 *
 * @param[in] PCallInfo Call info object to release
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS releaseCallInfo(PCallInfo);


/**
 * Initializes global SSL callbacks
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS initializeSslCallbacks();

/**
 * Releases the global SSL callbacks.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS releaseSslCallbacks();

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_COMMON_INCLUDE__ */
