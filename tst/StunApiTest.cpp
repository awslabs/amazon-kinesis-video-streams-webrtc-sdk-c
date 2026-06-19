#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class StunApiTest : public WebRtcClientTestBase {
};

#define TEST_STUN_PASSWORD (PCHAR) "bf1f29259cea581c873248d4ae73b30f"

TEST_F(StunApiTest, serializeValidityTests)
{
    StunPacket stunPacket;
    CHAR password[256];
    PBYTE pBuffer = NULL;
    BYTE buffer[10000];
    UINT32 size = 0;

    STRCPY(password, "test password");

    MEMSET(&stunPacket, 0x0, SIZEOF(stunPacket));
    stunPacket.allocationSize = SIZEOF(StunPacket);
    stunPacket.header.stunMessageType = STUN_PACKET_TYPE_BINDING_REQUEST;
    stunPacket.header.magicCookie = STUN_HEADER_MAGIC_COOKIE;

    pBuffer = buffer;

    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, TRUE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, FALSE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, NULL, 0, TRUE, TRUE, NULL, &size));

    password[0] = '\0';
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, NULL, 0, TRUE, TRUE, NULL, &size));

    password[0] = 't';
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, NULL, NULL));

    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, NULL, 0, TRUE, TRUE, NULL, NULL));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(NULL, NULL, 0, TRUE, TRUE, pBuffer, NULL));

    // Valid
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // Valid
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, FALSE, NULL, &size));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, FALSE, pBuffer, &size));

    // Valid
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, TRUE, NULL, &size));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, TRUE, pBuffer, &size));

    // Valid
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, NULL, &size));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, pBuffer, &size));

    // Invalid - size
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, FALSE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // Invalid - size
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, TRUE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // Invalid - size
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // Invalid cookie
    stunPacket.header.magicCookie = 123;
    EXPECT_NE(STATUS_SUCCESS, serializeStunPacket(&stunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));
}

TEST_F(StunApiTest, deserializeValidityTests)
{
    BYTE stunPacket[] = {0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x21, 0x8d, 0x70, 0xf0, 0x9c, 0xcd, 0x89, 0x06, 0x62, 0x25, 0x89, 0x97,
                         0x00, 0x06, 0x00, 0x11, 0x36, 0x61, 0x30, 0x35, 0x66, 0x38, 0x34, 0x38, 0x3a, 0x38, 0x61, 0x63, 0x33, 0x65, 0x39, 0x30,
                         0x32, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04, 0x7e, 0x7f, 0x00, 0xff, 0x80, 0x2a, 0x00, 0x08, 0x22, 0xf2, 0xa4, 0x44,
                         0x77, 0x68, 0x9b, 0x32, 0x00, 0x08, 0x00, 0x14, 0xee, 0x55, 0x92, 0xb0, 0xde, 0x31, 0x89, 0x24, 0xa7, 0xef, 0xe5, 0xaf,
                         0x2d, 0xbb, 0x84, 0x8e, 0xf0, 0xe6, 0xda, 0x26, 0x80, 0x28, 0x00, 0x04, 0x36, 0xbb, 0x52, 0x10};

    PBYTE pBuffer = stunPacket;
    UINT32 size = SIZEOF(stunPacket);
    PStunPacket pStunPacket = NULL;
    CHAR password[256];
    password[0] = '\0';
    BYTE stunPacketBad[SIZEOF(stunPacket)];

    EXPECT_NE(STATUS_SUCCESS, deserializeStunPacket(NULL, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_NE(STATUS_SUCCESS, deserializeStunPacket(pBuffer, 0, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_NE(
        STATUS_SUCCESS,
        deserializeStunPacket(pBuffer, SIZEOF(StunHeader) - 1, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_NE(STATUS_SUCCESS, deserializeStunPacket(pBuffer, size, NULL, 0, &pStunPacket));
    EXPECT_NE(STATUS_SUCCESS, deserializeStunPacket(pBuffer, size, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_NE(STATUS_SUCCESS, deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), NULL));

    // Modify the integrity
    MEMCPY(stunPacketBad, stunPacket, SIZEOF(stunPacket));
    // Modify the last bit of HMAC
    stunPacketBad[SIZEOF(stunPacket) - 9] = 0x27;
    EXPECT_EQ(STATUS_STUN_MESSAGE_INTEGRITY_MISMATCH,
              deserializeStunPacket(stunPacketBad, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    MEMCPY(stunPacketBad, stunPacket, SIZEOF(stunPacket));
    // Modify the body bit
    stunPacketBad[25] = 0x37;
    EXPECT_EQ(STATUS_STUN_MESSAGE_INTEGRITY_MISMATCH,
              deserializeStunPacket(stunPacketBad, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Modify the fingerprint
    MEMCPY(stunPacketBad, stunPacket, SIZEOF(stunPacket));
    // Modify the last bit of fingerprint
    stunPacketBad[SIZEOF(stunPacket) - 1] = 0x11;
    EXPECT_EQ(STATUS_STUN_FINGERPRINT_MISMATCH,
              deserializeStunPacket(stunPacketBad, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // De-serialize and attempt to add to it
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_TRUE(NULL == pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunApiTest, deserializeAttributeLengthOverflowNoOverread)
{
    // 20-byte STUN header + one 4-byte attribute header = 24 bytes total.
    //   bytes 0-1 : message type (BINDING_REQUEST 0x0001)
    //   bytes 2-3 : message length = 4 (exactly the one attribute header we include, so the
    //               "bufferSize >= messageLength + STUN_HEADER_LEN" header check passes: 24 >= 4 + 20,
    //               and the attribute walk is entered)
    //   bytes 4-7 : magic cookie 0x2112A442
    //   bytes 8-19: transaction id
    //   bytes 20-21: attribute type = DATA (0x0013)
    //   bytes 22-23: attribute length = 0xFFCC (65484) -- lies; no value bytes actually follow
    const UINT32 packetSize = 24;
    PBYTE pHeapPacket = (PBYTE) MEMALLOC(packetSize);
    ASSERT_TRUE(pHeapPacket != NULL);

    BYTE stunDataOverflow[packetSize] = {0x00, 0x01, 0x00, 0x04, 0x21, 0x12, 0xa4, 0x42, 0x00, 0x11, 0x22, 0x33,
                                         0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0x00, 0x13, 0xff, 0xcc};
    MEMCPY(pHeapPacket, stunDataOverflow, packetSize);

    PStunPacket pStunPacket = NULL;

    // The sizing pass rejects the attribute because header + padded value extends past the
    // bytes actually received (the recv buffer).
    EXPECT_EQ(STATUS_STUN_ATTRIBUTE_LENGTH_EXCEEDED_BUFFER_SIZE,
              deserializeStunPacket(pHeapPacket, packetSize, NULL, 0, &pStunPacket));
    EXPECT_TRUE(pStunPacket == NULL);

    SAFE_MEMFREE(pHeapPacket);
}

TEST_F(StunApiTest, deserializeTrailingDataAttributeOverread)
{
    const UINT16 fillerValueLen = 200; // <= STUN_MAX_USERNAME_LEN (512), so the USERNAME filler is valid
    const UINT16 dataAttrLen = 65532;  // ROUND_UP(65532, 4) == 65532, the largest non-wrapping value

    // Layout: STUN header | USERNAME header | USERNAME value | DATA header   (DATA value is absent)
    const UINT16 messageLength = STUN_ATTRIBUTE_HEADER_LEN + fillerValueLen + STUN_ATTRIBUTE_HEADER_LEN;
    const UINT32 packetSize = STUN_HEADER_LEN + messageLength;
    const UINT32 dataHeaderOffset = STUN_HEADER_LEN + STUN_ATTRIBUTE_HEADER_LEN + fillerValueLen;

    PBYTE pHeapPacket = (PBYTE) MEMCALLOC(1, packetSize);
    ASSERT_TRUE(pHeapPacket != NULL);

    // STUN header
    putInt16((PINT16) (pHeapPacket + 0), STUN_PACKET_TYPE_BINDING_REQUEST);
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_TYPE_LEN), messageLength);
    putInt32((PINT32) (pHeapPacket + STUN_HEADER_TYPE_LEN + STUN_HEADER_DATA_LEN), STUN_HEADER_MAGIC_COOKIE);
    // transaction id (bytes 8-19) left as zeroes

    // USERNAME filler header at offset 20 (value bytes 24..223 are present and zeroed)
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_LEN), STUN_ATTRIBUTE_TYPE_USERNAME);
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_LEN + STUN_ATTRIBUTE_HEADER_TYPE_LEN), fillerValueLen);

    // Trailing DATA attribute header at the very end of the datagram; its value is NOT present
    putInt16((PINT16) (pHeapPacket + dataHeaderOffset), STUN_ATTRIBUTE_TYPE_DATA);
    putInt16((PINT16) (pHeapPacket + dataHeaderOffset + STUN_ATTRIBUTE_HEADER_TYPE_LEN), dataAttrLen);

    PStunPacket pStunPacket = NULL;

    // The sizing pass rejects the DATA attribute because its header + padded value
    // extends past the received buffer -- and it does so on the second attribute, after the
    // USERNAME attribute has already validated cleanly.
    EXPECT_EQ(STATUS_STUN_ATTRIBUTE_LENGTH_EXCEEDED_BUFFER_SIZE,
              deserializeStunPacket(pHeapPacket, packetSize, NULL, 0, &pStunPacket));
    EXPECT_TRUE(pStunPacket == NULL);

    SAFE_MEMFREE(pHeapPacket);
}

TEST_F(StunApiTest, deserializeAddressAttributeFamilyOverread)
{
    // 20-byte STUN header + one 4-byte attribute header = 24 bytes; no value bytes follow.
    const UINT16 messageLength = STUN_ATTRIBUTE_HEADER_LEN;    // 4 -> header check passes: 24 >= 4 + 20
    const UINT32 packetSize = STUN_HEADER_LEN + messageLength; // 24

    PBYTE pHeapPacket = (PBYTE) MEMCALLOC(1, packetSize);
    ASSERT_TRUE(pHeapPacket != NULL);

    // STUN header
    putInt16((PINT16) (pHeapPacket + 0), STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_TYPE_LEN), messageLength);
    putInt32((PINT32) (pHeapPacket + STUN_HEADER_TYPE_LEN + STUN_HEADER_DATA_LEN), STUN_HEADER_MAGIC_COOKIE);
    // transaction id (bytes 8-19) left as zeroes

    // Address-type attribute header at offset 20; declared length 0, so no family/port bytes present.
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_LEN), STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    putInt16((PINT16) (pHeapPacket + STUN_HEADER_LEN + STUN_ATTRIBUTE_HEADER_TYPE_LEN), 0);

    PStunPacket pStunPacket = NULL;

    // Should rejected because the family bytes fall outside the buffer.
    EXPECT_EQ(STATUS_STUN_ATTRIBUTE_LENGTH_EXCEEDED_BUFFER_SIZE,
              deserializeStunPacket(pHeapPacket, packetSize, NULL, 0, &pStunPacket));
    EXPECT_TRUE(pStunPacket == NULL);

    SAFE_MEMFREE(pHeapPacket);
}

TEST_F(StunApiTest, packageIpValidityTests)
{
    KvsIpAddress address;
    UINT32 size;
    BYTE buffer[256];
    StunHeader stunHeader;
    PCHAR ipAddress = (PCHAR) "0123456789abcdef";

    address.family = KVS_IP_FAMILY_TYPE_IPV4;
    address.port = 123;

    MEMCPY(address.address, ipAddress, IPV6_ADDRESS_LENGTH);

    EXPECT_NE(STATUS_SUCCESS, stunPackageIpAddr(NULL, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL, NULL, &size));
    EXPECT_NE(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, NULL, NULL));
    EXPECT_NE(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL, NULL, NULL));

    // V4
    address.family = KVS_IP_FAMILY_TYPE_IPV4;
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, NULL, &size));
    EXPECT_EQ(12, size);
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, buffer, &size));
    EXPECT_EQ(12, size);

    // Address shouldn't change
    EXPECT_EQ(0, MEMCMP(ipAddress, address.address, IPV6_ADDRESS_LENGTH));

    // V6
    address.family = KVS_IP_FAMILY_TYPE_IPV6;
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, NULL, &size));
    EXPECT_EQ(24, size);
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address, buffer, &size));
    EXPECT_EQ(24, size);
    EXPECT_EQ(0, MEMCMP(ipAddress, address.address, IPV6_ADDRESS_LENGTH));

    //
    // Same sizes for XOR mapped
    //

    // V4
    address.family = KVS_IP_FAMILY_TYPE_IPV4;
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address, NULL, &size));
    EXPECT_EQ(12, size);
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address, buffer, &size));
    EXPECT_EQ(12, size);
    EXPECT_EQ(0, MEMCMP(ipAddress, address.address, IPV6_ADDRESS_LENGTH));

    // V6
    address.family = KVS_IP_FAMILY_TYPE_IPV6;
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address, NULL, &size));
    EXPECT_EQ(24, size);
    EXPECT_EQ(STATUS_SUCCESS, stunPackageIpAddr(&stunHeader, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address, buffer, &size));
    EXPECT_EQ(24, size);
    EXPECT_EQ(0, MEMCMP(ipAddress, address.address, IPV6_ADDRESS_LENGTH));
}

TEST_F(StunApiTest, createStunPackageValidityTests)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    PStunPacket pStunPacket;

    EXPECT_NE(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, NULL));
    EXPECT_NE(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, NULL));

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_TRUE(NULL == pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(0, MEMCMP(pStunPacket->header.transactionId, transactionId, STUN_TRANSACTION_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    // Random transaction id
    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, NULL, &pStunPacket));
    EXPECT_NE(0, MEMCMP(pStunPacket->header.transactionId, transactionId, STUN_TRANSACTION_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunApiTest, appendStunAttributeValidityTests)
{
    PStunPacket pStunPacket = (PStunPacket) 1;
    PKvsIpAddress pAddress = (PKvsIpAddress) 2;
    EXPECT_NE(STATUS_SUCCESS, appendStunAddressAttribute(NULL, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, pAddress));
    EXPECT_NE(STATUS_SUCCESS, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL));
    EXPECT_NE(STATUS_SUCCESS, appendStunAddressAttribute(NULL, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL));

    EXPECT_NE(STATUS_SUCCESS, appendStunUsernameAttribute(NULL, (PCHAR) "abc"));
    EXPECT_NE(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, NULL));

    EXPECT_NE(STATUS_SUCCESS, appendStunPriorityAttribute(NULL, 0));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
