#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class NatBehaviorTest : public WebRtcClientTestBase {
};

TEST_F(NatBehaviorTest, discoverNatBehaviorTest)
{
    PCHAR stunHostname = NULL;
    NAT_BEHAVIOR mappingBehavior = NAT_BEHAVIOR_NONE, filteringBehavior = NAT_BEHAVIOR_NONE;
    ASSERT_EQ(TRUE, mAccessKeyIdSet);
    stunHostname = (PCHAR) MEMALLOC((MAX_ICE_CONFIG_URI_LEN + 1) * SIZEOF(CHAR));
    SNPRINTF(stunHostname, MAX_ICE_CONFIG_URI_LEN + 1, KINESIS_VIDEO_STUN_URL, TEST_DEFAULT_REGION, TEST_DEFAULT_STUN_URL_POSTFIX);
    EXPECT_EQ(discoverNatBehavior(NULL, &mappingBehavior, &filteringBehavior, NULL, 0), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatBehavior(stunHostname, NULL, &filteringBehavior, NULL, 0), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatBehavior(stunHostname, &mappingBehavior, NULL, NULL, 0), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatBehavior(NULL, NULL, NULL, NULL, 0), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatBehavior(EMPTY_STRING, &mappingBehavior, &filteringBehavior, NULL, 0), STATUS_INVALID_ARG);
    EXPECT_EQ(discoverNatBehavior(stunHostname, &mappingBehavior, &filteringBehavior, NULL, 0), STATUS_SUCCESS);
    SAFE_MEMFREE(stunHostname);
}

TEST_F(NatBehaviorTest, getNatBehaviorStrApiTest)
{
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_NONE), NAT_BEHAVIOR_NONE_STR);
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT), NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT_STR);
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_NO_UDP_CONNECTIVITY), NAT_BEHAVIOR_NO_UDP_CONNECTIVITY_STR);
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_ENDPOINT_INDEPENDENT), NAT_BEHAVIOR_ENDPOINT_INDEPENDENT_STR);
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_ADDRESS_DEPENDENT), NAT_BEHAVIOR_ADDRESS_DEPENDENT_STR);
    EXPECT_STREQ(getNatBehaviorStr(NAT_BEHAVIOR_PORT_DEPENDENT), NAT_BEHAVIOR_PORT_DEPENDENT_STR);
}

TEST_F(NatBehaviorTest, executeNatTestApiTest)
{
    StunPacket bindingRequest;
    KvsIpAddress ipAddr;
    SocketConnection socketConnection;
    PStunPacket pTestResponse;
    NatTestData natTestData;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(executeNatTest(NULL, NULL, NULL, 0, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(executeNatTest(&bindingRequest, &ipAddr, NULL, 0, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(executeNatTest(&bindingRequest, &ipAddr, &socketConnection, 0, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_NE(executeNatTest(&bindingRequest, &ipAddr, &socketConnection, 0, &natTestData, &pTestResponse), STATUS_NULL_ARG);
}

TEST_F(NatBehaviorTest, discoverNatMappingBehaviorApiTest)
{
    IceServer stunServer;
    NatTestData natTestData;
    SocketConnection socketConnection;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(discoverNatMappingBehavior(NULL, NULL, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatMappingBehavior(&stunServer, NULL, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatMappingBehavior(&stunServer, &natTestData, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatMappingBehavior(&stunServer, &natTestData, &socketConnection, NULL), STATUS_NULL_ARG);
}

TEST_F(NatBehaviorTest, discoverNatFilteringBehaviorApiTest)
{
    IceServer stunServer;
    NatTestData natTestData;
    SocketConnection socketConnection;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(discoverNatFilteringBehavior(NULL, NULL, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatFilteringBehavior(&stunServer, NULL, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatFilteringBehavior(&stunServer, &natTestData, NULL, NULL), STATUS_NULL_ARG);
    EXPECT_EQ(discoverNatFilteringBehavior(&stunServer, &natTestData, &socketConnection, NULL), STATUS_NULL_ARG);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

