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
#include <dirent.h> //!< #TBD
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "fileio.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
PUBLIC_API STATUS readFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, PUINT64 pSize)
{
    UINT64 fileLen;
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL && pSize != NULL, STATUS_NULL_ARG);

    fp = FOPEN(filePath, binMode ? "rb" : "r");

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Get the size of the file
    FSEEK(fp, 0, SEEK_END);
    fileLen = FTELL(fp);

    if (pBuffer == NULL) {
        // requested the length - set and early return
        *pSize = fileLen;
        CHK(FALSE, STATUS_SUCCESS);
    }

    // Validate the buffer size
    CHK(fileLen <= *pSize, STATUS_BUFFER_TOO_SMALL);

    // Read the file into memory buffer
    FSEEK(fp, 0, SEEK_SET);
    CHK(FREAD(pBuffer, (SIZE_T) fileLen, 1, fp) == 1, STATUS_READ_FILE_FAILED);

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    return retStatus;
}

PUBLIC_API STATUS readFileSegment(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 readSize)
{
    UINT64 fileLen;
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;
    INT32 result = 0;

    CHK(filePath != NULL && pBuffer != NULL && readSize != 0, STATUS_NULL_ARG);

    fp = FOPEN(filePath, binMode ? "rb" : "r");

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Get the size of the file
    FSEEK(fp, 0, SEEK_END);
    fileLen = FTELL(fp);

    // Check if we are trying to read past the end of the file
    CHK(offset + readSize <= fileLen, STATUS_READ_FILE_FAILED);

    // Set the offset and read the file content
    result = FSEEK(fp, (UINT32) offset, SEEK_SET);
    CHK(result && (FREAD(pBuffer, (SIZE_T) readSize, 1, fp) == 1), STATUS_READ_FILE_FAILED);

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    return retStatus;
}

PUBLIC_API STATUS writeFile(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size)
{
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL && pBuffer != NULL, STATUS_NULL_ARG);

    fp = FOPEN(filePath, binMode ? (append ? "ab" : "wb") : (append ? "a" : "w"));

    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    // Write the buffer to the file
    CHK(FWRITE(pBuffer, (SIZE_T) size, 1, fp) == 1, STATUS_WRITE_TO_FILE_FAILED);

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    return retStatus;
}

PUBLIC_API STATUS getFileLength(PCHAR filePath, PUINT64 pLength)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_STATUS(readFile(filePath, TRUE, NULL, pLength));

CleanUp:

    return retStatus;
}

PUBLIC_API STATUS fileExists(PCHAR filePath, PBOOL pExists)
{
    if (filePath == NULL || pExists == NULL) {
        return STATUS_NULL_ARG;
    }

    struct GLOBAL_STAT st;
    INT32 result = FSTAT(filePath, &st);
    *pExists = (result == 0);

    return STATUS_SUCCESS;
}

PUBLIC_API STATUS createFile(PCHAR filePath, UINT64 size)
{
    STATUS retStatus = STATUS_SUCCESS;
    FILE* fp = NULL;

    CHK(filePath != NULL, STATUS_NULL_ARG);

    fp = FOPEN(filePath, "w+b");
    CHK(fp != NULL, STATUS_OPEN_FILE_FAILED);

    if (size != 0) {
        CHK(0 == FSEEK(fp, (UINT32)(size - 1), SEEK_SET), STATUS_INVALID_OPERATION);
        CHK(0 == FPUTC(0, fp), STATUS_INVALID_OPERATION);
    }

CleanUp:

    if (fp != NULL) {
        FCLOSE(fp);
        fp = NULL;
    }

    return retStatus;
}
