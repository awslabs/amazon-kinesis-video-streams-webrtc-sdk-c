/**
 * Implementation of a API calls based on LibWebSocket
 */
#define LOG_CLASS "LwsApiCalls"
#include "../Include_i.h"

static BOOL gInterruptedFlagBySignalHandler;
VOID lwsSignalHandler(INT32 signal)
{
    UNUSED_PARAM(signal);
    gInterruptedFlagBySignalHandler = TRUE;
}

INT32 lwsHttpCallbackRoutine(struct lws* wsi, enum lws_callback_reasons reason, PVOID user, PVOID pDataIn, size_t dataSize)
{
    UNUSED_PARAM(user);
    STATUS retStatus = STATUS_SUCCESS;
    PVOID customData;
    INT32 status, retValue = 0, size;
    PCHAR pCurPtr, pBuffer;
    PBYTE pEndPtr;
    PBYTE* ppStartPtr;
    PLwsCallInfo pLwsCallInfo;
    PRequestInfo pRequestInfo = NULL;
    PSingleListNode pCurNode;
    UINT64 item;
    UINT32 headerCount;
    UINT32 logLevel;
    PRequestHeader pRequestHeader;
    PSignalingClient pSignalingClient = NULL;
    BOOL locked = FALSE;

    DLOGV("HTTPS callback with reason %d", reason);

    // Early check before accessing the custom data field to see if we are interested in processing the message
    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            break;
        default:
            CHK(FALSE, retStatus);
    }

    customData = lws_get_opaque_user_data(wsi);
    pLwsCallInfo = (PLwsCallInfo) customData;

    lws_set_log_level(LLL_NOTICE | LLL_INFO | LLL_WARN | LLL_ERR, NULL);

    CHK(pLwsCallInfo != NULL && pLwsCallInfo->pSignalingClient != NULL && pLwsCallInfo->pSignalingClient->pLwsContext != NULL &&
            pLwsCallInfo->callInfo.pRequestInfo != NULL && pLwsCallInfo->protocolIndex == PROTOCOL_INDEX_HTTPS,
        retStatus);

    // Quick check whether we need to exit
    if (ATOMIC_LOAD(&pLwsCallInfo->cancelService)) {
        retValue = 1;
        ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
        CHK(FALSE, retStatus);
    }

    pSignalingClient = pLwsCallInfo->pSignalingClient;
    pRequestInfo = pLwsCallInfo->callInfo.pRequestInfo;
    pBuffer = pLwsCallInfo->buffer + LWS_PRE;

    logLevel = loggerGetLogLevel();

    MUTEX_LOCK(pSignalingClient->lwsServiceLock);
    locked = TRUE;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            pCurPtr = pDataIn == NULL ? "(None)" : (PCHAR) pDataIn;
            DLOGW("Client connection failed. Connection error string: %s", pCurPtr);
            STRNCPY(pLwsCallInfo->callInfo.errorBuffer, pCurPtr, CALL_INFO_ERROR_BUFFER_LEN);

            // TODO: Attempt to get more meaningful service return code

            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
            ATOMIC_STORE(&pLwsCallInfo->pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);

            break;

        case LWS_CALLBACK_CLOSED_CLIENT_HTTP:
            DLOGD("Client http closed");
            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);

            break;

        case LWS_CALLBACK_ESTABLISHED_CLIENT_HTTP:
            status = lws_http_client_http_response(wsi);
            DLOGD("Connected with server response: %d", status);

            pLwsCallInfo->callInfo.callResult = getServiceCallResultFromHttpStatus((UINT32) status);

            // Store the Request ID header
            if ((size = lws_hdr_custom_copy(wsi, pBuffer, LWS_SCRATCH_BUFFER_SIZE, SIGNALING_REQUEST_ID_HEADER_NAME,
                                            (SIZEOF(SIGNALING_REQUEST_ID_HEADER_NAME) - 1) * SIZEOF(CHAR))) > 0) {
                pBuffer[size] = '\0';
                DLOGI("Request ID: %s", pBuffer);
            }

            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            DLOGD("Received client http read: %d bytes", (INT32) dataSize);

            if (logLevel > LOG_LEVEL_INFO && logLevel < LOG_LEVEL_SILENT) {
                lwsl_hexdump_notice(pDataIn, dataSize);
            }

            if (dataSize != 0) {
                CHK(NULL != (pLwsCallInfo->callInfo.responseData = (PCHAR) MEMALLOC(dataSize)), STATUS_NOT_ENOUGH_MEMORY);
                MEMCPY(pLwsCallInfo->callInfo.responseData, pDataIn, dataSize);
                pLwsCallInfo->callInfo.responseDataLen = (UINT32) dataSize;
            }

            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP:
            DLOGD("Received client http");
            size = LWS_SCRATCH_BUFFER_SIZE;

            if (lws_http_client_read(wsi, &pBuffer, &size) < 0) {
                retValue = -1;
            }

            break;

        case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
            DLOGD("Http client completed");
            break;

        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            DLOGD("Client append handshake header\n");

            CHK_STATUS(singleListGetNodeCount(pRequestInfo->pRequestHeaders, &headerCount));
            ppStartPtr = (PBYTE*) pDataIn;
            pEndPtr = *ppStartPtr + dataSize - 1;

            // Iterate through the headers
            while (headerCount != 0) {
                CHK_STATUS(singleListGetHeadNode(pRequestInfo->pRequestHeaders, &pCurNode));
                CHK_STATUS(singleListGetNodeData(pCurNode, &item));

                pRequestHeader = (PRequestHeader) item;

                // Append the colon at the end of the name
                if (pRequestHeader->pName[pRequestHeader->nameLen - 1] != ':') {
                    STRCPY(pBuffer, pRequestHeader->pName);
                    pBuffer[pRequestHeader->nameLen] = ':';
                    pBuffer[pRequestHeader->nameLen + 1] = '\0';
                    pRequestHeader->pName = pBuffer;
                    pRequestHeader->nameLen++;
                }

                DLOGV("Appending header - %s %s", pRequestHeader->pName, pRequestHeader->pValue);

                status = lws_add_http_header_by_name(wsi, (PBYTE) pRequestHeader->pName, (PBYTE) pRequestHeader->pValue, pRequestHeader->valueLen,
                                                     ppStartPtr, pEndPtr);
                if (status != 0) {
                    retValue = 1;
                    CHK(FALSE, retStatus);
                }

                // Remove the head
                CHK_STATUS(singleListDeleteHead(pRequestInfo->pRequestHeaders));
                MEMFREE(pRequestHeader);

                // Decrement to iterate
                headerCount--;
            }

            lws_client_http_body_pending(wsi, 1);
            lws_callback_on_writable(wsi);

            break;

        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            DLOGD("Sending the body %.*s, size %d", pRequestInfo->bodySize, pRequestInfo->body, pRequestInfo->bodySize);
            MEMCPY(pBuffer, pRequestInfo->body, pRequestInfo->bodySize);

            size = lws_write(wsi, (PBYTE) pBuffer, (SIZE_T) pRequestInfo->bodySize, LWS_WRITE_TEXT);

            if (size != (INT32) pRequestInfo->bodySize) {
                DLOGW("Failed to write out the body of POST request entirely. Expected to write %d, wrote %d", pRequestInfo->bodySize, size);
                if (size > 0) {
                    // Schedule again
                    lws_client_http_body_pending(wsi, 1);
                    lws_callback_on_writable(wsi);
                } else {
                    // Quit
                    retValue = 1;
                }
            } else {
                lws_client_http_body_pending(wsi, 0);
            }

            break;

        default:
            break;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGW("Failed in HTTPS handling routine with 0x%08x", retStatus);
        if (pRequestInfo != NULL) {
            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
        }

        lws_cancel_service(lws_get_context(wsi));

        retValue = -1;
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);
    }

    return retValue;
}

INT32 lwsWssCallbackRoutine(struct lws* wsi, enum lws_callback_reasons reason, PVOID user, PVOID pDataIn, size_t dataSize)
{
    UNUSED_PARAM(user);
    STATUS retStatus = STATUS_SUCCESS;
    PVOID customData;
    INT32 status, size, writeSize, retValue = 0;
    PCHAR pCurPtr;
    PLwsCallInfo pLwsCallInfo;
    PRequestInfo pRequestInfo = NULL;
    PSignalingClient pSignalingClient = NULL;
    SIZE_T offset, bufferSize;
    BOOL connected, locked = FALSE;

    DLOGV("WSS callback with reason %d", reason);

    // Early check before accessing custom field to see if we are interested in the message
    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        case LWS_CALLBACK_CLIENT_ESTABLISHED:
        case LWS_CALLBACK_CLIENT_CLOSED:
        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
        case LWS_CALLBACK_CLIENT_RECEIVE:
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            break;
        default:
            CHK(FALSE, retStatus);
    }

    customData = lws_get_opaque_user_data(wsi);
    pLwsCallInfo = (PLwsCallInfo) customData;

    lws_set_log_level(LLL_NOTICE | LLL_INFO | LLL_WARN | LLL_ERR, NULL);

    CHK(pLwsCallInfo != NULL && pLwsCallInfo->pSignalingClient != NULL && pLwsCallInfo->pSignalingClient->pOngoingCallInfo != NULL &&
            pLwsCallInfo->pSignalingClient->pLwsContext != NULL && pLwsCallInfo->pSignalingClient->pOngoingCallInfo->callInfo.pRequestInfo != NULL &&
            pLwsCallInfo->protocolIndex == PROTOCOL_INDEX_WSS,
        retStatus);
    pSignalingClient = pLwsCallInfo->pSignalingClient;
    pLwsCallInfo = pSignalingClient->pOngoingCallInfo;
    pRequestInfo = pLwsCallInfo->callInfo.pRequestInfo;

    // Quick check whether we need to exit
    if (ATOMIC_LOAD(&pLwsCallInfo->cancelService)) {
        retValue = 1;
        ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
        CHK(FALSE, retStatus);
    }

    MUTEX_LOCK(pSignalingClient->lwsServiceLock);
    locked = TRUE;

    switch (reason) {
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            pCurPtr = pDataIn == NULL ? "(None)" : (PCHAR) pDataIn;
            DLOGW("Client connection failed. Connection error string: %s", pCurPtr);
            STRNCPY(pLwsCallInfo->callInfo.errorBuffer, pCurPtr, CALL_INFO_ERROR_BUFFER_LEN);

            // TODO: Attempt to get more meaningful service return code

            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
            connected = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->connected, FALSE);

            CVAR_BROADCAST(pSignalingClient->receiveCvar);
            CVAR_BROADCAST(pSignalingClient->sendCvar);
            ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_UNKNOWN);
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);

            if (connected && !ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
                // Handle re-connection in a reconnect handler thread
                CHK_STATUS(THREAD_CREATE(&pSignalingClient->reconnecterTracker.threadId, reconnectHandler, (PVOID) pSignalingClient));
                CHK_STATUS(THREAD_DETACH(pSignalingClient->reconnecterTracker.threadId));
            }

            break;

        case LWS_CALLBACK_CLIENT_ESTABLISHED:
            DLOGD("Connection established");

            // Set the call result to succeeded
            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
            ATOMIC_STORE_BOOL(&pSignalingClient->connected, TRUE);

            // Store the time when we connect for diagnostics
            MUTEX_LOCK(pSignalingClient->diagnosticsLock);
            pSignalingClient->diagnostics.connectTime = GETTIME();
            MUTEX_UNLOCK(pSignalingClient->diagnosticsLock);

            // Notify the listener thread
            CVAR_BROADCAST(pSignalingClient->connectedCvar);

            break;

        case LWS_CALLBACK_CLIENT_CLOSED:
            DLOGD("Client WSS closed");

            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
            connected = ATOMIC_EXCHANGE_BOOL(&pSignalingClient->connected, FALSE);

            CVAR_BROADCAST(pSignalingClient->receiveCvar);
            CVAR_BROADCAST(pSignalingClient->sendCvar);
            ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_UNKNOWN);

            if (connected && ATOMIC_LOAD(&pSignalingClient->result) != SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE &&
                !ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown)) {
                // Set the result failed
                ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);

                // Handle re-connection in a reconnect handler thread
                CHK_STATUS(THREAD_CREATE(&pSignalingClient->reconnecterTracker.threadId, reconnectHandler, (PVOID) pSignalingClient));
                CHK_STATUS(THREAD_DETACH(pSignalingClient->reconnecterTracker.threadId));
            }

            break;

        case LWS_CALLBACK_WS_PEER_INITIATED_CLOSE:
            status = 0;
            pCurPtr = NULL;
            size = (UINT32) dataSize;
            if (dataSize > SIZEOF(UINT16)) {
                // The status should be the first two bytes in network order
                status = getInt16(*(PINT16) pDataIn);

                // Set the string past the status
                pCurPtr = (PCHAR)((PBYTE) pDataIn + SIZEOF(UINT16));
                size -= SIZEOF(UINT16);
            }

            DLOGD("Peer initiated close with %d (0x%08x). Message: %.*s", status, (UINT32) status, size, pCurPtr);

            // Store the state as the result
            retValue = -1;

            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) status);

            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:
            DLOGD("Client receive %.*s", dataSize, (PCHAR) pDataIn);

            // Check if it's a binary data
            CHK(!lws_frame_is_binary(wsi), STATUS_SIGNALING_RECEIVE_BINARY_DATA_NOT_SUPPORTED);

            // Skip if it's the first and last fragment and the size is 0
            CHK(!(lws_is_first_fragment(wsi) && lws_is_final_fragment(wsi) && dataSize == 0), retStatus);

            // Check what type of a message it is. We will set the size to 0 on first and flush on last
            if (lws_is_first_fragment(wsi)) {
                pLwsCallInfo->receiveBufferSize = 0;
            }

            // Store the data in the buffer
            CHK(pLwsCallInfo->receiveBufferSize + (UINT32) dataSize + LWS_PRE <= SIZEOF(pLwsCallInfo->receiveBuffer),
                STATUS_SIGNALING_RECEIVED_MESSAGE_LARGER_THAN_MAX_DATA_LEN);
            MEMCPY(&pLwsCallInfo->receiveBuffer[LWS_PRE + pLwsCallInfo->receiveBufferSize], pDataIn, dataSize);
            pLwsCallInfo->receiveBufferSize += (UINT32) dataSize;

            // Flush on last
            if (lws_is_final_fragment(wsi)) {
                CHK_STATUS(receiveLwsMessage(pLwsCallInfo->pSignalingClient, (PCHAR) &pLwsCallInfo->receiveBuffer[LWS_PRE],
                                             pLwsCallInfo->receiveBufferSize / SIZEOF(CHAR)));
            }

            lws_callback_on_writable(wsi);

            break;

        case LWS_CALLBACK_CLIENT_WRITEABLE:
            DLOGD("Client is writable");

            // Check if we are attempting to terminate the connection
            if (!ATOMIC_LOAD_BOOL(&pSignalingClient->connected) && ATOMIC_LOAD(&pSignalingClient->messageResult) == SERVICE_CALL_UNKNOWN) {
                retValue = 1;
                CHK(FALSE, retStatus);
            }

            offset = (UINT32) ATOMIC_LOAD(&pLwsCallInfo->sendOffset);
            bufferSize = (UINT32) ATOMIC_LOAD(&pLwsCallInfo->sendBufferSize);
            writeSize = (INT32)(bufferSize - offset);

            // Check if we need to do anything
            CHK(writeSize > 0, retStatus);

            // Send data and notify on completion
            size = lws_write(wsi, &(pLwsCallInfo->sendBuffer[pLwsCallInfo->sendOffset]), (SIZE_T) writeSize, LWS_WRITE_TEXT);

            if (size < 0) {
                DLOGW("Write failed. Returned write size is %d", size);
                // Quit
                retValue = -1;
                CHK(FALSE, retStatus);
            }

            if (size == writeSize) {
                // Notify the listener
                ATOMIC_STORE(&pLwsCallInfo->sendOffset, 0);
                ATOMIC_STORE(&pLwsCallInfo->sendBufferSize, 0);
                CVAR_BROADCAST(pLwsCallInfo->pSignalingClient->sendCvar);
            } else {
                // Partial write
                DLOGV("Failed to write out the data entirely. Wrote %d out of %d", size, writeSize);
                // Schedule again
                lws_callback_on_writable(wsi);
            }

            break;

        default:
            break;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGW("Failed in LWS handling routine with 0x%08x", retStatus);
        if (pRequestInfo != NULL) {
            ATOMIC_STORE_BOOL(&pRequestInfo->terminating, TRUE);
        }

        lws_cancel_service(lws_get_context(wsi));

        retValue = -1;
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);
    }

    return retValue;
}

STATUS lwsCompleteSync(PLwsCallInfo pCallInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    volatile INT32 retVal = 0;
    PCHAR pHostStart, pHostEnd, pVerb;
    struct lws_client_connect_info connectInfo;
    struct lws* clientLws;
    struct lws_context* pContext;
    BOOL secureConnection, locked = FALSE;
    CHAR path[MAX_URI_CHAR_LEN + 1];

    CHK(pCallInfo != NULL && pCallInfo->callInfo.pRequestInfo != NULL && pCallInfo->pSignalingClient != NULL, STATUS_NULL_ARG);

    CHK_STATUS(requestRequiresSecureConnection(pCallInfo->callInfo.pRequestInfo->url, &secureConnection));
    DLOGV("Perform %s synchronous call for URL: %s", secureConnection ? "secure" : EMPTY_STRING, pCallInfo->callInfo.pRequestInfo->url);

    if (pCallInfo->protocolIndex == PROTOCOL_INDEX_WSS) {
        pVerb = NULL;

        // Remove the header as it will be added back by LWS
        CHK_STATUS(removeRequestHeader(pCallInfo->callInfo.pRequestInfo, (PCHAR) "user-agent"));

        // Sign the request
        CHK_STATUS(signAwsRequestInfoQueryParam(pCallInfo->callInfo.pRequestInfo));

        // Remove the headers
        CHK_STATUS(removeRequestHeaders(pCallInfo->callInfo.pRequestInfo));
    } else {
        pVerb = HTTP_REQUEST_VERB_POST_STRING;

        // Sign the request
        CHK_STATUS(signAwsRequestInfo(pCallInfo->callInfo.pRequestInfo));

        // Remove the header as it will be added back by LWS
        CHK_STATUS(removeRequestHeader(pCallInfo->callInfo.pRequestInfo, AWS_SIG_V4_HEADER_HOST));
    }

    pContext = pCallInfo->pSignalingClient->pLwsContext;

    // Execute the LWS REST call
    MEMSET(&connectInfo, 0x00, SIZEOF(struct lws_client_connect_info));
    connectInfo.context = pContext;
    connectInfo.ssl_connection = LCCSCF_USE_SSL;
    connectInfo.port = SIGNALING_DEFAULT_SSL_PORT;

    CHK_STATUS(getRequestHost(pCallInfo->callInfo.pRequestInfo->url, &pHostStart, &pHostEnd));
    CHK(pHostEnd == NULL || *pHostEnd == '/' || *pHostEnd == '?', STATUS_INTERNAL_ERROR);

    // Store the path
    path[MAX_URI_CHAR_LEN] = '\0';
    if (pHostEnd != NULL) {
        if (*pHostEnd == '/') {
            STRNCPY(path, pHostEnd, MAX_URI_CHAR_LEN);
        } else {
            path[0] = '/';
            STRNCPY(&path[1], pHostEnd, MAX_URI_CHAR_LEN - 1);
        }
    } else {
        path[0] = '/';
        path[1] = '\0';
    }

    // NULL terminate the host
    *pHostEnd = '\0';

    connectInfo.address = pHostStart;
    connectInfo.path = path;
    connectInfo.host = connectInfo.address;
    connectInfo.method = pVerb;
    connectInfo.protocol = pCallInfo->pSignalingClient->signalingProtocols[pCallInfo->protocolIndex].name;
    connectInfo.pwsi = &clientLws;

    connectInfo.opaque_user_data = pCallInfo;

    MUTEX_LOCK(pCallInfo->pSignalingClient->lwsServiceLock);
    locked = TRUE;
    CHK(NULL != lws_client_connect_via_info(&connectInfo), STATUS_SIGNALING_LWS_CLIENT_CONNECT_FAILED);
    MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsServiceLock);
    locked = FALSE;

    while (retVal >= 0 && !gInterruptedFlagBySignalHandler && pCallInfo->callInfo.pRequestInfo != NULL &&
           !ATOMIC_LOAD_BOOL(&pCallInfo->callInfo.pRequestInfo->terminating)) {
        if (!MUTEX_TRYLOCK(pCallInfo->pSignalingClient->lwsSerializerLock)) {
            THREAD_SLEEP(LWS_SERVICE_LOOP_ITERATION_WAIT);
        } else {
            retVal = lws_service(pContext, 0);
            MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsSerializerLock);
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsServiceLock);
    }

    LEAVES();
    return retStatus;
}

//////////////////////////////////////////////////////////////////////////
// API calls
//////////////////////////////////////////////////////////////////////////
STATUS describeChannelLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    PCHAR pResponseStr;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 i, strLen, resultLen;
    UINT32 tokenCount;
    UINT64 messageTtl;
    BOOL jsonInChannelDescription = FALSE, jsonInMvConfiguration = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, DESCRIBE_SIGNALING_CHANNEL_API_POSTFIX);

    // Prepare the json params for the call
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), DESCRIBE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(&pSignalingClient->channelDescription, 0x00, SIZEOF(SignalingChannelDescription));

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (!jsonInChannelDescription) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelInfo")) {
                pSignalingClient->channelDescription.version = SIGNALING_CHANNEL_DESCRIPTION_CURRENT_VERSION;
                jsonInChannelDescription = TRUE;
                i++;
            }
        } else {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelARN")) {
                strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelName")) {
                strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_CHANNEL_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelName, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelName[MAX_CHANNEL_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Version")) {
                strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_UPDATE_VERSION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.updateVersion, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.updateVersion[MAX_UPDATE_VERSION_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelStatus")) {
                strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_DESCRIBE_CHANNEL_STATUS_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                pSignalingClient->channelDescription.channelStatus = getChannelStatusFromString(pResponseStr + tokens[i + 1].start, strLen);
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelType")) {
                strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_DESCRIBE_CHANNEL_TYPE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                pSignalingClient->channelDescription.channelType = getChannelTypeFromString(pResponseStr + tokens[i + 1].start, strLen);
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "CreationTime")) {
                // TODO: In the future parse out the creation time but currently we don't need it
                i++;
            } else {
                if (!jsonInMvConfiguration) {
                    if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "SingleMasterConfiguration")) {
                        jsonInMvConfiguration = TRUE;
                        i++;
                    }
                } else {
                    if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "MessageTtlSeconds")) {
                        CHK_STATUS(STRTOUI64(pResponseStr + tokens[i + 1].start, pResponseStr + tokens[i + 1].end, 10, &messageTtl));

                        // NOTE: Ttl value is in seconds
                        pSignalingClient->channelDescription.messageTtl = messageTtl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                        i++;
                    }
                }
            }
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelStatus != SIGNALING_CHANNEL_STATUS_DELETING, STATUS_SIGNALING_CHANNEL_BEING_DELETED);

CleanUp:

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS createChannelLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    CHAR tagsJson[2 * MAX_JSON_PARAMETER_STRING_LEN];
    PCHAR pCurPtr, pTagsStart, pResponseStr;
    UINT32 i, strLen, resultLen;
    INT32 charsCopied;
    PLwsCallInfo pLwsCallInfo = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, CREATE_SIGNALING_CHANNEL_API_POSTFIX);

    tagsJson[0] = '\0';
    if (pSignalingClient->pChannelInfo->tagCount != 0) {
        // Prepare the tags elements. Format the list at the end
        pCurPtr = tagsJson + MAX_JSON_PARAMETER_STRING_LEN;
        pTagsStart = pCurPtr;
        for (i = 0; i < pSignalingClient->pChannelInfo->tagCount; i++) {
            charsCopied = SNPRINTF(pCurPtr, MAX_JSON_PARAMETER_STRING_LEN - (pCurPtr - pTagsStart), TAG_PARAM_JSON_OBJ_TEMPLATE,
                                   pSignalingClient->pChannelInfo->pTags[i].name, pSignalingClient->pChannelInfo->pTags[i].value);
            CHK(charsCopied > 0, STATUS_INTERNAL_ERROR);
            pCurPtr += charsCopied;
        }

        // Remove the tailing comma
        *(pCurPtr - 1) = '\0';

        // Prepare the json params for the call
        SNPRINTF(tagsJson, MAX_JSON_PARAMETER_STRING_LEN, TAGS_PARAM_JSON_TEMPLATE, tagsJson + MAX_JSON_PARAMETER_STRING_LEN);
    }

    // Prepare the json params for the call
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), CREATE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->pChannelInfo->pChannelName,
             getStringFromChannelType(pSignalingClient->pChannelInfo->channelType),
             pSignalingClient->pChannelInfo->messageTtl / HUNDREDS_OF_NANOS_IN_A_SECOND, tagsJson);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    // Parse out the ARN
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelARN")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
            pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
            i++;
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_SIGNALING_NO_ARN_RETURNED_ON_CREATE);

CleanUp:

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS getChannelEndpointLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    UINT32 i, resultLen, strLen, protocolLen = 0, endpointLen = 0;
    PCHAR pResponseStr, pProtocol = NULL, pEndpoint = NULL;
    PLwsCallInfo pLwsCallInfo = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;
    BOOL jsonInResourceEndpointList = FALSE, protocol = FALSE, endpoint = FALSE, inEndpointArray = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX);

    // Prepare the json params for the call
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
             SIGNALING_CHANNEL_PROTOCOL, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    // Parse and extract the endpoints
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    pSignalingClient->channelEndpointWss[0] = '\0';
    pSignalingClient->channelEndpointHttps[0] = '\0';

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (!jsonInResourceEndpointList) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ResourceEndpointList")) {
                jsonInResourceEndpointList = TRUE;
                i++;
            }
        } else {
            if (!inEndpointArray && tokens[i].type == JSMN_ARRAY) {
                inEndpointArray = TRUE;
            } else {
                if (tokens[i].type == JSMN_OBJECT) {
                    // Process if both are set
                    if (protocol && endpoint) {
                        if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointWss, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        } else if (0 == STRNCMPI(pProtocol, HTTPS_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointHttps, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        }
                    }

                    protocol = FALSE;
                    endpoint = FALSE;
                    protocolLen = 0;
                    endpointLen = 0;
                    pProtocol = NULL;
                    pEndpoint = NULL;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Protocol")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    pProtocol = pResponseStr + tokens[i + 1].start;
                    protocolLen = strLen;
                    protocol = TRUE;
                    i++;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ResourceEndpoint")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_CHANNEL_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    pEndpoint = pResponseStr + tokens[i + 1].start;
                    endpointLen = strLen;
                    endpoint = TRUE;
                    i++;
                }
            }
        }
    }

    // Check if we have unprocessed protocol
    if (protocol && endpoint) {
        if (0 == STRNCMPI(pProtocol, WSS_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointWss, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointWss[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        } else if (0 == STRNCMPI(pProtocol, HTTPS_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointHttps, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointHttps[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0' && pSignalingClient->channelEndpointWss[0] != '\0',
        STATUS_SIGNALING_MISSING_ENDPOINTS_IN_GET_ENDPOINT);

CleanUp:

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS getIceConfigLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    PCHAR pResponseStr;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmntok_t* pToken;
    UINT32 i, strLen, resultLen, configCount = 0, tokenCount;
    INT32 j;
    UINT64 ttl;
    BOOL jsonInIceServerList = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0', STATUS_INTERNAL_ERROR);

    // Update the diagnostics info on the number of ICE refresh calls
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.iceRefreshCount);

    // Create the API url
    STRCPY(url, pSignalingClient->channelEndpointHttps);
    STRCAT(url, GET_ICE_CONFIG_API_POSTFIX);

    // Prepare the json params for the call
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), GET_ICE_CONFIG_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->clientInfo.signalingClientInfo.clientId);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL, retStatus);

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(&pSignalingClient->iceConfigs, 0x00, MAX_ICE_CONFIG_COUNT * SIZEOF(IceConfigInfo));
    pSignalingClient->iceConfigCount = 0;

    // Loop through the tokens and extract the ice configuration
    for (i = 0; i < tokenCount; i++) {
        if (!jsonInIceServerList) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "IceServerList")) {
                jsonInIceServerList = TRUE;

                CHK(tokens[i + 1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                CHK(tokens[i + 1].size <= MAX_ICE_CONFIG_COUNT, STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT);
            }
        } else {
            pToken = &tokens[i];
            if (pToken->type == JSMN_OBJECT) {
                configCount++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Username")) {
                strLen = (UINT32)(pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_USER_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[configCount - 1].userName, pResponseStr + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[configCount - 1].userName[MAX_ICE_CONFIG_USER_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Password")) {
                strLen = (UINT32)(pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->iceConfigs[configCount - 1].password, pResponseStr + pToken[1].start, strLen);
                pSignalingClient->iceConfigs[configCount - 1].userName[MAX_ICE_CONFIG_CREDENTIAL_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Ttl")) {
                CHK_STATUS(STRTOUI64(pResponseStr + pToken[1].start, pResponseStr + pToken[1].end, 10, &ttl));

                // NOTE: Ttl value is in seconds
                pSignalingClient->iceConfigs[configCount - 1].ttl = ttl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Uris")) {
                // Expect an array of elements
                CHK(pToken[1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                CHK(pToken[1].size <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);
                for (j = 0; j < pToken[1].size; j++) {
                    strLen = (UINT32)(pToken[j + 2].end - pToken[j + 2].start);
                    CHK(strLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_SIGNALING_MAX_ICE_URI_LEN);
                    STRNCPY(pSignalingClient->iceConfigs[configCount - 1].uris[j], pResponseStr + pToken[j + 2].start, strLen);
                    pSignalingClient->iceConfigs[configCount - 1].uris[j][MAX_ICE_CONFIG_URI_LEN] = '\0';
                    pSignalingClient->iceConfigs[configCount - 1].uriCount++;
                }

                i += pToken[1].size + 1;
            }
        }
    }

    // Perform some validation on the ice configuration
    pSignalingClient->iceConfigCount = configCount;
    CHK_STATUS(validateIceConfiguration(pSignalingClient));

CleanUp:

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS deleteChannelLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    SIZE_T result;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Check if we need to terminate the ongoing listener
    if (!ATOMIC_LOAD_BOOL(&pSignalingClient->listenerTracker.terminated) && pSignalingClient->pOngoingCallInfo != NULL &&
        pSignalingClient->pOngoingCallInfo->callInfo.pRequestInfo != NULL) {
        terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_OK);
    }

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, DELETE_SIGNALING_CHANNEL_API_POSTFIX);

    // Prepare the json params for the call
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), DELETE_CHANNEL_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
             pSignalingClient->channelDescription.updateVersion);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);

    // Early return if we have a non-success result and it's not a resource not found
    result = ATOMIC_LOAD(&pSignalingClient->result);
    CHK((SERVICE_CALL_RESULT) result == SERVICE_CALL_RESULT_OK || (SERVICE_CALL_RESULT) result == SERVICE_CALL_RESOURCE_NOT_FOUND, retStatus);

    // Mark the channel as deleted
    ATOMIC_STORE_BOOL(&pSignalingClient->deleted, TRUE);

CleanUp:

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS createLwsCallInfo(PSignalingClient pSignalingClient, PRequestInfo pRequestInfo, UINT32 protocolIndex, PLwsCallInfo* ppLwsCallInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PLwsCallInfo pLwsCallInfo = NULL;

    CHK(pSignalingClient != NULL && pRequestInfo != NULL && ppLwsCallInfo != NULL, STATUS_NULL_ARG);

    CHK(NULL != (pLwsCallInfo = (PLwsCallInfo) MEMCALLOC(1, SIZEOF(LwsCallInfo))), STATUS_NOT_ENOUGH_MEMORY);

    pLwsCallInfo->callInfo.pRequestInfo = pRequestInfo;
    pLwsCallInfo->pSignalingClient = pSignalingClient;
    pLwsCallInfo->protocolIndex = protocolIndex;

    *ppLwsCallInfo = pLwsCallInfo;

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeLwsCallInfo(&pLwsCallInfo);
    }

    if (ppLwsCallInfo != NULL) {
        *ppLwsCallInfo = pLwsCallInfo;
    }

    LEAVES();
    return retStatus;
}

STATUS freeLwsCallInfo(PLwsCallInfo* ppLwsCallInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PLwsCallInfo pLwsCallInfo;

    CHK(ppLwsCallInfo != NULL, STATUS_NULL_ARG);
    pLwsCallInfo = *ppLwsCallInfo;

    CHK(pLwsCallInfo != NULL, retStatus);

    freeRequestInfo(&pLwsCallInfo->callInfo.pRequestInfo);
    SAFE_MEMFREE(pLwsCallInfo->callInfo.responseData);

    MEMFREE(pLwsCallInfo);

    *ppLwsCallInfo = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS connectSignalingChannelLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    PLwsCallInfo pLwsCallInfo = NULL;
    BOOL locked = FALSE;
    SERVICE_CALL_RESULT callResult = SERVICE_CALL_RESULT_NOT_SET;
    UINT64 timeout;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointWss[0] != '\0', STATUS_INTERNAL_ERROR);

    // Prepare the json params for the call
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        SNPRINTF(url, ARRAY_SIZE(url), SIGNALING_ENDPOINT_VIEWER_URL_WSS_TEMPLATE, pSignalingClient->channelEndpointWss,
                 SIGNALING_CHANNEL_ARN_PARAM_NAME, pSignalingClient->channelDescription.channelArn, SIGNALING_CLIENT_ID_PARAM_NAME,
                 pSignalingClient->clientInfo.signalingClientInfo.clientId);
    } else {
        SNPRINTF(url, ARRAY_SIZE(url), SIGNALING_ENDPOINT_MASTER_URL_WSS_TEMPLATE, pSignalingClient->channelEndpointWss,
                 SIGNALING_CHANNEL_ARN_PARAM_NAME, pSignalingClient->channelDescription.channelArn);
    }

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, NULL, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));
    // Override the POST with GET
    pRequestInfo->verb = HTTP_REQUEST_VERB_GET;

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_WSS, &pLwsCallInfo));

    // Store the info
    pSignalingClient->pOngoingCallInfo = pLwsCallInfo;

    // Don't let the thread to start running initially
    MUTEX_LOCK(pSignalingClient->connectedLock);
    locked = TRUE;

    // Set the state to not connected
    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) callResult);

    // The actual connection will be handled in a separate thread
    // Start the request/response thread
    CHK_STATUS(THREAD_CREATE(&pSignalingClient->listenerTracker.threadId, lwsListenerHandler, (PVOID) pLwsCallInfo));
    CHK_STATUS(THREAD_DETACH(pSignalingClient->listenerTracker.threadId));

    timeout = (pSignalingClient->clientInfo.connectTimeout != 0) ? pSignalingClient->clientInfo.connectTimeout : SIGNALING_CONNECT_TIMEOUT;

    // Wait for ready
    while ((callResult = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result)) == SERVICE_CALL_RESULT_NOT_SET) {
        CHK_STATUS(CVAR_WAIT(pSignalingClient->connectedCvar, pSignalingClient->connectedLock, timeout));
    }

    // Check whether we are connected and reset the result
    if (ATOMIC_LOAD_BOOL(&pSignalingClient->connected)) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_RESULT_OK);
    }

    MUTEX_UNLOCK(pSignalingClient->connectedLock);
    locked = FALSE;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        // Fix-up the timeout case
        SERVICE_CALL_RESULT serviceCallResult =
            (retStatus == STATUS_OPERATION_TIMED_OUT) ? SERVICE_CALL_NETWORK_CONNECTION_TIMEOUT : SERVICE_CALL_UNKNOWN;
        // Trigger termination
        if (!ATOMIC_LOAD_BOOL(&pSignalingClient->listenerTracker.terminated) && pSignalingClient->pOngoingCallInfo != NULL &&
            pSignalingClient->pOngoingCallInfo->callInfo.pRequestInfo != NULL) {
            terminateConnectionWithStatus(pSignalingClient, serviceCallResult);
        }

        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) serviceCallResult);
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->connectedLock);
    }

    LEAVES();
    return retStatus;
}

PVOID lwsListenerHandler(PVOID args)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PLwsCallInfo pLwsCallInfo = (PLwsCallInfo) args;
    PSignalingClient pSignalingClient = NULL;
    BOOL locked = FALSE;

    CHK(pLwsCallInfo != NULL && pLwsCallInfo->pSignalingClient != NULL, STATUS_NULL_ARG);
    pSignalingClient = pLwsCallInfo->pSignalingClient;

    MUTEX_LOCK(pSignalingClient->listenerTracker.lock);
    locked = TRUE;

    // Interlock to wait for the start sequence
    MUTEX_LOCK(pSignalingClient->connectedLock);
    MUTEX_UNLOCK(pSignalingClient->connectedLock);

    // Mark as started
    ATOMIC_STORE_BOOL(&pSignalingClient->listenerTracker.terminated, FALSE);

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

CleanUp:

    if (STATUS_FAILED(retStatus) && pSignalingClient != NULL) {
        ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) SERVICE_CALL_UNKNOWN);
    }

    // Set the tid to invalid as we are exiting
    if (pSignalingClient != NULL) {
        if (pSignalingClient->pOngoingCallInfo != NULL) {
            freeLwsCallInfo(&pSignalingClient->pOngoingCallInfo);
        }

        ATOMIC_STORE_BOOL(&pSignalingClient->listenerTracker.terminated, TRUE);

        // Trigger the cvar
        if (IS_VALID_CVAR_VALUE(pSignalingClient->connectedCvar)) {
            CVAR_BROADCAST(pSignalingClient->connectedCvar);
        }
        CVAR_BROADCAST(pSignalingClient->listenerTracker.await);
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->listenerTracker.lock);
    }

    LEAVES();
    return (PVOID)(ULONG_PTR) retStatus;
}

PVOID reconnectHandler(PVOID args)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR reconnectErrMsg[SIGNALING_MAX_ERROR_MESSAGE_LEN + 1];
    UINT32 reconnectErrLen;
    PSignalingClient pSignalingClient = (PSignalingClient) args;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // Await for the listener to clear
    MUTEX_LOCK(pSignalingClient->listenerTracker.lock);
    MUTEX_UNLOCK(pSignalingClient->listenerTracker.lock);

    // Indicate that we started
    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);

    // Set the time out before execution
    pSignalingClient->stepUntil = GETTIME() + SIGNALING_CONNECT_STATE_TIMEOUT;

    // Update the diagnostics info
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfReconnects);

    // Attempt to reconnect by driving the state machine to connected state
    CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

CleanUp:

    if (pSignalingClient != NULL) {
        // Call the error handler in case of an error
        if (STATUS_FAILED(retStatus)) {
            // Update the diagnostics before calling the error callback
            ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfRuntimeErrors);
            if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
                reconnectErrLen = SNPRINTF(reconnectErrMsg, SIGNALING_MAX_ERROR_MESSAGE_LEN, SIGNALING_RECONNECT_ERROR_MSG, retStatus);
                reconnectErrMsg[SIGNALING_MAX_ERROR_MESSAGE_LEN] = '\0';
                pSignalingClient->signalingClientCallbacks.errorReportFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                         STATUS_SIGNALING_RECONNECT_FAILED, reconnectErrMsg, reconnectErrLen);
            }
        }

        ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);

        // Notify the listeners to unlock
        CVAR_BROADCAST(pSignalingClient->reconnecterTracker.await);
    }

    LEAVES();
    return (PVOID)(ULONG_PTR) retStatus;
}

STATUS sendLwsMessage(PSignalingClient pSignalingClient, PCHAR pMessageType, PCHAR peerClientId, PCHAR pMessage, UINT32 messageLen,
                      PCHAR pCorrelationId, UINT32 correlationIdLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR encodedMessage[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1];
    UINT32 size, writtenSize, correlationLen;
    BOOL awaitForResponse;

    // Ensure we are in a connected state
    CHK_STATUS(acceptSignalingStateMachineState(pSignalingClient, SIGNALING_STATE_CONNECTED));

    CHK(pSignalingClient != NULL && pSignalingClient->pOngoingCallInfo != NULL, STATUS_NULL_ARG);

    // Calculate the lengths if not specified
    if (messageLen == 0) {
        size = (UINT32) STRLEN(pMessage);
    } else {
        size = messageLen;
    }

    if (correlationIdLen == 0) {
        correlationLen = (UINT32) STRLEN(pCorrelationId);
    } else {
        correlationLen = correlationIdLen;
    }

    // Base64 encode the message
    writtenSize = ARRAY_SIZE(encodedMessage);
    CHK_STATUS(base64Encode(pMessage, size, encodedMessage, &writtenSize));

    // Account for the template expansion + Action string + max recipient id
    size = SIZEOF(pSignalingClient->pOngoingCallInfo->sendBuffer) - LWS_PRE;
    CHK(writtenSize <= size, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);

    // Prepare json message
    if (correlationLen == 0) {
        writtenSize = (UINT32) SNPRINTF((PCHAR)(pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE), size, SIGNALING_SEND_MESSAGE_TEMPLATE,
                                        pMessageType, MAX_SIGNALING_CLIENT_ID_LEN, peerClientId, encodedMessage);
    } else {
        writtenSize = (UINT32) SNPRINTF((PCHAR)(pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE), size,
                                        SIGNALING_SEND_MESSAGE_TEMPLATE_WITH_CORRELATION_ID, pMessageType, MAX_SIGNALING_CLIENT_ID_LEN, peerClientId,
                                        encodedMessage, correlationLen, pCorrelationId);
    }

    // Validate against max
    CHK(writtenSize <= LWS_MESSAGE_BUFFER_SIZE, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);

    writtenSize *= SIZEOF(CHAR);
    CHK(writtenSize <= size, STATUS_INVALID_ARG);

    // Store the data pointer
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendBufferSize, LWS_PRE + writtenSize);
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendOffset, LWS_PRE);

    // Send the data to the web socket
    awaitForResponse = (correlationLen != 0) && BLOCK_ON_CORRELATION_ID;
    CHK_STATUS(writeLwsData(pSignalingClient, awaitForResponse));

    // Re-setting the buffer size and offset
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendBufferSize, 0);
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendOffset, 0);

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS writeLwsData(PSignalingClient pSignalingClient, BOOL awaitForResponse)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL sendLocked = FALSE, receiveLocked = FALSE, iterate = TRUE;
    SIZE_T offset, size;
    SERVICE_CALL_RESULT result;

    CHK(pSignalingClient != NULL && pSignalingClient->pOngoingCallInfo != NULL, STATUS_NULL_ARG);

    // See if anything needs to be done
    CHK(pSignalingClient->pOngoingCallInfo->sendBufferSize != pSignalingClient->pOngoingCallInfo->sendOffset, retStatus);

    DLOGD("Sending data over web socket: %s", pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE);

    // Initialize the send result to none
    ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Wake up the service event loop
    CHK_STATUS(wakeLwsServiceEventLoop(pSignalingClient));

    MUTEX_LOCK(pSignalingClient->sendLock);
    sendLocked = TRUE;
    while (iterate) {
        offset = ATOMIC_LOAD(&pSignalingClient->pOngoingCallInfo->sendOffset);
        size = ATOMIC_LOAD(&pSignalingClient->pOngoingCallInfo->sendBufferSize);

        result = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->messageResult);

        if (offset != size && result == SERVICE_CALL_RESULT_NOT_SET) {
            CHK_STATUS(CVAR_WAIT(pSignalingClient->sendCvar, pSignalingClient->sendLock, SIGNALING_SEND_TIMEOUT));
        } else {
            iterate = FALSE;
        }
    }

    MUTEX_UNLOCK(pSignalingClient->sendLock);
    sendLocked = FALSE;

    // Do not await for the response in case of correlation id not specified
    CHK(awaitForResponse, retStatus);

    // Await for the response
    MUTEX_LOCK(pSignalingClient->receiveLock);
    receiveLocked = TRUE;

    iterate = TRUE;
    while (iterate) {
        result = (SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->messageResult);

        if (result == SERVICE_CALL_RESULT_NOT_SET) {
            CHK_STATUS(CVAR_WAIT(pSignalingClient->receiveCvar, pSignalingClient->receiveLock, SIGNALING_SEND_TIMEOUT));
        } else {
            iterate = FALSE;
        }
    }

    MUTEX_UNLOCK(pSignalingClient->receiveLock);
    receiveLocked = FALSE;

    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->messageResult) == SERVICE_CALL_RESULT_OK, STATUS_SIGNALING_MESSAGE_DELIVERY_FAILED);

CleanUp:

    if (sendLocked) {
        MUTEX_UNLOCK(pSignalingClient->sendLock);
    }

    if (receiveLocked) {
        MUTEX_UNLOCK(pSignalingClient->receiveLock);
    }

    LEAVES();
    return retStatus;
}

STATUS receiveLwsMessage(PSignalingClient pSignalingClient, PCHAR pMessage, UINT32 messageLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 i, strLen, outLen = MAX_SIGNALING_MESSAGE_LEN;
    UINT32 tokenCount;
    PSignalingMessageWrapper pSignalingMessageWrapper = NULL;
    TID receivedTid = INVALID_TID_VALUE;
    BOOL parsedMessageType = FALSE, parsedStatusResponse = FALSE;
    PSignalingMessage pOngoingMessage;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // If we have a signalingMessage and if there is a correlation id specified then the response should be non-empty
    if (pMessage == NULL || messageLen == 0) {
        if (BLOCK_ON_CORRELATION_ID) {
            // Get empty correlation id message from the ongoing if exists
            CHK_STATUS(signalingGetOngoingMessage(pSignalingClient, EMPTY_STRING, EMPTY_STRING, &pOngoingMessage));
            if (pOngoingMessage == NULL) {
                DLOGW("Received an empty body for a message with no correlation id which has been already removed from the queue. Warning 0x%08x",
                      STATUS_SIGNALING_RECEIVE_EMPTY_DATA_NOT_SUPPORTED);
            } else {
                CHK_STATUS(signalingRemoveOngoingMessage(pSignalingClient, EMPTY_STRING));
            }
        }

        // Check if anything needs to be done
        CHK_WARN(pMessage != NULL && messageLen != 0, retStatus, "Signaling received an empty message");
    }

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pMessage, messageLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    CHK(NULL != (pSignalingMessageWrapper = (PSignalingMessageWrapper) MEMCALLOC(1, SIZEOF(SignalingMessageWrapper))), STATUS_NOT_ENOUGH_MEMORY);

    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "senderClientId")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_CLIENT_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId, pMessage + tokens[i + 1].start, strLen);
            pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId[MAX_SIGNALING_CLIENT_ID_LEN] = '\0';
            i++;
        } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "messageType")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_MESSAGE_TYPE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            CHK_STATUS(getMessageTypeFromString(pMessage + tokens[i + 1].start, strLen,
                                                &pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType));

            parsedMessageType = TRUE;
            i++;
        } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "messagePayload")) {
            strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_MESSAGE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

            // Base64 decode the message
            CHK_STATUS(base64Decode(pMessage + tokens[i + 1].start, strLen,
                                    (PBYTE)(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload), &outLen));
            pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload[MAX_SIGNALING_MESSAGE_LEN] = '\0';
            pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen = outLen;
            i++;
        } else {
            if (!parsedStatusResponse) {
                if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "statusResponse")) {
                    parsedStatusResponse = TRUE;
                    i++;
                }
            } else {
                if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "correlationId")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_CORRELATION_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId, pMessage + tokens[i + 1].start,
                            strLen);
                    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId[MAX_CORRELATION_ID_LEN] = '\0';

                    i++;
                } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "errorType")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_ERROR_TYPE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.errorType, pMessage + tokens[i + 1].start, strLen);
                    pSignalingMessageWrapper->receivedSignalingMessage.errorType[MAX_ERROR_TYPE_STRING_LEN] = '\0';

                    i++;
                } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "statusCode")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_STATUS_CODE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

                    // Parse the status code
                    CHK_STATUS(STRTOUI32(pMessage + tokens[i + 1].start, pMessage + tokens[i + 1].end, 10,
                                         &pSignalingMessageWrapper->receivedSignalingMessage.statusCode));

                    i++;
                } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "description")) {
                    strLen = (UINT32)(tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_MESSAGE_DESCRIPTION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.description, pMessage + tokens[i + 1].start, strLen);
                    pSignalingMessageWrapper->receivedSignalingMessage.description[MAX_MESSAGE_DESCRIPTION_LEN] = '\0';

                    i++;
                }
            }
        }
    }

    // Message type is a mandatory field.
    CHK(parsedMessageType, STATUS_SIGNALING_INVALID_MESSAGE_TYPE);
    pSignalingMessageWrapper->pSignalingClient = pSignalingClient;

    switch (pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType) {
        case SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE:
            if (pSignalingMessageWrapper->receivedSignalingMessage.statusCode != SERVICE_CALL_RESULT_OK) {
                DLOGW("Failed to deliver message. Correlation ID: %s, Error Type: %s, Error Code: %u, Description: %s",
                      pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId,
                      pSignalingMessageWrapper->receivedSignalingMessage.errorType, pSignalingMessageWrapper->receivedSignalingMessage.statusCode,
                      pSignalingMessageWrapper->receivedSignalingMessage.description);

                // Store the response
                ATOMIC_STORE(&pSignalingClient->messageResult,
                             (SIZE_T) getServiceCallResultFromHttpStatus(pSignalingMessageWrapper->receivedSignalingMessage.statusCode));
            } else {
                // Success
                ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_RESULT_OK);
            }

            // Notify the awaiting send
            CVAR_BROADCAST(pSignalingClient->receiveCvar);
            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);
            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_GO_AWAY:
            // Move the describe state
            CHK_STATUS(terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_GO_AWAY));

            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);

            // Iterate the state machinery
            CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            // Move to get ice config state
            CHK_STATUS(terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE));

            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);

            // Iterate the state machinery
            CHK_STATUS(stepSignalingStateMachine(pSignalingClient, retStatus));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_OFFER:
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId[0] != '\0',
                STATUS_SIGNALING_NO_PEER_CLIENT_ID_IN_MESSAGE);
            // Explicit fall-through !!!
        case SIGNALING_MESSAGE_TYPE_ANSWER:
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen > 0 &&
                    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen <= MAX_SIGNALING_MESSAGE_LEN,
                STATUS_SIGNALING_INVALID_PAYLOAD_LEN_IN_MESSAGE);
            CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload[0] != '\0', STATUS_SIGNALING_NO_PAYLOAD_IN_MESSAGE);
            break;

        default:
            break;
    }

    // Issue the callback on a separate thread
    CHK_STATUS(THREAD_CREATE(&receivedTid, receiveLwsMessageWrapper, (PVOID) pSignalingMessageWrapper));
    CHK_STATUS(THREAD_DETACH(receivedTid));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pSignalingClient != NULL && STATUS_FAILED(retStatus)) {
        ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfRuntimeErrors);
        if (pSignalingClient->signalingClientCallbacks.errorReportFn != NULL) {
            retStatus = pSignalingClient->signalingClientCallbacks.errorReportFn(pSignalingClient->signalingClientCallbacks.customData, retStatus,
                                                                                 pMessage, messageLen);
        }

        // Kill the receive thread on error
        if (IS_VALID_TID_VALUE(receivedTid)) {
            THREAD_CANCEL(receivedTid);
        }

        SAFE_MEMFREE(pSignalingMessageWrapper);
    }

    LEAVES();
    return retStatus;
}

STATUS terminateConnectionWithStatus(PSignalingClient pSignalingClient, SERVICE_CALL_RESULT callResult)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    CVAR_BROADCAST(pSignalingClient->connectedCvar);
    CVAR_BROADCAST(pSignalingClient->receiveCvar);
    CVAR_BROADCAST(pSignalingClient->sendCvar);
    ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_UNKNOWN);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) callResult);

    if (pSignalingClient->pOngoingCallInfo != NULL) {
        ATOMIC_STORE_BOOL(&pSignalingClient->pOngoingCallInfo->cancelService, TRUE);
    }

    // Wake up the service event loop
    CHK_STATUS(wakeLwsServiceEventLoop(pSignalingClient));
    CHK_STATUS(awaitForThreadTermination(&pSignalingClient->listenerTracker, SIGNALING_CLIENT_SHUTDOWN_TIMEOUT));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS getMessageTypeFromString(PCHAR typeStr, UINT32 typeLen, SIGNALING_MESSAGE_TYPE* pMessageType)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 len;

    CHK(typeStr != NULL && pMessageType != NULL, STATUS_NULL_ARG);

    if (typeLen == 0) {
        len = (UINT32) STRLEN(typeStr);
    } else {
        len = typeLen;
    }

    if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_OFFER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_OFFER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_SDP_TYPE_ANSWER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_ICE_CANDIDATE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    } else if (0 == STRNCMP(typeStr, SIGNALING_GO_AWAY, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_GO_AWAY;
    } else if (0 == STRNCMP(typeStr, SIGNALING_RECONNECT_ICE_SERVER, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER;
    } else if (0 == STRNCMP(typeStr, SIGNALING_STATUS_RESPONSE, len)) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE;
    } else {
        *pMessageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;
        CHK_WARN(FALSE, retStatus, "Unrecognized message type received");
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS terminateLwsListenerLoop(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSignalingClient != NULL, retStatus);

    if (pSignalingClient->pOngoingCallInfo != NULL) {
        // Check if anything needs to be done
        CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->listenerTracker.terminated), retStatus);

        // Terminate the listener
        terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_OK);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

PVOID receiveLwsMessageWrapper(PVOID args)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSignalingMessageWrapper pSignalingMessageWrapper = (PSignalingMessageWrapper) args;
    PSignalingClient pSignalingClient = NULL;

    CHK(pSignalingMessageWrapper != NULL, STATUS_NULL_ARG);

    pSignalingClient = pSignalingMessageWrapper->pSignalingClient;

    CHK(pSignalingClient != NULL, STATUS_INTERNAL_ERROR);

    // Updating the diagnostics info before calling the client callback
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfMessagesReceived);

    // Calling client receive message callback if specified
    if (pSignalingClient->signalingClientCallbacks.messageReceivedFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.messageReceivedFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                                &pSignalingMessageWrapper->receivedSignalingMessage));
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    SAFE_MEMFREE(pSignalingMessageWrapper);

    return (PVOID)(ULONG_PTR) retStatus;
}

STATUS wakeLwsServiceEventLoop(PSignalingClient pSignalingClient)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    // Early exit in case we don't need to do anything
    CHK(pSignalingClient != NULL && pSignalingClient->pLwsContext != NULL, retStatus);

    MUTEX_LOCK(pSignalingClient->lwsServiceLock);
    lws_callback_on_writable_all_protocol(pSignalingClient->pLwsContext, &pSignalingClient->signalingProtocols[WSS_SIGNALING_PROTOCOL_INDEX]);
    MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);

CleanUp:

    LEAVES();
    return retStatus;
}
