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

// ------------------------------- getIpAddrStr ----------------------

STATUS initTestKvsIpv4Address(PKvsIpAddress pKvsIpAddress)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT8 addr[] = {192, 168, 1, 1};

    CHK(pKvsIpAddress != NULL, STATUS_NULL_ARG);

    MEMSET(pKvsIpAddress, 0, SIZEOF(KvsIpAddress));
    pKvsIpAddress->family = KVS_IP_FAMILY_TYPE_IPV4;

    MEMCPY(pKvsIpAddress->address, addr, IPV4_ADDRESS_LENGTH);

CleanUp:
    return retStatus;
}

TEST_F(NetworkApiTest, GetIpAddrStrNullIpAddress)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    EXPECT_EQ(STATUS_NULL_ARG, getIpAddrStr(NULL, buffer, SIZEOF(buffer)));
}

TEST_F(NetworkApiTest, GetIpAddrStrInvalidBuffer)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));

    EXPECT_EQ(STATUS_INVALID_ARG, getIpAddrStr(&ipAddress, NULL, SIZEOF(buffer)));
    EXPECT_EQ(STATUS_INVALID_ARG, getIpAddrStr(&ipAddress, buffer, 0));
}

TEST_F(NetworkApiTest, GetIpAddrStrBufferTooSmall)
{
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));

    // Test with increasingly small buffers
    CHAR tinyBuffer[1];
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, getIpAddrStr(&ipAddress, tinyBuffer, SIZEOF(tinyBuffer)));

    CHAR smallBuffer[5];
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, getIpAddrStr(&ipAddress, smallBuffer, SIZEOF(smallBuffer)));
}

TEST_F(NetworkApiTest, GetIpAddrStrIpv4Addr)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));

    EXPECT_EQ(STATUS_SUCCESS, getIpAddrStr(&ipAddress, buffer, SIZEOF(buffer)));
    EXPECT_STREQ("192.168.1.1", buffer);
}

TEST_F(NetworkApiTest, GetIpAddrStrIpv6Addr)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];

    KvsIpAddress ipAddress;
    MEMSET(&ipAddress, 0, SIZEOF(KvsIpAddress));
    ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;

    // rfc3849 - 2001:db8::/32 as a documentation-only prefix in the IPv6
    //   address registry.  No end party is to be assigned this address.
    UINT8 addr[] = {0x20, 0x01,
                    0x0d, 0xb8,
                    0x12, 0x34,
                    0x56, 0x78,
                    0x9a, 0xbc,
                    0xde, 0xf0,
                    0x12, 0x34,
                    0x56, 0x78};
    MEMCPY(ipAddress.address, addr, IPV6_ADDRESS_LENGTH);

    EXPECT_EQ(STATUS_SUCCESS, getIpAddrStr(&ipAddress, buffer, SIZEOF(buffer)));
    EXPECT_STREQ("2001:0db8:1234:5678:9abc:def0:1234:5678", buffer);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
