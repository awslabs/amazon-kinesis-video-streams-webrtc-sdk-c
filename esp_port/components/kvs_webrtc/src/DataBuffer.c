/**
 * Implementation of generic data buffer functions
 */
#define LOG_CLASS "DataBuffer"
#include "../Include_i.h"
#include "DataBuffer.h"

/**
 * Initialize a data buffer with suggested size
 */
STATUS initDataBuffer(PDataBuffer* ppDataBuffer, UINT32 suggestedSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 bufferSize;
    PDataBuffer pDataBuffer = NULL;

    CHK(ppDataBuffer != NULL, STATUS_NULL_ARG);

    // Allocate the data buffer structure
    pDataBuffer = (PDataBuffer) MEMCALLOC(1, SIZEOF(DataBuffer));
    CHK(pDataBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Determine appropriate buffer size based on suggested size
    if (suggestedSize > 0) {
        bufferSize = suggestedSize;
        // Cap at a maximum size to prevent memory issues
        if (bufferSize > DATA_BUFFER_MAX_SIZE) {
            bufferSize = DATA_BUFFER_MAX_SIZE;
        }
    } else {
        // Use default size if no suggestion
        bufferSize = DATA_BUFFER_DEFAULT_SIZE;
    }

    // Allocate new buffer
    pDataBuffer->buffer = (PCHAR) MEMALLOC(bufferSize + 1); // +1 for null terminator
    CHK(pDataBuffer->buffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Initialize buffer state
    pDataBuffer->maxSize = bufferSize;
    pDataBuffer->currentSize = 0;
    pDataBuffer->inProgress = FALSE;

    // Clear buffer contents
    MEMSET(pDataBuffer->buffer, 0, bufferSize + 1);

    // Set the output parameter
    *ppDataBuffer = pDataBuffer;
    pDataBuffer = NULL; // Ownership transferred, don't free

    DLOGD("Data buffer initialized with size %d bytes", bufferSize);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to initialize data buffer with status 0x%08x", retStatus);

        if (pDataBuffer != NULL) {
            if (pDataBuffer->buffer != NULL) {
                MEMFREE(pDataBuffer->buffer);
            }
            MEMFREE(pDataBuffer);
        }

        if (ppDataBuffer != NULL) {
            *ppDataBuffer = NULL;
        }
    }

    LEAVES();
    return retStatus;
}

/**
 * Free a data buffer and its resources
 */
STATUS freeDataBuffer(PDataBuffer* ppDataBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    PDataBuffer pDataBuffer;

    CHK(ppDataBuffer != NULL, STATUS_NULL_ARG);
    pDataBuffer = *ppDataBuffer;

    // It's okay if the buffer is already NULL
    if (pDataBuffer != NULL) {
        if (pDataBuffer->buffer != NULL) {
            MEMFREE(pDataBuffer->buffer);
            pDataBuffer->buffer = NULL;
        }

        MEMFREE(pDataBuffer);
        *ppDataBuffer = NULL;
    }

CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * Reset a data buffer to empty state without deallocating memory
 */
STATUS resetDataBuffer(PDataBuffer pDataBuffer)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDataBuffer != NULL, STATUS_DATA_BUFFER_HANDLE_NULL);

    if (pDataBuffer->buffer != NULL) {
        // Clear the buffer contents but maintain the allocation
        if (pDataBuffer->maxSize > 0) {
            MEMSET(pDataBuffer->buffer, 0, pDataBuffer->maxSize + 1);
        }

        // Reset size counters and flags
        pDataBuffer->currentSize = 0;
        pDataBuffer->inProgress = FALSE;

        DLOGD("Data buffer reset (max size: %d)", pDataBuffer->maxSize);
    } else {
        // Return an error if buffer is NULL
        CHK(FALSE, STATUS_DATA_BUFFER_IS_NULL);
    }

CleanUp:
    LEAVES();
    return retStatus;
}

/**
 * Expand a data buffer to accommodate additional data
 */
STATUS expandDataBuffer(PDataBuffer pDataBuffer, UINT32 additionalSize)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 newBufferSize;
    PCHAR newBuffer;

    CHK(pDataBuffer != NULL, STATUS_DATA_BUFFER_HANDLE_NULL);
    CHK(pDataBuffer->buffer != NULL, STATUS_DATA_BUFFER_IS_NULL);
    CHK(additionalSize > 0, STATUS_INVALID_ARG);

    // Calculate new buffer size
    newBufferSize = pDataBuffer->maxSize + additionalSize;

    // Cap at a maximum size to prevent memory issues
    if (newBufferSize > DATA_BUFFER_MAX_SIZE) {
        newBufferSize = DATA_BUFFER_MAX_SIZE;
    }

    // Reallocate buffer to new size
    newBuffer = (PCHAR) MEMREALLOC(pDataBuffer->buffer, newBufferSize + 1); // +1 for null terminator
    CHK(newBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Update buffer state
    pDataBuffer->buffer = newBuffer;
    pDataBuffer->maxSize = newBufferSize;

    DLOGD("Data buffer expanded to %d bytes", newBufferSize);

CleanUp:
    if (STATUS_FAILED(retStatus)) {
        DLOGE("Failed to expand data buffer with status 0x%08x", retStatus);
    }

    LEAVES();
    return retStatus;
}

/**
 * Append data to a data buffer
 */
STATUS appendDataBuffer(PDataBuffer pDataBuffer, PCHAR pData, UINT32 dataLen, BOOL isFinal)
{
    ENTERS();
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDataBuffer != NULL, STATUS_DATA_BUFFER_HANDLE_NULL);
    CHK(pDataBuffer->buffer != NULL, STATUS_DATA_BUFFER_IS_NULL);
    CHK(pData != NULL && dataLen > 0, STATUS_INVALID_ARG);

    DLOGD("Appending %d bytes to data buffer, final: %d", dataLen, isFinal);

    // Mark buffer as being used for a message
    pDataBuffer->inProgress = TRUE;

    // Check if we have enough space in the buffer
    if (pDataBuffer->currentSize + dataLen > pDataBuffer->maxSize) {
        // Try to expand the buffer
        DLOGW("Data buffer too small (%d), expanding for additional %d bytes",
                 pDataBuffer->maxSize, dataLen);

        // Calculate needed additional space with padding
        UINT32 additionalSize = dataLen + DATA_BUFFER_EXPANSION_PADDING;

        CHK_STATUS(expandDataBuffer(pDataBuffer, additionalSize));
    }

    // Now we should have enough space, copy the data
    MEMCPY(pDataBuffer->buffer + pDataBuffer->currentSize, pData, dataLen);
    pDataBuffer->currentSize += dataLen;

    // If this is the final fragment, mark the buffer as complete
    if (isFinal) {
        // Null-terminate the buffer for string operations
        if (pDataBuffer->currentSize < pDataBuffer->maxSize) {
            pDataBuffer->buffer[pDataBuffer->currentSize] = '\0';
        } else {
            // This shouldn't happen as we've expanded the buffer, but just in case
            pDataBuffer->buffer[pDataBuffer->maxSize] = '\0';
        }

        // Set return value to indicate buffer is complete
        retStatus = STATUS_DATA_BUFFER_COMPLETE;
        pDataBuffer->inProgress = FALSE;
    }

CleanUp:
    if (STATUS_FAILED(retStatus) && retStatus != STATUS_DATA_BUFFER_COMPLETE) {
        DLOGE("Failed to append to data buffer with status 0x%08x", retStatus);

        // Only reset the buffer if we own it
        if (pDataBuffer != NULL) {
            resetDataBuffer(pDataBuffer);
        }
    }

    LEAVES();
    return retStatus;
}
