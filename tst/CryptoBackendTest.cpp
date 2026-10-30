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

#ifdef KVS_USE_OPENSSL

// The loaded libcrypto is AWS-LC exactly when the SDK was built for AWS-LC.
// An AWS-LC build must load an AWS-LC that matches its headers. A default
// OpenSSL build must not load AWS-LC at runtime. CMake already refuses a
// dependency prefix that holds the wrong library at configure time; this
// checks the library the loader actually picked.
TEST_F(CryptoBackendTest, loadedLibraryMatchesBackend)
{
    const char* pVersion = OpenSSL_version(OPENSSL_VERSION);
    ASSERT_NE(nullptr, pVersion);
#ifdef KVS_USE_AWS_LC
    // awslc_api_version_num() exists only in AWS-LC, so a build that linked
    // another libcrypto fails to link, or fails to load for a shared build.
    EXPECT_EQ((unsigned long) AWSLC_API_VERSION, awslc_api_version_num());
    EXPECT_NE(nullptr, STRSTR(pVersion, "AWS-LC")) << "Loaded libcrypto reports: " << pVersion;
#else
    EXPECT_EQ(nullptr, STRSTR(pVersion, "AWS-LC")) << "Loaded libcrypto reports: " << pVersion;
#endif
}

#endif

#ifdef KVS_USE_AWS_LC

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

static STATUS captureTlsOutput(UINT64 customData, PBYTE pBuffer, UINT32 bufferLen)
{
    std::vector<BYTE>* pOut = (std::vector<BYTE>*) customData;
    pOut->insert(pOut->end(), pBuffer, pBuffer + bufferLen);
    return STATUS_SUCCESS;
}

// TURN over TLS uses the SDK's own TLS session. It sends no SNI, so
// scripts/check-pqc.sh cannot pick it out of a capture. Check the groups its
// ClientHello offers here instead.
TEST_F(CryptoBackendTest, turnsClientHelloOffersPqcGroups)
{
    std::vector<BYTE> out;
    std::vector<UINT16> groups, keyShares;
    TlsSessionCallbacks callbacks = {(UINT64) &out, captureTlsOutput, 0, NULL};
    PTlsSession pTlsSession = NULL;

    ASSERT_EQ(STATUS_SUCCESS, createTlsSession(&callbacks, &pTlsSession));
    ASSERT_EQ(STATUS_SUCCESS, tlsSessionStart(pTlsSession, FALSE));
    EXPECT_EQ(STATUS_SUCCESS, freeTlsSession(&pTlsSession));

    // RFC 8446 4.1.2: record header (5), handshake header (4), version (2) and
    // random (32), then session id, cipher suites, compression and extensions.
    auto u16 = [&](SIZE_T i) { return (UINT16) ((out[i] << 8) | out[i + 1]); };
    ASSERT_GT(out.size(), 43u);
    ASSERT_EQ(0x16, out[0]);
    ASSERT_GE(out.size(), 5 + (SIZE_T) u16(3));
    ASSERT_EQ(0x01, out[5]);
    SIZE_T off = 43;
    off += 1 + out[off];
    off += 2 + u16(off);
    off += 1 + out[off];
    SIZE_T end = off + 2 + u16(off);
    ASSERT_LE(end, out.size());
    for (off += 2; off + 4 <= end; off += 4 + u16(off + 2)) {
        SIZE_T d = off + 4;
        if (u16(off) == 0x000a) {
            for (SIZE_T i = 2; i < 2 + (SIZE_T) u16(d); i += 2) {
                groups.push_back(u16(d + i));
            }
        } else if (u16(off) == 0x0033) {
            for (SIZE_T i = 2; i < 2 + (SIZE_T) u16(d); i += 4 + u16(d + i + 2)) {
                keyShares.push_back(u16(d + i));
            }
        }
    }

    // X25519MLKEM768 first, then X25519, P-256 and P-384, with an ML-KEM key share.
    EXPECT_EQ(std::vector<UINT16>({0x11ec, 0x001d, 0x0017, 0x0018}), groups);
    ASSERT_FALSE(keyShares.empty());
    EXPECT_EQ(0x11ec, keyShares[0]);
}

#endif

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
