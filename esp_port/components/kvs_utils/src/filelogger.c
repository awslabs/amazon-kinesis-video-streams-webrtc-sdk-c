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

#define LOG_CLASS "FileLogger"

#include "common_defs.h"
#include "error.h"
#include "fileio.h"
#include "filelogger.h"
#include "logger.h"
#include <inttypes.h>
#include "esp_log.h"

/**
 * Kinesis Video Producer File based logger
 */

PFileLogger gFileLogger = NULL;

STATUS flushLogToFile(PFileLoggerParameters loggerParameters)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHAR filePath[MAX_PATH_LEN + 1];
    UINT32 filePathLen = 0, fileIndexStrSize = 0;
    UINT64 fileIndexToRemove = 0;
    CHAR fileIndexBuffer[KVS_COMMON_FILE_INDEX_BUFFER_SIZE];
    UINT64 charLenToWrite = 0;

    CHK(loggerParameters != NULL, STATUS_NULL_ARG);
    CHK(loggerParameters->currentOffset != 0, retStatus);
    if (loggerParameters->currentFileIndex >= gFileLogger->maxFileCount) {
        fileIndexToRemove = loggerParameters->currentFileIndex - gFileLogger->maxFileCount;
        filePathLen = SNPRINTF(filePath, ARRAY_SIZE(filePath), "%s%s%s.%" PRIu64, gFileLogger->logFileDir, FPATHSEPARATOR_STR,
                               loggerParameters->logFile, fileIndexToRemove);
        CHK(filePathLen <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
        if (0 != FREMOVE(filePath)) {
            PRINTF("failed to remove file %s\n", filePath);
        }
    }

    filePathLen = SNPRINTF(filePath, ARRAY_SIZE(filePath), "%s%s%s.%" PRIu64, gFileLogger->logFileDir, FPATHSEPARATOR_STR, loggerParameters->logFile,
                           loggerParameters->currentFileIndex);
    CHK(filePathLen <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);

    // we need to set null terminator properly because flush is triggered after a vsnprintf.
    // currentOffset should never be equal to stringBufferLen since vsnprintf always leave space for null terminator.
    // just in case currentOffset is greater than stringBufferLen, then use stringBufferLen.
    charLenToWrite = MIN(loggerParameters->currentOffset, loggerParameters->stringBufferLen - 1);
    loggerParameters->stringBuffer[charLenToWrite] = '\0';
    CHK_STATUS(writeFile(filePath, TRUE, FALSE, (PBYTE) loggerParameters->stringBuffer, charLenToWrite * SIZEOF(CHAR)));
    loggerParameters->currentFileIndex++;

    ULLTOSTR(loggerParameters->currentFileIndex, fileIndexBuffer, ARRAY_SIZE(fileIndexBuffer), 10, &fileIndexStrSize);
    retStatus = writeFile(loggerParameters->indexFilePath, TRUE, FALSE, (PBYTE) fileIndexBuffer, (STRLEN(fileIndexBuffer)) * SIZEOF(CHAR));
    if (STATUS_FAILED(retStatus)) {
        PRINTF("Failed to write to index file due to error 0x%08" PRIx32 "\n", retStatus);
        retStatus = STATUS_SUCCESS;
    }

CleanUp:

    if (loggerParameters != NULL) {
        loggerParameters->currentOffset = 0;
    }

    return retStatus;
}

VOID fileLoggerLogPrintFn(UINT32 level, PCHAR tag, PCHAR fmt, ...)
{
    UNUSED_PARAM(tag);
    CHAR logFmtString[MAX_LOG_FORMAT_LENGTH + 1];
    INT32 offset = 0;
    STATUS status = STATUS_SUCCESS;
    FileLoggerParameters* levelLoggerParameters = NULL;
    va_list valist;
    UINT32 logLevel = GET_LOGGER_LOG_LEVEL();

    if (logLevel != LOG_LEVEL_SILENT && level >= logLevel && gFileLogger != NULL) {
        MUTEX_LOCK(gFileLogger->lock);
        addLogMetadata(logFmtString, (UINT32) ARRAY_SIZE(logFmtString), fmt, level);

        if (gFileLogger->printLog) {
            va_start(valist, fmt);
            vprintf(logFmtString, valist);
            va_end(valist);
        }
        if (level == gFileLogger->filterLevel) {
            levelLoggerParameters = &gFileLogger->levelLogger;
        } else if (gFileLogger->enableAllLevels && level != gFileLogger->filterLevel) {
            levelLoggerParameters = &gFileLogger->mainLogger;
        }

        if (levelLoggerParameters != NULL) {
#if defined _WIN32 || defined _WIN64
            // On mingw, vsnprintf has a bug where if the string length is greater than the buffer
            // size it would just return -1.

            va_start(valist, fmt);
            // _vscprintf give the resulting string length
            offset = _vscprintf(logFmtString, valist);
            va_end(valist);
            if (offset > 0 && levelLoggerParameters->currentOffset + offset >= levelLoggerParameters->stringBufferLen) {
                status = flushLogToFile(levelLoggerParameters);
                if (STATUS_FAILED(status)) {
                    PRINTF("flush log to file failed with 0x%08x\n", status);
                }
            }
            // even if flushLogToFile failed, currentOffset will still be reset to 0
            // _vsnprintf truncates the string if it is larger than buffer
            va_start(valist, fmt);
            offset = _vsnprintf(levelLoggerParameters->stringBuffer + levelLoggerParameters->currentOffset,
                                levelLoggerParameters->stringBufferLen - levelLoggerParameters->currentOffset, logFmtString, valist);
            va_end(valist);

            // truncation happened
            if (offset == -1) {
                PRINTF("truncating log message as it can't fit into string buffer\n");
                offset = (INT32) levelLoggerParameters->stringBufferLen - 1;
            } else if (offset < 0) {
                // something went wrong
                PRINTF("_vsnprintf failed\n");
                offset = 0; // shouldnt cause any change to gFileLogger->currentOffset
            }
#else

            va_start(valist, fmt);
            offset = vsnprintf(levelLoggerParameters->stringBuffer + levelLoggerParameters->currentOffset,
                               levelLoggerParameters->stringBufferLen - levelLoggerParameters->currentOffset, logFmtString, valist);
            va_end(valist);

            // If vsnprintf fills the stringBuffer then flush first and then vsnprintf again into the stringBuffer.
            // This is because we dont know how long the log message is
            if (offset > 0 && levelLoggerParameters->currentOffset + offset >= levelLoggerParameters->stringBufferLen) {
                status = flushLogToFile(levelLoggerParameters);
                if (STATUS_FAILED(status)) {
                    PRINTF("flush log to file failed with 0x%08" PRIx32 "\n", status);
                }
                // even if flushLogToFile failed, currentOffset will still be reset to 0
                va_start(valist, fmt);
                offset = vsnprintf(levelLoggerParameters->stringBuffer + levelLoggerParameters->currentOffset,
                                   levelLoggerParameters->stringBufferLen - levelLoggerParameters->currentOffset, logFmtString, valist);
                va_end(valist);

                // if buffer is not big enough, vsnprintf returns number of characters (excluding the terminating null byte)
                // which would have been written to the final string if enough space had been available, after writing
                // gFileLogger->stringBufferLen - 1 bytes. Here we are truncating the log if its length is longer than stringBufferLen.
                if (offset > levelLoggerParameters->stringBufferLen) {
                    PRINTF("truncating log message as it can't fit into string buffer here\n");
                    offset = (INT32) levelLoggerParameters->stringBufferLen - 1;
                }
            }

            if (offset < 0) {
                // something went wrong
                PRINTF("vsnprintf failed here\n");
                offset = 0; // shouldn't cause any change to gFileLogger->currentOffset
            }

#endif
            levelLoggerParameters->currentOffset += offset;
            if (level == gFileLogger->filterLevel) {
                gFileLogger->levelLogger = *levelLoggerParameters;
            } else if (gFileLogger->enableAllLevels && level != gFileLogger->filterLevel) {
                gFileLogger->mainLogger = *levelLoggerParameters;
            }
        }

        MUTEX_UNLOCK(gFileLogger->lock);
    }
}

STATUS createFileLogger(UINT64 maxStringBufferLen, UINT64 maxLogFileCount, PCHAR logFileDir, BOOL printLog, BOOL setGlobalLogFn,
                        logPrintFunc* pFilePrintFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(gFileLogger == NULL, retStatus); // dont allocate again if already allocated
    CHK(maxStringBufferLen <= MAX_FILE_LOGGER_STRING_BUFFER_SIZE && maxStringBufferLen >= MIN_FILE_LOGGER_STRING_BUFFER_SIZE &&
            maxLogFileCount <= MAX_FILE_LOGGER_LOG_FILE_COUNT && maxLogFileCount > 0,
        STATUS_INVALID_ARG);
    CHK(STRNLEN(logFileDir, MAX_PATH_LEN + 1) <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
    BOOL fileFound = FALSE;
    CHAR fileIndexBuffer[KVS_COMMON_FILE_INDEX_BUFFER_SIZE];
    UINT64 charWritten = 0, indexFileSize = KVS_COMMON_FILE_INDEX_BUFFER_SIZE;

    // allocate the struct and string buffer together
    gFileLogger = (PFileLogger) MEMALLOC(SIZEOF(FileLogger) + maxStringBufferLen * SIZEOF(CHAR));
    MEMSET(gFileLogger, 0x00, SIZEOF(FileLogger));
    // point stringBuffer to the right place
    gFileLogger->mainLogger.stringBuffer = (PCHAR) (gFileLogger + 1);
    gFileLogger->mainLogger.stringBufferLen = maxStringBufferLen;
    STRNCPY(gFileLogger->mainLogger.logFile, FILE_LOGGER_LOG_FILE_NAME, MAX_PATH_LEN);
    gFileLogger->mainLogger.currentOffset = 0;
    gFileLogger->lock = MUTEX_CREATE(FALSE);
    gFileLogger->maxFileCount = maxLogFileCount;
    gFileLogger->mainLogger.currentFileIndex = 0;
    gFileLogger->filterLevel = 0; // 0 is not a valid level anyways. So this is ok
    gFileLogger->printLog = printLog;
    gFileLogger->fileLoggerLogPrintFn = fileLoggerLogPrintFn;
    // gFileLogger->fileLoggerLogPrintFn = esp_log_write;
    STRNCPY(gFileLogger->logFileDir, logFileDir, MAX_PATH_LEN);
    gFileLogger->logFileDir[MAX_PATH_LEN] = '\0';
    gFileLogger->enableAllLevels = TRUE;
    gFileLogger->filterLevel = 0;

    charWritten = SNPRINTF(gFileLogger->mainLogger.indexFilePath, MAX_PATH_LEN + 1, "%s%s%s", gFileLogger->logFileDir, FPATHSEPARATOR_STR,
                           FILE_LOGGER_LAST_INDEX_FILE_NAME);
    CHK(charWritten <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
    gFileLogger->mainLogger.indexFilePath[charWritten] = '\0';

    CHK_STATUS(fileExists(gFileLogger->mainLogger.indexFilePath, &fileFound));
    if (fileFound) {
        CHK_STATUS(readFile(gFileLogger->mainLogger.indexFilePath, FALSE, NULL, &indexFileSize));
        CHK(indexFileSize < KVS_COMMON_FILE_INDEX_BUFFER_SIZE, STATUS_FILE_LOGGER_INDEX_FILE_INVALID_SIZE);
        CHK_STATUS(readFile(gFileLogger->mainLogger.indexFilePath, FALSE, (PBYTE) fileIndexBuffer, &indexFileSize));
        fileIndexBuffer[indexFileSize] = '\0';
        STRTOUI64(fileIndexBuffer, NULL, 10, &gFileLogger->mainLogger.currentFileIndex);
    }

    // See if we are required to set the global log function pointer as well
    if (setGlobalLogFn) {
        // Store the original one to be reset later
        gFileLogger->storedLoggerLogPrintFn = globalCustomLogPrintFn;
        // Overwrite with the file logger
        globalCustomLogPrintFn = fileLoggerLogPrintFn;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeFileLogger();
        gFileLogger = NULL;
    } else if (pFilePrintFn != NULL) {
        *pFilePrintFn = fileLoggerLogPrintFn;
    }

    return retStatus;
}

STATUS createFileLoggerWithLevelFiltering(UINT64 maxStringBufferLen, UINT64 maxLogFileCount, PCHAR logFileDir, BOOL printLog, BOOL setGlobalLogFn,
                                          BOOL enableAllLevels, UINT32 level, logPrintFunc* pFilePrintFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(gFileLogger == NULL, retStatus); // dont allocate again if already allocated
    CHK(maxStringBufferLen <= MAX_FILE_LOGGER_STRING_BUFFER_SIZE && maxStringBufferLen >= MIN_FILE_LOGGER_STRING_BUFFER_SIZE &&
            maxLogFileCount <= MAX_FILE_LOGGER_LOG_FILE_COUNT && maxLogFileCount > 0,
        STATUS_INVALID_ARG);
    CHK(STRNLEN(logFileDir, MAX_PATH_LEN + 1) <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
    BOOL fileFound = FALSE;
    CHAR fileIndexBuffer[KVS_COMMON_FILE_INDEX_BUFFER_SIZE];
    UINT64 charWritten = 0, indexFileSize = KVS_COMMON_FILE_INDEX_BUFFER_SIZE;

    if (enableAllLevels) {
        // allocate the struct and string buffer together
        gFileLogger = (PFileLogger) MEMALLOC(SIZEOF(FileLogger) + maxStringBufferLen * 2 * SIZEOF(CHAR));
    } else {
        // allocate the struct and string buffer together
        gFileLogger = (PFileLogger) MEMALLOC(SIZEOF(FileLogger) + maxStringBufferLen * SIZEOF(CHAR));
    }

    MEMSET(gFileLogger, 0x00, SIZEOF(FileLogger));

    gFileLogger->lock = MUTEX_CREATE(FALSE);
    gFileLogger->maxFileCount = maxLogFileCount;
    gFileLogger->printLog = printLog;
    gFileLogger->fileLoggerLogPrintFn = fileLoggerLogPrintFn;
    STRNCPY(gFileLogger->logFileDir, logFileDir, MAX_PATH_LEN);
    gFileLogger->logFileDir[MAX_PATH_LEN] = '\0';
    gFileLogger->enableAllLevels = enableAllLevels;

    if (gFileLogger->enableAllLevels) {
        // point stringBuffer to the right place
        gFileLogger->mainLogger.stringBuffer = (PCHAR) (gFileLogger + 1);
        gFileLogger->mainLogger.stringBufferLen = maxStringBufferLen;
        STRNCPY(gFileLogger->mainLogger.logFile, FILE_LOGGER_LOG_FILE_NAME, MAX_PATH_LEN);
        gFileLogger->mainLogger.currentOffset = 0;
        gFileLogger->mainLogger.currentFileIndex = 0;

        charWritten = SNPRINTF(gFileLogger->mainLogger.indexFilePath, MAX_PATH_LEN + 1, "%s%s%s", gFileLogger->logFileDir, FPATHSEPARATOR_STR,
                               FILE_LOGGER_LAST_INDEX_FILE_NAME);

        CHK(charWritten <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
        gFileLogger->mainLogger.indexFilePath[charWritten] = '\0';

        CHK_STATUS(fileExists(gFileLogger->mainLogger.indexFilePath, &fileFound));
        if (fileFound) {
            CHK_STATUS(readFile(gFileLogger->mainLogger.indexFilePath, FALSE, NULL, &indexFileSize));
            CHK(indexFileSize < KVS_COMMON_FILE_INDEX_BUFFER_SIZE, STATUS_FILE_LOGGER_INDEX_FILE_INVALID_SIZE);
            CHK_STATUS(readFile(gFileLogger->mainLogger.indexFilePath, FALSE, (PBYTE) fileIndexBuffer, &indexFileSize));
            fileIndexBuffer[indexFileSize] = '\0';
            STRTOUI64(fileIndexBuffer, NULL, 10, &gFileLogger->mainLogger.currentFileIndex);
        }
    }

    if (level < LOG_LEVEL_VERBOSE || level > LOG_LEVEL_PROFILE) {
        gFileLogger->filterLevel = 0; // 0 is not a valid level anyways. So this is ok
    } else {
        if (gFileLogger->enableAllLevels) {
            gFileLogger->levelLogger.stringBuffer = (PCHAR) (gFileLogger + 1) + maxStringBufferLen;
        } else {
            gFileLogger->levelLogger.stringBuffer = (PCHAR) (gFileLogger + 1);
        }

        gFileLogger->levelLogger.stringBufferLen = maxStringBufferLen;
        STRNCPY(gFileLogger->levelLogger.logFile, FILE_LOGGER_FILTER_LOG_FILE_NAME, MAX_PATH_LEN);
        gFileLogger->levelLogger.currentOffset = 0;
        gFileLogger->levelLogger.currentFileIndex = 0;
        charWritten = SNPRINTF(gFileLogger->levelLogger.indexFilePath, MAX_PATH_LEN + 1, "%s%s%s", gFileLogger->logFileDir, FPATHSEPARATOR_STR,
                               FILE_LOGGER_LAST_FILTER_INDEX_FILE_NAME);
        CHK(charWritten <= MAX_PATH_LEN, STATUS_PATH_TOO_LONG);
        gFileLogger->levelLogger.indexFilePath[charWritten] = '\0';

        indexFileSize = KVS_COMMON_FILE_INDEX_BUFFER_SIZE;
        MEMSET(fileIndexBuffer, '\0', SIZEOF(fileIndexBuffer));
        CHK_STATUS(fileExists(gFileLogger->levelLogger.indexFilePath, &fileFound));
        if (fileFound) {
            CHK_STATUS(readFile(gFileLogger->levelLogger.indexFilePath, FALSE, NULL, &indexFileSize));
            CHK(indexFileSize < KVS_COMMON_FILE_INDEX_BUFFER_SIZE, STATUS_FILE_LOGGER_INDEX_FILE_INVALID_SIZE);
            CHK_STATUS(readFile(gFileLogger->levelLogger.indexFilePath, FALSE, (PBYTE) fileIndexBuffer, &indexFileSize));
            fileIndexBuffer[indexFileSize] = '\0';
            STRTOUI64(fileIndexBuffer, NULL, 10, &gFileLogger->levelLogger.currentFileIndex);
        }
        gFileLogger->filterLevel = level;
    }

    // See if we are required to set the global log function pointer as well
    if (setGlobalLogFn) {
        // Store the original one to be reset later
        gFileLogger->storedLoggerLogPrintFn = globalCustomLogPrintFn;
        // Overwrite with the file logger
        globalCustomLogPrintFn = fileLoggerLogPrintFn;
    }

CleanUp:

    if (STATUS_FAILED(retStatus)) {
        freeFileLogger();
        gFileLogger = NULL;
    } else if (pFilePrintFn != NULL) {
        *pFilePrintFn = fileLoggerLogPrintFn;
    }

    return retStatus;
}

STATUS freeFileLogger()
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(gFileLogger != NULL, retStatus);

    if (IS_VALID_MUTEX_VALUE(gFileLogger->lock)) {
        // flush out remaining log
        MUTEX_LOCK(gFileLogger->lock);
        retStatus = flushLogToFile(&gFileLogger->mainLogger);
        if (STATUS_FAILED(retStatus)) {
            PRINTF("flush log to file failed with 0x%08" PRIx32 "\n", retStatus);
        }

        retStatus = flushLogToFile(&gFileLogger->levelLogger);
        if (STATUS_FAILED(retStatus)) {
            PRINTF("flush log to file failed with 0x%08" PRIx32 "\n", retStatus);
        }

        retStatus = STATUS_SUCCESS;
        MUTEX_UNLOCK(gFileLogger->lock);

        MUTEX_FREE(gFileLogger->lock);
    }

    // Reset the original logger functionality
    if (gFileLogger->storedLoggerLogPrintFn != NULL) {
        globalCustomLogPrintFn = gFileLogger->storedLoggerLogPrintFn;
    }

    MEMFREE(gFileLogger);
    gFileLogger = NULL;

CleanUp:

    return retStatus;
}
