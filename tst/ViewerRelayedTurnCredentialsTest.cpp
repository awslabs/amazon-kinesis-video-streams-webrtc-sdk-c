#include "WebRTCClientTestFixture.h"

// The parseViewerTurnCredentialPayload function lives in samples/common/Common.c.
// We declare it here as extern "C" and include the sample header for types/defines.
extern "C" {
#include "../samples/common/Samples.h"
STATUS parseViewerTurnCredentialPayload(PCHAR, UINT32, PIceConfigInfo, PUINT32);
}

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class ViewerRelayedTurnCredentialsTest : public WebRtcClientTestBase {};

// Test: parse a valid single-server payload
TEST_F(ViewerRelayedTurnCredentialsTest, parseSingleServer)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;

    const std::string payload = R"({"turnServers":[{"urls":["turn:1.2.3.4:443?transport=udp","turns:1.2.3.4:443?transport=tcp"],"username":"testUser","password":"testPass","ttl":300}]})";

    EXPECT_EQ(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));

    EXPECT_EQ(1u, iceConfigCount);
    EXPECT_EQ(2u, iceConfigs[0].uriCount);
    EXPECT_STREQ("turn:1.2.3.4:443?transport=udp", iceConfigs[0].uris[0]);
    EXPECT_STREQ("turns:1.2.3.4:443?transport=tcp", iceConfigs[0].uris[1]);
    EXPECT_STREQ("testUser", iceConfigs[0].userName);
    EXPECT_STREQ("testPass", iceConfigs[0].password);
    EXPECT_EQ(300 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[0].ttl);
}

// Test: parse multiple TURN servers
TEST_F(ViewerRelayedTurnCredentialsTest, parseMultipleServers)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;

    const std::string payload = R"({"turnServers":[
        {"urls":["turn:server1.com:443?transport=udp"],"username":"user1","password":"pass1","ttl":200},
        {"urls":["turn:server2.com:443?transport=tcp","turns:server2.com:443?transport=tcp"],"username":"user2","password":"pass2","ttl":400}
    ]})";

    EXPECT_EQ(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));

    EXPECT_EQ(2u, iceConfigCount);

    EXPECT_EQ(1u, iceConfigs[0].uriCount);
    EXPECT_STREQ("turn:server1.com:443?transport=udp", iceConfigs[0].uris[0]);
    EXPECT_STREQ("user1", iceConfigs[0].userName);
    EXPECT_STREQ("pass1", iceConfigs[0].password);
    EXPECT_EQ(200 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[0].ttl);

    EXPECT_EQ(2u, iceConfigs[1].uriCount);
    EXPECT_STREQ("turn:server2.com:443?transport=tcp", iceConfigs[1].uris[0]);
    EXPECT_STREQ("turns:server2.com:443?transport=tcp", iceConfigs[1].uris[1]);
    EXPECT_STREQ("user2", iceConfigs[1].userName);
    EXPECT_STREQ("pass2", iceConfigs[1].password);
    EXPECT_EQ(400 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[1].ttl);
}

// Test: null arguments return STATUS_NULL_ARG
TEST_F(ViewerRelayedTurnCredentialsTest, parseNullArgs)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;
    CHAR payload[] = "{\"turnServers\":[]}";

    EXPECT_EQ(STATUS_NULL_ARG, parseViewerTurnCredentialPayload(NULL, 10, iceConfigs, &iceConfigCount));
    EXPECT_EQ(STATUS_NULL_ARG, parseViewerTurnCredentialPayload(payload, 10, NULL, &iceConfigCount));
    EXPECT_EQ(STATUS_NULL_ARG, parseViewerTurnCredentialPayload(payload, 10, iceConfigs, NULL));
}

// Test: zero payload length returns STATUS_INVALID_ARG
TEST_F(ViewerRelayedTurnCredentialsTest, parseZeroLength)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;
    CHAR payload[] = "{\"turnServers\":[]}";

    EXPECT_EQ(STATUS_INVALID_ARG, parseViewerTurnCredentialPayload(payload, 0, iceConfigs, &iceConfigCount));
}

// Test: invalid JSON returns error
TEST_F(ViewerRelayedTurnCredentialsTest, parseInvalidJson)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;
    const std::string payload = "not valid json at all {{{";

    EXPECT_NE(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));
}

// Test: missing turnServers key returns error
TEST_F(ViewerRelayedTurnCredentialsTest, parseMissingTurnServersKey)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;
    const std::string payload = R"({"iceServers":[{"urls":["turn:x.com:443"],"username":"u","password":"p","ttl":300}]})";

    EXPECT_NE(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));
}

// Test: empty turnServers array parses successfully with count 0
TEST_F(ViewerRelayedTurnCredentialsTest, parseEmptyArray)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 99;
    const std::string payload = R"({"turnServers":[]})";

    EXPECT_EQ(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));
    EXPECT_EQ(0u, iceConfigCount);
}

// Test: payload with extra unknown fields is parsed successfully (forward compat)
TEST_F(ViewerRelayedTurnCredentialsTest, parseUnknownFieldsIgnored)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount = 0;

    const std::string payload = R"({"turnServers":[{"urls":["turn:a.com:443"],"username":"u","password":"p","ttl":100,"extraField":"ignored","nested":{"a":1}}]})";

    EXPECT_EQ(STATUS_SUCCESS,
              parseViewerTurnCredentialPayload((PCHAR) payload.c_str(), (UINT32) payload.length(), iceConfigs, &iceConfigCount));

    EXPECT_EQ(1u, iceConfigCount);
    EXPECT_STREQ("turn:a.com:443", iceConfigs[0].uris[0]);
    EXPECT_STREQ("u", iceConfigs[0].userName);
    EXPECT_STREQ("p", iceConfigs[0].password);
    EXPECT_EQ(100 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[0].ttl);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
