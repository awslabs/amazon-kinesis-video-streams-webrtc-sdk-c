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
#ifndef __AWS_KVS_WEBRTC_FILE_IO_INCLUDE__
#define __AWS_KVS_WEBRTC_FILE_IO_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "error.h"
#include "common_defs.h"


/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * @brief Read a file from the given full/relative filePath into the memory area pointed to by pBuffer.
 * Specifying NULL in pBuffer will return the size of the file.
 *
 * @param[in] filePath file path to read from
 * @param[in] binMode TRUE to read file stream as binary; FALSE to read as a normal text file
 * @param[in] pBuffer buffer to write contents of the file to. If NULL return the size in pSize.
 * @param[in] pSize destination PUINT64 to store the size of the file when pBuffer is NULL;
 *
 * @return STATUS code of the execution.
 */
PUBLIC_API STATUS readFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, PUINT64 pSize);

/**
 * @brief Read a section of the file from the given full/relative filePath into the memory area pointed to by pBuffer.
 * NOTE: The buffer should be large enough to read the section.
 *
 * @param[in] filePath file path to read from
 * @param[in] binMode TRUE to read file stream as binary; FALSE to read as a normal text file
 * @param[in] pBuffer buffer to write contents of the file to. Non-null
 * @param[in] offset Offset into the file to start reading from.
 * @param[in] readSize The number of bytes to read from the file.
 *
 * @return STATUS code of the execution.
 */
PUBLIC_API STATUS readFileSegment(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 readSize);

/**
 * @brief
 *
 * @param[in]
 * @param[in]
 * @param[in]
 * @param[in]
 *
 * @return STATUS code of the execution.
 */
/**
 * Write contents pointed to by pBuffer to the given filePath.
 *
 * Parameters:
 *     filePath - file path to write to
 *     binMode  - TRUE to read file stream as binary; FALSE to read as a normal text file
 *     append   - TRUE to append; FALSE to overwrite
 *     pBuffer  - memory location whose contents should be written to the file
 *     size     - number of bytes that should be written to the file
 */
PUBLIC_API STATUS writeFile(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size);

/**
 * @brief
 *
 * @param[in]
 * @param[in]
 * @param[in]
 * @param[in]
 *
 * @return STATUS code of the execution.
 *
 * Gets the file length of the given filePath.
 *
 * Parameters:
 *     filePath - file path whose file length should be computed
 *     pLength  - Returns the size of the file in bytes
 *
 * Returns:
 *     STATUS of the operation
 */
PUBLIC_API STATUS getFileLength(PCHAR filePath, PUINT64 pSize);

/**
 * @brief
 *
 * @param[in]
 * @param[in]
 * @param[in]
 * @param[in]
 *
 * @return STATUS code of the execution.
 *
 * Checks if the file or directory exists with a given full or relative path
 *
 * Parameters:
 *      filePath - file path to check
 *      pExists - TRUE if the file exists
 */
PUBLIC_API STATUS fileExists(PCHAR filePath, PBOOL pExists);

/**
 * @brief
 *
 * @param[in]
 * @param[in]
 * @param[in]
 * @param[in]
 *
 * @return STATUS code of the execution.
 *
 * Creates/overwrites a new file with a given size
 *
 * Parameters:
 *      filePath - file path to check
 *      size - The size of the newly created file
 */
PUBLIC_API STATUS createFile(PCHAR filePath, UINT64 size);

// TODO: non-implemented functionality
PUBLIC_API STATUS setFileLength(PCHAR filePath, UINT64 size);
PUBLIC_API STATUS updateFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 size);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_FILE_IO_INCLUDE__ */
