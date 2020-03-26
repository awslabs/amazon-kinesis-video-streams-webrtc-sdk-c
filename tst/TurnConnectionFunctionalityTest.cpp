#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

    class TurnConnectionFunctionalityTest : public WebRtcClientTestBase {
        PIceConfigInfo pIceConfigInfo;
        TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
        PConnectionListener pConnectionListener = NULL;

    public:
        VOID initializeTestTurnConnection(PTurnConnection *ppTurnConnection)
        {
            UINT32 i, j, iceConfigCount, uriCount;
            IceServer iceServers[MAX_ICE_SERVERS_COUNT];
            PIceServer pTurnServer = NULL;
            PTurnConnection pTurnConnection = NULL;

            EXPECT_TRUE(ppTurnConnection != NULL);

            initializeSignalingClient();
            EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfoCount(mSignalingClientHandle, &iceConfigCount));

            for (uriCount = 0, i = 0; i < iceConfigCount; i++) {
                EXPECT_EQ(STATUS_SUCCESS, signalingClientGetIceConfigInfo(mSignalingClientHandle, i, &pIceConfigInfo));
                for (j = 0; j < pIceConfigInfo->uriCount; j++) {
                    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServers[uriCount++], pIceConfigInfo->uris[j], pIceConfigInfo->userName, pIceConfigInfo->password));

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
            EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
            EXPECT_EQ(STATUS_SUCCESS, createTurnConnection(pTurnServer, timerQueueHandle, pConnectionListener,
                                                           TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL,
                                                           KVS_SOCKET_PROTOCOL_UDP, NULL, 0, &pTurnConnection, NULL));

            *ppTurnConnection = pTurnConnection;
        }

        VOID freeTestTurnConnection(PTurnConnection *ppTurnConnection)
        {
            EXPECT_TRUE(ppTurnConnection != NULL);

            EXPECT_EQ(STATUS_SUCCESS, freeTurnConnection(ppTurnConnection));
            EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
            timerQueueFree(&timerQueueHandle);
            deinitializeSignalingClient();
        }
    };

    /*
     * Given a valid turn endpoint and credentials, turnConnection should successfully allocate,
     * create permission, and create channel. Then manually trigger permission refresh and allocation refresh
     */
    TEST_F(TurnConnectionFunctionalityTest, turnConnectionRefreshPermissionTest)
    {
        if (!mAccessKeyIdSet) {
            return;
        }

        PTurnConnection pTurnConnection = NULL;
        BOOL turnReady = FALSE;
        KvsIpAddress turnPeerAddr;
        PTurnPeer pTurnPeer = NULL;
        PDoubleListNode pDoubleListNode = NULL;

        initializeTestTurnConnection(&pTurnConnection);

        turnPeerAddr.port = (UINT16) getInt16(8080);
        turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
        turnPeerAddr.isPointToPoint = FALSE;
        // random peer 10.1.1.1, we are not actually sending anything to it.
        turnPeerAddr.address[0] = 0x0A;
        turnPeerAddr.address[1] = 0x01;
        turnPeerAddr.address[2] = 0x01;
        turnPeerAddr.address[3] = 0x01;

        EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

        // wait until channel is created
        while(!turnReady) {
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            MUTEX_LOCK(pTurnConnection->lock);
            if (pTurnConnection->state == TURN_STATE_READY) {
                turnReady = TRUE;
            }
            MUTEX_UNLOCK(pTurnConnection->lock);
        }

        EXPECT_TRUE(turnReady == TRUE);

        // modify permission expiration time to trigger refresh permission
        MUTEX_LOCK(pTurnConnection->lock);
        EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(pTurnConnection->turnPeerList, &pDoubleListNode));
        pTurnPeer = (PTurnPeer) pDoubleListNode->data;
        pTurnPeer->permissionExpirationTime = GETTIME();
        MUTEX_UNLOCK(pTurnConnection->lock);

        // turn Connection timer run happens every second when at ready state.
        THREAD_SLEEP(1500 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);

        // verify we are no longer in ready state.
        MUTEX_LOCK(pTurnConnection->lock);
        EXPECT_TRUE(pTurnConnection->state != TURN_STATE_READY);
        MUTEX_UNLOCK(pTurnConnection->lock);

        turnReady = FALSE;

        while(!turnReady) {
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

        freeTestTurnConnection(&pTurnConnection);
    }

    TEST_F(TurnConnectionFunctionalityTest, turnConnectionStop)
    {
        if (!mAccessKeyIdSet) {
            return;
        }

        PTurnConnection pTurnConnection = NULL;
        BOOL turnReady = FALSE;
        KvsIpAddress turnPeerAddr;

        initializeTestTurnConnection(&pTurnConnection);

        turnPeerAddr.port = (UINT16) getInt16(8080);
        turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
        turnPeerAddr.isPointToPoint = FALSE;
        // random peer 10.1.1.1, we are not actually sending anything to it.
        turnPeerAddr.address[0] = 0x0A;
        turnPeerAddr.address[1] = 0x01;
        turnPeerAddr.address[2] = 0x01;
        turnPeerAddr.address[3] = 0x01;

        EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

        // wait until channel is created
        while(!turnReady) {
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            MUTEX_LOCK(pTurnConnection->lock);
            if (pTurnConnection->state == TURN_STATE_READY) {
                turnReady = TRUE;
            }
            MUTEX_UNLOCK(pTurnConnection->lock);
        }

        EXPECT_TRUE(turnReady == TRUE);
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionStop(pTurnConnection));

        // once clean up starts, turn connection still needs to send allocation with lifetime 0 and wait for response before
        // moving to state new. Thus multiplying 1.5
        THREAD_SLEEP((UINT64) (DEFAULT_TURN_START_CLEAN_UP_TIMEOUT * 1.5));
        MUTEX_LOCK(pTurnConnection->lock);
        // turn connection should've cleaned itself up and move to NEW state.
        EXPECT_TRUE(pTurnConnection->state == TURN_STATE_NEW);
        MUTEX_UNLOCK(pTurnConnection->lock);

        freeTestTurnConnection(&pTurnConnection);
    }

    STATUS turnConnectionReceivePartialChannelMessageTestTurnApplicationDataHandler(
            UINT64 customData, PSocketConnection pSocketConnection, PBYTE pBuffer, UINT32 bufferLen,
            PKvsIpAddress pSrc, PKvsIpAddress pDest)
    {
        UNUSED_PARAM(pSocketConnection);
        UNUSED_PARAM(pSrc);
        UNUSED_PARAM(pDest);
        PBYTE *pCurrentMessagePosition = (PBYTE*) customData;
        PBYTE currentMessagePosition = *pCurrentMessagePosition;
        UINT16 messageLen = (UINT16) getInt16(*(PINT16) (currentMessagePosition + 2));

        // should receive complete data each time callback is called.
        EXPECT_EQ(messageLen, bufferLen);
        EXPECT_EQ(0, MEMCMP(currentMessagePosition + 4, pBuffer, messageLen));
        // move pointer to next channel message
        currentMessagePosition += (4 + messageLen);
        *pCurrentMessagePosition = currentMessagePosition;

        return STATUS_SUCCESS;
    }

    TEST_F(TurnConnectionFunctionalityTest, turnConnectionReceivePartialChannelMessageTest)
    {
        if (!mAccessKeyIdSet) {
            return;
        }


        // there are 3 channel messages for channel 0x4001
        BYTE channelMsg[] = {0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x42, 0x37, 0x73, 0x2f,
                             0x51, 0x48, 0x7a, 0x54, 0x69, 0x69, 0x32, 0x7a, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55,
                             0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00,
                             0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00, 0x00,
                             0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0xe5, 0xf4, 0xfc, 0x35,
                             0xee, 0x7c, 0x13, 0x51, 0x14, 0x5d, 0xdb, 0xa7, 0xb0, 0xa7, 0xb1, 0xd4, 0x2b, 0xd3, 0x5f, 0x5b,
                             0x80, 0x28, 0x00, 0x04, 0x6a, 0x64, 0x06, 0x57, 0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50,
                             0x21, 0x12, 0xa4, 0x42, 0x41, 0x48, 0x31, 0x46, 0x54, 0x55, 0x4b, 0x39, 0x2b, 0x61, 0x52, 0x32,
                             0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55, 0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00,
                             0xc0, 0x57, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e,
                             0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff,
                             0x00, 0x08, 0x00, 0x14, 0x9a, 0x02, 0x8e, 0x1a, 0x75, 0x41, 0x97, 0xdf, 0x3b, 0x7a, 0x50, 0xc7,
                             0x26, 0xda, 0x18, 0x85, 0x86, 0x28, 0x2c, 0xcb, 0x80, 0x28, 0x00, 0x04, 0xaf, 0xdc, 0xa8, 0x68,
                             0x40, 0x01, 0x00, 0x60, 0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x2f, 0x77, 0x59, 0x57,
                             0x39, 0x4b, 0x69, 0x4a, 0x53, 0x75, 0x4b, 0x45, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55,
                             0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04, 0x00, 0x01, 0x00, 0x0a,
                             0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x24, 0x00, 0x04,
                             0x6e, 0x7e, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x3e, 0x39, 0x07, 0x98, 0xe5, 0x83, 0x14, 0x85,
                             0x23, 0xb3, 0x29, 0xc1, 0x92, 0x47, 0x45, 0x0c, 0xad, 0xdb, 0xa1, 0x6d, 0x80, 0x28, 0x00, 0x04,
                             0x94, 0x6c, 0x5d, 0x00};
        // breakdown of channel data in channelMsg
        BYTE channelData1[] = {
                0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x42, 0x37, 0x73, 0x2f,
                0x51, 0x48, 0x7a, 0x54, 0x69, 0x69, 0x32, 0x7a, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55,
                0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00,
                0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00, 0x00,
                0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0xe5, 0xf4, 0xfc, 0x35,
                0xee, 0x7c, 0x13, 0x51, 0x14, 0x5d, 0xdb, 0xa7, 0xb0, 0xa7, 0xb1, 0xd4, 0x2b, 0xd3, 0x5f, 0x5b,
                0x80, 0x28, 0x00, 0x04, 0x6a, 0x64, 0x06, 0x57,
        };

        BYTE channelData2[] = {
                0x40, 0x01, 0x00, 0x64, 0x00, 0x01, 0x00, 0x50, 0x21, 0x12, 0xa4, 0x42, 0x41, 0x48, 0x31, 0x46,
                0x54, 0x55, 0x4b, 0x39, 0x2b, 0x61, 0x52, 0x32, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55,
                0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00,
                0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x25, 0x00, 0x00,
                0x00, 0x24, 0x00, 0x04, 0x6e, 0x7f, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x9a, 0x02, 0x8e, 0x1a,
                0x75, 0x41, 0x97, 0xdf, 0x3b, 0x7a, 0x50, 0xc7, 0x26, 0xda, 0x18, 0x85, 0x86, 0x28, 0x2c, 0xcb,
                0x80, 0x28, 0x00, 0x04, 0xaf, 0xdc, 0xa8, 0x68,
        };

        BYTE channelData3[] = {
                0x40, 0x01, 0x00, 0x60, 0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x2f, 0x77, 0x59, 0x57,
                0x39, 0x4b, 0x69, 0x4a, 0x53, 0x75, 0x4b, 0x45, 0x00, 0x06, 0x00, 0x09, 0x79, 0x45, 0x78, 0x55,
                0x3a, 0x31, 0x63, 0x39, 0x64, 0x00, 0x00, 0x00, 0xc0, 0x57, 0x00, 0x04, 0x00, 0x01, 0x00, 0x0a,
                0x80, 0x2a, 0x00, 0x08, 0xe9, 0x60, 0x24, 0x7e, 0x0a, 0xd6, 0xc4, 0x79, 0x00, 0x24, 0x00, 0x04,
                0x6e, 0x7e, 0x1e, 0xff, 0x00, 0x08, 0x00, 0x14, 0x3e, 0x39, 0x07, 0x98, 0xe5, 0x83, 0x14, 0x85,
                0x23, 0xb3, 0x29, 0xc1, 0x92, 0x47, 0x45, 0x0c, 0xad, 0xdb, 0xa1, 0x6d, 0x80, 0x28, 0x00, 0x04,
                0x94, 0x6c, 0x5d, 0x00,
        };

        PTurnConnection pTurnConnection = NULL;
        BOOL turnReady = FALSE;
        KvsIpAddress turnPeerAddr;
        PBYTE currentMessagePosition = channelMsg;
        TurnChannelData turnChannelData[10];
        UINT32 turnChannelDataCount = 0;

        initializeTestTurnConnection(&pTurnConnection);

        turnPeerAddr.port = (UINT16) getInt16(8080);
        turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
        turnPeerAddr.isPointToPoint = FALSE;
        // random peer 10.1.1.1, we are not actually sending anything to it.
        turnPeerAddr.address[0] = 0x0A;
        turnPeerAddr.address[1] = 0x01;
        turnPeerAddr.address[2] = 0x01;
        turnPeerAddr.address[3] = 0x01;

        pTurnConnection->turnConnectionCallbacks.applicationDataAvailableFn = turnConnectionReceivePartialChannelMessageTestTurnApplicationDataHandler;
        pTurnConnection->turnConnectionCallbacks.customData = (UINT64) &currentMessagePosition;
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

        // wait until channel is created
        while(!turnReady) {
            THREAD_SLEEP(100 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND);
            MUTEX_LOCK(pTurnConnection->lock);
            if (pTurnConnection->state == TURN_STATE_READY) {
                turnReady = TRUE;
            }
            MUTEX_UNLOCK(pTurnConnection->lock);
        }

        EXPECT_TRUE(turnReady == TRUE);

        // index 120 lands in the middle of the second channel message. Simulating a partial channel message received from
        // socket
        turnChannelDataCount = ARRAY_SIZE(turnChannelData);
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionHandleChannelDataTcpMode(pTurnConnection, channelMsg, 120, turnChannelData, &turnChannelDataCount));
        EXPECT_EQ(turnChannelDataCount, 1);
        EXPECT_EQ(turnChannelData[0].size, ARRAY_SIZE(channelData1));
        EXPECT_EQ(0, MEMCMP(turnChannelData[0].data, channelData1, turnChannelData[0].size));

        turnChannelDataCount = ARRAY_SIZE(turnChannelData);
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionHandleChannelDataTcpMode(pTurnConnection, channelMsg + 120,
                                                                         ARRAY_SIZE(channelMsg) - 120, turnChannelData, &turnChannelDataCount));
        EXPECT_EQ(turnChannelDataCount, 2);
        EXPECT_EQ(turnChannelData[0].size, ARRAY_SIZE(channelData2));
        EXPECT_EQ(turnChannelData[1].size, ARRAY_SIZE(channelData3));
        EXPECT_EQ(0, MEMCMP(turnChannelData[0].data, channelData2, turnChannelData[0].size));
        EXPECT_EQ(0, MEMCMP(turnChannelData[1].data, channelData3, turnChannelData[1].size));

        freeTestTurnConnection(&pTurnConnection);
    }

    TEST_F(TurnConnectionFunctionalityTest, turnConnectionCallMultipleTurnSendDataInThreads)
    {
        if (!mAccessKeyIdSet) {
            return;
        }

        PTurnConnection pTurnConnection = NULL;
        BOOL turnReady = FALSE;
        KvsIpAddress turnPeerAddr;
        const UINT32 bufLen = 5;
        const UINT32 reqCount = 5;
        BYTE buf[reqCount][bufLen];
        std::thread threads[reqCount];
        UINT32 i, j;

        initializeTestTurnConnection(&pTurnConnection);

        turnPeerAddr.port = (UINT16) getInt16(8080);
        turnPeerAddr.family = KVS_IP_FAMILY_TYPE_IPV4;
        turnPeerAddr.isPointToPoint = FALSE;
        // random peer 10.1.1.1, we are not actually sending anything to it.
        turnPeerAddr.address[0] = 0x0A;
        turnPeerAddr.address[1] = 0x01;
        turnPeerAddr.address[2] = 0x01;
        turnPeerAddr.address[3] = 0x01;

        EXPECT_EQ(STATUS_SUCCESS, turnConnectionAddPeer(pTurnConnection, &turnPeerAddr));
        EXPECT_EQ(STATUS_SUCCESS, turnConnectionStart(pTurnConnection));

        // wait until channel is created
        while(!turnReady) {
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
            threads[i] = std::thread([](PTurnConnection pTurnConnection, PBYTE pBuf, UINT32 bufLen, PKvsIpAddress pKvsIpAddress) -> void {
                EXPECT_EQ(STATUS_SUCCESS, turnConnectionSendData(pTurnConnection, pBuf, bufLen, pKvsIpAddress));
            }, pTurnConnection, (PBYTE) buf[i], bufLen, &turnPeerAddr);
        }

        for (i = 0; i < reqCount; i++) {
            threads[i].join();
        }

        // allocation should be refreshed.
        MUTEX_LOCK(pTurnConnection->lock);
        EXPECT_GE(pTurnConnection->allocationExpirationTime, GETTIME());
        MUTEX_UNLOCK(pTurnConnection->lock);

        freeTestTurnConnection(&pTurnConnection);
    }

}
}
}
}
}
