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

    ///////////////////////////////////////////////
    // ConnectionListener Test
    ///////////////////////////////////////////////

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
        KvsIpAddress localhost;

        MEMSET(&localhost, 0x00, SIZEOF(KvsIpAddress));

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
        ConnectionListenerTestCustomData routine1CustomData, routine2CustomData;
        TID routine1, routine2;
        UINT32 connectionCount = 0, newConnectionCount = 0;
        PSocketConnection pSocketConnection = NULL;
        KvsIpAddress localhost;
        PDoubleListNode pCurNode;

        MEMSET(&routine1CustomData, 0x0, SIZEOF(ConnectionListenerTestCustomData));
        MEMSET(&routine2CustomData, 0x0, SIZEOF(ConnectionListenerTestCustomData));
        MEMSET(&localhost, 0x0, SIZEOF(KvsIpAddress));

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

    ///////////////////////////////////////////////
    // IceAgent Test
    ///////////////////////////////////////////////

    TEST_F(IceFunctionalityTest, IceAgentComputeCandidatePairPriorityUnitTest)
    {
        // https://tools.ietf.org/html/rfc5245#appendix-B.5
        UINT32 G = 123, D = 456; // G is controlling, D is controlled
        UINT64 priority = (UINT64) pow(2, 32) * MIN(G,D) + 2 * MAX(G,D) + (G>D ? 1 : 0);
        IceCandidate localCandidate, remoteCandidate;
        IceCandidatePair iceCandidatePair;

        localCandidate.priority = G;
        remoteCandidate.priority = D;
        iceCandidatePair.local = &localCandidate;
        iceCandidatePair.remote = &remoteCandidate;

        EXPECT_EQ(priority, computeCandidatePairPriority(&iceCandidatePair, TRUE));
    }

    TEST_F(IceFunctionalityTest, IceAgentUpdateCandidateAddressUnitTest)
    {
        IceCandidate localCandidate;
        KvsIpAddress newIpAddress;

        MEMSET(&newIpAddress, 0x0, SIZEOF(KvsIpAddress));

        newIpAddress.port = 8080;
        newIpAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
        newIpAddress.isPointToPoint = FALSE;
        MEMSET(newIpAddress.address, 0x11, ARRAY_SIZE(newIpAddress.address));

        localCandidate.state = ICE_CANDIDATE_STATE_NEW;

        EXPECT_NE(STATUS_SUCCESS, updateCandidateAddress(NULL, &newIpAddress));
        EXPECT_NE(STATUS_SUCCESS, updateCandidateAddress(&localCandidate, NULL));
        localCandidate.iceCandidateType = ICE_CANDIDATE_TYPE_HOST;
        EXPECT_NE(STATUS_SUCCESS, updateCandidateAddress(&localCandidate, &newIpAddress));
        localCandidate.iceCandidateType = ICE_CANDIDATE_TYPE_SERVER_REFLEXIVE;
        EXPECT_EQ(STATUS_SUCCESS, updateCandidateAddress(&localCandidate, &newIpAddress));

        EXPECT_EQ(localCandidate.ipAddress.port, newIpAddress.port);
        EXPECT_EQ(0, MEMCMP(localCandidate.ipAddress.address, newIpAddress.address, IPV4_ADDRESS_LENGTH));
    }

    TEST_F(IceFunctionalityTest, IceAgentIceAgentAddIceServerUnitTest)
    {
        IceServer iceServer;

        MEMSET(&iceServer, 0x00, SIZEOF(IceServer));

        EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443", NULL, NULL));
        EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443", (PCHAR) "", (PCHAR) ""));

        EXPECT_NE(STATUS_SUCCESS, parseIceServer(&iceServer, NULL, NULL, NULL));
        EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_USERNAME, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", NULL, NULL));
        EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_CREDENTIAL, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", NULL));
        EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_USERNAME, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "", (PCHAR) ""));
        EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_CREDENTIAL, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) ""));
        EXPECT_NE(STATUS_SUCCESS, parseIceServer(NULL, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
        EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
        EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turns:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
        EXPECT_EQ(STATUS_ICE_URL_INVALID_PREFIX, parseIceServer(&iceServer, (PCHAR) "randomUrl", (PCHAR) "username", (PCHAR) "password"));
    }

    TEST_F(IceFunctionalityTest, IceAgentAddRemoteCandidateUnitTest)
    {
        IceAgent iceAgent;
        UINT32 remoteCandidateCount = 0;
        PCHAR hostCandidateStr = (PCHAR) "sdpMidate:543899094 1 udp 2122260223 12.131.158.132 64616 typ host generation 0 ufrag OFZ/ network-id 1 network-cost 10";
        PCHAR relayCandidateStr = (PCHAR) "sdpMidate:1501054171 1 udp 41885439 59.189.124.250 62834 typ relay raddr 205.251.233.176 rport 14669 generation 0 ufrag OFZ/ network-id 1 network-cost 10";
        IceCandidate testLocalCandidate;

        MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
        MEMSET(&testLocalCandidate, 0x00, SIZEOF(IceCandidate));

        // init needed members in iceAgent
        iceAgent.lock = MUTEX_CREATE(TRUE);
        EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.remoteCandidates));
        EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.localCandidates));
        iceAgent.pTurnConnection = NULL;
        iceAgent.iceAgentState = ICE_AGENT_STATE_GATHERING;

        // invalid input
        EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, NULL));
        EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(NULL, hostCandidateStr));
        EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, (PCHAR) ""));
        EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, (PCHAR) "randomStuff"));

        // add a local candidate so that iceCandidate pair will be formed when add remote candidate succeeded
        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemTail(iceAgent.localCandidates, (UINT64) &testLocalCandidate));
        EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, hostCandidateStr));
        EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, hostCandidateStr));
        EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
        // duplicated candidates are not added
        EXPECT_EQ(1, remoteCandidateCount);
        // no candidate pair is formed since iceAgentState is ICE_AGENT_STATE_GATHERING
        EXPECT_EQ(0, iceAgent.candidatePairCount);

        iceAgent.iceAgentState = ICE_AGENT_STATE_CHECK_CONNECTION;
        EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, relayCandidateStr));
        EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
        EXPECT_EQ(2, remoteCandidateCount);
        // candidate pair is formed since iceAgentState is not ICE_AGENT_STATE_GATHERING
        EXPECT_EQ(1, iceAgent.candidatePairCount);

        MUTEX_FREE(iceAgent.lock);
        freeTransactionIdStore(&iceAgent.candidatePairs[0]->pTransactionIdStore);
        SAFE_MEMFREE(iceAgent.candidatePairs[0]);
        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.remoteCandidates, TRUE));
        EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.remoteCandidates));
        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.localCandidates, FALSE));
        EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.localCandidates));
    }

    TEST_F(IceFunctionalityTest, IceAgentFindCandidateWithIpUnitTest)
    {
        DoubleList candidateList;
        KvsIpAddress ipAddress;
        PIceCandidate pIceCandidate = NULL;
        IceCandidate candidateInList;

        MEMSET(&candidateList, 0x00, SIZEOF(DoubleList));
        MEMSET(&ipAddress, 0x00, SIZEOF(KvsIpAddress));

        EXPECT_NE(STATUS_SUCCESS, findCandidateWithIp(NULL, NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, NULL));

        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&ipAddress, (PCHAR) "127.0.0.1"));
        ipAddress.port = 123;
        ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
        // nothing is found when candidate list is empty
        EXPECT_EQ(NULL, pIceCandidate);

        candidateInList.ipAddress = ipAddress;
        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(&candidateList, (UINT64) &candidateInList));

        ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
        // family not match
        EXPECT_EQ(NULL, pIceCandidate);

        ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&ipAddress, (PCHAR) "127.0.0.2"));
        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
        // address not match
        EXPECT_EQ(NULL, pIceCandidate);

        EXPECT_EQ(STATUS_SUCCESS, populateIpFromString(&ipAddress, (PCHAR) "127.0.0.1"));
        ipAddress.port = 124;
        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
        // port not match
        EXPECT_EQ(NULL, pIceCandidate);

        ipAddress.port = 123;
        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
        // everything match
        EXPECT_EQ(&candidateInList, pIceCandidate);

        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(&candidateList, FALSE));
    }

    TEST_F(IceFunctionalityTest, IceAgentFindCandidateWithConnectionHandleUnitTest)
    {
        DoubleList candidateList;
        PIceCandidate pIceCandidate = NULL;
        IceCandidate candidateInList;
        SocketConnection socketConnection1, socketConnection2;

        MEMSET(&candidateList, 0x00, SIZEOF(DoubleList));
        MEMSET(&socketConnection1, 0x00, SIZEOF(SocketConnection));
        MEMSET(&socketConnection2, 0x00, SIZEOF(SocketConnection));

        EXPECT_NE(STATUS_SUCCESS, findCandidateWithSocketConnection(NULL, NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, findCandidateWithSocketConnection(&socketConnection1, NULL, NULL));
        EXPECT_NE(STATUS_SUCCESS, findCandidateWithSocketConnection(&socketConnection1, &candidateList, NULL));

        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithSocketConnection(&socketConnection1, &candidateList, &pIceCandidate));
        // nothing is found when candidate list is empty
        EXPECT_EQ(NULL, pIceCandidate);

        candidateInList.pSocketConnection = &socketConnection1;
        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(&candidateList, (UINT64) &candidateInList));

        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithSocketConnection(&socketConnection2, &candidateList, &pIceCandidate));
        // no matching socket connection
        EXPECT_EQ(NULL, pIceCandidate);

        EXPECT_EQ(STATUS_SUCCESS, findCandidateWithSocketConnection(&socketConnection1, &candidateList, &pIceCandidate));
        // found matching socket connection
        EXPECT_EQ(&candidateInList, pIceCandidate);

        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(&candidateList, FALSE));
    }

    TEST_F(IceFunctionalityTest, IceAgentCreateIceCandidatePairsUnitTest)
    {
        IceAgent iceAgent;
        IceCandidate localCandidate1, localCandidate2;
        IceCandidate remoteCandidate1, remoteCandidate2;
        UINT32 i;

        MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
        MEMSET(&localCandidate1, 0x00, SIZEOF(IceCandidate));
        MEMSET(&localCandidate2, 0x00, SIZEOF(IceCandidate));
        MEMSET(&remoteCandidate1, 0x00, SIZEOF(IceCandidate));
        MEMSET(&remoteCandidate2, 0x00, SIZEOF(IceCandidate));
        EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.localCandidates));
        EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.remoteCandidates));


        EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(NULL, NULL, FALSE));
        EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, NULL, FALSE));
        EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(NULL, &localCandidate1, FALSE));

        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.localCandidates, (UINT64) &localCandidate1));
        EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &localCandidate1, FALSE));
        // no remote candidate to form pair with
        EXPECT_EQ(0, iceAgent.candidatePairCount);

        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.remoteCandidates, (UINT64) &remoteCandidate1));
        EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate1, TRUE));
        EXPECT_EQ(1, iceAgent.candidatePairCount);
        EXPECT_EQ(&localCandidate1, iceAgent.candidatePairs[0]->local);
        EXPECT_EQ(&remoteCandidate1, iceAgent.candidatePairs[0]->remote);

        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.localCandidates, (UINT64) &localCandidate2));
        EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &localCandidate2, FALSE));
        // 2 local vs 1 remote, thus 2 pairs
        EXPECT_EQ(2, iceAgent.candidatePairCount);

        EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.remoteCandidates, (UINT64) &remoteCandidate2));
        EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate2, TRUE));
        EXPECT_EQ(4, iceAgent.candidatePairCount);

        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.localCandidates, FALSE));
        EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.remoteCandidates, FALSE));

        EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.localCandidates));
        EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.remoteCandidates));

        for (i = 0; i < iceAgent.candidatePairCount; ++i) {
            EXPECT_EQ(STATUS_SUCCESS, freeTransactionIdStore(&iceAgent.candidatePairs[i]->pTransactionIdStore));
            SAFE_MEMFREE(iceAgent.candidatePairs[i]);
        }
    }

    TEST_F(IceFunctionalityTest, IceAgentPruneUnconnectedIceCandidatePairUnitTest)
    {
        IceAgent iceAgent;
        UINT32 i;

        MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));

        EXPECT_NE(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(NULL));
        // candidate pair count can be 0
        EXPECT_EQ(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(&iceAgent));

        for (i = 0; i < 5; ++i) {
            iceAgent.candidatePairs[i] = (PIceCandidatePair) MEMALLOC(SIZEOF(IceCandidatePair));
            iceAgent.candidatePairs[i]->priority = i * 100;
            iceAgent.candidatePairs[i]->state = (ICE_CANDIDATE_PAIR_STATE) i;
            EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT,
                                                               &iceAgent.candidatePairs[i]->pTransactionIdStore));
        }

        iceAgent.candidatePairCount = 5;
        EXPECT_EQ(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(&iceAgent));
        EXPECT_EQ(1, iceAgent.candidatePairCount);
        // candidate pair at index 3 is in state ICE_CANDIDATE_PAIR_STATE_SUCCEEDED.
        // only candidate pair with state ICE_CANDIDATE_PAIR_STATE_SUCCEEDED wont get deleted
        EXPECT_EQ(300, iceAgent.candidatePairs[0]->priority);

        freeTransactionIdStore(&iceAgent.candidatePairs[0]->pTransactionIdStore);
        SAFE_MEMFREE(iceAgent.candidatePairs[0]);
    }

}
}
}
}
}
