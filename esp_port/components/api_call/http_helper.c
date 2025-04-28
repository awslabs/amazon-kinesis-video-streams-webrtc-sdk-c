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
#define LOG_CLASS "HttpHelper"
#include <llhttp.h>
#include "http_helper.h"
#include "aws_signer_v4.h"
#include "request_info.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct {
    llhttp_t httpParser;
    PVOID customData;
} CustomLlhttp, *PCustomLlhttp;

#define GET_USER_DATA(p) (((PCustomLlhttp) p)->customData)

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
PHttpField http_parser_getValueByField(struct list_head* head, char* field, UINT32 fieldLen)
{
    struct list_head* listptr;
    PHttpField node;
    UINT32 found = 0;

    list_for_each(listptr, head)
    {
        node = list_entry(listptr, HttpField, list);
        if (STRNCMP(node->field, field, node->fieldLen) == 0 && node->fieldLen == fieldLen) {
            found = 1;
            break;
        }
    }
    if (!found) {
        return NULL;
    } else {
        return node;
    }
}

int32_t http_parser_addRequiredHeader(struct list_head* head, char* field, UINT32 fieldLen, char* value, UINT32 valueLen)
{
    PHttpField node = (PHttpField) MEMALLOC(sizeof(HttpField));
    node->field = field;
    node->fieldLen = fieldLen;
    node->value = value;
    node->valueLen = valueLen;
    list_add(&node->list, head);
    return 0;
}

void http_parser_deleteAllHeader(struct list_head* head)
{
    struct list_head* listptr, *tmp;
    PHttpField node;

    list_for_each_safe(listptr, tmp, head)
    {
        node = list_entry(listptr, HttpField, list);
        SAFE_MEMFREE(node);
        node = NULL;
    }
    return;
}

static int _on_header_field(llhttp_t* httpParser, const char* at, size_t length)
{
    HttpResponseContext* pCtx = (HttpResponseContext*) GET_USER_DATA(httpParser);
    pCtx->curField.field = (char*) at;
    pCtx->curField.fieldLen = length;
    return 0;
}

static int _on_header_value(llhttp_t* httpParser, const char* at, size_t length)
{
    HttpResponseContext* pCtx = (HttpResponseContext*) GET_USER_DATA(httpParser);
    pCtx->curField.value = (char*) at;
    pCtx->curField.valueLen = length;
    return 0;
}

static int _on_body(llhttp_t* httpParser, const char* at, size_t length)
{
    HttpResponseContext* pCtx = (HttpResponseContext*) GET_USER_DATA(httpParser);
    pCtx->phttpBodyLoc = (char*) at;
    pCtx->httpBodyLen = length;
    return 0;
}

static int _on_header_value_complete(llhttp_t* httpParser)
{
    PHttpResponseContext pCtx = (PHttpResponseContext) GET_USER_DATA(httpParser);
    if (pCtx->requiredHeader == NULL) {
        return 0;
    }
    PHttpField node = http_parser_getValueByField(pCtx->requiredHeader, pCtx->curField.field, pCtx->curField.fieldLen);
    if (node != NULL) {
        node->value = pCtx->curField.value;
        node->valueLen = pCtx->curField.valueLen;
    } else {
        return -1;
    }

    return 0;
}

UINT32 http_parser_getHttpStatusCode(HttpResponseContext* pHttpRspCtx)
{
    return pHttpRspCtx->httpStatusCode;
}

PCHAR http_parser_getHttpBodyLocation(HttpResponseContext* pHttpRspCtx)
{
    return pHttpRspCtx->phttpBodyLoc;
}

UINT32 http_parser_getHttpBodyLength(HttpResponseContext* pHttpRspCtx)
{
    return pHttpRspCtx->httpBodyLen;
}

STATUS http_parser_start(HttpResponseContext** ppHttpRspCtx, PCHAR pBuf, UINT32 uLen, struct list_head* requiredHeader)
{
    STATUS retStatus = STATUS_SUCCESS;
    CustomLlhttp userParser = {0};
    llhttp_settings_t httpSettings = {
        NULL,                     //_on_message_begin, /* on_message_begin */
        NULL,                     //_on_url, /* on_url */
        NULL,                     /* on_status */
        _on_header_field,         /* on_header_field */
        _on_header_value,         /* on_header_value */
        NULL,                     /* on_headers_complete */
        _on_body,                 /* on_body */
        NULL,                     //_on_message_complete, /* on_message_complete */
        NULL,                     //_on_chunk_header, /* on_chunk_header */
        NULL,                     //_on_chunk_complete, /* on_chunk_complete */
        NULL,                     //_on_url_complete, /* on_url_complete */
        NULL,                     /* on_status_complete */
        NULL,                     /* on_header_field_complete */
        _on_header_value_complete /* on_header_value_complete */
    };
    enum llhttp_errno httpErrno = HPE_OK;

    HttpResponseContext* pCtx = (HttpResponseContext*) MEMALLOC(sizeof(HttpResponseContext));
    if (pCtx == NULL) {
        return -1;
    }
    MEMSET(pCtx, 0, sizeof(HttpResponseContext));
    pCtx->requiredHeader = requiredHeader;
    *ppHttpRspCtx = pCtx;

    llhttp_init((PVOID) &userParser, HTTP_RESPONSE, &httpSettings);
    userParser.customData = pCtx;
    httpErrno = llhttp_execute((void*) &userParser, pBuf, (size_t) uLen);
    // #TBD, need to be fixed.
    if (httpErrno != HPE_OK && httpErrno < HPE_CB_MESSAGE_BEGIN) {
        retStatus = STATUS_NET_RECV_DATA_FAILED;
    } else {
        pCtx->httpStatusCode = (UINT32)(userParser.httpParser.status_code);
        return STATUS_SUCCESS;
    }

    return retStatus;
}

STATUS http_parser_detroy(HttpResponseContext* pHttpRspCtx)
{
    STATUS retStatus = STATUS_SUCCESS;
    if (pHttpRspCtx != NULL && pHttpRspCtx->requiredHeader != NULL) {
        http_parser_deleteAllHeader(pHttpRspCtx->requiredHeader);
        SAFE_MEMFREE(pHttpRspCtx->requiredHeader);
    }
    SAFE_MEMFREE(pHttpRspCtx);
    return retStatus;
}

STATUS http_req_pack(PRequestInfo pRequestInfo, PCHAR pVerb, PCHAR pHost, UINT32 hostLen, PCHAR outputBuf, UINT32 bufLen, BOOL bWss, BOOL bAssign,
                     PCHAR clientKey)
{
    STATUS retStatus = STATUS_SUCCESS;
    PCHAR p = NULL;
    PCHAR pPath = NULL;
    PCHAR pHostStart, pHostEnd;
    PSingleListNode pCurNode;
    UINT64 item;
    PRequestHeader pRequestHeader;
    INT32 len = 0;
    INT32 totalLen = bufLen;
    // sign aws v4 signature.
    if (bAssign == TRUE) {
        // Sign the request
        if (!bWss) {
            CHK_STATUS(signAwsRequestInfo(pRequestInfo));
        } else {
            CHK_STATUS(signAwsRequestInfoQueryParam(pRequestInfo));
        }
    }

    CHK_STATUS(getRequestHost(pRequestInfo->url, &pHostStart, &pHostEnd));
    CHK(pHostEnd == NULL || *pHostEnd == '/' || *pHostEnd == '?', STATUS_INTERNAL_ERROR);
    MEMCPY(pHost, pHostStart, pHostEnd - pHostStart);
    pHost[pHostEnd - pHostStart] = '\0';

    UINT32 pathLen = strlen(pRequestInfo->url); // Need not be more than the length of the URL
    CHK(NULL != (pPath = (PCHAR) MEMCALLOC(pathLen + 1, SIZEOF(CHAR))), STATUS_HTTP_NOT_ENOUGH_MEMORY);

    // Store the pPath
    pPath[pathLen] = '\0';
    if (pHostEnd != NULL) {
        if (*pHostEnd == '/') {
            STRNCPY(pPath, pHostEnd, pathLen);
        } else {
            pPath[0] = '/';
            STRNCPY(&pPath[1], pHostEnd, pathLen - 1);
        }
    } else {
        pPath[0] = '/';
        pPath[1] = '\0';
    }

    if (bAssign == FALSE) {
        CHK_STATUS(setRequestHeader(pRequestInfo, "host", 0, pHost, 0));
    }
    /* Web socket upgrade */
    if (bWss && clientKey != NULL) {
        CHK_STATUS(setRequestHeader(pRequestInfo, "Pragma", 0, "no-cache", 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "Cache-Control", 0, "no-cache", 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "upgrade", 0, "WebSocket", 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "connection", 0, "Upgrade", 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "Sec-WebSocket-Key", 0, clientKey, 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "Sec-WebSocket-Protocol", 0, "wss", 0));
        CHK_STATUS(setRequestHeader(pRequestInfo, "Sec-WebSocket-Version", 0, "13", 0));
    }

    p = (PCHAR)(outputBuf);
    /* header */
    CHK(totalLen > 0, STATUS_HTTP_BUF_OVERFLOW);
    CHK((len = SNPRINTF(p, totalLen, "%s %s HTTP/1.1\r\n", pVerb, pPath)) > 0, STATUS_HTTP_BUF_OVERFLOW);
    totalLen -= len;
    p += len;

    CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));
    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &item));
        pRequestHeader = (PRequestHeader) item;
        CHK(totalLen > 0, STATUS_HTTP_BUF_OVERFLOW);
        CHK((len = SNPRINTF(p, totalLen, "%s: %s\r\n", pRequestHeader->pName, pRequestHeader->pValue)) > 0, STATUS_HTTP_BUF_OVERFLOW);
        totalLen -= len;
        p += len;
        CHK_STATUS(singleListGetNextNode(pCurNode, &pCurNode));
    }

    CHK(totalLen > 0, STATUS_HTTP_BUF_OVERFLOW);
    len = SNPRINTF(p, totalLen, "\r\n");
    CHK(len > 0, STATUS_HTTP_BUF_OVERFLOW);
    totalLen -= len;
    p += len;
    /* body */
    if (pRequestInfo->body != NULL) {
        CHK(totalLen > 0, STATUS_HTTP_BUF_OVERFLOW);
        CHK((len = SNPRINTF(p, totalLen, "%s\r\n", pRequestInfo->body)) > 0, STATUS_HTTP_BUF_OVERFLOW);
        totalLen -= len;
        p += len;
        CHK(totalLen > 0, STATUS_HTTP_BUF_OVERFLOW);
        CHK((len = SNPRINTF(p, totalLen, "\r\n")) > 0, STATUS_HTTP_BUF_OVERFLOW);
        totalLen -= len;
        p += len;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    SAFE_MEMFREE(pPath);
    return retStatus;
}
