#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IceFunctionalityTest : public WebRtcClientTestBase {
};

// check if iceCandidatePairs is in descending order
BOOL candidatePairsInOrder(PDoubleList iceCandidatePairs)
{
    BOOL inOrder = TRUE;
    UINT64 previousPriority = MAX_UINT64;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL && inOrder) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        if (pIceCandidatePair->priority > previousPriority) {
            inOrder = FALSE;
        }

        previousPriority = pIceCandidatePair->priority;
    }

    return inOrder;
}

TEST_F(IceFunctionalityTest, sortIceCandidatePairsTest)
{
    IceAgent iceAgent;
    IceCandidatePair iceCandidatePair[10];
    UINT32 i;

    doubleListCreate(&iceAgent.iceCandidatePairs);

    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));

    iceCandidatePair[0].priority = 1;
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[0]));
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));

    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 2;
    for (i = 0; i < 2; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[i]));
    }
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));

    iceCandidatePair[0].priority = 2;
    iceCandidatePair[1].priority = 1;
    for (i = 0; i < 2; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[i]));
    }
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));

    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 1;
    iceCandidatePair[2].priority = 2;
    for (i = 0; i < 3; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[i]));
    }
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));

    iceCandidatePair[0].priority = 1;
    iceCandidatePair[1].priority = 2;
    iceCandidatePair[2].priority = 1;
    for (i = 0; i < 3; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[i]));
    }
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));

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
    for (i = 0; i < 10; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, &iceCandidatePair[i]));
    }
    EXPECT_EQ(TRUE, candidatePairsInOrder(iceAgent.iceCandidatePairs));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
}

///////////////////////////////////////////////
// ConnectionListener Test
///////////////////////////////////////////////

typedef struct {
    PConnectionListener pConnectionListener;
    UINT32 connectionToAdd;
    KVS_IP_FAMILY_TYPE family;
    PSocketConnection socketConnectionList[10];
} ConnectionListenerTestCustomData, *PConnectionListenerTestCustomData;

PVOID connectionListenAddConnectionRoutine(PVOID arg)
{
    PConnectionListenerTestCustomData pCustomData = (PConnectionListenerTestCustomData) arg;
    UINT32 i;
    UINT64 randomDelay;
    PSocketConnection pSocketConnection = NULL;
    KvsIpAddress localhost;

    MEMSET(&localhost, 0x00, SIZEOF(KvsIpAddress));

    localhost.isPointToPoint = FALSE;
    localhost.port = 0;
    localhost.family = pCustomData->family;
    if (pCustomData->family == KVS_IP_FAMILY_TYPE_IPV4) {
        // 127.0.0.1
        localhost.address[0] = 0x7f;
        localhost.address[1] = 0x00;
        localhost.address[2] = 0x00;
        localhost.address[3] = 0x01;
    } else {
        // ::1
        localhost.address[15] = 1;
    }

    for (i = 0; i < pCustomData->connectionToAdd; ++i) {
        randomDelay = (UINT64)(RAND() % 300) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
        THREAD_SLEEP(randomDelay);
        CHECK(STATUS_SUCCEEDED(createSocketConnection((KVS_IP_FAMILY_TYPE) localhost.family, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, 0, NULL, 0,
                                                      &pSocketConnection)));
        pCustomData->socketConnectionList[i] = pSocketConnection;
        CHECK(STATUS_SUCCEEDED(connectionListenerAddConnection(pCustomData->pConnectionListener, pSocketConnection)));
    }

    return 0;
}

TEST_F(IceFunctionalityTest, connectionListenerFunctionalityTest)
{
    PConnectionListener pConnectionListener;
    ConnectionListenerTestCustomData routine1CustomData, routine2CustomData;
    TID routine1, routine2;
    UINT32 connectionCount , newConnectionCount, i;
    PSocketConnection pSocketConnection = NULL;
    KvsIpAddress localhost;
    TID threadId;

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
    routine1CustomData.family = KVS_IP_FAMILY_TYPE_IPV4;
    routine2CustomData.family = KVS_IP_FAMILY_TYPE_IPV6;

    THREAD_CREATE(&routine1, connectionListenAddConnectionRoutine, (PVOID) &routine1CustomData);
    THREAD_CREATE(&routine2, connectionListenAddConnectionRoutine, (PVOID) &routine2CustomData);

    THREAD_JOIN(routine1, NULL);
    THREAD_JOIN(routine2, NULL);

    connectionCount = pConnectionListener->socketCount;
    EXPECT_EQ(connectionCount, routine1CustomData.connectionToAdd + routine2CustomData.connectionToAdd);

    CHECK(STATUS_SUCCEEDED(
        createSocketConnection((KVS_IP_FAMILY_TYPE) localhost.family, KVS_SOCKET_PROTOCOL_UDP, &localhost, NULL, 0, NULL, 0, &pSocketConnection)));
    EXPECT_EQ(STATUS_SUCCESS, connectionListenerAddConnection(pConnectionListener, pSocketConnection));

    newConnectionCount = pConnectionListener->socketCount;
    EXPECT_EQ(connectionCount + 1, newConnectionCount);

    EXPECT_EQ(STATUS_SUCCESS, connectionListenerRemoveConnection(pConnectionListener, pSocketConnection));
    newConnectionCount = pConnectionListener->socketCount;
    EXPECT_EQ(connectionCount, newConnectionCount);

    // Keeping TSAN happy need to lock/unlock when retrieving the value of TID
    MUTEX_LOCK(pConnectionListener->lock);
    threadId = pConnectionListener->receiveDataRoutine;
    MUTEX_UNLOCK(pConnectionListener->lock);
    EXPECT_TRUE( IS_VALID_TID_VALUE(threadId));
    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, TRUE);

    THREAD_SLEEP(CONNECTION_LISTENER_SHUTDOWN_TIMEOUT + 1 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    MUTEX_LOCK(pConnectionListener->lock);
    threadId = pConnectionListener->receiveDataRoutine;
    MUTEX_UNLOCK(pConnectionListener->lock);
    EXPECT_FALSE( IS_VALID_TID_VALUE(threadId));

    EXPECT_EQ(STATUS_SUCCESS, freeConnectionListener(&pConnectionListener));

    EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&pSocketConnection));

    for (i = 0; i < routine1CustomData.connectionToAdd; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&routine1CustomData.socketConnectionList[i]));
    }

    for (i = 0; i < routine2CustomData.connectionToAdd; ++i) {
        EXPECT_EQ(STATUS_SUCCESS, freeSocketConnection(&routine2CustomData.socketConnectionList[i]));
    }
}

///////////////////////////////////////////////
// IceAgent Test
///////////////////////////////////////////////

TEST_F(IceFunctionalityTest, IceAgentComputeCandidatePairPriorityUnitTest)
{
    // https://tools.ietf.org/html/rfc5245#appendix-B.5
    UINT32 G = 123, D = 456; // G is controlling, D is controlled
    UINT64 priority = (UINT64) pow(2, 32) * MIN(G, D) + 2 * MAX(G, D) + (G > D ? 1 : 0);
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

    newIpAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    localCandidate.state = ICE_CANDIDATE_STATE_NEW;
    EXPECT_EQ(STATUS_SUCCESS, updateCandidateAddress(&localCandidate, &newIpAddress));

    EXPECT_EQ(localCandidate.ipAddress.port, newIpAddress.port);
    EXPECT_EQ(0, MEMCMP(localCandidate.ipAddress.address, newIpAddress.address, IPV6_ADDRESS_LENGTH));
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
    EXPECT_TRUE(iceServer.isSecure);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_NONE);
    EXPECT_EQ(STATUS_ICE_URL_INVALID_PREFIX, parseIceServer(&iceServer, (PCHAR) "randomUrl", (PCHAR) "username", (PCHAR) "password"));

    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turns:54.202.170.151:443?transport=tcp", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_TRUE(iceServer.isSecure);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_TCP);
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turns:54.202.170.151:443?transport=udp", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_TRUE(iceServer.isSecure);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_UDP);
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443?transport=tcp", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_TRUE(!iceServer.isSecure);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_TCP);
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443?transport=udp", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_TRUE(!iceServer.isSecure);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_UDP);
    EXPECT_EQ(443, (UINT16) getInt16(iceServer.ipAddress.port));

    /* we are not doing full validation. Only parsing out what we know */
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443?randomstuff", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_NONE);
}

TEST_F(IceFunctionalityTest, IceAgentAddRemoteCandidateUnitTest)
{
    IceAgent iceAgent;
    UINT32 remoteCandidateCount = 0, iceCandidateCount = 0;
    PCHAR ip4HostCandidateStr =
        (PCHAR) "sdpMidate:543899094 1 udp 2122260223 12.131.158.132 64616 typ host generation 0 ufrag OFZ/ network-id 1 network-cost 10";
    PCHAR ip6HostCandidateStr = (PCHAR) "candidate:2526845803 1 udp 2122262783 2600:1700:cd70:2540:fd41:66ab:a9cd:f0aa 55216 typ host generation 0 "
                                        "ufrag qnXe network-id 2 network-cost 10";
    PCHAR relayCandidateStr = (PCHAR) "sdpMidate:1501054171 1 udp 41885439 59.189.124.250 62834 typ relay raddr 205.251.233.176 rport 14669 "
                                      "generation 0 ufrag OFZ/ network-id 1 network-cost 10";
    IceCandidate ip4TestLocalCandidate, ip6TestLocalCandidate;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    MEMSET(&ip4TestLocalCandidate, 0x00, SIZEOF(IceCandidate));
    MEMSET(&ip6TestLocalCandidate, 0x00, SIZEOF(IceCandidate));
    ip4TestLocalCandidate.state = ICE_CANDIDATE_STATE_VALID;
    ip4TestLocalCandidate.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
    ip6TestLocalCandidate.state = ICE_CANDIDATE_STATE_VALID;
    ip6TestLocalCandidate.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;

    // init needed members in iceAgent
    iceAgent.lock = MUTEX_CREATE(TRUE);
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.remoteCandidates));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.localCandidates));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));
    iceAgent.iceAgentState = ICE_CANDIDATE_STATE_NEW;

    // invalid input
    EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(NULL, NULL));
    EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, NULL));
    EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(NULL, ip4HostCandidateStr));
    EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, (PCHAR) ""));
    EXPECT_NE(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, (PCHAR) "randomStuff"));

    // add a ip4 local candidate so that iceCandidate pair will be formed when add remote candidate succeeded
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemTail(iceAgent.localCandidates, (UINT64) &ip4TestLocalCandidate));
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, ip4HostCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, ip4HostCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
    // duplicated candidates are not added
    EXPECT_EQ(1, remoteCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.remoteCandidates, &pCurNode));
    // parsing candidate priority correctly
    EXPECT_EQ(2122260223, ((PIceCandidate)pCurNode->data)->priority);

    // candidate pair formed
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(1, iceCandidateCount);

    // add an ip6 local candidate so that iceCandidate pair will be formed when add remote candidate succeeded
    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemTail(iceAgent.localCandidates, (UINT64) &ip6TestLocalCandidate));
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, ip6HostCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, ip6HostCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
    // duplicated candidates are not added
    EXPECT_EQ(2, remoteCandidateCount);
    // candidate pair formed
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(2, iceCandidateCount);

    // parsing candidate priority correctly
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.remoteCandidates, &pCurNode));
    EXPECT_EQ(2122262783, ((PIceCandidate)pCurNode->data)->priority);

    iceAgent.iceAgentState = ICE_AGENT_STATE_CHECK_CONNECTION;
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, relayCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
    EXPECT_EQ(3, remoteCandidateCount);
    // candidate pair formed
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(3, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.remoteCandidates, &pCurNode));
    // parsing candidate priority correctly
    EXPECT_EQ(41885439, ((PIceCandidate)pCurNode->data)->priority);

    MUTEX_FREE(iceAgent.lock);
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    }
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
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

    EXPECT_EQ(1, inet_pton(AF_INET, (PCHAR) "127.0.0.1", &ipAddress.address));
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
    EXPECT_EQ(1, inet_pton(AF_INET, (PCHAR) "127.0.0.2", &ipAddress.address));
    EXPECT_EQ(STATUS_SUCCESS, findCandidateWithIp(&ipAddress, &candidateList, &pIceCandidate));
    // address not match
    EXPECT_EQ(NULL, pIceCandidate);

    EXPECT_EQ(1, inet_pton(AF_INET, (PCHAR) "127.0.0.1", &ipAddress.address));
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
    IceCandidate remoteCandidate1, remoteCandidate2, remoteCandidate3;
    UINT32 iceCandidateCount = 0;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    MEMSET(&localCandidate1, 0x00, SIZEOF(IceCandidate));
    MEMSET(&localCandidate2, 0x00, SIZEOF(IceCandidate));
    localCandidate1.state = ICE_CANDIDATE_STATE_VALID;
    localCandidate2.state = ICE_CANDIDATE_STATE_VALID;
    localCandidate1.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
    localCandidate2.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    MEMSET(&remoteCandidate1, 0x00, SIZEOF(IceCandidate));
    MEMSET(&remoteCandidate2, 0x00, SIZEOF(IceCandidate));
    MEMSET(&remoteCandidate3, 0x00, SIZEOF(IceCandidate));
    remoteCandidate1.state = ICE_CANDIDATE_STATE_VALID;
    remoteCandidate2.state = ICE_CANDIDATE_STATE_VALID;
    remoteCandidate3.state = ICE_CANDIDATE_STATE_VALID;
    remoteCandidate1.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    remoteCandidate2.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    remoteCandidate3.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.localCandidates));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.remoteCandidates));
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(NULL, NULL, FALSE));
    EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, NULL, FALSE));
    EXPECT_NE(STATUS_SUCCESS, createIceCandidatePairs(NULL, &localCandidate1, FALSE));

    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.localCandidates, (UINT64) &localCandidate1));
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &localCandidate1, FALSE));
    // no remote candidate to form pair with
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(0, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.remoteCandidates, (UINT64) &remoteCandidate1));
    remoteCandidate1.state = ICE_CANDIDATE_STATE_NEW;
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate1, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    // candidate has to be in ICE_CANDIDATE_STATE_VALID to form pair
    EXPECT_EQ(0, iceCandidateCount);

    remoteCandidate1.state = ICE_CANDIDATE_STATE_VALID;
    localCandidate1.state = ICE_CANDIDATE_STATE_NEW;
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate1, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    // candidate has to be in ICE_CANDIDATE_STATE_VALID to form pair
    EXPECT_EQ(0, iceCandidateCount);

    remoteCandidate1.state = ICE_CANDIDATE_STATE_VALID;
    localCandidate1.state = ICE_CANDIDATE_STATE_VALID;
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate1, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    // candidate has to be the same socket family type
    EXPECT_EQ(0, iceCandidateCount);

    remoteCandidate1.ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate1, TRUE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    // both candidate are valid now. Ice candidate pair should be created
    EXPECT_EQ(1, iceCandidateCount);
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
    EXPECT_EQ(&localCandidate1, pIceCandidatePair->local);
    EXPECT_EQ(&remoteCandidate1, pIceCandidatePair->remote);

    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.localCandidates, (UINT64) &localCandidate2));
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &localCandidate2, FALSE));
    // 1 local ip4 & 1 local ip6 vs 1 remote ip4, thus 1 pair
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(1, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.remoteCandidates, (UINT64) &remoteCandidate2));
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate2, TRUE));
    // 1 local ip4 & 1 local ip6 vs 1 remote ip4 & 1 remote ip6, thus 2 pairs
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(2, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListInsertItemHead(iceAgent.remoteCandidates, (UINT64) &remoteCandidate3));
    EXPECT_EQ(STATUS_SUCCESS, createIceCandidatePairs(&iceAgent, &remoteCandidate3, TRUE));
    // 1 local ip4 & 1 local ip6 vs 1 remote ip4 & 2 remote ip6, thus 3 pairs
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(3, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.localCandidates, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.remoteCandidates, FALSE));

    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.localCandidates));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.remoteCandidates));

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    }
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
}

TEST_F(IceFunctionalityTest, IceAgentPruneUnconnectedIceCandidatePairUnitTest)
{
    IceAgent iceAgent;
    UINT32 i, iceCandidateCount = 0;
    PIceCandidatePair iceCandidatePairs[5];
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    doubleListCreate(&iceAgent.iceCandidatePairs);

    EXPECT_NE(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(NULL));
    // candidate pair count can be 0
    EXPECT_EQ(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(&iceAgent));

    for (i = 0; i < 5; ++i) {
        iceCandidatePairs[i] = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
        iceCandidatePairs[i]->priority = i * 100;
        iceCandidatePairs[i]->state = (ICE_CANDIDATE_PAIR_STATE) i;
        EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &iceCandidatePairs[i]->pTransactionIdStore));
        EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, iceCandidatePairs[i]));
    }

    EXPECT_EQ(STATUS_SUCCESS, pruneUnconnectedIceCandidatePair(&iceAgent));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(1, iceCandidateCount);
    // candidate pair at index 3 is in state ICE_CANDIDATE_PAIR_STATE_SUCCEEDED.
    // only candidate pair with state ICE_CANDIDATE_PAIR_STATE_SUCCEEDED wont get deleted
    doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode);
    pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
    EXPECT_EQ(300, pIceCandidatePair->priority);

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;

        CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    }
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
}

TEST_F(IceFunctionalityTest, IceAgentCandidateGatheringTest)
{
    if (!mAccessKeyIdSet) {
        return;
    }

    typedef struct {
        std::vector<std::string> list;
        std::mutex lock;
    } CandidateList;

    PIceAgent pIceAgent = NULL;
    CHAR localIceUfrag[LOCAL_ICE_UFRAG_LEN + 1];
    CHAR localIcePwd[LOCAL_ICE_PWD_LEN + 1];
    RtcConfiguration configuration;
    IceAgentCallbacks iceAgentCallbacks;
    PConnectionListener pConnectionListener = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    BOOL foundHostCandidate = FALSE, foundSrflxCandidate = FALSE, foundRelayCandidate = FALSE;
    CandidateList candidateList;

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(localIceUfrag, 0x00, SIZEOF(localIceUfrag));
    MEMSET(localIcePwd, 0x00, SIZEOF(localIcePwd));
    MEMSET(&iceAgentCallbacks, 0x00, SIZEOF(IceAgentCallbacks));

    initializeSignalingClient();
    getIceServers(&configuration);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        CandidateList* candidateList1 = (CandidateList*) customData;
        candidateList1->lock.lock();
        if (candidateStr != NULL) {
            candidateList1->list.push_back(std::string(candidateStr));
        } else {
            candidateList1->list.push_back("");
        }
        candidateList1->lock.unlock();
    };

    iceAgentCallbacks.customData = (UINT64) &candidateList;
    iceAgentCallbacks.newLocalCandidateFn = onICECandidateHdlr;

    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(localIceUfrag, LOCAL_ICE_UFRAG_LEN));
    EXPECT_EQ(STATUS_SUCCESS, generateJSONSafeString(localIcePwd, LOCAL_ICE_PWD_LEN));
    EXPECT_EQ(STATUS_SUCCESS, createConnectionListener(&pConnectionListener));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS,
              createIceAgent(localIceUfrag, localIcePwd, &iceAgentCallbacks, &configuration, timerQueueHandle, pConnectionListener, &pIceAgent));

    EXPECT_EQ(STATUS_SUCCESS, iceAgentStartGathering(pIceAgent));

    THREAD_SLEEP(KVS_ICE_GATHER_REFLEXIVE_AND_RELAYED_CANDIDATE_TIMEOUT + 2 * HUNDREDS_OF_NANOS_IN_A_SECOND);

    // newLocalCandidateFn should've returned null in its last invocation, which was converted to empty string
    candidateList.lock.lock();
    EXPECT_TRUE(candidateList.list[candidateList.list.size() - 1].empty());

    for (std::vector<std::string>::iterator it = candidateList.list.begin(); it != candidateList.list.end(); ++it) {
        std::string candidateStr = *it;
        if (candidateStr.find(std::string(SDP_CANDIDATE_TYPE_HOST)) != std::string::npos) {
            foundHostCandidate = TRUE;
        } else if (candidateStr.find(std::string(SDP_CANDIDATE_TYPE_SERFLX)) != std::string::npos) {
            foundSrflxCandidate = TRUE;
        } else if (candidateStr.find(std::string(SDP_CANDIDATE_TYPE_RELAY)) != std::string::npos) {
            foundRelayCandidate = TRUE;
        }
    }
    candidateList.lock.unlock();

    EXPECT_TRUE(foundHostCandidate && foundSrflxCandidate && foundRelayCandidate);
    EXPECT_EQ(STATUS_SUCCESS, iceAgentShutdown(pIceAgent));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueShutdown(timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeIceAgent(&pIceAgent));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueFree(&timerQueueHandle));

    deinitializeSignalingClient();
}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
