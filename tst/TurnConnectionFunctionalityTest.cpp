#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class TurnConnectionFunctionalityTest : public WebRtcClientTestBase {
    PIceConfigInfo pIceConfigInfo;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;

  public:
    PConnectionListener pConnectionListener = NULL;
    PTurnConnection pTurnConnection = NULL;
    TurnChannelData turnChannelData[DEFAULT_TURN_CHANNEL_DATA_BUFFER_SIZE];
    UINT32 turnChannelDataCount = ARRAY_SIZE(turnChannelData);

    VOID initializeTestTurnConnection()
    {
        UINT32 i, j, iceConfigCount, uriCount;
        IceServer iceServers[MAX_ICE_SERVERS_COUNT];
        PIceServer pTurnServer = NULL;
        KvsIpAddress localIpInterfaces[MAX_LOCAL_NETWORK_INTERFACE_COUNT];
        UINT32 localIpInterfaceCount = ARRAY_SIZE(localIpInterfaces);
        PKvsIpAddress pTurnSocketAddr = NULL;
        PSocketConnection pTurnSocket = NULL;

        // If this failed we will not be in the Connected state, need to bail out
        ASSERT_EQ(STATUS_SUCCESS, initializeSignalingClient());

        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

        for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
            EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
            for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                iceServers[uriCount].setIpFn = NULL;
                EXPECT_EQ(STATUS_SUCCESS,
                          parseIceServer(&iceServers[uriCount++], pIceConfigInfo->uris[j], pIceConfigInfo->userName, pIceConfigInfo->password));
            }
        }

        for (i = 0; i < uriCount && pTurnServer == NULL; ++i) {
            if (iceServers[i].isTurn) {
                pTurnServer = &iceServers[i];
            }
        }

        EXPECT_TRUE(pTurnServer != NULL);
        EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));

        EXPECT_EQ(STATUS_SUCCESS, getLocalhostIpAddresses(localIpInterfaces, &localIpInterfaceCount, NULL, 0));
        for (i = 0; i < localIpInterfaceCount; ++i) {
            if (localIpInterfaces[i].family == pTurnServer->ipAddresses.ipv4Address.family && (pTurnSocketAddr == NULL || localIpInterfaces[i].isPointToPoint)) {
                pTurnSocketAddr = &localIpInterfaces[i];
            }
        }

        auto onDataHandler = [](UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                                PKvsIpAddress pDest) -> STATUS {
            UNUSED_PARAM(pSocketConnection);
            TurnConnectionFunctionalityTest* pTestBase = (TurnConnectionFunctionalityTest*) customData;
            pTestBase->turnChannelDataCount = ARRAY_SIZE(pTestBase->turnChannelData);
            EXPECT_EQ(STATUS_SUCCESS,
                      turnConnectionIncomingDataHandler(pTestBase->pTurnConnection, pBuffer, bufferLen, pSrc, pDest, pTestBase->turnChannelData,
                                                        &pTestBase->turnChannelDataCount));

            return STATUS_SUCCESS;
        };
        EXPECT_EQ(STATUS_SUCCESS,
                  createSocketConnection((KVS_IP_FAMILY_TYPE) pTurnServer->ipAddresses.ipv4Address.family, KVS_ICE_DEFAULT_TURN_PROTOCOL, NULL,
                                         &pTurnServer->ipAddresses.ipv4Address, (UINT64) this, onDataHandler, 0, &pTurnSocket));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pTurnSocket));
        ASSERT_EQ(STATUS_SUCCESS,
                  createTurnConnection(pTurnServer, timerQueueHandle, TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL, KVS_ICE_DEFAULT_TURN_PROTOCOL,
                                       NULL, pTurnSocket, pConnectionListener, KVS_IP_FAMILY_TYPE_IPV4, &pTurnConnection));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
    }

    // Same as initializeTestTurnConnection, but corrupts the TURN credential before creating the
    // turn connection so the TURN server rejects the authenticated Allocate with 401 UNAUTHORIZED.
    VOID initializeTestTurnConnectionWithBadCredential()
    {
        UINT32 i, j, iceConfigCount, uriCount;
        IceServer iceServers[MAX_ICE_SERVERS_COUNT];
        PIceServer pTurnServer = NULL;
        KvsIpAddress localIpInterfaces[MAX_LOCAL_NETWORK_INTERFACE_COUNT];
        UINT32 localIpInterfaceCount = ARRAY_SIZE(localIpInterfaces);
        PKvsIpAddress pTurnSocketAddr = NULL;
        PSocketConnection pTurnSocket = NULL;

        ASSERT_EQ(STATUS_SUCCESS, initializeSignalingClient());

        EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

        for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
            EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
            for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                iceServers[uriCount].setIpFn = NULL;
                EXPECT_EQ(STATUS_SUCCESS,
                          parseIceServer(&iceServers[uriCount++], pIceConfigInfo->uris[j], pIceConfigInfo->userName, pIceConfigInfo->password));
            }
        }

        for (i = 0; i < uriCount && pTurnServer == NULL; ++i) {
            if (iceServers[i].isTurn) {
                pTurnServer = &iceServers[i];
            }
        }

        EXPECT_TRUE(pTurnServer != NULL);

        // Corrupt the credential: keep a valid username so the server issues a nonce/realm challenge,
        // but make the password wrong so the authenticated Allocate is rejected with 401 UNAUTHORIZED.
        STRCPY(pTurnServer->credential, "this-is-an-invalid-turn-credential");

        EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));

        EXPECT_EQ(STATUS_SUCCESS, getLocalhostIpAddresses(localIpInterfaces, &localIpInterfaceCount, NULL, 0));
        for (i = 0; i < localIpInterfaceCount; ++i) {
            if (localIpInterfaces[i].family == pTurnServer->ipAddresses.ipv4Address.family && (pTurnSocketAddr == NULL || localIpInterfaces[i].isPointToPoint)) {
                pTurnSocketAddr = &localIpInterfaces[i];
            }
        }

        auto onDataHandler = [](UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen, PKvsIpAddress pSrc,
                                PKvsIpAddress pDest) -> STATUS {
            UNUSED_PARAM(pSocketConnection);
            TurnConnectionFunctionalityTest* pTestBase = (TurnConnectionFunctionalityTest*) customData;
            pTestBase->turnChannelDataCount = ARRAY_SIZE(pTestBase->turnChannelData);
            EXPECT_EQ(STATUS_SUCCESS,
                      turnConnectionIncomingDataHandler(pTestBase->pTurnConnection, pBuffer, bufferLen, pSrc, pDest, pTestBase->turnChannelData,
                                                        &pTestBase->turnChannelDataCount));

            return STATUS_SUCCESS;
        };
        EXPECT_EQ(STATUS_SUCCESS,
                  createSocketConnection((KVS_IP_FAMILY_TYPE) pTurnServer->ipAddresses.ipv4Address.family, KVS_ICE_DEFAULT_TURN_PROTOCOL, NULL,
                                         &pTurnServer->ipAddresses.ipv4Address, (UINT64) this, onDataHandler, 0, &pTurnSocket));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pTurnSocket));
        ASSERT_EQ(STATUS_SUCCESS,
                  createTurnConnection(pTurnServer, timerQueueHandle, TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL, KVS_ICE_DEFAULT_TURN_PROTOCOL,
                                       NULL, pTurnSocket, pConnectionListener, KVS_IP_FAMILY_TYPE_IPV4, &pTurnConnection));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
    }

    VOID freeTestTurnConnection()
    {
        EXPECT_TRUE(pTurnConnection != NULL);
        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
        pTurnConnection->pConnectionListener = NULL;
        EXPECT_EQ(STATUS_SUCCESS, freeTurnConnection(&pTurnConnection));
        timerQueueFree(&timerQueueHandle);
        deinitializeSignalingClient();
    }
};

TEST_F(TurnConnectionFunctionalityTest, turnConnectionReceiveRelayedAddress)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    UINT64 getRelayAddrTimeout;
    KvsIpAddress relayAddress;
    BOOL relayAddressReceived = FALSE;

    initializeTestTurnConnection();

    MEMSET(&relayAddress, 0x00, SIZEOF(KvsIpAddress));

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    getRelayAddrTimeout = GETTIME() + 3 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while ((relayAddressReceived = turnConnectionGetRelayAddress(pTurnConnection, &relayAddress)) == FALSE && GETTIME() < getRelayAddrTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(relayAddressReceived);

    freeTestTurnConnection();
}

/*
 * turnConnectionAddPeer should skip non-routable peer addresses (which a public TURN server would
 * reject with 403 Forbidden IP): the peer is not added, turnPeerCount is unchanged, and the
 * diagnostic counter is bumped. Routable peers are added normally, and setting
 * disableNonRoutablePeersFilterForRelay turns the filter off so non-routable peers are added too.
 */
TEST_F(TurnConnectionFunctionalityTest, turnConnectionFiltersNonRoutablePeers)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    KvsIpAddress nonRoutablePeer; // 192.168.1.1 (RFC 1918 private)
    KvsIpAddress routablePeer;    // 77.1.1.1 (public)

    initializeTestTurnConnection();

    MEMSET(&nonRoutablePeer, 0x00, SIZEOF(KvsIpAddress));
    nonRoutablePeer.family = KVS_IP_FAMILY_TYPE_IPV4;
    nonRoutablePeer.port = (UINT16) getInt16(8080);
    nonRoutablePeer.address[0] = 192;
    nonRoutablePeer.address[1] = 168;
    nonRoutablePeer.address[2] = 1;
    nonRoutablePeer.address[3] = 1;

    MEMSET(&routablePeer, 0x00, SIZEOF(KvsIpAddress));
    routablePeer.family = KVS_IP_FAMILY_TYPE_IPV4;
    routablePeer.port = (UINT16) getInt16(8080);
    routablePeer.address[0] = 0x4d; // 77.1.1.1
    routablePeer.address[1] = 0x01;
    routablePeer.address[2] = 0x01;
    routablePeer.address[3] = 0x01;

    // Filter enabled by default: non-routable peer is skipped (successful no-op), not added,
    // and the diagnostic counter is incremented.
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &nonRoutablePeer));
    EXPECT_EQ((UINT32) 0, pTurnConnection->turnPeerCount);
    EXPECT_EQ((UINT32) 1, pTurnConnection->nonRoutablePeersFilteredCount);

    // A routable peer is added normally.
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &routablePeer));
    EXPECT_EQ((UINT32) 1, pTurnConnection->turnPeerCount);
    EXPECT_EQ((UINT32) 1, pTurnConnection->nonRoutablePeersFilteredCount);

    // With the filter disabled (e.g. an on-prem/LAN TURN that relays to private peers), the
    // non-routable peer is added and the counter does not change.
    pTurnConnection->disableNonRoutablePeersFilterForRelay = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &nonRoutablePeer));
    EXPECT_EQ((UINT32) 2, pTurnConnection->turnPeerCount);
    EXPECT_EQ((UINT32) 1, pTurnConnection->nonRoutablePeersFilteredCount);

    freeTestTurnConnection();
}

/*
 * Given a valid turn endpoint and credentials, turnConnection should successfully allocate,
 * create permission, and create channel. Then manually trigger permission refresh and allocation refresh
 */
TEST_F(TurnConnectionFunctionalityTest, turnConnectionRefreshPermissionTest)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    DLOGI("Checking if TURN_STATE_READY is set");
    EXPECT_TRUE(turnReady == TRUE);

    // modify permission expiration time to trigger refresh permission
    MUTEX_LOCK(pTurnConnection->lock);
    pTurnConnection->turnPeerList[0].permissionExpirationTime = GETTIME();
    MUTEX_UNLOCK(pTurnConnection->lock);

    // verify we are no longer in ready state.
    turnReady = FALSE;
    turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(5 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state != TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    //here "TRUE" actually means not in the ready state
    EXPECT_TRUE(turnReady == TRUE);

    //and now let's make sure we get back to ready
    turnReady = FALSE;
    turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    // should be back to ready after refresh is done
    EXPECT_TRUE(turnReady == TRUE);

    // modify allocation expiration time to trigger refresh allocation
    MUTEX_LOCK(pTurnConnection->lock);
    pTurnConnection->allocationExpirationTime = GETTIME();
    MUTEX_UNLOCK(pTurnConnection->lock);

    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // allocation should be refreshed.
    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_GE(pTurnConnection->allocationExpirationTime, GETTIME());
    MUTEX_UNLOCK(pTurnConnection->lock);

    freeTestTurnConnection();
}

/*
 * Regression test for the TURN 401 handling. With a stale/invalid TURN credential the server rejects the
 * authenticated Allocate with 401 UNAUTHORIZED. The connection must never reach READY, and rather than
 * retransmitting until the 5s allocation timeout it must fail fast (correlating the 401 to our authenticated
 * request by transaction id) with the specific STATUS_TURN_CONNECTION_CREDENTIALS_REJECTED status.
 */
TEST_F(TurnConnectionFunctionalityTest, turnConnectionBadCredentialFailsFast)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL turnReady = FALSE, turnFailed = FALSE;
    KvsIpAddress turnPeerAddr;
    UINT64 startTime, elapsed;
    // Bound well under the 5s allocation timeout: fail-fast should trip within a couple of timer ticks.
    UINT64 failFastBudget = 3 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    // Overall wait can exceed the allocation timeout so a regression (loop-until-timeout) is still observed.
    UINT64 timeout = GETTIME() + 20 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnectionWithBadCredential();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    startTime = GETTIME();
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    while (!turnReady && !turnFailed && GETTIME() < timeout) {
        THREAD_SLEEP(50 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        } else if (pTurnConnection->state == TURN_STATE_FAILED) {
            turnFailed = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }
    elapsed = GETTIME() - startTime;

    // With a bad credential we must never become ready.
    EXPECT_FALSE(turnReady);
    // We must fail, and with the specific credential-rejection status (not a generic allocation timeout).
    EXPECT_TRUE(turnFailed);
    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_EQ(STATUS_TURN_CONNECTION_CREDENTIALS_REJECTED, pTurnConnection->errorStatus);
    MUTEX_UNLOCK(pTurnConnection->lock);
    // And it must happen fast, well before the allocation timeout would have expired.
    EXPECT_LT(elapsed, failFastBudget);

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionShutdownCompleteBeforeTimeout)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(turnReady == TRUE);
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionShutdown(pTurnConnection, KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT));

    EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) || ATOMIC_LOAD_BOOL(&pTurnConnection->stopTurnConnection));

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionShutdownAsync)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    UINT64 shutdownTimeout;
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(turnReady == TRUE);
    // return immediately
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionShutdown(pTurnConnection, 0));

    shutdownTimeout = GETTIME() + KVS_ICE_TURN_CONNECTION_SHUTDOWN_TIMEOUT;
    while (!turnConnectionIsShutdownComplete(pTurnConnection) && GETTIME() < shutdownTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) || ATOMIC_LOAD_BOOL(&pTurnConnection->stopTurnConnection));

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionShutdownWithAllocationRemovesTurnSocketConnection)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL doneAllocate = FALSE;
    UINT64 shutdownTimeout;
    UINT64 doneAllocateTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    PSocketConnection pTurnSocketConnection = NULL, pCurrSocketConnection = NULL;
    BOOL connectionRemovedFromListener = TRUE;
    UINT32 i;

    initializeTestTurnConnection();
    pTurnSocketConnection = pTurnConnection->pControlChannel;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!doneAllocate && GETTIME() < doneAllocateTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_CREATE_PERMISSION) {
            doneAllocate = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(doneAllocate == TRUE);
    // return immediately
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionShutdown(pTurnConnection, 0));

    shutdownTimeout = GETTIME() + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!turnConnectionIsShutdownComplete(pTurnConnection) && GETTIME() < shutdownTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) || ATOMIC_LOAD_BOOL(&pTurnConnection->stopTurnConnection));

    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_TRUE(ATOMIC_LOAD_BOOL(&pTurnSocketConnection->connectionClosed));
    MUTEX_UNLOCK(pTurnConnection->lock);

    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    MUTEX_LOCK(pConnectionListener->lock);
    for (i = 0; connectionRemovedFromListener && i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] != NULL) {
            pCurrSocketConnection = pConnectionListener->sockets[i];
            connectionRemovedFromListener = (pCurrSocketConnection != pTurnSocketConnection);
        }
    }
    MUTEX_UNLOCK(pConnectionListener->lock);

    /* make sure that pTurnSocketConnection has been removed from connection listener's list */
    EXPECT_TRUE(connectionRemovedFromListener == TRUE);

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionShutdownWithoutAllocationRemovesTurnSocketConnection)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL atGetCredential = FALSE;
    UINT64 shutdownTimeout;
    UINT64 atGetCredentialTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    PSocketConnection pTurnSocketConnection = NULL, pCurrSocketConnection = NULL;
    BOOL connectionRemovedFromListener = TRUE;
    UINT32 i;

    initializeTestTurnConnection();
    pTurnSocketConnection = pTurnConnection->pControlChannel;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until get credential state
    while (!atGetCredential && GETTIME() < atGetCredentialTimeout) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_GET_CREDENTIALS) {
            atGetCredential = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    // return immediately
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionShutdown(pTurnConnection, 0));

    shutdownTimeout = GETTIME() + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!turnConnectionIsShutdownComplete(pTurnConnection) && GETTIME() < shutdownTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(!ATOMIC_LOAD_BOOL(&pTurnConnection->hasAllocation) || ATOMIC_LOAD_BOOL(&pTurnConnection->stopTurnConnection));

    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_TRUE(ATOMIC_LOAD_BOOL(&pTurnSocketConnection->connectionClosed));
    MUTEX_UNLOCK(pTurnConnection->lock);

    THREAD_SLEEP(2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    MUTEX_LOCK(pConnectionListener->lock);
    for (i = 0; connectionRemovedFromListener && i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] != NULL) {
            pCurrSocketConnection = pConnectionListener->sockets[i];
            connectionRemovedFromListener = (pCurrSocketConnection != pTurnSocketConnection);
        }
    }
    MUTEX_UNLOCK(pConnectionListener->lock);

    /* make sure that pTurnSocketConnection has been removed from connection listener's list */
    EXPECT_TRUE(connectionRemovedFromListener == TRUE);

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionShutdownAfterFailure)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL atGetCredential = FALSE;
    UINT64 shutdownTimeout;
    UINT64 atGetCredentialTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    PSocketConnection pTurnSocketConnection = NULL, pCurrSocketConnection = NULL;
    UINT32 i;
    BOOL connectionRemovedFromListener = TRUE;

    initializeTestTurnConnection();
    pTurnSocketConnection = pTurnConnection->pControlChannel;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until get credential state
    while (!atGetCredential && GETTIME() < atGetCredentialTimeout) {
        THREAD_SLEEP(10 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_GET_CREDENTIALS) {
            atGetCredential = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    MUTEX_LOCK(pTurnConnection->lock);
    pTurnConnection->state = TURN_STATE_FAILED;
    pTurnConnection->errorStatus = STATUS_INVALID_OPERATION;
    MUTEX_UNLOCK(pTurnConnection->lock);

    shutdownTimeout = GETTIME() + 5 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    while (!turnConnectionIsShutdownComplete(pTurnConnection) && GETTIME() < shutdownTimeout) {
        THREAD_SLEEP(HUNDREDS_OF_NANOS_IN_A_SECOND);
    }

    EXPECT_TRUE(turnConnectionIsShutdownComplete(pTurnConnection));

    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_TRUE(ATOMIC_LOAD_BOOL(&pTurnSocketConnection->connectionClosed));
    MUTEX_UNLOCK(pTurnConnection->lock);

    /* select in connection timeout every 1s */
    THREAD_SLEEP(3 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    MUTEX_LOCK(pConnectionListener->lock);
    for (i = 0; connectionRemovedFromListener && i < CONNECTION_LISTENER_DEFAULT_MAX_LISTENING_CONNECTION; i++) {
        if (pConnectionListener->sockets[i] != NULL) {
            pCurrSocketConnection = pConnectionListener->sockets[i];
            connectionRemovedFromListener = (pCurrSocketConnection != pTurnSocketConnection);
        }
    }
    MUTEX_UNLOCK(pConnectionListener->lock);

    /* make sure that pTurnSocketConnection has been removed from connection listener's list */
    EXPECT_TRUE(connectionRemovedFromListener == TRUE);

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionReceivePartialChannelMessageTest)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    // there are 3 channel messages for channel 0x4001
    BYTE channelMsg[] = {0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x42, 0x37, 0x73, 0x2f, 0x51, 0x48, 0x7a, 0x54, 0x69,
                         0x69, 0x32, 0x7a, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57,
                         0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00,
                         0x00, 0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0xe5, 0xf4, 0xfc, 0x35, 0xee, 0x7c, 0x13, 0x51,
                         0x14, 0x5d, 0xdb, 0xa7, 0xb0, 0xa7, 0xb1, 0xd4, 0x2b, 0xd3, 0x5f, 0x5b, 0x80, 0x28, 0x00, 0x04, 0x6a, 0x64, 0x06, 0x57, 0x40,
                         0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x41, 0x48, 0x31, 0x46, 0x54, 0x55, 0x4b, 0x39, 0x2b, 0x61,
                         0x52, 0x32, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00,
                         0x04, 0x00, 0x02, 0x00, 0x00, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00, 0x00,
                         0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x9a, 0x02, 0x8e, 0x1a, 0x75, 0x41, 0x97, 0xdf, 0x3b,
                         0x7a, 0x50, 0xc7, 0x26, 0xda, 0x18, 0x85, 0x86, 0x28, 0x2c, 0xcb, 0x80, 0x28, 0x00, 0x04, 0xaf, 0xdc, 0xa8, 0x68, 0x40, 0x01,
                         0x00, 0x60, 0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x2f, 0x77, 0x59, 0x57, 0x39, 0x4b, 0x69, 0x4a, 0x53, 0x75, 0x4b,
                         0x45, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04,
                         0x00, 0x01, 0x00, 0x0a, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x24, 0x00, 0x04, 0x6e,
                         0x7e, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x3e, 0x39, 0x07, 0x98, 0xe5, 0x83, 0x14, 0x85, 0x23, 0xb3, 0x29, 0xc1, 0x92, 0x47,
                         0x45, 0x0c, 0xad, 0xdb, 0xa1, 0x6d, 0x80, 0x28, 0x00, 0x04, 0x94, 0x6c, 0x5d, 0x00};
    // breakdown of channel data in channelMsg
    BYTE channelData1[] = {
        0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x42, 0x37, 0x73, 0x2f, 0x51, 0x48, 0x7a, 0x54, 0x69,
        0x69, 0x32, 0x7a, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57,
        0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00,
        0x00, 0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0xe5, 0xf4, 0xfc, 0x35, 0xee, 0x7c, 0x13, 0x51,
        0x14, 0x5d, 0xdb, 0xa7, 0xb0, 0xa7, 0xb1, 0xd4, 0x2b, 0xd3, 0x5f, 0x5b, 0x80, 0x28, 0x00, 0x04, 0x6a, 0x64, 0x06, 0x57,
    };

    BYTE channelData2[] = {
        0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x41, 0x48, 0x31, 0x46, 0x54, 0x55, 0x4b, 0x39, 0x2b,
        0x61, 0x52, 0x32, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57,
        0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00,
        0x00, 0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x9a, 0x02, 0x8e, 0x1a, 0x75, 0x41, 0x97, 0xdf,
        0x3b, 0x7a, 0x50, 0xc7, 0x26, 0xda, 0x18, 0x85, 0x86, 0x28, 0x2c, 0xcb, 0x80, 0x28, 0x00, 0x04, 0xaf, 0xdc, 0xa8, 0x68,
    };

    BYTE channelData3[] = {
        0x40, 0x01, 0x00, 0x60, 0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x2f, 0x77, 0x59, 0x57, 0x39, 0x4b, 0x69, 0x4a,
        0x53, 0x75, 0x4b, 0x45, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00,
        0xc0, 0x57, 0x00, 0x04, 0x00, 0x01, 0x00, 0x0a, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79,
        0x00, 0x24, 0x00, 0x04, 0x6e, 0x7e, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x3e, 0x39, 0x07, 0x98, 0xe5, 0x83, 0x14, 0x85,
        0x23, 0xb3, 0x29, 0xc1, 0x92, 0x47, 0x45, 0x0c, 0xad, 0xdb, 0xa1, 0x6d, 0x80, 0x28, 0x00, 0x04, 0x94, 0x6c, 0x5d, 0x00,
    };

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    TurnChannelData turnChannelData;
    UINT32 turnChannelDataCount = 0, dataLenProcessed = 0;
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;
    PBYTE pCurrent = NULL;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(turnReady == TRUE);

    pCurrent = channelMsg;

    EXPECT_EQ(STATUS_SUCCESS,
              turnConnectionHandleChannelDataTcpMode(pTurnConnection, pCurrent, ARRAY_SIZE(channelMsg), &turnChannelData, &turnChannelDataCount,
                                                     &dataLenProcessed));
    /* Only parse out single channel data message */
    EXPECT_EQ(turnChannelDataCount, 1);
    EXPECT_EQ(turnChannelData.size, ARRAY_SIZE(channelData1) - TURN_DATA_CHANNEL_SEND_OVERHEAD);
    EXPECT_EQ(0, MEMCMP(turnChannelData.data, channelData1 + TURN_DATA_CHANNEL_SEND_OVERHEAD, turnChannelData.size));
    pCurrent += dataLenProcessed;

    EXPECT_EQ(STATUS_SUCCESS,
              turnConnectionHandleChannelDataTcpMode(pTurnConnection, pCurrent, 20, &turnChannelData, &turnChannelDataCount, &dataLenProcessed));
    /* didnt parse out anything because not complete message was given */
    EXPECT_EQ(turnChannelDataCount, 0);
    pCurrent += dataLenProcessed;

    EXPECT_EQ(STATUS_SUCCESS,
              turnConnectionHandleChannelDataTcpMode(pTurnConnection, pCurrent, ARRAY_SIZE(channelMsg), &turnChannelData, &turnChannelDataCount,
                                                     &dataLenProcessed));
    EXPECT_EQ(turnChannelDataCount, 1);
    EXPECT_EQ(turnChannelData.size, ARRAY_SIZE(channelData2) - TURN_DATA_CHANNEL_SEND_OVERHEAD);
    EXPECT_EQ(0, MEMCMP(turnChannelData.data, channelData2 + TURN_DATA_CHANNEL_SEND_OVERHEAD, turnChannelData.size));
    pCurrent += dataLenProcessed;

    EXPECT_EQ(STATUS_SUCCESS,
              turnConnectionHandleChannelDataTcpMode(pTurnConnection, pCurrent, ARRAY_SIZE(channelMsg), &turnChannelData, &turnChannelDataCount,
                                                     &dataLenProcessed));
    EXPECT_EQ(turnChannelDataCount, 1);
    EXPECT_EQ(turnChannelData.size, ARRAY_SIZE(channelData3) - TURN_DATA_CHANNEL_SEND_OVERHEAD);
    EXPECT_EQ(0, MEMCMP(turnChannelData.data, channelData3 + TURN_DATA_CHANNEL_SEND_OVERHEAD, turnChannelData.size));

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionReceiveChannelDataMixedWithStunMessage)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BYTE incomingData[] = {
        0x40,
        0x01,
        0x00,
        0x60,
        0x00,
        0x01,
        0x00,
        0x4c,
        0x21,
        0x12,
        0xa4,
        0x42,
        0x2f,
        0x77,
        0x59,
        0x57,
        0x39,
        0x4b,
        0x69,
        0x4a,
        0x53,
        0x75,
        0x4b,
        0x45,
        0x00,
        0x06,
        0x00,
        0x09,
        0x79,
        0x45,
        0x78,
        0x55,
        0x3a,
        0x31,
        0x63,
        0x39,
        0x64,
        0x00,
        0x00,
        0x00,
        0xc0,
        0x57,
        0x00,
        0x04,
        0x00,
        0x01,
        0x00,
        0x0a,
        0x80,
        0x2a,
        0x00,
        0x08,
        0xe9,
        0x60,
        0x24,
        0x7e,
        0x0a,
        0xd6,
        0xc4,
        0x79,
        0x00,
        0x24,
        0x00,
        0x04,
        0x6e,
        0x7e,
        0x1e,
        0xff,
        0x00,
        0x08,
        0x00,
        0x14,
        0x3e,
        0x39,
        0x07,
        0x98,
        0xe5,
        0x83,
        0x14,
        0x85,
        0x23,
        0xb3,
        0x29,
        0xc1,
        0x92,
        0x47,
        0x45,
        0x0c,
        0xad,
        0xdb,
        0xa1,
        0x6d,
        0x80,
        0x28,
        0x00,
        0x04,
        0x94,
        0x6c,
        0x5d,
        0x00,
        /* The second part is a STUN create permission success response */
        0x00,
        0x08,
        0x00,
        0x9c,
        0x21,
        0x12,
        0xa4,
        0x42,
        0x30,
        0x51,
        0x33,
        0x61,
        0x36,
        0x73,
        0x47,
        0x33,
        0x2f,
        0x39,
        0x69,
        0x55,
        0x00,
        0x12,
        0x00,
        0x08,
        0x00,
        0x01,
        0xa6,
        0x68,
        0xe1,
        0xba,
        0x82,
        0x84,
        0x00,
        0x06,
        0x00,
        0x58,
        0x31,
        0x35,
        0x37,
        0x30,
        0x36,
        0x36,
        0x39,
        0x34,
        0x37,
        0x31,
        0x3a,
        0x61,
        0x72,
        0x6e,
        0x3a,
        0x61,
        0x77,
        0x73,
        0x3a,
        0x6b,
        0x69,
        0x6e,
        0x65,
        0x73,
        0x69,
        0x73,
        0x76,
        0x69,
        0x64,
        0x65,
        0x6f,
        0x3a,
        0x75,
        0x73,
        0x2d,
        0x77,
        0x65,
        0x73,
        0x74,
        0x2d,
        0x32,
        0x3a,
        0x38,
        0x33,
        0x36,
        0x32,
        0x30,
        0x33,
        0x31,
        0x31,
        0x37,
        0x39,
        0x37,
        0x31,
        0x3a,
        0x63,
        0x68,
        0x61,
        0x6e,
        0x6e,
        0x65,
        0x6c,
        0x2f,
        0x66,
        0x6f,
        0x6f,
        0x34,
        0x2f,
        0x31,
        0x35,
        0x36,
        0x39,
        0x30,
        0x33,
        0x33,
        0x30,
        0x34,
        0x32,
        0x32,
        0x30,
        0x37,
        0x3a,
        0x56,
        0x49,
        0x45,
        0x57,
        0x45,
        0x52,
        0x00,
        0x14,
        0x00,
        0x03,
        0x6b,
        0x76,
        0x73,
        0x00,
        0x00,
        0x15,
        0x00,
        0x10,
        0x33,
        0x37,
        0x35,
        0x37,
        0x64,
        0x32,
        0x38,
        0x34,
        0x38,
        0x31,
        0x30,
        0x34,
        0x32,
        0x32,
        0x32,
        0x65,
        0x00,
        0x08,
        0x00,
        0x14,
        0x32,
        0x2f,
        0xac,
        0xaf,
        0x98,
        0x84,
        0x74,
        0x19,
        0xd1,
        0x4b,
        0xda,
        0x26,
        0x2c,
        0x89,
        0x1a,
        0x0d,
        0x24,
        0x39,
        0xbf,
        0xd6,
    };

    BYTE channelData[] = {
        0x40, 0x01, 0x00, 0x60, 0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x2f, 0x77, 0x59, 0x57, 0x39, 0x4b, 0x69, 0x4a,
        0x53, 0x75, 0x4b, 0x45, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00,
        0xc0, 0x57, 0x00, 0x04, 0x00, 0x01, 0x00, 0x0a, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79,
        0x00, 0x24, 0x00, 0x04, 0x6e, 0x7e, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x3e, 0x39, 0x07, 0x98, 0xe5, 0x83, 0x14, 0x85,
        0x23, 0xb3, 0x29, 0xc1, 0x92, 0x47, 0x45, 0x0c, 0xad, 0xdb, 0xa1, 0x6d, 0x80, 0x28, 0x00, 0x04, 0x94, 0x6c, 0x5d, 0x00,
    };

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    TurnChannelData turnChannelData[10];
    UINT32 turnChannelDataCount = ARRAY_SIZE(turnChannelData);
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(turnReady == TRUE);

    EXPECT_EQ(STATUS_SUCCESS,
              turnConnectionIncomingDataHandler(pTurnConnection, incomingData, ARRAY_SIZE(incomingData), NULL, NULL, turnChannelData,
                                                &turnChannelDataCount));
    /* parsed out item is what we expected */
    EXPECT_EQ(turnChannelDataCount, 1);
    EXPECT_EQ(turnChannelData[0].size, ARRAY_SIZE(channelData) - TURN_DATA_CHANNEL_SEND_OVERHEAD);
    EXPECT_EQ(0, MEMCMP(turnChannelData[0].data, channelData + TURN_DATA_CHANNEL_SEND_OVERHEAD, turnChannelData[0].size));

    freeTestTurnConnection();
}

TEST_F(TurnConnectionFunctionalityTest, turnConnectionCallMultipleTurnSendDataInThreads)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    BOOL turnReady = FALSE;
    KvsIpAddress turnPeerAddr;
    const UINT32 bufLen = 5;
    const UINT32 reqCount = 5;
    BYTE buf[reqCount][bufLen];
    std::thread threads[reqCount];
    UINT32 i, j;
    UINT64 turnReadyTimeout = GETTIME() + 10 * HUNDREDS_OF_NANOS_IN_A_SECOND;

    initializeTestTurnConnection();

    turnPeerAddr.port = (UINT16) getInt16(8080);
    turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
    turnPeerAddr.isPointToPoint = FALSE;
    /* random peer 77.1.1.1, we are not actually sending anything to it. */
    turnPeerAddr.address[0] = 0x4d;
    turnPeerAddr.address[1] = 0x01;
    turnPeerAddr.address[2] = 0x01;
    turnPeerAddr.address[3] = 0x01;

    EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
    EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

    // wait until channel is created
    while (!turnReady && GETTIME() < turnReadyTimeout) {
        THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
        MUTEX_LOCK(pTurnConnection->lock);
        if (pTurnConnection->state == TURN_STATE_READY) {
            turnReady = TRUE;
        }
        MUTEX_UNLOCK(pTurnConnection->lock);
    }

    EXPECT_TRUE(turnReady == TRUE);

    for (i = 0; i < reqCount; i++) {
        for (j = 0; j < bufLen; j++) {
            buf[i][j] = i;
        }
        threads[i] = std::thread(
            [](PTurnConnection pTurnConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pKvsIpAddress) -> void {
                EXPECT_EQ(STATUS_SUCCESS, turnConnectionSendData(pTurnConnection, pBuf, bufLen, pKvsIpAddress));
            },
            pTurnConnection, (PBYTE) buf[i], bufLen, &turnPeerAddr);
    }

    for (i = 0; i < reqCount; i++) {
        threads[i].join();
    }

    // allocation should be refreshed.
    MUTEX_LOCK(pTurnConnection->lock);
    EXPECT_GE(pTurnConnection->allocationExpirationTime, GETTIME());
    MUTEX_UNLOCK(pTurnConnection->lock);

    freeTestTurnConnection();
}

// ---------------------------------------------------------------------------
// Injection / stress tests for the non-routable peer filter (issue #2386, PR #2388).
//
// A flood of local / non-routable peer candidates must be absorbed cheaply. Each is rejected by
// turnConnectionAddPeer at O(1) -- before the lock, the duplicate scan and the transaction-id-store
// allocation -- so it never enters turnPeerList and never produces a CreatePermission that the TURN
// server would reject with 403 Forbidden IP, and so cannot stall the state machine until the
// ~5s-per-state timeout. These are pure logic (no network / credentials), so they are not gated on
// mAccessKeyIdSet and always run. Exhaustive per-range coverage of the classifier itself lives in
// NetworkApiTest.IsNonRoutableAddr*; here the focus is scale and behaviour under a flood.

// Allocates a minimal TurnConnection usable by turnConnectionAddPeer and the state-machine
// predicates. stateTimeoutTime is pushed far into the future so advance decisions reflect
// peer-state counting, not a timeout fallthrough.
static PTurnConnection allocStressTestTurnConnection()
{
    PTurnConnection pTurnConnection = (PTurnConnection) MEMCALLOC(1, SIZEOF(TurnConnection));
    EXPECT_TRUE(pTurnConnection != NULL);
    if (pTurnConnection != NULL) {
        pTurnConnection->lock = MUTEX_CREATE(FALSE);
        pTurnConnection->state = TURN_STATE_CREATE_PERMISSION;
        pTurnConnection->stateTimeoutTime = GETTIME() + 100 * HUNDREDS_OF_NANOS_IN_A_SECOND;
        pTurnConnection->ipFamilyType = KVS_IP_FAMILY_TYPE_IPV4;
        pTurnConnection->disableNonRoutablePeersFilterForRelay = FALSE; // filter on (default)
    }
    return pTurnConnection;
}

// Frees a stress-test TurnConnection, including any transaction-id stores held by peers that were
// actually added (so CI sanitizer builds do not flag a leak).
static VOID freeStressTestTurnConnection(PTurnConnection pTurnConnection)
{
    if (pTurnConnection != NULL) {
        for (UINT32 i = 0; i < pTurnConnection->turnPeerCount; ++i) {
            if (pTurnConnection->turnPeerList[i].pTransactionIdStore != NULL) {
                freeTransactionIdStore(&pTurnConnection->turnPeerList[i].pTransactionIdStore);
            }
        }
        MUTEX_FREE(pTurnConnection->lock);
        MEMFREE(pTurnConnection);
    }
}

// Flooding turnConnectionAddPeer with a huge number of DISTINCT non-routable IPv4 addresses must be
// absorbed at O(1) each: none enter the peer list, all are counted as filtered, and the whole flood
// finishes in a tiny fraction of even a single ~5s per-state timeout.
TEST_F(TurnConnectionFunctionalityTest, turnConnectionFiltersDistinctNonRoutableFloodFast)
{
    const UINT32 floodCount = 500000; // all distinct within 10.0.0.0/8 (16.7M addresses)

    PTurnConnection pTurnConnection = allocStressTestTurnConnection();
    ASSERT_TRUE(pTurnConnection != NULL);

    KvsIpAddress peer;
    MEMSET(&peer, 0x00, SIZEOF(KvsIpAddress));
    peer.family = KVS_IP_FAMILY_TYPE_IPV4;
    peer.port = (UINT16) getInt16(8080);
    peer.address[0] = 10; // 10.0.0.0/8 private

    // The filter logs at INFO on every skip; silence it during the flood so the timing reflects the
    // filter cost, not 500k log writes. The fixture restores the level for the next test.
    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_WARN);

    BOOL allFiltered = TRUE;
    UINT64 startTime = GETTIME();
    for (UINT32 i = 0; i < floodCount; ++i) {
        // Pack i across the low three octets so every address is unique: 10.a.b.c.
        peer.address[1] = (BYTE) (i >> 16);
        peer.address[2] = (BYTE) (i >> 8);
        peer.address[3] = (BYTE) i;
        // A filtered peer is a successful no-op; accumulate instead of asserting 500k times so the
        // timed region reflects only the filter cost.
        allFiltered = (BOOL) (allFiltered && (turnConnectionAddPeer(pTurnConnection, &peer) == STATUS_SUCCESS));
    }
    UINT64 floodElapsed = GETTIME() - startTime;

    SET_LOGGER_LOG_LEVEL(mLogLevel);

    EXPECT_TRUE(allFiltered);
    // None of the flood entered the peer list; every one was counted as filtered.
    EXPECT_EQ((UINT32) 0, pTurnConnection->turnPeerCount);
    EXPECT_EQ(floodCount, pTurnConnection->nonRoutablePeersFilteredCount);

    // "Finishes fast": half a million distinct non-routable peers absorbed in well under a single
    // ~5s per-state timeout. Bounded generously to stay robust under CI sanitizer builds while still
    // catching a regression to a per-peer network / allocation path.
    EXPECT_LT(floodElapsed, 3 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    freeStressTestTurnConnection(pTurnConnection);
}

// A flood of non-routable peers interleaved with a few genuinely routable ones must not crowd out or
// delay the real peers: only the routable peers are added, and the state machine's advance condition
// is satisfiable immediately once they succeed -- it never waits on the doomed non-routable peers.
TEST_F(TurnConnectionFunctionalityTest, turnConnectionFloodDoesNotDelayRoutablePeers)
{
    const UINT32 floodCount = 100000;
    const UINT32 routableCount = 3; // stays under DEFAULT_TURN_MAX_PEER_COUNT

    PTurnConnection pTurnConnection = allocStressTestTurnConnection();
    ASSERT_TRUE(pTurnConnection != NULL);

    KvsIpAddress peer;
    MEMSET(&peer, 0x00, SIZEOF(KvsIpAddress));
    peer.family = KVS_IP_FAMILY_TYPE_IPV4;
    peer.port = (UINT16) getInt16(8080);

    SET_LOGGER_LOG_LEVEL(LOG_LEVEL_WARN);

    BOOL allOk = TRUE;
    UINT32 routableAdded = 0;
    UINT64 startTime = GETTIME();
    for (UINT32 i = 0; i < floodCount; ++i) {
        // Every (floodCount / routableCount)-th peer is a distinct public/routable address; the rest
        // are distinct non-routable 10/8 addresses.
        if (routableAdded < routableCount && (i % (floodCount / routableCount)) == 0) {
            peer.address[0] = 77; // 77.0.0.0/8 is public/routable
            peer.address[1] = 0x01;
            peer.address[2] = 0x01;
            peer.address[3] = (BYTE) (routableAdded + 1); // 77.1.1.1, 77.1.1.2, 77.1.1.3
            allOk = (BOOL) (allOk && (turnConnectionAddPeer(pTurnConnection, &peer) == STATUS_SUCCESS));
            routableAdded++;
        } else {
            peer.address[0] = 10; // 10.0.0.0/8 private
            peer.address[1] = (BYTE) (i >> 16);
            peer.address[2] = (BYTE) (i >> 8);
            peer.address[3] = (BYTE) i;
            allOk = (BOOL) (allOk && (turnConnectionAddPeer(pTurnConnection, &peer) == STATUS_SUCCESS));
        }
    }
    UINT64 floodElapsed = GETTIME() - startTime;

    SET_LOGGER_LOG_LEVEL(mLogLevel);

    EXPECT_TRUE(allOk);
    // Only the routable peers were added; every non-routable one was filtered.
    EXPECT_EQ(routableCount, pTurnConnection->turnPeerCount);
    EXPECT_EQ(floodCount - routableCount, pTurnConnection->nonRoutablePeersFilteredCount);

    // Once the routable peers succeed, the advance condition (all peers accounted for) is met with a
    // tiny peer set -- the flood never inflated turnPeerCount, so there is nothing doomed to wait on.
    for (UINT32 i = 0; i < pTurnConnection->turnPeerCount; ++i) {
        pTurnConnection->turnPeerList[i].connectionState = TURN_PEER_CONN_STATE_READY;
    }
    UINT64 nextState = TURN_STATE_NONE;
    EXPECT_EQ(STATUS_SUCCESS, fromCreatePermissionTurnState((UINT64) pTurnConnection, &nextState));
    EXPECT_EQ(TURN_STATE_BIND_CHANNEL, nextState);

    EXPECT_LT(floodElapsed, 2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    freeStressTestTurnConnection(pTurnConnection);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
