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
#ifndef __AWS_KVS_WEBRTC_BASE64_INCLUDE__
#define __AWS_KVS_WEBRTC_BASE64_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
// #include "common_defs.h"
/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Base64 encode/decode functionality
 */
STATUS base64Encode(PVOID, UINT32, PCHAR, PUINT32);
STATUS base64Decode(PCHAR, UINT32, PBYTE, PUINT32);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_BASE64_INCLUDE__ */
