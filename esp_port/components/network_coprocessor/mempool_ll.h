// SPDX-License-Identifier: Apache-2.0
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/**
 * @addtogroup OSKernel
 * @{
 *   @defgroup OSMempool Memory Pools
 *   @{
 */


#ifndef _OS_MEMPOOL_H_
#define _OS_MEMPOOL_H_

#include <stdbool.h>
#include "sys/queue.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#if CONFIG_ESP_CACHE_MALLOC
#ifdef __cplusplus
extern "C" {
#endif

#define MYNEWT_VAL_OS_MEMPOOL_POISON (0)
#define MYNEWT_VAL_OS_MEMPOOL_CHECK (0)

#define MYNEWT_VAL(_name)                       MYNEWT_VAL_ ## _name
#define MYNEWT_VAL_CHOICE(_name, _val)          MYNEWT_VAL_ ## _name ## __ ## _val


#ifndef min
#define min(a, b) ((a)<(b)?(a):(b))
#endif

#ifndef max
#define max(a, b) ((a)>(b)?(a):(b))
#endif

#define OS_ALIGN(__n, __a) (                             \
        (((__n) & ((__a) - 1)) == 0)                   ? \
            (__n)                                      : \
            ((__n) + ((__a) - ((__n) & ((__a) - 1))))    \
        )
#define OS_ALIGNMENT 4

#if 0
extern portMUX_TYPE hosted_port_mutex;
#endif
extern SemaphoreHandle_t hosted_port_mutex;
//critical section
#if 0
static inline uint32_t
hosted_mp_enter_critical(void)
{
    portENTER_CRITICAL(&hosted_port_mutex);
    return 0;
}

static inline void
hosted_mp_exit_critical(uint32_t ctx)
{
    portEXIT_CRITICAL(&hosted_port_mutex);

}
#endif

typedef uint32_t os_sr_t;

#if 0
#define OS_ENTER_CRITICAL(_sr) (_sr = hosted_mp_enter_critical())
#define OS_EXIT_CRITICAL(_sr) (hosted_mp_exit_critical(_sr))
#endif

#define OS_INIT_CRITICAL() (hosted_port_mutex = xSemaphoreCreateMutex())
#define OS_ENTER_CRITICAL() (xSemaphoreTake((hosted_port_mutex), portMAX_DELAY))
#define OS_EXIT_CRITICAL() (xSemaphoreGive((hosted_port_mutex)))

enum os_error {
    OS_OK = 0,
    OS_ENOMEM = 1,
    OS_EINVAL = 2,
    OS_INVALID_PARM = 3,
    OS_MEM_NOT_ALIGNED = 4,
    OS_BAD_MUTEX = 5,
    OS_TIMEOUT = 6,
    OS_ERR_IN_ISR = 7,      /* Function cannot be called from ISR */
    OS_ERR_PRIV = 8,        /* Privileged access error */
    OS_NOT_STARTED = 9,     /* OS must be started to call this function, but isn't */
    OS_ENOENT = 10,         /* No such thing */
    OS_EBUSY = 11,          /* Resource busy */
    OS_ERROR = 12,          /* Generic Error */
};

typedef enum os_error os_error_t;


/**
 * A memory block structure. This simply contains a pointer to the free list
 * chain and is only used when the block is on the free list. When the block
 * has been removed from the free list the entire memory block is usable by the
 * caller.
 */
struct os_memblock {
    SLIST_ENTRY(os_memblock) mb_next;
};

/* XXX: Change this structure so that we keep the first address in the pool? */
/* XXX: add memory debug structure and associated code */
/* XXX: Change how I coded the SLIST_HEAD here. It should be named:
   SLIST_HEAD(,os_memblock) mp_head; */

/**
 * Memory pool
 */
struct os_mempool {
    /** Size of the memory blocks, in bytes. */
    uint32_t mp_block_size;
    /** The number of memory blocks. */
    uint16_t mp_num_blocks;
    /** The number of free blocks left */
    uint16_t mp_num_free;
    /** The lowest number of free blocks seen */
    uint16_t mp_min_free;
    /** Bitmap of OS_MEMPOOL_F_[...] values. */
    uint8_t mp_flags;
    /** Address of memory buffer used by pool */
    uintptr_t mp_membuf_addr;
    STAILQ_ENTRY(os_mempool) mp_list;
    SLIST_HEAD(,os_memblock);
    /** Name for memory block */
    const char *name;
};

/**
 * Indicates an extended mempool.  Address can be safely cast to
 * (struct os_mempool_ext *).
 */
#define OS_MEMPOOL_F_EXT        0x01

struct os_mempool_ext;

/**
 * Block put callback function.  If configured, this callback gets executed
 * whenever a block is freed to the corresponding extended mempool.  Note: The
 * os_memblock_put() function calls this callback instead of freeing the block
 * itself.  Therefore, it is the callback's responsibility to free the block
 * via a call to os_memblock_put_from_cb().
 *
 * @param ome                   The extended mempool that a block is being
 *                                  freed back to.
 * @param data                  The block being freed.
 * @param arg                   Optional argument configured along with the
 *                                  callback.
 *
 * @return                      Indicates whether the block was successfully
 *                                  freed.  A non-zero value should only be
 *                                  returned if the block was not successfully
 *                                  released back to its pool.
 */
typedef os_error_t os_mempool_put_fn(struct os_mempool_ext *ome, void *data,
                                     void *arg);

struct os_mempool_ext {
    struct os_mempool mpe_mp;

    /* Callback that is executed immediately when a block is freed. */
    os_mempool_put_fn *mpe_put_cb;
    void *mpe_put_arg;
};

#define OS_MEMPOOL_INFO_NAME_LEN (32)

/**
 * Information describing a memory pool, used to return OS information
 * to the management layer.
 */
struct os_mempool_info {
    /** Size of the memory blocks in the pool */
    int omi_block_size;
    /** Number of memory blocks in the pool */
    int omi_num_blocks;
    /** Number of free memory blocks */
    int omi_num_free;
    /** Minimum number of free memory blocks ever */
    int omi_min_free;
    /** Name of the memory pool */
    char omi_name[OS_MEMPOOL_INFO_NAME_LEN];
};

/**
 * Get information about the next system memory pool.
 *
 * @param mempool The current memory pool, or NULL if starting iteration.
 * @param info    A pointer to the structure to return memory pool information
 *                into.
 *
 * @return The next memory pool in the list to get information about, or NULL
 *         when at the last memory pool.
 */
struct os_mempool *os_mempool_info_get_next(struct os_mempool *,
        struct os_mempool_info *);

/*
 * To calculate size of the memory buffer needed for the pool. NOTE: This size
 * is NOT in bytes! The size is the number of os_membuf_t elements required for
 * the memory pool.
 */
#if (OS_CFG_ALIGNMENT == OS_CFG_ALIGN_4)
#define OS_MEMPOOL_SIZE(n,blksize)      ((((blksize) + 3) / 4) * (n))
typedef uint32_t os_membuf_t;
#else
#define OS_MEMPOOL_SIZE(n,blksize)      ((((blksize) + 7) / 8) * (n))
typedef uint64_t os_membuf_t;
#endif

/** Calculates the number of bytes required to initialize a memory pool. */
#define OS_MEMPOOL_BYTES(n,blksize)     \
    (sizeof (os_membuf_t) * OS_MEMPOOL_SIZE((n), (blksize)))

/**
 * Initialize a memory pool.
 *
 * @param mp            Pointer to a pointer to a mempool
 * @param blocks        The number of blocks in the pool
 * @param blocks_size   The size of the block, in bytes.
 * @param membuf        Pointer to memory to contain blocks.
 * @param name          Name of the pool.
 *
 * @return os_error_t
 */
os_error_t os_mempool_init(struct os_mempool *mp, uint16_t blocks,
                           uint32_t block_size, void *membuf, const char *name);

/**
 * Initializes an extended memory pool.  Extended attributes (e.g., callbacks)
 * are not specified when this function is called; they are assigned manually
 * after initialization.
 *
 * @param mpe           The extended memory pool to initialize.
 * @param blocks        The number of blocks in the pool.
 * @param block_size    The size of each block, in bytes.
 * @param membuf        Pointer to memory to contain blocks.
 * @param name          Name of the pool.
 *
 * @return os_error_t
 */
os_error_t os_mempool_ext_init(struct os_mempool_ext *mpe, uint16_t blocks,
                               uint32_t block_size, void *membuf, const char *name);

/**
 * Clears a memory pool.
 *
 * @param mp            The mempool to clear.
 *
 * @return os_error_t
 */
os_error_t os_mempool_clear(struct os_mempool *mp);

/**
 * Clears an extended memory pool.
 *
 * @param mpe            The extended memory pool to clear.
 *
 * @return os_error_t
 */
os_error_t os_mempool_ext_clear(struct os_mempool_ext *mpe);

/**
 * Performs an integrity check of the specified mempool.  This function
 * attempts to detect memory corruption in the specified memory pool.
 *
 * @param mp                    The mempool to check.
 *
 * @return                      true if the memory pool passes the integrity
 *                                  check;
 *                              false if the memory pool is corrupt.
 */
bool os_mempool_is_sane(const struct os_mempool *mp);

/**
 * Checks if a memory block was allocated from the specified mempool.
 *
 * @param mp                    The mempool to check as parent.
 * @param block_addr            The memory block to check as child.
 *
 * @return                      0 if the block does not belong to the mempool;
 *                              1 if the block does belong to the mempool.
 */
int os_memblock_from(const struct os_mempool *mp, const void *block_addr);

/**
 * Get a memory block from a memory pool
 *
 * @param mp Pointer to the memory pool
 *
 * @return void* Pointer to block if available; NULL otherwise
 */
void *os_memblock_get(struct os_mempool *mp);

/**
 * Puts the memory block back into the pool, ignoring the put callback, if any.
 * This function should only be called from a put callback to free a block
 * without causing infinite recursion.
 *
 * @param mp Pointer to memory pool
 * @param block_addr Pointer to memory block
 *
 * @return os_error_t
 */
os_error_t os_memblock_put_from_cb(struct os_mempool *mp, void *block_addr);

/**
 * Puts the memory block back into the pool
 *
 * @param mp Pointer to memory pool
 * @param block_addr Pointer to memory block
 *
 * @return os_error_t
 */
os_error_t os_memblock_put(struct os_mempool *mp, void *block_addr);

#ifdef __cplusplus
}
#endif

#endif

#endif  /* _OS_MEMPOOL_H_ */


/**
 *   @} OSMempool
 * @} OSKernel
 */
