/**
 * Kinesis Video Producer ConnectionListener
 */
#define LOG_CLASS "ConnectionListener"
#include "../Include_i.h"

STATUS createConnectionListener(PConnectionListener* ppConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    PConnectionListener pConnectionListener = NULL;

    CHK(ppConnectionListener != NULL, STATUS_NULL_ARG);

    pConnectionListener = (PConnectionListener) MEMCALLOC(1, SIZEOF(ConnectionListener) + MAX_UDP_PACKET_SIZE);
    CHK(pConnectionListener != NULL, STATUS_NOT_ENOUGH_MEMORY);

    CHK_STATUS(doubleListCreate(&pConnectionListener->connectionList));
    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, FALSE);
    ATOMIC_STORE_BOOL(&pConnectionListener->listenerRoutineStarted, FALSE);
    pConnectionListener->receiveDataRoutine = INVALID_TID_VALUE;
    pConnectionListener->lock = MUTEX_CREATE(FALSE);
    pConnectionListener->connectionRemovalLock = MUTEX_CREATE(FALSE);

    // pConnectionListener->pBuffer starts at the end of ConnectionListener struct
    pConnectionListener->pBuffer = (PBYTE) (pConnectionListener + 1);
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

    CHK(ppConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(*ppConnectionListener != NULL, retStatus);

    pConnectionListener = *ppConnectionListener;

    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, TRUE);

    if (IS_VALID_TID_VALUE(pConnectionListener->receiveDataRoutine)) {
        THREAD_JOIN(pConnectionListener->receiveDataRoutine, NULL);
        pConnectionListener->receiveDataRoutine = INVALID_TID_VALUE;
    }

    // PSocketConnections stored here are not owned by ConnectionListener
    if (pConnectionListener->connectionList != NULL) {
        CHK_LOG_ERR_NV(doubleListFree(pConnectionListener->connectionList));
    }

    if (pConnectionListener->lock != INVALID_MUTEX_VALUE) {
        MUTEX_FREE(pConnectionListener->lock);
    }

    if (pConnectionListener->connectionRemovalLock != INVALID_MUTEX_VALUE) {
        MUTEX_FREE(pConnectionListener->connectionRemovalLock);
    }

    MEMFREE(pConnectionListener);

    *ppConnectionListener = NULL;

CleanUp:

    CHK_LOG_ERR_NV(retStatus);

    return retStatus;
}

STATUS connectionListenerAddConnection(PConnectionListener pConnectionListener, PSocketConnection pSocketConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    CHK(pConnectionListener != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    CHK_STATUS(doubleListInsertItemHead(pConnectionListener->connectionList, (UINT64) pSocketConnection));

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
    BOOL locked = FALSE, connectionRemovalLocked = FALSE;
    PDoubleListNode pCurNode = NULL, pTargetNode = NULL;
    UINT64 data;

    CHK(pConnectionListener != NULL && pSocketConnection != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    CHK_STATUS(doubleListGetHeadNode(pConnectionListener->connectionList, &pCurNode));
    while (pCurNode != NULL && pTargetNode == NULL) {
        CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
        if (((PSocketConnection) data) == pSocketConnection) {
            pTargetNode = pCurNode;
        }
        pCurNode = pCurNode->pNext;
    }

    if (pTargetNode != NULL) {
        // to make sure that we remove only when select() is unblocked. Otherwise we could close socket that select()
        // is still listening and cause bad file descriptor error.
        MUTEX_LOCK(pConnectionListener->connectionRemovalLock);
        connectionRemovalLocked = TRUE;

        // not freeing the PSocketConnection as it is not owned by connectionListener
        CHK_STATUS(doubleListDeleteNode(pConnectionListener->connectionList, pTargetNode));

        MUTEX_UNLOCK(pConnectionListener->connectionRemovalLock);
        connectionRemovalLocked = FALSE;
    }

CleanUp:

    if (connectionRemovalLocked) {
        MUTEX_UNLOCK(pConnectionListener->connectionRemovalLock);
    }

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

STATUS connectionListenerRemoveAllConnection(PConnectionListener pConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE, connectionRemovalLocked = FALSE;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate), retStatus);

    MUTEX_LOCK(pConnectionListener->lock);
    locked = TRUE;

    // to make sure that we remove only when select() is unblocked. Otherwise we could close socket that select()
    // is still listening and cause bad file descriptor error.
    MUTEX_LOCK(pConnectionListener->connectionRemovalLock);
    connectionRemovalLocked = TRUE;

    // not freeing the PSocketConnection as it is not owned by connectionListener
    CHK_STATUS(doubleListClear(pConnectionListener->connectionList, FALSE));

    MUTEX_UNLOCK(pConnectionListener->connectionRemovalLock);
    connectionRemovalLocked = FALSE;

CleanUp:

    if (connectionRemovalLocked) {
        MUTEX_UNLOCK(pConnectionListener->connectionRemovalLock);
    }

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    return retStatus;
}

STATUS connectionListenerStart(PConnectionListener pConnectionListener)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);
    CHK(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate) && !ATOMIC_LOAD_BOOL(&pConnectionListener->listenerRoutineStarted), retStatus);

    ATOMIC_STORE_BOOL(&pConnectionListener->listenerRoutineStarted, TRUE);
    CHK_STATUS(THREAD_CREATE(&pConnectionListener->receiveDataRoutine,
                             connectionListenerReceiveDataRoutine,
                             (PVOID) pConnectionListener));

CleanUp:

    return retStatus;
}


PVOID connectionListenerReceiveDataRoutine(PVOID arg)
{
    STATUS retStatus = STATUS_SUCCESS;
    PConnectionListener pConnectionListener = (PConnectionListener) arg;
    PDoubleListNode pCurNode = NULL;
    UINT64 data;
    PSocketConnection pSocketConnection;
    BOOL locked = FALSE;

    INT32 nfds = 0;
    fd_set rfds;
    struct timeval tv;
    INT32 retval;
    INT64 readLen;
    // the source address is put here. sockaddr_storage can hold either sockaddr_in or sockaddr_in6
    struct sockaddr_storage srcAddrBuff;
    socklen_t srcAddrBuffLen = SIZEOF(srcAddrBuff);
    struct sockaddr_in *pIpv4Addr;
    struct sockaddr_in6 *pIpv6Addr;
    KvsIpAddress srcAddr;
    PKvsIpAddress pSrcAddr = NULL;

    CHK(pConnectionListener != NULL, STATUS_NULL_ARG);

    srcAddr.isPointToPoint = FALSE;

    while(!ATOMIC_LOAD_BOOL(&pConnectionListener->terminate)) {
        FD_ZERO(&rfds);
        nfds = 0;

        MUTEX_LOCK(pConnectionListener->lock);
        locked = TRUE;

        CHK_STATUS(doubleListGetHeadNode(pConnectionListener->connectionList, &pCurNode));
        while (pCurNode != NULL) {
            CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
            pSocketConnection = (PSocketConnection) data;
            pCurNode = pCurNode->pNext;
            FD_SET(pSocketConnection->localSocket, &rfds);
            nfds = MAX(nfds, pSocketConnection->localSocket);
        }

        nfds++;

        MUTEX_UNLOCK(pConnectionListener->lock);
        locked = FALSE;

        // timeout select every SOCKET_WAIT_FOR_DATA_TIMEOUT_SECONDS seconds and check if terminate
        // on linux tv need to be reinitialized after select is done.
        tv.tv_sec = SOCKET_WAIT_FOR_DATA_TIMEOUT_SECONDS;
        tv.tv_usec = 0;

        MUTEX_LOCK(pConnectionListener->connectionRemovalLock);

        retval = select(nfds, &rfds, NULL, NULL, &tv);

        MUTEX_UNLOCK(pConnectionListener->connectionRemovalLock);

        if (retval == -1) {
            DLOGE("select() failed with errno %s", strerror(errno));
        } else if (retval > 0) {

            MUTEX_LOCK(pConnectionListener->lock);
            locked = TRUE;

            CHK_STATUS(doubleListGetHeadNode(pConnectionListener->connectionList, &pCurNode));
            while (pCurNode != NULL) {
                CHK_STATUS(doubleListGetNodeData(pCurNode, &data));
                pSocketConnection = (PSocketConnection) data;

                if (FD_ISSET(pSocketConnection->localSocket, &rfds)) {

                    for(;;) {
                        readLen = recvfrom(pSocketConnection->localSocket, pConnectionListener->pBuffer, pConnectionListener->bufferLen, 0,
                                           (struct sockaddr *) &srcAddrBuff, &srcAddrBuffLen);
                        if (readLen < 0 ) {
                            if (errno != EWOULDBLOCK) {
                                DLOGE("recvfrom() failed with errno %s", strerror(errno));
                            } else {
                                break;
                            }

                        } else if (readLen == 0) {
                            CHK_STATUS(doubleListRemoveNode(pConnectionListener->connectionList, pCurNode));
                        }

                        if (readLen >= 0 &&
                            pSocketConnection->dataAvailableCallbackFn != NULL &&
                            // data could be encrypted so they need to be decrypted through socketConnectionReadData
                            // and get the decrypted data length.
                            STATUS_SUCCEEDED(socketConnectionReadData(pSocketConnection,
                                                                      pConnectionListener->pBuffer,
                                                                      pConnectionListener->bufferLen,
                                                                      (PUINT32) &readLen))) {



                            if (pSocketConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
                                if (srcAddrBuff.ss_family == AF_INET) {
                                    srcAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
                                    pIpv4Addr = (struct sockaddr_in *) &srcAddrBuff;
                                    MEMCPY(srcAddr.address, (PBYTE) &pIpv4Addr->sin_addr, IPV4_ADDRESS_LENGTH);
                                    srcAddr.port = pIpv4Addr->sin_port;
                                } else if (srcAddrBuff.ss_family == AF_INET6) {
                                    srcAddr.family = KVS_IP_FAMILY_TYPE_IPV6;
                                    pIpv6Addr = (struct sockaddr_in6 *) &srcAddrBuff;
                                    MEMCPY(srcAddr.address, (PBYTE) &pIpv6Addr->sin6_addr, IPV6_ADDRESS_LENGTH);
                                    srcAddr.port = pIpv6Addr->sin6_port;
                                }
                                pSrcAddr = &srcAddr;
                            } else {
                                // srcAddr is ignored in TCP callback handlers
                                pSrcAddr = NULL;
                            }

                            pSocketConnection->dataAvailableCallbackFn(pSocketConnection->dataAvailableCallbackCustomData,
                                                                       pSocketConnection,
                                                                       pConnectionListener->pBuffer,
                                                                       (UINT32) readLen,
                                                                       pSrcAddr,
                                                                       NULL); // no dest information available right now.
                        }

                        // reset srcAddrBuffLen to actual size
                        srcAddrBuffLen = SIZEOF(srcAddrBuff);

                    }
                }

                pCurNode = pCurNode->pNext;
            }

            MUTEX_UNLOCK(pConnectionListener->lock);
            locked = FALSE;
        }
    }

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pConnectionListener->lock);
    }

    ATOMIC_STORE_BOOL(&pConnectionListener->listenerRoutineStarted, FALSE);

    return (PVOID) (ULONG_PTR) retStatus;
}
