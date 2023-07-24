#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class NetworkApiTest : public WebRtcClientTestBase {
};

TEST_F(NetworkApiTest, GetIpWithHostNameTest)
{
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_NULL_ARG, getIpWithHostName(NULL, &ipAddress));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) "stun:stun.test.net:3478", &ipAddress));
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "35-90-63-38.t-ae7dd61a.kinesisvideo.us-west-2.amazonaws.com", &ipAddress));
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "12.34.45.40", &ipAddress));
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "2001:0db8:85a3:0000:0000:8a2e:0370:7334", &ipAddress));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) ".12.34.45.40", &ipAddress));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) "...........", &ipAddress));
}

TEST_F(NetworkApiTest, ipIpAddrTest)
{
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "stun:stun.test.net:3478", STRLEN("stun:stun.test.net:3478")));
    EXPECT_EQ(TRUE, isIpAddr((PCHAR) "12.34.45.40", STRLEN("12.34.45.40")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "567.123.345.000", STRLEN("567.123.345.000")));
    EXPECT_EQ(TRUE, isIpAddr((PCHAR) "2001:0db8:85a3:0000:0000:8a2e:0370:7334", STRLEN("2001:0db8:85a3:0000:0000:8a2e:0370:7334")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "2001:85a3:0000:0000:8a2e:0370:7334", STRLEN("2001:85a3:0000:0000:8a2e:0370:7334")));
}


} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
