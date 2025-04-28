/**
 * Memory heap public include file
 */
#ifndef __MEM_HEAP_INCLUDE_H__
#define __MEM_HEAP_INCLUDE_H__

#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>
#include <com/amazonaws/kinesis/video/utils/Include.h>

/**
 * We define minimal heap size as 1 MB
 */
#define MIN_HEAP_SIZE ((UINT64) 1 * 1024 * 1024)

/**
 * We define maximal heap size as 1TB
 */
#define MAX_HEAP_SIZE ((UINT64) 1 * 1024 * 1024 * 1024 * 1024)

/**
 * We define maximal heap size for file based allocations as 2^63 GB
 */
#define MAX_LARGE_HEAP_SIZE 0x7FFFFFFFFFFFFFFFULL

/**
 * Max possible individual allocation size. This is more of a hypothetical value.
 * The value will be used as a sentinel. The non-hybrid allocators will shift
 * the resulting handle at least two bits to the right and fill it with 1s to
 * ensure the hybrid allocator will know whether to use the direct or indirect heap.
 */
#define MAX_ALLOCATION_SIZE 0x0FFFFFFFFFFFFFFFULL

/**
 * Definition of the memory handle
 */
typedef UINT64 ALLOCATION_HANDLE;
typedef ALLOCATION_HANDLE* PALLOCATION_HANDLE;

/**
 * This is a sentinel indicating an invalid allocation handle value
 */
#ifndef INVALID_ALLOCATION_HANDLE_VALUE
#define INVALID_ALLOCATION_HANDLE_VALUE ((ALLOCATION_HANDLE) NULL)
#endif

#ifndef IS_VALID_ALLOCATION_HANDLE
#define IS_VALID_ALLOCATION_HANDLE(h) ((h) != INVALID_ALLOCATION_HANDLE_VALUE)
#endif

typedef enum {
    /**
     * No flags specified. Used as a sentinel
     */
    HEAP_FLAGS_NONE = 0x0,

    /**
     * Whether to use the AIV native heap allocator.
     */
    FLAGS_USE_AIV_HEAP = 0x1 << 0,

    /**
     * Whether to use the processes default heap allocator. The default is AIV native heap implementation
     */
    FLAGS_USE_SYSTEM_HEAP = 0x1 << 1,

    /**
     * Whether to use the hybrid heap allocator which combines system and vRam memory
     */
    FLAGS_USE_HYBRID_VRAM_HEAP = 0x1 << 2,

    /**
     * Whether to re-open the VRAM library. This fixes an FireOS4 bug for double-closing the so file
     */
    FLAGS_REOPEN_VRAM_LIBRARY = 0x1 << 3,

    /**
     * Whether to use the hybrid heap allocator which combined RAM-based heap and file based heap
     */
    FLAGS_USE_HYBRID_FILE_HEAP = 0x1 << 4,
} HEAP_BEHAVIOR_FLAGS;

/**
 * WARNING! This structure is the public facing structure
 * The actual implementation might have a larger size
 */
typedef struct {
    /**
     * The max size for the heap
     */
    UINT64 heapLimit;

    /**
     * Current heap size
     */
    UINT64 heapSize;

    /**
     * Number of items allocated
     */
    UINT64 numAlloc;
} Heap, *PHeap;

/**
 * Handle definition
 */
#ifndef HEAP_HANDLE_TO_POINTER
#define HEAP_HANDLE_TO_POINTER(h) ((PHeap) (h))
#endif

/**
 * Error values
 */
#define STATUS_HEAP_BASE                    0x10000000
#define STATUS_HEAP_FLAGS_ERROR             STATUS_HEAP_BASE + 0x00000001
#define STATUS_HEAP_NOT_INITIALIZED         STATUS_HEAP_BASE + 0x00000002
#define STATUS_HEAP_CORRUPTED               STATUS_HEAP_BASE + 0x00000003
#define STATUS_HEAP_VRAM_LIB_MISSING        STATUS_HEAP_BASE + 0x00000004
#define STATUS_HEAP_VRAM_LIB_REOPEN         STATUS_HEAP_BASE + 0x00000005
#define STATUS_HEAP_VRAM_INIT_FUNC_SYMBOL   STATUS_HEAP_BASE + 0x00000006
#define STATUS_HEAP_VRAM_ALLOC_FUNC_SYMBOL  STATUS_HEAP_BASE + 0x00000007
#define STATUS_HEAP_VRAM_FREE_FUNC_SYMBOL   STATUS_HEAP_BASE + 0x00000008
#define STATUS_HEAP_VRAM_LOCK_FUNC_SYMBOL   STATUS_HEAP_BASE + 0x00000009
#define STATUS_HEAP_VRAM_UNLOCK_FUNC_SYMBOL STATUS_HEAP_BASE + 0x0000000a
#define STATUS_HEAP_VRAM_UNINIT_FUNC_SYMBOL STATUS_HEAP_BASE + 0x0000000b
#define STATUS_HEAP_VRAM_GETMAX_FUNC_SYMBOL STATUS_HEAP_BASE + 0x0000000c
#define STATUS_HEAP_DIRECT_MEM_INIT         STATUS_HEAP_BASE + 0x0000000d
#define STATUS_HEAP_VRAM_INIT_FAILED        STATUS_HEAP_BASE + 0x0000000e
#define STATUS_HEAP_LIBRARY_FREE_FAILED     STATUS_HEAP_BASE + 0x0000000f
#define STATUS_HEAP_VRAM_ALLOC_FAILED       STATUS_HEAP_BASE + 0x00000010
#define STATUS_HEAP_VRAM_FREE_FAILED        STATUS_HEAP_BASE + 0x00000011
#define STATUS_HEAP_VRAM_MAP_FAILED         STATUS_HEAP_BASE + 0x00000012
#define STATUS_HEAP_VRAM_UNMAP_FAILED       STATUS_HEAP_BASE + 0x00000013
#define STATUS_HEAP_VRAM_UNINIT_FAILED      STATUS_HEAP_BASE + 0x00000014
#define STATUS_INVALID_ALLOCATION_SIZE      STATUS_HEAP_BASE + 0x00000015
#define STATUS_HEAP_REALLOC_ERROR           STATUS_HEAP_BASE + 0x00000016
#define STATUS_HEAP_FILE_HEAP_FILE_CORRUPT  STATUS_HEAP_BASE + 0x00000017

//////////////////////////////////////////////////////////////////////////
// Public functions
//////////////////////////////////////////////////////////////////////////
/**
 * Creates and initializes the heap
 */
PUBLIC_API STATUS heapInitialize(UINT64, UINT32, UINT32, PCHAR, PHeap*);

/**
 * Releases the entire heap.
 * IMPORTANT: Some heaps will leak memory if the allocations are not freed previously
 */
PUBLIC_API STATUS heapRelease(PHeap);

/**
 * Returns the current utilization size of the heap.
 */
PUBLIC_API STATUS heapGetSize(PHeap, PUINT64);

/**
 * Allocates memory from the heap
 */
PUBLIC_API STATUS heapAlloc(PHeap, UINT64, PALLOCATION_HANDLE);

/**
 * Frees allocated memory
 */
PUBLIC_API STATUS heapFree(PHeap, ALLOCATION_HANDLE);

/**
 * Gets the size of the allocation
 */
PUBLIC_API STATUS heapGetAllocSize(PHeap, ALLOCATION_HANDLE, PUINT64);

/**
 * Sets the size of the allocation and returns a new handle
 * NOTE: pHandle is IN/OUT param and will be updated on success.
 */
PUBLIC_API STATUS heapSetAllocSize(PHeap, PALLOCATION_HANDLE, UINT64);

/**
 * Maps the allocated handle and retrieves a memory address
 */
PUBLIC_API STATUS heapMap(PHeap, ALLOCATION_HANDLE, PVOID*, PUINT64);

/**
 * Un-maps the previously mapped memory
 */
PUBLIC_API STATUS heapUnmap(PHeap, PVOID);

/**
 * Debug validates/outputs information about the heap
 */
PUBLIC_API STATUS heapDebugCheckAllocator(PHeap, BOOL);

#ifdef __cplusplus
}
#endif

#endif // __MEM_HEAP_INCLUDE_H__
