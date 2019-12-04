#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

    class IceFunctionalityTest : public WebRtcClientTestBase {
    };

    // check if iceCandidatePairs is in descending order
    BOOL candidatePairsInOrder(PIceCandidatePair *iceCandidatePairs, UINT32 candidatePairCount)
    {
        BOOL inOrder = TRUE;
        UINT32 i;

        if (candidatePairCount < 2) {
            return inOrder;
        }

        for(i = 0; i < candidatePairCount - 1 && inOrder; ++i) {
            if (iceCandidatePairs[i]->priority < iceCandidatePairs[i+1]->priority) {
                inOrder = FALSE;
            }
        }

        return inOrder;
    }

    TEST_F(IceFunctionalityTest, sortIceCandidatePairsTest)
    {
        IceAgent iceAgent;
        IceCandidatePair iceCandidatePair[10];
        UINT32 i;
        for (i = 0; i < 10; ++i) {
            iceAgent.candidatePairs[i] = &iceCandidatePair[i];
        }

        iceAgent.candidatePairCount = 0;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 1;
        iceCandidatePair[0].priority = 1;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 2;
        iceCandidatePair[0].priority = 1;
        iceCandidatePair[1].priority = 2;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 2;
        iceCandidatePair[0].priority = 2;
        iceCandidatePair[1].priority = 1;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 3;
        iceCandidatePair[0].priority = 1;
        iceCandidatePair[1].priority = 1;
        iceCandidatePair[2].priority = 2;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 3;
        iceCandidatePair[0].priority = 1;
        iceCandidatePair[1].priority = 2;
        iceCandidatePair[2].priority = 1;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));

        iceAgent.candidatePairCount = 10;
        iceCandidatePair[0].priority = 12312;
        iceCandidatePair[1].priority = 23;
        iceCandidatePair[2].priority = 656;
        iceCandidatePair[3].priority = 123123;
        iceCandidatePair[4].priority = 432432;
        iceCandidatePair[5].priority = 312312312;
        iceCandidatePair[6].priority = 123123;
        iceCandidatePair[7].priority = 4546457;
        iceCandidatePair[8].priority = 87867;
        iceCandidatePair[9].priority = 87678;
        EXPECT_EQ(STATUS_SUCCESS, sortIceCandidatePairs(&iceAgent));
        EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.candidatePairs, iceAgent.candidatePairCount));
    }

    typedef struct {
        PConnectionListener pConnectionListener;
        UINT32 connectionToAdd;
    } ConnectionListenerTestCustomData,*PConnectionListenerTestCustomData;

    PVOID connectionListenAddConnectionRoutine(PVOID arg)
    {
        PConnectionListenerTestCustomData pCustomData = (PConnectionListenerTestCustomData) arg;
        UINT32 i;
        UINT64 randomDelay;
        PSocketConnection pSocketConnection = NULL;
        KvsIpAddress localhost = {0};

        localhost.family = KVS_IP_FAMILY_TYPE_IPV4;
        localhost.isPointToPoint = FALSE;
        // 127.0.0.1
        localhost.address[0] = 0x7f;
        localhost.address[1] = 0x00;
        localhost.address[2] = 0x00;
        localhost.address[3] = 0x01;
        localhost.port = 0;

        for(i = 0; i < pCustomData->connectionToAdd; ++i) {
            randomDelay = (UINT64) (RAND() % 300) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
            THREAD_SLEEP(randomDelay);
            CHECK(STATUS_SUCCEEDED(createSocketConnection(&localhost, NULL, KVS_SOCKET_PROTOCOL_UDP, 0, NULL, &pSocketConnection)));
            CHECK(STATUS_SUCCEEDED(connectionListenerAddConnection(pCustomData->pConnectionListener, pSocketConnection)));
        }

        return 0;
    }

    TEST_F(IceFunctionalityTest, connectionListenerFunctionalityTest)
    {
        PConnectionListener pConnectionListener;
        ConnectionListenerTestCustomData routine1CustomData = {0};
        ConnectionListenerTestCustomData routine2CustomData = {0};
        TID routine1, routine2;
        UINT32 connectionCount = 0, newConnectionCount = 0;
        PSocketConnection pSocketConnection = NULL;
        KvsIpAddress localhost = {0};
        PDoubleListNode pCurNode;

        localhost.family = KVS_IP_FAMILY_TYPE_IPV4;
        localhost.isPointToPoint = FALSE;
        // 127.0.0.1
        localhost.address[0] = 0x7f;
        localhost.address[1] = 0x00;
        localhost.address[2] = 0x00;
        localhost.address[3] = 0x01;
        localhost.port = 0;

        EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerStart(pConnectionListener));

        routine1CustomData.pConnectionListener = pConnectionListener;
        routine2CustomData.pConnectionListener = pConnectionListener;
        routine1CustomData.connectionToAdd = 3;
        routine2CustomData.connectionToAdd = 7;

        THREAD_CREATE(&routine1, connectionListenAddConnectionRoutine, (PVOID) &routine1CustomData);
        THREAD_CREATE(&routine2, connectionListenAddConnectionRoutine, (PVOID) &routine2CustomData);

        THREAD_JOIN(routine1, NULL);
        THREAD_JOIN(routine2, NULL);

        EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pConnectionListener->connectionList, &connectionCount));
        EXPECT_EQ(connectionCount, routine1CustomData.connectionToAdd + routine2CustomData.connectionToAdd);

        CHECK(STATUS_SUCCEEDED(createSocketConnection(&localhost, NULL, KVS_SOCKET_PROTOCOL_UDP, 0, NULL, &pSocketConnection)));
        EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pSocketConnection));

        EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pConnectionListener->connectionList, &newConnectionCount));
        EXPECT_EQ(connectionCount + 1, newConnectionCount);

        EXPECT_EQ(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, pSocketConnection));
        EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(pConnectionListener->connectionList, &newConnectionCount));
        EXPECT_EQ(connectionCount, newConnectionCount);
        EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSocketConnection));

        EXPECT_EQ(TRUE, IS_VALID_TID_VALUE(pConnectionListener->receiveDataRoutine));
        ATOMIC_STORE_BOOL(&pConnectionListener->terminate, TRUE);

        THREAD_SLEEP((SOCKET_WAIT_FOR_DATA_TIMEOUT_SECONDS + 1) * HUNDREDS_OF_NANOS_IN_A_SECOND);

        EXPECT_EQ(FALSE, ATOMIC_LOAD_BOOL(&pConnectionListener->listenerRoutineStarted));

        EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(pConnectionListener->connectionList, &pCurNode));
        while(pCurNode != NULL) {
            pSocketConnection = (PSocketConnection) pCurNode->data;
            EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSocketConnection));
            pCurNode = pCurNode->pNext;
        }

        EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));
    }


}
}
}
}
}