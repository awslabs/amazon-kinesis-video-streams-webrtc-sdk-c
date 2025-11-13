/**
 * Kinesis Video Producer AWS V4 Signer functionality
 */
#define LOG_CLASS "AwsV4Signer"
#include "common_defs.h"
#include "hex.h"
#include "aws_signer_v4.h"
#include "request_info.h"

#include <mbedtls/ssl.h>
#include <mbedtls/sha256.h>
#include <mbedtls/md5.h>
#include <mbedtls/error.h>

/**
 * Generates the AWS signature.
 *
 * IMPORTANT: The length of the buffer passed in is not enforced - it's assumed to have been allocated
 * with enough storage from the previous call with NULL param.
 *
 */

// Re-define the macros from crypto.h for mbedtls to avoid dependency
#define KVS_RSA_F4                  0x10001L
#define KVS_MD5_DIGEST_LENGTH       16
#define KVS_SHA1_DIGEST_LENGTH      20
#define KVS_MD5_DIGEST(m, mlen, ob) mbedtls_md5((m), (mlen), (ob));
#define KVS_HMAC(k, klen, m, mlen, ob, plen)                                                                                                         \
    CHK(0 == mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), (k), (klen), (m), (mlen), (ob)), STATUS_HMAC_GENERATION_ERROR);           \
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256));
#define KVS_SHA1_HMAC(k, klen, m, mlen, ob, plen)                                                                                                    \
    CHK(0 == mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), (k), (klen), (m), (mlen), (ob)), STATUS_HMAC_GENERATION_ERROR);             \
    *(plen) = mbedtls_md_get_size(mbedtls_md_info_from_type(MBEDTLS_MD_SHA1));
#define KVS_SHA256(m, mlen, ob) mbedtls_sha256((m), (mlen), (ob), 0);


STATUS generateAwsSigV4Signature(PRequestInfo pRequestInfo, PCHAR dateTimeStr, BOOL authHeaders, PCHAR* ppSigningInfo, PINT32 pSigningInfoLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 requestLen, scopeLen, signedHeadersLen = 0, scratchLen, hmacSize, hexHmacLen;
    INT32 signedStrLen, curSize;
    PCHAR pScratchBuf = NULL, pCredentialScope = NULL, pUrlEncodedCredentials = NULL, pSignedStr = NULL, pSignedHeaders = NULL;
    CHAR requestHexSha256[2 * SHA256_DIGEST_LENGTH + 1];
    BYTE hmac[KVS_MAX_HMAC_SIZE];
    CHAR hexHmac[KVS_MAX_HMAC_SIZE * 2 + 1];

    CHK(pRequestInfo != NULL && pRequestInfo->pAwsCredentials != NULL && ppSigningInfo != NULL && pSigningInfoLen != NULL, STATUS_NULL_ARG);

    // Set the returns first
    *pSigningInfoLen = 0;
    *ppSigningInfo = NULL;

    // Get the required sizes and select the largest for the scratch buffer
    CHK_STATUS(generateCanonicalRequestString(pRequestInfo, NULL, &requestLen));
    CHK_STATUS(generateCredentialScope(pRequestInfo, dateTimeStr, NULL, &scopeLen));
    CHK_STATUS(generateSignedHeaders(pRequestInfo, NULL, &signedHeadersLen));

    scratchLen = requestLen + 1;

    // Get the request length and allocate enough space and package the request
    CHK(NULL != (pScratchBuf = (PCHAR) MEMALLOC(scratchLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(generateCanonicalRequestString(pRequestInfo, pScratchBuf, &requestLen));

    // Calculate the hex encoded SHA256 of the canonical request
    CHK_STATUS(hexEncodedSha256((PBYTE) pScratchBuf, requestLen * SIZEOF(CHAR), requestHexSha256));
    SAFE_MEMFREE(pScratchBuf);

    // Get the credential scope
    CHK(NULL != (pCredentialScope = (PCHAR) MEMALLOC(scopeLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(generateCredentialScope(pRequestInfo, dateTimeStr, pCredentialScope, &scopeLen));

    // Get the signed headers
    CHK(NULL != (pSignedHeaders = (PCHAR) MEMALLOC(signedHeadersLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(generateSignedHeaders(pRequestInfo, pSignedHeaders, &signedHeadersLen));

    // https://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html
    // StringToSign =
    //    Algorithm + \n +
    //    RequestDateTime + \n +
    //    CredentialScope + \n +
    //    HashedCanonicalRequest
    signedStrLen = (UINT32) STRLEN(AWS_SIG_V4_ALGORITHM) + 1 + SIGNATURE_DATE_TIME_STRING_LEN + 1 + scopeLen + 1 + SIZEOF(requestHexSha256) + 1;
    CHK(NULL != (pSignedStr = (PCHAR) MEMALLOC(signedStrLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    curSize = SNPRINTF(pSignedStr, signedStrLen, SIGNED_STRING_TEMPLATE, AWS_SIG_V4_ALGORITHM, dateTimeStr, pCredentialScope, requestHexSha256);
    CHK(curSize > 0 && curSize < signedStrLen, STATUS_BUFFER_TOO_SMALL);

    // Set the actual size
    signedStrLen = curSize;

    scratchLen = STRLEN(AWS_SIG_V4_SIGNATURE_START) + pRequestInfo->pAwsCredentials->secretKeyLen + 1;
    CHK(NULL != (pScratchBuf = (PCHAR) MEMALLOC(scratchLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);

    // Create V4 signature
    // http://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
    curSize = (INT32) STRLEN(AWS_SIG_V4_SIGNATURE_START);
    MEMCPY(pScratchBuf, AWS_SIG_V4_SIGNATURE_START, curSize);
    MEMCPY((PBYTE) pScratchBuf + curSize, pRequestInfo->pAwsCredentials->secretKey, pRequestInfo->pAwsCredentials->secretKeyLen);
    curSize += pRequestInfo->pAwsCredentials->secretKeyLen;

    hmacSize = SIZEOF(hmac);
    CHK_STATUS(generateRequestHmac((PBYTE) pScratchBuf, curSize, (PBYTE) dateTimeStr, SIGNATURE_DATE_STRING_LEN * SIZEOF(CHAR), hmac, &hmacSize));
    CHK_STATUS(generateRequestHmac(hmac, hmacSize, (PBYTE) pRequestInfo->region, (UINT32) STRLEN(pRequestInfo->region), hmac, &hmacSize));
    CHK_STATUS(generateRequestHmac(hmac, hmacSize, (PBYTE) KINESIS_VIDEO_SERVICE_NAME, (UINT32) STRLEN(KINESIS_VIDEO_SERVICE_NAME), hmac, &hmacSize));
    CHK_STATUS(generateRequestHmac(hmac, hmacSize, (PBYTE) AWS_SIG_V4_SIGNATURE_END, (UINT32) STRLEN(AWS_SIG_V4_SIGNATURE_END), hmac, &hmacSize));
    CHK_STATUS(generateRequestHmac(hmac, hmacSize, (PBYTE) pSignedStr, signedStrLen * SIZEOF(CHAR), hmac, &hmacSize));

    // Increment the curSize to account for the NULL terminator that's required by the hex encoder
    hexHmacLen = ARRAY_SIZE(hexHmac);
    CHK_STATUS(hexEncodeCase(hmac, hmacSize, hexHmac, &hexHmacLen, FALSE));

    SAFE_MEMFREE(pScratchBuf);

    if (authHeaders) {
        // http://docs.aws.amazon.com/general/latest/gr/sigv4-add-signature-to-request.html
        scratchLen = SNPRINTF(NULL, 0, AUTH_HEADER_TEMPLATE, AWS_SIG_V4_ALGORITHM,
                           (int)pRequestInfo->pAwsCredentials->accessKeyIdLen,
                           pRequestInfo->pAwsCredentials->accessKeyId, pCredentialScope,
                           (int) signedHeadersLen, pSignedHeaders, hexHmac) + 1;
        CHK(NULL != (pScratchBuf = (PCHAR) MEMALLOC(scratchLen)), STATUS_NOT_ENOUGH_MEMORY);

        curSize = SNPRINTF(pScratchBuf, scratchLen, AUTH_HEADER_TEMPLATE, AWS_SIG_V4_ALGORITHM,
                           (int)pRequestInfo->pAwsCredentials->accessKeyIdLen,
                           pRequestInfo->pAwsCredentials->accessKeyId, pCredentialScope,
                           (int) signedHeadersLen, pSignedHeaders, hexHmac);
    } else {
        scratchLen = SNPRINTF(NULL, 0, SIGNATURE_PARAM_TEMPLATE, hexHmac) + 1;
        CHK(NULL != (pScratchBuf = (PCHAR) MEMALLOC(scratchLen)), STATUS_NOT_ENOUGH_MEMORY);

        // Create the signature query param
        curSize = SNPRINTF(pScratchBuf, scratchLen, SIGNATURE_PARAM_TEMPLATE, hexHmac);
    }

    CHK(curSize > 0 && curSize < scratchLen, STATUS_BUFFER_TOO_SMALL);

    *pSigningInfoLen = curSize;
    *ppSigningInfo = pScratchBuf;

CleanUp:

    SAFE_MEMFREE(pCredentialScope);
    SAFE_MEMFREE(pUrlEncodedCredentials);
    SAFE_MEMFREE(pSignedStr);
    SAFE_MEMFREE(pSignedHeaders);

    CHK_LOG_ERR(retStatus);
    LEAVES();
    return retStatus;
}

STATUS signAwsRequestInfo(PRequestInfo pRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 len;
    PCHAR pHostStart, pHostEnd, pSignatureInfo = NULL;
    CHAR dateTimeStr[SIGNATURE_DATE_TIME_STRING_LEN];
    CHAR contentLenBuf[16];

    CHK(pRequestInfo != NULL && pRequestInfo->pAwsCredentials != NULL, STATUS_NULL_ARG);
    // Generate the time
    CHK_STATUS(generateSignatureDateTime(pRequestInfo->currentTime, dateTimeStr));
    // Get the host header
    CHK_STATUS(getRequestHost(pRequestInfo->url, &pHostStart, &pHostEnd));
    len = (UINT32)(pHostEnd - pHostStart);

    CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_HEADER_HOST, 0, pHostStart, len));
    CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_HEADER_AMZ_DATE, 0, dateTimeStr, 0));
    CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_CONTENT_TYPE_NAME, 0, AWS_SIG_V4_CONTENT_TYPE_VALUE, 0));

    // Set the content-length
    if (pRequestInfo->body != NULL) {
        CHK_STATUS(ULTOSTR(pRequestInfo->bodySize, contentLenBuf, SIZEOF(contentLenBuf), 10, NULL));
        CHK_STATUS(setRequestHeader(pRequestInfo, (PCHAR) "content-length", 0, contentLenBuf, 0));
    }

    // Generate the signature
    CHK_STATUS(generateAwsSigV4Signature(pRequestInfo, dateTimeStr, TRUE, &pSignatureInfo, &len));

    // Set the header
    CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_HEADER_AUTH, 0, pSignatureInfo, len));

    // Set the security token header if provided
    if (pRequestInfo->pAwsCredentials->sessionTokenLen != 0) {
        CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_HEADER_AMZ_SECURITY_TOKEN, 0, pRequestInfo->pAwsCredentials->sessionToken,
                                      pRequestInfo->pAwsCredentials->sessionTokenLen));
    }

CleanUp:

    SAFE_MEMFREE(pSignatureInfo);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS signAwsRequestInfoQueryParam(PRequestInfo pRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 urlLen, remaining = 0, credsLen, expirationInSeconds, signedHeadersLen = 0, queryLen;
    INT32 len;
    PCHAR pHostStart, pHostEnd, pSignatureInfo = NULL, pEncodedCreds = NULL;
    PCHAR pQueryParams = NULL, pSignedHeaders = NULL, pEndUrl, pUriStart, pQuery;
    CHAR dateTimeStr[SIGNATURE_DATE_TIME_STRING_LEN];
    BOOL defaultPath;

    CHK(pRequestInfo != NULL && pRequestInfo->pAwsCredentials != NULL, STATUS_NULL_ARG);

    // Generate the time
    CHK_STATUS(generateSignatureDateTime(pRequestInfo->currentTime, dateTimeStr));

    // Need to add host header
    CHK_STATUS(getRequestHost(pRequestInfo->url, &pHostStart, &pHostEnd));
    len = (INT32)(pHostEnd - pHostStart);
    CHK_STATUS(setRequestHeader(pRequestInfo, AWS_SIG_V4_HEADER_HOST, 0, pHostStart, len));

    // Encode the credentials scope
    CHK_STATUS(generateEncodedCredentials(pRequestInfo, dateTimeStr, NULL, &credsLen));
    CHK(NULL != (pEncodedCreds = (PCHAR) MEMALLOC(credsLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(generateEncodedCredentials(pRequestInfo, dateTimeStr, pEncodedCreds, &credsLen));

    // Get the signed headers
    CHK_STATUS(generateSignedHeaders(pRequestInfo, NULL, &signedHeadersLen));
    CHK(NULL != (pSignedHeaders = (PCHAR) MEMALLOC(signedHeadersLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(generateSignedHeaders(pRequestInfo, pSignedHeaders, &signedHeadersLen));

    // Set the ptr to end of the url to add query params
    urlLen = (UINT32) STRLEN(pRequestInfo->url);

    // Calculate the expiration in seconds
    expirationInSeconds = MIN(MAX_AWS_SIGV4_CREDENTIALS_EXPIRATION_IN_SECONDS,
                              (UINT32)((pRequestInfo->pAwsCredentials->expiration - pRequestInfo->currentTime) / HUNDREDS_OF_NANOS_IN_A_SECOND));
    expirationInSeconds = MAX(MIN_AWS_SIGV4_CREDENTIALS_EXPIRATION_IN_SECONDS, expirationInSeconds);

#if USE_DYNAMIC_URL // Get the required size for the signedURL
    UINT32 signedUrlLen = 0;
    {
        UINT32 encodedReqLen = 0;

        CHK_STATUS(uriEncodeString(pRequestInfo->url, urlLen, NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;

        CHK_STATUS(uriEncodeString(AWS_SIG_V4_ALGORITHM, strlen(AWS_SIG_V4_ALGORITHM), NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;

        CHK_STATUS(uriEncodeString(pEncodedCreds, strlen(pEncodedCreds), NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;

        CHK_STATUS(uriEncodeString(dateTimeStr, strlen(dateTimeStr), NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;

        // expirationInSeconds
        signedUrlLen += 10; // size required to hold UINT32 max

        CHK_STATUS(uriEncodeString(pSignedHeaders, signedHeadersLen, NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;

        if (pRequestInfo->pAwsCredentials->sessionToken == NULL ||
                pRequestInfo->pAwsCredentials->sessionTokenLen == 0) {
            CHK_STATUS(uriEncodeString(AUTH_QUERY_TEMPLATE, strlen(AUTH_QUERY_TEMPLATE), NULL, &encodedReqLen));
            signedUrlLen += encodedReqLen;
        } else {
            CHK_STATUS(uriEncodeString(AUTH_QUERY_TEMPLATE_WITH_TOKEN, strlen(AUTH_QUERY_TEMPLATE_WITH_TOKEN), NULL, &encodedReqLen));
            signedUrlLen += encodedReqLen;

            CHK_STATUS(uriEncodeString(pRequestInfo->pAwsCredentials->sessionToken,
                    strlen(pRequestInfo->pAwsCredentials->sessionToken), NULL, &encodedReqLen));
            signedUrlLen += encodedReqLen;
        }

        // account for the actual signature
        CHK_STATUS(uriEncodeString(SIGNATURE_PARAM_TEMPLATE,
                   strlen(SIGNATURE_PARAM_TEMPLATE), NULL, &encodedReqLen));
        signedUrlLen += encodedReqLen;
        signedUrlLen += KVS_MAX_HMAC_SIZE; // Hex signature value size;
    }

    // Replace the URL with the larger buffer which could hold signed URL
    pRequestInfo->url = MEMREALLOC(pRequestInfo->url, signedUrlLen + 1);
    CHK(pRequestInfo->url != NULL, STATUS_NOT_ENOUGH_MEMORY);
#else
    UINT32 signedUrlLen = MAX_URI_CHAR_LEN;
#endif

    pEndUrl = pRequestInfo->url + urlLen;
    remaining = signedUrlLen - urlLen;
    // Add the params
    if (pRequestInfo->pAwsCredentials->sessionToken == NULL ||
            pRequestInfo->pAwsCredentials->sessionTokenLen == 0) {
        len = (UINT32) SNPRINTF(pEndUrl, remaining, AUTH_QUERY_TEMPLATE, AWS_SIG_V4_ALGORITHM,
                                pEncodedCreds, dateTimeStr, expirationInSeconds,
                                (int) signedHeadersLen, pSignedHeaders);
    } else {
        len = (UINT32) SNPRINTF(pEndUrl, remaining, AUTH_QUERY_TEMPLATE_WITH_TOKEN,
                                AWS_SIG_V4_ALGORITHM, pEncodedCreds, dateTimeStr,
                                expirationInSeconds, (int) signedHeadersLen, pSignedHeaders,
                                pRequestInfo->pAwsCredentials->sessionToken);
    }

    CHK(len > 0 && len < remaining, STATUS_BUFFER_TOO_SMALL);
    urlLen += len;
    remaining -= len;

    CHK_STATUS(getCanonicalQueryParams(pRequestInfo->url, urlLen, TRUE, &pQueryParams, &queryLen));

    // Reset the query params
    // Set the start of the query params to the end of the canonical uri
    CHK_STATUS(getCanonicalUri(pRequestInfo->url, urlLen, &pUriStart, &pQuery, &defaultPath));

    // Check that we have '?' as the end uri
    CHK(*pQuery == '?', STATUS_INTERNAL_ERROR);

    pQuery++;
    remaining--;

    UINT32 pQueryLen = strlen(pQuery);
    UINT32 extraBytesPQueryParams = queryLen - pQueryLen;

    // Copy the new params
    STRNCPY(pQuery, pQueryParams, remaining + pQueryLen);
    remaining -= extraBytesPQueryParams;

    // Free the new query params for reuse
    SAFE_MEMFREE(pQueryParams);

    // Generate the signature
    CHK_STATUS(generateAwsSigV4Signature(pRequestInfo, dateTimeStr, FALSE, &pSignatureInfo, &len));

    // Add the auth param
    STRNCAT(pRequestInfo->url, pSignatureInfo, remaining);

CleanUp:

    SAFE_MEMFREE(pSignatureInfo);
    SAFE_MEMFREE(pEncodedCreds);
    SAFE_MEMFREE(pSignedHeaders);
    SAFE_MEMFREE(pQueryParams);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS getCanonicalQueryParams(PCHAR pUrl, UINT32 urlLen, BOOL uriEncode, PCHAR* ppQuery, PUINT32 pQueryLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pUriStart, pEndPtr, pQueryParamStart, pQueryParamEnd, pParamValue, pNewParam, pCurPtr, pParam, pQuery = NULL;
    BOOL iterate = TRUE, inserted, firstParam = TRUE, defaultPath;
    PSingleList pSingleList = NULL;
    PSingleListNode pCurNode, pPrevNode;
    UINT32 nameLen, valueLen, maxLen, remaining, queryLen = 0, queryRequiredLen = 0;
    UINT64 item;

    CHK(pUrl != NULL && pQueryLen != NULL && ppQuery != NULL, STATUS_NULL_ARG);

    if (urlLen == 0) {
        urlLen = (UINT32) STRNLEN(pUrl, MAX_URI_CHAR_LEN);
    }

    pEndPtr = pUrl + urlLen;

    // Set the start of the query params to the end of the canonical uri
    CHK_STATUS(getCanonicalUri(pUrl, urlLen, &pUriStart, &pQueryParamStart, &defaultPath));

    // Check if we have any params
    CHK(*pQueryParamStart == '?', retStatus);

    // Skip the '?'
    pQueryParamStart++;

    // Create the single list to hold the sorted params
    CHK_STATUS(singleListCreate(&pSingleList));

    while (iterate) {
        pQueryParamEnd = STRNCHR(pQueryParamStart, (UINT32)(pEndPtr - pQueryParamStart), '&');
        if (pQueryParamEnd == NULL) {
            // break the loop
            iterate = FALSE;

            pQueryParamEnd = pEndPtr;
        }

        // Process the resulting param name and value
        CHK(NULL != (pParamValue = STRNCHR(pQueryParamStart, (UINT32)(pQueryParamEnd - pQueryParamStart), '=')), STATUS_INVALID_ARG);
        nameLen = (UINT32)(pParamValue - pQueryParamStart);

        // Advance param start past '='
        pParamValue++;
        valueLen = (UINT32)(pQueryParamEnd - pParamValue);

        UINT32 requiredSize = 0;
        // Find the size required to encode the value
        if (uriEncode) {
            CHK_STATUS(uriEncodeString(pParamValue, valueLen, NULL, &requiredSize));
        } else {
            requiredSize = valueLen;
        }

        // Max len nameLen + 1 for `=` + requiredSize for value + 1 for NULL
        maxLen = MIN(MAX_URI_CHAR_LEN, nameLen + 1 + requiredSize + 1);
        queryRequiredLen += maxLen; // size required for one `name=value&`

        CHK(NULL != (pNewParam = (PCHAR) MEMALLOC(maxLen * SIZEOF(CHAR))), STATUS_NOT_ENOUGH_MEMORY);

        pCurPtr = pNewParam;

        // Reconstruct the query string
        MEMCPY(pCurPtr, pQueryParamStart, nameLen * SIZEOF(CHAR));
        pCurPtr += nameLen;

        *pCurPtr++ = '=';

        // Calculate the remaining, taking into account '=' and the NULL terminator
        remaining = maxLen - nameLen - 1 - 1;

        // Encode the value
        if (uriEncode) {
            CHK_STATUS(uriEncodeString(pParamValue, valueLen, pCurPtr, &remaining));
        } else {
            STRNCPY(pCurPtr, pParamValue, valueLen);
        }

        // Iterate through the list and insert in an alpha order
        CHK_STATUS(singleListGetHeadNode(pSingleList, &pCurNode));
        inserted = FALSE;
        pPrevNode = NULL;
        while (!inserted && pCurNode != NULL) {
            CHK_STATUS(singleListGetNodeData(pCurNode, &item));
            pParam = (PCHAR) HANDLE_TO_POINTER(item);

            if (STRCMP(pNewParam, pParam) <= 0) {
                if (pPrevNode == NULL) {
                    // Insert at the head
                    CHK_STATUS(singleListInsertItemHead(pSingleList, POINTER_TO_HANDLE(pNewParam)));
                } else {
                    CHK_STATUS(singleListInsertItemAfter(pSingleList, pPrevNode, POINTER_TO_HANDLE(pNewParam)));
                }

                // Early return
                inserted = TRUE;
            }

            pPrevNode = pCurNode;

            CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
        }

        if (!inserted) {
            // If not inserted then add to the tail
            CHK_STATUS(singleListInsertItemTail(pSingleList, POINTER_TO_HANDLE(pNewParam)));
        }

        // Advance the start
        pQueryParamStart = pQueryParamEnd + 1;
    }

    // Now, we can re-create the query params
    remaining = queryRequiredLen;
    CHK(NULL != (pQuery = (PCHAR) MEMALLOC(remaining + 1)), STATUS_NOT_ENOUGH_MEMORY);

    *(pQuery + remaining) = '\0';
    pCurPtr = pQuery;

    CHK_STATUS(singleListGetHeadNode(pSingleList, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pParam = (PCHAR) HANDLE_TO_POINTER(item);

        // Account for '&'
        maxLen = (UINT32) STRLEN(pParam);

        CHK(maxLen + 1 < remaining, STATUS_INVALID_ARG);

        if (firstParam) {
            firstParam = FALSE;
        } else {
            *pCurPtr++ = '&';
        }

        MEMCPY(pCurPtr, pParam, maxLen * SIZEOF(CHAR));
        pCurPtr += maxLen;

        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

    *pCurPtr = '\0';
    queryLen = (UINT32)(pCurPtr - pQuery);

CleanUp:

    if (pSingleList != NULL) {
        singleListClear(pSingleList, TRUE);
        singleListFree(pSingleList);
    }

    if (ppQuery != NULL) {
        *ppQuery = pQuery;
    }

    if (pQueryLen != NULL) {
        *pQueryLen = queryLen;
    }

    LEAVES();
    return retStatus;
}

/**
 * Create a canonical request string for signing
 *
 * Info: http://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
 *
 * @param pRequestInfo
 *
 * @return Status code of the operation
 */
STATUS generateCanonicalRequestString(PRequestInfo pRequestInfo, PCHAR pRequestStr, PUINT32 pRequestLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pCurPtr, pVerbString, pUriStart, pUriEnd, pQueryStart, pQueryEnd;
    UINT32 requestLen = 0, curLen, urlLen, len, itemCount;
    BOOL defaultPath;

    CHK(pRequestInfo != NULL && pRequestLen != NULL, STATUS_NULL_ARG);

    CHK_STATUS(singleListGetNodeCount(pRequestInfo->pRequestHeaders, &itemCount));

    // Calculate the rough max size first including the new lines and hex of the 256 bit hash (2 * 32)
    //    CanonicalRequest =
    //            HTTPRequestMethod + '\n' +
    //            CanonicalURI + '\n' +
    //            CanonicalQueryString + '\n' +
    //            CanonicalHeaders + '\n' +
    //            SignedHeaders + '\n' +
    //            HexEncode(Hash(RequestPayload))

#if 0
    requestLen = MAX_REQUEST_VERB_STRING_LEN + 1 + MAX_URI_CHAR_LEN + 1 + MAX_URI_CHAR_LEN + 1 +
        itemCount * (MAX_REQUEST_HEADER_NAME_LEN + 1 + MAX_REQUEST_HEADER_VALUE_LEN + 1) + itemCount * (MAX_REQUEST_HEADER_NAME_LEN + 1) +
        SHA256_DIGEST_LENGTH * 2 + 1;
#else
    // Calculate the required size dynamically
    // HTTPRequestMethod + '\n'
    pVerbString = getRequestVerbString(pRequestInfo->verb);
    CHK(pVerbString != NULL, STATUS_INVALID_ARG);
    requestLen += (UINT32) STRLEN(pVerbString) + 1;

    // CanonicalURI + '\n'
    urlLen = (UINT32) STRLEN(pRequestInfo->url);
    CHK_STATUS(getCanonicalUri(pRequestInfo->url, urlLen, &pUriStart, &pUriEnd, &defaultPath));
    len = defaultPath ? 1 : (UINT32)(pUriEnd - pUriStart);
    requestLen += len + 1;

    // CanonicalQueryString + '\n'
    pQueryEnd = pRequestInfo->url + urlLen;
    pQueryStart = (pUriEnd == pQueryEnd) ? pUriEnd : pUriEnd + 1;
    len = (UINT32)(pQueryEnd - pQueryStart);
    requestLen += len + 1;

    // CanonicalHeaders + '\n'
    // CHK_STATUS(getCanonicalHeadersSize(pRequestInfo, &len));
    CHK_STATUS(generateCanonicalHeaders(pRequestInfo, NULL, &len));
    requestLen += len + 1;

    // SignedHeaders + '\n'
    // CHK_STATUS(getSignedHeadersSize(pRequestInfo, &len));
    CHK_STATUS(generateSignedHeaders(pRequestInfo, NULL, &len));
    requestLen += len + 1;

    // HexEncode(Hash(RequestPayload))
    requestLen += SHA256_DIGEST_LENGTH * 2;
#endif
    // See if we only are interested in the size
    CHK(pRequestStr != NULL, retStatus);

    pCurPtr = pRequestStr;
    requestLen = *pRequestLen;
    curLen = 0;

    // Get the request verb string
    pVerbString = getRequestVerbString(pRequestInfo->verb);
    CHK(pVerbString != NULL, STATUS_INVALID_ARG);
    len = (UINT32) STRLEN(pVerbString);
    CHK(curLen + len + 1 <= requestLen, STATUS_BUFFER_TOO_SMALL);
    MEMCPY(pCurPtr, pVerbString, SIZEOF(CHAR) * len);
    pCurPtr += len;
    *pCurPtr++ = '\n';
    curLen += len + 1;

    // Store the length of the URL
    urlLen = (UINT32) STRLEN(pRequestInfo->url);

    // Get the canonical URI
    CHK_STATUS(getCanonicalUri(pRequestInfo->url, urlLen, &pUriStart, &pUriEnd, &defaultPath));
    len = defaultPath ? 1 : (UINT32)(pUriEnd - pUriStart);

    CHK(curLen + len + 1 <= requestLen, STATUS_BUFFER_TOO_SMALL);
    MEMCPY(pCurPtr, pUriStart, len * SIZEOF(CHAR));
    pCurPtr += len;
    *pCurPtr++ = '\n';
    curLen += len + 1;

    // Get the canonical query.
    // We assume the params have been URI encoded and the in an ascending order
    pQueryEnd = pRequestInfo->url + urlLen;

    // The start of the query params is either end of the URI or ? so we skip one in that case
    pQueryStart = (pUriEnd == pQueryEnd) ? pUriEnd : pUriEnd + 1;

    len = (UINT32)(pQueryEnd - pQueryStart);
    CHK(curLen + len + 1 <= requestLen, STATUS_BUFFER_TOO_SMALL);
    MEMCPY(pCurPtr, pQueryStart, len * SIZEOF(CHAR));
    pCurPtr += len;
    *pCurPtr++ = '\n';
    curLen += len + 1;

    len = requestLen - curLen;
    CHK_STATUS(generateCanonicalHeaders(pRequestInfo, pCurPtr, &len));
    CHK(curLen + len + 1 <= requestLen, STATUS_BUFFER_TOO_SMALL);
    pCurPtr += len;

    *pCurPtr++ = '\n';
    curLen += len + 1;

    len = requestLen - curLen;
    CHK_STATUS(generateSignedHeaders(pRequestInfo, pCurPtr, &len));
    CHK(curLen + len + 1 <= requestLen, STATUS_BUFFER_TOO_SMALL);
    pCurPtr += len;
    *pCurPtr++ = '\n';
    curLen += len + 1;

    // Generate the hex encoded hash
    len = SHA256_DIGEST_LENGTH * 2;
    CHK(curLen + len <= requestLen, STATUS_BUFFER_TOO_SMALL);
    if (pRequestInfo->body == NULL) {
        // Streaming treats this portion as if the body were empty
        CHK_STATUS(hexEncodedSha256((PBYTE) EMPTY_STRING, 0, pCurPtr));
    } else {
        // standard signing
        CHK_STATUS(hexEncodedSha256((PBYTE) pRequestInfo->body, pRequestInfo->bodySize, pCurPtr));
    }

    pCurPtr += len;
    curLen += len;

    CHK(curLen <= requestLen, STATUS_BUFFER_TOO_SMALL);
    requestLen = curLen;

CleanUp:

    if (pRequestLen != NULL) {
        *pRequestLen = requestLen;
    }

    LEAVES();
    return retStatus;
}

STATUS generateCanonicalHeaders(PRequestInfo pRequestInfo, PCHAR pCanonicalHeaders, PUINT32 pCanonicalHeadersLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 overallLen = 0, valueLen, specifiedLen;
    PSingleListNode pCurNode;
    PRequestHeader pRequestHeader;
    UINT64 item;
    PCHAR pStart, pEnd;
    PCHAR pCurPtr = pCanonicalHeaders;

    CHK(pRequestInfo != NULL && pCanonicalHeadersLen != NULL, STATUS_NULL_ARG);

    specifiedLen = *pCanonicalHeadersLen;

    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));

    // Iterate through the headers
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pRequestHeader = (PRequestHeader) HANDLE_TO_POINTER(item);

        // Process only if we have a canonical header name
        if (IS_CANONICAL_HEADER_NAME(pRequestHeader->pName)) {
            CHK_STATUS(TRIMSTRALL(pRequestHeader->pValue, pRequestHeader->valueLen, &pStart, &pEnd));
            valueLen = (UINT32)(pEnd - pStart);

            // Increase the overall length as we use the lower-case header, colon, trimmed lower-case value and new line
            overallLen += pRequestHeader->nameLen + 1 + valueLen + 1;

            // Pack if we have the destination specified
            if (pCanonicalHeaders != NULL) {
                CHK(overallLen <= specifiedLen, STATUS_BUFFER_TOO_SMALL);

                // Copy over and convert to lower case
                TOLOWERSTR(pRequestHeader->pName, pRequestHeader->nameLen, pCurPtr);
                pCurPtr += pRequestHeader->nameLen;

                // Append the colon
                *pCurPtr++ = ':';

                // Append the trimmed lower-case string
                STRNCPY(pCurPtr, pStart, valueLen);
                pCurPtr += valueLen;

                // Append the new line
                *pCurPtr++ = '\n';
            }
        }

        // Iterate
        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

CleanUp:

    if (pCanonicalHeadersLen != NULL) {
        *pCanonicalHeadersLen = overallLen;
    }

    LEAVES();
    return retStatus;
}

STATUS generateSignedHeaders(PRequestInfo pRequestInfo, PCHAR pSignedHeaders, PUINT32 pSignedHeadersLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 overallLen = 0, specifiedLen;
    PSingleListNode pCurNode;
    PRequestHeader pRequestHeader;
    UINT64 item;
    PCHAR pCurPtr = pSignedHeaders;
    BOOL appended = FALSE;

    CHK(pRequestInfo != NULL && pSignedHeadersLen != NULL, STATUS_NULL_ARG);

    specifiedLen = *pSignedHeadersLen;

    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));

    // Iterate through the headers
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pRequestHeader = (PRequestHeader) HANDLE_TO_POINTER(item);

        // Process only if we have a canonical header name
        if (IS_CANONICAL_HEADER_NAME(pRequestHeader->pName)) {
            // Increase the overall length with the length of the header and a semicolon
            overallLen += pRequestHeader->nameLen;

            // Check if we need to append the semicolon
            if (appended) {
                overallLen++;
            }

            // Pack if we have the destination specified
            if (pSignedHeaders != NULL) {
                CHK(overallLen <= specifiedLen, STATUS_BUFFER_TOO_SMALL);

                // Append the colon if needed
                if (appended) {
                    *pCurPtr++ = ';';
                }

                // Copy over and convert to lower case
                TOLOWERSTR(pRequestHeader->pName, pRequestHeader->nameLen, pCurPtr);
                pCurPtr += pRequestHeader->nameLen;
            }

            appended = TRUE;
        }

        // Iterate
        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

CleanUp:

    if (pSignedHeadersLen != NULL) {
        *pSignedHeadersLen = overallLen;
    }

    LEAVES();
    return retStatus;
}

STATUS generateSignatureDateTime(UINT64 currentTime, PCHAR pDateTimeStr)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    time_t timeT;
    SIZE_T retSize;

    CHK(pDateTimeStr != NULL, STATUS_NULL_ARG);

    // Convert to time_t
    timeT = (time_t)(currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND);
    retSize = STRFTIME(pDateTimeStr, SIGNATURE_DATE_TIME_STRING_LEN, DATE_TIME_STRING_FORMAT, GMTIME(&timeT));
    CHK(retSize > 0, STATUS_BUFFER_TOO_SMALL);
    pDateTimeStr[retSize] = '\0';

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS generateCredentialScope(PRequestInfo pRequestInfo, PCHAR dateTimeStr, PCHAR pScope, PUINT32 pScopeLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 scopeLen = 0;

    CHK(pRequestInfo != NULL && dateTimeStr != NULL && pScopeLen != NULL, STATUS_NULL_ARG);

    // Calculate the max string length with a null terminator at the end
    scopeLen = SIGNATURE_DATE_TIME_STRING_LEN + 1 + MAX_REGION_NAME_LEN + 1 + (UINT32) STRLEN(KINESIS_VIDEO_SERVICE_NAME) + 1 +
        (UINT32) STRLEN(AWS_SIG_V4_SIGNATURE_END) + 1;

    // Early exit on buffer calculation
    CHK(pScope != NULL, retStatus);

    scopeLen = (UINT32) SNPRINTF(pScope, *pScopeLen, CREDENTIAL_SCOPE_TEMPLATE, SIGNATURE_DATE_STRING_LEN, dateTimeStr, pRequestInfo->region,
                                 KINESIS_VIDEO_SERVICE_NAME, AWS_SIG_V4_SIGNATURE_END);
    CHK(scopeLen > 0 && scopeLen <= *pScopeLen, STATUS_BUFFER_TOO_SMALL);

CleanUp:

    if (pScopeLen != NULL) {
        *pScopeLen = scopeLen;
    }

    LEAVES();
    return retStatus;
}

STATUS generateEncodedCredentials(PRequestInfo pRequestInfo, PCHAR dateTimeStr, PCHAR pCreds, PUINT32 pCredsLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 credsLen = 0;

    CHK(pRequestInfo != NULL && dateTimeStr != NULL && pCredsLen != NULL, STATUS_NULL_ARG);

    // Calculate the max string length with '/' and a null terminator at the end
    credsLen = MAX_ACCESS_KEY_LEN + 1 + SIGNATURE_DATE_TIME_STRING_LEN + 1 + MAX_REGION_NAME_LEN +
                1 + (UINT32) STRLEN(KINESIS_VIDEO_SERVICE_NAME) + 1 + (UINT32) STRLEN(AWS_SIG_V4_SIGNATURE_END) + 1;

    // Early exit on buffer calculation
    CHK(pCreds != NULL, retStatus);

    credsLen = (UINT32) SNPRINTF(pCreds, *pCredsLen, URL_ENCODED_CREDENTIAL_TEMPLATE, (int) pRequestInfo->pAwsCredentials->accessKeyIdLen,
                                 pRequestInfo->pAwsCredentials->accessKeyId, SIGNATURE_DATE_STRING_LEN, dateTimeStr,
                                 pRequestInfo->region, KINESIS_VIDEO_SERVICE_NAME, AWS_SIG_V4_SIGNATURE_END);
    CHK(credsLen > 0 && credsLen <= *pCredsLen, STATUS_BUFFER_TOO_SMALL);

CleanUp:

    if (pCredsLen != NULL) {
        *pCredsLen = credsLen;
    }

    LEAVES();
    return retStatus;
}

STATUS getRequestHost(PCHAR pUrl, PCHAR* ppStart, PCHAR* ppEnd)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStart = NULL, pEnd = NULL, pCurPtr;
    UINT32 urlLen;
    BOOL iterate = TRUE;

    CHK(pUrl != NULL && ppStart != NULL && ppEnd != NULL, STATUS_NULL_ARG);

    // We know for sure url is NULL terminated
    urlLen = (UINT32) STRLEN(pUrl);

    // Start from the schema delimiter
    pStart = STRSTR(pUrl, SCHEMA_DELIMITER_STRING);
    CHK(pStart != NULL, STATUS_INVALID_ARG);

    // Advance the pStart past the delimiter
    pStart += STRLEN(SCHEMA_DELIMITER_STRING);

    // Ensure we are not past the string
    CHK(pUrl + urlLen > pStart, STATUS_INVALID_ARG);

    // Set the end first
    pEnd = pUrl + urlLen;

    // Find the delimiter which would indicate end of the host - either one of "/:?"
    pCurPtr = pStart;
    while (iterate && pCurPtr <= pEnd) {
        switch (*pCurPtr) {
            case '/':
            case ':':
            case '?':
                iterate = FALSE;

                // Set the new end value
                pEnd = pCurPtr;
                /* fall-through */
            default:
                pCurPtr++;
        }
    }

CleanUp:

    if (ppStart != NULL) {
        *ppStart = pStart;
    }

    if (ppEnd != NULL) {
        *ppEnd = pEnd;
    }

    return retStatus;
}

STATUS getCanonicalUri(PCHAR pUrl, UINT32 len, PCHAR* ppStart, PCHAR* ppEnd, PBOOL pDefault)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR pStart = NULL, pEnd = NULL, pCur;
    UINT32 urlLen;
    BOOL iterate = TRUE, defaultPath = FALSE;

    CHK(pUrl != NULL && ppStart != NULL && ppEnd != NULL && pDefault != NULL, STATUS_NULL_ARG);

    // We know for sure url is NULL terminated
    urlLen = (len != 0) ? len : (UINT32) STRLEN(pUrl);

    // Start from the schema delimiter
    pCur = STRSTR(pUrl, SCHEMA_DELIMITER_STRING);
    CHK(pCur != NULL, STATUS_INVALID_ARG);

    // Advance the pCur past the delimiter
    pCur += STRLEN(SCHEMA_DELIMITER_STRING);

    // Ensure we are not past the string
    pEnd = pUrl + urlLen;
    CHK(pEnd > pCur, STATUS_INVALID_ARG);

    // Check if we have the host delimiter which is slash or a question mark - whichever is first
    while (iterate && pCur < pEnd && *pCur != '\0') {
        if (*pCur == '?') {
            // This is the case of the empty path with query params
            pEnd = pCur;
            pStart = DEFAULT_CANONICAL_URI_STRING;
            defaultPath = TRUE;
            iterate = FALSE;
        } else if (*pCur == '/') {
            // This is the case of the path which we find
            pStart = pCur;
            pEnd = STRNCHR(pCur, urlLen - (UINT32)(pCur - pUrl), '?');
            iterate = FALSE;
        }

        pCur++;
    }

    if (pEnd == NULL) {
        pEnd = pUrl + urlLen;
    }

CleanUp:

    if (ppStart != NULL) {
        *ppStart = pStart;
    }

    if (ppEnd != NULL) {
        *ppEnd = pEnd;
    }

    if (pDefault != NULL) {
        *pDefault = defaultPath;
    }

    return retStatus;
}

STATUS uriEncodeString(PCHAR pSrc, UINT32 srcLen, PCHAR pDst, PUINT32 pDstLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 encodedLen = 0, strLen, remaining, encLen = ARRAY_SIZE(URI_ENCODED_FORWARD_SLASH) - 1;
    PCHAR pCurPtr = pSrc, pEnc = pDst;
    CHAR ch;
    CHAR alpha[17] = "0123456789ABCDEF";

    // Set the source length to max if not specified
    strLen = (srcLen == 0) ? MAX_UINT32 : srcLen;

    // Set the remaining length
    remaining = (pDst == NULL) ? MAX_UINT32 : *pDstLen;

    CHK(pSrc != NULL && pDstLen != NULL, STATUS_NULL_ARG);

    while (((UINT32)(pCurPtr - pSrc) < strLen) && ((ch = *pCurPtr++) != '\0')) {
        if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '~' || ch == '.') {
            encodedLen++;

            if (pEnc != NULL) {
                CHK(remaining > 0, STATUS_NOT_ENOUGH_MEMORY);
                remaining--;
                *pEnc++ = ch;
            }
        } else if (ch == '/') {
            encodedLen += encLen;

            if (pEnc != NULL) {
                CHK(remaining > encLen, STATUS_NOT_ENOUGH_MEMORY);
                STRNCPY(pEnc, URI_ENCODED_FORWARD_SLASH, remaining);
                pEnc += encLen;
                remaining -= encLen;
            }
        } else {
            encodedLen += encLen;

            if (pEnc != NULL) {
                CHK(remaining > encLen, STATUS_NOT_ENOUGH_MEMORY);
                *pEnc++ = '%';
                *pEnc++ = alpha[ch >> 4];
                *pEnc++ = alpha[ch & 0x0f];
                remaining -= encLen;
            }
        }
    }

    // Account for the null terminator
    encodedLen++;

    if (pEnc != NULL) {
        CHK(remaining > 0, STATUS_NOT_ENOUGH_MEMORY);
        *pEnc++ = '\0';
        remaining--;
    }

CleanUp:

    if (pDstLen != NULL) {
        *pDstLen = encodedLen;
    }

    return retStatus;
}

STATUS uriDecodeString(PCHAR pSrc, UINT32 srcLen, PCHAR pDst, PUINT32 pDstLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 decodedLen = 0, strLen, remaining, size, decLen = ARRAY_SIZE(URI_ENCODED_FORWARD_SLASH) - 1;
    PCHAR pCurPtr = pSrc, pDec = pDst;
    CHAR ch;

    // Set the source length to max if not specified
    strLen = (srcLen == 0) ? MAX_UINT32 : srcLen;

    // Set the remaining length
    remaining = (pDst == NULL) ? MAX_UINT32 : *pDstLen;

    CHK(pSrc != NULL && pDstLen != NULL, STATUS_NULL_ARG);

    while (((UINT32)(pCurPtr - pSrc) < strLen) && ((ch = *pCurPtr) != '\0')) {
        if (ch == '%') {
            CHK((UINT32)(pCurPtr - pSrc) + decLen <= strLen && *(pCurPtr + 1) != '\0' && *(pCurPtr + 2) != '\0', STATUS_INVALID_ARG);
            if (pDec != NULL) {
                size = remaining;
                CHK_STATUS(hexDecode(pCurPtr + 1, 2, (PBYTE) pDec, &size));
                CHK(size == 1, STATUS_INVALID_ARG);
            }

            size = decLen;
        } else {
            if (pDec != NULL) {
                CHK(remaining > 0, STATUS_NOT_ENOUGH_MEMORY);
                *pDec = ch;
            }

            size = 1;
        }

        pCurPtr += size;
        decodedLen++;
        remaining--;

        if (pDec != NULL) {
            pDec++;
        }
    }

    // Account for the null terminator
    decodedLen++;

    if (pDec != NULL) {
        CHK(remaining > 0, STATUS_NOT_ENOUGH_MEMORY);
        *pDec++ = '\0';
        remaining--;
    }

CleanUp:

    if (pDstLen != NULL) {
        *pDstLen = decodedLen;
    }

    return retStatus;
}

PCHAR getRequestVerbString(HTTP_REQUEST_VERB verb)
{
    switch (verb) {
        case HTTP_REQUEST_VERB_PUT:
            return HTTP_REQUEST_VERB_PUT_STRING;
        case HTTP_REQUEST_VERB_GET:
            return HTTP_REQUEST_VERB_GET_STRING;
        case HTTP_REQUEST_VERB_POST:
            return HTTP_REQUEST_VERB_POST_STRING;
    }

    return NULL;
}

STATUS generateRequestHmac(PBYTE key, UINT32 keyLen, PBYTE message, UINT32 messageLen, PBYTE outBuffer, PUINT32 pHmacLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 hmacLen;

    CHK(pHmacLen != NULL, STATUS_NULL_ARG);

    *pHmacLen = 0;

    KVS_HMAC(key, keyLen, message, messageLen, outBuffer, &hmacLen);
    *pHmacLen = hmacLen;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS hexEncodedSha256(PBYTE pMessage, UINT32 size, PCHAR pEncodedHash)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BYTE hashBuf[SHA256_DIGEST_LENGTH];
    UINT32 encodedSize = SHA256_DIGEST_LENGTH * 2 + 1;

    CHK(pMessage != NULL && pEncodedHash != NULL, STATUS_NULL_ARG);

    // Generate the SHA256 of the message first
    KVS_SHA256(pMessage, size, hashBuf);

    // Hex encode lower case
    CHK_STATUS(hexEncodeCase(hashBuf, SHA256_DIGEST_LENGTH, pEncodedHash, &encodedSize, FALSE));

CleanUp:

    LEAVES();
    return retStatus;
}
