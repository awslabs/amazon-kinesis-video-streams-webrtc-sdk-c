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
#ifndef __KINESIS_VIDEO_AUTH_INCLUDE_I__
#define __KINESIS_VIDEO_AUTH_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "common.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Creates an AWS credentials object
 *
 * @param - PCHAR - IN - Access Key Id
 * @param - UINT32 - IN - Access Key Id Length excluding NULL terminator or 0 to calculate
 * @param - PCHAR - IN - Secret Key
 * @param - UINT32 - IN - Secret Key Length excluding NULL terminator or 0 to calculate
 * @param - PCHAR - IN/OPT - Session Token
 * @param - UINT32 - IN/OPT - Session Token Length excluding NULL terminator or 0 to calculate
 * @param - UINT64 - IN - Expiration in 100ns absolute time
 * @param - PAwsCredentials* - OUT - Constructed object
 *
 * @return - STATUS code of the execution
 */
STATUS aws_credential_create(PCHAR accessKeyId, UINT32 accessKeyIdLen, PCHAR secretKey, UINT32 secretKeyLen, PCHAR sessionToken,
                             UINT32 sessionTokenLen, UINT64 expiration, PAwsCredentials* ppAwsCredentials);

/**
 * Deserialize an AWS credentials object, adapt the accessKey/secretKey/sessionToken pointer
 * to offset following the AwsCredential structure
 *
 * @param - PBYTE - IN - Token to be deserialized.
 *
 * @return - STATUS code of the execution
 */
STATUS aws_credential_deserialize(PBYTE);
/**
 * Frees an Aws credentials object
 *
 * @param - PAwsCredentials* - IN/OUT - Object to be destroyed.
 *
 * @return - STATUS code of the execution
 */
STATUS aws_credential_free(PAwsCredentials*);
#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_AUTH_INCLUDE_I__ */
