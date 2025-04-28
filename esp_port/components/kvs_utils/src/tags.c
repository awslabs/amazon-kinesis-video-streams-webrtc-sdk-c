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
#include "common_defs.h"
#include "error.h"
#include "platform_utils.h"
#include "tags.h"

/**
 * Validates the tags
 *
 * @param 1 UINT32 - Number of tags
 * @param 2 PTag - Array of tags
 *
 * @return Status of the function call.
 */
STATUS validateTags(UINT32 tagCount, PTag tags)
{
    UINT32 i;
    STATUS retStatus = STATUS_SUCCESS;

    CHK(tagCount <= MAX_TAG_COUNT, STATUS_UTIL_MAX_TAG_COUNT);

    // If we have tag count not 0 then tags can't be NULL
    CHK(tagCount == 0 || tags != NULL, STATUS_UTIL_TAGS_COUNT_NON_ZERO_TAGS_NULL);
    for (i = 0; i < tagCount; i++) {
        // Validate the tag version
        CHK(tags[i].version <= TAG_CURRENT_VERSION, STATUS_UTIL_INVALID_TAG_VERSION);

        // Validate the tag name
        CHK(STRNLEN(tags[i].name, MAX_TAG_NAME_LEN + 1) <= MAX_TAG_NAME_LEN, STATUS_UTIL_INVALID_TAG_NAME_LEN);

        // Validate the tag value
        CHK(STRNLEN(tags[i].value, MAX_TAG_VALUE_LEN + 1) <= MAX_TAG_VALUE_LEN, STATUS_UTIL_INVALID_TAG_VALUE_LEN);
    }

CleanUp:

    return retStatus;
}

/**
 * Packages the tags in to a destination buffer.
 * NOTE: The tags are assumed to have been validated
 */
STATUS packageTags(UINT32 tagCount, PTag pSrcTags, UINT32 tagsSize, PTag pDstTags, PUINT32 pSize)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i, curSize = tagCount * TAG_FULL_LENGTH, remaining = tagsSize, nameSize, valueSize, structSize;
    PBYTE pCurPtr;

    CHK(tagCount == 0 || pSrcTags != NULL, STATUS_UTIL_TAGS_COUNT_NON_ZERO_TAGS_NULL);

    // Quick check for anything to be done
    CHK(pDstTags != NULL && tagCount != 0, retStatus);

    structSize = SIZEOF(Tag) * tagCount;
    pCurPtr = (PBYTE) pDstTags + structSize;
    CHK(remaining >= structSize, STATUS_NOT_ENOUGH_MEMORY);
    remaining -= structSize;
    curSize = structSize;

    for (i = 0; i < tagCount; i++) {
        // Get the name and value lengths - those should have been validated already
        nameSize = (UINT32)(STRLEN(pSrcTags[i].name) + 1) * SIZEOF(CHAR);
        valueSize = (UINT32)(STRLEN(pSrcTags[i].value) + 1) * SIZEOF(CHAR);
        CHK(remaining >= nameSize + valueSize, STATUS_NOT_ENOUGH_MEMORY);

        pDstTags[i].version = pSrcTags[i].version;

        // Fix-up the pointers first then copy
        pDstTags[i].name = (PCHAR) pCurPtr;
        MEMCPY(pDstTags[i].name, pSrcTags[i].name, nameSize);
        pCurPtr += nameSize;

        pDstTags[i].value = (PCHAR) pCurPtr;
        MEMCPY(pDstTags[i].value, pSrcTags[i].value, valueSize);
        pCurPtr += valueSize;

        remaining -= nameSize + valueSize;
        curSize += nameSize + valueSize;
    }

    // Round-up the allocation size to the 64 bit
    curSize = ROUND_UP(curSize, 8);

CleanUp:

    if (pSize != NULL) {
        // Current size is always 64bit aligned whether we are calculating the max allocation or the actual allocation.
        *pSize = curSize;
    }

    return retStatus;
}