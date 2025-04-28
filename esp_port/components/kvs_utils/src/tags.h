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
#ifndef __AWS_KVS_WEBRTC_TAGS_INCLUDE__
#define __AWS_KVS_WEBRTC_TAGS_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////
// Tags functionality
/////////////////////////////////////////

/**
 * Max tag count
 */
#define MAX_TAG_COUNT 50

/**
 * Max tag name length in chars
 */
#define MAX_TAG_NAME_LEN 128

/**
 * Max tag value length in chars
 */
#define MAX_TAG_VALUE_LEN 1024

/**
 * Defines the full tag structure length when the pointers to the strings are allocated after the
 * main struct. We will add 2 for NULL terminators
 */
#define TAG_FULL_LENGTH (SIZEOF(Tag) + (MAX_TAG_NAME_LEN + MAX_TAG_VALUE_LEN + 2) * SIZEOF(CHAR))

/**
 * Current version of the tag structure
 */
#define TAG_CURRENT_VERSION 0

/**
 * Tag declaration
 */
typedef struct __Tag Tag;
struct __Tag {
    // Version of the struct
    UINT32 version;

    // Tag name - null terminated
    PCHAR name; // pointer to a string with MAX_TAG_NAME_LEN chars max including the NULL terminator

    // Tag value - null terminated
    PCHAR value; // pointer to a string with MAX_TAG_VALUE_LEN chars max including the NULL terminator
};
typedef struct __Tag* PTag;

/**
 * Validates the tags
 *
 * @param 1 UINT32 - IN - Number of tags
 * @param 2 PTag - IN - Array of tags
 *
 * @return Status of the function call.
 */
STATUS validateTags(UINT32, PTag);

/**
 * Packages the tags in a provided buffer
 *
 * @param 1 UINT32 - IN - Number of tags
 * @param 2 PTag - IN - Array of tags
 * @param 3 UINT32 - IN - Buffer size
 * @param 4 PTag - IN/OUT - Buffer to package in
 * @param 5 PUINT32 - OUT/OPT - Real size of the bytes needed
 *
 * @return Status of the function call.
 */
STATUS packageTags(UINT32, PTag, UINT32, PTag, PUINT32);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_TAGS_INCLUDE__ */
