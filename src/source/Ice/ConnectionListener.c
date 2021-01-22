/**
 * Kinesis Video Producer ConnectionListener
 */
#define LOG_CLASS "ConnectionListener"
#include "../Include_i.h"

STATUS createConnectionListener(PConnectionListener* ppConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 allocationSize = SIZEOF(ConnectionListener) + MAX_UDP_PACKET_SIZE;
    PConnectionListener pConnectionListener = NULL;

    CHK(ppConnectionListener != NULL, STATUS_NULL_ARG);

    pConnectionListener = (PConnectionListener) MEMCALLOC(1, allocationSize);
    CHK(pConnectionListener != NULL, STATUS_NOT_ENOUGH_MEMORY);

    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, FALSE);
    pConnectionListener->receiveDataRoutine = INVALID_TID_VALUE;
    pConnectionListener->lock = MUTEX_CREATE(FALSE);

    // No sockets are present
    pConnectionListener->socketCount = 0;

    // pConnectionListener->pBuffer starts at the end of ConnectionListener struct
    pConnectionListener->pBuffer = (PBYTE)(pConnectionListener + 1);
    pConnectionListener->bufferLen = MAX_UDP_PACKET_SIZE;

CleanUp:

    if (STATUS_FAILED(retStatus) && pConnectionListener != NULL) {
        freeConnectionListener(&pConnectionListener);
        pConnectionListener = NULL;
    }

    if (ppConnectionListener != NULL) {
        *ppConnectionListener = pConnectionListener;
    }

    return retStatus;
}

STATUS freeConnectionListener(PConnectionListener* ppConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    PConnectionListener pConnectionListener = NULL;
    UINT64 timeToWait;
    TID threadId;
    BOOL threadTerminated = FALSE;

    CHK(ppConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(*ppConnectionListener != NULL, retStatus);

    pConnectionListener = *ppConnectionListener;

    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, TRUE);

    if (IS_VALID_MUTEX_VALUE(pConnectionListener->lock)) {
        // Try to await for the thread to finish up
        // NOTE: As TID is not atomic we need to wrap the read in locks
        timeToWait = GETTIME() + CONNECTION_LISTENER_SHUTDOWN_TIMEOUT;

        do {
            MUTEX_LOCK(pConnectionListener->lock);
            threadId = pConnectionListener->receiveDataRoutine;
            MUTEX_UNLOCK(pConnectionListener->lock);
            if (!IS_VALID_TID_VALUE(threadId)) {
                threadTerminated = TRUE;
            }

            // Allow the thread to finish and exit
            if (!threadTerminated) {
                THREAD_SLEEP(KVS_ICE_SHORT_CHECK_DELAY);
            }
        } while (!threadTerminated && GETTIME() < timeToWait);

        if (!threadTerminated) {
            DLOGW("Connection listener handler thread shutdown timed out");
        }

        MUTEX_FREE(pConnectionListener->lock);
    }

    MEMFREE(pConnectionListener);

    *ppConnectionListener = NULL;

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS connectionListenerAddConnection(PConnectionListener pConnectionListener, PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, iterate = TRUE;
    UINT32 i;

    CHK(pConnectionListener != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    // Check for space
    CHK(pConnectionListener->socketCount < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION, STATUS_NOT_ENOUGH_MEMORY);

    // Find an empty slot by checking whether connected
    for (i = 0; iterate && i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] == NULL) {
            pConnectionListener->sockets[i] = pSocketConnection;
            pConnectionListener->socketCount++;
            iterate = FALSE;
        }
    }

    MUTEX_UNLOCK(pConnectionListener->lock);
    locked = FALSE;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

STATUS connectionListenerRemoveConnection(PConnectionListener pConnectionListener, PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, iterate = TRUE;
    UINT32 i;

    CHK(pConnectionListener != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    // Mark socket as closed
    CHK_STATUS(socketConnectionClosed(pSocketConnection));

    // Remove from the list of sockets
    for (i = 0; iterate && i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] == pSocketConnection) {
            iterate = FALSE;

            // Mark the slot as empty and decrement the count
            pConnectionListener->sockets[i] = NULL;
            pConnectionListener->socketCount--;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

STATUS connectionListenerRemoveAllConnection(PConnectionListener pConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    UINT32 i;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    for (i = 0; i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] != NULL) {
            CHK_STATUS(socketConnectionClosed(pConnectionListener->sockets[i]));
            pConnectionListener->sockets[i] = NULL;
            pConnectionListener->socketCount--;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

STATUS connectionListenerStart(PConnectionListener pConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    CHK(!IS_VALID_TID_VALUE(pConnectionListener->receiveDataRoutine), retStatus);
    CHK_STATUS(THREAD_CREATE(&pConnectionListener->receiveDataRoutine, connectionListenerReceiveDataRoutine, (PVOID) pConnectionListener));
    CHK_STATUS(THREAD_DETACH(pConnectionListener->receiveDataRoutine));

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

PVOID connectionListenerReceiveDataRoutine(PVOID arg)
{
    STATUS retStatus = STATUS_SUCCESS;
    PConnectionListener pConnectionListener = (PConnectionListener) arg;
    PSocketConnection pSocketConnection;
    BOOL iterate = TRUE;
    PSocketConnection sockets[CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION];
    UINT32 i, socketCount;

    INT32 nfds = 0;
    fd_set rfds;
    struct timeval tv;
    INT32 retval, localSocket;
    INT64 readLen;
    // the source address is put here. sockaddr_storage can hold either sockaddr_in or sockaddr_in6
    struct sockaddr_storage srcAddrBuff;
    socklen_t srcAddrBuffLen = SIZEOF(srcAddrBuff);
    struct sockaddr_in* pIpv4Addr;
    struct sockaddr_in6* pIpv6Addr;
    KvsIpAddress srcAddr;
    PKvsIpAddress pSrcAddr = NULL;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);

    /* Ensure that memory sanitizers consider
     * rfds initialized even if FD_ZERO is
     * implemented in assembly. */
    MEMSET(&rfds, 0x00, SIZEOF(fd_set));

    srcAddr.isPointToPoint = FALSE;

    while (!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate)) {
        FD_ZERO(&rfds);
        nfds = 0;

        // Perform the socket connection gathering under the lock
        // NOTE: There is no cleanup jump from the lock/unlock block
        // so we don't need to use a boolean indicator whether locked
        MUTEX_LOCK(pConnectionListener->lock);
        for (i = 0, socketCount = 0; i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
            pSocketConnection = pConnectionListener->sockets[i];
            if (pSocketConnection != NULL) {
                if (!socketConnectionIsClosed(pSocketConnection)) {
                    MUTEX_LOCK(pSocketConnection->lock);
                    localSocket = pSocketConnection->localSocket;
                    MUTEX_UNLOCK(pSocketConnection->lock);
                    FD_SET(localSocket, &rfds);
                    nfds = MAX(nfds, localSocket);

                    // Store the sockets locally while in use and mark it as in use
                    sockets[socketCount++] = pSocketConnection;
                    ATOMIC_STORE_BOOL(&pSocketConnection->inUse, TRUE);
                } else {
                    // Remove the connection
                    pConnectionListener->sockets[i] = NULL;
                    pConnectionListener->socketCount--;
                }
            }
        }

        // Should be one more than the sockets count per API documentation
        nfds++;

        // Need to unlock the mutex to ensure other racing threads unblock
        MUTEX_UNLOCK(pConnectionListener->lock);

        // timeout select every SOCKET_WAIT_FOR_DATA_TIMEOUT_SECONDS seconds and check if terminate
        // on linux tv need to be reinitialized after select is done.
        tv.tv_sec = 0;
        tv.tv_usec = CONNECTION_LISTENER_SOCKET_WAIT_FOR_DATA_TIMEOUT / HUNDREDS_OF_NANOS_IN_A_MICROSECOND;

        // blocking call until resolves as a timeout, an error, a signal or data received
        retval = select(nfds, &rfds, NULL, NULL, &tv);

        // In case of 0 we have a timeout and should re-lock to allow for other
        // interlocking operations to proceed. A positive return means we received data
        if (retval == -1) {
            DLOGW("select() failed with errno %s", getErrorString(getErrorCode()));
        } else if (retval > 0) {
            for (i = 0; i < socketCount; i++) {
                pSocketConnection = sockets[i];
                if (!socketConnectionIsClosed(pSocketConnection)) {
                    MUTEX_LOCK(pSocketConnection->lock);
                    localSocket = pSocketConnection->localSocket;
                    MUTEX_UNLOCK(pSocketConnection->lock);

                    if (FD_ISSET(localSocket, &rfds)) {
                        iterate = TRUE;
                        while (iterate) {
                            readLen = recvfrom(localSocket, pConnectionListener->pBuffer, pConnectionListener->bufferLen, 0,
                                               (struct sockaddr*) &srcAddrBuff, &srcAddrBuffLen);
                            if (readLen < 0) {
                                switch (getErrorCode()) {
                                    case EWOULDBLOCK:
                                        break;
                                    default:
                                        /* on any other error, close connection */
                                        CHK_STATUS(socketConnectionClosed(pSocketConnection));
                                        DLOGD("recvfrom() failed with errno %s for socket %d", getErrorString(getErrorCode()), localSocket);
                                        break;
                                }

                                iterate = FALSE;
                            } else if (readLen == 0) {
                                CHK_STATUS(socketConnectionClosed(pSocketConnection));
                                iterate = FALSE;
                            } else if (/* readLen > 0 */
                                       ATOMIC_LOAD_BOOL(&pSocketConnection->receiveData) && pSocketConnection->dataAvailableCallbackFn != NULL &&
                                       /* data could be encrypted so they need to be decrypted through socketConnectionReadData
                                        * and get the decrypted data length. */
                                       STATUS_SUCCEEDED(socketConnectionReadData(pSocketConnection, pConnectionListener->pBuffer,
                                                                                 pConnectionListener->bufferLen, (PUINT32) &readLen))) {
                                if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
                                    if (srcAddrBuff.ss_family == AF_INET) {
                                        srcAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
                                        pIpv4Addr = (struct sockaddr_in*) &srcAddrBuff;
                                        MEMCPY(srcAddr.address, (PBYTE) &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
                                        srcAddr.port = pIpv4Addr->sin_port;
                                    } else if (srcAddrBuff.ss_family == AF_INET6) {
                                        srcAddr.family = KVS_IP_FAMILY_TYPE_IPV6;
                                        pIpv6Addr = (struct sockaddr_in6*) &srcAddrBuff;
                                        MEMCPY(srcAddr.address, (PBYTE) &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                                        srcAddr.port = pIpv6Addr->sin6_port;
                                    }
                                    pSrcAddr = &srcAddr;
                                } else {
                                    // srcAddr is ignored in TCP callback handlers
                                    pSrcAddr = NULL;
                                }

                                // readLen may be 0 if SSL does not emit any application data.
                                // in that case, no need to call dataAvailable callback
                                if (readLen > 0) {
                                    pSocketConnection->dataAvailableCallbackFn(pSocketConnection->dataAvailableCallbackCustomData, pSocketConnection,
                                                                               pConnectionListener->pBuffer, (UINT32) readLen, pSrcAddr,
                                                                               NULL); // no dest information available right now.
                                }
                            }

                            // reset srcAddrBuffLen to actual size
                            srcAddrBuffLen = SIZEOF(srcAddrBuff);
                        }
                    }
                }
            }
        }

        // Mark as unused
        for (i = 0; i < socketCount; i++) {
            ATOMIC_STORE_BOOL(&sockets[i]->inUse, FALSE);
        }
    }

CleanUp:

    if (pConnectionListener != NULL) {
        // As TID is 64 bit we can't atomically update it and need to do it under the lock
        MUTEX_LOCK(pConnectionListener->lock);
        pConnectionListener->receiveDataRoutine = INVALID_TID_VALUE;
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    CHK_LOG_ERR(retStatus);

    return (PVOID)(ULONG_PTR) retStatus;
}
