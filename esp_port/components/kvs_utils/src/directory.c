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
#include "directory.h"
#include "fileio.h"
#include "kvs_string.h"

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Traverses the directory iteratively
 *
 * Parameters:
 *      dirPath - path to the directory
 *      userData - custom data passed by the caller which is returned with the callback
 *      iterate - whether to iterate to sub-directories
 *      entryFn - callback function to call
 */
STATUS traverseDirectory(PCHAR dirPath, UINT64 userData, BOOL iterate, DirectoryEntryCallbackFunc entryFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR tempFileName[MAX_PATH_LEN];
    UINT32 pathLen;

#if defined __WINDOWS_BUILD__
    WIN32_FIND_DATA findData;
    HANDLE hFind = INVALID_HANDLE_VALUE;
    UINT32 error = 0;
#else
    UINT32 dirPathLen;
    DIR* pDir = NULL;
    struct dirent* pDirEnt = NULL;
    struct stat entryStat;
#endif

    CHK(dirPath != NULL && entryFn != NULL && dirPath[0] != '\0', STATUS_INVALID_ARG);

    // Ensure we don't get a very long paths. Need at least a separator and a null terminator.
    pathLen = (UINT32) STRLEN(dirPath);
    CHK(pathLen + 2 < MAX_PATH_LEN, STATUS_PATH_TOO_LONG);

    // Ensure the path is appended with the separator
    STRNCPY(tempFileName, dirPath, MAX_PATH_LEN);

#if defined __WINDOWS_BUILD__
    // Open the find

    if (tempFileName[pathLen - 1] != '*') {
        tempFileName[pathLen] = FPATHSEPARATOR;
        tempFileName[pathLen + 1] = '*';
        tempFileName[pathLen + 2] = '\0';
    }

    hFind = FindFirstFile(tempFileName, &findData);

    CHK(INVALID_HANDLE_VALUE != hFind, STATUS_DIRECTORY_OPEN_FAILED);

    do {
        if ((0 == STRCMP(findData.cFileName, ".")) || (0 == STRCMP(findData.cFileName, ".."))) {
            continue;
        }

        // Prepare the path
        tempFileName[pathLen] = '\0';

        // Check if it's a directory, link, file or unknown
        tempFileName[pathLen] = FPATHSEPARATOR;
        tempFileName[pathLen + 1] = '\0';
        STRNCAT(tempFileName, findData.cFileName, MAX_PATH_LEN - pathLen - 2);

        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_LINK, tempFileName, findData.cFileName));
        } else if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            // Iterate into sub-directories if specified
            if (iterate) {
                DLOGE("\r\nDir Path %s, tempFile %s find %s\r\n", dirPath, tempFileName, findData.cFileName);

                // Recurse into the directory
                CHK_STATUS(traverseDirectory(tempFileName, userData, iterate, entryFn));
            }

            // Call the callback
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_DIRECTORY, tempFileName, findData.cFileName));
        } else {
            // Treat as if a normal file
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_FILE, tempFileName, findData.cFileName));
        }

    } while (FindNextFile(hFind, &findData));

CleanUp:

    if (hFind != INVALID_HANDLE_VALUE) {
        FindClose(hFind);
    }

#else

    if (tempFileName[pathLen - 1] != FPATHSEPARATOR) {
        tempFileName[pathLen] = FPATHSEPARATOR;
        pathLen++;
        tempFileName[pathLen] = '\0';
    }

    pDir = FOPENDIR(tempFileName);

    // Need to make a distinction between various types of failures
    if (pDir == NULL) {
        switch (errno) {
            case EACCES:
                CHK(FALSE, STATUS_DIRECTORY_ACCESS_DENIED);
            case ENOENT:
                CHK(FALSE, STATUS_DIRECTORY_MISSING_PATH);
            default:
                CHK(FALSE, STATUS_DIRECTORY_OPEN_FAILED);
        }
    }

    while (NULL != (pDirEnt = FREADDIR(pDir))) {
        if ((0 == STRCMP(pDirEnt->d_name, ".")) || (0 == STRCMP(pDirEnt->d_name, ".."))) {
            continue;
        }

        // Prepare the path
        tempFileName[pathLen] = '\0';

        // Check if it's a directory, link, file or unknown
        // Ensure we don't overflow the buffer
        SIZE_T nameLen = STRLEN(pDirEnt->d_name);
        SIZE_T remainingSpace = MAX_PATH_LEN - pathLen - 1;
        if (nameLen < remainingSpace) {
            STRNCAT(tempFileName, pDirEnt->d_name, remainingSpace);
        } else {
            // Path is too long, skip this entry
            continue;
        }
        CHK(0 == FSTAT(tempFileName, &entryStat), STATUS_DIRECTORY_ENTRY_STAT_ERROR);

        if (S_ISREG(entryStat.st_mode)) {
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_FILE, tempFileName, pDirEnt->d_name));
        } else if (S_ISLNK(entryStat.st_mode)) {
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_LINK, tempFileName, pDirEnt->d_name));
        } else if (S_ISDIR(entryStat.st_mode)) {
            // Append the path separator and null terminate
            dirPathLen = STRLEN(tempFileName);
            CHK(dirPathLen + 2 < MAX_PATH_LEN, STATUS_PATH_TOO_LONG);

            // Iterate into sub-directories if specified
            if (iterate) {
                tempFileName[dirPathLen] = FPATHSEPARATOR;
                tempFileName[dirPathLen + 1] = '\0';

                // Recurse into the directory
                CHK_STATUS(traverseDirectory(tempFileName, userData, iterate, entryFn));
            }

            // Remove the path separator
            tempFileName[dirPathLen] = '\0';
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_DIRECTORY, tempFileName, pDirEnt->d_name));
        } else {
            // We treat this as unknown
            CHK_STATUS(entryFn(userData, DIR_ENTRY_TYPE_UNKNOWN, tempFileName, pDirEnt->d_name));
        }
    }

CleanUp:

    if (pDir != NULL) {
        FCLOSEDIR(pDir);
        pDir = NULL;
    }

#endif
    return retStatus;
}
