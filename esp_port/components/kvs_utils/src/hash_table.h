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
#ifndef __AWS_KVS_WEBRTC_HASH_TABLE_INCLUDE__
#define __AWS_KVS_WEBRTC_HASH_TABLE_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "error.h"
#include "common_defs.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
/**
 * Hash table declaration
 * NOTE: Variable size structure - the buckets follow directly after the main structure
 */
typedef struct {
    UINT32 itemCount;
    UINT32 bucketCount;
    UINT32 bucketLength;
    UINT32 flags;
    /*-- HashBucket[bucketCount] buckets; --*/
} HashTable, *PHashTable;

/**
 * Hash table entry declaration
 */
typedef struct {
    UINT64 key;
    UINT64 value;
} HashEntry, *PHashEntry;

/**
 * Internal Hash Table operations
 */
#define DEFAULT_HASH_TABLE_BUCKET_LENGTH    2
#define DEFAULT_HASH_TABLE_BUCKET_COUNT     10000
#define MIN_HASH_TABLE_ENTRIES_ALLOC_LENGTH 8

/**
 * Bucket declaration
 * NOTE: Variable size structure - the buckets can follow directly after the main structure
 * or in case of allocated array it's a separate allocation
 */
typedef struct {
    UINT32 count;
    UINT32 length;
    PHashEntry entries;
} HashBucket, *PHashBucket;

UINT64 getKeyHash(UINT64);
PHashBucket getHashBucket(PHashTable, UINT64);
/**
 * Minimum number of buckets
 */
#define MIN_HASH_BUCKET_COUNT 16

/**
 * Hash table iteration callback function.
 * IMPORTANT!
 * To terminate the iteration the caller must return
 * STATUS_HASH_ENTRY_ITERATION_ABORT
 *
 * @UINT64 - data that was passed to the iterate function
 * @PHashEntry - the entry to process
 */
typedef STATUS (*HashEntryCallbackFunc)(UINT64, PHashEntry);

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
/**
 * Create a new hash table with default parameters
 */
STATUS hashTableCreate(PHashTable*);

/**
 * Create a new hash table with specific parameters
 */
STATUS hashTableCreateWithParams(UINT32, UINT32, PHashTable*);

/**
 * Frees and de-allocates the hash table
 */
STATUS hashTableFree(PHashTable);

/**
 * Clears all the items and the buckets
 */
STATUS hashTableClear(PHashTable);

/**
 * Gets the number of items in the hash table
 */
STATUS hashTableGetCount(PHashTable, PUINT32);

/**
 * Whether the hash table is empty
 */
STATUS hashTableIsEmpty(PHashTable, PBOOL);

/**
 * Puts an item into the hash table
 */
STATUS hashTablePut(PHashTable, UINT64, UINT64);

/**
 * Upserts an item into the hash table
 */
STATUS hashTableUpsert(PHashTable, UINT64, UINT64);

/**
 * Gets an item from the hash table
 */
STATUS hashTableGet(PHashTable, UINT64, PUINT64);

/**
 * Checks whether an item exists in the hash table
 */
STATUS hashTableContains(PHashTable, UINT64, PBOOL);

/**
 * Removes an item from the hash table. If the bucket is empty it's deleted. The existing items will be shifted.
 */
STATUS hashTableRemove(PHashTable, UINT64);

/**
 * Gets the number of buckets
 */
STATUS hashTableGetBucketCount(PHashTable, PUINT32);

/**
 * Gets all the entries from the hash table
 */
STATUS hashTableGetAllEntries(PHashTable, PHashEntry, PUINT32);

/**
 * Iterates over the hash entries. No predefined order
 */
STATUS hashTableIterateEntries(PHashTable, UINT64, HashEntryCallbackFunc);

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_HASH_TABLE_INCLUDE__ */
