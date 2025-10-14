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
#define LOG_CLASS "FileCredentialProvider"

#include "common_defs.h"
#include "platform_utils.h"
#include "auth.h"
#include "file_credential_provider.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct __FileCredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Current time functionality - optional
    GetCurrentTimeFunc getCurrentTimeFn;

    // Custom data supplied to time function
    UINT64 customData;

    // Static Aws Credentials structure with the pointer following the main allocation
    PAwsCredentials pAwsCredentials;

    // Pointer to credential file path
    PCHAR credentialsFilepath[MAX_PATH_LEN + 1];
} FileCredentialProvider, *PFileCredentialProvider;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
static STATUS priv_file_credential_provider_get(PAwsCredentialProvider, PAwsCredentials*);
static STATUS priv_file_credential_provider_read(PFileCredentialProvider);

STATUS file_credential_provider_create(PCHAR pCredentialsFilepath, PAwsCredentialProvider* ppCredentialProvider)
{
    return file_credential_provider_createWithTime(pCredentialsFilepath, commonDefaultGetCurrentTimeFunc, 0, ppCredentialProvider);
}

STATUS file_credential_provider_createWithTime(PCHAR pCredentialsFilepath, GetCurrentTimeFunc getCurrentTimeFn, UINT64 customData,
                                               PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PFileCredentialProvider pFileCredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);
    CHK(pCredentialsFilepath != NULL && pCredentialsFilepath[0] != '\0', STATUS_INVALID_ARG);
    CHK((UINT32) STRLEN(pCredentialsFilepath) <= MAX_PATH_LEN, STATUS_INVALID_ARG);

    pFileCredentialProvider = (PFileCredentialProvider) MEMCALLOC(1, SIZEOF(FileCredentialProvider));
    CHK(pFileCredentialProvider != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pFileCredentialProvider->credentialProvider.getCredentialsFn = priv_file_credential_provider_get;

    // Store the file path in case we need to access it again
    STRNCPY((PCHAR) pFileCredentialProvider->credentialsFilepath, pCredentialsFilepath, MAX_PATH_LEN);

    // Store the time functionality and specify default if NULL
    pFileCredentialProvider->getCurrentTimeFn = (getCurrentTimeFn == NULL) ? commonDefaultGetCurrentTimeFunc : getCurrentTimeFn;
    pFileCredentialProvider->customData = customData;

    // Create the credentials object
    CHK_STATUS(priv_file_credential_provider_read(pFileCredentialProvider));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        file_credential_provider_free((PAwsCredentialProvider*) &pFileCredentialProvider);
        pFileCredentialProvider = NULL;
    }

    // Set the return value if it's not NULL
    if (ppCredentialProvider != NULL) {
        *ppCredentialProvider = (PAwsCredentialProvider) pFileCredentialProvider;
    }

    LEAVES();
    return retStatus;
}

STATUS file_credential_provider_free(PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PFileCredentialProvider pFileCredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pFileCredentialProvider = (PFileCredentialProvider) *ppCredentialProvider;

    // Call is idempotent
    CHK(pFileCredentialProvider != NULL, retStatus);

    // Release the underlying AWS credentials object
    aws_credential_free(&pFileCredentialProvider->pAwsCredentials);

    // Release the object
    MEMFREE(pFileCredentialProvider);

    // Set the pointer to NULL
    *ppCredentialProvider = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS priv_file_credential_provider_get(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;
    PFileCredentialProvider pFileCredentialProvider = (PFileCredentialProvider) pCredentialProvider;

    CHK(pFileCredentialProvider != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    // Fill the credentials
    CHK_STATUS(priv_file_credential_provider_read(pFileCredentialProvider));

    *ppAwsCredentials = pFileCredentialProvider->pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}

/**
 * Read the credential file and sets the values of the AWS credentials object
 *
 * @param - PFileCredentialProvider - the PFileCredentialProvider object
 *
 * @return - STATUS code of the execution
 */

static STATUS priv_file_credential_provider_read(PFileCredentialProvider pFileCredentialProvider)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 fileLen;
    FILE* fp = NULL;
    CHAR credentialMarker[12];
    CHAR thirdTokenStr[MAX(MAX_EXPIRATION_LEN, MAX_SECRET_KEY_LEN) + 1], fourthTokenStr[MAX_SECRET_KEY_LEN + 1], accessKeyId[MAX_ACCESS_KEY_LEN + 1];
    CHAR sessionToken[MAX_SESSION_TOKEN_LEN + 1];
    PCHAR expirationStr = NULL, secretKey = NULL;
    UINT32 accessKeyIdLen = 0, secretKeyLen = 0, sessionTokenLen = 0;
    UINT64 expiration, currentTime;

    CHK(pFileCredentialProvider != NULL && pFileCredentialProvider->credentialsFilepath != NULL, STATUS_NULL_ARG);

    // Refresh the credentials by reading from the credentials file if needed
    currentTime = pFileCredentialProvider->getCurrentTimeFn(pFileCredentialProvider->customData);

    CHK(pFileCredentialProvider->pAwsCredentials == NULL ||
            currentTime + CREDENTIAL_FILE_READ_GRACE_PERIOD > pFileCredentialProvider->pAwsCredentials->expiration,
        retStatus);

    fp = FOPEN((PCHAR) pFileCredentialProvider->credentialsFilepath, "r");

    CHK(fp != NULL, STATUS_FILE_CREDENTIAL_PROVIDER_OPEN_FILE_FAILED);

    // Get the size of the file
    FSEEK(fp, 0, SEEK_END);

    fileLen = (UINT64) FTELL(fp);

    DLOGV("Reading AWS credentials from file: %s, file length = %" PRIu64 ".", pFileCredentialProvider->credentialsFilepath, fileLen);

    CHK(fileLen < MAX_CREDENTIAL_FILE_LEN, STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_LENGTH);

    FSEEK(fp, 0, SEEK_SET);

    // empty buffers
    thirdTokenStr[0] = '\0';
    fourthTokenStr[0] = '\0';
    accessKeyId[0] = '\0';
    sessionToken[0] = '\0';

    /*
     * Currently the credential file can have two formats, one is "CREDENTIALS accessKey expiration secretKey sessionToken", and
     * the other is just "CREDENTIALS accessKey secretKey". So the second token can be either expiration or secret key.
     */

    FSCANF(fp, "%11s %" STR(MAX_ACCESS_KEY_LEN) "s %" STR(MAX_EXPIRATION_LEN) "s %" STR(MAX_SECRET_KEY_LEN) "s %" STR(MAX_SESSION_TOKEN_LEN) "s",
           credentialMarker, accessKeyId, thirdTokenStr, fourthTokenStr, sessionToken);

    // if the fourth token is empty, it means the credential file only has accessKey and secretKey
    if (fourthTokenStr[0] == '\0') {
        secretKey = thirdTokenStr;
    } else {
        expirationStr = thirdTokenStr;
        secretKey = fourthTokenStr;
    }

    CHK(STRCMP(credentialMarker, "CREDENTIALS") == 0, STATUS_FILE_CREDENTIAL_PROVIDER_INVALID_FILE_FORMAT);

    // Set the lengths
    accessKeyIdLen = (UINT32) STRNLEN(accessKeyId, MAX_ACCESS_KEY_LEN);
    secretKeyLen = (UINT32) STRNLEN(secretKey, MAX_SECRET_KEY_LEN);
    sessionTokenLen = (UINT32) STRNLEN(sessionToken, MAX_SESSION_TOKEN_LEN);

    if (expirationStr != NULL) {
        convertTimestampToEpoch(expirationStr, currentTime / HUNDREDS_OF_NANOS_IN_A_SECOND, &expiration);
    } else {
        expiration = currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION;
    }

    // Fix-up the expiration to be no more than max enforced token rotation to avoid extra token rotations
    // as we are caching the returned value which is likely to be an hour but we are enforcing max
    // rotation to be more frequent.
    expiration = MIN(expiration, currentTime + MAX_ENFORCED_TOKEN_EXPIRATION_DURATION);

    CHK(accessKeyIdLen != 0 && secretKeyLen != 0, STATUS_INVALID_AUTH_LEN);

    if (pFileCredentialProvider->pAwsCredentials != NULL) {
        aws_credential_free(&pFileCredentialProvider->pAwsCredentials);
        pFileCredentialProvider->pAwsCredentials = NULL;
    }

    CHK_STATUS(aws_credential_create(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration,
                                     &pFileCredentialProvider->pAwsCredentials));

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
    }

    return retStatus;
}
