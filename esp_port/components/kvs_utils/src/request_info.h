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
#ifndef __KINESIS_VIDEO_REQUEST_INFO_INCLUDE_I__
#define __KINESIS_VIDEO_REQUEST_INFO_INCLUDE_I__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"
#include "common.h"
#include "single_linked_list.h"
#include "stack_queue.h"

/**
 * @brief Creates a Request Info object
 *
 * @param[in] PCHAR URL of the request
 * @param[in,opt] PCHAR Body of the request
 * @param[in] PCHAR Region
 * @param[in,opt] PCHAR CA Certificate path/file
 * @param[in,opt] PCHAR SSL Certificate path/file
 * @param [in,opt] PCHAR SSL Certificate private key file path
 * @param[in,opt] SSL_CERTIFICATE_TYPE SSL certificate file type
 * @param[in,opt] PCHAR User agent string
 * @param[in] UINT64 Connection timeout
 * @param[in] UINT64 Completion timeout
 * @param[in,opt] UINT64 Low speed limit
 * @param[in,opt] UINT64 Low speed time limit
 * @param[in,opt] PAwsCredentials Credentials to use for the call
 * @param[in,out] PRequestInfo* The newly created object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createRequestInfo(PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, PCHAR, SSL_CERTIFICATE_TYPE, PCHAR, UINT64, UINT64, UINT64, UINT64,
                                    PAwsCredentials, PRequestInfo*);

/**
 * @brief Frees a Request Info object
 *
 * @param[in,out] PRequestInfo* The object to release
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS freeRequestInfo(PRequestInfo*);

/**
 * @brief Checks whether the request URL requires a secure connection
 *
 * @param[in] PCHAR Request URL
 * @param[out] PBOOL returned value
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS requestRequiresSecureConnection(PCHAR, PBOOL);

/**
 * @brief Sets a header in the request info
 *
 * @param[in] PRequestInfo Request Info object
 * @param[in] PCHAR Header name
 * @param[in,opt] UINT32 Header name length. Calculated if 0
 * @param[in] PCHAR Header value
 * @param[in,opt] UINT32 Header value length. Calculated if 0
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS setRequestHeader(PRequestInfo, PCHAR, UINT32, PCHAR, UINT32);

/**
 * @brief Removes a header from the headers list if exists
 *
 * @param[in] PRequestInfo Request Info object
 * @param[in] PCHAR Header name to check and remove
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS removeRequestHeader(PRequestInfo, PCHAR);

/**
 * @brief Removes and deletes all headers
 *
 * @param[in] PRequestInfo Request Info object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS removeRequestHeaders(PRequestInfo);

/**
 * @brief Creates a request header
 *
 * @param[in] PCHAR Header name
 * @param[in] UINT32 Header name length
 * @param[in] PCHAR Header value
 * @param[in] UINT32 Header value length
 * @param[out] PRequestHeader* Resulting object
 *
 * @return STATUS code of the execution. STATUS_SUCCESS on success
 */
PUBLIC_API STATUS createRequestHeader(PCHAR, UINT32, PCHAR, UINT32, PRequestHeader*);

/**
 * @brief Convenience method to convert HTTP statuses to SERVICE_CALL_RESULT status.
 *
 * @param[in] UINT32 http_status the HTTP status code of the call
 *
 * @return SERVICE_CALL_RESULT The HTTP status translated into a SERVICE_CALL_RESULT value.
 */
PUBLIC_API SERVICE_CALL_RESULT getServiceCallResultFromHttpStatus(UINT32);

#ifdef __cplusplus
}
#endif
#endif /* __KINESIS_VIDEO_REQUEST_INFO_INCLUDE_I__ */
