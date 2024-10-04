
/**
 * Implementation of a turn connection states machine callbacks
 */
#define LOG_CLASS "TurnConnectionState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState TURN_CONNECTION_STATE_MACHINE_STATES[] = {
    {TURN_STATE_NEW, TURN_STATE_NEW | TURN_STATE_CLEAN_UP, fromNewTurnState, executeNewTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CHECK_SOCKET_CONNECTION, TURN_STATE_NEW | TURN_STATE_CHECK_SOCKET_CONNECTION, fromCheckSocketConnectionTurnState,
     executeCheckSocketConnectionTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_GET_CREDENTIALS, TURN_STATE_CHECK_SOCKET_CONNECTION | TURN_STATE_GET_CREDENTIALS, fromGetCredentialsTurnState,
     executeGetCredentialsTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_ALLOCATION, TURN_STATE_ALLOCATION | TURN_STATE_GET_CREDENTIALS, fromAllocationTurnState, executeAllocationTurnState, NULL,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CREATE_PERMISSION, TURN_STATE_CREATE_PERMISSION | TURN_STATE_ALLOCATION | TURN_STATE_READY, fromCreatePermissionTurnState,
     executeCreatePermissionTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_BIND_CHANNEL, TURN_STATE_BIND_CHANNEL | TURN_STATE_CREATE_PERMISSION, fromBindChannelTurnState, executeBindChannelTurnState, NULL,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_READY, TURN_STATE_READY | TURN_STATE_BIND_CHANNEL, fromReadyTurnState, executeReadyTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CLEAN_UP,
     TURN_STATE_CLEAN_UP | TURN_STATE_FAILED | TURN_STATE_CHECK_SOCKET_CONNECTION | TURN_STATE_GET_CREDENTIALS | TURN_STATE_ALLOCATION |
         TURN_STATE_CREATE_PERMISSION | TURN_STATE_BIND_CHANNEL | TURN_STATE_READY,
     fromCleanUpTurnState, executeCleanUpTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_FAILED,
     TURN_STATE_CLEAN_UP | TURN_STATE_FAILED | TURN_STATE_CHECK_SOCKET_CONNECTION | TURN_STATE_GET_CREDENTIALS | TURN_STATE_ALLOCATION |
         TURN_STATE_CREATE_PERMISSION | TURN_STATE_BIND_CHANNEL | TURN_STATE_READY,
     fromFailedTurnState, executeFailedTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
};

UINT32 TURN_CONNECTION_STATE_MACHINE_STATE_COUNT = ARRAY_SIZE(TURN_CONNECTION_STATE_MACHINE_STATES);

PCHAR turnConnectionGetStateStr(UINT64 state)
{
    switch (state) {
        case TURN_STATE_NEW:
            return TURN_STATE_NEW_STR;
        case TURN_STATE_CHECK_SOCKET_CONNECTION:
            return TURN_STATE_CHECK_SOCKET_CONNECTION_STR;
        case TURN_STATE_GET_CREDENTIALS:
            return TURN_STATE_GET_CREDENTIALS_STR;
        case TURN_STATE_ALLOCATION:
            return TURN_STATE_ALLOCATION_STR;
        case TURN_STATE_CREATE_PERMISSION:
            return TURN_STATE_CREATE_PERMISSION_STR;
        case TURN_STATE_BIND_CHANNEL:
            return TURN_STATE_BIND_CHANNEL_STR;
        case TURN_STATE_READY:
            return TURN_STATE_READY_STR;
        case TURN_STATE_CLEAN_UP:
            return TURN_STATE_CLEAN_UP_STR;
        case TURN_STATE_FAILED:
            return TURN_STATE_FAILED_STR;
    }
    return TURN_STATE_UNKNOWN_STR;
}

STATUS stepTurnConnectionStateMachine(PTurnConnection pTurnConnection)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 oldState;
    UINT64 currentTime;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    do {
        oldState = pTurnConnection->state;

        retStatus = stepStateMachine(pTurnConnection->pStateMachine);

        if (STATUS_SUCCEEDED(retStatus) && ATOMIC_LOAD_BOOL(&pTurnConnection->stopTurnConnection) && pTurnConnection->state != TURN_STATE_NEW &&
            pTurnConnection->state != TURN_STATE_CLEAN_UP) {
            currentTime = GETTIME();
            pTurnConnection->state = TURN_STATE_CLEAN_UP;
            pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CLEAN_UP_TIMEOUT;

            /* fix up state to trigger transition into TURN_STATE_CLEAN_UP */
            retStatus = STATUS_SUCCESS;
            CHK_STATUS(stepStateMachine(pTurnConnection->pStateMachine));
        } else if (STATUS_FAILED(retStatus) && pTurnConnection->state != TURN_STATE_FAILED) {
            pTurnConnection->errorStatus = retStatus;
            pTurnConnection->state = TURN_STATE_FAILED;

            /* There is data race condition when editing the candidate state without holding
             * the IceAgent lock. However holding the turn lock and then locking the ice agent lock
             * can result in a dead lock. Ice must always be locked first, and then turn.
             */

            MUTEX_UNLOCK(pTurnConnection->lock);
            if (pTurnConnection->turnConnectionCallbacks.turnStateFailedFn != NULL) {
                pTurnConnection->turnConnectionCallbacks.turnStateFailedFn(pTurnConnection->pControlChannel,
                                                                           pTurnConnection->turnConnectionCallbacks.customData);
            }
            MUTEX_LOCK(pTurnConnection->lock);

            /* fix up state to trigger transition into TURN_STATE_FAILED  */
            retStatus = STATUS_SUCCESS;
            CHK_STATUS(stepStateMachine(pTurnConnection->pStateMachine));
        }

        if (oldState != pTurnConnection->state) {
            DLOGD("[%p] Turn connection state changed from %s to %s.", (PVOID) pTurnConnection, turnConnectionGetStateStr(oldState),
                  turnConnectionGetStateStr(pTurnConnection->state));
        } else {
            // state machine retry is not used. resetStateMachineRetryCount just to avoid
            // state machine retry grace period overflow warning.
            CHK_STATUS(resetStateMachineRetryCount(pTurnConnection->pStateMachine));
        }
    } while (oldState != pTurnConnection->state);

CleanUp:

    CHK_LOG_ERR(retStatus);

    LEAVES();
    return retStatus;
}

///////////////////////////////////////////////////////////////////////////
// State machine callback functions
///////////////////////////////////////////////////////////////////////////
STATUS fromNewTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_NEW;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    state = TURN_STATE_CHECK_SOCKET_CONNECTION;
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeNewTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);
    pTurnConnection->state = TURN_STATE_NEW;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromCheckSocketConnectionTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_CHECK_SOCKET_CONNECTION;
    BOOL locked = FALSE;
    UINT64 currentTime;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;
    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    currentTime = GETTIME();
    if (socketConnectionIsConnected(pTurnConnection->pControlChannel)) {
        state = TURN_STATE_GET_CREDENTIALS;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_GET_CREDENTIAL_TIMEOUT;
    }

    *pState = state;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeCheckSocketConnectionTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->state != TURN_STATE_CHECK_SOCKET_CONNECTION) {
        pTurnConnection->state = TURN_STATE_CHECK_SOCKET_CONNECTION;
        CHK_STATUS(
            turnConnectionPackageTurnAllocationRequest(NULL, NULL, NULL, 0, DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS, &pTurnConnection->pTurnPacket));
    }
CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromGetCredentialsTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    UINT64 currentTime;
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_GET_CREDENTIALS;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    currentTime = GETTIME();

    if (pTurnConnection->credentialObtained) {
        state = TURN_STATE_ALLOCATION;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_ALLOCATION_TIMEOUT;
    }

    *pState = state;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeGetCredentialsTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 currentTime;
    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    currentTime = GETTIME();

    if (pTurnConnection->state != TURN_STATE_GET_CREDENTIALS) {
        pTurnConnection->turnProfileDiagnostics.getCredentialsStartTime = currentTime;
        /* initialize TLS once tcp connection is established */
        /* Start receiving data for TLS handshake */
        ATOMIC_STORE_BOOL(&pTurnConnection->pControlChannel->receiveData, TRUE);

        /* We dont support DTLS and TCP, so only options are TCP/TLS and UDP. */
        /* TODO: add plain TCP once it becomes available. */
        if (pTurnConnection->protocol == KVS_SOCKET_PROTOCOL_TCP && pTurnConnection->pControlChannel->pTlsSession == NULL) {
            CHK_STATUS(socketConnectionInitSecureConnection(pTurnConnection->pControlChannel, FALSE));
        }
        pTurnConnection->state = TURN_STATE_GET_CREDENTIALS;
    } else {
        CHK(currentTime <= pTurnConnection->stateTimeoutTime, STATUS_TURN_CONNECTION_GET_CREDENTIALS_FAILED);
    }
    CHK_STATUS(iceUtilsSendStunPacket(pTurnConnection->pTurnPacket, NULL, 0, &pTurnConnection->turnServer.ipAddress, pTurnConnection->pControlChannel,
                                      NULL, FALSE));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromAllocationTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_ALLOCATION;
    UINT64 currentTime;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    if (ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation)) {
        state = TURN_STATE_CREATE_PERMISSION;
        currentTime = GETTIME();
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
    }

    *pState = state;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeAllocationTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 currentTime;
    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    currentTime = GETTIME();
    if (pTurnConnection->state != TURN_STATE_ALLOCATION) {
        DLOGV("Updated turn allocation request credential after receiving 401");
        pTurnConnection->turnProfileDiagnostics.createAllocationStartTime = GETTIME();
        // update turn allocation packet with credentials
        CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnPacket));
        CHK_STATUS(turnConnectionGetLongTermKey(pTurnConnection->turnServer.username, pTurnConnection->turnRealm,
                                                pTurnConnection->turnServer.credential, pTurnConnection->longTermKey,
                                                SIZEOF(pTurnConnection->longTermKey)));
        CHK_STATUS(turnConnectionPackageTurnAllocationRequest(pTurnConnection->turnServer.username, pTurnConnection->turnRealm,
                                                              pTurnConnection->turnNonce, pTurnConnection->nonceLen,
                                                              DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS, &pTurnConnection->pTurnPacket));
        pTurnConnection->state = TURN_STATE_ALLOCATION;
    } else {
        CHK(currentTime <= pTurnConnection->stateTimeoutTime, STATUS_TURN_CONNECTION_ALLOCATION_FAILED);
    }
    CHK_STATUS(iceUtilsSendStunPacket(pTurnConnection->pTurnPacket, pTurnConnection->longTermKey, ARRAY_SIZE(pTurnConnection->longTermKey),
                                      &pTurnConnection->turnServer.ipAddress, pTurnConnection->pControlChannel, NULL, FALSE));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromCreatePermissionTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_CREATE_PERMISSION, currentTime;
    UINT32 channelWithPermissionCount = 0, i = 0;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    *pState = state;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    currentTime = GETTIME();

    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        // As soon as create permission succeeded, we start sending channel bind message.
        // So connectionState could've already advanced to ready state.
        if (pTurnConnection->turnPeerList[i].connectionState == TURN_PEER_CONN_STATE_BIND_CHANNEL ||
            pTurnConnection->turnPeerList[i].connectionState == TURN_PEER_CONN_STATE_READY) {
            channelWithPermissionCount++;
        }
    }

    // push back timeout if no peer is available yet
    if (pTurnConnection->turnPeerCount == 0) {
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
        CHK(FALSE, retStatus);
    }

    if (currentTime > pTurnConnection->stateTimeoutTime || channelWithPermissionCount == pTurnConnection->turnPeerCount) {
        CHK(channelWithPermissionCount > 0, STATUS_TURN_CONNECTION_FAILED_TO_CREATE_PERMISSION);

        // go to next state if we have at least one ready peer
        state = TURN_STATE_BIND_CHANNEL;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_BIND_CHANNEL_TIMEOUT;
    }
    *pState = state;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeCreatePermissionTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    CHAR ipAddrStr[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    UINT64 currentTime;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->state != TURN_STATE_CREATE_PERMISSION) {
        CHK_STATUS(getIpAddrStr(&pTurnConnection->relayAddress, ipAddrStr, ARRAY_SIZE(ipAddrStr)));
        DLOGD("Relay address received: %s, port: %u", ipAddrStr, (UINT16) getInt16(pTurnConnection->relayAddress.port));
        if (pTurnConnection->pTurnCreatePermissionPacket != NULL) {
            CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnCreatePermissionPacket));
        }
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_CREATE_PERMISSION, NULL, &pTurnConnection->pTurnCreatePermissionPacket));
        // use host address as placeholder. hostAddress should have the same family as peer address
        CHK_STATUS(appendStunAddressAttribute(pTurnConnection->pTurnCreatePermissionPacket, STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS,
                                              &pTurnConnection->hostAddress));
        CHK_STATUS(appendStunUsernameAttribute(pTurnConnection->pTurnCreatePermissionPacket, pTurnConnection->turnServer.username));
        CHK_STATUS(appendStunRealmAttribute(pTurnConnection->pTurnCreatePermissionPacket, pTurnConnection->turnRealm));
        CHK_STATUS(appendStunNonceAttribute(pTurnConnection->pTurnCreatePermissionPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));

        // create channel bind packet here too so for each peer as soon as permission is created, it can start
        // sending channel bind request
        if (pTurnConnection->pTurnChannelBindPacket != NULL) {
            CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnChannelBindPacket));
        }
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_CHANNEL_BIND_REQUEST, NULL, &pTurnConnection->pTurnChannelBindPacket));
        // use host address as placeholder
        CHK_STATUS(
            appendStunAddressAttribute(pTurnConnection->pTurnChannelBindPacket, STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS, &pTurnConnection->hostAddress));
        CHK_STATUS(appendStunChannelNumberAttribute(pTurnConnection->pTurnChannelBindPacket, 0));
        CHK_STATUS(appendStunUsernameAttribute(pTurnConnection->pTurnChannelBindPacket, pTurnConnection->turnServer.username));
        CHK_STATUS(appendStunRealmAttribute(pTurnConnection->pTurnChannelBindPacket, pTurnConnection->turnRealm));
        CHK_STATUS(appendStunNonceAttribute(pTurnConnection->pTurnChannelBindPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));

        if (pTurnConnection->pTurnAllocationRefreshPacket != NULL) {
            CHK_STATUS(freeStunPacket(&pTurnConnection->pTurnAllocationRefreshPacket));
        }
        CHK_STATUS(createStunPacket(STUN_PACKET_TYPE_REFRESH, NULL, &pTurnConnection->pTurnAllocationRefreshPacket));
        CHK_STATUS(appendStunLifetimeAttribute(pTurnConnection->pTurnAllocationRefreshPacket, DEFAULT_TURN_ALLOCATION_LIFETIME_SECONDS));
        CHK_STATUS(appendStunUsernameAttribute(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->turnServer.username));
        CHK_STATUS(appendStunRealmAttribute(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->turnRealm));
        CHK_STATUS(appendStunNonceAttribute(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->turnNonce, pTurnConnection->nonceLen));

        pTurnConnection->state = TURN_STATE_CREATE_PERMISSION;
    }

    CHK_STATUS(checkTurnPeerConnections(pTurnConnection));

    // push back timeout if no peer is available yet
    if (pTurnConnection->turnPeerCount == 0) {
        currentTime = GETTIME();
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
        CHK(FALSE, retStatus);
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromBindChannelTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_BIND_CHANNEL;
    UINT64 currentTime;
    BOOL locked = FALSE;
    UINT32 readyPeerCount = 0, i = 0;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    currentTime = GETTIME();
    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        if (pTurnConnection->turnPeerList[i].connectionState == TURN_PEER_CONN_STATE_READY) {
            readyPeerCount++;
        }
    }
    if (currentTime > pTurnConnection->stateTimeoutTime || readyPeerCount == pTurnConnection->turnPeerCount) {
        CHK(readyPeerCount > 0, STATUS_TURN_CONNECTION_FAILED_TO_BIND_CHANNEL);
        // go to next state if we have at least one ready peer
        state = TURN_STATE_READY;
    }
    *pState = state;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeBindChannelTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);
    if (pTurnConnection->state != TURN_STATE_BIND_CHANNEL) {
        pTurnConnection->state = TURN_STATE_BIND_CHANNEL;
    }
    CHK_STATUS(checkTurnPeerConnections(pTurnConnection));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromReadyTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_READY;
    BOOL refreshPeerPermission = FALSE;
    UINT64 currentTime;
    UINT32 i;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP || pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    CHK_STATUS(turnConnectionRefreshPermission(pTurnConnection, &refreshPeerPermission));
    currentTime = GETTIME();
    if (refreshPeerPermission) {
        // reset pTurnPeer->connectionState to make them go through create permission and channel bind again
        for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
            pTurnConnection->turnPeerList[i].connectionState = TURN_PEER_CONN_STATE_CREATE_PERMISSION;
        }

        pTurnConnection->currentTimerCallingPeriod = DEFAULT_TURN_TIMER_INTERVAL_BEFORE_READY;
        state = TURN_STATE_CREATE_PERMISSION;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
        MUTEX_UNLOCK(pTurnConnection->lock);
        locked = FALSE;
        CHK_STATUS(timerQueueUpdateTimerPeriod(pTurnConnection->timerQueueHandle, (UINT64) pTurnConnection,
                                               (UINT32) ATOMIC_LOAD(&pTurnConnection->timerCallbackId), pTurnConnection->currentTimerCallingPeriod));
    } else if (pTurnConnection->currentTimerCallingPeriod != DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY) {
        // use longer timer interval as now it just needs to check disconnection and permission expiration.
        pTurnConnection->currentTimerCallingPeriod = DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY;
        MUTEX_UNLOCK(pTurnConnection->lock);
        locked = FALSE;
        CHK_STATUS(timerQueueUpdateTimerPeriod(pTurnConnection->timerQueueHandle, (UINT64) pTurnConnection,
                                               (UINT32) ATOMIC_LOAD(&pTurnConnection->timerCallbackId), pTurnConnection->currentTimerCallingPeriod));
    }

    *pState = state;

CleanUp:

    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeReadyTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    pTurnConnection->state = TURN_STATE_READY;
    CHK_STATUS(checkTurnPeerConnections(pTurnConnection));

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromCleanUpTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_CLEAN_UP;
    UINT64 currentTime;
    UINT32 i = 0;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_FAILED) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    /* start cleaning up even if we dont receive allocation freed response in time, or if connection is already closed,
     * since we already sent multiple STUN refresh packets with 0 lifetime. */
    currentTime = GETTIME();
    if (socketConnectionIsClosed(pTurnConnection->pControlChannel) || !ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) ||
        currentTime > pTurnConnection->stateTimeoutTime) {
        // clean transactionId store for each turn peer, preserving the peers
        for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
            transactionIdStoreClear(pTurnConnection->turnPeerList[i].pTransactionIdStore);
        }

        CHK_STATUS(turnConnectionFreePreAllocatedPackets(pTurnConnection));
        if (pTurnConnection != NULL) {
            CHK_STATUS(socketConnectionClosed(pTurnConnection->pControlChannel));
        }
        state = STATUS_SUCCEEDED(pTurnConnection->errorStatus) ? TURN_STATE_CLEAN_UP : TURN_STATE_FAILED;
        ATOMIC_STORE_BOOL(&pTurnConnection->shutdownComplete, TRUE);
    }
    *pState = state;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeCleanUpTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    PStunAttributeLifetime pStunAttributeLifetime = NULL;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    pTurnConnection->state = TURN_STATE_CLEAN_UP;
    if (ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation)) {
        CHK_STATUS(getStunAttribute(pTurnConnection->pTurnAllocationRefreshPacket, STUN_ATTRIBUTE_TYPE_LIFETIME,
                                    (PStunAttributeHeader*) &pStunAttributeLifetime));
        CHK(pStunAttributeLifetime != NULL, STATUS_INTERNAL_ERROR);
        pStunAttributeLifetime->lifetime = 0;
        CHK_STATUS(iceUtilsSendStunPacket(pTurnConnection->pTurnAllocationRefreshPacket, pTurnConnection->longTermKey,
                                          ARRAY_SIZE(pTurnConnection->longTermKey), &pTurnConnection->turnServer.ipAddress,
                                          pTurnConnection->pControlChannel, NULL, FALSE));
        pTurnConnection->deallocatePacketSent = TRUE;
    }

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS fromFailedTurnState(UINT64 customData, PUINT64 pState)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_FAILED;
    UINT64 currentTime;
    BOOL locked = FALSE;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);
    MUTEX_LOCK(pTurnConnection->lock);
    locked = TRUE;

    if (pTurnConnection->state == TURN_STATE_CLEAN_UP) {
        *pState = pTurnConnection->state;
        CHK(FALSE, STATUS_SUCCESS);
    }

    /* If we haven't done cleanup, go to cleanup state which will do the cleanup then go to failed state again. */
    if (!ATOMIC_LOAD_BOOL(&pTurnConnection->shutdownComplete)) {
        currentTime = GETTIME();
        state = TURN_STATE_CLEAN_UP;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CLEAN_UP_TIMEOUT;
    }

    *pState = state;

CleanUp:
    if (locked) {
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    LEAVES();
    return retStatus;
}

STATUS executeFailedTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    pTurnConnection->state = TURN_STATE_FAILED;
    DLOGW("TurnConnection in TURN_STATE_FAILED due to 0x%08x. Aborting TurnConnection", pTurnConnection->errorStatus);
    /* Since we are aborting, not gonna do cleanup */
    ATOMIC_STORE_BOOL(&pTurnConnection->hasAllocation, FALSE);

CleanUp:

    LEAVES();
    return retStatus;
}
