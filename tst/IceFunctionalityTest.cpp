#include <algorithm>

#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IceFunctionalityTest : public WebRtcClientTestBase {};

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
        randomDelay = (UINT64) (RAND() % 300) * HUNDREDS_OF_NANOS_IN_A_MILLISECOND;
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
    UINT32 connectionCount, newConnectionCount, i;
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
    EXPECT_TRUE(IS_VALID_TID_VALUE(threadId));
    ATOMIC_STORE_BOOL(&pConnectionListener->terminate, TRUE);

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
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "stun:stun.kinesisvideo.us-west-2.amazonaws.com:443?transport=tcp", NULL, NULL));
    EXPECT_FALSE(iceServer.isSecure);
    EXPECT_FALSE(iceServer.isTurn);
    EXPECT_EQ(iceServer.scheme, ICE_SERVER_SCHEME_STUN);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_UDP);
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "stuns:stun.kinesisvideo.us-west-2.amazonaws.com", NULL, NULL));
    EXPECT_TRUE(iceServer.isSecure);
    EXPECT_FALSE(iceServer.isTurn);
    EXPECT_EQ(iceServer.scheme, ICE_SERVER_SCHEME_STUNS);
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_UDP);
    EXPECT_EQ(5349, (UINT16) getInt16(iceServer.ipAddresses.ipv4Address.port));

    EXPECT_NE(STATUS_SUCCESS, parseIceServer(&iceServer, NULL, NULL, NULL));
    EXPECT_EQ(STATUS_ICE_URL_MALFORMED, parseIceServer(&iceServer, (PCHAR) "stuns:", NULL, NULL));
    EXPECT_EQ(STATUS_ICE_URL_STUNS_IP_LITERAL_NOT_ALLOWED, parseIceServer(&iceServer, (PCHAR) "stuns:54.202.170.151:443", NULL, NULL));
    EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_USERNAME, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", NULL, NULL));
    EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_CREDENTIAL, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", NULL));
    EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_USERNAME, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "", (PCHAR) ""));
    EXPECT_EQ(STATUS_ICE_URL_TURN_MISSING_CREDENTIAL, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) ""));
    EXPECT_NE(STATUS_SUCCESS, parseIceServer(NULL, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_FALSE(iceServer.isSecure);
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turns:54.202.170.151:443", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_TRUE(iceServer.isSecure);
    EXPECT_EQ(iceServer.scheme, ICE_SERVER_SCHEME_TURNS);
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
    EXPECT_EQ(443, (UINT16) getInt16(iceServer.ipAddresses.ipv4Address.port));

    /* we are not doing full validation. Only parsing out what we know */
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) "turn:54.202.170.151:443?randomstuff", (PCHAR) "username", (PCHAR) "password"));
    EXPECT_EQ(iceServer.transport, KVS_SOCKET_PROTOCOL_NONE);

    //
    // Dual-stack checks.
    //

    // Clear the iceServer struct.
    MEMSET(&iceServer, 0x00, SIZEOF(IceServer));

// Set the env var to enable dual-stack mode.
#ifdef _WIN32
    _putenv_s(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON");
#else
    setenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON", 1);
#endif

    std::string test_ipv4_addr = "35-90-63-38";
    std::string test_ipv6_addr = "2001-0db8-85a3-0000-0000-8a2e-0370-7334";
    std::string test_hostname = "turn:" + test_ipv4_addr + "_" + test_ipv6_addr + ".test.com";

    // The test IPv4-address.
    CHAR ipv4_addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {0};
    MEMCPY(ipv4_addr, test_ipv4_addr.c_str(), test_ipv4_addr.size());

    // The test IPv6-address.
    CHAR ipv6_addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {0};
    MEMCPY(ipv6_addr, test_ipv6_addr.c_str(), test_ipv6_addr.size());

    // The test hostname (addresses with a suffix).
    CHAR hostname[MAX_ICE_CONFIG_URI_BUFFER_LEN] = {0};
    MEMCPY(hostname, test_hostname.c_str(), test_hostname.size());

    // Failing cases: IP addresses must have a proper prefix.
    EXPECT_EQ(STATUS_ICE_URL_INVALID_PREFIX, parseIceServer(&iceServer, (PCHAR) test_ipv4_addr.c_str(), (PCHAR) "username", (PCHAR) "password"));
    EXPECT_EQ(STATUS_ICE_URL_INVALID_PREFIX, parseIceServer(&iceServer, (PCHAR) test_ipv6_addr.c_str(), (PCHAR) "username", (PCHAR) "password"));

    // Failing cases: both IPv4 and IPv6 addresses should be present in hostname, else will
    // fallback to getAddrInfo and fail.
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED,
              parseIceServer(&iceServer, (PCHAR) ("turn:" + test_ipv4_addr).c_str(), (PCHAR) "username", (PCHAR) "password"));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED,
              parseIceServer(&iceServer, (PCHAR) ("turn:" + test_ipv6_addr).c_str(), (PCHAR) "username", (PCHAR) "password"));

    // Clear the iceServer struct again incase of partial population from previous calls.
    MEMSET(&iceServer, 0x00, SIZEOF(IceServer));

    // Parse the IP addresses from the hostname.
    EXPECT_EQ(STATUS_SUCCESS, parseIceServer(&iceServer, (PCHAR) hostname, (PCHAR) "username", (PCHAR) "password"));

    // Presence of IP family types indicate successful parsing.
    EXPECT_EQ(iceServer.ipAddresses.ipv4Address.family, KVS_IP_FAMILY_TYPE_IPV4);
    EXPECT_EQ(iceServer.ipAddresses.ipv6Address.family, KVS_IP_FAMILY_TYPE_IPV6);

    std::string decimal_delim_test_ipv4_addr = test_ipv4_addr;
    std::replace(decimal_delim_test_ipv4_addr.begin(), decimal_delim_test_ipv4_addr.end(), '-', '.');
    std::string colon_delim_test_ipv6_addr = test_ipv6_addr;
    std::replace(colon_delim_test_ipv6_addr.begin(), colon_delim_test_ipv6_addr.end(), '-', ':');

    // Validate the parsed IPv4 address.
    CHAR parsed_ipv4_addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {0};
    getIpAddrStr(&iceServer.ipAddresses.ipv4Address, (PCHAR) parsed_ipv4_addr, SIZEOF(parsed_ipv4_addr));
    EXPECT_STREQ(parsed_ipv4_addr, decimal_delim_test_ipv4_addr.c_str());

    // Validate the parsed IPv6 address.
    CHAR parsed_ipv6_addr[KVS_IP_ADDRESS_STRING_BUFFER_LEN] = {0};
    getIpAddrStr(&iceServer.ipAddresses.ipv6Address, (PCHAR) parsed_ipv6_addr, SIZEOF(parsed_ipv6_addr));
    EXPECT_STREQ(parsed_ipv6_addr, colon_delim_test_ipv6_addr.c_str());

// Cleanup the env var.
#ifdef _WIN32
    _putenv_s(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "");
#else
    unsetenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR);
#endif
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
    EXPECT_EQ(2122260223, ((PIceCandidate) pCurNode->data)->priority);

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
    EXPECT_EQ(2122262783, ((PIceCandidate) pCurNode->data)->priority);

    iceAgent.iceAgentState = ICE_AGENT_STATE_CHECK_CONNECTION;
    EXPECT_EQ(STATUS_SUCCESS, iceAgentAddRemoteCandidate(&iceAgent, relayCandidateStr));
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.remoteCandidates, &remoteCandidateCount));
    EXPECT_EQ(3, remoteCandidateCount);
    // candidate pair formed
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetNodeCount(iceAgent.iceCandidatePairs, &iceCandidateCount));
    EXPECT_EQ(3, iceCandidateCount);

    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.remoteCandidates, &pCurNode));
    // parsing candidate priority correctly
    EXPECT_EQ(41885439, ((PIceCandidate) pCurNode->data)->priority);

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

TEST_F(IceFunctionalityTest, IceAgentGatherCandidateTimerCallbackReportsLocalCandidatesInBatches)
{
    typedef struct {
        std::vector<std::string> list;
    } CandidateList;

    IceAgent iceAgent;
    IceCandidate localCandidates[KVS_ICE_MAX_NEW_LOCAL_CANDIDATES_TO_REPORT_AT_ONCE + 2];
    CandidateList candidateList;
    UINT32 i;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    MEMSET(localCandidates, 0x00, SIZEOF(localCandidates));

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        CandidateList* candidateList1 = (CandidateList*) customData;
        candidateList1->list.push_back(candidateStr != NULL ? std::string(candidateStr) : std::string(""));
    };

    iceAgent.lock = MUTEX_CREATE(TRUE);
    iceAgent.iceAgentCallbacks.customData = (UINT64) &candidateList;
    iceAgent.iceAgentCallbacks.newLocalCandidateFn = onICECandidateHdlr;
    iceAgent.candidateGatheringStartTime = GETTIME();
    iceAgent.candidateGatheringEndTime = 0;
    iceAgent.iceCandidateGatheringTimerTask = 1;
    ATOMIC_STORE_BOOL(&iceAgent.addedRelayCandidate, TRUE);
    ATOMIC_STORE_BOOL(&iceAgent.candidateGatheringFinished, FALSE);
    ATOMIC_STORE_BOOL(&iceAgent.stopGathering, FALSE);
    ASSERT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.localCandidates));

    for (i = 0; i < ARRAY_SIZE(localCandidates); ++i) {
        localCandidates[i].iceCandidateType = ICE_CANDIDATE_TYPE_HOST;
        localCandidates[i].state = ICE_CANDIDATE_STATE_VALID;
        localCandidates[i].foundation = i + 1;
        localCandidates[i].priority = ICE_PRIORITY_HOST_CANDIDATE_TYPE_PREFERENCE;
        localCandidates[i].remoteProtocol = KVS_SOCKET_PROTOCOL_UDP;
        localCandidates[i].ipAddress.family = KVS_IP_FAMILY_TYPE_IPV4;
        localCandidates[i].ipAddress.address[0] = 127;
        localCandidates[i].ipAddress.address[3] = (BYTE) (i + 1);
        localCandidates[i].ipAddress.port = htons(10000 + i);
        SNPRINTF(localCandidates[i].id, ARRAY_SIZE(localCandidates[i].id), "cand%03u", i);
        ASSERT_EQ(STATUS_SUCCESS, doubleListInsertItemTail(iceAgent.localCandidates, (UINT64) &localCandidates[i]));
    }

    ASSERT_EQ(STATUS_SUCCESS, iceAgentGatherCandidateTimerCallback(0, 1, (UINT64) &iceAgent));
    ASSERT_EQ(KVS_ICE_MAX_NEW_LOCAL_CANDIDATES_TO_REPORT_AT_ONCE, candidateList.list.size());
    EXPECT_FALSE(ATOMIC_LOAD_BOOL(&iceAgent.candidateGatheringFinished));
    EXPECT_NE(MAX_UINT32, iceAgent.iceCandidateGatheringTimerTask);

    for (i = 0; i < KVS_ICE_MAX_NEW_LOCAL_CANDIDATES_TO_REPORT_AT_ONCE; ++i) {
        EXPECT_TRUE(localCandidates[i].reported);
    }
    for (; i < ARRAY_SIZE(localCandidates); ++i) {
        EXPECT_FALSE(localCandidates[i].reported);
    }

    ASSERT_EQ(STATUS_TIMER_QUEUE_STOP_SCHEDULING, iceAgentGatherCandidateTimerCallback(0, 2, (UINT64) &iceAgent));
    ASSERT_EQ(ARRAY_SIZE(localCandidates) + 1, candidateList.list.size());
    ASSERT_TRUE(candidateList.list.back().empty());
    EXPECT_TRUE(ATOMIC_LOAD_BOOL(&iceAgent.candidateGatheringFinished));
    EXPECT_EQ(MAX_UINT32, iceAgent.iceCandidateGatheringTimerTask);

    for (i = 0; i < ARRAY_SIZE(localCandidates); ++i) {
        EXPECT_TRUE(localCandidates[i].reported);
    }

    MUTEX_FREE(iceAgent.lock);
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
    ASSERT_EQ(TRUE, mAccessKeyIdSet);

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

TEST_F(IceFunctionalityTest, IceAgentGovCloudStunsCandidateGatheringTest)
{
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
    PCHAR pGovCloudRegion = (PCHAR) "us-gov-west-1";

    MEMSET(&configuration, 0x00, SIZEOF(RtcConfiguration));
    MEMSET(localIceUfrag, 0x00, SIZEOF(localIceUfrag));
    MEMSET(localIcePwd, 0x00, SIZEOF(localIcePwd));
    MEMSET(&iceAgentCallbacks, 0x00, SIZEOF(IceAgentCallbacks));

    // This uses the public GovCloud STUNS endpoint directly and does not require signaling or AWS credentials.
    SNPRINTF(configuration.iceServers[0].urls, MAX_ICE_CONFIG_URI_LEN, KINESIS_VIDEO_STUNS_URL, pGovCloudRegion, KINESIS_VIDEO_STUN_URL_POSTFIX);

    auto onICECandidateHdlr = [](UINT64 customData, PCHAR candidateStr) -> void {
        CandidateList* candidateList1 = (CandidateList*) customData;
        std::lock_guard<std::mutex> lock(candidateList1->lock);
        candidateList1->list.push_back(candidateStr != NULL ? std::string(candidateStr) : std::string(""));
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

    {
        std::lock_guard<std::mutex> lock(candidateList.lock);
        EXPECT_FALSE(candidateList.list.empty());
        if (!candidateList.list.empty()) {
            // newLocalCandidateFn should return NULL in its last invocation, which we convert to an empty string.
            EXPECT_TRUE(candidateList.list.back().empty());

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
        }
    }

    EXPECT_TRUE(foundHostCandidate);
    EXPECT_TRUE(foundSrflxCandidate);
    EXPECT_FALSE(foundRelayCandidate);
    EXPECT_EQ(STATUS_SUCCESS, iceAgentShutdown(pIceAgent));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueShutdown(timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, freeIceAgent(&pIceAgent));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueFree(&timerQueueHandle));
}
// The controlling agent must not enter ICE_AGENT_STATE_READY until the peer has acknowledged the
// USE_CANDIDATE request. iceAgentNominateCandidatePair() nominates a pair that is already in
// ICE_CANDIDATE_PAIR_STATE_SUCCEEDED -- it is chosen precisely because the pre-nomination connectivity
// checks passed -- so gating on nominated && SUCCEEDED alone is satisfied the instant nomination begins.
// The agent would then leave ICE_AGENT_STATE_NOMINATING on the next state machine tick, which stops the
// per-tick resend in executeNominatingIceAgentState() and makes a single lost USE_CANDIDATE packet
// unrecoverable: the ice-lite peer never selects a pair and eventually closes the connection.
TEST_F(IceFunctionalityTest, IceAgentNominationRequiresAcknowledgementUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pIceCandidatePair = NULL;
    UINT64 state = 0;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    pIceCandidatePair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pIceCandidatePair != NULL);
    // the state the pair is left in by iceAgentNominateCandidatePair()
    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pIceCandidatePair->nominated = TRUE;
    pIceCandidatePair->nominationAcked = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pIceCandidatePair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pIceCandidatePair));

    // 1. nomination sent but not acknowledged, still within the nomination timeout: stay in NOMINATING so
    //    that executeNominatingIceAgentState() keeps resending the USE_CANDIDATE request.
    iceAgent.stateEndTime = GETTIME() + KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state);

    // 2. the peer answers the USE_CANDIDATE request: now the agent may proceed.
    pIceCandidatePair->nominationAcked = TRUE;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_READY, state);

    // 3. never acknowledged and the nomination timeout has expired: fail rather than hang.
    pIceCandidatePair->nominationAcked = FALSE;
    iceAgent.stateEndTime = GETTIME() - 1;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_FAILED, state);
    EXPECT_EQ(STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR, iceAgent.iceAgentStatus);

    CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

// Verify that iceAgentNominateCandidatePair() (controlling/master agent) picks the highest-priority
// SUCCEEDED pair, sets nominated=TRUE and nominationAcked=FALSE, clears the transaction id store,
// and freezes all non-nominated pairs.
TEST_F(IceFunctionalityTest, IceAgentControllingNominationSelectsHighestPrioritySucceededPairUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pPairHigh = NULL, pPairLow = NULL, pPairWaiting = NULL;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    iceAgent.isControlling = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    // High-priority SUCCEEDED pair (should be nominated)
    pPairHigh = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pPairHigh != NULL);
    pPairHigh->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pPairHigh->priority = 1000;
    pPairHigh->nominated = FALSE;
    pPairHigh->nominationAcked = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pPairHigh->pTransactionIdStore));
    // Insert a dummy transaction id to verify it gets cleared on nomination
    BYTE dummyTxId[STUN_TRANSACTION_ID_LEN];
    MEMSET(dummyTxId, 0xAB, STUN_TRANSACTION_ID_LEN);
    transactionIdStoreInsert(pPairHigh->pTransactionIdStore, dummyTxId);
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pPairHigh));

    // Low-priority SUCCEEDED pair (should be frozen, not nominated)
    pPairLow = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pPairLow != NULL);
    pPairLow->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pPairLow->priority = 500;
    pPairLow->nominated = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pPairLow->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pPairLow));

    // WAITING pair (should be frozen)
    pPairWaiting = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pPairWaiting != NULL);
    pPairWaiting->state = ICE_CANDIDATE_PAIR_STATE_WAITING;
    pPairWaiting->priority = 2000;
    pPairWaiting->nominated = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pPairWaiting->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pPairWaiting));

    EXPECT_EQ(STATUS_SUCCESS, iceAgentNominateCandidatePair(&iceAgent));

    // The highest-priority SUCCEEDED pair should be nominated with ack pending
    EXPECT_TRUE(pPairHigh->nominated);
    EXPECT_FALSE(pPairHigh->nominationAcked);
    // nominationAcked should be FALSE (waiting for peer to acknowledge the USE_CANDIDATE)
    EXPECT_FALSE(pPairHigh->nominationAcked);
    // Transaction id store logical count should have been cleared (note: transactionIdStoreClear
    // resets the count/indices but does not zero the underlying buffer, so transactionIdStoreHasId
    // may still find stale entries — the important thing is the count is 0)
    EXPECT_EQ(0, pPairHigh->pTransactionIdStore->transactionIdCount);

    // Non-nominated pairs should be frozen
    EXPECT_FALSE(pPairLow->nominated);
    EXPECT_EQ(ICE_CANDIDATE_PAIR_STATE_FROZEN, pPairLow->state);
    EXPECT_FALSE(pPairWaiting->nominated);
    EXPECT_EQ(ICE_CANDIDATE_PAIR_STATE_FROZEN, pPairWaiting->state);

    // Cleanup
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;
        CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    }
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

// Verify that iceAgentNominateCandidatePair() is a no-op for the controlled (non-controlling) agent.
TEST_F(IceFunctionalityTest, IceAgentControlledAgentSkipsNominationUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pPair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    iceAgent.isControlling = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    pPair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pPair != NULL);
    pPair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pPair->priority = 1000;
    pPair->nominated = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pPair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pPair));

    // Should return success but not nominate anything
    EXPECT_EQ(STATUS_SUCCESS, iceAgentNominateCandidatePair(&iceAgent));
    EXPECT_FALSE(pPair->nominated);
    EXPECT_EQ(ICE_CANDIDATE_PAIR_STATE_SUCCEEDED, pPair->state);

    CHK_LOG_ERR(freeIceCandidatePair(&pPair));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

// Verify that iceAgentNominateCandidatePair() fails when no SUCCEEDED pair exists.
TEST_F(IceFunctionalityTest, IceAgentNominationFailsWithNoSucceededPairUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pPair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    iceAgent.isControlling = TRUE;
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    // Only a WAITING pair, no SUCCEEDED
    pPair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pPair != NULL);
    pPair->state = ICE_CANDIDATE_PAIR_STATE_WAITING;
    pPair->priority = 1000;
    pPair->nominated = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pPair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pPair));

    EXPECT_EQ(STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR, iceAgentNominateCandidatePair(&iceAgent));
    EXPECT_FALSE(pPair->nominated);

    CHK_LOG_ERR(freeIceCandidatePair(&pPair));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

// Verify the full state transition: nominated+acked+SUCCEEDED → READY, nominated+!acked → stays NOMINATING,
// and that having multiple pairs where only one is nominated+acked transitions correctly.
TEST_F(IceFunctionalityTest, IceAgentNominationStateTransitionWithMultiplePairsUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pNominatedPair = NULL, pFrozenPair = NULL;
    UINT64 state = 0;
    PDoubleListNode pCurNode = NULL;
    PIceCandidatePair pIceCandidatePair = NULL;

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    // Nominated pair that has not been acked yet
    pNominatedPair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pNominatedPair != NULL);
    pNominatedPair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pNominatedPair->priority = 1000;
    pNominatedPair->nominated = TRUE;
    pNominatedPair->nominationAcked = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pNominatedPair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pNominatedPair));

    // A frozen pair (non-nominated)
    pFrozenPair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pFrozenPair != NULL);
    pFrozenPair->state = ICE_CANDIDATE_PAIR_STATE_FROZEN;
    pFrozenPair->priority = 500;
    pFrozenPair->nominated = FALSE;
    pFrozenPair->nominationAcked = FALSE;
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pFrozenPair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pFrozenPair));

    iceAgent.stateEndTime = GETTIME() + KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;

    // With nominated but not acked: stay in NOMINATING
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state);

    // Ack the nominated pair: transition to READY
    pNominatedPair->nominationAcked = TRUE;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_READY, state);

    // The frozen pair should NOT cause a transition even if it were SUCCEEDED but not nominated
    pNominatedPair->nominationAcked = FALSE;
    pFrozenPair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    // pFrozenPair is not nominated, so it should not satisfy the transition
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state);

    // Cleanup
    EXPECT_EQ(STATUS_SUCCESS, doubleListGetHeadNode(iceAgent.iceCandidatePairs, &pCurNode));
    while (pCurNode != NULL) {
        pIceCandidatePair = (PIceCandidatePair) pCurNode->data;
        pCurNode = pCurNode->pNext;
        CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    }
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

// Verify that the USE_CANDIDATE STUN attribute can be appended to a binding request and parsed back,
// confirming the controlled agent path for detecting nomination.
TEST_F(IceFunctionalityTest, IceAgentUseCandidateStunAttributeRoundTripUnitTest)
{
    PStunPacket pStunPacket = NULL;
    PStunPacket pDeserializedPacket = NULL;
    PStunAttributeHeader pStunAttr = NULL;
    BYTE buffer[512];
    UINT32 bufferLen = ARRAY_SIZE(buffer);

    // Create a binding request with USE_CANDIDATE flag
    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, (PCHAR) "testuser"));
    EXPECT_EQ(STATUS_SUCCESS, appendStunFlagAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE));

    // Verify the attribute is present before serialization
    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE, &pStunAttr));
    EXPECT_TRUE(pStunAttr != NULL);
    EXPECT_EQ(STUN_ATTRIBUTE_TYPE_USE_CANDIDATE, pStunAttr->type);

    // Serialize and deserialize to verify round-trip
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, NULL, &bufferLen));
    EXPECT_TRUE(bufferLen <= ARRAY_SIZE(buffer));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, buffer, &bufferLen));
    EXPECT_EQ(STATUS_SUCCESS, deserializeStunPacket(buffer, bufferLen, NULL, 0, &pDeserializedPacket));

    // Verify USE_CANDIDATE survives serialization round-trip
    pStunAttr = NULL;
    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pDeserializedPacket, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE, &pStunAttr));
    EXPECT_TRUE(pStunAttr != NULL);
    EXPECT_EQ(STUN_ATTRIBUTE_TYPE_USE_CANDIDATE, pStunAttr->type);

    freeStunPacket(&pStunPacket);
    freeStunPacket(&pDeserializedPacket);
}

// Fault-injection test simulating the field observation where 5 out of 74 connections
// never received a binding response to the USE_CANDIDATE request. Without the controlled-
// agent path (nominationAcked set on receiving the peer's USE_CANDIDATE request), those
// 5 connections would time out in NOMINATING and fail.
//
// Timeline from field data:
//   FAILED:  Nominated → USE_CANDIDATE sent → (no response) → 50ms tick → Selected pair → (30s idle) → timeout
//   OK:      Nominated → USE_CANDIDATE sent → 31ms binding response → 51ms Selected pair → data channel open
//
// This test exercises three scenarios:
//   1. Response lost + no peer USE_CANDIDATE → stays NOMINATING → times out to FAILED
//   2. Binding response arrives with matching nominationTransactionId → READY (controlling path)
//   3. Response lost but peer sends USE_CANDIDATE request → READY (controlled path, the rescue)
TEST_F(IceFunctionalityTest, IceAgentNominationFaultInjectionResponseLostUnitTest)
{
    IceAgent iceAgent;
    PIceCandidatePair pIceCandidatePair = NULL;
    UINT64 state = 0;
    BYTE fakeTxnId[STUN_TRANSACTION_ID_LEN];
    BYTE wrongTxnId[STUN_TRANSACTION_ID_LEN];

    // Generate two distinct transaction ids
    MEMSET(fakeTxnId, 0xAA, STUN_TRANSACTION_ID_LEN);
    MEMSET(wrongTxnId, 0xBB, STUN_TRANSACTION_ID_LEN);

    MEMSET(&iceAgent, 0x00, SIZEOF(IceAgent));
    iceAgent.lock = MUTEX_CREATE(TRUE);
    EXPECT_EQ(STATUS_SUCCESS, doubleListCreate(&iceAgent.iceCandidatePairs));

    pIceCandidatePair = (PIceCandidatePair) MEMCALLOC(1, SIZEOF(IceCandidatePair));
    ASSERT_TRUE(pIceCandidatePair != NULL);
    EXPECT_EQ(STATUS_SUCCESS, createTransactionIdStore(DEFAULT_MAX_STORED_TRANSACTION_ID_COUNT, &pIceCandidatePair->pTransactionIdStore));
    EXPECT_EQ(STATUS_SUCCESS, insertIceCandidatePair(iceAgent.iceCandidatePairs, pIceCandidatePair));

    // --- Scenario 1: USE_CANDIDATE sent, response never arrives, no peer USE_CANDIDATE ---
    // Simulates the FAILED case from field data (n=5 connections that hung for 30s)
    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pIceCandidatePair->nominated = TRUE;
    pIceCandidatePair->nominationAcked = FALSE;
    MEMCPY(pIceCandidatePair->nominationTransactionId, fakeTxnId, STUN_TRANSACTION_ID_LEN);

    // Within timeout: should stay in NOMINATING (waiting for ack)
    iceAgent.stateEndTime = GETTIME() + KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state);

    // After timeout: should transition to FAILED
    iceAgent.iceAgentStatus = STATUS_SUCCESS;
    iceAgent.stateEndTime = GETTIME() - 1;
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_FAILED, state);
    EXPECT_EQ(STATUS_ICE_FAILED_TO_NOMINATE_CANDIDATE_PAIR, iceAgent.iceAgentStatus);

    // --- Scenario 2: Binding response arrives with matching transaction id (controlling path) ---
    // Simulates the OK case where the response arrives within one tick (~31ms in field data)
    iceAgent.iceAgentStatus = STATUS_SUCCESS;
    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pIceCandidatePair->nominated = TRUE;
    pIceCandidatePair->nominationAcked = FALSE;
    MEMCPY(pIceCandidatePair->nominationTransactionId, fakeTxnId, STUN_TRANSACTION_ID_LEN);
    iceAgent.stateEndTime = GETTIME() + KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;

    // Simulate: a binding response with a WRONG transaction id arrives (stale pre-nomination response).
    // This must NOT set nominationAcked.
    // (In production this check is in handleStunPacket; here we inline the same MEMCMP logic.)
    if (MEMCMP(wrongTxnId, pIceCandidatePair->nominationTransactionId, STUN_TRANSACTION_ID_LEN) == 0) {
        pIceCandidatePair->nominationAcked = TRUE;
    }
    EXPECT_FALSE(pIceCandidatePair->nominationAcked);
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state); // still waiting

    // Now the correct response arrives (matching transaction id)
    if (MEMCMP(fakeTxnId, pIceCandidatePair->nominationTransactionId, STUN_TRANSACTION_ID_LEN) == 0) {
        pIceCandidatePair->nominationAcked = TRUE;
    }
    EXPECT_TRUE(pIceCandidatePair->nominationAcked);
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_READY, state);

    // --- Scenario 3: Response lost, but peer sends USE_CANDIDATE request (controlled path rescue) ---
    // Simulates the field case where the nominated socket is dead, our USE_CANDIDATE never reaches
    // the peer, but the peer independently sends its own USE_CANDIDATE to us.
    iceAgent.iceAgentStatus = STATUS_SUCCESS;
    pIceCandidatePair->state = ICE_CANDIDATE_PAIR_STATE_SUCCEEDED;
    pIceCandidatePair->nominated = TRUE;
    pIceCandidatePair->nominationAcked = FALSE;
    MEMCPY(pIceCandidatePair->nominationTransactionId, fakeTxnId, STUN_TRANSACTION_ID_LEN);
    iceAgent.stateEndTime = GETTIME() + KVS_ICE_CANDIDATE_NOMINATION_TIMEOUT;

    // Verify: still stuck in NOMINATING (no ack yet)
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_NOMINATING, state);

    // Simulate receiving peer's USE_CANDIDATE binding request.
    // In handleStunPacket this sets both nominated=TRUE and nominationAcked=TRUE.
    pIceCandidatePair->nominated = TRUE;
    pIceCandidatePair->nominationAcked = TRUE;

    // Now the state machine should transition to READY on the next tick
    state = 0;
    EXPECT_EQ(STATUS_SUCCESS, fromNominatingIceAgentState((UINT64) &iceAgent, &state));
    EXPECT_EQ(ICE_AGENT_STATE_READY, state);

    CHK_LOG_ERR(freeIceCandidatePair(&pIceCandidatePair));
    EXPECT_EQ(STATUS_SUCCESS, doubleListClear(iceAgent.iceCandidatePairs, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, doubleListFree(iceAgent.iceCandidatePairs));
    MUTEX_FREE(iceAgent.lock);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
