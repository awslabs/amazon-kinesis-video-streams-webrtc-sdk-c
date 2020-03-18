/**
 * Kinesis Video Tcp
 */
#define LOG_CLASS "SocketConnection"
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

    pSocketConnection->secureConnection = FALSE;
    pSocketConnection->protocol = protocol;
    if (protocol == KVS_SOCKET_PROTOCOL_TCP) {
        pSocketConnection->peerIpAddr = *pPeerIpAddr;
    }
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, FALSE);
    pSocketConnection->freeBios = TRUE;
    pSocketConnection->dataAvailableCallbackCustomData = customData;
    pSocketConnection->dataAvailableCallbackFn = dataAvailableFn;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

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
    CHK((pSocketConnection->pWriteBio = BIO_new_fd(pSocketConnection->localSocket, BIO_NOCLOSE)) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    CHK((pSocketConnection->pReadBio = BIO_new(BIO_s_mem())) != NULL, STATUS_SSL_CTX_CREATION_FAILED);
    BIO_set_mem_eof_return(pSocketConnection->pReadBio, -1);
    SSL_set_bio(pSocketConnection->pSsl, pSocketConnection->pReadBio, pSocketConnection->pWriteBio);
    pSocketConnection->freeBios = FALSE;

    // init handshake
    SSL_connect(pSocketConnection->pSsl);
    pSocketConnection->secureConnection = TRUE;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (STATUS_FAILED(retStatus)) {
        ERR_print_errors_fp (stderr);
    }

    LEAVES();
    return retStatus;
}

STATUS socketConnectionSendData(PSocketConnection pSocketConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pDestIp)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, iterate = TRUE;
    INT32 sslRet, sslErr, result;

    socklen_t addrLen;
    struct sockaddr *destAddr;
    struct sockaddr_in ipv4Addr;
    struct sockaddr_in6 ipv6Addr;
    fd_set wfds;
    struct timeval tv;

    CHK(pSocketConnection != NULL && pBuf != NULL, STATUS_NULL_ARG);
    CHK(bufLen != 0 && (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP || pDestIp != NULL), STATUS_INVALID_ARG);
    CHK_WARN(!ATOMIC_LOAD_BOOL(&pSocketConnection->connectionClosed), STATUS_SOCKET_CONNECTION_CLOSED_ALREADY, "Failed to send data. Socket closed already");

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP && pSocketConnection->secureConnection) {
        // underlying BIO has been bound to socket so SSL_write sends the encrypted data.
        // because we are using non-blocking socket, SSL_write may return SSL_ERROR_WANT_WRITE in which case
        // SSL_write should be called again with the same buffer.
        while(iterate) {
            sslRet = SSL_write(pSocketConnection->pSsl, pBuf, bufLen);
            if (sslRet > 0 || (sslErr = SSL_get_error(pSocketConnection->pSsl, sslRet)) != SSL_ERROR_WANT_WRITE) {
                iterate = FALSE;
            } else {
                THREAD_SLEEP(SSL_WRITE_RETRY_DELAY);
            }
        }
        CHK_WARN(sslRet > 0, retStatus, "SSL_write failed with %s", ERR_error_string(sslErr, NULL));

    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP) {
        if (send(pSocketConnection->localSocket, pBuf, bufLen, 0) < 0) {
            DLOGE("send data failed with errno %s", strerror(errno));
            CHK(FALSE, STATUS_SEND_DATA_FAILED);
        }

    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
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

        // sending through UDP never block
        result = sendto(pSocketConnection->localSocket, pBuf, bufLen, 0, destAddr, addrLen);
        // In non-blocking mode, socket could return EAGAIN or EWOULDBLOCK if it havent finish previous send.
        // In that case, use select to wait until socket is ready for write and send again.
        if (result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            FD_ZERO(&wfds);
            FD_SET(pSocketConnection->localSocket, &wfds);
            tv.tv_sec = 0;
            tv.tv_usec = SOCKET_SEND_RETRY_TIMEOUT_MICRO_SECOND;
            result = select(pSocketConnection->localSocket + 1, NULL, &wfds, NULL, &tv);

            if (result > 0) {
                result = sendto(pSocketConnection->localSocket, pBuf, bufLen, 0, destAddr, addrLen);
            } else if (result == 0) {
                DLOGD("select() timed out");
                CHK(FALSE, STATUS_SEND_DATA_FAILED);
            } else {
                DLOGD("select() failed with errno %s", strerror(errno));
                CHK(FALSE, STATUS_SEND_DATA_FAILED);
            }
        }
        if (result < 0) {
            DLOGD("sendto data failed with errno %s", strerror(errno));
            CHK(FALSE, STATUS_SEND_DATA_FAILED);
        }
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
    BOOL locked = FALSE;
    INT32 sslReadRet = 0;
    UINT32 writtenBytes = 0;

    CHK(pSocketConnection != NULL && pBuf != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    // return early if connection is not secure or no data
    CHK(pSocketConnection->secureConnection && *pDataLen > 0, retStatus);

    CHK(BIO_write(pSocketConnection->pReadBio, pBuf, *pDataLen) > 0, STATUS_SECURE_SOCKET_READ_FAILED);

    // read as much as possible
    while(writtenBytes < bufferLen) {
        sslReadRet = SSL_read(pSocketConnection->pSsl, pBuf + writtenBytes, bufferLen - writtenBytes);
        // if SSL_read fail or has no more data, break and consume already written data.
        // Unlikely that we will get sslReadRet == 0 here because socketConnectionReadData is only called when
        // socket recevies data. If ssl handshake is not done then -1 is returned.
        if (sslReadRet <= 0) {
            DLOGV("SSL_read returned %d. Length of data already written %u", sslReadRet, writtenBytes);
            break;
        }

        writtenBytes += sslReadRet;
    }

    *pDataLen = writtenBytes;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pSocketConnection->lock);
    }

    return retStatus;
}

STATUS socketConnectionClosed(PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
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
