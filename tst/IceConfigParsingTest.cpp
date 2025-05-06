
#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IceConfigParsingTest : public WebRtcClientTestBase {};

TEST_F(IceConfigParsingTest, ParseSuccess)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount;

    const std::string mockResponse = R"({
        "IceServerList": [
            {
                "Username": "testUser1",
                "Password": "testPass1",
                "Ttl": 300,
                "Uris": [
                    "turn:example1.com:443?transport=udp",
                    "turn:example1.com:443?transport=tcp"
                ]
            },
            {
                "Username": "testUser2",
                "Password": "testPass2",
                "Ttl": 300,
                "Uris": [
                    "turn:example2.com:443?transport=udp"
                ]
            }
        ]
    })";
    UINT32 mockResponseLen = mockResponse.length();

    EXPECT_EQ(STATUS_SUCCESS, parseIceConfigResponse(mockResponse.c_str(), mockResponseLen, ARRAY_SIZE(iceConfigs), iceConfigs, &iceConfigCount));

    EXPECT_EQ(2, iceConfigCount);

    // First config
    EXPECT_STREQ("testUser1", iceConfigs[0].userName);
    EXPECT_STREQ("testPass1", iceConfigs[0].password);
    EXPECT_EQ(300 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[0].ttl);
    EXPECT_EQ(2, iceConfigs[0].uriCount);
    EXPECT_STREQ("turn:example1.com:443?transport=udp", iceConfigs[0].uris[0]);
    EXPECT_STREQ("turn:example1.com:443?transport=tcp", iceConfigs[0].uris[1]);

    // Second config
    EXPECT_STREQ("testUser2", iceConfigs[1].userName);
    EXPECT_STREQ("testPass2", iceConfigs[1].password);
    EXPECT_EQ(300 * HUNDREDS_OF_NANOS_IN_A_SECOND, iceConfigs[1].ttl);
    EXPECT_EQ(1, iceConfigs[1].uriCount);
    EXPECT_STREQ("turn:example2.com:443?transport=udp", iceConfigs[1].uris[0]);
}

TEST_F(IceConfigParsingTest, InvalidArgs)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount;

    const std::string emptyResponse = "";
    UINT32 emptyResponseLen = emptyResponse.length();

    const std::string mockResponse = R"({
        "IceServerList": [
            {
                "Username": "testUser1",
                "Password": "testPass1",
                "Ttl": 300,
                "Uris": [
                    "turn:example1.com:443?transport=udp"
                ]
            }
        ]
    })";
    UINT32 mockResponseLen = mockResponse.length();

    // NULL in a parameter
    EXPECT_EQ(STATUS_NULL_ARG, parseIceConfigResponse(NULL, mockResponseLen, MAX_ICE_CONFIG_COUNT, iceConfigs, &iceConfigCount));
    EXPECT_EQ(STATUS_NULL_ARG, parseIceConfigResponse(mockResponse.c_str(), mockResponseLen, MAX_ICE_CONFIG_COUNT, NULL, &iceConfigCount));
    EXPECT_EQ(STATUS_NULL_ARG, parseIceConfigResponse(mockResponse.c_str(), mockResponseLen, MAX_ICE_CONFIG_COUNT, iceConfigs, NULL));

    // Empty input string
    EXPECT_EQ(STATUS_INVALID_API_CALL_RETURN_JSON,
              parseIceConfigResponse(emptyResponse.c_str(), emptyResponseLen, MAX_ICE_CONFIG_COUNT, iceConfigs, &iceConfigCount));

    // 0 output parameter length
    EXPECT_EQ(STATUS_INVALID_ARG, parseIceConfigResponse(mockResponse.c_str(), mockResponseLen, 0, iceConfigs, &iceConfigCount));
}

TEST_F(IceConfigParsingTest, TooManyIceConfigsReturned)
{
    // Array size = 1
    IceConfigInfo iceConfigs[1];
    UINT32 iceConfigCount;

    // But there are 2 ICE configurations returned in the response
    const std::string mockResponse = R"({
        "IceServerList": [
            {
                "Username": "testUser1",
                "Password": "testPass1",
                "Ttl": 300,
                "Uris": [
                    "turn:example1.com:443?transport=udp",
                    "turn:example1.com:443?transport=tcp"
                ]
            },
            {
                "Username": "testUser2",
                "Password": "testPass2",
                "Ttl": 300,
                "Uris": [
                    "turn:example2.com:443?transport=udp"
                ]
            }
        ]
    })";
    UINT32 mockResponseLen = mockResponse.length();

    EXPECT_EQ(STATUS_SIGNALING_MAX_ICE_CONFIG_COUNT,
              parseIceConfigResponse(mockResponse.c_str(), mockResponseLen, ARRAY_SIZE(iceConfigs), iceConfigs, &iceConfigCount));
}

TEST_F(IceConfigParsingTest, TooManyUrisReturned)
{
    IceConfigInfo iceConfigs[MAX_ICE_CONFIG_COUNT];
    UINT32 iceConfigCount;

    // Create exactly MAX_ICE_CONFIG_URI_COUNT + 1 URIs to test the boundary
    std::string uris;
    for (UINT32 i = 0; i <= MAX_ICE_CONFIG_URI_COUNT; i++) {
        if (i > 0) {
            uris += ",\n                ";
        }
        uris += "\"turn:example" + std::to_string(i) + ".com:443?transport=udp\"";
    }

    const std::string mockResponse = R"({
        "IceServerList": [
            {
                "Username": "testUser1",
                "Password": "testPass1",
                "Ttl": 300,
                "Uris": [
                    )" +
        uris + R"(
                ]
            }
        ]
    })";

    // Verify the error is returned
    EXPECT_EQ(STATUS_SIGNALING_MAX_ICE_URI_COUNT,
              parseIceConfigResponse(mockResponse.c_str(), mockResponse.length(), ARRAY_SIZE(iceConfigs), iceConfigs, &iceConfigCount));

    // Optionally verify the state wasn't modified
    EXPECT_EQ(0, iceConfigCount);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
