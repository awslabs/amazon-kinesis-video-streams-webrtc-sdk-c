#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class CryptoApiTest : public WebRtcClientTestBase {
};

TEST_F(CryptoApiTest, createRtcCertificateApiTest)
{
    PRtcCertificate pRtcCertificate = NULL;
    EXPECT_EQ(STATUS_NULL_ARG, createRtcCertificate(NULL));
    EXPECT_EQ(STATUS_SUCCESS, createRtcCertificate(&pRtcCertificate));
    EXPECT_TRUE(pRtcCertificate != NULL);
    EXPECT_EQ(STATUS_SUCCESS, freeRtcCertificate(NULL));
    EXPECT_EQ(STATUS_SUCCESS, freeRtcCertificate(pRtcCertificate));
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
