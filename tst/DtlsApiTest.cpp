#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class DtlsApiTest : public WebRtcClientTestBase {
};

#ifdef KVS_USE_OPENSSL
TEST_F(DtlsApiTest, createCertificateAndKey_Returns_Success)
{
    X509* pCert = NULL;
    EVP_PKEY* pKey = NULL;

    EXPECT_EQ(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, &pCert, &pKey), STATUS_SUCCESS);
    EXPECT_NE(pCert, nullptr);
    EXPECT_NE(pKey, nullptr);

    EXPECT_EQ(freeCertificateAndKey(&pCert, &pKey), STATUS_SUCCESS);
    EXPECT_EQ(pCert, nullptr);
    EXPECT_EQ(pKey, nullptr);
}

TEST_F(DtlsApiTest, dtlsSessionIsInitFinished_Null_Check)
{
    PDtlsSession pClient = NULL;
    BOOL isDtlsConnected = FALSE;
    DtlsSessionCallbacks callbacks;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_NULL_ARG, dtlsSessionIsInitFinished(pClient, &isDtlsConnected));
    EXPECT_EQ(FALSE, isDtlsConnected);
    EXPECT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pClient));
    EXPECT_EQ(STATUS_NULL_ARG, dtlsSessionIsInitFinished(pClient, NULL));
    freeDtlsSession(&pClient);
    EXPECT_EQ(NULL, pClient);
    timerQueueFree(&timerQueueHandle);

}

TEST_F(DtlsApiTest, dtlsSessionCreated_RefCount)
{
    DtlsSessionCallbacks callbacks;
    PDtlsSession pClient = NULL;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pClient));
    EXPECT_EQ(0, pClient->objRefCount);
    freeDtlsSession(&pClient);
    EXPECT_EQ(NULL, pClient);
    timerQueueFree(&timerQueueHandle);
}

TEST_F(DtlsApiTest, dtlsProcessPacket_Api_Check)
{
    DtlsSessionCallbacks callbacks;
    PDtlsSession pClient = NULL;
    INT32 length;
    TIMER_QUEUE_HANDLE timerQueueHandle = INVALID_TIMER_QUEUE_HANDLE_VALUE;
    EXPECT_EQ(STATUS_NULL_ARG, dtlsSessionProcessPacket(pClient, NULL, &length));
    EXPECT_EQ(STATUS_SUCCESS, timerQueueCreate(&timerQueueHandle));
    EXPECT_EQ(STATUS_SUCCESS, createDtlsSession(&callbacks, timerQueueHandle, 0, FALSE, NULL, &pClient));
    EXPECT_EQ(STATUS_NULL_ARG, dtlsSessionProcessPacket(pClient, NULL, NULL));
    EXPECT_EQ(STATUS_SSL_PACKET_BEFORE_DTLS_READY, dtlsSessionProcessPacket(pClient, NULL, &length));
    freeDtlsSession(&pClient);
    timerQueueFree(&timerQueueHandle);
}

#elif KVS_USE_MBEDTLS
TEST_F(DtlsApiTest, createCertificateAndKey_Returns_Success)
{
    mbedtls_x509_crt cert;
    mbedtls_pk_context key;

    EXPECT_EQ(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, &cert, &key), STATUS_SUCCESS);
    EXPECT_NE(cert.raw.p, nullptr);
    EXPECT_NE(cert.raw.len, 0);
    EXPECT_NE(key.pk_ctx, nullptr);
    EXPECT_NE(key.pk_info, nullptr);

    EXPECT_EQ(freeCertificateAndKey(&cert, &key), STATUS_SUCCESS);
    EXPECT_EQ(cert.raw.p, nullptr);
    EXPECT_EQ(cert.raw.len, 0);
    EXPECT_EQ(key.pk_ctx, nullptr);
    EXPECT_EQ(key.pk_info, nullptr);
}
#endif

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
