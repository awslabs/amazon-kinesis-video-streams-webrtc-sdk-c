/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mempool.h"
#include "esp_log.h"
#include "sdkconfig.h"

struct hosted_mempool {
	struct os_mempool *pool;
	uint8_t *heap;
	uint8_t static_heap;
	size_t num_blocks;
	size_t block_size;
	struct mempool_ops_t *ops;
};

const char *TAG = "HS_MP";

/* For Statically allocated memory, please pass as pre_allocated_mem.
 * If NULL passed, will allocate from heap
 */
struct hosted_mempool * hosted_mempool_create(void *pre_allocated_mem,
		size_t pre_allocated_mem_size, size_t num_blocks, size_t block_size)
{
#ifdef CONFIG_ESP_CACHE_MALLOC
	struct hosted_mempool *new = NULL;
	struct os_mempool *pool = NULL;
	uint8_t *heap = NULL;
	char str[MEMPOOL_NAME_STR_SIZE] = {0};

	if (!pre_allocated_mem) {
		/* no pre-allocated mem, allocate new */
		heap = (uint8_t *)MEM_ALLOC(MEMPOOL_ALIGNED(OS_MEMPOOL_BYTES(
						num_blocks,block_size)));
		if (!heap) {
			ESP_LOGE(TAG, "mempool create failed, no mem\n");
			return NULL;
		}
	} else {
		/* preallocated memory for mem pool */
		heap = pre_allocated_mem;
		if (pre_allocated_mem_size < num_blocks*block_size) {
			ESP_LOGE(TAG, "mempool create failed, insufficient mem\n");
			return NULL;
		}

		if (!IS_MEMPOOL_ALIGNED((unsigned long)pre_allocated_mem)) {
			ESP_LOGE(TAG, "mempool create failed, mempool start addr unaligned\n");
			return NULL;
		}
	}

	new = (struct hosted_mempool*)CALLOC(1, sizeof(struct hosted_mempool));
	pool = (struct os_mempool *)CALLOC(1, sizeof(struct os_mempool));

	if(!new || !pool) {
		goto free_buffs;
	}

	new->ops = os_mempool_get_ops();
	if (!new->ops) {
		goto free_buffs;
	}

	snprintf(str, MEMPOOL_NAME_STR_SIZE, "hosted_%p", pool);

	if (new->ops->mempool_init(pool, num_blocks, block_size, heap, str)) {
		ESP_LOGE(TAG, "mempool_init failed\n");
		goto free_buffs;
	}

	if (pre_allocated_mem)
		new->static_heap = 1;

	new->heap = heap;
	new->pool = pool;
	new->num_blocks = num_blocks;
	new->block_size = block_size;

#if MEMPOOL_DEBUG
	ESP_LOGI(MEM_TAG, "Create mempool %p with num_blk[%lu] blk_size:[%lu]", new->pool, new->num_blocks, new->block_size);
#endif

	return new;

free_buffs:
	FREE(new);
	FREE(pool);
	if (!pre_allocated_mem)
		FREE(heap);
	return NULL;
#else
	return NULL;
#endif
}

void hosted_mempool_destroy(struct hosted_mempool *mempool)
{
#ifdef CONFIG_ESP_CACHE_MALLOC
	if (!mempool)
		return;
#if MEMPOOL_DEBUG
	ESP_LOGI(MEM_TAG, "Destroy mempool %p num_blk[%lu] blk_size:[%lu]", mempool->pool, mempool->num_blocks, mempool->block_size);
#endif

	mempool->ops->mempool_unregister(mempool->pool);
	FREE(mempool->pool);

	if (!mempool->static_heap)
		FREE(mempool->heap);

	FREE(mempool);
#endif
}

void * hosted_mempool_alloc(struct hosted_mempool *mempool,
		size_t nbytes, uint8_t need_memset)
{
	void *mem = NULL;

#ifdef CONFIG_ESP_CACHE_MALLOC
	if (!mempool) {
		ESP_LOGE(TAG, "mempool %p is NULL", mempool);
		return NULL;
	}

#if MYNEWT_VAL(OS_MEMPOOL_CHECK)
	assert(mempool->heap);
	assert(mempool->pool);

	if(nbytes > mempool->block_size) {
		ESP_LOGE(TAG, "Exp alloc bytes[%u] > mempool block size[%u]\n",
				nbytes, mempool->block_size);
		return NULL;
	}
#endif

	mem = mempool->ops->memblock_get(mempool->pool);
#else
	mem = MEM_ALLOC(MEMPOOL_ALIGNED(nbytes));
#endif
	if (mem && need_memset)
		memset(mem, 0, nbytes);

	if (!mem) {
		ESP_LOGE(TAG, "mempool %p alloc failed nbytes[%u]", mempool, nbytes);
	}
	return mem;
}

int hosted_mempool_free(struct hosted_mempool *mempool, void *mem)
{
	if (!mem) {
		return 0;
	}
#ifdef CONFIG_ESP_CACHE_MALLOC
	if (!mempool) {
		ESP_LOGE(TAG, "%s: mempool %p is NULL", __func__, mempool);
		return MEMPOOL_FAIL;
	}

#if MYNEWT_VAL(OS_MEMPOOL_CHECK)
	assert(mempool->heap);
	assert(mempool->pool);
#endif

	return mempool->ops->memblock_put(mempool->pool, mem);
#else
	FREE(mem);
	return 0;
#endif
}
