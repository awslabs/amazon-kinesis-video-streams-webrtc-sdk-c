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
    DualKvsIpAddresses ipAddresses;
    EXPECT_EQ(STATUS_NULL_ARG, getIpWithHostName(NULL, &ipAddresses));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) "stun:stun.test.net:3478", &ipAddresses));
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "35-90-63-38.t-ae7dd61a.kinesisvideo.us-west-2.amazonaws.com", &ipAddresses));

    // Test dual-stack TURN server hostname parsing with the dual-stack envvar set.
    #ifdef _WIN32
        _putenv_s(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON");
    #else
        setenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "ON", 1);
    #endif
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "35-90-63-38_2001-0db8-85a3-0000-0000-8a2e-0370-7334.t-ae7dd61a.kinesisvideo.us-west-2.api.aws", &ipAddresses));
    #ifdef _WIN32
        _putenv_s(USE_DUAL_STACK_ENDPOINTS_ENV_VAR, "");
    #else
        unsetenv(USE_DUAL_STACK_ENDPOINTS_ENV_VAR);
    #endif

    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "12.34.45.40", &ipAddresses));
    EXPECT_EQ(STATUS_SUCCESS, getIpWithHostName((PCHAR) "2001:0db8:85a3:0000:0000:8a2e:0370:7334", &ipAddresses));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) ".12.34.45.40", &ipAddresses));
    EXPECT_EQ(STATUS_RESOLVE_HOSTNAME_FAILED, getIpWithHostName((PCHAR) "...........", &ipAddresses));
}

TEST_F(NetworkApiTest, ipIpAddrTest)
{
    EXPECT_EQ(TRUE, isIpAddr((PCHAR) "12.34.45.40", STRLEN("12.34.45.40")));
    EXPECT_EQ(TRUE, isIpAddr((PCHAR) "2001:0db8:85a3:0000:0000:8a2e:0370:7334", STRLEN("2001:0db8:85a3:0000:0000:8a2e:0370:7334")));

    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "stun:stun.test.net:3478", STRLEN("stun:stun.test.net:3478")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "567.123.345.000", STRLEN("567.123.345.000")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "2001:85a3:0000:0000:8a2e:0370:7334", STRLEN("2001:85a3:0000:0000:8a2e:0370:7334")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "2001:85a3:0000:0000:8a2e:0370:7334:7334:7334", STRLEN("2001:85a3:0000:0000:8a2e:0370:7334:7334:7334")));

    // Should fail if extra characters are present.
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "12.34.45.40.extraCharacters", STRLEN("12.34.45.40.extraCharacters")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "2001:85a3:0000:0000:8a2e:0370:7334:extraCharacters", STRLEN("2001:85a3:0000:0000:8a2e:0370:7334:extraCharacters")));

    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "12.34.45.40extraCharacters", STRLEN("12.34.45.40extraCharacters")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "2001:85a3:0000:0000:8a2e:0370:7334extraCharacters", STRLEN("2001:85a3:0000:0000:8a2e:0370:7334extraCharacters")));

    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "extraCharacters12.34.45.40", STRLEN("extraCharacters12.34.45.40")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "extraCharacters2001:85a3:0000:0000:8a2e:0370:7334", STRLEN("extraCharacters2001:85a3:0000:0000:8a2e:0370:7334")));

    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "extraCharacters.12.34.45.40", STRLEN("extraCharacters.12.34.45.40")));
    EXPECT_EQ(FALSE, isIpAddr((PCHAR) "extraCharacters.2001:85a3:0000:0000:8a2e:0370:7334", STRLEN("extraCharacters.2001:85a3:0000:0000:8a2e:0370:7334")));

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

    // Should be empty on error
    EXPECT_STREQ("", tinyBuffer);
    EXPECT_STREQ("", smallBuffer);
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

// ------------------------------- getIpAddrPortStr ----------------------

TEST_F(NetworkApiTest, GetIpAddrPortStrNullIpAddress)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    EXPECT_EQ(STATUS_NULL_ARG, getIpAddrPortStr(NULL, buffer, SIZEOF(buffer)));
}

TEST_F(NetworkApiTest, GetIpAddrPortStrInvalidBuffer)
{
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));
    ipAddress.port = htons(8080);

    EXPECT_EQ(STATUS_INVALID_ARG, getIpAddrPortStr(&ipAddress, NULL, SIZEOF(CHAR) * 32));
    CHAR buffer[32];
    EXPECT_EQ(STATUS_INVALID_ARG, getIpAddrPortStr(&ipAddress, buffer, 0));
}

TEST_F(NetworkApiTest, GetIpAddrPortStrBufferTooSmall)
{
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));
    ipAddress.port = htons(8080);

    CHAR tinyBuffer[10];
    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, getIpAddrPortStr(&ipAddress, tinyBuffer, SIZEOF(tinyBuffer)));
}

TEST_F(NetworkApiTest, GetIpAddrPortStrIpv4)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));
    ipAddress.port = htons(8080);

    EXPECT_EQ(STATUS_SUCCESS, getIpAddrPortStr(&ipAddress, buffer, SIZEOF(buffer)));
    EXPECT_STREQ("192.168.1.1:8080", buffer);
}

TEST_F(NetworkApiTest, GetIpAddrPortStrIpv4HighPort)
{
    CHAR buffer[KVS_IP_ADDRESS_STRING_BUFFER_LEN];
    KvsIpAddress ipAddress;
    EXPECT_EQ(STATUS_SUCCESS, initTestKvsIpv4Address(&ipAddress));
    ipAddress.port = htons(65535);

    EXPECT_EQ(STATUS_SUCCESS, getIpAddrPortStr(&ipAddress, buffer, SIZEOF(buffer)));
    EXPECT_STREQ("192.168.1.1:65535", buffer);
}

TEST_F(NetworkApiTest, GetIpAddrPortStrIpv6)
{
    CHAR buffer[KVS_IP_ADDRESS_PORT_STRING_BUFFER_LEN];
    KvsIpAddress ipAddress;
    MEMSET(&ipAddress, 0, SIZEOF(KvsIpAddress));
    ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    ipAddress.port = htons(443);

    UINT8 addr[] = {0x20, 0x01, 0x0d, 0xb8, 0x12, 0x34, 0x56, 0x78,
                    0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78};
    MEMCPY(ipAddress.address, addr, IPV6_ADDRESS_LENGTH);

    EXPECT_EQ(STATUS_SUCCESS, getIpAddrPortStr(&ipAddress, buffer, SIZEOF(buffer)));
    EXPECT_STREQ("[2001:0db8:1234:5678:9abc:def0:1234:5678]:443", buffer);
}

TEST_F(NetworkApiTest, GetIpAddrPortStrIpv6BufferTooSmallByOne)
{
    // "[2001:0db8:1234:5678:9abc:def0:1234:5678]:65535" is 48 long
    CHAR buffer[48 - 1];
    KvsIpAddress ipAddress;
    MEMSET(&ipAddress, 0, SIZEOF(KvsIpAddress));
    ipAddress.family = KVS_IP_FAMILY_TYPE_IPV6;
    ipAddress.port = htons(65535);

    UINT8 addr[] = {0x20, 0x01, 0x0d, 0xb8, 0x12, 0x34, 0x56, 0x78,
                    0x9a, 0xbc, 0xde, 0xf0, 0x12, 0x34, 0x56, 0x78};
    MEMCPY(ipAddress.address, addr, IPV6_ADDRESS_LENGTH);

    EXPECT_EQ(STATUS_BUFFER_TOO_SMALL, getIpAddrPortStr(&ipAddress, buffer, SIZEOF(buffer)));

    // Should be empty on error
    EXPECT_STREQ("", buffer);
}

// ------------------------------- IS_NON_ROUTABLE_ADDR ----------------------
// Unit tests for the non-globally-reachable address classifier used by
// turnConnectionAddPeer to skip TURN peers a public TURN server would 403.

static VOID setKvsV4Addr(PKvsIpAddress pAddr, UINT8 a, UINT8 b, UINT8 c, UINT8 d)
{
    MEMSET(pAddr, 0, SIZEOF(KvsIpAddress));
    pAddr->family = KVS_IP_FAMILY_TYPE_IPV4;
    pAddr->address[0] = a;
    pAddr->address[1] = b;
    pAddr->address[2] = c;
    pAddr->address[3] = d;
}

// Sets IPv6 bytes [0..3] and [15]; remaining bytes are zero. That is enough to
// exercise every range prefix (and the ::/::1 all-zero cases) the classifier checks.
static VOID setKvsV6Addr(PKvsIpAddress pAddr, UINT8 b0, UINT8 b1, UINT8 b2, UINT8 b3, UINT8 b15)
{
    MEMSET(pAddr, 0, SIZEOF(KvsIpAddress));
    pAddr->family = KVS_IP_FAMILY_TYPE_IPV6;
    pAddr->address[0] = b0;
    pAddr->address[1] = b1;
    pAddr->address[2] = b2;
    pAddr->address[3] = b3;
    pAddr->address[15] = b15;
}

TEST_F(NetworkApiTest, IsNonRoutableAddrNullAndUnsetFamily)
{
    KvsIpAddress addr;

    // NULL is safe and not classified as non-routable.
    EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(NULL));

    // Family neither IPv4 nor IPv6 -> not classified.
    MEMSET(&addr, 0, SIZEOF(KvsIpAddress));
    addr.family = KVS_IP_FAMILY_TYPE_NOT_SET;
    EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr));
}

TEST_F(NetworkApiTest, IsNonRoutableAddrIpv4NonRoutable)
{
    KvsIpAddress addr;

    setKvsV4Addr(&addr, 0, 1, 2, 3);         EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 0.0.0.0/8
    setKvsV4Addr(&addr, 10, 0, 0, 1);        EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 10.0.0.0/8
    setKvsV4Addr(&addr, 100, 64, 0, 1);      EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 100.64.0.0/10 low
    setKvsV4Addr(&addr, 100, 127, 255, 255); EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 100.64.0.0/10 high
    setKvsV4Addr(&addr, 127, 0, 0, 1);       EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 127.0.0.0/8 loopback
    setKvsV4Addr(&addr, 169, 254, 1, 1);     EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 169.254.0.0/16 link-local
    setKvsV4Addr(&addr, 172, 16, 0, 1);      EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 172.16.0.0/12 low
    setKvsV4Addr(&addr, 172, 31, 255, 255);  EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 172.16.0.0/12 high
    setKvsV4Addr(&addr, 192, 0, 0, 1);       EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 192.0.0.0/24
    setKvsV4Addr(&addr, 192, 0, 2, 5);       EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 192.0.2.0/24 TEST-NET-1
    setKvsV4Addr(&addr, 192, 88, 99, 1);     EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 192.88.99.0/24
    setKvsV4Addr(&addr, 192, 168, 1, 1);     EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 192.168.0.0/16
    setKvsV4Addr(&addr, 198, 18, 0, 1);      EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 198.18.0.0/15 low
    setKvsV4Addr(&addr, 198, 19, 255, 255);  EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 198.18.0.0/15 high
    setKvsV4Addr(&addr, 198, 51, 100, 7);    EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 198.51.100.0/24 TEST-NET-2
    setKvsV4Addr(&addr, 203, 0, 113, 7);     EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 203.0.113.0/24 TEST-NET-3
    setKvsV4Addr(&addr, 240, 0, 0, 1);       EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 240.0.0.0/4
    setKvsV4Addr(&addr, 255, 255, 255, 255); EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // limited broadcast
}

TEST_F(NetworkApiTest, IsNonRoutableAddrIpv4RoutableAndBoundaries)
{
    KvsIpAddress addr;

    setKvsV4Addr(&addr, 8, 8, 8, 8);         EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // public
    setKvsV4Addr(&addr, 1, 1, 1, 1);         EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // public
    setKvsV4Addr(&addr, 11, 112, 226, 120);  EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // public (seen in live test)
    setKvsV4Addr(&addr, 100, 63, 255, 255);  EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just below 100.64/10
    setKvsV4Addr(&addr, 100, 128, 0, 0);     EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just above 100.64/10
    setKvsV4Addr(&addr, 172, 15, 255, 255);  EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just below 172.16/12
    setKvsV4Addr(&addr, 172, 32, 0, 0);      EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just above 172.16/12
    setKvsV4Addr(&addr, 198, 17, 255, 255);  EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just below 198.18/15
    setKvsV4Addr(&addr, 198, 20, 0, 0);      EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just above 198.18/15
    setKvsV4Addr(&addr, 239, 255, 255, 255); EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // just below 240/4 (multicast, not filtered)
    setKvsV4Addr(&addr, 192, 1, 0, 1);       EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // 192.x but not a reserved /24 nor 192.168
    setKvsV4Addr(&addr, 203, 0, 114, 1);     EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // adjacent to TEST-NET-3
}

TEST_F(NetworkApiTest, IsNonRoutableAddrIpv6NonRoutable)
{
    KvsIpAddress addr;

    setKvsV6Addr(&addr, 0, 0, 0, 0, 0);            EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // ::/128 unspecified
    setKvsV6Addr(&addr, 0, 0, 0, 0, 1);            EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // ::1/128 loopback
    setKvsV6Addr(&addr, 0xfe, 0x80, 0, 0, 1);      EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // fe80::/10 low
    setKvsV6Addr(&addr, 0xfe, 0xbf, 0, 0, 1);      EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // fe80::/10 high
    setKvsV6Addr(&addr, 0xfc, 0, 0, 0, 1);         EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // fc00::/7 (fc00)
    setKvsV6Addr(&addr, 0xfd, 0, 0, 0, 1);         EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // fc00::/7 (fd00)
    setKvsV6Addr(&addr, 0x20, 0x01, 0x0d, 0xb8, 1); EXPECT_TRUE(IS_NON_ROUTABLE_ADDR(&addr)); // 2001:db8::/32 documentation
}

TEST_F(NetworkApiTest, IsNonRoutableAddrIpv6RoutableAndBoundaries)
{
    KvsIpAddress addr;

    setKvsV6Addr(&addr, 0x26, 0x00, 0x03, 0x82, 0x11); EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // 2600:382:: global (live test)
    setKvsV6Addr(&addr, 0x20, 0x01, 0x48, 0x60, 1);    EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // 2001:4860:: global (not db8)
    setKvsV6Addr(&addr, 0xfe, 0xc0, 0, 0, 1);          EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // fec0:: (outside fe80/10 and fc00/7)
    setKvsV6Addr(&addr, 0xfe, 0x7f, 0, 0, 1);          EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // fe7f:: (just below fe80/10)
    setKvsV6Addr(&addr, 0xfe, 0x00, 0, 0, 1);          EXPECT_FALSE(IS_NON_ROUTABLE_ADDR(&addr)); // fe00:: (not ULA, not link-local)
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
