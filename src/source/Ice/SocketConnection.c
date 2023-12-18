/**
 * Kinesis Video Tcp
 */
#define LOG_CLASS "SocketConnection"
#include "../Include_i.h"

STATUS createSocketConnection(KVS_IP_FAMILY_TYPE familyType, KVS_SOCKET_PROTOCOL protocol, PKvsIpAddress pBindAddr, PKvsIpAddress pPeerIpAddr,
                              UINT64 customData, ConnectionDataAvailableFunc dataAvailableFn, UINT32 sendBufSize,
                              PSocketConnection* ppSocketConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PSocketConnection pSocketConnection = NULL;
    CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    CHK(ppSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(protocol == KVS_SOCKET_PROTOCOL_UDP || pPeerIpAddr != NULL, STATUS_INVALID_ARG);

    pSocketConnection = (PSocketConnection) MEMCALLOC(1, SIZEOF(SocketConnection));
    CHK(pSocketConnection != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pSocketConnection->lock = MUTEX_CREATE(FALSE);
    CHK(pSocketConnection->lock != INVALID_MUTEX_VALUE, STATUS_INVALID_OPERATION);

    CHK_STATUS(createSocket(familyType, protocol, sendBufSize, &pSocketConnection->localSocket));
    if (pBindAddr) {
        CHK_STATUS(socketBind(pBindAddr, pSocketConnection->localSocket));
        pSocketConnection->hostIpAddr = *pBindAddr;
    }

    pSocketConnection->secureConnection = FALSE;
    pSocketConnection->protocol = protocol;
    if (protocol == KVS_SOCKET_PROTOCOL_TCP) {
        pSocketConnection->peerIpAddr = *pPeerIpAddr;
        CHK_STATUS(socketConnect(pPeerIpAddr, pSocketConnection->localSocket));
    }

    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, FALSE);
    ATOMIC_STORE_BOOL(&pSocketConnection->receiveData, FALSE);
    ATOMIC_STORE_BOOL(&pSocketConnection->inUse, FALSE);
    pSocketConnection->dataAvailableCallbackCustomData = customData;
    pSocketConnection->dataAvailableCallbackFn = dataAvailableFn;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (pBindAddr) {
        getIpAddrStr(pBindAddr, ipAddr, ARRAY_SIZE(ipAddr));
        DLOGD("create socket with ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pBindAddr->port), pBindAddr->family);
    } else {
        DLOGD("create socket without the bind address(%d:%d)", familyType, protocol);
    }
    if (protocol == KVS_SOCKET_PROTOCOL_TCP) {
        getIpAddrStr(pPeerIpAddr, ipAddr, ARRAY_SIZE(ipAddr));
        DLOGD("tcp socket connected with ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pPeerIpAddr->port), pPeerIpAddr->family);
    }

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
    UINT64 shutdownTimeout;
    CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    CHK(ppSocketConnection != NULL, STATUS_NULL_ARG);
    pSocketConnection = *ppSocketConnection;
    CHK(pSocketConnection != NULL, retStatus);

    // connectionClosed is accessed and modified when checking if socket connection is closed
    // Hence the modification needs to be protected
    MUTEX_LOCK(pSocketConnection->lock);
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);
    MUTEX_UNLOCK(pSocketConnection->lock);

    // Await for the socket connection to be released
    shutdownTimeout = GETTIME() + KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT;
    while (ATOMIC_LOAD_BOOL(&pSocketConnection->inUse) && GETTIME() < shutdownTimeout) {
        THREAD_SLEEP(KVS_ICE_SHORT_CHECK_DELAY);
    }

    if (ATOMIC_LOAD_BOOL(&pSocketConnection->inUse)) {
        DLOGW("Shutting down socket connection timedout after %u seconds", KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT / HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    if (IS_VALID_MUTEX_VALUE(pSocketConnection->lock)) {
        MUTEX_FREE(pSocketConnection->lock);
    }

    if (pSocketConnection->pTlsSession != NULL) {
        freeTlsSession(&pSocketConnection->pTlsSession);
    }

    getIpAddrStr(&pSocketConnection->hostIpAddr, ipAddr, ARRAY_SIZE(ipAddr));
    DLOGD("close socket with ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pSocketConnection->hostIpAddr.port),
          pSocketConnection->hostIpAddr.family);

    getIpAddrStr(&pSocketConnection->peerIpAddr, ipAddr, ARRAY_SIZE(ipAddr));
    DLOGD("close socket connected with ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pSocketConnection->peerIpAddr.port),
          pSocketConnection->peerIpAddr.family);

    if (STATUS_FAILED(retStatus = closeSocket(pSocketConnection->localSocket))) {
        DLOGW("Failed to close the local socket with 0x%08x", retStatus);
    }

    MEMFREE(pSocketConnection);

    *ppSocketConnection = NULL;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS socketConnectionTlsSessionOutBoundPacket(UINT64 customData, PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSocketConnection pSocketConnection = NULL;
    CHK(customData != 0, STATUS_NULL_ARG);

    pSocketConnection = (PSocketConnection) customData;
    CHK_STATUS(socketSendDataWithRetry(pSocketConnection, pBuffer, bufferLen, NULL, NULL));

CleanUp:
    return retStatus;
}

VOID socketConnectionTlsSessionOnStateChange(UINT64 customData, TLS_SESSION_STATE state)
{
    PSocketConnection pSocketConnection = NULL;
    if (customData == 0) {
        return;
    }

    pSocketConnection = (PSocketConnection) customData;
    switch (state) {
        case TLS_SESSION_STATE_NEW:
            pSocketConnection->tlsHandshakeStartTime = INVALID_TIMESTAMP_VALUE;
            break;
        case TLS_SESSION_STATE_CONNECTING:
            pSocketConnection->tlsHandshakeStartTime = GETTIME();
            break;
        case TLS_SESSION_STATE_CONNECTED:
            if (IS_VALID_TIMESTAMP(pSocketConnection->tlsHandshakeStartTime)) {
                PROFILE_WITH_START_TIME(pSocketConnection->tlsHandshakeStartTime, "TLS handshake time");
                pSocketConnection->tlsHandshakeStartTime = INVALID_TIMESTAMP_VALUE;
            }
            break;
        case TLS_SESSION_STATE_CLOSED:
            ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);
            break;
    }
}

STATUS socketConnectionInitSecureConnection(PSocketConnection pSocketConnection, BOOL isServer)
{
    ENTERS();
    TlsSessionCallbacks callbacks;
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    CHK(pSocketConnection->pTlsSession == NULL, STATUS_INVALID_ARG);

    callbacks.outBoundPacketFnCustomData = callbacks.stateChangeFnCustomData = (UINT64) pSocketConnection;
    callbacks.outboundPacketFn = socketConnectionTlsSessionOutBoundPacket;
    callbacks.stateChangeFn = socketConnectionTlsSessionOnStateChange;

    CHK_STATUS(createTlsSession(&callbacks, &pSocketConnection->pTlsSession));
    CHK_STATUS(tlsSessionStart(pSocketConnection->pTlsSession, isServer));
    pSocketConnection->secureConnection = TRUE;

CleanUp:
    if (STATUS_FAILED(retStatus) && pSocketConnection->pTlsSession != NULL) {
        freeTlsSession(&pSocketConnection->pTlsSession);
    }

    if (locked) {
        MUTEX_UNLOCK(pSocketConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS socketConnectionSendData(PSocketConnection pSocketConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pDestIp)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK((pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP || pDestIp != NULL), STATUS_INVALID_ARG);

    // Using a single CHK_WARN might output too much spew in bad network conditions
    if (ATOMIC_LOAD_BOOL(&pSocketConnection->connectionClosed)) {
        DLOGW("Warning: Failed to send data. Socket closed already");
        CHK(FALSE, STATUS_SOCKET_CONNECTION_CLOSED_ALREADY);
    }

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    /* Should have a valid buffer */
    CHK(pBuf != NULL && bufLen > 0, STATUS_INVALID_ARG);
    if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP && pSocketConnection->secureConnection) {
        CHK_STATUS(tlsSessionPutApplicationData(pSocketConnection->pTlsSession, pBuf, bufLen));
    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_TCP) {
        CHK_STATUS(retStatus = socketSendDataWithRetry(pSocketConnection, pBuf, bufLen, NULL, NULL));
    } else if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
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
    BOOL locked = FALSE;

    CHK(pSocketConnection != NULL && pBuf != NULL && pDataLen != NULL, STATUS_NULL_ARG);
    CHK(bufferLen != 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pSocketConnection->lock);
    locked = TRUE;

    // return early if connection is not secure
    CHK(pSocketConnection->secureConnection, retStatus);

    CHK_STATUS(tlsSessionProcessPacket(pSocketConnection->pTlsSession, pBuf, bufferLen, pDataLen));

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
    MUTEX_LOCK(pSocketConnection->lock);
    DLOGD("Close socket %d", pSocketConnection->localSocket);
    ATOMIC_STORE_BOOL(&pSocketConnection->connectionClosed, TRUE);
    if (pSocketConnection->pTlsSession != NULL) {
        tlsSessionShutdown(pSocketConnection->pTlsSession);
    }
    MUTEX_UNLOCK(pSocketConnection->lock);

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
    struct sockaddr* peerSockAddr = NULL;
    socklen_t addrLen;
    struct sockaddr_in ipv4PeerAddr;
    struct sockaddr_in6 ipv6PeerAddr;
    CHAR hostIpAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    CHAR peerIpAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

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
        peerSockAddr = (struct sockaddr*) &ipv4PeerAddr;
    } else {
        addrLen = SIZEOF(struct sockaddr_in6);
        MEMSET(&ipv6PeerAddr, 0x00, SIZEOF(ipv6PeerAddr));
        ipv6PeerAddr.sin6_family = AF_INET6;
        ipv6PeerAddr.sin6_port = pSocketConnection->peerIpAddr.port;
        MEMCPY(&ipv6PeerAddr.sin6_addr, pSocketConnection->peerIpAddr.address, IPV6_ADDRESS_LENGTH);
        peerSockAddr = (struct sockaddr*) &ipv6PeerAddr;
    }

    MUTEX_LOCK(pSocketConnection->lock);
    retVal = connect(pSocketConnection->localSocket, peerSockAddr, addrLen);
    MUTEX_UNLOCK(pSocketConnection->lock);

    getIpAddrStr(&pSocketConnection->hostIpAddr, hostIpAddr, ARRAY_SIZE(hostIpAddr));
    getIpAddrStr(&pSocketConnection->peerIpAddr, peerIpAddr, ARRAY_SIZE(peerIpAddr));
    DLOGD("connect ip: %s:%u. family:%d with ip: %s:%u. family:%d", hostIpAddr, (UINT16) getInt16(pSocketConnection->hostIpAddr.port),
          pSocketConnection->hostIpAddr.family, peerIpAddr, (UINT16) getInt16(pSocketConnection->peerIpAddr.port),
          pSocketConnection->peerIpAddr.family);

    if (retVal == 0 || getErrorCode() == EISCONN) {
        return TRUE;
    }

    DLOGW("socket connection check failed with errno %s(%d)", getErrorString(getErrorCode()), getErrorCode());
    return FALSE;
}

STATUS socketSendDataWithRetry(PSocketConnection pSocketConnection, PBYTE buf, UINT32 bufLen, PKvsIpAddress pDestIp, PUINT32 pBytesWritten)
{
    STATUS retStatus = STATUS_SUCCESS;
    INT32 socketWriteAttempt = 0;
    SSIZE_T result = 0;
    UINT32 bytesWritten = 0;
    INT32 errorNum = 0;

    struct pollfd wfds;
    socklen_t addrLen = 0;
    struct sockaddr* destAddr = NULL;
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
            destAddr = (struct sockaddr*) &ipv4Addr;

        } else {
            addrLen = SIZEOF(ipv6Addr);
            MEMSET(&ipv6Addr, 0x00, SIZEOF(ipv6Addr));
            ipv6Addr.sin6_family = AF_INET6;
            ipv6Addr.sin6_port = pDestIp->port;
            MEMCPY(&ipv6Addr.sin6_addr, pDestIp->address, IPV6_ADDRESS_LENGTH);
            destAddr = (struct sockaddr*) &ipv6Addr;
        }
    }

    while (socketWriteAttempt < MAX_SOCKET_WRITE_RETRY && bytesWritten < bufLen) {
        result = sendto(pSocketConnection->localSocket, buf + bytesWritten, bufLen - bytesWritten, NO_SIGNAL_SEND, destAddr, addrLen);
        if (result < 0) {
            errorNum = getErrorCode();
            if (errorNum == EAGAIN || errorNum == EWOULDBLOCK) {
                MEMSET(&wfds, 0x00, SIZEOF(struct pollfd));
                wfds.fd = pSocketConnection->localSocket;
                wfds.events = POLLOUT;
                wfds.revents = 0;
                result = POLL(&wfds, 1, SOCKET_SEND_RETRY_TIMEOUT_MILLI_SECOND);

                if (result == 0) {
                    /* loop back and try again */
                    DLOGE("poll() timed out");
                } else if (result < 0) {
                    DLOGE("poll() failed with errno %s", getErrorString(getErrorCode()));
                    break;
                }
            } else if (errorNum == EINTR) {
                /* nothing need to be done, just retry */
            } else {
                /* fatal error from send() */
                DLOGE("sendto() failed with errno %s(%d)", getErrorString(errorNum), errorNum);
                CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

                if (pDestIp != NULL) {
                    getIpAddrStr(pDestIp, ipAddr, ARRAY_SIZE(ipAddr));
                    DLOGD("Dest Ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pDestIp->port), pDestIp->family);
                } else {
                    DLOGD("TCP dest IP");
                }

                getIpAddrStr(&pSocketConnection->hostIpAddr, ipAddr, ARRAY_SIZE(ipAddr));
                DLOGD("hostIpAddr Ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pSocketConnection->hostIpAddr.port),
                      pSocketConnection->hostIpAddr.family);
                break;
            }

            // Indicate an attempt only on error
            socketWriteAttempt++;
        } else {
            bytesWritten += result;
        }
    }

    if (pBytesWritten != NULL) {
        *pBytesWritten = bytesWritten;
    }

    if (result < 0) {
        CLOSE_SOCKET_IF_CANT_RETRY(errorNum, pSocketConnection);
    }

    if (bytesWritten < bufLen) {
        DLOGD("Failed to send data. Bytes sent %u. Data len %u. Retry count %u", bytesWritten, bufLen, socketWriteAttempt);
        retStatus = STATUS_SEND_DATA_FAILED;
    }

CleanUp:

    // CHK_LOG_ERR might be too verbose in this case
    if (STATUS_FAILED(retStatus)) {
        DLOGD("Warning: Send data failed with 0x%08x", retStatus);
    }

    return retStatus;
}
