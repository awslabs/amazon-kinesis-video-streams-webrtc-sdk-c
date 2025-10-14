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
#ifndef __AWS_KVS_WEBRTC_DIRECTORY_INCLUDE__
#define __AWS_KVS_WEBRTC_DIRECTORY_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "platform_esp32.h"
#include "error.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef enum { DIR_ENTRY_TYPE_FILE, DIR_ENTRY_TYPE_LINK, DIR_ENTRY_TYPE_DIRECTORY, DIR_ENTRY_TYPE_UNKNOWN } DIR_ENTRY_TYPES;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Callback function declaration.
 *
 * @UINT64 - the caller passed data
 * @DIR_ENTRY_TYPES - the type of the entry
 * @PCHAR - the full path of the entry
 * @PCHAR - the name of the entry
 */
typedef STATUS (*DirectoryEntryCallbackFunc)(UINT64, DIR_ENTRY_TYPES, PCHAR, PCHAR);

/**
 * Remove a directory - empty or not
 *
 * @PCHAR - directory path
 * @UINT64 - custom caller data passed to the callback
 * @BOOL - whether to iterate
 * @DirectoryEntryCallbackFunc - the callback function called with each entry
 */
PUBLIC_API STATUS traverseDirectory(PCHAR, UINT64, BOOL iterate, DirectoryEntryCallbackFunc);

// TODO: Non-implemented functions
/**
 * Remove a directory - empty or not
 *
 * @PCHAR - directory path
 */
PUBLIC_API STATUS removeDirectory(PCHAR);

/**
 * Gets the directory size
 *
 * @PCHAR - directory path
 * @PUINT64 - returned combined size
 */
PUBLIC_API STATUS getDirectorySize(PCHAR, PUINT64);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_DIRECTORY_INCLUDE__ */
