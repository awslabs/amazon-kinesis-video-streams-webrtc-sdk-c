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
#define LOG_CLASS "AwsAuth"

#include "auth.h"
#include "platform_utils.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Create credentials object
 */
STATUS aws_credential_create(PCHAR accessKeyId, UINT32 accessKeyIdLen, PCHAR secretKey, UINT32 secretKeyLen, PCHAR sessionToken,
                             UINT32 sessionTokenLen, UINT64 expiration, PAwsCredentials* ppAwsCredentials)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials = NULL;
    UINT32 size;
    PCHAR pCurPtr;

    CHK(ppAwsCredentials != NULL, STATUS_NULL_ARG);

    // Session token is optional. If NULL then the session token len should be 0
    CHK(accessKeyId != NULL && secretKey != NULL && (sessionToken != NULL || sessionTokenLen == 0), STATUS_INVALID_ARG);

    // Calculate the length if not specified
    if (accessKeyIdLen == 0) {
        accessKeyIdLen = (UINT32) STRNLEN(accessKeyId, MAX_AUTH_LEN);
    }

    if (secretKeyLen == 0) {
        secretKeyLen = (UINT32) STRNLEN(secretKey, MAX_AUTH_LEN);
    }

    if (sessionToken != NULL && sessionTokenLen == 0) {
        sessionTokenLen = (UINT32) STRNLEN(sessionToken, MAX_AUTH_LEN);
    }

    // Adding enough space for the NULL terminators
    size = SIZEOF(AwsCredentials) + SIZEOF(CHAR) * (accessKeyIdLen + 1 + secretKeyLen + 1);

    // Add space for the session token if specified
    if (sessionToken != NULL) {
        size += SIZEOF(CHAR) * (sessionTokenLen + 1);
    }

    CHK(size < MAX_AUTH_LEN, STATUS_INVALID_AUTH_LEN);

    // Allocate the entire structure
    pAwsCredentials = (PAwsCredentials) MEMCALLOC(1, size);
    CHK(pAwsCredentials != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pAwsCredentials->version = AWS_CREDENTIALS_CURRENT_VERSION;

    pAwsCredentials->accessKeyIdLen = accessKeyIdLen;
    pAwsCredentials->secretKeyLen = secretKeyLen;
    pAwsCredentials->sessionTokenLen = sessionTokenLen;
    pAwsCredentials->expiration = expiration;

    // Set the overall size of the allocation
    pAwsCredentials->size = size;

    // Set the fields to point to the bottom of the structure
    pCurPtr = (PCHAR)(pAwsCredentials + 1);

    // Set the fields and copy the data forward excluding NULL terminator and then null terminate
    pAwsCredentials->accessKeyId = pCurPtr;
    MEMCPY(pCurPtr, accessKeyId, accessKeyIdLen * SIZEOF(CHAR));
    pCurPtr += accessKeyIdLen;
    *pCurPtr++ = '\0';

    pAwsCredentials->secretKey = pCurPtr;
    MEMCPY(pCurPtr, secretKey, secretKeyLen * SIZEOF(CHAR));
    pCurPtr += secretKeyLen;
    *pCurPtr++ = '\0';

    // Copy session token if exists
    if (sessionToken != NULL) {
        pAwsCredentials->sessionToken = pCurPtr;
        MEMCPY(pCurPtr, (PBYTE) sessionToken, sessionTokenLen * SIZEOF(CHAR));
        pCurPtr += sessionTokenLen;
        *pCurPtr++ = '\0';
    } else {
        pAwsCredentials->sessionToken = NULL;
    }

    // Validate the overall size in case of errors
    CHK((PBYTE) pAwsCredentials + pAwsCredentials->size == (PBYTE) pCurPtr, STATUS_INTERNAL_ERROR);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        aws_credential_free(&pAwsCredentials);
        pAwsCredentials = NULL;
    }

    // Set the return value if it's not NULL
    if (ppAwsCredentials != NULL) {
        *ppAwsCredentials = (PAwsCredentials) pAwsCredentials;
    }

    LEAVES();
    return retStatus;
}

STATUS aws_credential_deserialize(PBYTE token)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials = (PAwsCredentials) token;

    CHK(token != NULL, STATUS_NULL_ARG);
    CHK(pAwsCredentials->accessKeyId != NULL && pAwsCredentials->secretKey != NULL &&
            (pAwsCredentials->sessionToken != NULL || pAwsCredentials->sessionTokenLen == 0),
        STATUS_INVALID_ARG);

    pAwsCredentials->accessKeyId = (PCHAR)(pAwsCredentials + 1);
    pAwsCredentials->secretKey = (PCHAR)(pAwsCredentials->accessKeyId + pAwsCredentials->accessKeyIdLen + 1);
    if (pAwsCredentials->sessionToken != NULL) {
        pAwsCredentials->sessionToken = (PCHAR)(pAwsCredentials->secretKey + pAwsCredentials->secretKeyLen + 1);
    }

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS aws_credential_free(PAwsCredentials* ppAwsCredentials)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials = NULL;

    CHK(ppAwsCredentials != NULL, STATUS_NULL_ARG);

    pAwsCredentials = (PAwsCredentials) *ppAwsCredentials;

    // Call is idempotent
    CHK(pAwsCredentials != NULL, retStatus);

    // Release the object
    MEMFREE(pAwsCredentials);

    // Set the pointer to NULL
    *ppAwsCredentials = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}
