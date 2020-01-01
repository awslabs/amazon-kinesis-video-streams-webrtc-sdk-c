#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class DtlsApiTest : public WebRtcClientTestBase {
};

TEST_F(DtlsApiTest, createCertificateAndKey_Returns_Success)
{
    X509 *pCert = NULL;
    EVP_PKEY *pKey = NULL;

    EXPECT_EQ(createCertificateAndKey(GENERATED_CERTIFICATE_BITS, &pCert, &pKey), STATUS_SUCCESS);
    EXPECT_NE(pCert, nullptr);
    EXPECT_NE(pKey, nullptr);

    EXPECT_EQ(freeCertificateAndKey(&pCert, &pKey), STATUS_SUCCESS);
    EXPECT_EQ(pCert, nullptr);
    EXPECT_EQ(pKey, nullptr);
}

}
}
}
}
}
