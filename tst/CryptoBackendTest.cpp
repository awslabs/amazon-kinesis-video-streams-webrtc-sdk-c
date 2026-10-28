#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

// These tests check which crypto library the test binary actually linked and
// loaded at runtime. The compile-time defines (KVS_USE_AWS_LC, KVS_USE_OPENSSL)
// only say which headers were used. A build can still pick up a different
// libcrypto, for example a system OpenSSL found ahead of the AWS-LC one in the
// dependency prefix, or a stale AWS-LC left in the prefix by an earlier
// configure. These checks fail in that case.
//
// OpenSSL_version() is the common probe: it returns a string compiled into
// libcrypto itself, so it describes the library loaded at runtime rather than
// the headers.
class CryptoBackendTest : public ::testing::Test {};

#ifdef KVS_USE_AWS_LC

// The loaded libcrypto is AWS-LC, and it matches the headers the SDK was built
// with. awslc_api_version_num() exists only in AWS-LC, so a build that linked
// another libcrypto fails to link, or fails to load for a shared build.
TEST_F(CryptoBackendTest, linkedLibraryIsAwsLc)
{
    EXPECT_EQ((unsigned long) AWSLC_API_VERSION, awslc_api_version_num());

    const char* pVersion = OpenSSL_version(OPENSSL_VERSION);
    ASSERT_NE(nullptr, pVersion);
    EXPECT_NE(nullptr, STRSTR(pVersion, "AWS-LC")) << "Loaded libcrypto reports: " << pVersion;
}

// FIPS mode is fixed when AWS-LC is compiled. A FIPS build of the SDK must load
// a FIPS AWS-LC, and a non-FIPS build must not. The startup banner logs the same
// mismatch; this fails the build instead of only warning about it.
TEST_F(CryptoBackendTest, fipsModeMatchesBuild)
{
#ifdef KVS_USE_AWS_LC_FIPS
    EXPECT_EQ(1, FIPS_mode());
#else
    EXPECT_EQ(0, FIPS_mode());
#endif
}

// The SDK's own PQC group list is checked by scripts/verify-pqc.sh rather than
// here. Applying it needs SSL_CTX_* from libssl, which the test binary does not
// link (it links libcrypto only), and adding libssl to the test link line would
// change the build for every backend to serve one assertion.

#elif defined(KVS_USE_OPENSSL)

// The default OpenSSL build must not load AWS-LC. This catches a dependency
// prefix that still holds an AWS-LC libcrypto from an earlier build.
TEST_F(CryptoBackendTest, linkedLibraryIsNotAwsLc)
{
    const char* pVersion = OpenSSL_version(OPENSSL_VERSION);
    ASSERT_NE(nullptr, pVersion);
    EXPECT_EQ(nullptr, STRSTR(pVersion, "AWS-LC")) << "Loaded libcrypto reports: " << pVersion;
}

#endif

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
