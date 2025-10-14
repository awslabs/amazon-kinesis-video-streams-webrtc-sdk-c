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
#ifndef __AWS_KVS_WEBRTC_FILE_LOGGER_INCLUDE__
#define __AWS_KVS_WEBRTC_FILE_LOGGER_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "platform_utils.h"
#include "error.h"

/******************************************************************************
 * Main defines
 ******************************************************************************/

/**
 * File based logger limit constants
 */
#define MAX_FILE_LOGGER_STRING_BUFFER_SIZE (100 * 1024 * 1024)
#define MIN_FILE_LOGGER_STRING_BUFFER_SIZE (10 * 1024)
#define MAX_FILE_LOGGER_LOG_FILE_COUNT     (10 * 1024)

/**
 * Default values used in the file logger
 */
#define FILE_LOGGER_LOG_FILE_NAME               "kvsFileLog"
#define FILE_LOGGER_FILTER_LOG_FILE_NAME        "kvsFileLogFilter"
#define FILE_LOGGER_LAST_INDEX_FILE_NAME        "kvsFileLogIndex"
#define FILE_LOGGER_LAST_FILTER_INDEX_FILE_NAME "kvsFileFilterLogIndex"
#define FILE_LOGGER_STRING_BUFFER_SIZE          (100 * 1024)
#define FILE_LOGGER_LOG_FILE_COUNT              3
#define FILE_LOGGER_LOG_FILE_DIRECTORY_PATH     "./"

/**
 * Default values for the limits
 */
#define KVS_COMMON_FILE_INDEX_BUFFER_SIZE       256

/**
 * Default values for the limits
 */
#define KVS_COMMON_FILE_INDEX_BUFFER_SIZE 256

typedef struct {
    // string buffer. once the buffer is full, its content will be flushed to file
    PCHAR stringBuffer;

    // Size of the buffer in bytes
    // This will point to the end of the FileLogger to allow for single allocation and preserve the processor cache locality
    UINT64 stringBufferLen;

    // bytes starting from beginning of stringBuffer that contains valid data
    UINT64 currentOffset;

    // file to store last log file index
    CHAR indexFilePath[MAX_PATH_LEN + 1];

    // file to store log file name
    CHAR logFile[MAX_PATH_LEN + 1];

    // index for next log file
    UINT64 currentFileIndex;
} FileLoggerParameters, *PFileLoggerParameters;

/**
 * file logger declaration
 */
typedef struct {
    FileLoggerParameters mainLogger;

    FileLoggerParameters levelLogger;

    // lock protecting the print operation
    MUTEX lock;

    // filter level
    UINT32 filterLevel;

    // directory to put the log file
    CHAR logFileDir[MAX_PATH_LEN + 1];

    // max number of log file allowed
    UINT64 maxFileCount;

    // print log to stdout too
    BOOL printLog;

    BOOL enableAllLevels;

    // file logger logPrint callback
    logPrintFunc fileLoggerLogPrintFn;

    // Original stored logger function
    logPrintFunc storedLoggerLogPrintFn;
} FileLogger, *PFileLogger;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Creates a file based logger object and installs the global logger callback function
 *
 * @param - UINT64 - IN - Size of string buffer in file logger. When the string buffer is full the logger will flush everything into a new file
 * @param - UINT64 - IN - Max number of log file. When exceeded, the oldest file will be deleted when new one is generated
 * @param - PCHAR - IN - Directory in which the log file will be generated
 * @param - BOOL - IN - Whether to print log to std out too
 * @param - BOOL - IN - Whether to set global logger function pointer
 * @param - logPrintFunc* - OUT/OPT - Optional function pointer to be returned to the caller that contains the main function for actual output
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createFileLogger(UINT64, UINT64, PCHAR, BOOL, BOOL, logPrintFunc*);

/**
 * Creates a file based logger object and installs the global logger callback function
 *
 * @param - UINT64 - IN - Size of string buffer in file logger. When the string buffer is full the logger will flush everything into a new file
 * @param - UINT64 - IN - Max number of log file. When exceeded, the oldest file will be deleted when new one is generated
 * @param - PCHAR - IN - Directory in which the log file will be generated
 * @param - BOOL - IN - Whether to print log to std out too
 * @param - BOOL - IN - Whether to set global logger function pointer
 * @param - BOOL - IN - Whether to enable logging other log levels into a file
 * @param - UINT32 - IN - Log level that needs to be filtered into another file
 * @param - logPrintFunc* - OUT/OPT - Optional function pointer to be returned to the caller that contains the main function for actual output
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createFileLoggerWithLevelFiltering(UINT64, UINT64, PCHAR, BOOL, BOOL, BOOL, UINT32, logPrintFunc*);

/**
 * Frees the static file logger object and resets the global logging function if it was
 * previously set by the create function.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freeFileLogger();

/*!@} */

/**
 * Helper macros to be used in pairs at the application start and end
 */
#define SET_FILE_LOGGER()                                                                                                                            \
    createFileLogger(FILE_LOGGER_STRING_BUFFER_SIZE, FILE_LOGGER_LOG_FILE_COUNT, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE, TRUE, NULL)
#define RESET_FILE_LOGGER() freeFileLogger()

/**
 * Flushes currentOffset number of chars from stringBuffer into logfile.
 * If maxFileCount is exceeded, the earliest file is deleted before writing to the new file.
 * After file_logger_flushToFile finishes, currentOffset is set to 0, whether the status of execution was success or not.
 *
 * @return - STATUS of execution
 */
STATUS file_logger_flushToFile();

/**
 * Helper macros to be used in pairs at the application start and end
 */
#define CREATE_DEFAULT_FILE_LOGGER()                                                                                                                 \
    createFileLogger(FILE_LOGGER_STRING_BUFFER_SIZE, FILE_LOGGER_LOG_FILE_COUNT, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE, TRUE, NULL);

#define RELEASE_FILE_LOGGER() freeFileLogger();

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_FILE_LOGGER_INCLUDE__ */
