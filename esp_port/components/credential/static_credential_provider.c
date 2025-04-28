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
#define LOG_CLASS "StaticCredentialProvider"
#include "platform_utils.h"
#include "auth.h"
#include "static_credential_provider.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct __StaticCredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Storing the AWS credentials
    PAwsCredentials pAwsCredentials;
} StaticCredentialProvider, *PStaticCredentialProvider;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
STATUS priv_static_credential_provider_get(PAwsCredentialProvider, PAwsCredentials*);

STATUS createStaticCredentialProvider(PCHAR accessKeyId, UINT32 accessKeyIdLen, PCHAR secretKey, UINT32 secretKeyLen, PCHAR sessionToken,
                                         UINT32 sessionTokenLen, UINT64 expiration, PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PAwsCredentials pAwsCredentials = NULL;
    PStaticCredentialProvider pStaticCredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);
    // Create the credentials object

    CHK_STATUS(
        aws_credential_create(accessKeyId, accessKeyIdLen, secretKey, secretKeyLen, sessionToken, sessionTokenLen, expiration, &pAwsCredentials));

    pStaticCredentialProvider = (PStaticCredentialProvider) MEMCALLOC(1, SIZEOF(StaticCredentialProvider));
    CHK(pStaticCredentialProvider != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pStaticCredentialProvider->pAwsCredentials = pAwsCredentials;
    pStaticCredentialProvider->credentialProvider.getCredentialsFn = priv_static_credential_provider_get;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeStaticCredentialProvider((PAwsCredentialProvider*) &pStaticCredentialProvider);
        pStaticCredentialProvider = NULL;
    }

    // Set the return value if it's not NULL
    if (ppCredentialProvider != NULL) {
        *ppCredentialProvider = (PAwsCredentialProvider) pStaticCredentialProvider;
    }

    LEAVES();
    return retStatus;
}

STATUS freeStaticCredentialProvider(PAwsCredentialProvider* ppCredentialProvider)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PStaticCredentialProvider pStaticCredentialProvider = NULL;

    CHK(ppCredentialProvider != NULL, STATUS_NULL_ARG);

    pStaticCredentialProvider = (PStaticCredentialProvider) *ppCredentialProvider;

    // Call is idempotent
    CHK(pStaticCredentialProvider != NULL, retStatus);

    // Release the underlying AWS credentials object
    aws_credential_free(&pStaticCredentialProvider->pAwsCredentials);

    // Release the object
    MEMFREE(pStaticCredentialProvider);

    // Set the pointer to NULL
    *ppCredentialProvider = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS priv_static_credential_provider_get(PAwsCredentialProvider pCredentialProvider, PAwsCredentials* ppAwsCredentials)
{
    ENTERS();

    STATUS retStatus = STATUS_SUCCESS;

    PStaticCredentialProvider pStaticCredentialProvider = (PStaticCredentialProvider) pCredentialProvider;

    CHK(pStaticCredentialProvider != NULL && ppAwsCredentials != NULL, STATUS_NULL_ARG);

    *ppAwsCredentials = pStaticCredentialProvider->pAwsCredentials;

CleanUp:

    LEAVES();
    return retStatus;
}
