#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

    class TurnConnectionFunctionalityTest : public WebRtcClientTestBase {
    };

    /*
     * Given a valid turn endpoint and credentials, turnConnection should successfully allocate,
     * create permission, and create channel. Then manually trigger permission refresh and allocation refresh
     */
    TEST_F(TurnConnectionFunctionalityTest, turnConnectionRefreshPermissionTest)
    {
        UINT32 i, j, iceConfigCount, uriCount;
        PIceConfigInfo pIceConfigInfo;
        IceServer iceServers[MAX_ICE_SERVERS_COUNT];
        PIceServer pTurnServer = NULL;
        TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
        PConnectionListener pConnectionListener = NULL;
        PTurnConnection pTurnConnection = NULL;
        BOOL turnReady = FALSE;
        KvsIpAddress turnPeerAddr;
        PTurnPeer pTurnPeer = NULL;
        PDoubleListNode pDoubleListNode = NULL;

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
                DLOGD("url %s, password %s, usename %s", pTurnServer->url, pTurnServer->credential, pTurnServer->username);
            }
        }

        EXPECT_TRUE(pTurnServer != NULL);

        EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));
        EXPECT_EQ(STATUS_SUCCESS, createTurnConnection(pTurnServer, timerQueueHandle, pConnectionListener,
                                                       TURN_CONNECTION_DATA_TRANSFER_MODE_DATA_CHANNEL,
                                                       KVS_SOCKET_PROTOCOL_UDP, NULL, &pTurnConnection));

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

        // wait until channel is craeted
        while(!turnReady) {
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

        EXPECT_EQ(STATUS_SUCCESS, freeTurnConnection(&pTurnConnection));
        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
        timerQueueFree(&timerQueueHandle);
        deinitializeSignalingClient();
    }

}
}
}
}
}
