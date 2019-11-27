#include "WebRTCClientTestFixture.h"

namespace com { namespace amazonaws { namespace kinesis { namespace video { namespace webrtcclient {

class RollingBufferFunctionalityTest : public WebRtcClientTestBase {
};

TEST_F(RollingBufferFunctionalityTest, appendDataToBufferAndVerify)
{
    PRollingBuffer pRollingBuffer;
    UINT64 first = (UINT64) 1, second = (UINT64) 2, third = (UINT64) 3, fourth = (UINT64) 4;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(2, NULL, &pRollingBuffer));
    EXPECT_EQ(0, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, first));
    EXPECT_EQ(1, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(first, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, second));
    EXPECT_EQ(2, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(first, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(second, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, third));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(second, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, fourth));
    EXPECT_EQ(4, pRollingBuffer->headIndex);
    EXPECT_EQ(2, pRollingBuffer->tailIndex);
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(fourth, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, insertDataToBufferAndVerify)
{
    PRollingBuffer pRollingBuffer;
    UINT64 first = (UINT64) 1, second = (UINT64) 2, third = (UINT64) 3, fourth = (UINT64) 4, fifth = (UINT64) 5;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, NULL, &pRollingBuffer));
    pRollingBuffer->headIndex = 3;
    pRollingBuffer->tailIndex = 1;
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferInsertData(pRollingBuffer, 1, first));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(first, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(STATUS_ROLLING_BUFFER_NOT_IN_RANGE, rollingBufferInsertData(pRollingBuffer, 0, second));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(NULL, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(STATUS_ROLLING_BUFFER_NOT_IN_RANGE, rollingBufferInsertData(pRollingBuffer, 3, third));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(NULL, pRollingBuffer->dataBuffer[3]);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferInsertData(pRollingBuffer, 2, fourth));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(fourth, pRollingBuffer->dataBuffer[2]);
    EXPECT_EQ(STATUS_ROLLING_BUFFER_NOT_IN_RANGE, rollingBufferInsertData(pRollingBuffer, 5, fifth));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(NULL, pRollingBuffer->dataBuffer[5]);
    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, extractDataFromBufferAndInsertBack)
{
    PRollingBuffer pRollingBuffer;
    UINT64 first = (UINT64) 1, second = (UINT64) 2, third = (UINT64) 3, fourth = (UINT64) 4;
    UINT64 data;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(3, NULL, &pRollingBuffer));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, first));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, second));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, third));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, fourth));

    EXPECT_EQ(4, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(fourth, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(second, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[2]);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 2, &data));
    EXPECT_EQ(third, data);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 2, &data));
    EXPECT_EQ(NULL, data);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferInsertData(pRollingBuffer, 2, third));
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[2]);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferInsertData(pRollingBuffer, 3, third));
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[0]);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}
}
}
}
}
}
