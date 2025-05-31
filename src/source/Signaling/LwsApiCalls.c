/**
 * Implementation of a API calls based on LibWebSocket
 */
#define LOG_CLASS "LwsApiCalls"
#include "../Include_i.h"
#include "kvssignaling/signaling_api.h"
#define WEBRTC_SCHEME_NAME "webrtc"

static BOOL gInterruptedFlagBySignalHandler;
VOID lwsSignalHandler(INT32 signal)
{
    UNUSED_PARAM(signal);
    gInterruptedFlagBySignalHandler = TRUE;
}

static STATUS getChannelRoleType(SIGNALING_CHANNEL_ROLE_TYPE roleType, SignalingRole_t* pOutputRole)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pOutputRole != NULL, STATUS_SIGNALING_INVALID_OUTPUT_ROLE);

    switch (roleType) {
        case SIGNALING_CHANNEL_ROLE_TYPE_MASTER:
            *pOutputRole = SIGNALING_ROLE_MASTER;
            break;
        case SIGNALING_CHANNEL_ROLE_TYPE_VIEWER:
            *pOutputRole = SIGNALING_ROLE_VIEWER;
            break;
        default:
            *pOutputRole = SIGNALING_ROLE_NONE;
            break;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS getMessageType(SignalingTypeMessage_t messageType, SIGNALING_MESSAGE_TYPE* pMessageType)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pMessageType != NULL, STATUS_SIGNALING_INVALID_OUTPUT_MESSAGE_TYPE);

    if (messageType == SIGNALING_TYPE_MESSAGE_SDP_OFFER) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_OFFER;
    } else if (messageType == SIGNALING_TYPE_MESSAGE_SDP_ANSWER) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ANSWER;
    } else if (messageType == SIGNALING_TYPE_MESSAGE_ICE_CANDIDATE) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE;
    } else if (messageType == SIGNALING_TYPE_MESSAGE_GO_AWAY) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_GO_AWAY;
    } else if (messageType == SIGNALING_TYPE_MESSAGE_RECONNECT_ICE_SERVER) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER;
    } else if (messageType == SIGNALING_TYPE_MESSAGE_STATUS_RESPONSE) {
        *pMessageType = SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE;
    } else {
        *pMessageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;
        CHK_WARN(FALSE, retStatus, "Unrecognized message type received");
    }

CleanUp:

    LEAVES();
    return retStatus;
}

static STATUS updateIceServerList(PSignalingClient pSignalingClient, SignalingIceServer_t* pIceServers, SIZE_T numIceServers)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, j;

    CHK(pIceServers != NULL, STATUS_SIGNALING_ICE_CONFIG_REFRESH_FAILED);
    CHK(numIceServers <= MAX_ICE_CONFIG_COUNT, STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT);

    MEMSET(&pSignalingClient->iceConfigs, 0x00, MAX_ICE_CONFIG_COUNT * SIZEOF(IceConfigInfo));
    pSignalingClient->iceConfigCount = 0;

    for (i = 0; i < numIceServers; i++) {
        if (pIceServers[i].pUserName != NULL) {
            CHK(pIceServers[i].userNameLength <= MAX_ICE_CONFIG_USER_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingClient->iceConfigs[i].userName, pIceServers[i].pUserName, pIceServers[i].userNameLength);
            pSignalingClient->iceConfigs[i].userName[pIceServers[i].userNameLength] = '\0';
        }

        if (pIceServers[i].pPassword != NULL) {
            CHK(pIceServers[i].passwordLength <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingClient->iceConfigs[i].password, pIceServers[i].pPassword, pIceServers[i].passwordLength);
            pSignalingClient->iceConfigs[i].password[pIceServers[i].passwordLength] = '\0';
        }

        if (pIceServers[i].messageTtlSeconds > 0) {
            // NOTE: Ttl value is in seconds
            pSignalingClient->iceConfigs[i].ttl = (UINT64) pIceServers[i].messageTtlSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
        }

        if (pIceServers[i].pUris[0] != NULL) {
            CHK(pIceServers[i].urisNum <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);

            for (j = 0; j < pIceServers[i].urisNum; j++) {
                CHK(pIceServers[i].urisLength[j] <= MAX_ICE_CONFIG_URI_LEN, STATUS_SIGNALING_MAX_ICE_URI_LEN);

                STRNCPY(pSignalingClient->iceConfigs[i].uris[j], pIceServers[i].pUris[j], pIceServers[i].urisLength[j]);
                pSignalingClient->iceConfigs[i].uris[j][pIceServers[i].urisLength[j]] = '\0';
                pSignalingClient->iceConfigs[i].uriCount++;
            }
        }
    }

    // Perform some validation on the ice configuration
    pSignalingClient->iceConfigCount = numIceServers;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

INT32 lwsHttpCallbackRoutine(struct lws* wsi, enum lws_callback_reasons reason, PVOID user, PVOID pDataIn, size_t dataSize)
{
    UNUSED_PARAM(user);
    STATUS retStatus = STATUS_SUCCESS;
    PVOID customData;
    INT32 status, retValue = 0, size;
    PCHAR pCurPtr, pBuffer;
    CHAR dateHdrBuffer[MAX_DATE_HEADER_BUFFER_LENGTH + 1];
    PBYTE pEndPtr;
    PBYTE* ppStartPtr;
    PLwsCallInfo pLwsCallInfo;
    PRequestInfo pRequestInfo = NULL;
    PSingleListNode pCurNode;
    UINT64 item, serverTime;
    UINT32 headerCount;
    UINT32 logLevel;
    PRequestHeader pRequestHeader;
    PSignalingClient pSignalingClient = NULL;
    BOOL locked = FALSE;
    time_t td;
    SIZE_T len;
    UINT64 nowTime, clockSkew = 0;
    PStateMachineState pStateMachineState;
    BOOL skewMapContains = FALSE;

    UNUSED_PARAM(logLevel);
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

    lws_set_log_level(LLL_NOTICE | LLL_WARN | LLL_ERR, NULL);

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
    nowTime = SIGNALING_GET_CURRENT_TIME(pSignalingClient);

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
            getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState);

            DLOGD("Connected with server response: %d", status);
            pLwsCallInfo->callInfo.callResult = getServiceCallResultFromHttpStatus((UINT32) status);

            len = (SIZE_T) lws_hdr_copy(wsi, &dateHdrBuffer[0], MAX_DATE_HEADER_BUFFER_LENGTH, WSI_TOKEN_HTTP_DATE);

            time(&td);

            if (len) {
                // on failure to parse lws_http_date_unix returns non zero value
                if (0 == lws_http_date_parse_unix(&dateHdrBuffer[0], len, &td)) {
                    DLOGV("Date Header Returned By Server:  %s", dateHdrBuffer);

                    serverTime = ((UINT64) td) * HUNDREDS_OF_NANOS_IN_A_SECOND;

                    if (serverTime > nowTime + MIN_CLOCK_SKEW_TIME_TO_CORRECT) {
                        // Server time is ahead
                        clockSkew = (serverTime - nowTime);
                        DLOGD("Detected Clock Skew!  Server time is AHEAD of Device time: Server time: %" PRIu64 ", now time: %" PRIu64, serverTime,
                              nowTime);
                    } else if (nowTime > serverTime + MIN_CLOCK_SKEW_TIME_TO_CORRECT) {
                        clockSkew = (nowTime - serverTime);
                        clockSkew |= ((UINT64) (1ULL << 63));
                        DLOGD("Detected Clock Skew!  Device time is AHEAD of Server time: Server time: %" PRIu64 ", now time: %" PRIu64, serverTime,
                              nowTime);
                        // PIC hashTable implementation only stores UINT64 so I will flip the sign of the msb
                        // This limits the range of the max clock skew we can represent to just under 2925 years.
                    }

                    hashTableContains(pSignalingClient->diagnostics.pEndpointToClockSkewHashMap, pStateMachineState->state, &skewMapContains);
                    if (clockSkew > 0) {
                        hashTablePut(pSignalingClient->diagnostics.pEndpointToClockSkewHashMap, pStateMachineState->state, clockSkew);
                    } else if (clockSkew == 0 && skewMapContains) {
                        // This means the item is in the map so at one point there was a clock skew offset but it has been corrected
                        // So we should no longer be correcting for a clock skew, remove this item from the map
                        hashTableRemove(pSignalingClient->diagnostics.pEndpointToClockSkewHashMap, pStateMachineState->state);
                    }
                }
            }

            // Store the Request ID header
            if ((size = lws_hdr_custom_copy(wsi, pBuffer, LWS_SCRATCH_BUFFER_SIZE, SIGNALING_REQUEST_ID_HEADER_NAME,
                                            (SIZEOF(SIGNALING_REQUEST_ID_HEADER_NAME) - 1) * SIZEOF(CHAR))) > 0) {
                pBuffer[size] = '\0';
                DLOGI("Request ID: %s", pBuffer);
            }

            break;

        case LWS_CALLBACK_RECEIVE_CLIENT_HTTP_READ:
            DLOGD("Received client http read: %d bytes", (INT32) dataSize);
            lwsl_hexdump_debug(pDataIn, dataSize);

            if (dataSize != 0) {
                CHK(NULL != (pLwsCallInfo->callInfo.responseData = (PCHAR) MEMALLOC(dataSize + 1)), STATUS_NOT_ENOUGH_MEMORY);
                MEMCPY(pLwsCallInfo->callInfo.responseData, pDataIn, dataSize);
                pLwsCallInfo->callInfo.responseData[dataSize] = '\0';
                pLwsCallInfo->callInfo.responseDataLen = (UINT32) dataSize;

                if (pLwsCallInfo->callInfo.callResult != SERVICE_CALL_RESULT_OK) {
                    DLOGW("Received client http read response:  %s", pLwsCallInfo->callInfo.responseData);
                    if (pLwsCallInfo->callInfo.callResult == SERVICE_CALL_FORBIDDEN) {
                        if (isCallResultSignatureExpired(&pLwsCallInfo->callInfo)) {
                            // Set more specific result, this is so in the state machine
                            // We don't call GetToken again rather RETRY the existing API (now with clock skew correction)
                            pLwsCallInfo->callInfo.callResult = SERVICE_CALL_SIGNATURE_EXPIRED;
                        } else if (isCallResultSignatureNotYetCurrent(&pLwsCallInfo->callInfo)) {
                            // server time is ahead
                            pLwsCallInfo->callInfo.callResult = SERVICE_CALL_SIGNATURE_NOT_YET_CURRENT;
                        }
                    }
                } else {
                    DLOGV("Received client http read response:  %s", pLwsCallInfo->callInfo.responseData);
                }
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
            DLOGI("WSS callback with reason %d", reason);
            CHK(FALSE, retStatus);
    }

    customData = lws_get_opaque_user_data(wsi);
    pLwsCallInfo = (PLwsCallInfo) customData;

    lws_set_log_level(LLL_NOTICE | LLL_WARN | LLL_ERR, NULL);

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
                // Handle re-connection in a reconnect handler thread. Set the terminated indicator before the thread
                // creation and the thread itself will reset it. NOTE: Need to check for a failure and reset.
                ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);
                retStatus = THREAD_CREATE(&pSignalingClient->reconnecterTracker.threadId, reconnectHandler, (PVOID) pSignalingClient);
                if (STATUS_FAILED(retStatus)) {
                    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
                    CHK(FALSE, retStatus);
                }

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
            pSignalingClient->diagnostics.connectTime = SIGNALING_GET_CURRENT_TIME(pSignalingClient);
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

                // Handle re-connection in a reconnect handler thread. Set the terminated indicator before the thread
                // creation and the thread itself will reset it. NOTE: Need to check for a failure and reset.
                ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, FALSE);
                retStatus = THREAD_CREATE(&pSignalingClient->reconnecterTracker.threadId, reconnectHandler, (PVOID) pSignalingClient);
                if (STATUS_FAILED(retStatus)) {
                    ATOMIC_STORE_BOOL(&pSignalingClient->reconnecterTracker.terminated, TRUE);
                    CHK(FALSE, retStatus);
                }
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
                pCurPtr = (PCHAR) ((PBYTE) pDataIn + SIZEOF(UINT16));
                size -= SIZEOF(UINT16);
            }

            DLOGD("Peer initiated close with %d (0x%08x). Message: %.*s", status, (UINT32) status, size, pCurPtr);

            // Store the state as the result
            retValue = -1;

            ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) status);

            break;

        case LWS_CALLBACK_CLIENT_RECEIVE:

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
            writeSize = (INT32) (bufferSize - offset);

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
    BOOL secureConnection, locked = FALSE, serializerLocked = FALSE, iterate = TRUE;
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

    // Attempt to iterate and acquire the locks
    // NOTE: The https protocol should be called sequentially only
    MUTEX_LOCK(pCallInfo->pSignalingClient->lwsSerializerLock);
    serializerLocked = TRUE;

    // Ensure we are not running another https protocol
    // The WSIs for all of the protocols are set and cleared in this function only.
    // The HTTPS is serialized via the state machine lock and we should not encounter
    // another https protocol in flight. The only case is when we have an http request
    // and a wss is in progress. This is the case when we have a current websocket listener
    // and need to perform an https call due to ICE server config refresh for example.
    // If we have an ongoing wss operations, we can't call lws_client_connect_via_info API
    // due to threading model of LWS. What we need to do is to wake up the potentially blocked
    // ongoing wss handler for it to release the service lock which it holds while calling lws_service()
    // API so we can grab the lock in order to perform the lws_client_connect_via_info API call.
    // The need to wake up the wss handler (if any) to compete for the lock is the reason for this
    // loop. In order to avoid pegging of the CPU while the contention for the lock happens,
    // we are setting an atomic and releasing it to trigger a timed wait when the lws_service call
    // awakes to make sure we are not going to starve this thread.

    // NOTE: The THREAD_SLEEP calls in this routine are not designed to adjust
    // the execution timing/race conditions but to eliminate a busy wait in a spin-lock
    // type scenario for resource contention.

    // We should have HTTPS protocol serialized at the state machine level
    CHK_ERR(pCallInfo->pSignalingClient->currentWsi[PROTOCOL_INDEX_HTTPS] == NULL, STATUS_INVALID_OPERATION,
            "HTTPS requests should be processed sequentially.");

    // Indicate that we are trying to acquire the lock
    ATOMIC_STORE_BOOL(&pCallInfo->pSignalingClient->serviceLockContention, TRUE);
    while (iterate && pCallInfo->pSignalingClient->currentWsi[PROTOCOL_INDEX_WSS] != NULL) {
        if (!MUTEX_TRYLOCK(pCallInfo->pSignalingClient->lwsServiceLock)) {
            // Wake up the event loop
            CHK_STATUS(wakeLwsServiceEventLoop(pCallInfo->pSignalingClient, PROTOCOL_INDEX_WSS));
        } else {
            locked = TRUE;
            iterate = FALSE;
        }
    }
    ATOMIC_STORE_BOOL(&pCallInfo->pSignalingClient->serviceLockContention, FALSE);

    // Now we should be running with a lock
    CHK(NULL != (pCallInfo->pSignalingClient->currentWsi[pCallInfo->protocolIndex] = lws_client_connect_via_info(&connectInfo)),
        STATUS_SIGNALING_LWS_CLIENT_CONNECT_FAILED);
    if (locked) {
        MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsServiceLock);
        locked = FALSE;
    }

    MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsSerializerLock);
    serializerLocked = FALSE;

    while (retVal >= 0 && !gInterruptedFlagBySignalHandler && pCallInfo->callInfo.pRequestInfo != NULL &&
           !ATOMIC_LOAD_BOOL(&pCallInfo->callInfo.pRequestInfo->terminating)) {
        if (!MUTEX_TRYLOCK(pCallInfo->pSignalingClient->lwsServiceLock)) {
            THREAD_SLEEP(LWS_SERVICE_LOOP_ITERATION_WAIT);
        } else {
            retVal = lws_service(pContext, 0);
            MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsServiceLock);

            // Add a minor timeout to relinquish the thread quota to eliminate thread starvation
            // when competing for the service lock
            if (ATOMIC_LOAD_BOOL(&pCallInfo->pSignalingClient->serviceLockContention)) {
                THREAD_SLEEP(LWS_SERVICE_LOOP_ITERATION_WAIT);
            }
        }
    }

    // Clear the wsi on exit
    MUTEX_LOCK(pCallInfo->pSignalingClient->lwsSerializerLock);
    pCallInfo->pSignalingClient->currentWsi[pCallInfo->protocolIndex] = NULL;
    MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsSerializerLock);

CleanUp:

    // Reset the lock contention indicator in case of failure
    if (STATUS_FAILED(retStatus) && pCallInfo != NULL && pCallInfo->pSignalingClient != NULL) {
        ATOMIC_STORE_BOOL(&pCallInfo->pSignalingClient->serviceLockContention, FALSE);
    }

    if (serializerLocked) {
        MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsSerializerLock);
    }

    if (locked) {
        MUTEX_UNLOCK(pCallInfo->pSignalingClient->lwsServiceLock);
    }

    LEAVES();
    return retStatus;
}

BOOL isCallResultSignatureExpired(PCallInfo pCallInfo)
{
    return (STRNSTR(pCallInfo->responseData, "Signature expired", pCallInfo->responseDataLen) != NULL);
}

BOOL isCallResultSignatureNotYetCurrent(PCallInfo pCallInfo)
{
    return (STRNSTR(pCallInfo->responseData, "Signature not yet current", pCallInfo->responseDataLen) != NULL);
}

STATUS checkAndCorrectForClockSkew(PSignalingClient pSignalingClient, PRequestInfo pRequestInfo)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    PStateMachineState pStateMachineState;
    PHashTable pClockSkewMap;
    UINT64 clockSkewOffset;
    CHK_STATUS(getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState));

    pClockSkewMap = pSignalingClient->diagnostics.pEndpointToClockSkewHashMap;

    CHK_STATUS(hashTableGet(pClockSkewMap, pStateMachineState->state, &clockSkewOffset));

    // if we made it here that means there is clock skew
    if (clockSkewOffset & ((UINT64) (1ULL << 63))) {
        clockSkewOffset ^= ((UINT64) (1ULL << 63));
        DLOGV("Detected device time is AHEAD of server time!");
        pRequestInfo->currentTime -= clockSkewOffset;
    } else {
        DLOGV("Detected server time is AHEAD of device time!");
        pRequestInfo->currentTime += clockSkewOffset;
    }

    DLOGW("Clockskew corrected!");

CleanUp:

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
    UINT32 resultLen;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingAwsRegion_t awsRegion;
    SignalingChannelName_t channelName;
    SignalingChannelInfo_t channelInfo;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->pChannelInfo != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->pChannelInfo->pChannelName != NULL, STATUS_NULL_ARG);

    // Prepare AWS region
    awsRegion.pAwsRegion = pSignalingClient->pChannelInfo->pRegion;
    awsRegion.awsRegionLength = STRNLEN(pSignalingClient->pChannelInfo->pRegion, MAX_REGION_NAME_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Prepare channel name
    channelName.channelNameLength = STRNLEN(pSignalingClient->pChannelInfo->pChannelName, MAX_CHANNEL_NAME_LEN);
    channelName.pChannelName = pSignalingClient->pChannelInfo->pChannelName;

    retSignal = Signaling_ConstructDescribeSignalingChannelRequest(&awsRegion, &channelName, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    // createRequestInfo does not have access to the getCurrentTime callback, this hook is used for tests.
    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    retSignal = Signaling_ParseDescribeSignalingChannelResponse(pResponseStr, resultLen, &channelInfo);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Parse the response
    MEMSET(&pSignalingClient->channelDescription, 0x00, SIZEOF(SignalingChannelDescription));

    pSignalingClient->channelDescription.channelType =
        channelInfo.channelType == SIGNALING_TYPE_CHANNEL_SINGLE_MASTER ? SIGNALING_CHANNEL_TYPE_SINGLE_MASTER : SIGNALING_CHANNEL_TYPE_UNKNOWN;

    if (channelInfo.channelArn.pChannelArn != NULL) {
        CHK(channelInfo.channelArn.channelArnLength <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelDescription.channelArn, channelInfo.channelArn.pChannelArn, channelInfo.channelArn.channelArnLength);
        pSignalingClient->channelDescription.channelArn[channelInfo.channelArn.channelArnLength] = '\0';
    }

    if (channelInfo.channelName.pChannelName != NULL) {
        CHK(channelInfo.channelName.channelNameLength <= MAX_CHANNEL_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelDescription.channelName, channelInfo.channelName.pChannelName, channelInfo.channelName.channelNameLength);
        pSignalingClient->channelDescription.channelName[channelInfo.channelName.channelNameLength] = '\0';
    }

    if (channelInfo.pChannelStatus != NULL) {
        CHK(channelInfo.channelStatusLength <= MAX_DESCRIBE_CHANNEL_STATUS_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        pSignalingClient->channelDescription.channelStatus =
            getChannelStatusFromString((PCHAR) channelInfo.pChannelStatus, channelInfo.channelStatusLength);
    }

    if (channelInfo.pVersion != NULL) {
        CHK(channelInfo.versionLength <= MAX_UPDATE_VERSION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelDescription.updateVersion, channelInfo.pVersion, channelInfo.versionLength);
        pSignalingClient->channelDescription.updateVersion[channelInfo.versionLength] = '\0';
    }

    if (channelInfo.messageTtlSeconds != 0) {
        // NOTE: Ttl value is in seconds
        pSignalingClient->channelDescription.messageTtl = channelInfo.messageTtlSeconds * HUNDREDS_OF_NANOS_IN_A_SECOND;
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelStatus != SIGNALING_CHANNEL_STATUS_DELETING, STATUS_SIGNALING_CHANNEL_BEING_DELETED);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

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
    PCHAR pResponseStr;
    UINT32 i, resultLen;
    PLwsCallInfo pLwsCallInfo = NULL;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingAwsRegion_t awsRegion;
    CreateSignalingChannelRequestInfo_t createSignalingChannelRequestInfo;
    SignalingChannelArn_t channelArn;
    SignalingTag_t tags[MAX_TAG_COUNT];

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->pChannelInfo->tagCount <= MAX_TAG_COUNT, STATUS_INVALID_ARG);

    // Prepare AWS region
    awsRegion.pAwsRegion = pSignalingClient->pChannelInfo->pRegion;
    awsRegion.awsRegionLength = STRNLEN(pSignalingClient->pChannelInfo->pRegion, MAX_REGION_NAME_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Create the API url
    MEMSET(&createSignalingChannelRequestInfo, 0, SIZEOF(CreateSignalingChannelRequestInfo_t));
    createSignalingChannelRequestInfo.channelName.channelNameLength = STRNLEN(pSignalingClient->pChannelInfo->pChannelName, MAX_CHANNEL_NAME_LEN);
    createSignalingChannelRequestInfo.channelName.pChannelName = pSignalingClient->pChannelInfo->pChannelName;
    createSignalingChannelRequestInfo.messageTtlSeconds = pSignalingClient->pChannelInfo->messageTtl / HUNDREDS_OF_NANOS_IN_A_SECOND;
    createSignalingChannelRequestInfo.channelType = pSignalingClient->pChannelInfo->channelType == SIGNALING_TYPE_CHANNEL_SINGLE_MASTER
        ? SIGNALING_TYPE_CHANNEL_SINGLE_MASTER
        : SIGNALING_TYPE_CHANNEL_UNKNOWN;
    createSignalingChannelRequestInfo.numTags = pSignalingClient->pChannelInfo->tagCount;
    /* Format tags into structure buffer. */
    createSignalingChannelRequestInfo.pTags = tags;
    for (i = 0; i < pSignalingClient->pChannelInfo->tagCount; i++) {
        tags[i].nameLength = STRNLEN(pSignalingClient->pChannelInfo->pTags[i].name, MAX_TAG_NAME_LEN);
        tags[i].pName = pSignalingClient->pChannelInfo->pTags[i].name;
        tags[i].valueLength = STRNLEN(pSignalingClient->pChannelInfo->pTags[i].value, MAX_TAG_VALUE_LEN);
        tags[i].pValue = pSignalingClient->pChannelInfo->pTags[i].value;
    }
    retSignal = Signaling_ConstructCreateSignalingChannelRequest(&awsRegion, &createSignalingChannelRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse out the ARN
    retSignal = Signaling_ParseCreateSignalingChannelResponse(pResponseStr, resultLen, &channelArn);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    if (channelArn.pChannelArn != NULL) {
        CHK(channelArn.channelArnLength <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

        STRNCPY(pSignalingClient->channelDescription.channelArn, channelArn.pChannelArn, channelArn.channelArnLength);
        pSignalingClient->channelDescription.channelArn[channelArn.channelArnLength] = '\0';
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_SIGNALING_NO_ARN_RETURNED_ON_CREATE);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

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
    UINT32 resultLen;
    PCHAR pResponseStr;
    PLwsCallInfo pLwsCallInfo = NULL;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingAwsRegion_t awsRegion;
    GetSignalingChannelEndpointRequestInfo_t getSignalingChannelEndpointRequestInfo;
    SignalingChannelEndpoints_t channelEndpoints;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Prepare AWS region
    awsRegion.pAwsRegion = pSignalingClient->pChannelInfo->pRegion;
    awsRegion.awsRegionLength = STRNLEN(pSignalingClient->pChannelInfo->pRegion, MAX_REGION_NAME_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Create the API url
    getSignalingChannelEndpointRequestInfo.channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    getSignalingChannelEndpointRequestInfo.channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    getSignalingChannelEndpointRequestInfo.protocols = SIGNALING_PROTOCOL_HTTPS | SIGNALING_PROTOCOL_WEBSOCKET_SECURE;
    if (pSignalingClient->mediaStorageConfig.storageStatus != FALSE) {
        getSignalingChannelEndpointRequestInfo.protocols |= SIGNALING_PROTOCOL_WEBRTC;
    }
    CHK_STATUS(getChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType, &getSignalingChannelEndpointRequestInfo.role));
    retSignal = Signaling_ConstructGetSignalingChannelEndpointRequest(&awsRegion, &getSignalingChannelEndpointRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse and extract the endpoints
    retSignal = Signaling_ParseGetSignalingChannelEndpointResponse(pResponseStr, resultLen, &channelEndpoints);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    pSignalingClient->channelEndpointWss[0] = '\0';
    pSignalingClient->channelEndpointHttps[0] = '\0';
    pSignalingClient->channelEndpointWebrtc[0] = '\0';

    // Parse the response
    if (channelEndpoints.wssEndpoint.pEndpoint != NULL) {
        CHK(channelEndpoints.wssEndpoint.endpointLength <= MAX_SIGNALING_ENDPOINT_URI_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelEndpointWss, channelEndpoints.wssEndpoint.pEndpoint, channelEndpoints.wssEndpoint.endpointLength);
        pSignalingClient->channelEndpointWss[channelEndpoints.wssEndpoint.endpointLength] = '\0';
    }

    if (channelEndpoints.httpsEndpoint.pEndpoint != NULL) {
        CHK(channelEndpoints.httpsEndpoint.endpointLength <= MAX_SIGNALING_ENDPOINT_URI_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelEndpointHttps, channelEndpoints.httpsEndpoint.pEndpoint, channelEndpoints.httpsEndpoint.endpointLength);
        pSignalingClient->channelEndpointHttps[channelEndpoints.httpsEndpoint.endpointLength] = '\0';
    }

    if (channelEndpoints.webrtcEndpoint.pEndpoint != NULL) {
        CHK(channelEndpoints.webrtcEndpoint.endpointLength <= MAX_SIGNALING_ENDPOINT_URI_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingClient->channelEndpointWebrtc, channelEndpoints.webrtcEndpoint.pEndpoint, channelEndpoints.webrtcEndpoint.endpointLength);
        pSignalingClient->channelEndpointWebrtc[channelEndpoints.webrtcEndpoint.endpointLength] = '\0';
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0' && pSignalingClient->channelEndpointWss[0] != '\0',
        STATUS_SIGNALING_MISSING_ENDPOINTS_IN_GET_ENDPOINT);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

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
    UINT32 resultLen;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingChannelEndpoint_t httpsEndpoint;
    GetIceServerConfigRequestInfo_t getIceServerConfigRequestInfo;
    SignalingIceServer_t iceServers[MAX_ICE_CONFIG_COUNT];
    UINT64 numIceServers = MAX_ICE_CONFIG_COUNT;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0', STATUS_INTERNAL_ERROR);

    // Update the diagnostics info on the number of ICE refresh calls
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.iceRefreshCount);

    // Prepare HTTPS endpoint.
    httpsEndpoint.pEndpoint = pSignalingClient->channelEndpointHttps;
    httpsEndpoint.endpointLength = STRNLEN(pSignalingClient->channelEndpointHttps, MAX_SIGNALING_ENDPOINT_URI_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    getIceServerConfigRequestInfo.channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    getIceServerConfigRequestInfo.channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    getIceServerConfigRequestInfo.pClientId = pSignalingClient->clientInfo.signalingClientInfo.clientId;
    getIceServerConfigRequestInfo.clientIdLength = STRNLEN(pSignalingClient->clientInfo.signalingClientInfo.clientId, MAX_SIGNALING_CLIENT_ID_LEN);
    retSignal = Signaling_ConstructGetIceServerConfigRequest(&httpsEndpoint, &getIceServerConfigRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse the response
    retSignal = Signaling_ParseGetIceServerConfigResponse(pResponseStr, resultLen, iceServers, (size_t*) &numIceServers);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Parse and validate the response
    CHK_STATUS(updateIceServerList(pSignalingClient, iceServers, numIceServers));
    CHK_STATUS(validateIceConfiguration(pSignalingClient));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

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
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingAwsRegion_t awsRegion;
    DeleteSignalingChannelRequestInfo_t deleteSignalingChannelRequestInfo;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Check if we need to terminate the ongoing listener
    if (!ATOMIC_LOAD_BOOL(&pSignalingClient->listenerTracker.terminated) && pSignalingClient->pOngoingCallInfo != NULL &&
        pSignalingClient->pOngoingCallInfo->callInfo.pRequestInfo != NULL) {
        terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_OK);
    }

    // Prepare AWS region
    awsRegion.pAwsRegion = pSignalingClient->pChannelInfo->pRegion;
    awsRegion.awsRegionLength = STRNLEN(pSignalingClient->pChannelInfo->pRegion, MAX_REGION_NAME_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Create the API url
    deleteSignalingChannelRequestInfo.channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    deleteSignalingChannelRequestInfo.channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    deleteSignalingChannelRequestInfo.pVersion = pSignalingClient->channelDescription.updateVersion;
    deleteSignalingChannelRequestInfo.versionLength = STRNLEN(pSignalingClient->channelDescription.updateVersion, MAX_UPDATE_VERSION_LEN);
    retSignal = Signaling_ConstructDeleteSignalingChannelRequest(&awsRegion, &deleteSignalingChannelRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);

    // Early return if we have a non-success result and it's not a resource not found
    result = ATOMIC_LOAD(&pSignalingClient->result);
    CHK((SERVICE_CALL_RESULT) result == SERVICE_CALL_RESULT_OK || (SERVICE_CALL_RESULT) result == SERVICE_CALL_RESOURCE_NOT_FOUND,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    // Mark the channel as deleted
    ATOMIC_STORE_BOOL(&pSignalingClient->deleted, TRUE);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

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
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingChannelEndpoint_t wssEndpoint;
    ConnectWssEndpointRequestInfo_t connectWssEndpointRequestInfo;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointWss[0] != '\0', STATUS_INTERNAL_ERROR);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = NULL;
    signalRequest.bodyLength = 0;
    // Prepare WSS endpoint
    wssEndpoint.pEndpoint = pSignalingClient->channelEndpointWss;
    wssEndpoint.endpointLength = STRNLEN(pSignalingClient->channelEndpointWss, MAX_SIGNALING_ENDPOINT_URI_LEN);
    MEMSET(&connectWssEndpointRequestInfo, 0, SIZEOF(ConnectWssEndpointRequestInfo_t));
    connectWssEndpointRequestInfo.channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    connectWssEndpointRequestInfo.channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    CHK_STATUS(getChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType, &connectWssEndpointRequestInfo.role));
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        connectWssEndpointRequestInfo.pClientId = pSignalingClient->clientInfo.signalingClientInfo.clientId;
        connectWssEndpointRequestInfo.clientIdLength =
            STRNLEN(pSignalingClient->clientInfo.signalingClientInfo.clientId, MAX_SIGNALING_CLIENT_ID_LEN);
    }
    retSignal = Signaling_ConstructConnectWssEndpointRequest(&wssEndpoint, &connectWssEndpointRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

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

STATUS joinStorageSessionLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingChannelEndpoint_t webrtcEndpoint;
    JoinStorageSessionRequestInfo_t joinStorageSessionRequestInfo;

    UNUSED_PARAM(pResponseStr);
    UNUSED_PARAM(resultLen);

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointWebrtc[0] != '\0', STATUS_INTERNAL_ERROR);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Prepare WebRTC endpoint
    webrtcEndpoint.pEndpoint = pSignalingClient->channelEndpointWebrtc;
    webrtcEndpoint.endpointLength = STRNLEN(pSignalingClient->channelEndpointWebrtc, MAX_SIGNALING_ENDPOINT_URI_LEN);
    // Create the API url
    joinStorageSessionRequestInfo.channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    joinStorageSessionRequestInfo.channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    CHK_STATUS(getChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType, &joinStorageSessionRequestInfo.role));
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        joinStorageSessionRequestInfo.pClientId = pSignalingClient->clientInfo.signalingClientInfo.clientId;
        joinStorageSessionRequestInfo.clientIdLength =
            STRNLEN(pSignalingClient->clientInfo.signalingClientInfo.clientId, MAX_SIGNALING_CLIENT_ID_LEN);
    }
    retSignal = Signaling_ConstructJoinStorageSessionRequest(&webrtcEndpoint, &joinStorageSessionRequestInfo, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    // createRequestInfo does not have access to the getCurrentTime callback, this hook is used for tests.
    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS,
                                 &pLwsCallInfo)); //!< TBD. Accroding to the design document, the prefix of url will be webrtc://

    if (pSignalingClient->mediaStorageConfig.storageStatus) {
        pSignalingClient->diagnostics.joinSessionToOfferRecvTime = GETTIME();
    }
    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK, STATUS_SIGNALING_LWS_CALL_FAILED);

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelStatus != SIGNALING_CHANNEL_STATUS_DELETING, STATUS_SIGNALING_CHANNEL_BEING_DELETED);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS describeMediaStorageConfLws(PSignalingClient pSignalingClient, UINT64 time)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UNUSED_PARAM(time);

    PRequestInfo pRequestInfo = NULL;
    CHAR url[MAX_URI_CHAR_LEN + 1];
    CHAR paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;
    SignalingResult_t retSignal;
    SignalingRequest_t signalRequest;
    SignalingAwsRegion_t awsRegion;
    SignalingChannelArn_t channelArn;
    SignalingMediaStorageConfig_t mediaStorageConfig;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_INVALID_OPERATION);

    // Prepare AWS region
    awsRegion.pAwsRegion = pSignalingClient->pChannelInfo->pRegion;
    awsRegion.awsRegionLength = STRNLEN(pSignalingClient->pChannelInfo->pRegion, MAX_REGION_NAME_LEN);
    // Prepare URL buffer
    signalRequest.pUrl = &url[0];
    signalRequest.urlLength = MAX_URI_CHAR_LEN + 1;
    // Prepare body buffer
    signalRequest.pBody = &paramsJson[0];
    signalRequest.bodyLength = MAX_JSON_PARAMETER_STRING_LEN;
    // Create the API url
    channelArn.pChannelArn = pSignalingClient->channelDescription.channelArn;
    channelArn.channelArnLength = STRNLEN(pSignalingClient->channelDescription.channelArn, MAX_ARN_LEN);
    retSignal = Signaling_ConstructDescribeMediaStorageConfigRequest(&awsRegion, &channelArn, &signalRequest);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Create the request info with the body
    CHK_STATUS(createRequestInfo(url, paramsJson, pSignalingClient->pChannelInfo->pRegion, pSignalingClient->pChannelInfo->pCertPath, NULL, NULL,
                                 SSL_CERTIFICATE_TYPE_NOT_SPECIFIED, pSignalingClient->pChannelInfo->pUserAgent,
                                 SIGNALING_SERVICE_API_CALL_CONNECTION_TIMEOUT, SIGNALING_SERVICE_API_CALL_COMPLETION_TIMEOUT,
                                 DEFAULT_LOW_SPEED_LIMIT, DEFAULT_LOW_SPEED_TIME_LIMIT, pSignalingClient->pAwsCredentials, &pRequestInfo));

    // createRequestInfo does not have access to the getCurrentTime callback, this hook is used for tests.
    if (pSignalingClient->signalingClientCallbacks.getCurrentTimeFn != NULL) {
        pRequestInfo->currentTime =
            pSignalingClient->signalingClientCallbacks.getCurrentTimeFn(pSignalingClient->signalingClientCallbacks.customData);
    }

    checkAndCorrectForClockSkew(pSignalingClient, pRequestInfo);

    CHK_STATUS(createLwsCallInfo(pSignalingClient, pRequestInfo, PROTOCOL_INDEX_HTTPS, &pLwsCallInfo));

    // Make a blocking call
    CHK_STATUS(lwsCompleteSync(pLwsCallInfo));

    // Set the service call result
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) pLwsCallInfo->callInfo.callResult);
    pResponseStr = pLwsCallInfo->callInfo.responseData;
    resultLen = pLwsCallInfo->callInfo.responseDataLen;

    // Early return if we have a non-success result
    CHK((SERVICE_CALL_RESULT) ATOMIC_LOAD(&pSignalingClient->result) == SERVICE_CALL_RESULT_OK && resultLen != 0 && pResponseStr != NULL,
        STATUS_SIGNALING_LWS_CALL_FAILED);

    // Parse the response
    retSignal = Signaling_ParseDescribeMediaStorageConfigResponse(pResponseStr, resultLen, &mediaStorageConfig);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Parse the response
    MEMSET(&pSignalingClient->mediaStorageConfig, 0x00, SIZEOF(MediaStorageConfig));

    if (mediaStorageConfig.pStatus != NULL) {
        CHK(mediaStorageConfig.statusLength <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

        if (STRNCMP("ENABLED", mediaStorageConfig.pStatus, mediaStorageConfig.statusLength) == 0) {
            pSignalingClient->mediaStorageConfig.storageStatus = TRUE;
        } else {
            pSignalingClient->mediaStorageConfig.storageStatus = FALSE;
        }
    }

    if (mediaStorageConfig.pStreamArn != NULL) {
        CHK(mediaStorageConfig.streamArnLength <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

        STRNCPY(pSignalingClient->mediaStorageConfig.storageStreamArn, mediaStorageConfig.pStreamArn, mediaStorageConfig.streamArnLength);
        pSignalingClient->mediaStorageConfig.storageStreamArn[MAX_ARN_LEN] = '\0';
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelDescription.channelStatus != SIGNALING_CHANNEL_STATUS_DELETING, STATUS_SIGNALING_CHANNEL_BEING_DELETED);

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

    freeLwsCallInfo(&pLwsCallInfo);

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
    return (PVOID) (ULONG_PTR) retStatus;
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

    // Exit immediately if we are shutting down in case we are getting terminated while we were waiting for the
    // listener thread to terminate. The shutdown flag would have been checked prior kicking off the reconnect
    // thread but there is a slight chance of a race condition.
    CHK(!ATOMIC_LOAD_BOOL(&pSignalingClient->shutdown), retStatus);

    // Update the diagnostics info
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfReconnects);

    // Attempt to reconnect by driving the state machine to connected state
    CHK_STATUS(signalingStateMachineIterator(pSignalingClient, SIGNALING_GET_CURRENT_TIME(pSignalingClient) + SIGNALING_CONNECT_STATE_TIMEOUT,
                                             pSignalingClient->mediaStorageConfig.storageStatus ? SIGNALING_STATE_JOIN_SESSION_CONNECTED
                                                                                                : SIGNALING_STATE_CONNECTED));

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
    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS sendLwsMessage(PSignalingClient pSignalingClient, SIGNALING_MESSAGE_TYPE messageType, PCHAR peerClientId, PCHAR pMessage, UINT32 messageLen,
                      PCHAR pCorrelationId, UINT32 correlationIdLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    CHAR encodedMessage[MAX_SESSION_DESCRIPTION_INIT_SDP_LEN + 1];
    UINT32 size, writtenSize, correlationLen;
    BOOL awaitForResponse;
    SignalingResult_t retSignal;
    WssSendMessage_t wssSendMessage;
    SIZE_T bufferSize;

    // Ensure we are in a connected state
    CHK_STATUS(acceptSignalingStateMachineState(pSignalingClient, SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION_CONNECTED));

    CHK(pSignalingClient != NULL && pSignalingClient->pOngoingCallInfo != NULL, STATUS_NULL_ARG);

    MEMSET(&wssSendMessage, 0, SIZEOF(WssSendMessage_t));

    // Prepare the buffer to send
    switch (messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            wssSendMessage.messageType = SIGNALING_TYPE_MESSAGE_SDP_OFFER;
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            wssSendMessage.messageType = SIGNALING_TYPE_MESSAGE_SDP_ANSWER;
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            wssSendMessage.messageType = SIGNALING_TYPE_MESSAGE_ICE_CANDIDATE;
            break;
        default:
            CHK(FALSE, STATUS_INVALID_ARG);
    }
    DLOGD("%s", pMessage);
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

    /* Prepare send message. */
    wssSendMessage.base64EncodedMessageLength = writtenSize;
    wssSendMessage.pBase64EncodedMessage = encodedMessage;
    wssSendMessage.correlationIdLength = correlationLen;
    wssSendMessage.pCorrelationId = pCorrelationId;
    wssSendMessage.recipientClientIdLength = STRNLEN(peerClientId, MAX_SIGNALING_CLIENT_ID_LEN);
    wssSendMessage.pRecipientClientId = peerClientId;

    // Prepare json message
    bufferSize = SIZEOF(pSignalingClient->pOngoingCallInfo->sendBuffer) - LWS_PRE - 1; /* -1 for null terminator. */
    retSignal =
        Signaling_ConstructWssMessage(&wssSendMessage, (PCHAR) (pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE), (size_t*) &bufferSize);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    // Validate against max
    CHK(bufferSize + LWS_PRE <= LWS_MESSAGE_BUFFER_SIZE, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);

    // Set null terminator to make sure sendBuffer is a valid string.
    *(pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE + bufferSize) = '\0';

    writtenSize = bufferSize * SIZEOF(CHAR);
    CHK(writtenSize <= size, STATUS_INVALID_ARG);

    // Store the data pointer
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendBufferSize, LWS_PRE + writtenSize);
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendOffset, LWS_PRE);

    // Send the data to the web socket
    awaitForResponse = (correlationLen != 0) && BLOCK_ON_CORRELATION_ID;

    DLOGD("Sending data over web socket: Message type: %d, RecepientId: %s", wssSendMessage.messageType, peerClientId);

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

    // Initialize the send result to none
    ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_RESULT_NOT_SET);

    // Wake up the service event loop
    CHK_STATUS(wakeLwsServiceEventLoop(pSignalingClient, PROTOCOL_INDEX_WSS));

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
    UINT32 outLen = MAX_SIGNALING_MESSAGE_LEN;
    PSignalingMessageWrapper pSignalingMessageWrapper = NULL;
    TID receivedTid = INVALID_TID_VALUE;
    BOOL parsedMessageType = FALSE;
    PSignalingMessage pOngoingMessage;
    SignalingResult_t retSignal;
    WssRecvMessage_t wssRecvMessage;
    UINT64 messageLength = messageLen;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    // If we have a signalingMessage and if there is a correlation id specified then the response should be non-empty
    if (pMessage == NULL || messageLength == 0) {
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
        CHK_WARN(pMessage != NULL && messageLength != 0, retStatus, "Signaling received an empty message");
    }

    // DLOGV("receive LWS Message:\n%s", pMessage);
    MEMSET(&wssRecvMessage, 0, SIZEOF(WssRecvMessage_t));
    // Parse the response
    retSignal = Signaling_ParseWssRecvMessage(pMessage, (SIZE_T) messageLength, &wssRecvMessage);
    CHK(retSignal == SIGNALING_RESULT_OK, retSignal);

    CHK(NULL != (pSignalingMessageWrapper = (PSignalingMessageWrapper) MEMCALLOC(1, SIZEOF(SignalingMessageWrapper))), STATUS_NOT_ENOUGH_MEMORY);

    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;

    if (wssRecvMessage.pSenderClientId != NULL) {
        CHK(wssRecvMessage.senderClientIdLength <= MAX_SIGNALING_CLIENT_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId, wssRecvMessage.pSenderClientId,
                wssRecvMessage.senderClientIdLength);
        pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId[wssRecvMessage.senderClientIdLength] = '\0';
    }

    if (wssRecvMessage.messageType != SIGNALING_TYPE_MESSAGE_UNKNOWN) {
        CHK_STATUS(getMessageType(wssRecvMessage.messageType, &pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType));

        parsedMessageType = TRUE;
    }

    if (wssRecvMessage.pBase64EncodedPayload != NULL) {
        // Base64 decode the message
        CHK(wssRecvMessage.base64EncodedPayloadLength <= MAX_SIGNALING_MESSAGE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        CHK_STATUS(base64Decode((PCHAR) wssRecvMessage.pBase64EncodedPayload, wssRecvMessage.base64EncodedPayloadLength,
                                (PBYTE) (pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload), &outLen));
        pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payload[wssRecvMessage.base64EncodedPayloadLength] = '\0';
        pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.payloadLen = outLen;
    }

    if (wssRecvMessage.statusResponse.pCorrelationId != NULL) {
        CHK(wssRecvMessage.statusResponse.correlationIdLength <= MAX_CORRELATION_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId, wssRecvMessage.statusResponse.pCorrelationId,
                wssRecvMessage.statusResponse.correlationIdLength);
        pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.correlationId[wssRecvMessage.statusResponse.correlationIdLength] = '\0';
    }

    if (wssRecvMessage.statusResponse.pErrorType != NULL) {
        CHK(wssRecvMessage.statusResponse.errorTypeLength <= MAX_ERROR_TYPE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.errorType, wssRecvMessage.statusResponse.pErrorType,
                wssRecvMessage.statusResponse.errorTypeLength);
        pSignalingMessageWrapper->receivedSignalingMessage.errorType[wssRecvMessage.statusResponse.errorTypeLength] = '\0';
    }

    if (wssRecvMessage.statusResponse.pStatusCode != NULL) {
        CHK(wssRecvMessage.statusResponse.statusCodeLength <= MAX_STATUS_CODE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

        // Parse the status code
        CHK_STATUS(STRTOUI32((PCHAR) wssRecvMessage.statusResponse.pStatusCode,
                             ((PCHAR) wssRecvMessage.statusResponse.pStatusCode) + wssRecvMessage.statusResponse.statusCodeLength, 10,
                             &pSignalingMessageWrapper->receivedSignalingMessage.statusCode));
    }

    if (wssRecvMessage.statusResponse.pDescription != NULL) {
        CHK(wssRecvMessage.statusResponse.descriptionLength <= MAX_MESSAGE_DESCRIPTION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
        STRNCPY(pSignalingMessageWrapper->receivedSignalingMessage.description, wssRecvMessage.statusResponse.pDescription,
                wssRecvMessage.statusResponse.descriptionLength);
        pSignalingMessageWrapper->receivedSignalingMessage.description[wssRecvMessage.statusResponse.descriptionLength] = '\0';
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
            CHK_STATUS(signalingStateMachineIterator(pSignalingClient, SIGNALING_GET_CURRENT_TIME(pSignalingClient) + SIGNALING_CONNECT_STATE_TIMEOUT,
                                                     pSignalingClient->mediaStorageConfig.storageStatus ? SIGNALING_STATE_JOIN_SESSION_CONNECTED
                                                                                                        : SIGNALING_STATE_CONNECTED));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            // Move to get ice config state
            CHK_STATUS(terminateConnectionWithStatus(pSignalingClient, SERVICE_CALL_RESULT_SIGNALING_RECONNECT_ICE));

            // Delete the message wrapper and exit
            SAFE_MEMFREE(pSignalingMessageWrapper);

            // Iterate the state machinery
            CHK_STATUS(signalingStateMachineIterator(pSignalingClient, SIGNALING_GET_CURRENT_TIME(pSignalingClient) + SIGNALING_CONNECT_STATE_TIMEOUT,
                                                     pSignalingClient->mediaStorageConfig.storageStatus ? SIGNALING_STATE_JOIN_SESSION_CONNECTED
                                                                                                        : SIGNALING_STATE_CONNECTED));

            CHK(FALSE, retStatus);
            break;

        case SIGNALING_MESSAGE_TYPE_OFFER:
            if (!pSignalingClient->mediaStorageConfig.storageStatus) {
                CHK(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.peerClientId[0] != '\0',
                    STATUS_SIGNALING_NO_PEER_CLIENT_ID_IN_MESSAGE);
            }
            //  Explicit fall-through !!!
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

    DLOGD("Client received message of type: %s",
          getMessageTypeInString(pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType));

#ifdef ENABLE_KVS_THREADPOOL
    // This would fail if threadpool was not created
    CHK_STATUS(threadpoolContextPush(receiveLwsMessageWrapper, pSignalingMessageWrapper));
#else
    // Issue the callback on a separate thread
    CHK_STATUS(THREAD_CREATE(&receivedTid, receiveLwsMessageWrapper, (PVOID) pSignalingMessageWrapper));
    CHK_STATUS(THREAD_DETACH(receivedTid));
#endif

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
    UINT32 i;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pSignalingClient->connected, FALSE);
    CVAR_BROADCAST(pSignalingClient->connectedCvar);
    CVAR_BROADCAST(pSignalingClient->receiveCvar);
    CVAR_BROADCAST(pSignalingClient->sendCvar);
    CVAR_BROADCAST(pSignalingClient->jssWaitCvar);
    ATOMIC_STORE(&pSignalingClient->messageResult, (SIZE_T) SERVICE_CALL_UNKNOWN);
    ATOMIC_STORE(&pSignalingClient->result, (SIZE_T) callResult);

    if (pSignalingClient->pOngoingCallInfo != NULL) {
        ATOMIC_STORE_BOOL(&pSignalingClient->pOngoingCallInfo->cancelService, TRUE);
    }

    // Wake up the service event loop for all of the protocols
    for (i = 0; i < LWS_PROTOCOL_COUNT; i++) {
        CHK_STATUS(wakeLwsServiceEventLoop(pSignalingClient, i));
    }

    CHK_STATUS(awaitForThreadTermination(&pSignalingClient->listenerTracker, SIGNALING_CLIENT_SHUTDOWN_TIMEOUT));

CleanUp:

    LEAVES();
    return retStatus;
}

PCHAR getMessageTypeInString(SIGNALING_MESSAGE_TYPE messageType)
{
    switch (messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            return SIGNALING_SDP_TYPE_OFFER;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            return SIGNALING_SDP_TYPE_ANSWER;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            return SIGNALING_ICE_CANDIDATE;
        case SIGNALING_MESSAGE_TYPE_GO_AWAY:
            return SIGNALING_GO_AWAY;
        case SIGNALING_MESSAGE_TYPE_RECONNECT_ICE_SERVER:
            return SIGNALING_RECONNECT_ICE_SERVER;
        case SIGNALING_MESSAGE_TYPE_STATUS_RESPONSE:
            return SIGNALING_STATUS_RESPONSE;
        case SIGNALING_MESSAGE_TYPE_UNKNOWN:
            return SIGNALING_MESSAGE_UNKNOWN;
    }
    return SIGNALING_MESSAGE_UNKNOWN;
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
    SIGNALING_MESSAGE_TYPE messageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;

    CHK(pSignalingMessageWrapper != NULL, STATUS_NULL_ARG);

    messageType = pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.messageType;

    pSignalingClient = pSignalingMessageWrapper->pSignalingClient;

    CHK(pSignalingClient != NULL, STATUS_INTERNAL_ERROR);

    // Updating the diagnostics info before calling the client callback
    ATOMIC_INCREMENT(&pSignalingClient->diagnostics.numberOfMessagesReceived);

    if (messageType == SIGNALING_MESSAGE_TYPE_OFFER) {
        MUTEX_LOCK(pSignalingClient->offerSendReceiveTimeLock);
        pSignalingClient->offerReceivedTime = GETTIME();
        MUTEX_UNLOCK(pSignalingClient->offerSendReceiveTimeLock);

        if (pSignalingClient->mediaStorageConfig.storageStatus) {
            MUTEX_LOCK(pSignalingClient->jssWaitLock);
            ATOMIC_STORE_BOOL(&pSignalingClient->offerReceived, TRUE);
            DLOGI("Offer Received from JoinStorageSession Call.");
            pSignalingClient->diagnostics.joinSessionToOfferRecvTime =
                pSignalingClient->offerReceivedTime - pSignalingClient->diagnostics.joinSessionToOfferRecvTime;
            CVAR_BROADCAST(pSignalingClient->jssWaitCvar);
            MUTEX_UNLOCK(pSignalingClient->jssWaitLock);
        }
    } else if (messageType == SIGNALING_MESSAGE_TYPE_ANSWER) {
        MUTEX_LOCK(pSignalingClient->offerSendReceiveTimeLock);
        PROFILE_WITH_START_TIME_OBJ(pSignalingClient->offerSentTime, pSignalingClient->diagnostics.offerToAnswerTime,
                                    "Offer Sent to Answer Received time");
        MUTEX_UNLOCK(pSignalingClient->offerSendReceiveTimeLock);
    }
    // Calling client receive message callback if specified
    if (pSignalingClient->signalingClientCallbacks.messageReceivedFn != NULL) {
        CHK_STATUS(pSignalingClient->signalingClientCallbacks.messageReceivedFn(pSignalingClient->signalingClientCallbacks.customData,
                                                                                &pSignalingMessageWrapper->receivedSignalingMessage));
    }

CleanUp:
    CHK_LOG_ERR(retStatus);

    SAFE_MEMFREE(pSignalingMessageWrapper);

    return (PVOID) (ULONG_PTR) retStatus;
}

STATUS wakeLwsServiceEventLoop(PSignalingClient pSignalingClient, UINT32 protocolIndex)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    // Early exit in case we don't need to do anything
    CHK(pSignalingClient != NULL && pSignalingClient->pLwsContext != NULL, retStatus);

    if (pSignalingClient->currentWsi[protocolIndex] != NULL) {
        lws_callback_on_writable(pSignalingClient->currentWsi[protocolIndex]);
    }

CleanUp:

    LEAVES();
    return retStatus;
}
