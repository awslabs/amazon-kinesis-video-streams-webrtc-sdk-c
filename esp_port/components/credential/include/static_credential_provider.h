
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
#ifndef __KINESIS_VIDEO_STATIC_CREDENTIAL_PROVIDER_INCLUDE_I__
#define __KINESIS_VIDEO_STATIC_CREDENTIAL_PROVIDER_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "error.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Creates a Static AWS credential provider object
 *
 * @param[in] PCHAR Access Key Id
 * @param[in] UINT32 Access Key Id Length excluding NULL terminator or 0 to calculate
 * @param[in] PCHAR Secret Key
 * @param[in] UINT32 Secret Key Length excluding NULL terminator or 0 to calculate
 * @param[in, out] PCHAR Session Token
 * @param[in, out] UINT32 Session Token Length excluding NULL terminator or 0 to calculate
 * @param[in] UINT64 Expiration in 100ns absolute time
 * @param[in, out] PAwsCredentialProvider* - OUT - Constructed AWS credentials provider object
 *
 * @return - STATUS code of the execution
 */
STATUS createStaticCredentialProvider(PCHAR, UINT32, PCHAR, UINT32, PCHAR, UINT32, UINT64, PAwsCredentialProvider*);
/**
 * Frees a Static Aws credential provider object
 *
 * @param[in, out] PAwsCredentialProvider* Object to be destroyed.
 *
 * @return - STATUS code of the execution
 */
STATUS freeStaticCredentialProvider(PAwsCredentialProvider*);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_STATIC_CREDENTIAL_PROVIDER_INCLUDE_I__ */
