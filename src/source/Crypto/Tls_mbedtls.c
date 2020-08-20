/**
 * Kinesis Video TLS
 */
#define LOG_CLASS "TLS_mbedtls"
#include "../Include_i.h"

STATUS createTlsSession(PTlsSessionCallbacks pCallbacks, PTlsSession* ppTlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTlsSession pTlsSession = NULL;

    CHK(ppTlsSession != NULL && pCallbacks != NULL && pCallbacks->outboundPacketFn != NULL, STATUS_NULL_ARG);

    pTlsSession = (PTlsSession) MEMCALLOC(1, SIZEOF(TlsSession));
    CHK(pTlsSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(createIOBuffer(DEFAULT_MTU_SIZE, &pTlsSession->pReadBuffer));
    pTlsSession->callbacks = *pCallbacks;
    pTlsSession->state = TLS_SESSION_STATE_NEW;

    // initialize mbedtls stuff with sane values
    mbedtls_entropy_init(&pTlsSession->entropy);
    mbedtls_ctr_drbg_init(&pTlsSession->ctrDrbg);
    mbedtls_x509_crt_init(&pTlsSession->cacert);
    mbedtls_ssl_config_init(&pTlsSession->sslCtxConfig);
    mbedtls_ssl_init(&pTlsSession->sslCtx);
    CHK(mbedtls_ctr_drbg_seed(&pTlsSession->ctrDrbg, mbedtls_entropy_func, &pTlsSession->entropy, NULL, 0) == 0, STATUS_CREATE_SSL_FAILED);
    CHK(mbedtls_x509_crt_parse_file(&pTlsSession->cacert, KVS_CA_CERT_PATH) == 0, STATUS_INVALID_CA_CERT_PATH);

CleanUp:
    if (STATUS_FAILED(retStatus) && pTlsSession != NULL) {
        freeTlsSession(&pTlsSession);
    }

    if (ppTlsSession != NULL) {
        *ppTlsSession = pTlsSession;
    }

    LEAVES();
    return retStatus;
}

STATUS freeTlsSession(PTlsSession* ppTlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTlsSession pTlsSession = NULL;

    CHK(ppTlsSession != NULL, STATUS_NULL_ARG);

    pTlsSession = *ppTlsSession;
    CHK(pTlsSession != NULL, retStatus);

    mbedtls_entropy_free(&pTlsSession->entropy);
    mbedtls_ctr_drbg_free(&pTlsSession->ctrDrbg);
    mbedtls_x509_crt_free(&pTlsSession->cacert);
    mbedtls_ssl_config_free(&pTlsSession->sslCtxConfig);
    mbedtls_ssl_free(&pTlsSession->sslCtx);

    freeIOBuffer(&pTlsSession->pReadBuffer);
    retStatus = tlsSessionShutdown(pTlsSession);
    SAFE_MEMFREE(*ppTlsSession);

CleanUp:
    return retStatus;
}

INT32 tlsSessionSendCallback(PVOID customData, const unsigned char* buf, ULONG len)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTlsSession pTlsSession = (PTlsSession) customData;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);

    pTlsSession->callbacks.outboundPacketFn(pTlsSession->callbacks.outBoundPacketFnCustomData, (PBYTE) buf, len);

CleanUp:

    return STATUS_FAILED(retStatus) ? -retStatus : len;
}

INT32 tlsSessionReceiveCallback(PVOID customData, unsigned char* buf, ULONG len)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTlsSession pTlsSession = (PTlsSession) customData;
    PIOBuffer pBuffer;
    UINT32 readBytes = MBEDTLS_ERR_SSL_WANT_READ;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);

    pBuffer = pTlsSession->pReadBuffer;

    if (pBuffer->off < pBuffer->len) {
        retStatus = ioBufferRead(pBuffer, buf, len, &readBytes);
    }

CleanUp:

    return STATUS_FAILED(retStatus) ? -retStatus : readBytes;
}

STATUS tlsSessionStart(PTlsSession pTlsSession, BOOL isServer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 sslRet;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state == TLS_SESSION_STATE_NEW, retStatus);

    CHK(mbedtls_ssl_config_defaults(&pTlsSession->sslCtxConfig, isServer ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT,
                                    MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) == 0,
        STATUS_CREATE_SSL_FAILED);

    mbedtls_ssl_conf_ca_chain(&pTlsSession->sslCtxConfig, &pTlsSession->cacert, NULL);
    mbedtls_ssl_conf_authmode(&pTlsSession->sslCtxConfig, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_rng(&pTlsSession->sslCtxConfig, mbedtls_ctr_drbg_random, &pTlsSession->ctrDrbg);
    CHK(mbedtls_ssl_setup(&pTlsSession->sslCtx, &pTlsSession->sslCtxConfig) == 0, STATUS_SSL_CTX_CREATION_FAILED);
    mbedtls_ssl_set_mtu(&pTlsSession->sslCtx, DEFAULT_MTU_SIZE);
    mbedtls_ssl_set_bio(&pTlsSession->sslCtx, pTlsSession, tlsSessionSendCallback, tlsSessionReceiveCallback, NULL);

    /* init and send handshake */
    tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CONNECTING);
    sslRet = mbedtls_ssl_handshake(&pTlsSession->sslCtx);
    CHK(sslRet == MBEDTLS_ERR_SSL_WANT_READ || sslRet == MBEDTLS_ERR_SSL_WANT_WRITE, STATUS_SSL_CTX_CREATION_FAILED);
    LOG_MBEDTLS_ERROR("mbedtls_ssl_handshake", sslRet);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS tlsSessionProcessPacket(PTlsSession pTlsSession, PBYTE pData, UINT32 bufferLen, PUINT32 pDataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    INT32 sslRet, readBytes = 0;
    BOOL iterate = TRUE;
    PIOBuffer pReadBuffer;

    CHK(pTlsSession != NULL && pData != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != TLS_SESSION_STATE_NEW, STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND);
    CHK(pTlsSession->state != TLS_SESSION_STATE_CLOSED, STATUS_SOCKET_CONNECTION_CLOSED_ALREADY);

    pReadBuffer = pTlsSession->pReadBuffer;
    CHK_STATUS(ioBufferWrite(pReadBuffer, pData, *pDataLen));

    // read application data
    while (iterate && pReadBuffer->off < pReadBuffer->len && bufferLen > 0) {
        sslRet = mbedtls_ssl_read(&pTlsSession->sslCtx, pData + readBytes, bufferLen);
        if (sslRet > 0) {
            readBytes += sslRet;
            bufferLen -= sslRet;
        } else if (sslRet == 0 || sslRet == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            // if sslRet is 0, the connection is closed already.
            // if sslRet is MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY, the client notified us that the connection is going to be closed.
            // In either case, we'll make sure that the state will change to CLOSED. If it's already closed, it'll be just a noop.
            DLOGD("Detected TLS close_notify alert");
            CHK_STATUS(tlsSessionShutdown(pTlsSession));
            iterate = FALSE;
        } else if (sslRet == MBEDTLS_ERR_SSL_WANT_READ || sslRet == MBEDTLS_ERR_SSL_WANT_WRITE) {
            iterate = FALSE;
        } else {
            LOG_MBEDTLS_ERROR("mbedtls_ssl_read", sslRet);
            readBytes = 0;
            retStatus = STATUS_INTERNAL_ERROR;
            iterate = FALSE;
        }
    }

    if (pTlsSession->sslCtx.state == MBEDTLS_SSL_HANDSHAKE_OVER) {
        tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CONNECTED);
    }

CleanUp:
    if (pDataLen != NULL) {
        *pDataLen = readBytes;
    }

    // CHK_LOG_ERR might be too verbose
    if (STATUS_FAILED(retStatus)) {
        DLOGD("Warning: reading socket data failed with 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

STATUS tlsSessionPutApplicationData(PTlsSession pTlsSession, PBYTE pData, UINT32 dataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 writtenBytes = 0;
    BOOL iterate = TRUE;
    INT32 sslRet;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);

    while (iterate && writtenBytes < dataLen) {
        sslRet = mbedtls_ssl_write(&pTlsSession->sslCtx, pData + writtenBytes, dataLen - writtenBytes);
        if (sslRet > 0) {
            writtenBytes += sslRet;
        } else if (sslRet == MBEDTLS_ERR_SSL_WANT_READ || sslRet == MBEDTLS_ERR_SSL_WANT_WRITE) {
            iterate = FALSE;
        } else {
            LOG_MBEDTLS_ERROR("mbedtls_ssl_write", sslRet);
            writtenBytes = 0;
            retStatus = STATUS_INTERNAL_ERROR;
            iterate = FALSE;
        }
    }

CleanUp:
    LEAVES();
    return retStatus;
}

STATUS tlsSessionShutdown(PTlsSession pTlsSession)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 sslRet;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != TLS_SESSION_STATE_CLOSED, retStatus);

    while (mbedtls_ssl_close_notify(&pTlsSession->sslCtx) == MBEDTLS_ERR_SSL_WANT_WRITE) {
        // keep flushing outgoing buffer until nothing left
    }
    CHK_STATUS(tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CLOSED));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}
