/**
 * Implementation of a API calls based on LibWebSocket
 */
#define LOG_CLASS "LwsApiCalls"
#include "../Include_i.h"

#include <libwebsockets.h>
#include "Signaling.h"
#include "LwsApiCalls.h"

#ifdef KVS_USE_OPENSSL
#include <openssl/pem.h>
#endif

#define WEBRTC_SCHEME_NAME "webrtc"

static BOOL gInterruptedFlagBySignalHandler;
VOID lwsSignalHandler(INT32 signal)
{
    UNUSED_PARAM(signal);
    gInterruptedFlagBySignalHandler = TRUE;
}

#ifdef KVS_USE_AWS_LC
/**
 * Map a signaling request URL to a human-readable label for the PQC log line, so
 * each TLS handshake is attributable to the API that caused it.
 *
 * Lives here rather than in the crypto layer because the *_API_POSTFIX macros
 * are signaling-specific.
 */
static PCHAR lwsPqcApiLabel(PCHAR pUrl)
{
    if (IS_NULL_OR_EMPTY_STRING(pUrl)) {
        return (PCHAR) "https";
    }

    /* DescribeMediaStorageConfiguration is checked before DescribeSignalingChannel
     * so the shorter name does not shadow it. */
    if (STRSTR(pUrl, DESCRIBE_MEDIA_STORAGE_CONF_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane DescribeMediaStorageConfiguration";
    } else if (STRSTR(pUrl, UPDATE_MEDIA_STORAGE_CONF_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane UpdateMediaStorageConfiguration";
    } else if (STRSTR(pUrl, DESCRIBE_SIGNALING_CHANNEL_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane DescribeSignalingChannel";
    } else if (STRSTR(pUrl, CREATE_SIGNALING_CHANNEL_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane CreateSignalingChannel";
    } else if (STRSTR(pUrl, GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane GetSignalingChannelEndpoint";
    } else if (STRSTR(pUrl, DELETE_SIGNALING_CHANNEL_API_POSTFIX) != NULL) {
        return (PCHAR) "control-plane DeleteSignalingChannel";
    } else if (STRSTR(pUrl, GET_ICE_CONFIG_API_POSTFIX) != NULL) {
        return (PCHAR) "data-plane GetIceServerConfig";
    } else if (STRSTR(pUrl, JOIN_STORAGE_SESSION_API_POSTFIX) != NULL) {
        return (PCHAR) "data-plane JoinStorageSession";
    }

    return (PCHAR) "https";
}
#endif

#ifdef KVS_USE_OPENSSL
/**
 * Add every certificate in the configured CA file to the libwebsockets client trust store.
 *
 * libwebsockets is handed the CA file as client_ssl_ca_mem (Signaling.c), and on the
 * OpenSSL/AWS-LC backend lws 4.x decodes only the FIRST PEM block of that buffer: it
 * runs one d2i_X509, builds a fresh X509_STORE holding that single cert, and installs it
 * with SSL_CTX_set_cert_store (lib/tls/openssl/openssl-client.c). Any further
 * certificates in the file are silently ignored. That is invisible against production,
 * where every KVS host chains to the same public root, but breaks as soon as the control
 * plane and the per-channel endpoints chain to different roots, e.g. a pre-production
 * control plane behind an internal CA whose r-/m-/v- hosts still use a public one.
 *
 * This runs from LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS, which lws raises
 * right after installing that one-cert store, and adds the rest of the file to it. The
 * store lws installed has already discarded any OS trust anchors, so the effective trust
 * set stays exactly "the certificates in pCertPath", only now all of them.
 *
 * Failures are logged and swallowed: the first certificate is already trusted, so the
 * worst case is the pre-existing single-cert behavior.
 */
static VOID lwsLoadCaBundle(SSL_CTX* pSslCtx, PCHAR pCertPath)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pCertBuf = NULL;
    UINT64 certLen = 0;
    BIO* pBio = NULL;
    X509* pCert = NULL;
    X509_STORE* pStore = NULL;
    UINT32 added = 0, seen = 0;

    CHK(pSslCtx != NULL && !IS_NULL_OR_EMPTY_STRING(pCertPath), STATUS_NULL_ARG);

    pStore = SSL_CTX_get_cert_store(pSslCtx);
    CHK(pStore != NULL, STATUS_INVALID_OPERATION);

    CHK_STATUS(readFile(pCertPath, FALSE, NULL, &certLen));
    CHK(certLen > 0 && certLen <= MAX_UINT32, STATUS_INVALID_CERT_PATH_LENGTH);
    pCertBuf = (PBYTE) MEMALLOC(certLen);
    CHK(pCertBuf != NULL, STATUS_NOT_ENOUGH_MEMORY);
    CHK_STATUS(readFile(pCertPath, FALSE, pCertBuf, &certLen));

    pBio = BIO_new_mem_buf(pCertBuf, (int) certLen);
    CHK(pBio != NULL, STATUS_NOT_ENOUGH_MEMORY);

    while ((pCert = PEM_read_bio_X509(pBio, NULL, NULL, NULL)) != NULL) {
        seen++;
        // Duplicates of the certificate lws already loaded are expected and harmless;
        // both OpenSSL 1.1+ and AWS-LC return success for an exact duplicate.
        if (X509_STORE_add_cert(pStore, pCert) == 1) {
            added++;
        } else {
            DLOGW("Certificate %u in %s could not be added to the TLS trust store", seen, pCertPath);
        }
        X509_free(pCert);
        pCert = NULL;
    }
    // PEM_read_bio_X509 sets PEM_R_NO_START_LINE when it runs off the end of the buffer.
    // That is the normal loop exit, not an error worth surfacing.
    ERR_clear_error();

    if (seen > 1) {
        DLOGI("Loaded %u of %u certificates from %s into the signaling TLS trust store", added, seen, pCertPath);
    } else {
        DLOGD("CA file %s holds a single certificate; nothing beyond what libwebsockets already loaded", pCertPath);
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGW("Could not load additional CA certificates from %s (0x%08x); continuing with the first certificate only", pCertPath, retStatus);
    }

    if (pBio != NULL) {
        BIO_free(pBio);
    }
    SAFE_MEMFREE(pCertBuf);
}
#endif

INT32 lwsHttpCallbackRoutine(PVOID wsi, INT32 reason, PVOID user, PVOID pDataIn, size_t dataSize)
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
    PRequestHeader pRequestHeader;
    PSignalingClient pSignalingClient = NULL;
    BOOL locked = FALSE;
    time_t td;
    SIZE_T len;
    UINT64 nowTime, clockSkew = 0;
    PStateMachineState pStateMachineState;
    BOOL skewMapContains = FALSE;

    DLOGV("HTTPS callback with reason %d", reason);

#ifdef KVS_USE_OPENSSL
    /*
     * Handled before the filter below and before the opaque-user-data lookup:
     * libwebsockets raises this while building the client SSL_CTX, at which point
     * no wsi user data exists yet. `user` is the SSL_CTX per the lws docs, and
     * `wsi` is a stub whose only guarantee is that lws_get_context() works.
     * The context user pointer is the signaling client (Signaling.c sets it).
     */
    if (reason == LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS && user != NULL) {
        if (wsi != NULL) {
            pSignalingClient = (PSignalingClient) lws_context_user(lws_get_context((struct lws*) wsi));
        }
        if (pSignalingClient != NULL && pSignalingClient->pChannelInfo != NULL) {
            lwsLoadCaBundle((SSL_CTX*) user, pSignalingClient->pChannelInfo->pCertPath);
        } else {
            DLOGW("Signaling client not reachable from the libwebsockets context; only the first CA certificate will be trusted");
        }
        pSignalingClient = NULL;

#ifdef KVS_USE_AWS_LC
        /*
         * This is where the post-quantum group list gets applied to the libwebsockets
         * client context. Without it the context would silently inherit AWS-LC's
         * defaults, which happen to lead with X25519MLKEM768 today but are not a
         * contract. Configuring explicitly makes the offer deterministic and means
         * the log line below can state what was offered rather than guess.
         */
        if (STATUS_FAILED(awsLcConfigureTlsGroups((SSL_CTX*) user, TRUE))) {
            DLOGW("Failed to configure post-quantum groups on the libwebsockets client context; continuing with AWS-LC defaults");
        } else {
            // Record success so the per-handshake log lines can report what was
            // actually offered instead of asserting it. One vhost/SSL_CTX is
            // shared by both protocols, so this covers HTTPS and WSS alike.
            awsLcSetPqcConfigured(TRUE);
        }
#endif
        return 0;
    }
#endif

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

    customData = lws_get_opaque_user_data((struct lws*) wsi);
    pLwsCallInfo = (PLwsCallInfo) customData;

    CHK_STATUS(configureLwsLogging(loggerGetLogLevel()));

    CHK(pLwsCallInfo != NULL && pLwsCallInfo->pSignalingClient != NULL && pLwsCallInfo->pSignalingClient->pWebsocketContext != NULL &&
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
            status = lws_http_client_http_response((struct lws*) wsi);
            getStateMachineCurrentState(pSignalingClient->pStateMachine, &pStateMachineState);

#ifdef KVS_USE_AWS_LC
            // TLS is complete by the time the HTTP response is available, so the
            // negotiated group is final here. Labelled per API so control-plane
            // and data-plane calls are distinguishable in the log.
            awsLcRecordNegotiatedGroup((SSL*) lws_get_ssl((struct lws*) wsi), pLwsCallInfo->pPqcApiLabel);
#endif

            DLOGD("Connected with server response: %d", status);
            pLwsCallInfo->callInfo.callResult = getServiceCallResultFromHttpStatus((UINT32) status);

            len = (SIZE_T) lws_hdr_copy((struct lws*) wsi, &dateHdrBuffer[0], MAX_DATE_HEADER_BUFFER_LENGTH, WSI_TOKEN_HTTP_DATE);

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
            if ((size = lws_hdr_custom_copy((struct lws*) wsi, pBuffer, LWS_SCRATCH_BUFFER_SIZE, SIGNALING_REQUEST_ID_HEADER_NAME,
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

            if (lws_http_client_read((struct lws*) wsi, &pBuffer, &size) < 0) {
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

                status = lws_add_http_header_by_name((struct lws*) wsi, (PBYTE) pRequestHeader->pName, (PBYTE) pRequestHeader->pValue,
                                                     pRequestHeader->valueLen, ppStartPtr, pEndPtr);
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

            lws_client_http_body_pending((struct lws*) wsi, 1);
            lws_callback_on_writable((struct lws*) wsi);

            break;

        case LWS_CALLBACK_CLIENT_HTTP_WRITEABLE:
            DLOGD("Sending the body %.*s, size %d", pRequestInfo->bodySize, pRequestInfo->body, pRequestInfo->bodySize);
            MEMCPY(pBuffer, pRequestInfo->body, pRequestInfo->bodySize);

            // HTTP body must use LWS_WRITE_HTTP; the h2 role drops LWS_WRITE_TEXT (http/1.1 tolerated it).
            size = lws_write((struct lws*) wsi, (PBYTE) pBuffer, (SIZE_T) pRequestInfo->bodySize, LWS_WRITE_HTTP);

            if (size != (INT32) pRequestInfo->bodySize) {
                DLOGW("Failed to write out the body of POST request entirely. Expected to write %d, wrote %d", pRequestInfo->bodySize, size);
                if (size > 0) {
                    // Schedule again
                    lws_client_http_body_pending((struct lws*) wsi, 1);
                    lws_callback_on_writable((struct lws*) wsi);
                } else {
                    // Quit
                    retValue = 1;
                }
            } else {
                lws_client_http_body_pending((struct lws*) wsi, 0);
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

        lws_cancel_service(lws_get_context((struct lws*) wsi));

        retValue = -1;
    }

    if (locked) {
        MUTEX_UNLOCK(pSignalingClient->lwsServiceLock);
    }

    return retValue;
}

INT32 lwsWssCallbackRoutine(PVOID wsi, INT32 reason, PVOID user, PVOID pDataIn, size_t dataSize)
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

    // NOTE: there is deliberately no LWS_CALLBACK_OPENSSL_LOAD_EXTRA_CLIENT_VERIFY_CERTS
    // handler here. libwebsockets raises that callback on protocols[0] only
    // (lws_context_init_client_ssl in lib/tls/tls-client.c calls
    // vhost->protocols[0].callback directly), and PROTOCOL_INDEX_HTTPS is 0, so it
    // always lands in lwsHttpCallbackRoutine. Verified by instrumenting both
    // routines: the hook fires exactly once, in the HTTPS routine. The SDK creates
    // a single lws context with one vhost, so HTTPS and WSS share one client
    // SSL_CTX and the group list configured there covers this connection too.

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

    customData = lws_get_opaque_user_data((struct lws*) wsi);
    pLwsCallInfo = (PLwsCallInfo) customData;

    CHK_STATUS(configureLwsLogging(loggerGetLogLevel()));

    CHK(pLwsCallInfo != NULL && pLwsCallInfo->pSignalingClient != NULL && pLwsCallInfo->pSignalingClient->pOngoingCallInfo != NULL &&
            pLwsCallInfo->pSignalingClient->pWebsocketContext != NULL &&
            pLwsCallInfo->pSignalingClient->pOngoingCallInfo->callInfo.pRequestInfo != NULL && pLwsCallInfo->protocolIndex == PROTOCOL_INDEX_WSS,
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

#ifdef KVS_USE_AWS_LC
            // The WebSocket upgrade has completed, so the TLS handshake beneath it
            // is final. This is the long-lived signaling connection (ConnectAsMaster
            // / ConnectAsViewer).
            awsLcRecordNegotiatedGroup((SSL*) lws_get_ssl((struct lws*) wsi), (PCHAR) "signaling WSS");
#endif

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
            if (pLwsCallInfo->receiveBufferSize + (UINT32) dataSize + LWS_PRE > SIZEOF(pLwsCallInfo->receiveBuffer)) {
                DLOGE("Signaling message size (%u bytes so far) exceeds receive buffer (%u bytes). "
                      "Increase KVS_SIGNALING_MESSAGE_LEN in CMake to accommodate larger messages.",
                      pLwsCallInfo->receiveBufferSize + (UINT32) dataSize, (UINT32) SIZEOF(pLwsCallInfo->receiveBuffer) - LWS_PRE);
            }
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

    pContext = (struct lws_context*) pCallInfo->pSignalingClient->pWebsocketContext;

    // Execute the LWS REST call
    MEMSET(&connectInfo, 0x00, SIZEOF(struct lws_client_connect_info));
    connectInfo.context = pContext;
    connectInfo.ssl_connection = LCCSCF_USE_SSL;
    connectInfo.port = SIGNALING_DEFAULT_SSL_PORT;

#ifdef KVS_USE_AWS_LC
    // Capture this before the in-place host termination below destroys the path.
    pCallInfo->pPqcApiLabel = lwsPqcApiLabel(pCallInfo->callInfo.pRequestInfo->url);
#endif

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
    // The REST/WSS plumbing here is HTTP/1.1-only. lws v5 offers "h2,http/1.1" by
    // default and the per-channel endpoints negotiate h2, then reject the request
    // (HTML 400 on JoinStorageSession/GetIceServerConfig), so pin ALPN to http/1.1.
    connectInfo.alpn = "http/1.1";
    connectInfo.protocol = ((struct lws_protocols*) pCallInfo->pSignalingClient->signalingProtocols[pCallInfo->protocolIndex])->name;
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
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelName")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_CHANNEL_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.channelName, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.channelName[MAX_CHANNEL_NAME_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Version")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_UPDATE_VERSION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                STRNCPY(pSignalingClient->channelDescription.updateVersion, pResponseStr + tokens[i + 1].start, strLen);
                pSignalingClient->channelDescription.updateVersion[MAX_UPDATE_VERSION_LEN] = '\0';
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelStatus")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_DESCRIBE_CHANNEL_STATUS_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                pSignalingClient->channelDescription.channelStatus = getChannelStatusFromString(pResponseStr + tokens[i + 1].start, strLen);
                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelType")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
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

    DLOGD("=== DescribeMediaStorageConfiguration COMPLETE ===");
    DLOGD("Final storage status: %s (%d)", pSignalingClient->mediaStorageConfig.storageStatus ? "ENABLED" : "DISABLED",
          pSignalingClient->mediaStorageConfig.storageStatus);
    DLOGD("Final storage stream ARN: %s", pSignalingClient->mediaStorageConfig.storageStreamArn);
    DLOGD("Endpoint cache usable: %s",
          (IS_VALID_TIMESTAMP(pSignalingClient->getEndpointTime) &&
           SIGNALING_GET_CURRENT_TIME(pSignalingClient) <= pSignalingClient->getEndpointTime + pSignalingClient->pChannelInfo->cachingPeriod)
              ? "YES"
              : "NO (will fetch fresh endpoints)");

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
            CHK(charsCopied > 0 && charsCopied < MAX_JSON_PARAMETER_STRING_LEN - (pCurPtr - pTagsStart), STATUS_INTERNAL_ERROR);
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
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ChannelARN")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            STRNCPY(pSignalingClient->channelDescription.channelArn, pResponseStr + tokens[i + 1].start, strLen);
            pSignalingClient->channelDescription.channelArn[MAX_ARN_LEN] = '\0';
            i++;
        }
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
    UINT32 i, resultLen, strLen, protocolLen = 0, endpointLen = 0;
    PCHAR pResponseStr, pProtocol = NULL, pEndpoint = NULL;
    PLwsCallInfo pLwsCallInfo = NULL;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 tokenCount;
    BOOL jsonInResourceEndpointList = FALSE, protocol = FALSE, endpoint = FALSE, inEndpointArray = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    DLOGD("=== GetChannelEndpoint START ===");
    DLOGD("Storage status: %s (%d)", pSignalingClient->mediaStorageConfig.storageStatus ? "ENABLED" : "DISABLED",
          pSignalingClient->mediaStorageConfig.storageStatus);

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, GET_SIGNALING_CHANNEL_ENDPOINT_API_POSTFIX);

    // Prepare the json params for the call
    if (pSignalingClient->mediaStorageConfig.storageStatus == FALSE) {
        DLOGD("Requesting protocols: WSS, HTTPS (storage disabled - no WEBRTC)");
        SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
    } else {
        DLOGD("Requesting protocols: WSS, HTTPS, WEBRTC (storage enabled)");
        SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), GET_CHANNEL_ENDPOINT_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn,
                 SIGNALING_CHANNEL_PROTOCOL_W_MEDIA_STORAGE, getStringFromChannelRoleType(pSignalingClient->pChannelInfo->channelRoleType));
    }
    DLOGD("Request params: %s", paramsJson);

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
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    pSignalingClient->channelEndpointWss[0] = '\0';
    pSignalingClient->channelEndpointHttps[0] = '\0';
    pSignalingClient->channelEndpointWebrtc[0] = '\0';

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
                        } else if (0 == STRNCMPI(pProtocol, WEBRTC_SCHEME_NAME, protocolLen)) {
                            STRNCPY(pSignalingClient->channelEndpointWebrtc, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
                            pSignalingClient->channelEndpointWebrtc[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
                        }
                    }

                    protocol = FALSE;
                    endpoint = FALSE;
                    protocolLen = 0;
                    endpointLen = 0;
                    pProtocol = NULL;
                    pEndpoint = NULL;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Protocol")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    pProtocol = pResponseStr + tokens[i + 1].start;
                    protocolLen = strLen;
                    protocol = TRUE;
                    i++;
                } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "ResourceEndpoint")) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_SIGNALING_ENDPOINT_URI_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
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
        } else if (0 == STRNCMPI(pProtocol, WEBRTC_SCHEME_NAME, protocolLen)) {
            STRNCPY(pSignalingClient->channelEndpointWebrtc, pEndpoint, MIN(endpointLen, MAX_SIGNALING_ENDPOINT_URI_LEN));
            pSignalingClient->channelEndpointWebrtc[MAX_SIGNALING_ENDPOINT_URI_LEN] = '\0';
        }
    }

    // Perform some validation on the channel description
    CHK(pSignalingClient->channelEndpointHttps[0] != '\0' && pSignalingClient->channelEndpointWss[0] != '\0',
        STATUS_SIGNALING_MISSING_ENDPOINTS_IN_GET_ENDPOINT);

    DLOGD("=== GetChannelEndpoint COMPLETE ===");
    DLOGD("Retrieved endpoints:");
    DLOGD("  HTTPS: %s", pSignalingClient->channelEndpointHttps);
    DLOGD("  WSS: %s", pSignalingClient->channelEndpointWss);
    DLOGD("  WEBRTC: %s", pSignalingClient->channelEndpointWebrtc[0] != '\0' ? pSignalingClient->channelEndpointWebrtc : "(not requested/empty)");

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
    CHAR url[MAX_URI_CHAR_LEN + 1], paramsJson[MAX_JSON_PARAMETER_STRING_LEN];
    PLwsCallInfo pLwsCallInfo = NULL;
    PCHAR pResponseStr;
    UINT32 resultLen;

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

    CHK_STATUS(
        parseIceConfigResponse(pResponseStr, resultLen, MAX_ICE_CONFIG_COUNT, pSignalingClient->iceConfigs, &pSignalingClient->iceConfigCount));

    // Perform some validation on the ice configuration
    CHK_STATUS(validateIceConfiguration(pSignalingClient));

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        DLOGE("Call Failed with Status:  0x%08x", retStatus);
    }

    freeLwsCallInfo(&pLwsCallInfo);

    LEAVES();
    return retStatus;
}

STATUS parseIceConfigResponse(PCHAR pResponseStr, UINT32 responseLen, UINT8 maxIceConfigs, PIceConfigInfo pIceConfigs, PUINT32 pIceConfigCount)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmntok_t* pToken;
    UINT32 i, configCount = 0, strLen; // Note: strLen excludes the NULL terminator
    INT32 j, tokenCount;
    UINT64 ttl;
    BOOL jsonInIceServerList = FALSE;

    CHK(pIceConfigs != NULL && pIceConfigCount != NULL && pResponseStr != NULL, STATUS_NULL_ARG);
    CHK(maxIceConfigs > 0, STATUS_INVALID_ARG);
    CHK(!IS_EMPTY_STRING(pResponseStr), STATUS_INVALID_API_CALL_RETURN_JSON);

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, responseLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(pIceConfigs, 0x00, maxIceConfigs * SIZEOF(IceConfigInfo));
    *pIceConfigCount = 0;

    // Loop through the tokens and extract the ice configuration
    for (i = 0; i < tokenCount; i++) {
        if (!jsonInIceServerList) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "IceServerList")) {
                jsonInIceServerList = TRUE;

                CHK(tokens[i + 1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                if (tokens[i + 1].size > maxIceConfigs) {
                    DLOGW("Received more ice configs (%d) than supported (%d). Will ignore the rest.", tokens[i + 1].size, maxIceConfigs);
                }
            }
        } else {
            pToken = &tokens[i];
            if (pToken->type == JSMN_UNDEFINED) {
                DLOGW("Encountered unexpected item in the JSON! %*.s", pResponseStr, responseLen);
                // Skip this token and continue parsing
                // Don't return error, just move to next token
                if (i + 1 < tokenCount && tokens[i + 1].type != JSMN_OBJECT) {
                    i++; // Skip the value associated with this field
                }
                continue;
            } else if (pToken->type == JSMN_OBJECT) {
                if (configCount + 1 > maxIceConfigs) {
                    // That's all we have room to parse
                    break;
                }
                configCount++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Username")) {
                strLen = (UINT32) (pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_USER_NAME_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                SNPRINTF(pIceConfigs[configCount - 1].userName, MAX_ICE_CONFIG_USER_NAME_BUFFER_LEN, "%.*s", strLen, pResponseStr + pToken[1].start);
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Password")) {
                strLen = (UINT32) (pToken[1].end - pToken[1].start);
                CHK(strLen <= MAX_ICE_CONFIG_CREDENTIAL_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                SNPRINTF(pIceConfigs[configCount - 1].password, MAX_ICE_CONFIG_CREDENTIAL_BUFFER_LEN, "%.*s", strLen, pResponseStr + pToken[1].start);
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Ttl")) {
                retStatus = STRTOUI64((PCHAR) pResponseStr + pToken[1].start, (PCHAR) pResponseStr + pToken[1].end, 10, &ttl);
                if (STATUS_FAILED(retStatus)) {
                    strLen = (UINT32) (pToken[1].end - pToken[1].start);
                    DLOGE("Unable to convert TTL: %.*s to a number", strLen, (PCHAR) pResponseStr + pToken[1].start);
                    retStatus = STATUS_INVALID_API_CALL_RETURN_JSON;
                    CHK(FALSE, retStatus);
                }
                pIceConfigs[configCount - 1].ttl = ttl * HUNDREDS_OF_NANOS_IN_A_SECOND;
                i++;
            } else if (compareJsonString(pResponseStr, pToken, JSMN_STRING, (PCHAR) "Uris")) {
                CHK(pToken[1].type == JSMN_ARRAY, STATUS_INVALID_API_CALL_RETURN_JSON);
                CHK(pToken[1].size <= MAX_ICE_CONFIG_URI_COUNT, STATUS_SIGNALING_MAX_ICE_URI_COUNT);
                for (j = 0; j < pToken[1].size; j++) {
                    strLen = (UINT32) (pToken[j + 2].end - pToken[j + 2].start);
                    CHK(strLen <= MAX_ICE_CONFIG_URI_LEN, STATUS_SIGNALING_MAX_ICE_URI_LEN);
                    SNPRINTF(pIceConfigs[configCount - 1].uris[j], MAX_ICE_CONFIG_URI_BUFFER_LEN, "%.*s", strLen, pResponseStr + pToken[j + 2].start);
                    pIceConfigs[configCount - 1].uriCount++;
                }
                i += pToken[1].size + 1;
            }
        }
    }

    *pIceConfigCount = configCount;

CleanUp:
    CHK_LOG_ERR(retStatus);

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
    } else {
        DLOGW("Signaling connect returned result %u, connection not established", callResult);
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

    UNUSED_PARAM(pLwsCallInfo);
    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);
    CHK(pSignalingClient->channelEndpointWebrtc[0] != '\0', STATUS_INTERNAL_ERROR);

    // Create the API url
    STRCPY(url, pSignalingClient->channelEndpointWebrtc);
    STRCAT(url, JOIN_STORAGE_SESSION_API_POSTFIX);

    // Prepare the json params for the call
    if (pSignalingClient->pChannelInfo->channelRoleType == SIGNALING_CHANNEL_ROLE_TYPE_VIEWER) {
        SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), SIGNALING_JOIN_STORAGE_SESSION_VIEWER_PARAM_JSON_TEMPLATE,
                 pSignalingClient->channelDescription.channelArn, pSignalingClient->clientInfo.signalingClientInfo.clientId);
    } else {
        SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), SIGNALING_JOIN_STORAGE_SESSION_MASTER_PARAM_JSON_TEMPLATE,
                 pSignalingClient->channelDescription.channelArn);
    }

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
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    UINT32 i, strLen, resultLen;
    UINT32 tokenCount;
    BOOL jsonInMediaStorageConfig = FALSE;
    BOOL previousStorageStatus; // Store the previous status to compare
    BOOL newStorageStatus = FALSE;

    CHK(pSignalingClient != NULL, STATUS_NULL_ARG);

    DLOGD("=== DescribeMediaStorageConfiguration START ===");
    DLOGD("Cache policy: %d", pSignalingClient->pChannelInfo->cachingPolicy);

    // Store the current storage status before making the API call
    // This allows us to detect when storage status changes and invalidate endpoint cache accordingly
    previousStorageStatus = pSignalingClient->mediaStorageConfig.storageStatus;
    DLOGD("Previous storage status: %s (%d)", previousStorageStatus ? "ENABLED" : "DISABLED", previousStorageStatus);
    DLOGD("Previous storage stream ARN: %s", pSignalingClient->mediaStorageConfig.storageStreamArn);

    // Create the API url
    STRCPY(url, pSignalingClient->pChannelInfo->pControlPlaneUrl);
    STRCAT(url, DESCRIBE_MEDIA_STORAGE_CONF_API_POSTFIX);

    // Prepare the json params for the call
    CHK(pSignalingClient->channelDescription.channelArn[0] != '\0', STATUS_NULL_ARG);
    SNPRINTF(paramsJson, ARRAY_SIZE(paramsJson), DESCRIBE_MEDIA_STORAGE_CONF_PARAM_JSON_TEMPLATE, pSignalingClient->channelDescription.channelArn);
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

    DLOGD("API call successful, parsing response (length: %u bytes)", resultLen);

    // Parse the response
    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pResponseStr, resultLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    DLOGD("Parsed %d JSON tokens, extracting storage configuration...", tokenCount);

    // Loop through the tokens and extract the stream description
    for (i = 1; i < tokenCount; i++) {
        if (!jsonInMediaStorageConfig) {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "MediaStorageConfiguration")) {
                jsonInMediaStorageConfig = TRUE;
                i++;
            }
        } else {
            if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "Status")) {
                strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                if (STRNCMP("ENABLED", pResponseStr + tokens[i + 1].start, strLen) == 0) {
                    newStorageStatus = TRUE;
                } else {
                    newStorageStatus = FALSE;
                }

                // Update the storage status
                pSignalingClient->mediaStorageConfig.storageStatus = newStorageStatus;

                // Check if storage status changed - this is important regardless of caching policy
                // because whenever storage status changes, we need different protocol endpoints
                // (WEBRTC is only needed when storage is enabled)
                if (previousStorageStatus != newStorageStatus) {
                    DLOGD("Storage status changed: %d -> %d", previousStorageStatus, newStorageStatus);

                    // Invalidate getEndpointTime to force GetChannelEndpoint to make a fresh API call
                    // This ensures we request the correct protocol list based on the new storage status:
                    // - If storage was disabled and now enabled: need to request WEBRTC protocol
                    // - If storage was enabled and now disabled: can skip WEBRTC protocol request
                    pSignalingClient->getEndpointTime = INVALID_TIMESTAMP_VALUE;
                    DLOGD("Invalidated getEndpointTime due to storage status change");
                }

                i++;
            } else if (compareJsonString(pResponseStr, &tokens[i], JSMN_STRING, (PCHAR) "StreamARN")) {
                // StorageStream may be null.
                if (tokens[i + 1].type != JSMN_PRIMITIVE) {
                    strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
                    CHK(strLen <= MAX_ARN_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
                    STRNCPY(pSignalingClient->mediaStorageConfig.storageStreamArn, pResponseStr + tokens[i + 1].start, strLen);
                    pSignalingClient->mediaStorageConfig.storageStreamArn[MAX_ARN_LEN] = '\0';
                }
                i++;
            }
        }
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
    CHAR encodedIceConfig[MAX_ENCODED_ICE_SERVER_INFOS_STR_LEN + 1];
    CHAR encodedUris[MAX_ICE_SERVER_URI_STR_LEN + 1];
    UINT32 size, writtenSize, correlationLen, iceCount, uriCount, urisLen, iceConfigLen;
    BOOL awaitForResponse;
    PCHAR pMessageType;
    UINT64 curTime;

    // Ensure we are in a connected state
    CHK_STATUS(acceptSignalingStateMachineState(pSignalingClient, SIGNALING_STATE_CONNECTED | SIGNALING_STATE_JOIN_SESSION_CONNECTED));

    CHK(pSignalingClient != NULL && pSignalingClient->pOngoingCallInfo != NULL, STATUS_NULL_ARG);

    // Prepare the buffer to send
    switch (messageType) {
        case SIGNALING_MESSAGE_TYPE_OFFER:
            pMessageType = (PCHAR) SIGNALING_SDP_TYPE_OFFER;
            break;
        case SIGNALING_MESSAGE_TYPE_ANSWER:
            pMessageType = (PCHAR) SIGNALING_SDP_TYPE_ANSWER;
            break;
        case SIGNALING_MESSAGE_TYPE_ICE_CANDIDATE:
            pMessageType = (PCHAR) SIGNALING_ICE_CANDIDATE;
            break;
        default:
            CHK(FALSE, STATUS_INVALID_ARG);
    }
    DLOGD("%s", pMessageType);
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

    // Start off with an empty string
    encodedIceConfig[0] = '\0';

    // In case of an Offer, package the ICE candidates only if we have a set of non-expired ICE configs
    if (messageType == SIGNALING_MESSAGE_TYPE_OFFER && pSignalingClient->iceConfigCount != 0 &&
        (curTime = SIGNALING_GET_CURRENT_TIME(pSignalingClient)) <= pSignalingClient->iceConfigExpiration &&
        STATUS_SUCCEEDED(validateIceConfiguration(pSignalingClient))) {
        // Start the ice infos by copying the preamble, then the main body and then the ending
        STRCPY(encodedIceConfig, SIGNALING_ICE_SERVER_LIST_TEMPLATE_START);
        iceConfigLen = ARRAY_SIZE(SIGNALING_ICE_SERVER_LIST_TEMPLATE_START) - 1; // remove the null terminator

        for (iceCount = 0; iceCount < pSignalingClient->iceConfigCount; iceCount++) {
            encodedUris[0] = '\0';
            for (uriCount = 0; uriCount < pSignalingClient->iceConfigs[iceCount].uriCount; uriCount++) {
                STRCAT(encodedUris, "\"");
                STRCAT(encodedUris, pSignalingClient->iceConfigs[iceCount].uris[uriCount]);
                STRCAT(encodedUris, "\",");
            }

            // remove the last comma
            urisLen = STRLEN(encodedUris);
            encodedUris[--urisLen] = '\0';

            // Construct the encoded ice config
            // NOTE: We need to subtract the passed time to get the TTL of the expiration correct
            writtenSize = (UINT32) SNPRINTF(encodedIceConfig + iceConfigLen, MAX_ICE_SERVER_INFO_STR_LEN, SIGNALING_ICE_SERVER_TEMPLATE,
                                            pSignalingClient->iceConfigs[iceCount].password,
                                            (pSignalingClient->iceConfigs[iceCount].ttl - (curTime - pSignalingClient->iceConfigTime)) /
                                                HUNDREDS_OF_NANOS_IN_A_SECOND,
                                            encodedUris, pSignalingClient->iceConfigs[iceCount].userName);
            CHK(writtenSize <= MAX_ICE_SERVER_INFO_STR_LEN, STATUS_SIGNALING_MAX_MESSAGE_LEN_AFTER_ENCODING);
            iceConfigLen += writtenSize;
        }

        // Get rid of the last comma
        iceConfigLen--;

        // Closing the JSON array
        STRCPY(encodedIceConfig + iceConfigLen, SIGNALING_ICE_SERVER_LIST_TEMPLATE_END);
    }

    // Prepare json message
    if (correlationLen == 0) {
        writtenSize = (UINT32) SNPRINTF((PCHAR) (pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE), size, SIGNALING_SEND_MESSAGE_TEMPLATE,
                                        pMessageType, MAX_SIGNALING_CLIENT_ID_LEN, peerClientId, encodedMessage, encodedIceConfig);
    } else {
        writtenSize = (UINT32) SNPRINTF((PCHAR) (pSignalingClient->pOngoingCallInfo->sendBuffer + LWS_PRE), size,
                                        SIGNALING_SEND_MESSAGE_TEMPLATE_WITH_CORRELATION_ID, pMessageType, MAX_SIGNALING_CLIENT_ID_LEN, peerClientId,
                                        encodedMessage, correlationLen, pCorrelationId, encodedIceConfig);
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

    DLOGD("Sending data over web socket: Message type: %s, RecepientId: %s", pMessageType, peerClientId);

    CHK_STATUS(writeLwsData(pSignalingClient, awaitForResponse));

    // Re-setting the buffer size and offset
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendBufferSize, 0);
    ATOMIC_STORE(&pSignalingClient->pOngoingCallInfo->sendOffset, 0);

CleanUp:
    CHK_LOG_ERR(retStatus);

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
    CHK_LOG_ERR(retStatus);

    if (sendLocked) {
        MUTEX_UNLOCK(pSignalingClient->sendLock);
    }

    if (receiveLocked) {
        MUTEX_UNLOCK(pSignalingClient->receiveLock);
    }

    LEAVES();
    return retStatus;
}

STATUS parseSignalingMessage(PCHAR pMessage, UINT32 messageLen, PReceivedSignalingMessage pReceivedSignalingMessage)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    jsmn_parser parser;
    jsmntok_t tokens[MAX_JSON_TOKEN_COUNT];
    jsmntok_t* pToken;
    INT32 tokenCount;
    UINT32 i, strLen, outLen, printResult;
    BOOL parsedMessageType = FALSE, parsedStatusResponse = FALSE;

    CHK(pMessage != NULL && pReceivedSignalingMessage != NULL, STATUS_NULL_ARG);
    CHK(messageLen <= MAX_SIGNALING_MESSAGE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);

    MEMSET(pReceivedSignalingMessage, 0x00, SIZEOF(ReceivedSignalingMessage));
    pReceivedSignalingMessage->signalingMessage.messageType = SIGNALING_MESSAGE_TYPE_UNKNOWN;

    jsmn_init(&parser);
    tokenCount = jsmn_parse(&parser, pMessage, messageLen, tokens, SIZEOF(tokens) / SIZEOF(jsmntok_t));
    CHK(tokenCount > 1, STATUS_INVALID_API_CALL_RETURN_JSON);
    CHK(tokens[0].type == JSMN_OBJECT, STATUS_INVALID_API_CALL_RETURN_JSON);

    for (i = 1; i < tokenCount; i++) {
        if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "senderClientId")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_CLIENT_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            printResult = SNPRINTF(pReceivedSignalingMessage->signalingMessage.peerClientId, MAX_SIGNALING_CLIENT_ID_LEN + 1, "%.*s", strLen,
                                   pMessage + tokens[i + 1].start);
            CHK(printResult >= 0 && printResult <= MAX_SIGNALING_CLIENT_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            i++;
        } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "messageType")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_MESSAGE_TYPE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            CHK_STATUS(getMessageTypeFromString(pMessage + tokens[i + 1].start, strLen, &pReceivedSignalingMessage->signalingMessage.messageType));
            parsedMessageType = TRUE;
            i++;
        } else if (compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "messagePayload")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_SIGNALING_MESSAGE_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            outLen = SIZEOF(pReceivedSignalingMessage->signalingMessage.payload);
            // Base64 method will set outLen <= original input outLen
            CHK_STATUS(base64Decode(pMessage + tokens[i + 1].start, strLen, (PBYTE) (pReceivedSignalingMessage->signalingMessage.payload), &outLen));

            // Need to manually null-terminate the output of base64Decode
            pReceivedSignalingMessage->signalingMessage.payload[outLen] = '\0';
            pReceivedSignalingMessage->signalingMessage.payloadLen = outLen;
            i++;
        } else if (!parsedStatusResponse && compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "statusResponse")) {
            parsedStatusResponse = TRUE;
            i++;
        } else if (parsedStatusResponse && compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "correlationId")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_CORRELATION_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            printResult = SNPRINTF(pReceivedSignalingMessage->signalingMessage.correlationId, MAX_CORRELATION_ID_LEN + 1, "%.*s", strLen,
                                   pMessage + tokens[i + 1].start);
            CHK(printResult >= 0 && printResult <= MAX_CORRELATION_ID_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            i++;
        } else if (parsedStatusResponse && compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "errorType")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_ERROR_TYPE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            printResult =
                SNPRINTF(pReceivedSignalingMessage->errorType, MAX_ERROR_TYPE_STRING_LEN + 1, "%.*s", strLen, pMessage + tokens[i + 1].start);
            CHK(printResult >= 0 && printResult <= MAX_ERROR_TYPE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            i++;
        } else if (parsedStatusResponse && compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "statusCode")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_STATUS_CODE_STRING_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            CHK_STATUS(STRTOUI32(pMessage + tokens[i + 1].start, pMessage + tokens[i + 1].end, 10, &pReceivedSignalingMessage->statusCode));
            i++;
        } else if (parsedStatusResponse && compareJsonString(pMessage, &tokens[i], JSMN_STRING, (PCHAR) "description")) {
            strLen = (UINT32) (tokens[i + 1].end - tokens[i + 1].start);
            CHK(strLen <= MAX_MESSAGE_DESCRIPTION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            printResult =
                SNPRINTF(pReceivedSignalingMessage->description, MAX_MESSAGE_DESCRIPTION_LEN + 1, "%.*s", strLen, pMessage + tokens[i + 1].start);
            CHK(printResult >= 0 && printResult <= MAX_MESSAGE_DESCRIPTION_LEN, STATUS_INVALID_API_CALL_RETURN_JSON);
            i++;
        }
    }

    CHK_ERR(parsedMessageType, STATUS_SIGNALING_INVALID_MESSAGE_TYPE, "Could not parse: %.*s", messageLen, pMessage);

CleanUp:
    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS receiveLwsMessage(PSignalingClient pSignalingClient, PCHAR pMessage, UINT32 messageLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, strLen;
    PSignalingMessageWrapper pSignalingMessageWrapper = NULL;
    TID receivedTid = INVALID_TID_VALUE;
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
    CHK(NULL != (pSignalingMessageWrapper = (PSignalingMessageWrapper) MEMCALLOC(1, SIZEOF(SignalingMessageWrapper))), STATUS_NOT_ENOUGH_MEMORY);
    pSignalingMessageWrapper->receivedSignalingMessage.signalingMessage.version = SIGNALING_MESSAGE_CURRENT_VERSION;
    CHK_STATUS(parseSignalingMessage(pMessage, messageLen, &pSignalingMessageWrapper->receivedSignalingMessage));

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
    CHK(pSignalingClient != NULL && pSignalingClient->pWebsocketContext != NULL, retStatus);

    if (pSignalingClient->currentWsi[protocolIndex] != NULL) {
        lws_callback_on_writable(pSignalingClient->currentWsi[protocolIndex]);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS configureLwsLogging(UINT32 kvsLogLevel)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    INT32 lws_levels = 0;

    if (kvsLogLevel <= LOG_LEVEL_ERROR) {
        lws_levels |= LLL_ERR;
    }
    if (kvsLogLevel <= LOG_LEVEL_WARN) {
        lws_levels |= LLL_WARN;
    }
    if (kvsLogLevel <= LOG_LEVEL_INFO || kvsLogLevel == LOG_LEVEL_PROFILE) {
        lws_levels |= LLL_NOTICE;
    }
    if (kvsLogLevel <= LOG_LEVEL_DEBUG) {
        lws_levels |= LLL_INFO;
    }

    lws_set_log_level(lws_levels, NULL);

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}