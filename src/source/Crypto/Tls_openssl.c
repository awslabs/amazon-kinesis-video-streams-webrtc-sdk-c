/**
 * Kinesis Video TLS
 */
#define LOG_CLASS "TLS_openssl"
#include "../Include_i.h"

STATUS createTlsSession(PTlsSessionCallbacks pCallbacks, PTlsSession* ppTlsSession)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTlsSession pTlsSession = NULL;

    CHK(ppTlsSession != NULL && pCallbacks != NULL, STATUS_NULL_ARG);
    CHK(pCallbacks->outboundPacketFn != NULL, STATUS_INVALID_ARG);

    pTlsSession = MEMCALLOC(1, SIZEOF(TlsSession));
    CHK(pTlsSession != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pTlsSession->callbacks = *pCallbacks;
    pTlsSession->state = TLS_SESSION_STATE_NEW;

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

    if (pTlsSession->pSslCtx != NULL) {
        SSL_CTX_free(pTlsSession->pSslCtx);
    }

    if (pTlsSession->pSsl != NULL) {
        SSL_free(pTlsSession->pSsl);
    }

    retStatus = tlsSessionShutdown(pTlsSession);
    SAFE_MEMFREE(*ppTlsSession);

CleanUp:
    return retStatus;
}

// https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set_verify.html
INT32 tlsSessionCertificateVerifyCallback(INT32 preverify_ok, X509_STORE_CTX* ctx)
{
    UNUSED_PARAM(preverify_ok);
    UNUSED_PARAM(ctx);
    return 1;
}

STATUS tlsSessionStart(PTlsSession pTlsSession, BOOL isServer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BIO *pReadBio = NULL, *pWriteBio = NULL;
    BOOL freeBios = TRUE;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state == TLS_SESSION_STATE_NEW, retStatus);

    pTlsSession->pSslCtx = SSL_CTX_new(SSLv23_method());
    CHK(pTlsSession->pSslCtx != NULL, STATUS_SSL_CTX_CREATION_FAILED);

    SSL_CTX_set_read_ahead(pTlsSession->pSslCtx, 1);
    SSL_CTX_set_verify(pTlsSession->pSslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, tlsSessionCertificateVerifyCallback);
    CHK(SSL_CTX_set_cipher_list(pTlsSession->pSslCtx, "HIGH:!aNULL:!MD5:!RC4"), STATUS_SSL_CTX_CREATION_FAILED);

    pTlsSession->pSsl = SSL_new(pTlsSession->pSslCtx);
    CHK(pTlsSession->pSsl != NULL, STATUS_CREATE_SSL_FAILED);

    if (isServer) {
        SSL_set_accept_state(pTlsSession->pSsl);
    } else {
        SSL_set_connect_state(pTlsSession->pSsl);
    }

    SSL_set_mode(pTlsSession->pSsl, SSL_MODE_AUTO_RETRY);
    CHK((pReadBio = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK((pWriteBio = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);

    BIO_set_mem_eof_return(pReadBio, -1);
    BIO_set_mem_eof_return(pWriteBio, -1);
    SSL_set_bio(pTlsSession->pSsl, pReadBio, pWriteBio);
    freeBios = FALSE;

    /* init handshake */
    tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CONNECTING);
    SSL_do_handshake(pTlsSession->pSsl);

    /* send handshake */
    CHK_STATUS(tlsSessionPutApplicationData(pTlsSession, NULL, 0));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        if (freeBios) {
            if (pReadBio != NULL) {
                BIO_free(pReadBio);
            }

            if (pWriteBio != NULL) {
                BIO_free(pWriteBio);
            }
        }
        ERR_print_errors_fp(stderr);
    }

    LEAVES();
    return retStatus;
}

STATUS tlsSessionProcessPacket(PTlsSession pTlsSession, PBYTE pData, UINT32 bufferLen, PUINT32 pDataLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL continueRead = TRUE;
    INT32 sslReadRet = 0;
    UINT32 writtenBytes = 0;
    UINT64 sslErrorRet;

    CHK(pTlsSession != NULL && pData != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != TLS_SESSION_STATE_NEW, STATUS_SOCKET_CONNECTION_NOT_READY_TO_SEND);
    CHK(pTlsSession->state != TLS_SESSION_STATE_CLOSED, STATUS_SOCKET_CONNECTION_CLOSED_ALREADY);

    // return early if there's no data
    CHK(*pDataLen != 0, retStatus);

    CHK(BIO_write(SSL_get_rbio(pTlsSession->pSsl), pData, *pDataLen) > 0, STATUS_SECURE_SOCKET_READ_FAILED);

    // read as much as possible
    while (continueRead && writtenBytes < bufferLen) {
        sslReadRet = SSL_read(pTlsSession->pSsl, pData + writtenBytes, bufferLen - writtenBytes);
        if (sslReadRet <= 0) {
            sslReadRet = SSL_get_error(pTlsSession->pSsl, sslReadRet);
            switch (sslReadRet) {
                case SSL_ERROR_WANT_WRITE:
                    continueRead = FALSE;
                    break;
                case SSL_ERROR_WANT_READ:
                case SSL_ERROR_ZERO_RETURN:
                    break;
                default:
                    sslErrorRet = ERR_get_error();
                    DLOGW("SSL_read failed with %s", ERR_error_string(sslErrorRet, NULL));
                    break;
            }
            continueRead = FALSE;
        } else {
            writtenBytes += sslReadRet;
        }
    }

    *pDataLen = writtenBytes;

CleanUp:

    // CHK_LOG_ERR might be too verbose
    if (STATUS_FAILED(retStatus)) {
        DLOGD("Warning: reading socket data failed with 0x%08x", retStatus);
    }

    return retStatus;
}

STATUS tlsSessionPutApplicationData(PTlsSession pTlsSession, PBYTE pData, UINT32 dataLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 sslRet = 0, sslErr = 0;

    SIZE_T wBioDataLen = 0;
    PCHAR wBioBuffer = NULL;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);

    if (SSL_is_init_finished(pTlsSession->pSsl)) {
        tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CONNECTED);

        sslRet = SSL_write(pTlsSession->pSsl, pData, dataLen);
        if (sslRet < 0) {
            sslErr = SSL_get_error(pTlsSession->pSsl, sslRet);
            switch (sslErr) {
                case SSL_ERROR_WANT_READ:
                    /* explicit fall-through */
                case SSL_ERROR_WANT_WRITE:
                    break;
                default:
                    DLOGD("Warning: SSL_write failed with %s", ERR_error_string(sslErr, NULL));
                    tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CLOSED);
                    break;
            }

            CHK(FALSE, STATUS_SEND_DATA_FAILED);
        }
    }

    wBioDataLen = (SIZE_T) BIO_get_mem_data(SSL_get_wbio(pTlsSession->pSsl), &wBioBuffer);
    CHK_ERR(wBioDataLen >= 0, STATUS_SEND_DATA_FAILED, "BIO_get_mem_data failed");

    if (wBioDataLen > 0) {
        retStatus =
            pTlsSession->callbacks.outboundPacketFn(pTlsSession->callbacks.outBoundPacketFnCustomData, (PBYTE) wBioBuffer, (UINT32) wBioDataLen);

        /* reset bio to clear its content since it's already sent if possible */
        BIO_reset(SSL_get_wbio(pTlsSession->pSsl));
    }

CleanUp:
    return retStatus;
}

STATUS tlsSessionShutdown(PTlsSession pTlsSession)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pTlsSession != NULL, STATUS_NULL_ARG);
    CHK(pTlsSession->state != TLS_SESSION_STATE_CLOSED, retStatus);
    CHK_STATUS(tlsSessionChangeState(pTlsSession, TLS_SESSION_STATE_CLOSED));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}
