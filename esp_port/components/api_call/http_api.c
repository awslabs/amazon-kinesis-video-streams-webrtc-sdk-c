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
#define LOG_CLASS "HttpApi"
#include "http_api.h"
#include "http_helper.h"
#include "netio.h"

/******************************************************************************
 * DEFINITION
 ******************************************************************************/
#define HTTP_API_ENTER() DLOGD("%s(%d) enter", __func__, __LINE__)
#define HTTP_API_EXIT()  DLOGD("%s(%d) exit", __func__, __LINE__)

#define HTTP_API_SECURE_PORT          "443"
#define HTTP_API_CHANNEL_PROTOCOL     "\"WSS\", \"HTTPS\""

#define HTTP_API_CONNECTION_TIMEOUT 10000    // 10 seconds
#define HTTP_API_COMPLETION_TIMEOUT 15000    // 15 seconds
#define HTTP_API_RECV_BUFFER_MAX_SIZE 8192   // 8KB receive buffer
#define HTTP_API_SEND_BUFFER_MAX_SIZE 2048   // 2KB send buffer

// Redefinitions to avoid dependency on channel_info.h
// Max control plane URI char len
#define MAX_CONTROL_PLANE_URI_CHAR_LEN 256

#define HTTP_API_ROLE_ALIASES                   "/role-aliases"
#define HTTP_API_CREDENTIALS                    "/credentials"
#define HTTP_API_IOT_THING_NAME_HEADER          "x-amzn-iot-thingname"

// https://docs.aws.amazon.com/iot/latest/developerguide/authorizing-direct-aws.html
// STATUS http_api_getIotCredential(PSignalingClient pSignalingClient, UINT64 time, PIotCredentialProvider pIotCredentialProvider)
STATUS http_api_getIotCredential(PIotCredentialProvider pIotCredentialProvider)
{
    HTTP_API_ENTER();
    STATUS retStatus = STATUS_SUCCESS;
    // PChannelInfo pChannelInfo = pSignalingClient->pChannelInfo;

    /* Variables for network connection */
    SIZE_T uBytesReceived = 0;

    /* Variables for HTTP request */
    PCHAR pUrl = NULL;
    UINT32 urlLen;

    PRequestInfo pRequestInfo = NULL;
    PCHAR pHttpBody = NULL;
    PCHAR pHost = NULL;
    // rsp
    UINT32 uHttpStatusCode = 0;
    HttpResponseContext* pHttpRspCtx = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;
    // new net io.
    NetIoHandle xNetIoHandle = NULL;
    uint8_t* pHttpSendBuffer = NULL;
    uint8_t* pHttpRecvBuffer = NULL;

    CHK(NULL != (pHost = (PCHAR) MEMCALLOC(MAX_CONTROL_PLANE_URI_CHAR_LEN, 1)), STATUS_HTTP_NOT_ENOUGH_MEMORY);
    urlLen = STRLEN(CONTROL_PLANE_URI_PREFIX) + STRLEN(pIotCredentialProvider->iotGetCredentialEndpoint) + STRLEN(HTTP_API_ROLE_ALIASES) +
        STRLEN("/") + STRLEN(pIotCredentialProvider->roleAlias) + STRLEN(HTTP_API_CREDENTIALS) + 1;
    CHK(NULL != (pUrl = (PCHAR) MEMCALLOC(urlLen, 1)), STATUS_HTTP_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pHttpSendBuffer = (uint8_t*) MEMCALLOC(HTTP_API_SEND_BUFFER_MAX_SIZE, 1)), STATUS_HTTP_NOT_ENOUGH_MEMORY);
    CHK(NULL != (pHttpRecvBuffer = (uint8_t*) MEMCALLOC(HTTP_API_RECV_BUFFER_MAX_SIZE, 1)), STATUS_HTTP_NOT_ENOUGH_MEMORY);

    // Create the API url
    CHK(SNPRINTF(pUrl, urlLen, "%s%s%s%c%s%s", CONTROL_PLANE_URI_PREFIX, pIotCredentialProvider->iotGetCredentialEndpoint, HTTP_API_ROLE_ALIASES, '/',
                 pIotCredentialProvider->roleAlias, HTTP_API_CREDENTIALS) > 0,
        STATUS_HTTP_IOT_FAILED);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(pUrl, pHttpBody, pIotCredentialProvider->awsRegion, pIotCredentialProvider->caCertPath, pIotCredentialProvider->certPath,
                                   pIotCredentialProvider->privateKeyPath, SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, DEFAULT_USER_AGENT_NAME,
                                   HTTP_API_CONNECTION_TIMEOUT, HTTP_API_COMPLETION_TIMEOUT, DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT,
                                   pIotCredentialProvider->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(setRequestHeader(pRequestInfo, HTTP_API_IOT_THING_NAME_HEADER, 0, pIotCredentialProvider->thingName, 0));
    CHK_STATUS(setRequestHeader(pRequestInfo, "accept", 0, "*/*", 0));

    /* Initialize and generate HTTP request, then send it. */
    CHK(NULL != (xNetIoHandle = NetIo_create()), STATUS_HTTP_NOT_ENOUGH_MEMORY);
    CHK_STATUS(NetIo_setRecvTimeout(xNetIoHandle, HTTP_API_COMPLETION_TIMEOUT));
    CHK_STATUS(NetIo_setSendTimeout(xNetIoHandle, HTTP_API_COMPLETION_TIMEOUT));

    CHK_STATUS(http_req_pack(pRequestInfo, HTTP_REQUEST_VERB_GET_STRING, pHost, MAX_CONTROL_PLANE_URI_CHAR_LEN, (PCHAR) pHttpSendBuffer,
                             HTTP_API_SEND_BUFFER_MAX_SIZE, FALSE, FALSE, NULL));

    CHK_STATUS(NetIo_connectWithX509Path(xNetIoHandle, pHost, HTTP_API_SECURE_PORT, pIotCredentialProvider->caCertPath,
                                         pIotCredentialProvider->certPath, pIotCredentialProvider->privateKeyPath));

    CHK(NetIo_send(xNetIoHandle, (unsigned char*) pHttpSendBuffer, STRLEN((PCHAR) pHttpSendBuffer)) == STATUS_SUCCESS, STATUS_NET_SEND_DATA_FAILED);

    CHK_STATUS(NetIo_recv(xNetIoHandle, (unsigned char*) pHttpRecvBuffer, HTTP_API_RECV_BUFFER_MAX_SIZE, &uBytesReceived));

    CHK(uBytesReceived > 0, STATUS_NET_RECV_DATA_FAILED);

    CHK(http_parser_start(&pHttpRspCtx, (CHAR*) pHttpRecvBuffer, (UINT32) uBytesReceived, NULL) == STATUS_SUCCESS, STATUS_HTTP_PARSER_ERROR);
    pResponseStr = http_parser_getHttpBodyLocation(pHttpRspCtx);
    resultLen = http_parser_getHttpBodyLength(pHttpRspCtx);
    uHttpStatusCode = http_parser_getHttpStatusCode(pHttpRspCtx);

    if (uHttpStatusCode != 200) {
        DLOGE("HTTP status code: %d, response: %s", uHttpStatusCode, pResponseStr ? pResponseStr : "NULL");
        retStatus = STATUS_HTTP_IOT_FAILED;
        goto CleanUp;
    }

    CHK_STATUS(http_api_rsp_getIoTCredential(pIotCredentialProvider, (const CHAR*) pResponseStr, resultLen));
    /* We got a success response here. */

CleanUp:
    CHK_LOG_ERR(retStatus);

    if (xNetIoHandle != NULL) {
        NetIo_disconnect(xNetIoHandle);
        NetIo_terminate(xNetIoHandle);
    }

    if (pHttpRspCtx != NULL) {
        if (http_parser_detroy(pHttpRspCtx) != STATUS_SUCCESS) {
            DLOGE("destroying http parser failed. \n");
        }
    }
    SAFE_MEMFREE(pHttpBody);
    SAFE_MEMFREE(pHost);
    SAFE_MEMFREE(pUrl);
    SAFE_MEMFREE(pHttpSendBuffer);
    SAFE_MEMFREE(pHttpRecvBuffer);
    freeRequestInfo(&pRequestInfo);
    HTTP_API_EXIT();
    return retStatus;
}
