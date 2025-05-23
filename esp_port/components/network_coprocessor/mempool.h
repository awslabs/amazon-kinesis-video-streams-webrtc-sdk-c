// SPDX-License-Identifier: Apache-2.0
// Copyright 2015-2022 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

#ifdef CONFIG_ESP_CACHE_MALLOC
#include "mempool_ll.h"
struct hosted_mempool {
	struct os_mempool *pool;
	uint8_t *heap;
	uint8_t static_heap;
	size_t num_blocks;
	size_t block_size;
};
#endif

#define MEM_DUMP(s) \
    printf("%s free:%lu min-free:%lu lfb-def:%u lfb-8bit:%u\n\n", s, \
                  esp_get_free_heap_size(), esp_get_minimum_free_heap_size(), \
                  heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT),\
                  heap_caps_get_largest_free_block(MALLOC_CAP_8BIT))

#define MEMPOOL_OK                       0
#define MEMPOOL_FAIL                     -1

#define CALLOC(x,y)                      calloc(x,y)
#define MALLOC(x)                        malloc(x)
#define MEM_ALLOC(x)                     heap_caps_malloc(x, MALLOC_CAP_DMA)
#define FREE(x) do {                     \
	if (x) {                             \
		free(x);                         \
		x = NULL;                        \
	}                                    \
} while(0);

#define MEMPOOL_NAME_STR_SIZE            32

#define MEMPOOL_ALIGNMENT_BYTES          4
#define MEMPOOL_ALIGNMENT_MASK           (MEMPOOL_ALIGNMENT_BYTES-1)
#define IS_MEMPOOL_ALIGNED(VAL)          (!((VAL)& MEMPOOL_ALIGNMENT_MASK))
#define MEMPOOL_ALIGNED(VAL)             ((VAL) + MEMPOOL_ALIGNMENT_BYTES - \
                                             ((VAL)& MEMPOOL_ALIGNMENT_MASK))

#define MEMSET_REQUIRED                  1
#define MEMSET_NOT_REQUIRED              0

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
  #define ESP_MUTEX_INIT(mUtEx) portMUX_INITIALIZE(&(mUtEx));
#else
  #define ESP_MUTEX_INIT(mUtEx) vPortCPUInitializeMutex(&(mUtEx));
#endif


struct hosted_mempool * hosted_mempool_create(void *pre_allocated_mem,
		size_t pre_allocated_mem_size, size_t num_blocks, size_t block_size);
void hosted_mempool_destroy(struct hosted_mempool *mempool);
void * hosted_mempool_alloc(struct hosted_mempool *mempool,
		size_t nbytes, uint8_t need_memset);
int hosted_mempool_free(struct hosted_mempool *mempool, void *mem);

#endif
