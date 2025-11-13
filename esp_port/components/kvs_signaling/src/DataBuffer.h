/**
 * Generic Data Buffer Management
 * Used for reassembling fragmented messages
 */
#ifndef __DATA_BUFFER_H__
#define __DATA_BUFFER_H__

#ifdef __cplusplus
extern "C" {
#endif

// #include "../Include_i.h"
#include <com/amazonaws/kinesis/video/webrtcclient/Include.h>

// Configuration parameters
#define DATA_BUFFER_DEFAULT_SIZE (2 * 1024)   // Default buffer size: 2KB
#define DATA_BUFFER_MAX_SIZE (20 * 1024) // Maximum buffer size: 20KB
#define DATA_BUFFER_EXPANSION_PADDING (512)   // Padding when expanding buffer

// Special status codes for buffer operations
#define STATUS_DATA_BUFFER_COMPLETE 0x01000000 // Buffer contains complete message
#define STATUS_DATA_BUFFER_HANDLE_NULL 0x01000001 // Handle is NULL
#define STATUS_DATA_BUFFER_IS_NULL 0x01000002 // Buffer is NULL

/**
 * Data buffer structure to manage fragmented data
 */
typedef struct {
    PCHAR buffer;          // Pointer to the allocated buffer
    UINT32 currentSize;    // Current used size of the buffer
    UINT32 maxSize;        // Maximum capacity of the buffer
    BOOL inProgress;       // Whether a multi-part message is currently being assembled
} DataBuffer, *PDataBuffer;

/**
 * Initialize a data buffer with suggested size
 *
 * @param ppDataBuffer - OUT - Address of the pointer to the data buffer to initialize
 * @param suggestedSize - IN - Suggested initial size of the buffer (0 for default)
 *
 * @return STATUS code of the execution
 */
STATUS initDataBuffer(PDataBuffer* ppDataBuffer, UINT32 suggestedSize);

/**
 * Free a data buffer and its resources
 *
 * @param ppDataBuffer - IN/OUT - Address of the pointer to the data buffer to free
 *
 * @return STATUS code of the execution
 */
STATUS freeDataBuffer(PDataBuffer* ppDataBuffer);

/**
 * Reset a data buffer to empty state without deallocating memory
 *
 * @param pDataBuffer - IN - Pointer to the data buffer to reset
 *
 * @return STATUS code of the execution
 */
STATUS resetDataBuffer(PDataBuffer pDataBuffer);

/**
 * Expand a data buffer to accommodate additional data
 *
 * @param pDataBuffer - IN - Pointer to the data buffer to expand
 * @param additionalSize - IN - Additional space needed in bytes
 *
 * @return STATUS code of the execution
 */
STATUS expandDataBuffer(PDataBuffer pDataBuffer, UINT32 additionalSize);

/**
 * Append data to a data buffer
 *
 * @param pDataBuffer - IN - Pointer to the data buffer to append to
 * @param pData - IN - Data to append
 * @param dataLen - IN - Length of data to append
 * @param isFinal - IN - Whether this is the final fragment (TRUE) or more data is expected (FALSE)
 *
 * @return STATUS code of the execution
 *         Returns STATUS_DATA_BUFFER_COMPLETE when buffer contains a complete message
 */
STATUS appendDataBuffer(PDataBuffer pDataBuffer, const char* pData, UINT32 dataLen, BOOL isFinal);

#ifdef __cplusplus
}
#endif
#endif /* __DATA_BUFFER_H__ */
