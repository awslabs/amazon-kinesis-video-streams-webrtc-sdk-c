/*
 * Copyright 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include <dirent.h>
#include <sys/stat.h>
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "fileio.h"
#include "flash_wrapper.h"

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

PUBLIC_API STATUS readFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, PUINT64 pSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    bool exists = false;
    SIZE_T fileLen = 0;

    CHK(filePath != NULL && pSize != NULL, STATUS_NULL_ARG);

    // Check if file exists
    CHK(flash_wrapper_exists(filePath, &exists) == ESP_OK, STATUS_OPEN_FILE_FAILED);
    CHK(exists == true, STATUS_OPEN_FILE_FAILED);

    // Get the size of the file
    CHK(flash_wrapper_get_size(filePath, &fileLen) == ESP_OK, STATUS_READ_FILE_FAILED);

    if (pBuffer == NULL) {
        DLOGD("pBuffer is NULL, setting pSize to fileLen: %zu", fileLen);
        // requested the length - set and early return
        *pSize = fileLen;
        CHK(FALSE, STATUS_SUCCESS);
    }

    DLOGD("File size: %zu, pSize: %" PRIu64, fileLen, *pSize);

    // Validate the buffer size
    CHK(*pSize > 0, STATUS_BUFFER_TOO_SMALL);

    // Read the file into memory buffer
    CHK(flash_wrapper_read(filePath, pBuffer, *pSize, 0) == ESP_OK, STATUS_READ_FILE_FAILED);

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

PUBLIC_API STATUS readFileSegment(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 readSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    bool exists;
    size_t total_size = 0;

    CHK(filePath != NULL && pBuffer != NULL && readSize != 0, STATUS_NULL_ARG);

    // Check if file exists
    CHK(flash_wrapper_exists(filePath, &exists) == ESP_OK, STATUS_OPEN_FILE_FAILED);
    CHK(exists == true, STATUS_OPEN_FILE_FAILED);

    // Get total file size
    CHK(flash_wrapper_read(filePath, NULL, 0, 0) == ESP_OK, STATUS_READ_FILE_FAILED);
    total_size = readSize;

    // Check if we are trying to read past the end of the file
    CHK(offset + readSize <= total_size, STATUS_READ_FILE_FAILED);

    // Read the file segment
    CHK(flash_wrapper_read(filePath, pBuffer, readSize, offset) == ESP_OK, STATUS_READ_FILE_FAILED);

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

PUBLIC_API STATUS writeFile(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE existingData = NULL;
    UINT64 existingSize = 0;

    CHK(filePath != NULL && pBuffer != NULL, STATUS_NULL_ARG);

    if (append) {
        // For append mode, we need to:
        // 1. Read existing content
        // 2. Allocate new buffer
        // 3. Copy existing + new content
        // 4. Write combined buffer
        bool exists;
        CHK(flash_wrapper_exists(filePath, &exists) == ESP_OK, STATUS_OPEN_FILE_FAILED);

        if (exists) {
            CHK(flash_wrapper_read(filePath, NULL, 0, 0) == ESP_OK, STATUS_READ_FILE_FAILED);
            existingSize = size; // Get size from read call

            if (existingSize > 0) {
                existingData = (PBYTE) MEMALLOC(existingSize);
                CHK(existingData != NULL, STATUS_NOT_ENOUGH_MEMORY);

                CHK(flash_wrapper_read(filePath, existingData, existingSize, 0) == ESP_OK, STATUS_READ_FILE_FAILED);

                // Create combined buffer
                PBYTE combinedBuffer = (PBYTE) MEMALLOC(existingSize + size);
                CHK(combinedBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

                MEMCPY(combinedBuffer, existingData, existingSize);
                MEMCPY(combinedBuffer + existingSize, pBuffer, size);

                // Write combined buffer
                CHK(flash_wrapper_write(filePath, combinedBuffer, existingSize + size) == ESP_OK, STATUS_WRITE_TO_FILE_FAILED);

                MEMFREE(combinedBuffer);
            }
        }

        if (!exists || existingSize == 0) {
            // File doesn't exist or is empty, just write new content
            CHK(flash_wrapper_write(filePath, pBuffer, size) == ESP_OK, STATUS_WRITE_TO_FILE_FAILED);
        }
    } else {
        // For non-append mode, simply write the buffer
        CHK(flash_wrapper_write(filePath, pBuffer, size) == ESP_OK, STATUS_WRITE_TO_FILE_FAILED);
    }

CleanUp:
    if (existingData != NULL) {
        MEMFREE(existingData);
    }
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

PUBLIC_API STATUS getFileLength(PCHAR filePath, PUINT64 pLength)
{
    STATUS retStatus = STATUS_SUCCESS;
    bool exists;

    CHK(filePath != NULL && pLength != NULL, STATUS_NULL_ARG);

    // Check if file exists
    CHK(flash_wrapper_exists(filePath, &exists) == ESP_OK, STATUS_OPEN_FILE_FAILED);
    CHK(exists == true, STATUS_OPEN_FILE_FAILED);

    // Get file size
    *pLength = 0;
    CHK(flash_wrapper_get_size(filePath, (size_t*) pLength) == ESP_OK, STATUS_READ_FILE_FAILED);

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

PUBLIC_API STATUS fileExists(PCHAR filePath, PBOOL pExists)
{
    STATUS retStatus = STATUS_SUCCESS;

    if (filePath == NULL || pExists == NULL) {
        return STATUS_NULL_ARG;
    }

    bool exists;
    CHK(flash_wrapper_exists(filePath, &exists) == ESP_OK, STATUS_INVALID_OPERATION);
    *pExists = exists;

CleanUp:
    CHK_LOG_ERR(retStatus);
    return retStatus;
}

PUBLIC_API STATUS createFile(PCHAR filePath, UINT64 size)
{
    STATUS retStatus = STATUS_SUCCESS;
    PBYTE pZeroBuffer = NULL;

    CHK(filePath != NULL, STATUS_NULL_ARG);

    if (size > 0) {
        // Allocate a zero buffer
        pZeroBuffer = (PBYTE) MEMCALLOC(1, size);
        CHK(pZeroBuffer != NULL, STATUS_NOT_ENOUGH_MEMORY);

        // Write the zero buffer
        CHK(flash_wrapper_write(filePath, pZeroBuffer, size) == ESP_OK, STATUS_WRITE_TO_FILE_FAILED);
    } else {
        // Just create an empty file
        CHK(flash_wrapper_write(filePath, (PBYTE)"", 0) == ESP_OK, STATUS_WRITE_TO_FILE_FAILED);
    }

CleanUp:
    CHK_LOG_ERR(retStatus);
    if (pZeroBuffer != NULL) {
        MEMFREE(pZeroBuffer);
    }
    return retStatus;
}
