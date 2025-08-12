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
#ifndef __AWS_KVS_WEBRTC_HTTP_API_INCLUDE__
#define __AWS_KVS_WEBRTC_HTTP_API_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "error.h"
#include "common.h"

/**
 * Service call callback functionality
 */
typedef UINT64 (*BlockingServiceCallFunc)(PRequestInfo, PCallInfo);

typedef struct __IotCredentialProvider {
    // First member should be the abstract credential provider
    AwsCredentialProvider credentialProvider;

    // Current time functionality - optional
    GetCurrentTimeFunc getCurrentTimeFn;

    // Custom data supplied to time function
    UINT64 customData;

    // IoT credential endpoint
#if DUP_IOT_GET_CREDENTIAL_EP
    CHAR iotGetCredentialEndpoint[MAX_URI_CHAR_LEN + 1];
#else
    PCHAR iotGetCredentialEndpoint;
#endif

    // IoT certificate file path
    CHAR certPath[MAX_PATH_LEN + 1];

    // IoT private key file path
    CHAR privateKeyPath[MAX_PATH_LEN + 1];

    // CA certificate file path
    CHAR caCertPath[MAX_PATH_LEN + 1];

    // IoT role alias
    CHAR roleAlias[MAX_ROLE_ALIAS_LEN + 1];

    // String name is used as IoT thing-name
    CHAR thingName[MAX_IOT_THING_NAME_LEN + 1];

    // AWS region
    CHAR awsRegion[MAX_AWS_REGION_LEN + 1];

    // Static Aws Credentials structure with the pointer following the main allocation
    PAwsCredentials pAwsCredentials;

    // Service call functionality
    BlockingServiceCallFunc serviceCallFn;
} IotCredentialProvider, *PIotCredentialProvider;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
STATUS http_api_getIotCredential(PIotCredentialProvider pIotCredentialProvider);

STATUS http_api_rsp_getIoTCredential(PIotCredentialProvider pIotCredentialProvider, const CHAR* pResponseStr, UINT32 resultLen);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_HTTP_API_INCLUDE__ */
