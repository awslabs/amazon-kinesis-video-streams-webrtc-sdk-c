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
    EXPECT_NE(appendStunAddressAttribute(NULL, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, pAddress), STATUS_SUCCESS);
    EXPECT_NE(appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL), STATUS_SUCCESS);
    EXPECT_NE(appendStunAddressAttribute(NULL, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, NULL), STATUS_SUCCESS);

    EXPECT_NE(appendStunUsernameAttribute(NULL, (PCHAR) "abc"), STATUS_SUCCESS);
    EXPECT_NE(appendStunUsernameAttribute(pStunPacket, NULL), STATUS_SUCCESS);

    EXPECT_NE(appendStunPriorityAttribute(NULL, 0), STATUS_SUCCESS);
}

TEST_F(StunApiTest, appendStunChangeRequestAttributeTest)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    PStunPacket pStunPacket;
    PStunAttributeHeader pAttribute;
    EXPECT_EQ(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket), STATUS_SUCCESS);
    EXPECT_EQ(appendStunChangeRequestAttribute(pStunPacket, STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_CHANGE_IP | STUN_ATTRIBUTE_CHANGE_REQUEST_FLAG_CHANGE_PORT), STATUS_SUCCESS);
    pAttribute = (PStunAttributeHeader) pStunPacket->attributeList[pStunPacket->attributesCount - 1];
    EXPECT_EQ(pAttribute->type, STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST);
    EXPECT_EQ(freeStunPacket(&pStunPacket), STATUS_SUCCESS);
}

TEST_F(StunApiTest, appendStunErrorCodeAttributeTest)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    PStunPacket pStunPacket;
    PStunAttributeHeader pAttribute;
    CHAR errorPhrase[128] = "Sample phrase";
    CHAR password[256] = "test password";
    BYTE buffer[10000];
    UINT32 size = 0;
    BOOL found;

    EXPECT_EQ(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket), STATUS_SUCCESS);
    EXPECT_EQ(appendStunErrorCodeAttribute(pStunPacket, NULL, 12), STATUS_NULL_ARG);
    EXPECT_EQ(appendStunErrorCodeAttribute(pStunPacket, errorPhrase, 12), STATUS_SUCCESS);
    pAttribute = (PStunAttributeHeader) pStunPacket->attributeList[pStunPacket->attributesCount - 1];
    EXPECT_EQ(pAttribute->type, STUN_ATTRIBUTE_TYPE_ERROR_CODE);
    EXPECT_EQ(serializeStunPacket(pStunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, NULL, &size), STATUS_SUCCESS);
    EXPECT_EQ(serializeStunPacket(pStunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, buffer, &size), STATUS_SUCCESS);

    // Loop through the buffer to find the error phrase
    for(SIZE_T i = 0; i < SIZEOF(buffer) - STRLEN(errorPhrase); ++i) {
        if (STRNCMP(reinterpret_cast<PCHAR>(buffer + i), errorPhrase, STRLEN(errorPhrase)) == 0) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_EQ(freeStunPacket(&pStunPacket), STATUS_SUCCESS);
}

TEST_F(StunApiTest, appendStunDataAttributeTest)
{
    CHAR password[256];
    BYTE buffer[10000];
    UINT32 size = 0;
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    PStunPacket pStunPacket;
    PStunAttributeHeader pAttribute;
    BYTE data[128] = {0};
    STRCPY(password, "test password");
    EXPECT_EQ(createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket), STATUS_SUCCESS);
    EXPECT_EQ(appendStunDataAttribute(pStunPacket, NULL, 12), STATUS_NULL_ARG);
    EXPECT_EQ(appendStunDataAttribute(pStunPacket, data, 128), STATUS_SUCCESS);
    pAttribute = (PStunAttributeHeader) pStunPacket->attributeList[pStunPacket->attributesCount - 1];
    EXPECT_EQ(STUN_ATTRIBUTE_TYPE_DATA, pAttribute->type);

    EXPECT_EQ(serializeStunPacket(pStunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, NULL, &size), STATUS_SUCCESS);
    EXPECT_EQ(serializeStunPacket(pStunPacket, (PBYTE) password, STRLEN(password) * SIZEOF(CHAR), FALSE, FALSE, buffer, &size), STATUS_SUCCESS);

    EXPECT_EQ(freeStunPacket(&pStunPacket), STATUS_SUCCESS);
}

TEST_F(StunApiTest, getPackagedStunAttributeSizeTest)
{
    StunAttributeHeader attributeHeader;
    StunAttributeRealm realmAttribute;
    StunAttributeNonce nonceAttribute;
    StunAttributeData dataAttribute;
    StunAttributeUsername usernameAttribute;
    StunAttributeErrorCode errorAttribute;
    MEMSET(&attributeHeader, 0x00, SIZEOF(attributeHeader));
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_RESPONSE_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_SOURCE_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_REFLECTED_FROM;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_XOR_PEER_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_CHANGED_ADDRESS;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 32);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_USE_CANDIDATE;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_DONT_FRAGMENT;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_PRIORITY;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_LIFETIME;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_CHANGE_REQUEST;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_REQUESTED_TRANSPORT;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 16);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 16);
    realmAttribute.attribute.type = STUN_ATTRIBUTE_TYPE_REALM;
    realmAttribute.paddedLength = 20;
    EXPECT_EQ(getPackagedStunAttributeSize(reinterpret_cast<PStunAttributeHeader>(&realmAttribute)), 40);
    nonceAttribute.attribute.type = STUN_ATTRIBUTE_TYPE_NONCE;
    nonceAttribute.paddedLength = 20;
    EXPECT_EQ(getPackagedStunAttributeSize(reinterpret_cast<PStunAttributeHeader>(&nonceAttribute)), 40);
    dataAttribute.attribute.type = STUN_ATTRIBUTE_TYPE_DATA;
    dataAttribute.paddedLength = 20;
    EXPECT_EQ(getPackagedStunAttributeSize(reinterpret_cast<PStunAttributeHeader>(&dataAttribute)), 40);
    usernameAttribute.attribute.type = STUN_ATTRIBUTE_TYPE_USERNAME;
    usernameAttribute.paddedLength = 20;
    EXPECT_EQ(getPackagedStunAttributeSize(reinterpret_cast<PStunAttributeHeader>(&usernameAttribute)), 40);
    errorAttribute.attribute.type = STUN_ATTRIBUTE_TYPE_ERROR_CODE;
    errorAttribute.paddedLength = 20;
    EXPECT_EQ(getPackagedStunAttributeSize(reinterpret_cast<PStunAttributeHeader>(&errorAttribute)), 40);
    attributeHeader.type = STUN_ATTRIBUTE_TYPE_CHANNEL_NUMBER;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
    attributeHeader.type = (STUN_ATTRIBUTE_TYPE) 0xFFFF;
    attributeHeader.length = 0;
    EXPECT_EQ(getPackagedStunAttributeSize(&attributeHeader), 8);
}

TEST_F(StunApiTest, convertStunErrorCodeTest)
{
    StunResult_t stunResult;

    stunResult = STUN_RESULT_OK;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_SUCCESS);
    stunResult = STUN_RESULT_BAD_PARAM;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_INVALID_ARG);
    stunResult = STUN_RESULT_OUT_OF_MEMORY;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_NOT_ENOUGH_MEMORY);
    stunResult = STUN_RESULT_INVALID_MESSAGE_LENGTH;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_INVALID_MESSAGE_LENGTH);
    stunResult = STUN_RESULT_MAGIC_COOKIE_MISMATCH;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_MAGIC_COOKIE_MISMATCH);
    stunResult = STUN_RESULT_INVALID_ATTRIBUTE_LENGTH;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_INVALID_ATTRIBUTE_LENGTH);
    stunResult = STUN_RESULT_NO_MORE_ATTRIBUTE_FOUND;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_NO_MORE_ATTRIBUTE_FOUND);
    stunResult = STUN_RESULT_NO_ATTRIBUTE_FOUND;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_ATTRIBUTE_NOT_FOUND);

    stunResult = STUN_RESULT_BASE;
    EXPECT_EQ(convertStunErrorCode(stunResult), STATUS_STUN_UNKNOWN_ERROR);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
