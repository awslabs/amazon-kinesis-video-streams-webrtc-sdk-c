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
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatBehavior(NULL, &mappingBehavior, &filteringBehavior, NULL, 0));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatBehavior(stunHostname, NULL, &filteringBehavior, NULL, 0));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatBehavior(stunHostname, &mappingBehavior, NULL, NULL, 0));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatBehavior(NULL, NULL, NULL, NULL, 0));
    EXPECT_EQ(STATUS_INVALID_ARG, discoverNatBehavior(EMPTY_STRING, &mappingBehavior, &filteringBehavior, NULL, 0));
    EXPECT_EQ(STATUS_SUCCESS, discoverNatBehavior(stunHostname, &mappingBehavior, &filteringBehavior, NULL, 0));
    SAFE_MEMFREE(stunHostname);
}

TEST_F(NatBehaviorTest, getNatBehaviorStrApiTest)
{
    EXPECT_STREQ(NAT_BEHAVIOR_NONE_STR, getNatBehaviorStr(NAT_BEHAVIOR_NONE));
    EXPECT_STREQ(NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT_STR, getNatBehaviorStr(NAT_BEHAVIOR_NOT_BEHIND_ANY_NAT));
    EXPECT_STREQ(NAT_BEHAVIOR_NO_UDP_CONNECTIVITY_STR, getNatBehaviorStr(NAT_BEHAVIOR_NO_UDP_CONNECTIVITY));
    EXPECT_STREQ(NAT_BEHAVIOR_ENDPOINT_INDEPENDENT_STR, getNatBehaviorStr(NAT_BEHAVIOR_ENDPOINT_INDEPENDENT));
    EXPECT_STREQ(NAT_BEHAVIOR_ADDRESS_DEPENDENT_STR, getNatBehaviorStr(NAT_BEHAVIOR_ADDRESS_DEPENDENT));
    EXPECT_STREQ(NAT_BEHAVIOR_PORT_DEPENDENT_STR, getNatBehaviorStr(NAT_BEHAVIOR_PORT_DEPENDENT));
}

TEST_F(NatBehaviorTest, executeNatTestApiTest)
{
    StunPacket bindingRequest;
    KvsIpAddress ipAddr;
    SocketConnection socketConnection;
    PStunPacket pTestResponse;
    NatTestData natTestData;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(STATUS_NULL_ARG, executeNatTest(NULL, NULL, NULL, 0, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, executeNatTest(&bindingRequest, &ipAddr, NULL, 0, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, executeNatTest(&bindingRequest, &ipAddr, &socketConnection, 0, NULL, NULL));
    EXPECT_NE(STATUS_NULL_ARG, executeNatTest(&bindingRequest, &ipAddr, &socketConnection, 0, &natTestData, &pTestResponse));
}

TEST_F(NatBehaviorTest, discoverNatMappingBehaviorApiTest)
{
    PIceServer pStunServer;
    NatTestData natTestData;
    SocketConnection socketConnection;
    NAT_BEHAVIOR mappingBehavior = NAT_BEHAVIOR_NONE;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatMappingBehavior(NULL, NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatMappingBehavior(pStunServer, NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatMappingBehavior(pStunServer, &natTestData, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatMappingBehavior(pStunServer, &natTestData, &socketConnection, NULL));
}

TEST_F(NatBehaviorTest, discoverNatFilteringBehaviorApiTest)
{
    PIceServer pStunServer;
    NatTestData natTestData;
    SocketConnection socketConnection;
    NAT_BEHAVIOR mappingBehavior = NAT_BEHAVIOR_NONE;
    MEMSET(&natTestData, 0x00, SIZEOF(NatTestData));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatFilteringBehavior(NULL, NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatFilteringBehavior(pStunServer, NULL, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatFilteringBehavior(pStunServer, &natTestData, NULL, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, discoverNatFilteringBehavior(pStunServer, &natTestData, &socketConnection, NULL));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com

