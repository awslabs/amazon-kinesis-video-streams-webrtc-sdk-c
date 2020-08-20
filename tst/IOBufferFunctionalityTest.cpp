#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class IOBufferFunctionalityTest : public WebRtcClientTestBase {
};

TEST_F(IOBufferFunctionalityTest, writeReadDataAndVerifyStartWithEmptyBuffer)
{
    PIOBuffer pIOBuffer;
    CHAR data[] = "this is a test";
    INT32 dataLen = ARRAY_SIZE(data);
    PBYTE pBuff = (PBYTE) MEMALLOC(dataLen);
    UINT32 readLen;
    EXPECT_TRUE(pBuff != NULL);

    EXPECT_EQ(STATUS_SUCCESS, createIOBuffer(0, &pIOBuffer));
    EXPECT_TRUE(pIOBuffer != NULL);
    EXPECT_EQ(NULL, pIOBuffer->raw);
    EXPECT_EQ(0, pIOBuffer->len);
    EXPECT_EQ(0, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, ioBufferWrite(pIOBuffer, (PBYTE) data, dataLen));
    EXPECT_EQ(dataLen, pIOBuffer->len);
    EXPECT_EQ(dataLen, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw, data, dataLen));

    EXPECT_EQ(STATUS_SUCCESS, ioBufferWrite(pIOBuffer, (PBYTE) data, dataLen));
    EXPECT_EQ(2 * dataLen, pIOBuffer->len);
    EXPECT_EQ(2 * dataLen, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw, data, dataLen));
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw + dataLen, data, dataLen));

    EXPECT_EQ(STATUS_SUCCESS, ioBufferRead(pIOBuffer, pBuff, dataLen, &readLen));
    EXPECT_EQ(dataLen, readLen);
    EXPECT_EQ(0, MEMCMP(pBuff, data, dataLen));
    EXPECT_EQ(2 * dataLen, pIOBuffer->len);
    EXPECT_EQ(2 * dataLen, pIOBuffer->cap);
    EXPECT_EQ(dataLen, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, ioBufferRead(pIOBuffer, pBuff, dataLen, &readLen));
    EXPECT_EQ(dataLen, readLen);
    EXPECT_EQ(0, MEMCMP(pBuff, data, dataLen));
    EXPECT_EQ(0, pIOBuffer->len);
    EXPECT_EQ(2 * dataLen, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, freeIOBuffer(&pIOBuffer));
    EXPECT_EQ(NULL, pIOBuffer);
    MEMFREE(pBuff);
}

TEST_F(IOBufferFunctionalityTest, writeReadDataAndVerifyStartWithPreinitializedBuffer)
{
    PIOBuffer pIOBuffer;
    CHAR data[] = "this is a test";
    INT32 dataLen = ARRAY_SIZE(data);
    PBYTE pBuff = (PBYTE) MEMALLOC(dataLen);
    INT32 initCap = 64;
    UINT32 readLen;
    EXPECT_TRUE(pBuff != NULL);

    EXPECT_EQ(STATUS_SUCCESS, createIOBuffer(initCap, &pIOBuffer));
    EXPECT_TRUE(pIOBuffer != NULL);
    EXPECT_TRUE(pIOBuffer->raw != NULL);
    EXPECT_EQ(0, pIOBuffer->len);
    EXPECT_EQ(initCap, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, ioBufferWrite(pIOBuffer, (PBYTE) data, dataLen));
    EXPECT_EQ(dataLen, pIOBuffer->len);
    EXPECT_EQ(initCap, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw, data, dataLen));

    EXPECT_EQ(STATUS_SUCCESS, ioBufferWrite(pIOBuffer, (PBYTE) data, dataLen));
    EXPECT_EQ(2 * dataLen, pIOBuffer->len);
    EXPECT_EQ(initCap, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw, data, dataLen));
    EXPECT_EQ(0, MEMCMP(pIOBuffer->raw + dataLen, data, dataLen));

    EXPECT_EQ(STATUS_SUCCESS, ioBufferRead(pIOBuffer, pBuff, dataLen, &readLen));
    EXPECT_EQ(dataLen, readLen);
    EXPECT_EQ(0, MEMCMP(pBuff, data, dataLen));
    EXPECT_EQ(2 * dataLen, pIOBuffer->len);
    EXPECT_EQ(initCap, pIOBuffer->cap);
    EXPECT_EQ(dataLen, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, ioBufferRead(pIOBuffer, pBuff, dataLen, &readLen));
    EXPECT_EQ(dataLen, readLen);
    EXPECT_EQ(0, MEMCMP(pBuff, data, dataLen));
    EXPECT_EQ(0, pIOBuffer->len);
    EXPECT_EQ(initCap, pIOBuffer->cap);
    EXPECT_EQ(0, pIOBuffer->off);

    EXPECT_EQ(STATUS_SUCCESS, freeIOBuffer(&pIOBuffer));
    EXPECT_EQ(NULL, pIOBuffer);
    MEMFREE(pBuff);
}

} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
