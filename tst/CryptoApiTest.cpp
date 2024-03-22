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
    EXPECT_EQ(createRtcCertificate(NULL), STATUS_NULL_ARG);
    EXPECT_EQ(createRtcCertificate(&pRtcCertificate), STATUS_SUCCESS);
    EXPECT_TRUE(pRtcCertificate != NULL);
    EXPECT_EQ(freeRtcCertificate(NULL), STATUS_SUCCESS);
    EXPECT_EQ(freeRtcCertificate(pRtcCertificate), STATUS_SUCCESS);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
