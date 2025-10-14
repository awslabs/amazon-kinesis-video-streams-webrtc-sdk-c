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
#ifndef __AWS_KVS_WEBRTC_HTTP_HELPER_INCLUDE__
#define __AWS_KVS_WEBRTC_HTTP_HELPER_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "list.h"
#include "request_info.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct {
    PCHAR field;
    UINT32 fieldLen;
    PCHAR value;
    UINT32 valueLen;
    struct list_head list;
} HttpField, *PHttpField;

typedef struct {
    UINT32 httpStatusCode;
    UINT32 httpBodyLen;
    PCHAR phttpBodyLoc;
    HttpField curField;
    struct list_head* requiredHeader;
} HttpResponseContext, *PHttpResponseContext;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
INT32 http_parser_addRequiredHeader(struct list_head* head, PCHAR field, UINT32 fieldLen, PCHAR value, UINT32 valudLen);
PHttpField http_parser_getValueByField(struct list_head* head, PCHAR field, UINT32 fieldLen);
UINT32 http_parser_getHttpStatusCode(PHttpResponseContext pHttpRspCtx);
PCHAR http_parser_getHttpBodyLocation(PHttpResponseContext pHttpRspCtx);
UINT32 http_parser_getHttpBodyLength(PHttpResponseContext pHttpRspCtx);
STATUS http_parser_start(PHttpResponseContext* ppHttpRspCtx, PCHAR pBuf, UINT32 uLen, struct list_head* requiredHeader);
STATUS http_parser_detroy(PHttpResponseContext pHttpRspCtx);
STATUS http_req_pack(PRequestInfo pRequestInfo, PCHAR pVerb, PCHAR pHost, UINT32 hostLen, PCHAR outputBuf, UINT32 bufLen, BOOL bWss, BOOL bAssign,
                     PCHAR clientKey);
#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_HTTP_HELPER_INCLUDE__ */
