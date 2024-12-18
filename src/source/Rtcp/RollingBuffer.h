/*******************************************
RTCP Rolling Buffer include file
*******************************************/
#ifndef __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_ROLLING_BUFFER_H
#define __KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_ROLLING_BUFFER_H

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef STATUS (*FreeDataFunc)(PUINT64);

/**
 * @file RollingBuffer.h
 *
 * @brief A circular buffer implementation that allows appending, inserting,
 *        extracting, and managing data in a fixed-size, thread-safe buffer.
 *        The buffer overwrites the oldest elements when it reaches capacity.
 */

/**
 * @struct RollingBuffer
 *
 * @brief Represents a circular buffer with a fixed capacity and provides thread-safe
 *        operations to append, insert, extract, and manage data.
 *
 * The buffer uses a pair of indices, `headIndex` and `tailIndex`, to track where data
 * is inserted and extracted. The buffer automatically overwrites the oldest data once
 * it reaches capacity, ensuring that the most recent elements (FIFO) are always retained.
 * In addition to FIFO, the buffer also contains operations to insert and extract elements
 * in non-head and tail locations.
 *
 * The `RollingBuffer` structure itself only stores UINT64, which can either be actual
 * UINT64's, or more commonly can be pointers to some dynamically allocated data. In the
 * latter case, the user can provide a freeDataFn to let the rolling buffer manage freeing
 * the memory.
 *
 * @note **Index Range**: The valid indexes in the buffer can be in the range of
 *       `[0, MAX_UINT64]`, **NOT** `[0, capacity)`. This is because the indices
 *       are designed to represent the logical order of elements in the buffer and
 *       do not follow the conventional circular array index range. Index overflow
 *       behavior is handled to support wrapping around.
 *
 * @see createRollingBuffer(), freeRollingBuffer(), rollingBufferAppendData(),
 *      rollingBufferInsertData(), rollingBufferExtractData(), rollingBufferGetSize(),
 *      rollingBufferIsEmpty(), rollingBufferIsIndexInRange()
 *
 * @warning This structure does not handle resizing dynamically. Once the buffer reaches
 *          its capacity, it will overwrite data unless data is extracted.
 *
 */
typedef struct {
    // Lock guarding the rolling buffer
    MUTEX lock;
    // Current number of elements in the buffer
    UINT32 size;
    // Max size of data buffer array
    UINT32 capacity;
    // Head index point to next empty slot to put data
    UINT64 headIndex;
    // Tail index point to oldest slot with data inside
    UINT64 tailIndex;
    // Buffer storing pointers, each pointer point to actual data
    PUINT64 dataBuffer;
    // Function being called when data pointer is removed from buffer
    FreeDataFunc freeDataFn;
} RollingBuffer, *PRollingBuffer;

#define ROLLING_BUFFER_MAP_INDEX(pRollingBuffer, index) ((index) % (pRollingBuffer)->capacity)

/**
 * @brief Creates a new rolling buffer with the specified capacity.
 *
 * Allocates and initializes a rolling buffer structure to hold data with the given capacity.
 * A custom free function for data can be provided, which will be called when data is overwritten or cleared.
 *
 * @param[in] capacity       The maximum number of elements the rolling buffer can hold.
 * @param[in] freeDataFunc   Pointer to a function for freeing data elements, or NULL if no freeing is needed.
 * @param[out] ppRollingBuffer   Pointer to the rolling buffer instance to be created.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_INVALID_ARG if the capacity is zero.
 * @return STATUS_NULL_ARG if ppRollingBuffer is NULL.
 * @return STATUS_NOT_ENOUGH_MEMORY if memory allocation fails.
 */
STATUS createRollingBuffer(UINT32, FreeDataFunc, PRollingBuffer*);

/**
 * @brief Frees a rolling buffer and all associated resources. This operation is idempotent.
 *
 * Deallocates all memory associated with the rolling buffer, including all buffered data,
 * by applying the free function if one was specified at creation. Sets the buffer pointer
 * (*ppRollingBuffer) to NULL.
 *
 * @param[in, out] ppRollingBuffer   Pointer to the rolling buffer instance to be freed.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_NULL_ARG if ppRollingBuffer is NULL.
 */
STATUS freeRollingBuffer(PRollingBuffer*);

/**
 * @brief Appends data to the rolling buffer.
 *
 * Adds a new element to the rolling buffer. If the buffer is full, the oldest element is overwritten.
 * The provided free function will be called on the overwritten element if it exists. This may move the
 * head and tail indexes.
 *
 * @param[in] pRollingBuffer   Pointer to the rolling buffer instance.
 * @param[in] data             Data element to add to the buffer.
 * @param[out] pIndex          Optional pointer to receive the index where the data was inserted.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_NULL_ARG if pRollingBuffer is NULL.
 * @return STATUS_ROLLING_BUFFER_NOT_IN_RANGE if the buffer is full and the data cannot be added.
 */
STATUS rollingBufferAppendData(PRollingBuffer, UINT64, PUINT64);

/**
 * @brief Inserts data at a specified index in the rolling buffer.
 *
 * Replaces data at the specified index in the buffer, freeing the existing element if needed.
 * The index must be within the current valid range of the buffer. This does not move the head
 * or tail indexes.
 *
 * @param[in] pRollingBuffer   Pointer to the rolling buffer instance.
 * @param[in] index            Index at which to insert data.
 * @param[in] data             Data element to insert.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_ROLLING_BUFFER_NOT_IN_RANGE if the index is out of range.
 * @return STATUS_NULL_ARG if pRollingBuffer is NULL.
 */
STATUS rollingBufferInsertData(PRollingBuffer, UINT64, UINT64);

/**
 * @brief Extracts data from the buffer at the specified index.
 *
 * Extracts data from the buffer, if the index is valid, and stores it in the
 * location pointed to by `pData`. The data at the index is removed from the buffer.
 * The user takes ownership of and is responsible for managing the memory of the
 * extracted data. This does not move the head or tail indexes.
 *
 * @param pRollingBuffer A pointer to the rolling buffer structure.
 * @param index The index from which to extract the data.
 * @param pData A pointer where the extracted data will be stored.
 *
 * @return STATUS_SUCCESS if the operation is successful, or an error code otherwise.
 *
 * @note The extracted data is not freed by the buffer. The user is responsible
 *       for freeing the data if necessary.
 */
STATUS rollingBufferExtractData(PRollingBuffer, UINT64, PUINT64);

/**
 * @brief Gets the current size of the rolling buffer.
 *
 * Provides the number of elements currently stored in the buffer.
 *
 * @param[in] pRollingBuffer   Pointer to the rolling buffer instance.
 * @param[out] pSize           Pointer to store the current size of the buffer.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_NULL_ARG if pRollingBuffer or pSize is NULL.
 */
STATUS rollingBufferGetSize(PRollingBuffer, PUINT32);

/**
 * @brief Checks if the rolling buffer is empty.
 *
 * Determines if the buffer contains any elements.
 *
 * @param[in] pRollingBuffer   Pointer to the rolling buffer instance.
 * @param[out] pIsEmpty        Pointer to store the result; TRUE if empty, FALSE otherwise.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_NULL_ARG if pRollingBuffer or pIsEmpty is NULL.
 */
STATUS rollingBufferIsEmpty(PRollingBuffer, PBOOL);

/**
 * @brief Checks if an index is within the valid range of the rolling buffer.
 *
 * Determines if the specified index is within the valid range of the buffer, accounting for overflow.
 *
 * If the `headIndex` is less than `tailIndex` (indicating overflow), there are two possible valid sub-ranges:
 * 1. From `tailIndex` to `UINT64_MAX`.
 * 2. From `0` to `headIndex - 1`.
 *
 * @param[in] pRollingBuffer   Pointer to the rolling buffer instance.
 * @param[in] index            The index to check.
 * @param[out] pIsValidIndex   Pointer to store the result; TRUE if the index is within the valid range, FALSE otherwise.
 *
 * @return STATUS_SUCCESS on success.
 * @return STATUS_NULL_ARG if pRollingBuffer or pIsValidIndex is NULL.
 * @return STATUS_ROLLING_BUFFER_NOT_IN_RANGE if the index is out of range.
 */
STATUS rollingBufferIsIndexInRange(PRollingBuffer, UINT64, PBOOL);

#ifdef __cplusplus
}
#endif
#endif //__KINESIS_VIDEO_WEBRTC_CLIENT_RTCP_ROLLING_BUFFER_H
