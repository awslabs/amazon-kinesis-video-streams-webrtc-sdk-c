#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class SrtpApiTest : public WebRtcClientTestBase {
};

auto DEFAULT_TEST_PROFILE = KVS_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80;
auto DEFAULT_TEST_PROFILE_AUTH_TAG_SIZE = 10;
BYTE SKEL_RTP_PACKET[17] = {0x80, 0x60, 0x69, 0x8f, 0xd9, 0xc2, 0x93, 0xda, 0x1c, 0x64, 0x27, 0x82, 0x98, 0x36, 0xbe, 0x88, 0x9e};

TEST_F(SrtpApiTest, encryptDecryptRtpPacketSuccess)
{
    // Setup keys  of arbitrary size
    BYTE test_key[30] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                         0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
    PSrtpSession pSrtpSession;
    INT32 len = SIZEOF(SKEL_RTP_PACKET);

    EXPECT_EQ(STATUS_SUCCESS, initSrtpSession(test_key, test_key, DEFAULT_TEST_PROFILE, &pSrtpSession));

    PBYTE rtpPacket = (PBYTE) MEMCALLOC(1, SIZEOF(SKEL_RTP_PACKET) + SRTP_MAX_TRAILER_LEN);
    MEMCPY(rtpPacket, SKEL_RTP_PACKET, SIZEOF(SKEL_RTP_PACKET));

    EXPECT_EQ(STATUS_SUCCESS, encryptRtpPacket(pSrtpSession, rtpPacket, &len));
    EXPECT_EQ(len, SIZEOF(SKEL_RTP_PACKET) + DEFAULT_TEST_PROFILE_AUTH_TAG_SIZE);

    EXPECT_EQ(STATUS_SUCCESS, decryptSrtpPacket(pSrtpSession, rtpPacket, &len));
    EXPECT_EQ(len, SIZEOF(SKEL_RTP_PACKET));

    MEMFREE(rtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, freeSrtpSession(&pSrtpSession));
    EXPECT_EQ(STATUS_SUCCESS, freeSrtpSession(&pSrtpSession));
}

TEST_F(SrtpApiTest, encryptDecryptKeyMisMatchFails)
{
    BYTE transmitKey[30] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                            0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};
    BYTE receiveKey[30] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                           0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1F}; /* different key*/

    INT32 len = SIZEOF(SKEL_RTP_PACKET);
    PSrtpSession pSrtpSession = NULL;

    EXPECT_EQ(STATUS_SUCCESS, initSrtpSession(receiveKey, transmitKey, DEFAULT_TEST_PROFILE, &pSrtpSession));

    PBYTE rtpPacket = (PBYTE) MEMCALLOC(1, SIZEOF(SKEL_RTP_PACKET) + SRTP_MAX_TRAILER_LEN);
    MEMCPY(rtpPacket, SKEL_RTP_PACKET, SIZEOF(SKEL_RTP_PACKET));

    EXPECT_EQ(STATUS_SUCCESS, encryptRtpPacket(pSrtpSession, rtpPacket, &len));
    EXPECT_EQ(len, SIZEOF(SKEL_RTP_PACKET) + DEFAULT_TEST_PROFILE_AUTH_TAG_SIZE);

    EXPECT_NE(STATUS_SUCCESS, decryptSrtpPacket(pSrtpSession, rtpPacket, &len));

    MEMFREE(rtpPacket);

    EXPECT_EQ(STATUS_SUCCESS, freeSrtpSession(&pSrtpSession));
    EXPECT_EQ(STATUS_SUCCESS, freeSrtpSession(&pSrtpSession));
}

TEST_F(SrtpApiTest, noSrtpKeyReturnsFailure)
{
    PBYTE transmitKey = NULL;
    BYTE receiveKey[30] = {0x00, 0x01, 0x02, 0x03, 0x05, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                           0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D};

    PSrtpSession pSrtpSession = NULL;

    EXPECT_NE(STATUS_SUCCESS, initSrtpSession(receiveKey, transmitKey, DEFAULT_TEST_PROFILE, &pSrtpSession));
    EXPECT_EQ(STATUS_SUCCESS, freeSrtpSession(&pSrtpSession));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
