/**
 * Kinesis Video TurnConnection
 */
#define LOG_CLASS "TurnConnection"
#include "../Include_i.h"

extern StateMachineState TURN_CONNECTION_STATE_MACHINE_STATES[];
extern UINT32 TURN_CONNECTION_STATE_MACHINE_STATE_COUNT;

STATUS createTurnConnection(PIceServer pTurnServer, TIMER_QUEUE_HANDLE timerQueueHandle, TURN_CONNECTION_DATA_TRANSFER_MODE dataTransferMode,
                            KVS_SOCKET_PROTOCOL protocol, PTurnConnectionCallbacks pTurnConnectionCallbacks, PSocketConnection pTurnSocket,
                            PConnectionListener pConnectionListener, PTurnConnection* ppTurnConnection)
{
    UNUSED_PARAM(dataTransferMode);
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = NULL;
    CHAR turnStateMachineName[MAX_STATE_MACHINE_NAME_LENGTH];

    CHK(pTurnServer != NULL && ppTurnConnection != NULL && pTurnSocket != NULL, STATUS_NULL_ARG);
    CHK(IS_VALID_TIMER_QUEUE_HANDLE(timerQueueHandle), STATUS_INVALID_ARG);
    CHK(pTurnServer->isTurn && !IS_EMPTY_STRING(pTurnServer->url) && !IS_EMPTY_STRING(pTurnServer->credential) &&
            !IS_EMPTY_STRING(pTurnServer->username),
        STATUS_INVALID_ARG);

    pTurnConnection = (PTurnConnection) MEMCALLOC(
        1, SIZEOF(TurnConnection) + DEFAULT_TURN_MESSAGE_RECV_CHANNEL_DATA_BUFFER_LEN * 2 + DEFAULT_TURN_MESSAGE_SEND_CHANNEL_DATA_BUFFER_LEN);
    CHK(pTurnConnection != NULL, STATUS_NOT_ENOUGH_MEMORY);
    pTurnConnection->lock = MUTEX_CREATE(TRUE);
    pTurnConnection->sendLock = MUTEX_CREATE(FALSE);
    pTurnConnection->freeAllocationCvar = CVAR_CREATE();
    pTurnConnection->timerQueueHandle = timerQueueHandle;
    pTurnConnection->turnServer = *pTurnServer;
    pTurnConnection->state = TURN_STATE_NEW;
    pTurnConnection->stateTimeoutTime = INVALID_TIMESTAMP_VALUE;
    pTurnConnection->errorStatus = STATUS_SUCCESS;
    pTurnConnection->timerCallbackId = MAX_UINT32;
    pTurnConnection->pTurnPacket = NULL;
    pTurnConnection->pTurnChannelBindPacket = NULL;
    pTurnConnection->pConnectionListener = pConnectionListener;
    pTurnConnection->dataTransferMode =
        TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL; // only TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL for now
    pTurnConnection->protocol = protocol;
    pTurnConnection->relayAddressReported = FALSE;
    pTurnConnection->pControlChannel = pTurnSocket;

    ATOMIC_STORE_BOOL(&pTurnConnection->stopTurnConnection, FALSE);
    ATOMIC_STORE_BOOL(&pTurnConnection->hasAllocation, FALSE);
    ATOMIC_STORE_BOOL(&pTurnConnection->shutdownComplete, FALSE);

    if (pTurnConnectionCallbacks != NULL) {
        pTurnConnection->turnConnectionCallbacks = *pTurnConnectionCallbacks;
    }
    pTurnConnection->recvDataBufferSize = DEFAULT_TURN_MESSAGE_RECV_CHANNEL_DATA_BUFFER_LEN;
    pTurnConnection->dataBufferSize = DEFAULT_TURN_MESSAGE_SEND_CHANNEL_DATA_BUFFER_LEN;
    pTurnConnection->sendDataBuffer = (PBYTE) (pTurnConnection + 1);
    pTurnConnection->recvDataBuffer = pTurnConnection->sendDataBuffer + pTurnConnection->dataBufferSize;
    pTurnConnection->completeChannelDataBuffer =
        pTurnConnection->sendDataBuffer + pTurnConnection->dataBufferSize + pTurnConnection->recvDataBufferSize;
    pTurnConnection->currRecvDataLen = 0;
    pTurnConnection->allocationExpirationTime = INVALID_TIMESTAMP_VALUE;
    pTurnConnection->nextAllocationRefreshTime = 0;
    pTurnConnection->currentTimerCallingPeriod = DEFAULT_TURN_TIMER_INTERVAL_BEFORE_READY;

    SNPRINTF(turnStateMachineName, MAX_STATE_MACHINE_NAME_LENGTH, "%s-%p", TURN_STATE_MACHINE_NAME, (PVOID) pTurnConnection);
    CHK_STATUS(createStateMachineWithName(TURN_CONNECTION_STATE_MACHINE_STATES, TURN_CONNECTION_STATE_MACHINE_STATE_COUNT, (UINT64) pTurnConnection,
                                          turnConnectionGetTime, (UINT64) pTurnConnection, turnStateMachineName, &pTurnConnection->pStateMachine));

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (STATUS_FAILED(retStatus) && pTurnConnection != NULL) {
        freeTurnConnection(&pTurnConnection);
    }

    if (ppTurnConnection != NULL) {
        *ppTurnConnection = pTurnConnection;
    }

    LEAVES();
    return retStatus;
}

UINT64 turnConnectionGetTime(UINT64 customData)
{
    UNUSED_PARAM(customData);
    return GETTIME();
}

STATUS freeTurnConnection(PTurnConnection* ppTurnConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = NULL;
    PTurnPeer pTurnPeer = NULL;
    SIZE_T timerCallbackId;
    UINT32 i;

    CHK(ppTurnConnection != NULL, STATUS_NULL_ARG);
    // free is idempotent
    CHK(*ppTurnConnection != NULL, retStatus);

    pTurnConnection = *ppTurnConnection;

    // Ensure we are not freeing everything without cancelling the timer
    timerCallbackId = ATOMIC_EXCHANGE(&pTurnConnection->timerCallbackId, MAX_UINT32);
    if (timerCallbackId != MAX_UINT32) {
        CHK_LOG_ERR(timerQueueCancelTimer(pTurnConnection->timerQueueHandle, (UINT32) timerCallbackId, (UINT64) pTurnConnection));
    }
    // shutdown control channel
    if (pTurnConnection->pControlChannel) {
        CHK_LOG_ERR(connectionListenerRemoveConnection(pTurnConnection->pConnectionListener, pTurnConnection->pControlChannel));
        CHK_LOG_ERR(freeSocketConnection(&pTurnConnection->pControlChannel));
    }

    // free transactionId store for each turn peer
    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        pTurnPeer = &pTurnConnection->turnPeerList[i];
        freeTransactionIdStore(&pTurnPeer->pTransactionIdStore);
    }

    if (IS_VALID_MUTEX_VALUE(pTurnConnection->lock)) {
        /* in case some thread is in the middle of a turn api call. */
        MUTEX_LOCK(pTurnConnection->lock);
        MUTEX_UNLOCK(pTurnConnection->lock);
        MUTEX_FREE(pTurnConnection->lock);
    }

    if (IS_VALID_MUTEX_VALUE(pTurnConnection->sendLock)) {
        /* in case some thread is in the middle of a turn api call. */
        MUTEX_LOCK(pTurnConnection->sendLock);
        MUTEX_UNLOCK(pTurnConnection->sendLock);
        MUTEX_FREE(pTurnConnection->sendLock);
    }

    if (IS_VALID_CVAR_VALUE(pTurnConnection->freeAllocationCvar)) {
        CVAR_FREE(pTurnConnection->freeAllocationCvar);
    }

    freeStateMachine(pTurnConnection->pStateMachine);

    turnConnectionFreePreAllocatedPackets(pTurnConnection);

    MEMFREE(pTurnConnection);

    *ppTurnConnection = NULL;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS turnConnectionIncomingDataHandler(PTurnConnection pTurnConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc, PKvsIpAddress pDest,
                                         PTurnChannelData channelDataList, PUINT32 pChannelDataCount)
{
    ENTERS();
    UNUSED_PARAM(pSrc);
    UNUSED_PARAM(pDest);
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 remainingDataSize = 0, processedDataLen, channelDataCount = 0, totalChannelDataCount = 0, channelDataListSize;
    PBYTE pCurrent = NULL;

    CHK(pTurnConnection != NULL && channelDataList != NULL && pChannelDataCount != NULL, STATUS_NULL_ARG);
    CHK_WARN(bufferLen > 0 && pBuffer != NULL, retStatus, "Got empty buffer");

    /* initially pChannelDataCount contains size of channelDataList */
    channelDataListSize = *pChannelDataCount;
    remainingDataSize = bufferLen;
    pCurrent = pBuffer;
    while (remainingDataSize > 0 && totalChannelDataCount < channelDataListSize) {
        processedDataLen = 0;
        channelDataCount = 0;
        if (IS_STUN_PACKET(pCurrent)) {
            processedDataLen = GET_STUN_PACKET_SIZE(pCurrent) + STUN_HEADER_LEN; /* size of entire STUN packet */
            if (STUN_PACKET_IS_TYPE_ERROR(pCurrent)) {
                CHK_STATUS(turnConnectionHandleStunError(pTurnConnection, pCurrent, processedDataLen));
            } else {
                CHK_STATUS(turnConnectionHandleStun(pTurnConnection, pCurrent, processedDataLen));
            }
        } else {
            /* must be channel data if not stun */
            CHK_STATUS(turnConnectionHandleChannelData(pTurnConnection, pCurrent, remainingDataSize, &channelDataList[totalChannelDataCount],
                                                       &channelDataCount, &processedDataLen));
        }

        CHK(remainingDataSize >= processedDataLen, STATUS_INVALID_ARG_LEN);
        pCurrent += processedDataLen;
        remainingDataSize -= processedDataLen;
        /* channelDataCount will be either 1 or 0 */
        totalChannelDataCount += channelDataCount;
    }

    *pChannelDataCount = totalChannelDataCount;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS turnConnectionHandleStun(PTurnConnection pTurnConnection, PBYTE pBuffer, UINT32 bufferLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 stunPacketType = 0;
    PStunAttributeHeader pStunAttr = NULL;
    PStunAttributeAddress pStunAttributeAddress = NULL;
    PStunAttributeLifetime pStunAttributeLifetime = NULL;
    PStunPacket pStunPacket = NULL;
    CHAR profileDebugStr[MAX_TURN_PROFILE_LOG_DESC_LEN];
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    BOOL locked = FALSE;
    ATOMIC_BOOL hasAllocation = FALSE;
    PTurnPeer pTurnPeer = NULL;
    UINT64 currentTime;
    UINT32 i;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);
    CHK(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);
    CHK(IS_STUN_PACKET(pBuffer) && !STUN_PACKET_IS_TYPE_ERROR(pBuffer), retStatus);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    currentTime = GETTIME();
    // only handling STUN response
    stunPacketType = (UINT16) getInt16(*((PUINT16) pBuffer));

    switch (stunPacketType) {
        case STUN_PACKET_TYPE_ALLOCATE_SUCCESS_RESPONSE:
            /* If shutdown has been initiated, ignore the allocation response */
            CHK(!ATOMIC_LOAD(&pTurnConnection->stopTurnConnection), retStatus);
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, pTurnConnection->longTermKey, KVS_MD5_DIGEST_LENGTH, &pStunPacket));
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_RELAYED_ADDRESS, &pStunAttr));
            CHK_WARN(pStunAttr != NULL, retStatus, "No relay address attribute found in TURN allocate response. Dropping Packet");
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_LIFETIME, (PStunAttributeHeader*) &pStunAttributeLifetime));
            CHK_WARN(pStunAttributeLifetime != NULL, retStatus, "Missing lifetime in Allocation response. Dropping Packet");

            // convert lifetime to 100ns and store it
            pTurnConnection->allocationExpirationTime = (pStunAttributeLifetime->lifetime * HUNDREDS_OF_NANOS_IN_A_SECOND) + currentTime;

            pStunAttributeAddress = (PStunAttributeAddress) pStunAttr;
            pTurnConnection->relayAddress = pStunAttributeAddress->address;
            ATOMIC_STORE_BOOL(&pTurnConnection->hasAllocation, TRUE);
            getIpAddrStr(&pTurnConnection->relayAddress, ipAddrStr, ARRAY_SIZE(ipAddrStr));
            SNPRINTF(profileDebugStr, MAX_TURN_PROFILE_LOG_DESC_LEN, "%p - %s:%d - %s", (PVOID) pTurnConnection, ipAddrStr,
                     pTurnConnection->relayAddress.port, "TURN allocation");
            DLOGD("[%p - %s:%d] TURN Allocation succeeded. Life time: %u seconds. Allocation expiration epoch %" PRIu64, pTurnConnection, ipAddrStr,
                  pTurnConnection->relayAddress.port, pStunAttributeLifetime->lifetime,
                  pTurnConnection->allocationExpirationTime / DEFAULT_TIME_UNIT_IN_NANOS);
            PROFILE_WITH_START_TIME_OBJ(pTurnConnection->turnProfileDiagnostics.createAllocationStartTime,
                                        pTurnConnection->turnProfileDiagnostics.createAllocationTime, profileDebugStr);

            if (!pTurnConnection->relayAddressReported && pTurnConnection->turnConnectionCallbacks.relayAddressAvailableFn != NULL) {
                pTurnConnection->relayAddressReported = TRUE;

                // release lock early and report relay candidate
                MUTEX_UNLOCK(pTurnConnection->lock);
                locked = FALSE;

                pTurnConnection->turnConnectionCallbacks.relayAddressAvailableFn(pTurnConnection->turnConnectionCallbacks.customData,
                                                                                 &pTurnConnection->relayAddress, pTurnConnection->pControlChannel);
            }

            break;

        case STUN_PACKET_TYPE_REFRESH_SUCCESS_RESPONSE:
            CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, pTurnConnection->longTermKey, KVS_MD5_DIGEST_LENGTH, &pStunPacket));
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_LIFETIME, (PStunAttributeHeader*) &pStunAttributeLifetime));
            CHK_WARN(pStunAttributeLifetime != NULL, retStatus, "No lifetime attribute found in TURN refresh response. Dropping Packet");

            if (pStunAttributeLifetime->lifetime == 0) {
                hasAllocation = ATOMIC_EXCHANGE_BOOL(&pTurnConnection->hasAllocation, FALSE);
                CHK(hasAllocation, retStatus);
                DLOGD("TURN Allocation freed.");
                CVAR_SIGNAL(pTurnConnection->freeAllocationCvar);
            } else {
                // convert lifetime to 100ns and store it
                pTurnConnection->allocationExpirationTime = (pStunAttributeLifetime->lifetime * HUNDREDS_OF_NANOS_IN_A_SECOND) + currentTime;
                DLOGD("Refreshed TURN allocation lifetime is %u seconds. Allocation expiration epoch %" PRIu64, pStunAttributeLifetime->lifetime,
                      pTurnConnection->allocationExpirationTime / DEFAULT_TIME_UNIT_IN_NANOS);
            }

            break;

        case STUN_PACKET_TYPE_CREATE_PERMISSION_SUCCESS_RESPONSE:
            for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
                pTurnPeer = &pTurnConnection->turnPeerList[i];
                if (transactionIdStoreHasId(pTurnPeer->pTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET)) {
                    if (pTurnPeer->connectionState == TURN_PEER_CONN_STATE_CREATE_PERMISSION) {
                        pTurnPeer->connectionState = TURN_PEER_CONN_STATE_BIND_CHANNEL;
                        CHK_STATUS(getIpAddrStr(&pTurnPeer->address, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
                        DLOGD("[%p] Create permission succeeded for peer %s:%d", pTurnConnection, ipAddrStr, pTurnPeer->address.port);
                        if (pTurnPeer->firstTimeCreatePermResponse) {
                            pTurnPeer->firstTimeCreatePermResponse = FALSE;
                            SNPRINTF(profileDebugStr, MAX_TURN_PROFILE_LOG_DESC_LEN, "%p - %s:%d - %s", (PVOID) pTurnConnection, ipAddrStr,
                                     pTurnPeer->address.port, "TURN create permission");
                            PROFILE_WITH_START_TIME_OBJ(pTurnPeer->createPermissionStartTime, pTurnPeer->createPermissionTime, profileDebugStr);
                        }
                    }

                    pTurnPeer->permissionExpirationTime = TURN_PERMISSION_LIFETIME + currentTime;
                }
            }

            break;

        case STUN_PACKET_TYPE_CHANNEL_BIND_SUCCESS_RESPONSE:
            for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
                pTurnPeer = &pTurnConnection->turnPeerList[i];
                if (pTurnPeer->connectionState == TURN_PEER_CONN_STATE_BIND_CHANNEL &&
                    transactionIdStoreHasId(pTurnPeer->pTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET)) {
                    // pTurnPeer->ready means this peer is ready to receive data. pTurnPeer->connectionState could
                    // change after reaching TURN_PEER_CONN_STATE_READY due to refreshing permission and channel.
                    if (!pTurnPeer->ready) {
                        pTurnPeer->ready = TRUE;
                    }
                    pTurnPeer->connectionState = TURN_PEER_CONN_STATE_READY;

                    CHK_STATUS(getIpAddrStr(&pTurnPeer->address, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
                    DLOGD("[%p] Channel bind succeeded with peer %s, port: %d, channel number %u", pTurnConnection, ipAddrStr,
                          pTurnPeer->address.port, pTurnPeer->channelNumber);
                    if (pTurnPeer->firstTimeBindChannelResponse) {
                        pTurnPeer->firstTimeBindChannelResponse = FALSE;
                        SNPRINTF(profileDebugStr, MAX_TURN_PROFILE_LOG_DESC_LEN, "%p - %s:%d:%u - %s", (PVOID) pTurnConnection, ipAddrStr,
                                 pTurnPeer->address.port, pTurnPeer->channelNumber, "TURN bind channel");
                        PROFILE_WITH_START_TIME_OBJ(pTurnPeer->bindChannelStartTime, pTurnPeer->bindChannelTime, profileDebugStr);
                    }

                    break;
                }
            }

            break;

        case STUN_PACKET_TYPE_DATA_INDICATION:
            DLOGD("Received data indication");
            // no-ops for now, turn server has to use data channel if it is established. client is free to use either.
            break;

        default:
            DLOGD("Drop unsupported TURN message with type 0x%02x", stunPacketType);
            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    if (pStunPacket != NULL) {
        freeStunPacket(&pStunPacket);
    }

    LEAVES();
    return retStatus;
}

STATUS turnConnectionHandleStunError(PTurnConnection pTurnConnection, PBYTE pBuffer, UINT32 bufferLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT16 stunPacketType = 0;
    PStunAttributeHeader pStunAttr = NULL;
    PStunAttributeErrorCode pStunAttributeErrorCode = NULL;
    PStunAttributeNonce pStunAttributeNonce = NULL;
    PStunAttributeRealm pStunAttributeRealm = NULL;
    PStunPacket pStunPacket = NULL;
    BOOL locked = FALSE, iterate = TRUE;
    PTurnPeer pTurnPeer = NULL;
    CHAR profileDebugStr[MAX_TURN_PROFILE_LOG_DESC_LEN];
    UINT32 i;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);
    CHK(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);
    CHK(STUN_PACKET_IS_TYPE_ERROR(pBuffer), retStatus);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    stunPacketType = (UINT16) getInt16(*((PUINT16) pBuffer));
    /* we could get errors like expired nonce after sending the deallocation packet. The allocate would have been
     * deallocated even if the response is error response, and if we try to deallocate again we would get invalid
     * allocation error. Therefore if we get an error after we've sent the deallocation packet, consider the
     * deallocation process finished. */
    if (pTurnConnection->deallocatePacketSent) {
        ATOMIC_STORE_BOOL(&pTurnConnection->hasAllocation, FALSE);
        CHK(FALSE, retStatus);
    }

    if (pTurnConnection->credentialObtained) {
        retStatus = deserializeStunPacket(pBuffer, bufferLen, pTurnConnection->longTermKey, KVS_MD5_DIGEST_LENGTH, &pStunPacket);
    }
    /* if deserializing with password didnt work, try deserialize without password again */
    if (!pTurnConnection->credentialObtained || STATUS_FAILED(retStatus)) {
        CHK_STATUS(deserializeStunPacket(pBuffer, bufferLen, NULL, 0, &pStunPacket));
        retStatus = STATUS_SUCCESS;
    }

    CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_ERROR_CODE, &pStunAttr));
    CHK_WARN(pStunAttr != NULL, retStatus, "No error code attribute found in Stun Error response. Dropping Packet");
    pStunAttributeErrorCode = (PStunAttributeErrorCode) pStunAttr;

    switch (pStunAttributeErrorCode->errorCode) {
        case STUN_ERROR_UNAUTHORIZED:
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_NONCE, &pStunAttr));
            CHK_WARN(pStunAttr != NULL, retStatus, "No Nonce attribute found in Allocate Error response. Dropping Packet");
            pStunAttributeNonce = (PStunAttributeNonce) pStunAttr;
            CHK_WARN(pStunAttributeNonce->attribute.length <= STUN_MAX_NONCE_LEN, retStatus,
                     "Invalid Nonce found in Allocate Error response. Dropping Packet");
            pTurnConnection->nonceLen = pStunAttributeNonce->attribute.length;
            MEMCPY(pTurnConnection->turnNonce, pStunAttributeNonce->nonce, pTurnConnection->nonceLen);

            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_REALM, &pStunAttr));
            CHK_WARN(pStunAttr != NULL, retStatus, "No Realm attribute found in Allocate Error response. Dropping Packet");
            pStunAttributeRealm = (PStunAttributeRealm) pStunAttr;
            CHK_WARN(pStunAttributeRealm->attribute.length <= STUN_MAX_REALM_LEN, retStatus,
                     "Invalid Realm found in Allocate Error response. Dropping Packet");
            // pStunAttributeRealm->attribute.length does not include null terminator and pStunAttributeRealm->realm is not null terminated
            STRNCPY(pTurnConnection->turnRealm, pStunAttributeRealm->realm, pStunAttributeRealm->attribute.length);
            pTurnConnection->turnRealm[pStunAttributeRealm->attribute.length] = '\0';

            pTurnConnection->credentialObtained = TRUE;
            SNPRINTF(profileDebugStr, MAX_TURN_PROFILE_LOG_DESC_LEN, "%p - %s", (PVOID) pTurnConnection, "TURN Get Credentials");
            PROFILE_WITH_START_TIME_OBJ(pTurnConnection->turnProfileDiagnostics.getCredentialsStartTime,
                                        pTurnConnection->turnProfileDiagnostics.getCredentialsTime, profileDebugStr);
            CHK_STATUS(turnConnectionUpdateNonce(pTurnConnection));
            break;

        case STUN_ERROR_STALE_NONCE:
            DLOGD("Updating stale nonce");
            CHK_STATUS(getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_NONCE, &pStunAttr));
            CHK_WARN(pStunAttr != NULL, retStatus, "No Nonce attribute found in Refresh Error response. Dropping Packet");
            pStunAttributeNonce = (PStunAttributeNonce) pStunAttr;
            CHK_WARN(pStunAttributeNonce->attribute.length <= STUN_MAX_NONCE_LEN, retStatus,
                     "Invalid Nonce found in Refresh Error response. Dropping Packet");
            pTurnConnection->nonceLen = pStunAttributeNonce->attribute.length;
            MEMCPY(pTurnConnection->turnNonce, pStunAttributeNonce->nonce, pTurnConnection->nonceLen);

            CHK_STATUS(turnConnectionUpdateNonce(pTurnConnection));
            break;

        default:
            /* Remove peer for any other error */
            DLOGW("Received STUN error response. Error type: 0x%02x, Error Code: %u. attribute len %u, Error detail: %s.", stunPacketType,
                  pStunAttributeErrorCode->errorCode, pStunAttributeErrorCode->attribute.length, pStunAttributeErrorCode->errorPhrase);
            BOOL found = FALSE;
            /* Find TurnPeer using transaction Id, then mark it as failed */
            for (i = 0; iterate && i < pTurnConnection->turnPeerCount; ++i) {
                pTurnPeer = &pTurnConnection->turnPeerList[i];
                if (transactionIdStoreHasId(pTurnPeer->pTransactionIdStore, pBuffer + STUN_PACKET_TRANSACTION_ID_OFFSET)) {
                    CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
                    pTurnPeer->connectionState = TURN_PEER_CONN_STATE_FAILED;
                    iterate = FALSE;
                    found = TRUE;
                    getIpAddrStr(&pTurnPeer->address, ipAddr, ARRAY_SIZE(ipAddr));
                    DLOGD("remove turn peer with ip: %s:%u. family:%d", ipAddr, (UINT16) getInt16(pTurnPeer->address.port),
                          pTurnPeer->address.family);
                }
            }
            if (found == FALSE) {
                DLOGD("can not find any corresponding turn peer");
            }
            break;
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    if (pStunPacket != NULL) {
        freeStunPacket(&pStunPacket);
    }

    LEAVES();
    return retStatus;
}

STATUS turnConnectionHandleChannelData(PTurnConnection pTurnConnection, PBYTE pBuffer, UINT32 bufferLen, PTurnChannelData pChannelData,
                                       PUINT32 pChannelDataCount, PUINT32 pProcessedDataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;

    UINT32 turnChannelDataCount = 0, hexStrLen = 0;
    UINT16 channelNumber = 0;
    PTurnPeer pTurnPeer = NULL;
    PCHAR hexStr = NULL;

    CHK(pTurnConnection != NULL && pChannelData != NULL && pChannelDataCount != NULL && pProcessedDataLen != NULL, STATUS_NULL_ARG);
    CHK(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->protocol == KVS_SOCKET_PROTOCOL_UDP) {
        channelNumber = (UINT16) getInt16(*(PINT16) pBuffer);
        if ((pTurnPeer = turnConnectionGetPeerWithChannelNumber(pTurnConnection, channelNumber)) != NULL) {
            /*
             * Not expecting fragmented channel message in UDP mode.
             * Data channel messages from UDP connection may or may not padded. Thus turnConnectionHandleChannelDataTcpMode wont
             * be able to parse it.
             */
            pChannelData->data = pBuffer + TURN_DATA_CHANNEL_SEND_OVERHEAD;
            pChannelData->size = GET_STUN_PACKET_SIZE(pBuffer);
            pChannelData->senderAddr = pTurnPeer->address;
            turnChannelDataCount = 1;

            if (pChannelData->size + TURN_DATA_CHANNEL_SEND_OVERHEAD < bufferLen) {
                DLOGD("Not expecting multiple channel data messages in one UDP packet.");
            }

        } else {
            CHK_STATUS(hexEncode(pBuffer, bufferLen, NULL, &hexStrLen));
            hexStr = MEMCALLOC(1, hexStrLen * SIZEOF(CHAR));
            CHK(hexStr != NULL, STATUS_NOT_ENOUGH_MEMORY);
            CHK_STATUS(hexEncode(pBuffer, bufferLen, hexStr, &hexStrLen));
            DLOGE("Turn connection does not have channel number, dumping payload: %s", hexStr);
            turnChannelDataCount = 0;

            SAFE_MEMFREE(hexStr);
        }
        *pProcessedDataLen = bufferLen;

    } else {
        CHK_STATUS(
            turnConnectionHandleChannelDataTcpMode(pTurnConnection, pBuffer, bufferLen, pChannelData, &turnChannelDataCount, pProcessedDataLen));
    }

    *pChannelDataCount = turnChannelDataCount;

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (hexStr != NULL) {
        SAFE_MEMFREE(hexStr);
    }
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

/*
 * turnConnectionHandleChannelDataTcpMode will process a single turn channel data item from pBuffer then return.
 * If there is a complete channel data item in buffer, upon return *pTurnChannelDataCount will be 1, *pTurnChannelData
 * will data details about the parsed channel data. *pProcessedDataLen will be the length of data processed.
 */
STATUS turnConnectionHandleChannelDataTcpMode(PTurnConnection pTurnConnection, PBYTE pBuffer, UINT32 bufferLen, PTurnChannelData pChannelData,
                                              PUINT32 pTurnChannelDataCount, PUINT32 pProcessedDataLen)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 bytesToCopy = 0, remainingMsgSize = 0, paddedChannelDataLen = 0, remainingBufLen = 0, channelDataCount = 0;
    PBYTE pCurPos = NULL;
    UINT16 channelNumber = 0;
    PTurnPeer pTurnPeer = NULL;

    CHK(pTurnConnection != NULL && pChannelData != NULL && pTurnChannelDataCount != NULL && pProcessedDataLen != NULL, STATUS_NULL_ARG);
    CHK(pBuffer != NULL && bufferLen > 0, STATUS_INVALID_ARG);

    pCurPos = pBuffer;
    remainingBufLen = bufferLen;
    /* process only one channel data and return. Because channel data can be intermixed with STUN packet.
     * need to check remainingBufLen too because channel data could be incomplete. */
    while (remainingBufLen != 0 && channelDataCount == 0) {
        if (pTurnConnection->currRecvDataLen != 0) {
            DLOGV("currRecvDataLen: %d", pTurnConnection->currRecvDataLen);
            if (pTurnConnection->currRecvDataLen >= TURN_DATA_CHANNEL_SEND_OVERHEAD) {
                /* pTurnConnection->recvDataBuffer always has channel data start */
                paddedChannelDataLen = ROUND_UP((UINT32) getInt16(*(PINT16) (pTurnConnection->recvDataBuffer + SIZEOF(channelNumber))), 4);
                remainingMsgSize = paddedChannelDataLen - (pTurnConnection->currRecvDataLen - TURN_DATA_CHANNEL_SEND_OVERHEAD);
                bytesToCopy = MIN(remainingMsgSize, remainingBufLen);
                remainingBufLen -= bytesToCopy;

                if (bytesToCopy > (pTurnConnection->recvDataBufferSize - pTurnConnection->currRecvDataLen)) {
                    /* drop current message if it is longer than buffer size. */
                    pTurnConnection->currRecvDataLen = 0;
                    CHK(FALSE, STATUS_BUFFER_TOO_SMALL);
                }

                MEMCPY(pTurnConnection->recvDataBuffer + pTurnConnection->currRecvDataLen, pCurPos, bytesToCopy);
                pTurnConnection->currRecvDataLen += bytesToCopy;
                pCurPos += bytesToCopy;

                CHECK_EXT(pTurnConnection->currRecvDataLen <= paddedChannelDataLen + TURN_DATA_CHANNEL_SEND_OVERHEAD,
                          "Should not store more than one channel data message in recvDataBuffer");

                /*
                 * once assembled a complete channel data in recvDataBuffer, copy over to completeChannelDataBuffer to
                 * make room for subsequent partial channel data.
                 */
                if (pTurnConnection->currRecvDataLen == (paddedChannelDataLen + TURN_DATA_CHANNEL_SEND_OVERHEAD)) {
                    channelNumber = (UINT16) getInt16(*(PINT16) pTurnConnection->recvDataBuffer);
                    if ((pTurnPeer = turnConnectionGetPeerWithChannelNumber(pTurnConnection, channelNumber)) != NULL) {
                        MEMCPY(pTurnConnection->completeChannelDataBuffer, pTurnConnection->recvDataBuffer, pTurnConnection->currRecvDataLen);
                        pChannelData->data = pTurnConnection->completeChannelDataBuffer + TURN_DATA_CHANNEL_SEND_OVERHEAD;
                        pChannelData->size = GET_STUN_PACKET_SIZE(pTurnConnection->completeChannelDataBuffer);
                        pChannelData->senderAddr = pTurnPeer->address;
                        channelDataCount++;
                    }

                    pTurnConnection->currRecvDataLen = 0;
                }
            } else {
                /* copy just enough to make a complete channel data header */
                bytesToCopy = MIN(remainingMsgSize, TURN_DATA_CHANNEL_SEND_OVERHEAD - pTurnConnection->currRecvDataLen);
                MEMCPY(pTurnConnection->recvDataBuffer + pTurnConnection->currRecvDataLen, pCurPos, bytesToCopy);
                pTurnConnection->currRecvDataLen += bytesToCopy;
                pCurPos += bytesToCopy;
            }
        } else {
            /* new channel message start */
            CHK(*pCurPos == TURN_DATA_CHANNEL_MSG_FIRST_BYTE, STATUS_TURN_MISSING_CHANNEL_DATA_HEADER);

            paddedChannelDataLen = ROUND_UP((UINT32) getInt16(*(PINT16) (pCurPos + SIZEOF(UINT16))), 4);
            if (remainingBufLen >= (paddedChannelDataLen + TURN_DATA_CHANNEL_SEND_OVERHEAD)) {
                channelNumber = (UINT16) getInt16(*(PINT16) pCurPos);
                if ((pTurnPeer = turnConnectionGetPeerWithChannelNumber(pTurnConnection, channelNumber)) != NULL) {
                    pChannelData->data = pCurPos + TURN_DATA_CHANNEL_SEND_OVERHEAD;
                    pChannelData->size = GET_STUN_PACKET_SIZE(pCurPos);
                    pChannelData->senderAddr = pTurnPeer->address;
                    channelDataCount++;
                }

                remainingBufLen -= (paddedChannelDataLen + TURN_DATA_CHANNEL_SEND_OVERHEAD);
                pCurPos += (paddedChannelDataLen + TURN_DATA_CHANNEL_SEND_OVERHEAD);
            } else {
                CHK(pTurnConnection->currRecvDataLen == 0, STATUS_TURN_NEW_DATA_CHANNEL_MSG_HEADER_BEFORE_PREVIOUS_MSG_FINISH);
                CHK(remainingBufLen <= (pTurnConnection->recvDataBufferSize), STATUS_BUFFER_TOO_SMALL);

                MEMCPY(pTurnConnection->recvDataBuffer, pCurPos, remainingBufLen);
                pTurnConnection->currRecvDataLen += remainingBufLen;
                pCurPos += remainingBufLen;
                remainingBufLen = 0;
            }
        }
    }

    /* return actual channel data count */
    *pTurnChannelDataCount = channelDataCount;
    *pProcessedDataLen = bufferLen - remainingBufLen;

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

STATUS turnConnectionAddPeer(PTurnConnection pTurnConnection, PKvsIpAddress pPeerAddress)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnPeer pTurnPeer = NULL;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pPeerAddress != NULL, STATUS_NULL_ARG);
    CHK(pTurnConnection->turnServer.ipAddress.family == pPeerAddress->family, STATUS_INVALID_ARG);
    CHK_WARN(IS_IPV4_ADDR(pPeerAddress), retStatus, "Drop IPv6 turn peer because only IPv4 turn peer is supported right now");

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    /* check for duplicate */
    CHK(turnConnectionGetPeerWithIp(pTurnConnection, pPeerAddress) == NULL, retStatus);
    CHK_WARN(pTurnConnection->turnPeerCount < DEFAULT_TURN_MAX_PEER_COUNT, STATUS_INVALID_OPERATION, "Add peer failed. Max peer count reached");

    pTurnPeer = &pTurnConnection->turnPeerList[pTurnConnection->turnPeerCount++];

    pTurnPeer->connectionState = TURN_PEER_CONN_STATE_CREATE_PERMISSION;
    pTurnPeer->address = *pPeerAddress;
    pTurnPeer->xorAddress = *pPeerAddress;
    /* safe to down cast because DEFAULT_TURN_MAX_PEER_COUNT is enforced */
    pTurnPeer->channelNumber = (UINT16) pTurnConnection->turnPeerCount + TURN_CHANNEL_BIND_CHANNEL_NUMBER_BASE;
    pTurnPeer->permissionExpirationTime = INVALID_TIMESTAMP_VALUE;
    pTurnPeer->ready = FALSE;
    pTurnPeer->firstTimeCreatePermReq = TRUE;
    pTurnPeer->firstTimeBindChannelReq = TRUE;
    pTurnPeer->firstTimeCreatePermResponse = TRUE;
    pTurnPeer->firstTimeBindChannelResponse = TRUE;

    CHK_STATUS(xorIpAddress(&pTurnPeer->xorAddress, NULL)); /* only work for IPv4 for now */
    CHK_STATUS(createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pTurnPeer->pTransactionIdStore));
    pTurnPeer = NULL;

CleanUp:

    if (STATUS_FAILED(retStatus) && pTurnPeer != NULL) {
        freeTransactionIdStore(&pTurnPeer->pTransactionIdStore);
        pTurnConnection->turnPeerCount--;
    }

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS turnConnectionSendData(PTurnConnection pTurnConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pDestIp)
{
    STATUS retStatus = STATUS_SUCCESS;
    PTurnPeer pSendPeer = NULL;
    UINT32 paddedDataLen = 0;
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    BOOL locked = FALSE;
    BOOL sendLocked = FALSE;

    CHK(pTurnConnection != NULL && pDestIp != NULL, STATUS_NULL_ARG);
    CHK(pBuf != NULL && bufLen > 0, STATUS_INVALID_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (!(pTurnConnection->state == TURN_STATE_CREATE_PERMISSION || pTurnConnection->state == TURN_STATE_BIND_CHANNEL ||
          pTurnConnection->state == TURN_STATE_READY)) {
        DLOGV("TurnConnection not ready to send data");

        // If turn is not ready yet. Drop the send since ice will retry.
        CHK(FALSE, retStatus);
    }

    pSendPeer = turnConnectionGetPeerWithIp(pTurnConnection, pDestIp);

    CHK_STATUS(getIpAddrStr(pDestIp, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
    if (pSendPeer == NULL) {
        DLOGV("Unable to send data through turn because peer with address %s:%u is not found", ipAddrStr, KVS_GET_IP_ADDRESS_PORT(pDestIp));
        CHK(FALSE, retStatus);
    } else if (pSendPeer->connectionState == TURN_PEER_CONN_STATE_FAILED) {
        CHK(FALSE, STATUS_TURN_CONNECTION_PEER_NOT_USABLE);
    } else if (!pSendPeer->ready) {
        DLOGV("Unable to send data through turn because turn channel is not established with peer with address %s:%u", ipAddrStr,
              KVS_GET_IP_ADDRESS_PORT(pDestIp));
        CHK(FALSE, retStatus);
    }

    MUTEX_UNLOCK(pTurnConnection->lock);
    locked = FALSE;

    /* need to serialize send because every send load data into the same buffer pTurnConnection->sendDataBuffer */
    MUTEX_LOCK(pTurnConnection->sendLock);
    sendLocked = TRUE;

    CHK(pTurnConnection->dataBufferSize - TURN_DATA_CHANNEL_SEND_OVERHEAD >= bufLen, STATUS_BUFFER_TOO_SMALL);

    paddedDataLen = (UINT32) ROUND_UP(TURN_DATA_CHANNEL_SEND_OVERHEAD + bufLen, 4);

    /* generate data channel TURN message */
    putInt16((PINT16) (pTurnConnection->sendDataBuffer), pSendPeer->channelNumber);
    putInt16((PINT16) (pTurnConnection->sendDataBuffer + 2), (UINT16) bufLen);
    MEMCPY(pTurnConnection->sendDataBuffer + TURN_DATA_CHANNEL_SEND_OVERHEAD, pBuf, bufLen);

    retStatus = iceUtilsSendData(pTurnConnection->sendDataBuffer, paddedDataLen, &pTurnConnection->turnServer.ipAddress,
                                 pTurnConnection->pControlChannel, NULL, FALSE);

    if (STATUS_FAILED(retStatus)) {
        DLOGW("iceUtilsSendData failed with 0x%08x", retStatus);
        if (retStatus != STATUS_SOCKET_CONNECTION_CLOSED_ALREADY) {
            retStatus = STATUS_SUCCESS;
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (sendLocked) {
        MUTEX_UNLOCK(pTurnConnection->sendLock);
    }

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    return retStatus;
}

STATUS turnConnectionStart(PTurnConnection pTurnConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL locked = FALSE;
    SIZE_T timerCallbackId;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    /* only execute when turnConnection is in TURN_STATE_NEW */
    CHK(pTurnConnection->state == TURN_STATE_NEW, retStatus);

    MUTEX_UNLOCK(pTurnConnection->lock);
    locked = FALSE;

    timerCallbackId = ATOMIC_EXCHANGE(&pTurnConnection->timerCallbackId, MAX_UINT32);
    if (timerCallbackId != MAX_UINT32) {
        CHK_STATUS(timerQueueCancelTimer(pTurnConnection->timerQueueHandle, (UINT32) timerCallbackId, (UINT64) pTurnConnection));
    }

    /* schedule the timer, which will drive the state machine. */
    CHK_STATUS(timerQueueAddTimer(pTurnConnection->timerQueueHandle, KVS_ICE_DEFAULT_TIMER_START_DELAY, pTurnConnection->currentTimerCallingPeriod,
                                  turnConnectionTimerCallback, (UINT64) pTurnConnection, (PUINT32) &timerCallbackId));

    ATOMIC_STORE(&pTurnConnection->timerCallbackId, timerCallbackId);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    return retStatus;
}

STATUS turnConnectionRefreshAllocation(PTurnConnection pTurnConnection)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currTime = 0;
    PStunAttributeLifetime pStunAttributeLifetime = NULL;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    currTime = GETTIME();
    // return early if we are not in grace period yet or if we just sent refresh earlier
    CHK(IS_VALID_TIMESTAMP(pTurnConnection->allocationExpirationTime) &&
            currTime + DEFAULT_TURN_ALLOCATION_REFRESH_GRACE_PERIOD >= pTurnConnection->allocationExpirationTime &&
            currTime >= pTurnConnection->nextAllocationRefreshTime,
        retStatus);

    DLOGD("Refresh turn allocation");

    CHK_STATUS(getStunAttribute(pTurnConnection->pTurnAllocationRefreshPacket, STUN_ATTRIBUTE_TYPE_LIFETIME,
                                (PStunAttributeHeader*) &pStunAttributeLifetime));
    CHK(pStunAttributeLifetime != NULL, STATUS_INTERNAL_ERROR);

    pStunAttributeLifetime->lifetime = DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS;

    CHK_STATUS(iceUtilsSendStunPacket(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->longTermKey,
                                      ARRAY_SIZE(pTurnConnection->longTermKey), &pTurnConnection->turnServer.ipAddress,
                                      pTurnConnection->pControlChannel, NULL, FALSE));

    pTurnConnection->nextAllocationRefreshTime = currTime + DEFAULT_TURN_SEND_REFRESH_INVERVAL;

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS turnConnectionRefreshPermission(PTurnConnection pTurnConnection, PBOOL pNeedRefresh)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currTime = 0;
    PTurnPeer pTurnPeer = NULL;
    BOOL needRefresh = FALSE;
    CHAR ipAddr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    UINT32 i;

    CHK(pTurnConnection != NULL && pNeedRefresh != NULL, STATUS_NULL_ARG);

    currTime = GETTIME();

    // refresh all peers whenever one of them expire is close to expiration
    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        pTurnPeer = &pTurnConnection->turnPeerList[i];
        if (IS_VALID_TIMESTAMP(pTurnPeer->permissionExpirationTime) &&
            currTime + DEFAULT_TURN_PERMISSION_REFRESH_GRACE_PERIOD >= pTurnPeer->permissionExpirationTime) {
            getIpAddrStr(&pTurnPeer->address, ipAddr, ARRAY_SIZE(ipAddr));
            DLOGD("[%p] Refreshing turn permission for %s:%d", pTurnConnection, ipAddr, pTurnPeer->address.port);
            needRefresh = TRUE;
        }
    }

CleanUp:

    if (pNeedRefresh != NULL) {
        *pNeedRefresh = needRefresh;
    }

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS turnConnectionFreePreAllocatedPackets(PTurnConnection pTurnConnection)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->pTurnPacket != NULL) {
        CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnPacket));
    }

    if (pTurnConnection->pTurnChannelBindPacket != NULL) {
        CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnChannelBindPacket));
    }

    if (pTurnConnection->pTurnCreatePermissionPacket != NULL) {
        CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnCreatePermissionPacket));
    }

    if (pTurnConnection->pTurnAllocationRefreshPacket != NULL) {
        CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnAllocationRefreshPacket));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS turnConnectionUpdateNonce(PTurnConnection pTurnConnection)
{
    STATUS retStatus = STATUS_SUCCESS;

    // assume holding pTurnConnection->lock

    // update nonce for pre-created packets
    if (pTurnConnection->pTurnPacket != NULL) {
        CHK_STATUS(updateStunNonceAttribute(pTurnConnection->pTurnPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));
    }

    if (pTurnConnection->pTurnAllocationRefreshPacket != NULL) {
        CHK_STATUS(updateStunNonceAttribute(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));
    }

    if (pTurnConnection->pTurnChannelBindPacket != NULL) {
        CHK_STATUS(updateStunNonceAttribute(pTurnConnection->pTurnChannelBindPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));
    }

    if (pTurnConnection->pTurnCreatePermissionPacket != NULL) {
        CHK_STATUS(updateStunNonceAttribute(pTurnConnection->pTurnCreatePermissionPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));
    }

CleanUp:

    CHK_LOG_ERR(retStatus);
    return retStatus;
}

STATUS turnConnectionShutdown(PTurnConnection pTurnConnection, UINT64 waitUntilAllocationFreedTimeout)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 currentTime = 0, timeoutTime = 0;
    BOOL locked = FALSE;
    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    ATOMIC_STORE(&pTurnConnection->stopTurnConnection, TRUE);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    CHK(ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation), retStatus);

    if (waitUntilAllocationFreedTimeout > 0) {
        currentTime = GETTIME();
        timeoutTime = currentTime + waitUntilAllocationFreedTimeout;

        while (ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) && currentTime < timeoutTime) {
            CVAR_WAIT(pTurnConnection->freeAllocationCvar, pTurnConnection->lock, waitUntilAllocationFreedTimeout);
            currentTime = GETTIME();
        }

        if (ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation)) {
            DLOGD("Failed to free turn allocation within timeout of %" PRIu64 " milliseconds",
                  waitUntilAllocationFreedTimeout / HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        }
    }

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    return retStatus;
}

BOOL turnConnectionIsShutdownComplete(PTurnConnection pTurnConnection)
{
    if (pTurnConnection == NULL) {
        return TRUE;
    } else {
        return ATOMIC_LOAD_BOOL(&pTurnConnection->shutdownComplete);
    }
}

BOOL turnConnectionGetRelayAddress(PTurnConnection pTurnConnection, PKvsIpAddress pKvsIpAddress)
{
    if (pTurnConnection == NULL || !ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation)) {
        return FALSE;
    } else if (pKvsIpAddress != NULL) {
        MUTEX_LOCK(pTurnConnection->lock);
        *pKvsIpAddress = pTurnConnection->relayAddress;
        MUTEX_UNLOCK(pTurnConnection->lock);
        return TRUE;
    }

    return FALSE;
}

STATUS checkTurnPeerConnections(PTurnConnection pTurnConnection)
{
    STATUS retStatus = STATUS_SUCCESS, sendStatus = STATUS_SUCCESS;
    PTurnPeer pTurnPeer = NULL;
    PStunAttributeAddress pStunAttributeAddress = NULL;
    PStunAttributeChannelNumber pStunAttributeChannelNumber = NULL;
    UINT32 i = 0;

    UNUSED_PARAM(sendStatus);

    // turn mutex is assumed to be locked.
    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);
    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        pTurnPeer = &pTurnConnection->turnPeerList[i];

        if (pTurnPeer->connectionState == TURN_PEER_CONN_STATE_CREATE_PERMISSION) {
            if (pTurnPeer->firstTimeCreatePermReq) {
                pTurnPeer->createPermissionStartTime = GETTIME();
                pTurnPeer->firstTimeCreatePermReq = FALSE;
            }
            // update peer address;
            CHK_STATUS(getStunAttribute(pTurnConnection->pTurnCreatePermissionPacket, STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS,
                                        (PStunAttributeHeader*) &pStunAttributeAddress));
            CHK_WARN(pStunAttributeAddress != NULL, STATUS_INTERNAL_ERROR, "xor peer address attribute not found");
            pStunAttributeAddress->address = pTurnPeer->address;

            CHK_STATUS(iceUtilsGenerateTransactionId(pTurnConnection->pTurnCreatePermissionPacket->header.transactionId,
                                                     ARRAY_SIZE(pTurnConnection->pTurnCreatePermissionPacket->header.transactionId)));

            CHK(pTurnPeer->pTransactionIdStore != NULL, STATUS_INVALID_OPERATION);
            transactionIdStoreInsert(pTurnPeer->pTransactionIdStore, pTurnConnection->pTurnCreatePermissionPacket->header.transactionId);
            sendStatus = iceUtilsSendStunPacket(pTurnConnection->pTurnCreatePermissionPacket, pTurnConnection->longTermKey,
                                                ARRAY_SIZE(pTurnConnection->longTermKey), &pTurnConnection->turnServer.ipAddress,
                                                pTurnConnection->pControlChannel, NULL, FALSE);

        } else if (pTurnPeer->connectionState == TURN_PEER_CONN_STATE_BIND_CHANNEL) {
            if (pTurnPeer->firstTimeBindChannelReq) {
                pTurnPeer->bindChannelStartTime = GETTIME();
                pTurnPeer->firstTimeBindChannelReq = FALSE;
            }
            // update peer address;
            CHK_STATUS(getStunAttribute(pTurnConnection->pTurnChannelBindPacket, STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS,
                                        (PStunAttributeHeader*) &pStunAttributeAddress));
            CHK_WARN(pStunAttributeAddress != NULL, STATUS_INTERNAL_ERROR, "xor peer address attribute not found");
            pStunAttributeAddress->address = pTurnPeer->address;

            // update channel number
            CHK_STATUS(getStunAttribute(pTurnConnection->pTurnChannelBindPacket, STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER,
                                        (PStunAttributeHeader*) &pStunAttributeChannelNumber));
            CHK_WARN(pStunAttributeChannelNumber != NULL, STATUS_INTERNAL_ERROR, "channel number attribute not found");
            pStunAttributeChannelNumber->channelNumber = pTurnPeer->channelNumber;

            CHK_STATUS(iceUtilsGenerateTransactionId(pTurnConnection->pTurnChannelBindPacket->header.transactionId,
                                                     ARRAY_SIZE(pTurnConnection->pTurnChannelBindPacket->header.transactionId)));

            CHK(pTurnPeer->pTransactionIdStore != NULL, STATUS_INVALID_OPERATION);
            transactionIdStoreInsert(pTurnPeer->pTransactionIdStore, pTurnConnection->pTurnChannelBindPacket->header.transactionId);
            sendStatus = iceUtilsSendStunPacket(pTurnConnection->pTurnChannelBindPacket, pTurnConnection->longTermKey,
                                                ARRAY_SIZE(pTurnConnection->longTermKey), &pTurnConnection->turnServer.ipAddress,
                                                pTurnConnection->pControlChannel, NULL, FALSE);
        }
    }

    CHK_STATUS(turnConnectionRefreshAllocation(pTurnConnection));

CleanUp:

    CHK_LOG_ERR(retStatus);

    return retStatus;
}

STATUS turnConnectionTimerCallback(UINT32 timerId, UINT64 currentTime, UINT64 customData)
{
    UNUSED_PARAM(timerId);
    UNUSED_PARAM(currentTime);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    BOOL locked = FALSE, stopScheduling = FALSE;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    CHK_STATUS(stepTurnConnectionStateMachine(pTurnConnection));

    stopScheduling = ATOMIC_LOAD_BOOL(&pTurnConnection->shutdownComplete);

CleanUp:

    CHK_LOG_ERR(retStatus);

    if (stopScheduling) {
        retStatus = STATUS_TIMER_QUEUE_STOP_SCHEDULING;
        if (pTurnConnection != NULL) {
            ATOMIC_STORE(&pTurnConnection->timerCallbackId, MAX_UINT32);
        }
    }

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    return retStatus;
}

STATUS turnConnectionGetLongTermKey(PCHAR username, PCHAR realm, PCHAR password, PBYTE pBuffer, UINT32 bufferLen)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR stringBuffer[STUN_MAX_USERNAME_LEN + MAX_ICE_CONFIG_CREDENTIAL_LEN + STUN_MAX_REALM_LEN + 2]; // 2 for two ":" between each value
    INT32 amountWritten = 0;

    CHK(username != NULL && realm != NULL && password != NULL && pBuffer != NULL, STATUS_NULL_ARG);
    CHK(username[0] != '\0' && realm[0] != '\0' && password[0] != '\0' && bufferLen >= KVS_MD5_DIGEST_LENGTH, STATUS_INVALID_ARG);
    CHK((STRLEN(username) + STRLEN(realm) + STRLEN(password)) <= ARRAY_SIZE(stringBuffer) - 2, STATUS_INVALID_ARG);

    amountWritten = SNPRINTF(stringBuffer, SIZEOF(stringBuffer), "%s:%s:%s", username, realm, password);
    CHK_ERR(amountWritten > 0, STATUS_INTERNAL_ERROR, "SNPRINTF error: Failed to generate the long term key with username, realm and password");

    // TODO: Return back the error check
    KVS_MD5_DIGEST((PBYTE) stringBuffer, STRLEN(stringBuffer), pBuffer);

CleanUp:

    return retStatus;
}

STATUS turnConnectionPackageTurnAllocationRequest(PCHAR username, PCHAR realm, PBYTE nonce, UINT16 nonceLen, UINT32 lifetime,
                                                  PStunPacket* ppStunPacket)
{
    STATUS retStatus = STATUS_SUCCESS;
    PStunPacket pTurnAllocateRequest = NULL;

    CHK(ppStunPacket != NULL, STATUS_NULL_ARG);
    CHK((username == NULL && realm == NULL && nonce == NULL) || (username != NULL && realm != NULL && nonce != NULL && nonceLen > 0),
        STATUS_INVALID_ARG);

    CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_ALLOCATE, NULL, &pTurnAllocateRequest));
    CHK_STATUS(appendStunLifetimeAttribute(pTurnAllocateRequest, lifetime));
    CHK_STATUS(appendStunRequestedTransportAttribute(pTurnAllocateRequest, (UINT8) TURN_REQUEST_TRANSPORT_UDP));

    // either username, realm and nonce are all null or all not null
    if (username != NULL) {
        CHK_STATUS(appendStunUsernameAttribute(pTurnAllocateRequest, username));
        CHK_STATUS(appendStunRealmAttribute(pTurnAllocateRequest, realm));
        CHK_STATUS(appendStunNonceAttribute(pTurnAllocateRequest, nonce, nonceLen));
    }

CleanUp:

    if (STATUS_FAILED(retStatus) && pTurnAllocateRequest != NULL) {
        freeStunPacket(&pTurnAllocateRequest);
        pTurnAllocateRequest = NULL;
    }

    if (pTurnAllocateRequest != NULL && ppStunPacket != NULL) {
        *ppStunPacket = pTurnAllocateRequest;
    }

    return retStatus;
}

PTurnPeer turnConnectionGetPeerWithChannelNumber(PTurnConnection pTurnConnection, UINT16 channelNumber)
{
    PTurnPeer pTurnPeer = NULL;
    UINT32 i = 0;

    for (; i < pTurnConnection->turnPeerCount; ++i) {
        if (pTurnConnection->turnPeerList[i].channelNumber == channelNumber) {
            pTurnPeer = &pTurnConnection->turnPeerList[i];
        }
    }

    return pTurnPeer;
}

PTurnPeer turnConnectionGetPeerWithIp(PTurnConnection pTurnConnection, PKvsIpAddress pKvsIpAddress)
{
    PTurnPeer pTurnPeer = NULL;
    UINT32 i = 0;

    for (; i < pTurnConnection->turnPeerCount; ++i) {
        if (isSameIpAddress(&pTurnConnection->turnPeerList[i].address, pKvsIpAddress, TRUE)) {
            pTurnPeer = &pTurnConnection->turnPeerList[i];
        }
    }

    return pTurnPeer;
}

VOID turnConnectionFatalError(PTurnConnection pTurnConnection, STATUS errorStatus)
{
    if (pTurnConnection == NULL) {
        return;
    }

    /* Assume holding pTurnConnection->lock */
    pTurnConnection->errorStatus = errorStatus;
    pTurnConnection->state = TURN_STATE_FAILED;
}
