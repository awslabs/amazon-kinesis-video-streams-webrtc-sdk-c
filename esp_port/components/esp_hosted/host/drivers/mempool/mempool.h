/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include <string.h>
#include <stdio.h>
#include <sys/queue.h>
#include "os_wrapper.h"


#define MEMPOOL_OK                       0
#define MEMPOOL_FAIL                     -1


#define LOG                              printf

#define MEMPOOL_NAME_STR_SIZE            32

#define MEMPOOL_ALIGNMENT_BYTES          64
#define MEMPOOL_ALIGNMENT_MASK           (MEMPOOL_ALIGNMENT_BYTES-1)
#define IS_MEMPOOL_ALIGNED(VAL)          (!((VAL)& MEMPOOL_ALIGNMENT_MASK))
#define MEMPOOL_ALIGNED(VAL)             ((VAL) + MEMPOOL_ALIGNMENT_BYTES - \
                                             ((VAL)& MEMPOOL_ALIGNMENT_MASK))

#define MEMSET_REQUIRED                  1
#define MEMSET_NOT_REQUIRED              0


#ifdef CONFIG_USE_MEMPOOL
struct mempool_entry {
	SLIST_ENTRY(mempool_entry) entries;
};

typedef SLIST_HEAD(slisthead, mempool_entry) mempool_t;

struct mempool {
	mempool_t head;
	void * spinlock;
	uint32_t block_size;
};
#endif

struct mempool * mempool_create(uint32_t block_size);
void mempool_destroy(struct mempool* mp);
void * mempool_alloc(struct mempool* mp, int nbytes, int need_memset);
void mempool_free(struct mempool* mp, void *mem);
#endif
