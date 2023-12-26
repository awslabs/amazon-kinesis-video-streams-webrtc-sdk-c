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
            if (localIpInterfaces[i].family == pTurnServer->ipAddress.family && (pTurnSocketAddr == NULL || localIpInterfaces[i].isPointToPoint)) {
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
                  createSocketConnection((KVS_IP_FAMILY_TYPE) pTurnServer->ipAddress.family, KVS_ICE_DEFAULT_TURN_PROTOCOL, NULL,
                                         &pTurnServer->ipAddress, (UINT64) this, onDataHandler, 0, &pTurnSocket));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pTurnSocket));
        ASSERT_EQ(STATUS_SUCCESS,
                  createTurnConnection(pTurnServer, timerQueueHandle, TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL, KVS_ICE_DEFAULT_TURN_PROTOCOL,
                                       NULL, pTurnSocket, pConnectionListener, &pTurnConnection));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
    }

    VOID freeTestTurnConnection()
    {
        EXPECT_TRUE(pTurnConnection != NULL);
        EXPECT_EQ(STATUS_SUCCESS, freeTurnConnection(&pTurnConnection));
        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
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

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
