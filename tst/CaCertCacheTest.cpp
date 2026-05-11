#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class CaCertCacheTest : public WebRtcClientTestBase {
};

TEST_F(CaCertCacheTest, initWithNullPath_ReturnsNullArg)
{
    EXPECT_EQ(STATUS_NULL_ARG, initCaCertCache(NULL));
}

TEST_F(CaCertCacheTest, initAndDeinit_Success)
{
    // Deinit first in case a previous test left it initialized
    deinitCaCertCache();

    EXPECT_EQ(STATUS_SUCCESS, initCaCertCache((PCHAR) mCaCertPath));

    PBYTE pBuf = NULL;
    UINT32 bufLen = 0;
    EXPECT_EQ(STATUS_SUCCESS, getCachedCaCert(&pBuf, &bufLen));
    EXPECT_TRUE(pBuf != NULL);
    EXPECT_TRUE(bufLen > 0);

    EXPECT_EQ(STATUS_SUCCESS, deinitCaCertCache());

    // After deinit, cache should be empty
    EXPECT_EQ(STATUS_SUCCESS, getCachedCaCert(&pBuf, &bufLen));
    EXPECT_EQ(NULL, pBuf);
    EXPECT_EQ(0, bufLen);
}

TEST_F(CaCertCacheTest, doubleInit_IsIdempotent)
{
    deinitCaCertCache();

    EXPECT_EQ(STATUS_SUCCESS, initCaCertCache((PCHAR) mCaCertPath));
    // Second init should succeed (no-op since already initialized)
    EXPECT_EQ(STATUS_SUCCESS, initCaCertCache((PCHAR) mCaCertPath));

    PBYTE pBuf = NULL;
    UINT32 bufLen = 0;
    EXPECT_EQ(STATUS_SUCCESS, getCachedCaCert(&pBuf, &bufLen));
    EXPECT_TRUE(pBuf != NULL);
    EXPECT_TRUE(bufLen > 0);

    EXPECT_EQ(STATUS_SUCCESS, deinitCaCertCache());
}

TEST_F(CaCertCacheTest, getCachedCaCert_NullArgs)
{
    UINT32 bufLen = 0;
    PBYTE pBuf = NULL;
    EXPECT_EQ(STATUS_NULL_ARG, getCachedCaCert(NULL, &bufLen));
    EXPECT_EQ(STATUS_NULL_ARG, getCachedCaCert(&pBuf, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, getCachedCaCert(NULL, NULL));
}

TEST_F(CaCertCacheTest, getCachedCaCert_WithoutInit_ReturnsNull)
{
    deinitCaCertCache();

    PBYTE pBuf = NULL;
    UINT32 bufLen = 0;
    EXPECT_EQ(STATUS_SUCCESS, getCachedCaCert(&pBuf, &bufLen));
    EXPECT_EQ(NULL, pBuf);
    EXPECT_EQ(0, bufLen);
}

TEST_F(CaCertCacheTest, cachedContentMatchesFile)
{
    deinitCaCertCache();
    EXPECT_EQ(STATUS_SUCCESS, initCaCertCache((PCHAR) mCaCertPath));

    PBYTE pCachedBuf = NULL;
    UINT32 cachedBufLen = 0;
    EXPECT_EQ(STATUS_SUCCESS, getCachedCaCert(&pCachedBuf, &cachedBufLen));

    // Read the file directly and compare
    UINT64 fileLen = 0;
    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) mCaCertPath, FALSE, NULL, &fileLen));
    PBYTE pFileBuf = (PBYTE) MEMCALLOC(1, fileLen + 1);
    EXPECT_TRUE(pFileBuf != NULL);
    EXPECT_EQ(STATUS_SUCCESS, readFile((PCHAR) mCaCertPath, FALSE, pFileBuf, &fileLen));

    EXPECT_EQ((UINT32) fileLen, cachedBufLen);
    EXPECT_EQ(0, MEMCMP(pCachedBuf, pFileBuf, cachedBufLen));

    SAFE_MEMFREE(pFileBuf);
    EXPECT_EQ(STATUS_SUCCESS, deinitCaCertCache());
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
