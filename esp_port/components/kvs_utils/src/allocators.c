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
#include "allocators.h"

#include <inttypes.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#if CONFIG_SPIRAM_BOOT_INIT
#define ALLOC_EXT   1
#else
#define ALLOC_EXT   0
#endif

static const char *TAG = "allocators";
//
// Default allocator functions
//
PVOID defaultMemAlloc(SIZE_T size)
{
#if ALLOC_EXT
    void *ptr =  heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    return ptr;
#else
    return malloc(size);
#endif
}

PVOID defaultMemAlignAlloc(SIZE_T size, SIZE_T alignment)
{
#if defined(__MACH__)
    // On Mac allocations are 16 byte aligned. There is hardly an equivalent anyway
    UNUSED_PARAM(alignment);
    return malloc(size);
#elif defined(_MSC_VER) || defined(__MINGW64__) || defined(__MINGW32__)
    return _aligned_malloc(size, alignment);
#else
#if ALLOC_EXT
    return heap_caps_aligned_alloc(alignment, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
#else
    return memalign(size, alignment);
#endif // PSRAM
#endif
}

PVOID defaultMemCalloc(SIZE_T num, SIZE_T size)
{
#if ALLOC_EXT
    void *ptr = heap_caps_calloc(num, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
    return ptr;
#else
    return calloc(num, size);
#endif
}

PVOID defaultMemRealloc(PVOID ptr, SIZE_T size)
{
#if ALLOC_EXT
    return heap_caps_realloc(ptr, size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
#else
    return realloc(ptr, size);
#endif
}

VOID defaultMemFree(VOID* ptr)
{
    free(ptr);
}

memAlloc globalMemAlloc = defaultMemAlloc;
memAlignAlloc globalMemAlignAlloc = defaultMemAlignAlloc;
memCalloc globalMemCalloc = defaultMemCalloc;
memRealloc globalMemRealloc = defaultMemRealloc;
memFree globalMemFree = defaultMemFree;

VOID dumpMemoryHex(PVOID pMem, UINT32 size)
{
#ifdef LOG_STREAMING
    DLOGS("============================================");
    DLOGS("Dumping memory: %p, size: %d", pMem, size);
    DLOGS("++++++++++++++++++++++++++++++++++++++++++++");

    CHAR buf[256];
    PCHAR pCur = buf;
    PBYTE pByte = (PBYTE) pMem;
    for (UINT32 i = 0; i < size; i++) {
        SPRINTF(pCur, "%02x ", *pByte++);
        pCur += 3;
        if ((i + 1) % 16 == 0) {
            DLOGS("%s", buf);
            buf[0] = '\0';
            pCur = buf;
        }
    }

    DLOGS("++++++++++++++++++++++++++++++++++++++++++++");
    DLOGS("Dumping memory done!");
    DLOGS("============================================");
#else
    UNUSED_PARAM(pMem);
    UNUSED_PARAM(size);
#endif
}

BOOL checkBufferValues(PVOID ptr, BYTE val, SIZE_T size)
{
    SIZE_T i;
    PBYTE pBuf = (PBYTE) ptr;

    if (pBuf == NULL) {
        return FALSE;
    }

    for (i = 0; i < size; pBuf++, i++) {
        if (*pBuf != val) {
            return FALSE;
        }
    }

    return TRUE;
}

memChk globalMemChk = checkBufferValues;

void print_mem_stats(const char *tag)
{
    uint32_t freeSize = esp_get_free_heap_size();
    const char *log_tag = tag ? tag : TAG;
    ESP_LOGI(log_tag, "The available total size of heap:%" PRIu32 , freeSize);

    printf("\tDescription\tInternal\tSPIRAM\n");
    printf("Current Free Memory\t%d\t\t%d\n",
           heap_caps_get_free_size(MALLOC_CAP_8BIT) - heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
           heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("Largest Free Block\t%d\t\t%d\n",
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    printf("Min. Ever Free Size\t%d\t\t%d\n",
           heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
           heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
}
