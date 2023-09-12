
/**
 * Implementation of a turn connection states machine callbacks
 */
#define LOG_CLASS "TurnConnectionState"
#include "../Include_i.h"

/**
 * Static definitions of the states
 */
StateMachineState ICE_AGENT_STATE_MACHINE_STATES[] = {
    {TURN_STATE_NEW, /*TODO ACCEPT*/, fromNewTurnState, executeNewTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CHECK_SOCKET_CONNECTION, /*TODO ACCEPT*/, fromCheckSocketConnectionTurnState, executeCheckSocketConnectionTurnState, NULL,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_GET_CREDENTIALS, /*TODO ACCEPT*/, fromGetCredentialsTurnState, executeGetCredentialsTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_ALLOCATION, /*TODO ACCEPT*/, fromAllocationTurnState, executeAllocationTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CREATE_PERMISSION, /*TODO ACCEPT*/, fromCreatePermissionTurnState, executeCreatePermissionTurnState, NULL,
     INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_BIND_CHANNEL, /*TODO ACCEPT*/, fromBindChannelTurnState, executeBindChannelTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_READY, /*TODO ACCEPT*/, fromReadyTurnState, executeReadyTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
    {TURN_STATE_CLEAN_UP, /*TODO ACCEPT*/, fromCleanUpTurnState, executeCleanUpTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL,
     STATUS_TURN_INVALID_STATE},
    {TURN_STATE_FAILED, /*TODO ACCEPT*/, fromFailedTurnState, executeFailedTurnState, NULL, INFINITE_RETRY_COUNT_SENTINEL, STATUS_TURN_INVALID_STATE},
};

PCHAR turnConnectionGetStateStr(TURN_CONNECTION_STATE state)
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

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    if (socketConnectionIsConnected(pTurnConnection->pControlChannel)) {
        state = TURN_STATE_GET_CREDENTIALS;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeCheckSocketConnectionTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state;

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
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;
    UINT64 state = TURN_STATE_GET_CREDENTIALS;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->credentialObtained) {
        state = TURN_STATE_ALLOCATION;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_ALLOCATION_TIMEOUT;
        pTurnConnection->stateTryCountMax = DEFAULT_TURN_ALLOCATION_MAX_TRY_COUNT;
        pTurnConnection->stateTryCount = 0;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeGetCredentialsTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->state != TURN_STATE_GET_CREDENTIALS) {
        /* initialize TLS once tcp connection is established */
        /* Start receiving data for TLS handshake */
        ATOMIC_STORE_BOOL(&pTurnConnection->pControlChannel->receiveData, TRUE);

        /* We dont support DTLS and TCP, so only options are TCP/TLS and UDP. */
        /* TODO: add plain TCP once it becomes available. */
        if (pTurnConnection->protocol == KVS_SOCKET_PROTOCOL_TCP && pTurnConnection->pControlChannel->pTlsSession == NULL) {
            CHK_STATUS(socketConnectionInitSecureConnection(pTurnConnection->pControlChannel, FALSE));
        }
        pTurnConnection->state = TURN_STATE_GET_CREDENTIALS;
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

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);
    if (ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation)) {
        state = TURN_STATE_CREATE_PERMISSION;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
    }

    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeAllocationTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    if (pTurnConnection->state != TURN_STATE_ALLOCATION) {
        DLOGV("Updated turn allocation request credential after receiving 401");

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
        pTurnConnection->stateTryCount++;
        CHK(pTurnConnection->stateTryCount < pTurnConnection->stateTryCountMax, STATUS_TURN_CONNECTION_ALLOCAITON_FAILED);
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
    UINT64 state = TURN_STATE_CREATE_PERMISSION;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    if (currentTime >= pTurnConnection->stateTimeoutTime || channelWithPermissionCount == pTurnConnection->turnPeerCount) {
        CHK(channelWithPermissionCount > 0, STATUS_TURN_CONNECTION_FAILED_TO_CREATE_PERMISSION);

        // go to next state if we have at least one ready peer
        state = TURN_STATE_BIND_CHANNEL;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_BIND_CHANNEL_TIMEOUT;
    }
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeCreatePermissionTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

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
        // sending chaneel bind request
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

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    if (currentTime >= pTurnConnection->stateTimeoutTime || readyPeerCount == pTurnConnection->turnPeerCount) {
        CHK(readyPeerCount > 0, STATUS_TURN_CONNECTION_FAILED_TO_BIND_CHANNEL);
        // go to next state if we have at least one ready peer
        state = TURN_STATE_READY;
    }
    *pState = state;

CleanUp:

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
    pTurnConnection->state = TURN_STATE_BIND_CHANNEL;
    for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        if (pTurnConnection->turnPeerList[i].connectionState == TURN_PEER_CONN_STATE_READY) {
            readyPeerCount++;
        }
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

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    CHK_STATUS(turnConnectionRefreshPermission(pTurnConnection, &refreshPeerPermission));
    if (refreshPeerPermission) {
        // reset pTurnPeer->connectionState to make them go through create permission and channel bind again
        for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
            pTurnConnection->turnPeerList[i].connectionState = TURN_PEER_CONN_STATE_CREATE_PERMISSION;
        }

        pTurnConnection->currentTimerCallingPeriod = DEFAULT_TURN_TIMER_INTERVAL_BEFORE_READY;
        CHK_STATUS(timerQueueUpdateTimerPeriod(pTurnConnection->timerQueueHandle, (UINT64) pTurnConnection,
                                               (UINT32) ATOMIC_LOAD(&pTurnConnection->timerCallbackId), pTurnConnection->currentTimerCallingPeriod));
        state = TURN_STATE_CREATE_PERMISSION;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CREATE_PERMISSION_TIMEOUT;
    } else if (pTurnConnection->currentTimerCallingPeriod != DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY) {
        // use longer timer interval as now it just needs to check disconnection and permission expiration.
        pTurnConnection->currentTimerCallingPeriod = DEFAULT_TURN_TIMER_INTERVAL_AFTER_READY;
        CHK_STATUS(timerQueueUpdateTimerPeriod(pTurnConnection->timerQueueHandle, (UINT64) pTurnConnection,
                                               (UINT32) ATOMIC_LOAD(&pTurnConnection->timerCallbackId), pTurnConnection->currentTimerCallingPeriod));
    }

    *pState = state;

CleanUp:

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
    UINT64 state = TURN_STATE_CLEANUP;

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    /* start cleanning up even if we dont receive allocation freed response in time, or if connection is already closed,
     * since we already sent multiple STUN refresh packets with 0 lifetime. */
    if (socketConnectionIsClosed(pTurnConnection->pControlChannel) || !ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) ||
        currentTime >= pTurnConnection->stateTimeoutTime) {
        // clean transactionId store for each turn peer, preserving the peers
        for (i = 0; i < pTurnConnection->turnPeerCount; ++i) {
            transactionIdStoreClear(pTurnConnection->turnPeerList[i].pTransactionIdStore);
        }

        CHK_STATUS(turnConnectionFreePreAllocatedPackets(pTurnConnection));
        if (pTurnConnection != NULL) {
            CHK_STATUS(socketConnectionClosed(pTurnConnection->pControlChannel));
        }
        state = STATUS_SUCCEEDED(pTurnConnection->errorStatus) ? TURN_STATE_NEW : TURN_STATE_FAILED;
        ATOMIC_STORE_BOOL(&pTurnConnection->shutdownComplete, TRUE);
    }
    *pState = state;

CleanUp:

    LEAVES();
    return retStatus;
}

STATUS executeCleanUpTurnState(UINT64 customData, UINT64 time)
{
    ENTERS();
    UNUSED_PARAM(time);
    STATUS retStatus = STATUS_SUCCESS;
    PTurnConnection pTurnConnection = (PTurnConnection) customData;

    CHK(pTurnConnection != NULL, STATUS_NULL_ARG);

    pTurnConnection->state = TURN_STATE_CLEANUP;
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

    CHK(pTurnConnection != NULL && pState != NULL, STATUS_NULL_ARG);

    /* If we haven't done cleanup, go to cleanup state which will do the cleanup then go to failed state again. */
    if (!ATOMIC_LOAD_BOOL(&pTurnConnection->shutdownComplete)) {
        state = TURN_STATE_CLEAN_UP;
        pTurnConnection->stateTimeoutTime = currentTime + DEFAULT_TURN_CLEAN_UP_TIMEOUT;
    }

    *pState = state;

CleanUp:

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
