#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class DtlsApiTest : public WebRtcClientTestBase {
};

#ifdef KVS_USE_OPENSSL
TEST_F(DtlsApiTest, createCertificateAndKey_Returns_Success)
{
    X509 *pCert = NULL;
    EVP_PKEY *pKey = NULL;

    EXPECT_EQ(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, FALSE, &pCert, &pKey), STATUS_SUCCESS);
    EXPECT_NE(pCert, nullptr);
    EXPECT_NE(pKey, nullptr);

    EXPECT_EQ(freeCertificateAndKey(&pCert, &pKey), STATUS_SUCCESS);
    EXPECT_EQ(pCert, nullptr);
    EXPECT_EQ(pKey, nullptr);
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

}
}
}
}
}
