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
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#define LOG_CLASS "HttpApiRsp"
#include "http_api.h"
#include "auth.h"

#define WEBRTC_SCHEME_NAME "webrtc"

#define HTTP_RSP_ENTER() // DLOGD("enter")
#define HTTP_RSP_EXIT()  // DLOGD("exit")

/******************************************************************************
 * FUNCTION
 ******************************************************************************/

STATUS http_api_rsp_getIoTCredential(PIotCredentialProvider pIotCredentialProvider, const CHAR* pResponseStr, UINT32 resultLen)
{
    HTTP_RSP_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    jsmn_parser parser;
    jsmntok_t* pTokens = NULL;

    UINT32 i, accessKeyIdLen = 0, secretKeyLen = 0, sessionTokenLen = 0, expirationTimestampLen = 0;
    INT32 tokenCount;

    PCHAR accessKeyId = NULL, secretKey = NULL, sessionToken = NULL, expirationTimestamp = NULL;
    UINT64 expiration, currentTime;
    CHAR expirationTimestampStr[MAX_EXPIRATION_LEN + 1];

    // CHK(pIotCredentialProvider != NULL && pCallInfo != NULL, STATUS_NULL_ARG);
    CHK(NULL != (pTokens = (jsmntok_t*) MEMALLOC(MAX_JSON_TOKEN_COUNT * SIZEOF(jsmntok_t))), STATUS_NOT_ENOUGH_MEMORY);
    CHK(resultLen > 0, STATUS_HTTP_IOT_FAILED);

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, pTokens, MAX_JSON_TOKEN_COUNT);
    CHK(tokenCount > 1, STATUS_JSON_API_CALL_INVALID_RETURN);
    CHK(pTokens[0].type == JSMN_OBJECT, STATUS_JSON_API_CALL_INVALID_RETURN);

    for (i = 1; i < (UINT32) tokenCount; i++) {
        if (compareJsonString(pResponseStr, &pTokens[i], JSMN_STRING, (PCHAR) "accessKeyId")) {
            accessKeyIdLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(accessKeyIdLen <= MAX_ACCESS_KEY_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            accessKeyId = pResponseStr + pTokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &pTokens[i], JSMN_STRING, (PCHAR) "secretAccessKey")) {
            secretKeyLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(secretKeyLen <= MAX_SECRET_KEY_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            secretKey = pResponseStr + pTokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &pTokens[i], JSMN_STRING, (PCHAR) "sessionToken")) {
            sessionTokenLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(sessionTokenLen <= MAX_SESSION_TOKEN_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            sessionToken = pResponseStr + pTokens[i + 1].start;
            i++;
        } else if (compareJsonString(pResponseStr, &pTokens[i], JSMN_STRING, "expiration")) {
            expirationTimestampLen = (UINT32)(pTokens[i + 1].end - pTokens[i + 1].start);
            CHK(expirationTimestampLen <= MAX_EXPIRATION_LEN, STATUS_JSON_API_CALL_INVALID_RETURN);
            expirationTimestamp = pResponseStr + pTokens[i + 1].start;
            MEMCPY(expirationTimestampStr, expirationTimestamp, expirationTimestampLen);
            expirationTimestampStr[expirationTimestampLen] = '\0';
            i++;
        }
    }

    CHK(accessKeyId != NULL && secretKey != NULL && sessionToken != NULL, STATUS_HTTP_IOT_FAILED);
    //#TBD
    currentTime = pIotCredentialProvider->getCurrentTimeFn(pIotCredentialProvider->customData);
    CHK_STATUS(convertTimestampToEpoch(expirationTimestampStr, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration));
    DLOGD("Iot credential expiration time %" PRIu64, expiration / HUNDREDS_OF_NANOS_IN_A_SECOND);

    if (pIotCredentialProvider->pAwsCredentials != NULL) {
        aws_credential_free(&pIotCredentialProvider->pAwsCredentials);
        pIotCredentialProvider->pAwsCredentials = NULL;
    }

    // Fix-up the expiration to be no more than max enforced token rotation to avoid extra token rotations
    // as we are caching the returned value which is likely to be an hour but we are enforcing max
    // rotation to be more frequent.
    expiration = MIN(expiration, currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION);

    CHK_STATUS(aws_credential_create(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration,
                                     &pIotCredentialProvider->pAwsCredentials));

CleanUp:

    SAFE_MEMFREE(pTokens);
    HTTP_RSP_EXIT();
    return retStatus;
}
