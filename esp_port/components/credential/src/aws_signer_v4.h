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
#ifndef __KINESIS_VIDEO_AWS_V4_SIGNER_INCLUDE_I__
#define __KINESIS_VIDEO_AWS_V4_SIGNER_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "request_info.h"
#include "inttypes.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
#define AWS_SIG_V4_HEADER_AMZ_DATE           (PCHAR) "X-Amz-Date"
#define AWS_SIG_V4_HEADER_AMZ_SECURITY_TOKEN (PCHAR) "x-amz-security-token"
#define AWS_SIG_V4_HEADER_AUTH               (PCHAR) "Authorization"
#define AWS_SIG_V4_HEADER_HOST               (PCHAR) "host"
#define AWS_SIG_V4_ALGORITHM                 (PCHAR) "AWS4-HMAC-SHA256"
#define AWS_SIG_V4_SIGNATURE_END             (PCHAR) "aws4_request"
#define AWS_SIG_V4_SIGNATURE_START           (PCHAR) "AWS4"
#define AWS_SIG_V4_CONTENT_TYPE_NAME         (PCHAR) "content-type"
#define AWS_SIG_V4_CONTENT_LENGTH_NAME       (PCHAR) "content-length"
#define AWS_SIG_V4_CONTENT_TYPE_VALUE        (PCHAR) "application/json"

// Datetime string length
#define SIGNATURE_DATE_TIME_STRING_LEN 17

// Date only string length
#define SIGNATURE_DATE_STRING_LEN 8

#define MAX_CREDENTIAL_SCOPE_LEN asd

// Hex encoded signature len
#define AWS_SIGV4_SIGNATURE_STRING_LEN (EVP_MAX_MD_SIZE * 2)

// Datetime string format
#define DATE_TIME_STRING_FORMAT (PCHAR) "%Y%m%dT%H%M%SZ"

// Template for the credential scope
#define CREDENTIAL_SCOPE_TEMPLATE "%.*s/%s/%s/%s"

// Template for the URL encoded credentials
#define URL_ENCODED_CREDENTIAL_TEMPLATE "%.*s/%.*s/%s/%s/%s"

// Template for the signed string
#define SIGNED_STRING_TEMPLATE "%s\n%s\n%s\n%s"

// Authentication header template
#define AUTH_HEADER_TEMPLATE "%s Credential=%.*s/%s, SignedHeaders=%.*s, Signature=%s"

// Authentication query template
#define AUTH_QUERY_TEMPLATE "&X-Amz-Algorithm=%s&X-Amz-Credential=%s&X-Amz-Date=%s&X-Amz-Expires=%" PRIu32 "&X-Amz-SignedHeaders=%.*s"

// Token query param template
#define SECURITY_TOKEN_PARAM_TEMPLATE "&X-Amz-Security-Token=%s"

// Authentication query template
#define AUTH_QUERY_TEMPLATE_WITH_TOKEN                                                                                                               \
    "&X-Amz-Algorithm=%s&X-Amz-Credential=%s&X-Amz-Date=%s&X-Amz-Expires=%" PRIu32 "&X-Amz-SignedHeaders=%.*s" SECURITY_TOKEN_PARAM_TEMPLATE

// Signature query param template
#define SIGNATURE_PARAM_TEMPLATE "&X-Amz-Signature=%s"

// Max string length for the request verb - no NULL terminator
#define MAX_REQUEST_VERB_STRING_LEN 4

// Max and min expiration in seconds
#define MAX_AWS_SIGV4_CREDENTIALS_EXPIRATION_IN_SECONDS 604800
#define MIN_AWS_SIGV4_CREDENTIALS_EXPIRATION_IN_SECONDS 1

// Checks whether a canonical header
#define IS_CANONICAL_HEADER_NAME(h)                                                                                                                  \
    ((0 != STRCMP((h), AWS_SIG_V4_HEADER_AMZ_SECURITY_TOKEN)) && (0 != STRCMP((h), AWS_SIG_V4_CONTENT_TYPE_NAME)) &&                                 \
     (0 != STRCMP((h), AWS_SIG_V4_CONTENT_LENGTH_NAME)) && (0 != STRCMP((h), AWS_SIG_V4_HEADER_AUTH)))

// URI-encoded backslash value
#define URI_ENCODED_FORWARD_SLASH "%2F"

#define SHA256_DIGEST_LENGTH 32

#define KVS_MAX_HMAC_SIZE 64

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * @brief Generates AWS SigV4 signature encoded string into the provided buffer.
 *
 * @param[in] - PRequestInfo - IN request info for signing
 * @param[in] - PCHAR - IN - Date/time string to use
 * @param[in] - BOOL - IN - Whether to generate auth headers signing info or query auth info
 * @param[in, out] - PCHAR* - OUT - Newly allocated buffer containing the info. NOTE: Caller should free.
 * @param[in, out] - PUINT32 - OUT - Returns the length of the string
 *
 * @return - STATUS code of the execution
 */
STATUS generateAwsSigV4Signature(PRequestInfo, PCHAR, BOOL, PCHAR*, PINT32);

/**
 * @brief Generates a canonical request string
 *
 * @param - PRequestInfo - IN - Request object
 * @param - PCHAR - OUT/OPT - Canonical request string if specified
 * @param - PUINT32 - IN/OUT - String length in / required out
 *
 * @return - STATUS code of the execution
 */
STATUS generateCanonicalRequestString(PRequestInfo, PCHAR, PUINT32);

/**
 * @brief Generates canonical headers
 *
 * @param - PRequestInfo - IN - Request object
 * @param - PCHAR - OUT/OPT - Canonical headers string if specified
 * @param - PUINT32 - IN/OUT - String length in / required out
 *
 * @return - STATUS code of the execution
 */
STATUS generateCanonicalHeaders(PRequestInfo, PCHAR, PUINT32);

/**
 * @brief Generates signed headers string
 *
 * @param - PRequestInfo - IN - Request object
 * @param - PCHAR - OUT/OPT - Canonical headers string if specified
 * @param - PUINT32 - IN/OUT - String length in / required out
 *
 * @return - STATUS code of the execution
 */
STATUS generateSignedHeaders(PRequestInfo, PCHAR, PUINT32);

/**
 * @brief Generates signature date time
 *
 * @param - UINT64 - IN - Current date
 * @param - PCHAR - OUT - Signature date/time with length of SIGNATURE_DATE_TIME_STRING_LEN characters including NULL terminator
 *
 * @return - STATUS code of the execution
 */
STATUS generateSignatureDateTime(UINT64, PCHAR);

/**
 * @brief Generates credential scope
 *
 * @param - PRequestInfo - IN - Request object
 * @param - PCHAR - IN - Datetime when signed
 * @param - PCHAR - OUT/OPT - Scope string if specified
 * @param - PUINT32 - IN/OUT - String length in / required out
 *
 * @return - STATUS code of the execution
 */
STATUS generateCredentialScope(PRequestInfo, PCHAR, PCHAR, PUINT32);

/**
 * @brief Generates URI Encoded credentials
 *
 * @param - PRequestInfo - IN - Request object
 * @param - PCHAR - IN - Datetime when signed
 * @param - PCHAR - OUT/OPT - Scope string if specified
 * @param - PUINT32 - IN/OUT - String length in / required out
 *
 * @return - STATUS code of the execution
 */
STATUS generateEncodedCredentials(PRequestInfo, PCHAR, PCHAR, PUINT32);

/**
 * @brief Calculates a Hex encoded SHA256 of a message
 *
 * @param - PBYTE - IN - Message to calculate SHA256 for
 * @param - UINT32 - IN - Size of the message in bytes
 * @param - PCHAR - OUT - Hex encoded SHA256 with length of 2 * 32 characters
 *
 * @return - STATUS code of the execution
 */
STATUS hexEncodedSha256(PBYTE, UINT32, PCHAR);

/**
 *
 * @brief Generates HMAC of a message
 *
 * @param - PBYTE - IN - key for HMAC
 * @param - UINT32 - IN - key length
 * @param - PBYTE - IN - message to compute HMAC
 * @param - UINT32 - IN - message length
 * @param - PBYTE - IN - buffer to place the result md
 * @param - PUINT32 - IN/OUT - length of resulting md placed in buffer
 *
 * @return - STATUS code of the execution
 */
STATUS generateRequestHmac(PBYTE, UINT32, PBYTE, UINT32, PBYTE, PUINT32);

/**
 * @brief Gets a canonical URI for the request
 *
 * @param - PCHAR - IN - Request URL
 * @param - UINT32 - IN/OPT - Request len. Specifying 0 will calculate the length.
 * @param - PCHAR* - OUT - The canonical URI start character. NULL on error.
 * @param - PCHAR* - OUT - The canonical URI end character. NULL on error.
 * @param - PBOOL - OUT - Whether the default '/' path is returned.
 *
 * @return - STATUS code of the execution
 */
STATUS getCanonicalUri(PCHAR, UINT32, PCHAR*, PCHAR*, PBOOL);

/**
 * @brief Gets the canonical query params of the URL
 *
 * NOTE: The returned query params string is allocated and should be freed by the caller.
 *
 * @param - PCHAR - IN/OUT Uri to modify
 * @param - UINT32 - IN/OPT - Request len. Specifying 0 will calculate the length.
 * @param - BOOL - IN - Whether to URI encode the param value
 * @param - PCHAR* - OUT - The canonical request params
 * @param - PUINT32 - OUT - The canonical request params len
 *
 * @return - STATUS code of the execution
 */
STATUS getCanonicalQueryParams(PCHAR, UINT32, BOOL, PCHAR*, PUINT32);

/**
 * @brief URI-encode a string
 *
 * @param - PCHAR - IN - String to encode
 * @param - UINT32 - IN - Length of the source. Specifying 0 will calculate the length.
 * @param - PCHAR - IN/OUT - Optional encoded string. Specifying NULL will calculate the length only.
 * @param - PUINT32 - IN/OUT - The length of the encoded string. Calculated in no output is specified
 *
 * @return - STATUS code of the execution
 */
STATUS uriEncodeString(PCHAR, UINT32, PCHAR, PUINT32);
/**
 * @brief URI-decode a string
 *
 * @param - PCHAR - IN - String to decode
 * @param - UINT32 - IN - Length of the source. Specifying 0 will calculate the length.
 * @param - PCHAR - IN/OUT - Optional decoded string. Specifying NULL will calculate the length only.
 * @param - PUINT32 - IN/OUT - The length of the decoded string. Calculated in no output is specified
 *
 * @return - STATUS code of the execution
 */
STATUS uriDecodeString(PCHAR, UINT32, PCHAR, PUINT32);
/**
 * @brief Returns a string representing the specified Verb
 *
 * @param - CURL_VERB - IN - Specified Verb to convert to string representation
 *
 * @return - PCHAR - Representative string or NULL on error
 */
PCHAR getRequestVerbString(HTTP_REQUEST_VERB);
/**
 * @brief Signs a request by appending SigV4 headers
 *
 * @param[in, out] pRequestInfo request info for signing.
 *
 * @return STATUS code of the execution.
 */
STATUS signAwsRequestInfo(PRequestInfo pRequestInfo);
/**
 * @brief Signs a request by appending SigV4 query param
 *
 * @param - PRequestInfo - IN/OUT request info for signing
 *
 * @return - STATUS code of the execution
 */
STATUS signAwsRequestInfoQueryParam(PRequestInfo pRequestInfo);
/**
 * @brief Gets a request host string
 *
 * @param - PCHAR - IN - Request URL
 * @param - PCHAR* - OUT - The request host start character. NULL on error.
 * @param - PCHAR* - OUT - The request host end character. NULL on error.
 *
 * @return - STATUS code of the execution
 */
STATUS getRequestHost(PCHAR, PCHAR*, PCHAR*);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_AWS_V4_SIGNER_INCLUDE_I__ */
