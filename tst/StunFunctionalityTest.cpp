#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class StunFunctionalityTest : public WebRtcClientTestBase {
};

#define TEST_STUN_PASSWORD (PCHAR) "bf1f29259cea581c873248d4ae73b30f"

TEST_F(StunFunctionalityTest, basicValidParseTest)
{
    BYTE bindingRequestBytes[] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66,
                                  0x68, 0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d};

    BYTE bindingSuccessResponseXorMappedAddressBytes1[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66, 0x68,
                                                           0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d, 0x00, 0x20,
                                                           0x00, 0x08, 0x00, 0x01, 0x14, 0x00, 0x17, 0xe2, 0x60, 0xe9};

    BYTE bindingSuccessResponseXorMappedAddressBytes2[] = {
        0x01, 0x01, 0x00, 0x2c, 0x21, 0x12, 0xa4, 0x42, 0xc0, 0x63, 0xc0, 0x3b, 0xbe, 0x17, 0x7f, 0x5e, 0x22, 0x62, 0x42, 0x7c, 0x00, 0x20,
        0x00, 0x08, 0x00, 0x01, 0xd0, 0x11, 0x2b, 0x7d, 0x3a, 0x23, 0x00, 0x08, 0x00, 0x14, 0xc3, 0x9e, 0xc4, 0xb1, 0x7c, 0xbe, 0x48, 0x6c,
        0x02, 0x9f, 0x05, 0xbb, 0x7b, 0x83, 0xde, 0xc3, 0x5b, 0x0b, 0x7f, 0x53, 0x80, 0x28, 0x00, 0x04, 0xec, 0xf8, 0x14, 0x77};

    BYTE bindingSuccessResponseXorMappedAddressBytes3[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0xa7, 0xf1, 0xd9,
                                                           0x2a, 0x82, 0xc8, 0xd8, 0xfe, 0x43, 0x4d, 0x98, 0x55, 0x00, 0x20,
                                                           0x00, 0x08, 0x00, 0x01, 0x1b, 0xa9, 0x17, 0xe2, 0x60, 0xed};

    BYTE bindingSuccessResponseXorMappedAddressBytes4[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0xc4, 0xe2, 0xab, 0xd8, 0xdd, 0x26, 0x1b,
                                                           0xa4, 0x67, 0xb7, 0x4b, 0x2d, 0x00, 0x20, 0x00, 0x14, 0x00, 0x02, 0xc1, 0x6a, 0x07, 0x12,
                                                           0xb3, 0x42, 0xa2, 0x62, 0x96, 0x98, 0xec, 0xc2, 0x7e, 0xb0, 0x93, 0x24, 0x28, 0x14};

    BYTE bindingRequestUsernameBytes[] = {0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x21, 0x8d, 0x70, 0xf0, 0x9c, 0xcd, 0x89, 0x06,
                                          0x62, 0x25, 0x89, 0x97, 0x00, 0x06, 0x00, 0x11, 0x36, 0x61, 0x30, 0x35, 0x66, 0x38, 0x34, 0x38,
                                          0x3a, 0x38, 0x61, 0x63, 0x33, 0x65, 0x39, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04,
                                          0x7e, 0x7f, 0x00, 0xff, 0x80, 0x2a, 0x00, 0x08, 0x22, 0xf2, 0xa4, 0x44, 0x77, 0x68, 0x9b, 0x32,
                                          0x00, 0x08, 0x00, 0x14, 0xee, 0x55, 0x92, 0xb0, 0xde, 0x31, 0x89, 0x24, 0xa7, 0xef, 0xe5, 0xaf,
                                          0x2d, 0xbb, 0x84, 0x8e, 0xf0, 0xe6, 0xda, 0x26, 0x80, 0x28, 0x00, 0x04, 0x36, 0xbb, 0x52, 0x10};

    PStunPacket pStunPacket = NULL;
    PStunAttributeHeader pAttribute;
    PStunAttributeAddress pStunAttributeAddress = NULL;

    //
    // Binding request
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestBytes, SIZEOF(bindingRequestBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 0);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(pStunPacket->attributesCount, 0);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Binding success response xor mapped single
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes1, SIZEOF(bindingSuccessResponseXorMappedAddressBytes1),
                                    (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 12);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(pStunPacket->attributesCount, 1);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Binding success response xor mapped multiple
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes2, SIZEOF(bindingSuccessResponseXorMappedAddressBytes2),
                                    (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 44);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(pStunPacket->attributesCount, 3);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    EXPECT_EQ(pStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY);
    EXPECT_EQ(pStunPacket->attributeList[2]->type, STUN_ATTRIBUTE_TYPE_FINGERPRINT);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Binding success request user name multiple
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestUsernameBytes, SIZEOF(bindingRequestUsernameBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 76);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);

    // There are 2 other attributes which will be ignored
    EXPECT_EQ(pStunPacket->attributesCount, 5);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_USERNAME);
    EXPECT_EQ(STRNCMP(((PStunAttributeUsername) pStunPacket->attributeList[0])->userName, "6a05f848:8ac3e902", 17), 0);
    EXPECT_EQ(((PStunAttributeUsername) pStunPacket->attributeList[0])->paddedLength, 20);

    EXPECT_EQ(pStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_PRIORITY);
    EXPECT_EQ(((PStunAttributePriority) pStunPacket->attributeList[1])->priority, 0x7E7F00FF);

    EXPECT_EQ(pStunPacket->attributeList[3]->type, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY);
    EXPECT_EQ(pStunPacket->attributeList[4]->type, STUN_ATTRIBUTE_TYPE_FINGERPRINT);

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_FINGERPRINT, &pAttribute));
    EXPECT_TRUE(NULL != pAttribute);

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY, &pAttribute));
    EXPECT_TRUE(NULL != pAttribute);

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_PRIORITY, &pAttribute));
    EXPECT_TRUE(NULL != pAttribute);

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USERNAME, &pAttribute));
    EXPECT_TRUE(NULL != pAttribute);

    pAttribute = NULL;
    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &pAttribute));
    EXPECT_TRUE(NULL == pAttribute);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Binding success response xor mapped address for IPv4
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes3, SIZEOF(bindingSuccessResponseXorMappedAddressBytes3), NULL, 0,
                                    &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 12);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(pStunPacket->attributesCount, 1);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    pStunAttributeAddress = (PStunAttributeAddress) pStunPacket->attributeList[0];

    const BYTE ip4Addr[] = {0x36, 0xF0, 0xC4, 0xAF};
    const UINT16 ip4Port = 15035;
    EXPECT_EQ(0, MEMCMP(pStunAttributeAddress->address.address, ip4Addr, IPV4_ADDRESS_LENGTH));
    EXPECT_EQ(ip4Port, (UINT16) getInt16(pStunAttributeAddress->address.port));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);

    //
    // Binding success response xor mapped address for IPv6
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes4, SIZEOF(bindingSuccessResponseXorMappedAddressBytes4), NULL, 0,
                                    &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 12);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_RESPONSE_SUCCESS);
    EXPECT_EQ(pStunPacket->attributesCount, 1);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    pStunAttributeAddress = (PStunAttributeAddress) pStunPacket->attributeList[0];

    // 2600:1700:6680:3d40:31e4:6514:f493:6339
    const BYTE ip6Addr[] = {0x26, 0x00, 0x17, 0x00, 0x66, 0x80, 0x3d, 0x40, 0x31, 0xe4, 0x65, 0x14, 0xf4, 0x93, 0x63, 0x39};
    const UINT16 ip6Port = 57464;
    EXPECT_EQ(0, MEMCMP(pStunAttributeAddress->address.address, ip6Addr, IPV6_ADDRESS_LENGTH));
    EXPECT_EQ(ip6Port, (UINT16) getInt16(pStunAttributeAddress->address.port));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(NULL, pStunPacket);
}

TEST_F(StunFunctionalityTest, roundtripFidelityTest)
{
    PBYTE pBuffer = NULL;
    UINT32 size;

    BYTE bindingRequestUsernameBytes[] = {0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x21, 0x8d, 0x70, 0xf0, 0x9c, 0xcd, 0x89, 0x06,
                                          0x62, 0x25, 0x89, 0x97, 0x00, 0x06, 0x00, 0x11, 0x36, 0x61, 0x30, 0x35, 0x66, 0x38, 0x34, 0x38,
                                          0x3a, 0x38, 0x61, 0x63, 0x33, 0x65, 0x39, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04,
                                          0x7e, 0x7f, 0x00, 0xff, 0x80, 0x2a, 0x00, 0x08, 0x22, 0xf2, 0xa4, 0x44, 0x77, 0x68, 0x9b, 0x32,
                                          0x00, 0x08, 0x00, 0x14, 0xee, 0x55, 0x92, 0xb0, 0xde, 0x31, 0x89, 0x24, 0xa7, 0xef, 0xe5, 0xaf,
                                          0x2d, 0xbb, 0x84, 0x8e, 0xf0, 0xe6, 0xda, 0x26, 0x80, 0x28, 0x00, 0x04, 0x36, 0xbb, 0x52, 0x10};

    PStunPacket pStunPacket = NULL, pSerializedStunPacket = NULL;

    //
    // Binding success request user name multiple
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestUsernameBytes, SIZEOF(bindingRequestUsernameBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 76);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);

    // There are 2 other attributes which will be ignored
    EXPECT_EQ(pStunPacket->attributesCount, 5);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_USERNAME);
    EXPECT_EQ(pStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_PRIORITY);
    EXPECT_EQ(pStunPacket->attributeList[2]->type, STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING);
    EXPECT_EQ(pStunPacket->attributeList[3]->type, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY);
    EXPECT_EQ(pStunPacket->attributeList[4]->type, STUN_ATTRIBUTE_TYPE_FINGERPRINT);

    // Serialize it
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_TRUE(NULL != (pBuffer = (PBYTE) MEMALLOC(size)));
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // De-serialize it back again
    EXPECT_EQ(
        STATUS_SUCCESS,
        deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pSerializedStunPacket));

    EXPECT_EQ(pSerializedStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pSerializedStunPacket->header.messageLength, 76);
    EXPECT_EQ(pSerializedStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);

    EXPECT_EQ(pSerializedStunPacket->attributesCount, 5);
    EXPECT_EQ(pSerializedStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_USERNAME);
    EXPECT_EQ(pSerializedStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_PRIORITY);
    EXPECT_EQ(pSerializedStunPacket->attributeList[2]->type, STUN_ATTRIBUTE_TYPE_ICE_CONTROLLING);
    EXPECT_EQ(pSerializedStunPacket->attributeList[3]->type, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY);
    EXPECT_EQ(pSerializedStunPacket->attributeList[4]->type, STUN_ATTRIBUTE_TYPE_FINGERPRINT);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pSerializedStunPacket));
    SAFE_MEMFREE(pBuffer);
}

TEST_F(StunFunctionalityTest, appendAddressAfterParseTest)
{
    BYTE bindingRequestBytes[] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66,
                                  0x68, 0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d};

    BYTE bindingSuccessResponseXorMappedAddressBytes[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66, 0x68,
                                                          0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d, 0x00, 0x20,
                                                          0x00, 0x08, 0x00, 0x01, 0x14, 0x00, 0x17, 0xe2, 0x60, 0xe9};

    BYTE bindingRequestUsernameBytes[] = {0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x21, 0x8d, 0x70, 0xf0, 0x9c, 0xcd, 0x89, 0x06,
                                          0x62, 0x25, 0x89, 0x97, 0x00, 0x06, 0x00, 0x11, 0x36, 0x61, 0x30, 0x35, 0x66, 0x38, 0x34, 0x38,
                                          0x3a, 0x38, 0x61, 0x63, 0x33, 0x65, 0x39, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04,
                                          0x7e, 0x7f, 0x00, 0xff, 0x80, 0x2a, 0x00, 0x08, 0x22, 0xf2, 0xa4, 0x44, 0x77, 0x68, 0x9b, 0x32,
                                          0x00, 0x08, 0x00, 0x14, 0xee, 0x55, 0x92, 0xb0, 0xde, 0x31, 0x89, 0x24, 0xa7, 0xef, 0xe5, 0xaf,
                                          0x2d, 0xbb, 0x84, 0x8e, 0xf0, 0xe6, 0xda, 0x26, 0x80, 0x28, 0x00, 0x04, 0x36, 0xbb, 0x52, 0x10};

    PStunPacket pStunPacket = NULL;
    KvsIpAddress address;

    address.family = KVS_IP_FAMILY_TYPE_IPV4;

    //
    // No attributes
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestBytes, SIZEOF(bindingRequestBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should be out of memory
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Single attribute
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes, SIZEOF(bindingSuccessResponseXorMappedAddressBytes),
                                    (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should be out of memory
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Contains message integrity and fingerprint
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestUsernameBytes, SIZEOF(bindingRequestUsernameBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should fail as we are adding after integrity/fingerprint
    EXPECT_EQ(STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY,
              appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, appendUsernameAfterParseTest)
{
    PCHAR userName0 = (PCHAR) "abcd";
    PCHAR userName1 = (PCHAR) "abcde";
    PCHAR userName2 = (PCHAR) "abcdef";
    PCHAR userName3 = (PCHAR) "abcdefg";

    BYTE bindingRequestBytes[] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66,
                                  0x68, 0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d};

    BYTE bindingSuccessResponseXorMappedAddressBytes[] = {0x01, 0x01, 0x00, 0x0c, 0x21, 0x12, 0xa4, 0x42, 0x70, 0x66, 0x68,
                                                          0x6e, 0x70, 0x62, 0x50, 0x66, 0x41, 0x61, 0x6b, 0x4d, 0x00, 0x20,
                                                          0x00, 0x08, 0x00, 0x01, 0x14, 0x00, 0x17, 0xe2, 0x60, 0xe9};

    BYTE bindingRequestUsernameBytes[] = {0x00, 0x01, 0x00, 0x4c, 0x21, 0x12, 0xa4, 0x42, 0x21, 0x8d, 0x70, 0xf0, 0x9c, 0xcd, 0x89, 0x06,
                                          0x62, 0x25, 0x89, 0x97, 0x00, 0x06, 0x00, 0x11, 0x36, 0x61, 0x30, 0x35, 0x66, 0x38, 0x34, 0x38,
                                          0x3a, 0x38, 0x61, 0x63, 0x33, 0x65, 0x39, 0x30, 0x32, 0x00, 0x00, 0x00, 0x00, 0x24, 0x00, 0x04,
                                          0x7e, 0x7f, 0x00, 0xff, 0x80, 0x2a, 0x00, 0x08, 0x22, 0xf2, 0xa4, 0x44, 0x77, 0x68, 0x9b, 0x32,
                                          0x00, 0x08, 0x00, 0x14, 0xee, 0x55, 0x92, 0xb0, 0xde, 0x31, 0x89, 0x24, 0xa7, 0xef, 0xe5, 0xaf,
                                          0x2d, 0xbb, 0x84, 0x8e, 0xf0, 0xe6, 0xda, 0x26, 0x80, 0x28, 0x00, 0x04, 0x36, 0xbb, 0x52, 0x10};

    PStunPacket pStunPacket = NULL;

    //
    // No attributes
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestBytes, SIZEOF(bindingRequestBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should be out of memory
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName0));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName1));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName2));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName3));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Single attribute
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingSuccessResponseXorMappedAddressBytes, SIZEOF(bindingSuccessResponseXorMappedAddressBytes),
                                    (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should be out of memory
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName0));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName1));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName2));
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, userName3));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));

    //
    // Contains message integrity and fingerprint
    //
    EXPECT_EQ(STATUS_SUCCESS,
              deserializeStunPacket(bindingRequestUsernameBytes, SIZEOF(bindingRequestUsernameBytes), (PBYTE) TEST_STUN_PASSWORD,
                                    (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pStunPacket));

    // Attempt to add to it should fail as we are adding after integrity/fingerprint
    EXPECT_EQ(STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY, appendStunUsernameAttribute(pStunPacket, userName0));
    EXPECT_EQ(STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY, appendStunUsernameAttribute(pStunPacket, userName1));
    EXPECT_EQ(STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY, appendStunUsernameAttribute(pStunPacket, userName2));
    EXPECT_EQ(STATUS_STUN_ATTRIBUTES_AFTER_FINGERPRINT_MESSAGE_INTEGRITY, appendStunUsernameAttribute(pStunPacket, userName3));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, appendAddressAttributeMaxCountTest)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pStunPacket;
    UINT32 i;
    KvsIpAddress address;

    address.family = KVS_IP_FAMILY_TYPE_IPV4;

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));

    for (i = 0; i <= STUN_ATTRIBUTE_MAX_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address));
    }

    // Should fail with one more
    EXPECT_EQ(STATUS_STUN_MAX_ATTRIBUTE_COUNT, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_MAPPED_ADDRESS, &address));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, appendUsernameAttributeMaxCountTest)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pStunPacket;
    UINT32 i;
    PCHAR userName = (PCHAR) "abcde";

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));

    for (i = 0; i <= STUN_ATTRIBUTE_MAX_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, userName));
    }

    // Should fail with one more
    EXPECT_EQ(STATUS_STUN_MAX_ATTRIBUTE_COUNT, appendStunUsernameAttribute(pStunPacket, userName));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, appendUsernameAttributeMaxAllocationTest)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pStunPacket;
    UINT32 i;
    CHAR userName[70 + 1];
    CHAR longUserName[STUN_MAX_USERNAME_LEN + 1];

    MEMSET(longUserName, 'a', ARRAY_SIZE(longUserName));
    longUserName[ARRAY_SIZE(longUserName) - 1] = '\0';

    MEMSET(userName, 'a', ARRAY_SIZE(userName));
    userName[ARRAY_SIZE(userName) - 1] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));

    for (i = 0; i < STUN_ATTRIBUTE_MAX_COUNT; i++) {
        EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, userName));
    }

    // Should fail with a single alloc as we will be over the limit
    EXPECT_EQ(STATUS_NOT_ENOUGH_MEMORY, appendStunUsernameAttribute(pStunPacket, longUserName));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, attributeDetectionTest)
{
    PStunAttributeHeader pUsernameAttribute;
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pStunPacket;
    CHAR userName[70 + 1];

    MEMSET(userName, 'a', ARRAY_SIZE(userName));
    userName[ARRAY_SIZE(userName) - 1] = '\0';

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USERNAME, &pUsernameAttribute));
    EXPECT_TRUE(NULL == pUsernameAttribute);
    EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, userName));
    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USERNAME, &pUsernameAttribute));
    EXPECT_TRUE(NULL != pUsernameAttribute);
    EXPECT_TRUE(pUsernameAttribute->type == STUN_ATTRIBUTE_TYPE_USERNAME);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, roundtripAfterCreateAddFidelityTest)
{
    PBYTE pBuffer = NULL;
    UINT32 size;
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    KvsIpAddress address;

    address.family = KVS_IP_FAMILY_TYPE_IPV4;
    address.port = (UINT16) getInt16(12345);
    MEMCPY(address.address, (PBYTE) "0123456789abcdef", IPV6_ADDRESS_LENGTH);

    MEMCPY(transactionId, (PBYTE) "ABCDEFGHIJKL", STUN_TRANSACTION_ID_LEN);

    PStunPacket pStunPacket = NULL, pSerializedStunPacket = NULL;

    //
    // Create STUN packet and add various attributes
    //
    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(pStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pStunPacket->header.messageLength, 0);
    EXPECT_EQ(pStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);
    EXPECT_EQ(pStunPacket->allocationSize, STUN_PACKET_ALLOCATION_SIZE);
    EXPECT_EQ(pStunPacket->attributesCount, 0);

    EXPECT_EQ(STATUS_SUCCESS, appendStunPriorityAttribute(pStunPacket, 10));
    EXPECT_EQ(STATUS_SUCCESS, appendStunFlagAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE));
    EXPECT_EQ(STATUS_SUCCESS, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_REFLECTED_FROM, &address));
    EXPECT_EQ(STATUS_SUCCESS, appendStunUsernameAttribute(pStunPacket, (PCHAR) "abc"));

    EXPECT_EQ(pStunPacket->header.messageLength, 32);

    // Validate the attributes
    EXPECT_EQ(pStunPacket->attributesCount, 4);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_PRIORITY);
    EXPECT_EQ(10, ((PStunAttributePriority) pStunPacket->attributeList[0])->priority);
    EXPECT_EQ(pStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE);
    EXPECT_EQ(pStunPacket->attributeList[2]->type, STUN_ATTRIBUTE_TYPE_REFLECTED_FROM);
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV4, ((PStunAttributeAddress) pStunPacket->attributeList[2])->address.family);
    EXPECT_EQ((UINT16) getInt16(12345), ((PStunAttributeAddress) pStunPacket->attributeList[2])->address.port);
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pStunPacket->attributeList[2])->address.address, IPV6_ADDRESS_LENGTH));
    EXPECT_EQ(pStunPacket->attributeList[3]->type, STUN_ATTRIBUTE_TYPE_USERNAME);
    EXPECT_EQ(0, MEMCMP("abc", ((PStunAttributeUsername) pStunPacket->attributeList[3])->userName, 3));

    // Serialize it
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_TRUE(NULL != (pBuffer = (PBYTE) MEMALLOC(size)));
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // De-serialize it back again
    EXPECT_EQ(
        STATUS_SUCCESS,
        deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pSerializedStunPacket));

    EXPECT_EQ(pSerializedStunPacket->header.magicCookie, STUN_HEADER_MAGIC_COOKIE);
    EXPECT_EQ(pSerializedStunPacket->header.messageLength, 64);
    EXPECT_EQ(pSerializedStunPacket->header.stunMessageType, STUN_PACKET_TYPE_BINDING_REQUEST);

    // Validate the values
    EXPECT_EQ(pSerializedStunPacket->attributesCount, 6);
    EXPECT_EQ(pSerializedStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_PRIORITY);
    EXPECT_EQ(10, ((PStunAttributePriority) pSerializedStunPacket->attributeList[0])->priority);
    EXPECT_EQ(pSerializedStunPacket->attributeList[1]->type, STUN_ATTRIBUTE_TYPE_USE_CANDIDATE);
    EXPECT_EQ(pSerializedStunPacket->attributeList[2]->type, STUN_ATTRIBUTE_TYPE_REFLECTED_FROM);
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV4, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[2])->address.family);
    EXPECT_EQ(12345, (UINT16) getInt16(((PStunAttributeAddress) pSerializedStunPacket->attributeList[2])->address.port));
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[2])->address.address, IPV4_ADDRESS_LENGTH));
    EXPECT_EQ(pSerializedStunPacket->attributeList[3]->type, STUN_ATTRIBUTE_TYPE_USERNAME);
    EXPECT_EQ(0, MEMCMP("abc", ((PStunAttributeUsername) pSerializedStunPacket->attributeList[3])->userName, 3));
    EXPECT_EQ(pSerializedStunPacket->attributeList[4]->type, STUN_ATTRIBUTE_TYPE_MESSAGE_INTEGRITY);
    EXPECT_EQ(pSerializedStunPacket->attributeList[5]->type, STUN_ATTRIBUTE_TYPE_FINGERPRINT);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pSerializedStunPacket));
    SAFE_MEMFREE(pBuffer);
}

TEST_F(StunFunctionalityTest, serializeStunRequestWithNoAttribute)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN] = {0};
    PStunPacket pStunPacket;
    UINT32 stunPacketBufferSize = STUN_PACKET_ALLOCATION_SIZE, actualPacketSize = 0;
    BYTE stunPacketBuffer[STUN_PACKET_ALLOCATION_SIZE];

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, NULL, &actualPacketSize));
    EXPECT_TRUE(actualPacketSize < stunPacketBufferSize);
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, stunPacketBuffer, &actualPacketSize));

    EXPECT_EQ(actualPacketSize, STUN_HEADER_LEN);
    EXPECT_EQ((UINT16) STUN_PACKET_TYPE_BINDING_REQUEST, (UINT16) getInt16(*(PUINT16) stunPacketBuffer));
    EXPECT_TRUE(IS_STUN_PACKET(stunPacketBuffer));
    EXPECT_EQ(0, MEMCMP(stunPacketBuffer + STUN_HEADER_LEN - STUN_TRANSACTION_ID_LEN, transactionId, STUN_TRANSACTION_ID_LEN));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

TEST_F(StunFunctionalityTest, serializeDeserializeStunControlAttribute)
{
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    PStunPacket pStunPacket, pDeserializedPacket;
    UINT32 stunPacketBufferSize = STUN_PACKET_ALLOCATION_SIZE, actualPacketSize = 0;
    BYTE stunPacketBuffer[STUN_PACKET_ALLOCATION_SIZE];
    PStunAttributeHeader pStunAttributeHeader;
    PStunAttributeIceControl pStunAttributeIceControl;
    UINT64 magicValue = 123;

    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, appendStunIceControllAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED, magicValue));
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, NULL, &actualPacketSize));
    EXPECT_TRUE(actualPacketSize < stunPacketBufferSize);
    EXPECT_EQ(STATUS_SUCCESS, serializeStunPacket(pStunPacket, NULL, 0, FALSE, FALSE, stunPacketBuffer, &actualPacketSize));
    EXPECT_TRUE(IS_STUN_PACKET(stunPacketBuffer));

    EXPECT_EQ(STATUS_SUCCESS, deserializeStunPacket(stunPacketBuffer, actualPacketSize, NULL, 0, &pDeserializedPacket));
    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pDeserializedPacket, STUN_ATTRIBUTE_TYPE_ICE_CONTROLLED, &pStunAttributeHeader));
    EXPECT_TRUE(pStunAttributeHeader != NULL);
    pStunAttributeIceControl = (PStunAttributeIceControl) pStunAttributeHeader;
    EXPECT_EQ(pStunAttributeIceControl->tieBreaker, magicValue);

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pDeserializedPacket));
}

TEST_F(StunFunctionalityTest, serializeDeserializeXORAddress)
{
    PBYTE pBuffer = NULL;
    UINT32 size;
    BYTE transactionId[STUN_TRANSACTION_ID_LEN];
    KvsIpAddress address;

    address.family = KVS_IP_FAMILY_TYPE_IPV4;
    address.port = (UINT16) getInt16(12345);
    MEMCPY(address.address, (PBYTE) "0123456789abcdef", IPV6_ADDRESS_LENGTH);

    MEMCPY(transactionId, (PBYTE) "ABCDEFGHIJKL", STUN_TRANSACTION_ID_LEN);

    PStunPacket pStunPacket = NULL, pSerializedStunPacket = NULL;

    //
    // Create STUN packet and XOR MAPPED ADDRESS attribute with an IPv4 address
    //
    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address));
    // 12 bytes = ATTRIBUTE HEADER (4 bytes) + ADDRESS HEADER (4 bytes) + IPv4 (4 bytes)
    EXPECT_EQ(pStunPacket->header.messageLength, 12);

    // Validate the attribute
    EXPECT_EQ(pStunPacket->attributesCount, 1);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV4, ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.family);
    EXPECT_EQ((UINT16) getInt16(12345), ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.port);
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.address, IPV6_ADDRESS_LENGTH));

    // Serialize it
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_TRUE(NULL != (pBuffer = (PBYTE) MEMALLOC(size)));
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // De-serialize it back again
    EXPECT_EQ(
        STATUS_SUCCESS,
        deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pSerializedStunPacket));

    // Validate the values
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV4, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.family);
    EXPECT_EQ(12345, (UINT16) getInt16(((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.port));
    // Validate that the address should be the same after being XORed and reXORed
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.address, IPV4_ADDRESS_LENGTH));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pSerializedStunPacket));
    SAFE_MEMFREE(pBuffer);

    //
    // Create STUN packet and XOR MAPPED ADDRESS attribute with an IPv6 address
    //
    address.family = KVS_IP_FAMILY_TYPE_IPV6;
    EXPECT_EQ(STATUS_SUCCESS, createStunPacket(STUN_PACKET_TYPE_BINDING_REQUEST, transactionId, &pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, appendStunAddressAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS, &address));
    // 24 bytes = ATTRIBUTE HEADER (4 bytes) + ADDRESS HEADER (4 bytes) + IPv6 (16 bytes)
    EXPECT_EQ(pStunPacket->header.messageLength, 24);

    // Validate the attribute
    EXPECT_EQ(pStunPacket->attributesCount, 1);
    EXPECT_EQ(pStunPacket->attributeList[0]->type, STUN_ATTRIBUTE_TYPE_XOR_MAPPED_ADDRESS);
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV6, ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.family);
    EXPECT_EQ((UINT16) getInt16(12345), ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.port);
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pStunPacket->attributeList[0])->address.address, IPV6_ADDRESS_LENGTH));

    // Serialize it
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, NULL, &size));
    EXPECT_TRUE(NULL != (pBuffer = (PBYTE) MEMALLOC(size)));
    EXPECT_EQ(STATUS_SUCCESS,
              serializeStunPacket(pStunPacket, (PBYTE) TEST_STUN_PASSWORD, STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), TRUE, TRUE, pBuffer, &size));

    // De-serialize it back again
    EXPECT_EQ(
        STATUS_SUCCESS,
        deserializeStunPacket(pBuffer, size, (PBYTE) TEST_STUN_PASSWORD, (UINT32) STRLEN(TEST_STUN_PASSWORD) * SIZEOF(CHAR), &pSerializedStunPacket));

    // Validate the values
    EXPECT_EQ((UINT16) KVS_IP_FAMILY_TYPE_IPV6, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.family);
    EXPECT_EQ(12345, (UINT16) getInt16(((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.port));
    // Validate that the address should be the same after being XORed and reXORed
    EXPECT_EQ(0, MEMCMP(address.address, ((PStunAttributeAddress) pSerializedStunPacket->attributeList[0])->address.address, IPV6_ADDRESS_LENGTH));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pSerializedStunPacket));
    SAFE_MEMFREE(pBuffer);
}

TEST_F(StunFunctionalityTest, deserializeStunErrorCode)
{
    /**
     * Error Code is 403. Error phrase is "Forbidden IP" without null terminator
     */
    BYTE stunErrorBuffer[] = {0x01, 0x18, 0x00, 0x54, 0x21, 0x12, 0xa4, 0x42, 0xd8, 0x83, 0x1d, 0x97, 0x54, 0x68, 0xf1, 0xbc, 0xf2, 0xe3,
                              0x40, 0x96, 0x00, 0x09, 0x00, 0x10, 0x00, 0x00, 0x04, 0x03, 0x46, 0x6f, 0x72, 0x62, 0x69, 0x64, 0x64, 0x65,
                              0x6e, 0x20, 0x49, 0x50, 0x80, 0x22, 0x00, 0x1a, 0x43, 0x6f, 0x74, 0x75, 0x72, 0x6e, 0x2d, 0x34, 0x2e, 0x35,
                              0x2e, 0x31, 0x2e, 0x31, 0x20, 0x27, 0x64, 0x61, 0x6e, 0x20, 0x45, 0x69, 0x64, 0x65, 0x72, 0x27, 0x20, 0x27,
                              0x00, 0x08, 0x00, 0x14, 0xba, 0xd4, 0xef, 0xe4, 0x0c, 0xa8, 0x6c, 0x6e, 0xc6, 0x10, 0xf1, 0x48, 0xaa, 0xc6,
                              0x8f, 0xe9, 0xb6, 0x25, 0x58, 0xd6, 0x80, 0x28, 0x00, 0x04, 0x0d, 0xc7, 0xfe, 0x7a};
    BYTE turnKey[] = {0x69, 0xa8, 0xc7, 0xf7, 0x79, 0x72, 0x3c, 0x58, 0xae, 0xc4, 0xbd, 0xa3, 0x79, 0x1c, 0x02, 0xbd};
    PStunPacket pStunPacket = NULL;
    PStunAttributeErrorCode pStunAttributeErrorCode = NULL;
    PStunAttributeHeader pStunAttr = NULL;

    EXPECT_EQ(STATUS_SUCCESS, deserializeStunPacket(stunErrorBuffer, ARRAY_SIZE(stunErrorBuffer), turnKey, KVS_MD5_DIGEST_LENGTH, &pStunPacket));

    EXPECT_EQ(STATUS_SUCCESS, getStunAttribute(pStunPacket, STUN_ATTRIBUTE_TYPE_ERROR_CODE, &pStunAttr));
    EXPECT_TRUE(pStunAttr != NULL);
    pStunAttributeErrorCode = (PStunAttributeErrorCode) pStunAttr;

    EXPECT_EQ(pStunAttributeErrorCode->errorCode, 403);
    EXPECT_EQ(0, STRCMP(pStunAttributeErrorCode->errorPhrase, "Forbidden IP"));

    EXPECT_EQ(STATUS_SUCCESS, freeStunPacket(&pStunPacket));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
