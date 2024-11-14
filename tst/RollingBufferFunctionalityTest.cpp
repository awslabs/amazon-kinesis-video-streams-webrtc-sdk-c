#include "WebRTCClientTestFixture.h"

namespace com {
namespace amazonaws {
namespace kinesis {
namespace video {
namespace webrtcclient {

class RollingBufferFunctionalityTest : public WebRtcClientTestBase {};

STATUS RollingBufferFunctionalityTestFreeBufferFunc(PUINT64 data)
{
    if (data == NULL) {
        return STATUS_NULL_ARG;
    }
    *data = 0;
    return STATUS_SUCCESS;
}

STATUS RollingBufferFunctionalityTestFreeHeapMemoryFunc(PUINT64 data)
{
    if (data != NULL) {
        PCHAR pChar = (PCHAR) *data;
        SAFE_MEMFREE(pChar);
    }
    return STATUS_SUCCESS;
}

TEST_F(RollingBufferFunctionalityTest, appendDataToBufferAndVerify)
{
    PRollingBuffer pRollingBuffer;
    UINT64 first = (UINT64) 1, second = (UINT64) 2, third = (UINT64) 3, fourth = (UINT64) 4;
    UINT64 index;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(2, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));
    EXPECT_EQ(0, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, first, &index));
    EXPECT_EQ(1, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(first, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(0, index);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, second, &index));
    EXPECT_EQ(2, pRollingBuffer->headIndex);
    EXPECT_EQ(0, pRollingBuffer->tailIndex);
    EXPECT_EQ(first, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(second, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(1, index);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, third, &index));
    EXPECT_EQ(3, pRollingBuffer->headIndex);
    EXPECT_EQ(1, pRollingBuffer->tailIndex);
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(second, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(2, index);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, fourth, &index));
    EXPECT_EQ(4, pRollingBuffer->headIndex);
    EXPECT_EQ(2, pRollingBuffer->tailIndex);
    EXPECT_EQ(third, pRollingBuffer->dataBuffer[0]);
    EXPECT_EQ(fourth, pRollingBuffer->dataBuffer[1]);
    EXPECT_EQ(3, index);
    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, insertDataToBufferAndVerify)
{
    PRollingBuffer pRollingBuffer;
    UINT64 first = (UINT64) 1, second = (UINT64) 2, third = (UINT64) 3, fourth = (UINT64) 4, fifth = (UINT64) 5;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));
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
    UINT64 index;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(3, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, first, &index));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, second, &index));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, third, &index));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, fourth, &index));

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

TEST_F(RollingBufferFunctionalityTest, RollingBufferIsEmptyInitially)
{
    PRollingBuffer pRollingBuffer;
    UINT32 size = 0;
    BOOL isEmpty;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(0, size);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsEmpty(pRollingBuffer, &isEmpty));
    EXPECT_TRUE(isEmpty);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferFreeIsIdempotentTest)
{
    PRollingBuffer pRollingBuffer;

    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, CreateRollingBufferNegativeTest)
{
    EXPECT_EQ(STATUS_INVALID_ARG, createRollingBuffer(0, RollingBufferFunctionalityTestFreeBufferFunc, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, NULL));
}

TEST_F(RollingBufferFunctionalityTest, FreeRollingBufferNegativeTest)
{
    EXPECT_EQ(STATUS_NULL_ARG, freeRollingBuffer(NULL));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferAppendDataNegativeTest)
{
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferAppendData(NULL, (UINT64) 1, NULL));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferInsertDataNegativeTest)
{
    PRollingBuffer pRollingBuffer;

    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferInsertData(NULL, (UINT64) 1, 1));

    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));
    EXPECT_EQ(STATUS_ROLLING_BUFFER_NOT_IN_RANGE, rollingBufferInsertData(pRollingBuffer, (UINT64) 100000, 2193));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferExtractDataNegativeTest)
{
    PRollingBuffer pRollingBuffer;
    UINT64 data;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferExtractData(pRollingBuffer, (UINT64) 1, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferExtractData(NULL, (UINT64) 1, &data));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferExtractData(NULL, (UINT64) 1, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferGetSizeNegativeTest)
{
    PRollingBuffer pRollingBuffer;
    UINT32 size;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferGetSize(pRollingBuffer, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferGetSize(NULL, &size));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferGetSize(NULL, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferIsEmptyNegativeTest)
{
    PRollingBuffer pRollingBuffer;
    BOOL isEmpty;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsEmpty(pRollingBuffer, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsEmpty(NULL, &isEmpty));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsEmpty(NULL, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RollingBufferIsIndexInRangeNegativeTest)
{
    PRollingBuffer pRollingBuffer;
    BOOL isInRange;
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsIndexInRange(pRollingBuffer, 1, NULL));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsIndexInRange(NULL, 1, &isInRange));
    EXPECT_EQ(STATUS_NULL_ARG, rollingBufferIsIndexInRange(NULL, 1, NULL));

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RangesAreCorrectInEmptyCase)
{
    PRollingBuffer pRollingBuffer;
    BOOL isInRange;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsIndexInRange(pRollingBuffer, 0, &isInRange));
    EXPECT_FALSE(isInRange);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsIndexInRange(pRollingBuffer, 1, &isInRange));
    EXPECT_FALSE(isInRange);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, 1000, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsIndexInRange(pRollingBuffer, 0, &isInRange));
    EXPECT_TRUE(isInRange);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsIndexInRange(pRollingBuffer, 1, &isInRange));
    EXPECT_FALSE(isInRange);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RangesAreCorrectInOverflowCase)
{
    PRollingBuffer pRollingBuffer;
    UINT32 capacity = 10, startingOffset = 3, i;
    BOOL isInRange;
    UINT64 data, index;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(capacity, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    // Fill the buffer up with elements
    pRollingBuffer->headIndex = UINT64_MAX - capacity - startingOffset;
    pRollingBuffer->tailIndex = pRollingBuffer->headIndex;
    for (i = 1; i <= capacity; i++) {
        EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, 100000, NULL));
        EXPECT_EQ(i, pRollingBuffer->size);
    }

    // Insert more elements to force headIndex and tailIndex to wrap around
    for (i = 1; i <= 6; i++) { // Adding 6 more elements will cause overflow
        EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, i, &index));

        // Note: head is always ahead by 1 since it points to next empty slot
        EXPECT_EQ(pRollingBuffer->headIndex, UINT64_MAX - startingOffset + i);
        EXPECT_EQ(capacity, pRollingBuffer->size);
    }

    // Valid range should be [4294967288 - 2)
    for (i = 0; i < 6; i++) { // Check that the indexes of those 6 elements are valid
        EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsIndexInRange(pRollingBuffer, UINT64_MAX - startingOffset + i, &isInRange));
        EXPECT_TRUE(isInRange);
    }

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, IndexesAreCorrectInOverflowCase)
{
    PRollingBuffer pRollingBuffer;
    UINT32 capacity = 10, startingOffset = 3;
    BOOL isEmpty;
    UINT64 data, index;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(capacity, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    // Starting point: 3 elements from the overflow
    pRollingBuffer->headIndex = UINT64_MAX - startingOffset;
    pRollingBuffer->tailIndex = pRollingBuffer->headIndex - capacity;
    pRollingBuffer->size = capacity;

    // Insert elements to force headIndex and tailIndex to wrap around
    for (UINT32 i = 1; i <= 6; i++) { // Adding 6 elements will cause overflow
        EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, i, &index));
        EXPECT_EQ(pRollingBuffer->headIndex, UINT64_MAX - startingOffset + i);
        EXPECT_EQ(capacity, pRollingBuffer->size);
    }

    // Retrieve the element at UINT64_MAX - 1
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, UINT64_MAX - 1, &data));
    EXPECT_EQ(startingOffset, data);

    // Retrieve the element at UINT64_MAX
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, UINT64_MAX, &data));
    EXPECT_EQ(startingOffset + 1, data);

    // Retrieve the element at 0
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 0, &data));
    EXPECT_EQ(startingOffset + 2, data);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, AddHeapElementNoMemoryLeak)
{
    PRollingBuffer pRollingBuffer;
    PCHAR testString = NULL;

    // Rolling buffer of size 4
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(4, RollingBufferFunctionalityTestFreeHeapMemoryFunc, &pRollingBuffer));

    // Allocate memory for the string
    testString = (PCHAR) MEMCALLOC(50, SIZEOF(CHAR));

    // Set the string value
    STRCPY(testString, "Test string for buffer");
    DLOGE("testString is: %s", testString);

    // Append the actual pointer to the buffer (not the address of the pointer)
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) testString, NULL));

    // Free the rolling buffer and ensure the free function is called
    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, AddElementsIsNoLongerEmpty)
{
    PRollingBuffer pRollingBuffer;
    UINT32 size = 0;
    BOOL isEmpty;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 1, NULL));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsEmpty(pRollingBuffer, &isEmpty));
    EXPECT_FALSE(isEmpty);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, AddThenRemoveElementIsEmpty)
{
    PRollingBuffer pRollingBuffer;
    UINT64 index, data;
    BOOL isEmpty;

    // Rolling buffer of size 10
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(10, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 1, &index));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, index, &data));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferIsEmpty(pRollingBuffer, &isEmpty));
    EXPECT_TRUE(isEmpty);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, SizeIsCorrect)
{
    PRollingBuffer pRollingBuffer;
    UINT64 data = 0;
    UINT32 size = 0;

    // Rolling buffer of size 5
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(5, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    EXPECT_EQ(0, rollingBufferGetSize(pRollingBuffer, &size));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 1, NULL));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(1, size);

    // Add 5 more elements. Element at index 0 (1) should be overwritten by new element at index 5 (6) during wraparound.
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 2, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 3, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 4, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 5, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 6, NULL));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(5, size);

    // Note: now the valid indexes range is 1 to 5 instead of 0 to 4.
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 5, &data));
    EXPECT_EQ(6, data);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 4, &data));
    EXPECT_EQ(5, data);

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(3, size);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RemoveItemTwiceSizeIsOk)
{
    PRollingBuffer pRollingBuffer;
    UINT64 data = 0;
    UINT32 size = 0;

    // Rolling buffer of size 5
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(5, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    // Add the item
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 1, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(1, size);

    // Remove the item
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 0, &data));
    EXPECT_EQ(1, data);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(0, size);

    // Request an item that no longer exists. The size should not be changed and go negative
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 0, &data));
    EXPECT_EQ(0, data);
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(0, size);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}

TEST_F(RollingBufferFunctionalityTest, RemoveItemOutOfRangeDoesntChangeSize)
{
    PRollingBuffer pRollingBuffer;
    UINT64 data = 0;
    UINT32 size = 0;

    // Rolling buffer of size 2
    EXPECT_EQ(STATUS_SUCCESS, createRollingBuffer(2, RollingBufferFunctionalityTestFreeBufferFunc, &pRollingBuffer));

    // Add the item
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 1, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 2, NULL));
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferAppendData(pRollingBuffer, (UINT64) 3, NULL));

    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(2, size);

    // Remove the item at index 0, which should be gone already
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferExtractData(pRollingBuffer, 0, &data));
    EXPECT_EQ(0, data);

    // Size should not change
    EXPECT_EQ(STATUS_SUCCESS, rollingBufferGetSize(pRollingBuffer, &size));
    EXPECT_EQ(2, size);

    EXPECT_EQ(STATUS_SUCCESS, freeRollingBuffer(&pRollingBuffer));
}
} // namespace webrtcclient
} // namespace video
} // namespace kinesis
} // namespace amazonaws
} // namespace com
