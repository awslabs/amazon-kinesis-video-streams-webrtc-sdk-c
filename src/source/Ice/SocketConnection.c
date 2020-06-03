/**
 * Kinesis Video Tcp
 */
#define LOG_CLASS "SocketConnection"

#include <execinfo.h>
#include "../Include_i.h"

STATUS createSocketConnection(PKvsIpAddress pHostIpAddr, PKvsIpAddress pPeerIpAddr, KVS_SOCKET_PROTOCOL protocol,
                              UINT64 customData, ConnectionDataAvailableFunc dataAvailableFn, UINT32 sendBufSize,
                              PSocketConnection *ppSocketConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSocketConnection pSocketConnection = NULL;

    CHK(pHostIpAddr != NULL && ppSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(protocol == KVS_SOCKET_PROTOCOL_UDP || pPeerIpAddr != NULL, STATUS_INVALID_ARG);

    pSocketConnection = (PSocketConnection) MEMCALLOC(1, SIZEOF(SocketConnection));
    CHK(pSocketConnection != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSocketConnection->lock = MUTEX_CREATE(FALSE);
    CHK(pSocketConnection->lock != INVALID_MUTEX_VALUE, STATUS_INVALID_OPERATION);

    CHK_STATUS(createSocket(pHostIpAddr, pPeerIpAddr, protocol, sendBufSize, &pSocketConnection->localSocket));
    pSocketConnection->hostIpAddr = *pHostIpAddr;

    pSocketConnection->secureConnection = FALSE;
    pSocketConnection->protocol = protocol;
    if (protocol == KVS_SOCKET_PROTOCOL_TCP) {
        pSocketConnection->peerIpAddr = *pPeerIpAddr;
    }
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, FALSE);
    ATOMIC_STORE_BOOL(&pSocketConnection->receiveData, FALSE);
    pSocketConnection->freeBios = TRUE;
    pSocketConnection->dataAvailableCallbackCustomData = customData;
    pSocketConnection->dataAvailableCallbackFn = dataAvailableFn;
    pSocketConnection->tlsHandshakeStartTime = INVALID_TIMESTAMP_VALUE;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && pSocketConnection != NULL) {
        freeSocketConnection(&pSocketConnection);
        pSocketConnection = NULL;
    }

    if (ppSocketConnection != NULL) {
        *ppSocketConnection = pSocketConnection;
    }

    LEAVES();
    return retStatus;
}

STATUS freeSocketConnection(PSocketConnection* ppSocketConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSocketConnection pSocketConnection = NULL;

    CHK(ppSocketConnection != NULL, STATUS_NULL_ARG);
    pSocketConnection = *ppSocketConnection;
    CHK(pSocketConnection != NULL, retStatus);
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);

    if (IS_VALID_MUTEX_VALUE(pSocketConnection->lock)) {
        MUTEX_FREE(pSocketConnection->lock);
    }

    if (pSocketConnection->pSslCtx != NULL) {
        SSL_CTX_free(pSocketConnection->pSslCtx);
    }

    if (pSocketConnection->freeBios && pSocketConnection->pReadBio != NULL) {
        BIO_free(pSocketConnection->pReadBio);
    }

    if (pSocketConnection->freeBios && pSocketConnection->pWriteBio != NULL) {
        BIO_free(pSocketConnection->pWriteBio);
    }

    if (pSocketConnection->pSsl != NULL) {
        SSL_free(pSocketConnection->pSsl);
    }

    close(pSocketConnection->localSocket);

    MEMFREE(pSocketConnection);

    *ppSocketConnection = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS socketConnectionInitSecureConnection(PSocketConnection pSocketConnection, BOOL isServer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);

    pSocketConnection->pSslCtx = SSL_CTX_new(SSLv23_method());

    CHK(pSocketConnection->pSslCtx != NULL, STATUS_SSL_CTX_CREATION_FAILED);

    SSL_CTX_set_read_ahead(pSocketConnection->pSslCtx, 1);
    SSL_CTX_set_verify(pSocketConnection->pSslCtx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, certificateVerifyCallback);

    CHK(SSL_CTX_set_cipher_list(pSocketConnection->pSslCtx, "HIGH:!aNULL:!MD5:!RC4"), STATUS_SSL_CTX_CREATION_FAILED);

    pSocketConnection->pSsl = SSL_new(pSocketConnection->pSslCtx);
    CHK(pSocketConnection->pSsl != NULL, STATUS_CREATE_SSL_FAILED);

    if (isServer) {
        SSL_set_accept_state(pSocketConnection->pSsl);
    } else {
        SSL_set_connect_state(pSocketConnection->pSsl);
    }

    SSL_set_mode(pSocketConnection->pSsl, SSL_MODE_AUTO_RETRY);
    CHK((pSocketConnection->pReadBio = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK((pSocketConnection->pWriteBio = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);

    BIO_set_mem_eof_return(pSocketConnection->pReadBio, -1);
    BIO_set_mem_eof_return(pSocketConnection->pWriteBio, -1);
    SSL_set_bio(pSocketConnection->pSsl, pSocketConnection->pReadBio, pSocketConnection->pWriteBio);
    pSocketConnection->freeBios = FALSE;

    /* init handshake */
    SSL_do_handshake(pSocketConnection->pSsl);
    pSocketConnection->secureConnection = TRUE;
    pSocketConnection->tlsHandshakeStartTime = GETTIME();

    /* send handshake */
    CHK_STATUS(socketConnectionSendData(pSocketConnection, NULL, 0, NULL));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus)) {
        ERR_print_errors_fp (stderr);
    }

    LEAVES();
    return retStatus;
}

STATUS socketConnectionSendData(PSocketConnection pSocketConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pDestIp)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    INT32 sslRet = 0, sslErr = 0;

    SIZE_T wBioDataLen = 0;
    PCHAR wBioBuffer = NULL;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK((pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP || pDestIp != NULL), STATUS_INVALID_ARG);

    // Using a single CHK_WARN might output too much spew in bad network conditions
    if (ATOMIC_LOAD_BOOL(&pSocketConnection->connectionClosed)) {
        DLOGD("Warning: Failed to send data. Socket closed already");
        CHK(FALSE, STATUS_SOCKET_CONNECTION_CLOSED_ALREADY);
    }

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP && pSocketConnection->secureConnection) {
        if (SSL_is_init_finished(pSocketConnection->pSsl)) {
            if (IS_VALID_TIMESTAMP(pSocketConnection->tlsHandshakeStartTime)) {
                DLOGD("TLS handshake done. Time taken %" PRIu64 " ms",
                      (GETTIME() - pSocketConnection->tlsHandshakeStartTime) / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
                pSocketConnection->tlsHandshakeStartTime = INVALID_TIMESTAMP_VALUE;
            }
            /* Should have a valid buffer */
            CHK(pBuf != NULL && bufLen > 0, STATUS_INVALID_ARG);
            sslRet = SSL_write(pSocketConnection->pSsl, pBuf, bufLen);
            if (sslRet < 0){
                sslErr = SSL_get_error(pSocketConnection->pSsl, sslRet);
                switch (sslErr) {
                    case SSL_ERROR_WANT_READ:
                        /* explicit fall-through */
                    case SSL_ERROR_WANT_WRITE:
                        break;
                    default:
                        DLOGD("Warning: SSL_write failed with %s", ERR_error_string(sslErr, NULL));
                        DLOGD("Close socket %d", pSocketConnection->localSocket);
                        ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);
                        break;
                }

                CHK(FALSE, STATUS_SEND_DATA_FAILED);
            }
        }

        wBioDataLen = (SIZE_T) BIO_get_mem_data(SSL_get_wbio(pSocketConnection->pSsl), &wBioBuffer);
        CHK_ERR(wBioDataLen >= 0, STATUS_SEND_DATA_FAILED, "BIO_get_mem_data failed");

        if (wBioDataLen > 0) {
            retStatus = socketSendDataWithRetry(pSocketConnection, (PBYTE) wBioBuffer, (UINT32) wBioDataLen, NULL, NULL);

            /* reset bio to clear its content since it's already sent if possible */
            BIO_reset(SSL_get_wbio(pSocketConnection->pSsl));
        }

    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP) {
        /* Should have a valid buffer */
        CHK(pBuf != NULL && bufLen > 0, STATUS_INVALID_ARG);
        CHK_STATUS(retStatus = socketSendDataWithRetry(pSocketConnection, pBuf, bufLen, NULL, NULL));

    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
        /* Should have a valid buffer */
        CHK(pBuf != NULL && bufLen > 0, STATUS_INVALID_ARG);
        CHK_STATUS(retStatus = socketSendDataWithRetry(pSocketConnection, pBuf, bufLen, pDestIp, NULL));
    } else {
        CHECK_EXT(FALSE, "socketConnectionSendData should not reach here. Nothing is sent.");
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pSocketConnection->lock);
    }

    return retStatus;
}

STATUS socketConnectionReadData(PSocketConnection pSocketConnection, PBYTE pBuf, UINT32 bufferLen, PUINT32 pDataLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, continueRead = TRUE;
    INT32 sslReadRet = 0;
    UINT32 writtenBytes = 0;
    UINT64 sslErrorRet;

    CHK(pSocketConnection != NULL && pBuf != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    // return early if connection is not secure or no data
    CHK(pSocketConnection->secureConnection && *pDataLen > 0, retStatus);

    CHK(BIO_write(pSocketConnection->pReadBio, pBuf, *pDataLen) > 0, STATUS_SECURE_SOCKET_READ_FAILED);

    // read as much as possible
    while(continueRead && writtenBytes < bufferLen) {
        sslReadRet = SSL_read(pSocketConnection->pSsl, pBuf + writtenBytes, bufferLen - writtenBytes);
        if (sslReadRet <= 0) {
            sslReadRet = SSL_get_error(pSocketConnection->pSsl, sslReadRet);
            switch (sslReadRet) {
                case SSL_ERROR_WANT_WRITE:
                    continueRead = FALSE;
                    break;
                case SSL_ERROR_WANT_READ:
                    break;
                default:
                    sslErrorRet = ERR_get_error();
                    DLOGW("SSL_read failed with %s", ERR_error_string(sslErrorRet, NULL));
                    break;
            }
            break;
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

    if (locked) {
        MUTEX_UNLOCK(pSocketConnection->lock);
    }

    return retStatus;
}

STATUS socketConnectionClosed(PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pSocketConnection->connectionClosed), retStatus);
    PVOID trace[3];
    INT32 q = backtrace(trace, 3);
    PCHAR *symbols = backtrace_symbols(trace, q);
    INT32 j;
    for(j = 0; j < q; ++j) {
        DLOGD("%s", symbols[j]);
    }

    DLOGD("Close socket %d", pSocketConnection->localSocket);
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

BOOL socketConnectionIsClosed(PSocketConnection pSocketConnection)
{
    if (pSocketConnection == NULL) {
        return TRUE;
    } else {
        return ATOMIC_LOAD_BOOL(&pSocketConnection->connectionClosed);
    }
}

BOOL socketConnectionIsConnected(PSocketConnection pSocketConnection)
{
    INT32 retVal;
    struct sockaddr *peerSockAddr = NULL;
    socklen_t addrLen;
    struct sockaddr_in ipv4PeerAddr;
    struct sockaddr_in6 ipv6PeerAddr;

    CHECK(pSocketConnection != NULL);

    if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
        return TRUE;
    }

    if (pSocketConnection->peerIpAddr.family == KVS_IP_FAMILY_TYPE_IPV4) {
        addrLen = SIZEOF(struct sockaddr_in);
        MEMSET(&ipv4PeerAddr, 0x00, SIZEOF(ipv4PeerAddr));
        ipv4PeerAddr.sin_family = AF_INET;
        ipv4PeerAddr.sin_port = pSocketConnection->peerIpAddr.port;
        MEMCPY(&ipv4PeerAddr.sin_addr, pSocketConnection->peerIpAddr.address, IPV4_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr *) &ipv4PeerAddr;
    } else {
        addrLen = SIZEOF(struct sockaddr_in6);
        MEMSET(&ipv6PeerAddr, 0x00, SIZEOF(ipv6PeerAddr));
        ipv6PeerAddr.sin6_family = AF_INET6;
        ipv6PeerAddr.sin6_port = pSocketConnection->peerIpAddr.port;
        MEMCPY(&ipv6PeerAddr.sin6_addr, pSocketConnection->peerIpAddr.address, IPV6_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr *) &ipv6PeerAddr;
    }

    retVal = connect(pSocketConnection->localSocket, peerSockAddr, addrLen);
    if (retVal == 0 || errno == EISCONN) {
        return TRUE;
    }

    DLOGW("socket connection check failed with errno %s", strerror(errno));
    return FALSE;
}

// https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set_verify.html
INT32 certificateVerifyCallback(INT32 preverify_ok, X509_STORE_CTX *ctx)
{
    UNUSED_PARAM(preverify_ok);
    UNUSED_PARAM(ctx);
    return 1;
}

STATUS socketSendDataWithRetry(PSocketConnection pSocketConnection, PBYTE buf, UINT32 bufLen, PKvsIpAddress pDestIp, PUINT32 pBytesWritten)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 socketWriteAttempt = 0;
    SSIZE_T result = 0;
    UINT32 bytesWritten = 0;

    fd_set wfds;
    struct timeval tv;
    socklen_t addrLen = 0;
    struct sockaddr *destAddr = NULL;
    struct sockaddr_in ipv4Addr;
    struct sockaddr_in6 ipv6Addr;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(buf != NULL && bufLen > 0, STATUS_INVALID_ARG);

    if (pDestIp != NULL) {
        if (IS_IPV4_ADDR(pDestIp)) {
            addrLen = SIZEOF(ipv4Addr);
            MEMSET(&ipv4Addr, 0x00, SIZEOF(ipv4Addr));
            ipv4Addr.sin_family = AF_INET;
            ipv4Addr.sin_port = pDestIp->port;
            MEMCPY(&ipv4Addr.sin_addr, pDestIp->address, IPV4_ADDRESS_LENGTH);
            destAddr = (struct sockaddr *) &ipv4Addr;

        } else {
            addrLen = SIZEOF(ipv6Addr);
            MEMSET(&ipv6Addr, 0x00, SIZEOF(ipv6Addr));
            ipv6Addr.sin6_family = AF_INET6;
            ipv6Addr.sin6_port = pDestIp->port;
            MEMCPY(&ipv6Addr.sin6_addr, pDestIp->address, IPV6_ADDRESS_LENGTH);
            destAddr = (struct sockaddr *) &ipv6Addr;
        }
    }

    while (socketWriteAttempt < MAX_SOCKET_WRITE_RETRY && bytesWritten < bufLen) {
        result = sendto(pSocketConnection->localSocket, buf, bufLen, NO_SIGNAL, destAddr, addrLen);
        if (result < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                FD_ZERO(&wfds);
                FD_SET(pSocketConnection->localSocket, &wfds);
                tv.tv_sec = 0;
                tv.tv_usec = SOCKET_SEND_RETRY_TIMEOUT_MICRO_SECOND;
                result = select(pSocketConnection->localSocket + 1, NULL, &wfds, NULL, &tv);

                if (result == 0) {
                    /* loop back and try again */
                    DLOGD("select() timed out");
                } else if (result < 0) {
                    DLOGD("select() failed with errno %s", strerror(errno));
                    break;
                }
            } else if (errno == EINTR) {
                /* nothing need to be done, just retry */
            } else {
                /* fatal error from send() */
                DLOGD("sendto() failed with errno %s", strerror(errno));
                break;
            }
        } else {
            bytesWritten += result;
        }
        socketWriteAttempt++;
    }

    if (pBytesWritten != NULL) {
        *pBytesWritten = bytesWritten;
    }

    if (result < 0) {
        CLOSE_SOCKET_IF_CANT_RETRY(errno, pSocketConnection);
    }

    if (bytesWritten < bufLen) {
        DLOGD("Failed to send data. Bytes sent %u. Data len %u. Retry count %u",
              bytesWritten, bufLen, socketWriteAttempt);
        retStatus = STATUS_SEND_DATA_FAILED;
    }

CleanUp:

    // CHK_LOG_ERR might be too verbose in this case
    if (STATUS_FAILED(retStatus)) {
        DLOGD("Warning: Send data failed with 0x%08x", retStatus);
    }

    return retStatus;
}
