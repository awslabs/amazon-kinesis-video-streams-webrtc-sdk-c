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
#ifndef __KINESIS_VIDEO_FILE_CREDENTIAL_PROVIDER_INCLUDE_I__
#define __KINESIS_VIDEO_FILE_CREDENTIAL_PROVIDER_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "time_port.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/**
 * Grace period which is added to the current time to determine whether the extracted credentials are still valid
 */
#define CREDENTIAL_FILE_READ_GRACE_PERIOD                                                                                                            \
    (5 * HUNDREDS_OF_NANOS_IN_A_SECOND + MIN_STREAMING_TOKEN_EXPIRATION_DURATION + STREAMING_TOKEN_EXPIRATION_GRACE_PERIOD)

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Creates a File based AWS credential provider object
 *
 * @param - PCHAR - IN - Credentials file path
 * @param - PAwsCredentialProvider* - OUT - Constructed AWS credentials provider object
 *
 * @return - STATUS code of the execution
 */
STATUS file_credential_provider_create(PCHAR, PAwsCredentialProvider*);
/**
 * Creates a File based AWS credential provider object
 *
 * @param - PCHAR - IN - Credentials file path
 * @param - GetCurrentTimeFunc - IN - Current time function
 * @param - UINT64 - IN - Time function custom data
 * @param - PAwsCredentialProvider* - OUT - Constructed AWS credentials provider object
 *
 * @return - STATUS code of the execution
 */
STATUS file_credential_provider_createWithTime(PCHAR, GetCurrentTimeFunc, UINT64, PAwsCredentialProvider*);
/**
 * Frees a File based Aws credential provider object
 *
 * @param - PAwsCredentialProvider* - IN/OUT - Object to be destroyed.
 *
 * @return - STATUS code of the execution
 */
STATUS file_credential_provider_free(PAwsCredentialProvider*);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_FILE_CREDENTIAL_PROVIDER_INCLUDE_I__ */
