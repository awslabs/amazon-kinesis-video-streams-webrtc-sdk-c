#include "Include_i.h"

/**
 * Create a new hash table with default parameters
 */
STATUS hashTableCreate(PHashTable* ppHashTable)
{
    return hashTableCreateWithParams(DEFAULT_HASH_TABLE_BUCKET_COUNT, DEFAULT_HASH_TABLE_BUCKET_LENGTH, ppHashTable);
}
/**
 * Create a new hash table with specific parameters
 */
STATUS hashTableCreateWithParams(UINT32 bucketCount, UINT32 bucketLength, PHashTable* ppHashTable)
{
    STATUS retStatus = STATUS_SUCCESS;
    PHashTable pHashTable = NULL;
    UINT32 allocSize;
    UINT32 entryAllocSize;
    UINT32 bucketAllocSize;
    UINT32 i;
    PHashBucket pHashBucket;
    PHashEntry pHashEntry;

    CHK(bucketCount >= MIN_HASH_BUCKET_COUNT && bucketLength > 0 && ppHashTable != NULL, STATUS_NULL_ARG);

    // Pre-set the default
    *ppHashTable = NULL;

    // Calculate the size of the main allocation
    entryAllocSize = SIZEOF(HashEntry) * bucketLength;
    bucketAllocSize = SIZEOF(HashBucket) * bucketCount;
    allocSize = SIZEOF(HashTable) + bucketAllocSize + entryAllocSize * bucketCount;

    // Allocate the main structure
    pHashTable = (PHashTable) MEMCALLOC(1, allocSize);
    CHK(pHashTable != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // Set the values. NOTE: The buckets follow immediately after the main struct
    pHashTable->bucketCount = bucketCount;
    pHashTable->bucketLength = bucketLength;
    pHashTable->itemCount = 0;

    // Set the bucket pointers
    // NOTE: the buckets have been NULL-ed by calloc.
    // The buckets follow immediately after the hash table
    // The entries follow immediately after the buckets array
    pHashBucket = (PHashBucket) (pHashTable + 1);
    pHashEntry = (PHashEntry) (((PBYTE) pHashBucket) + bucketAllocSize);

    for (i = 0; i < bucketCount; i++) {
        pHashBucket->count = 0;
        pHashBucket->length = bucketLength;
        pHashBucket->entries = pHashEntry;

        pHashBucket++;
        pHashEntry += bucketLength;
    }

    // Finally, set the return value
    *ppHashTable = pHashTable;

CleanUp:

    // Clean-up on error
    if (STATUS_FAILED(retStatus)) {
        // Free everything
        hashTableFree(pHashTable);
    }

    return retStatus;
}

/**
 * Frees and de-allocates the hash table
 */
STATUS hashTableFree(PHashTable pHashTable)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;
    PHashBucket pHashBucket;

    // The call is idempotent so we shouldn't fail
    CHK(pHashTable != NULL, retStatus);

    // We shouldn't fail here even if clear fails
    hashTableClear(pHashTable);

    // Free the buckets
    pHashBucket = (PHashBucket) (pHashTable + 1);
    for (i = 0; i < pHashTable->bucketCount; i++) {
        // Check if the entries have been allocated by comparing the length against the original bucketLength
        if (pHashBucket[i].length != pHashTable->bucketLength) {
            MEMFREE(pHashBucket[i].entries);
        }
    }

    // Free the structure itself
    MEMFREE(pHashTable);

CleanUp:

    return retStatus;
}

/**
 * Clears all the items and the buckets
 */
STATUS hashTableClear(PHashTable pHashTable)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 i;
    PHashBucket pHashBucket;

    CHK(pHashTable != NULL, STATUS_NULL_ARG);

    // Iterate through and clear buckets.
    // NOTE: This doesn't de-allocate the buckets
    pHashBucket = (PHashBucket) (pHashTable + 1);
    for (i = 0; i < pHashTable->bucketCount; i++) {
        pHashBucket[i].count = 0;
    }

    // Reset the table
    pHashTable->itemCount = 0;

CleanUp:

    return retStatus;
}

/**
 * Gets the number of items in the hash table
 */
STATUS hashTableGetCount(PHashTable pHashTable, PUINT32 pItemCount)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pHashTable != NULL && pItemCount != NULL, STATUS_NULL_ARG);

    *pItemCount = pHashTable->itemCount;

CleanUp:

    return retStatus;
}

/**
 * Gets the number of buckets in the hash table
 */
STATUS hashTableGetBucketCount(PHashTable pHashTable, PUINT32 pBucketCount)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pHashTable != NULL && pBucketCount != NULL, STATUS_NULL_ARG);

    *pBucketCount = pHashTable->bucketCount;

CleanUp:

    return retStatus;
}

/**
 * Whether the hash table is empty
 */
STATUS hashTableIsEmpty(PHashTable pHashTable, PBOOL pIsEmpty)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pHashTable != NULL && pIsEmpty != NULL, STATUS_NULL_ARG);

    *pIsEmpty = (pHashTable->itemCount == 0);

CleanUp:

    return retStatus;
}

/**
 * Checks whether an item exists in the hash table
 */
STATUS hashTableContains(PHashTable pHashTable, UINT64 key, PBOOL pContains)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT64 value;

    CHK(pContains != NULL, STATUS_NULL_ARG);
    retStatus = hashTableGet(pHashTable, key, &value);
    CHK(retStatus == STATUS_HASH_KEY_NOT_PRESENT || retStatus == STATUS_SUCCESS, retStatus);

    // The return is status success if the item was retrieved OK
    *pContains = (retStatus == STATUS_SUCCESS);

    // Reset the status
    retStatus = STATUS_SUCCESS;

CleanUp:

    return retStatus;
}

/**
 * Gets an item from the hash table
 */
STATUS hashTableGet(PHashTable pHashTable, UINT64 key, PUINT64 pValue)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL found = FALSE;
    PHashBucket pHashBucket;
    PHashEntry pHashEntry;
    UINT32 i;

    CHK(pHashTable != NULL && pValue != NULL, STATUS_NULL_ARG);

    // Get the bucket
    pHashBucket = getHashBucket(pHashTable, key);
    CHK(pHashBucket != NULL, STATUS_INTERNAL_ERROR);

    // Find the item if any matching the key
    pHashEntry = pHashBucket->entries;
    for (i = 0; i < pHashBucket->count; i++, pHashEntry++) {
        if (pHashEntry->key == key) {
            *pValue = pHashEntry->value;
            found = TRUE;
            break;
        }
    }

    CHK(found, STATUS_HASH_KEY_NOT_PRESENT);

CleanUp:

    return retStatus;
}

/**
 * Puts an item into the hash table
 */
STATUS hashTablePut(PHashTable pHashTable, UINT64 key, UINT64 value)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL contains = FALSE;

    // Check if the item exists and bail out if it does
    CHK_STATUS(hashTableContains(pHashTable, key, &contains));
    CHK(!contains, STATUS_HASH_KEY_ALREADY_PRESENT);

    // Perform an upsert
    CHK_STATUS(hashTableUpsert(pHashTable, key, value));

CleanUp:

    return retStatus;
}

/**
 * Upserts an item into the hash table
 */
STATUS hashTableUpsert(PHashTable pHashTable, UINT64 key, UINT64 value)
{
    STATUS retStatus = STATUS_SUCCESS;
    PHashBucket pHashBucket;
    PHashEntry pNewHashEntry;
    PHashEntry pHashEntry;
    UINT32 i, allocSize, entriesLength;

    CHK(pHashTable != NULL, STATUS_NULL_ARG);

    // Get the bucket and the entries
    pHashBucket = getHashBucket(pHashTable, key);
    CHK(pHashBucket != NULL, STATUS_INTERNAL_ERROR);
    pHashEntry = pHashBucket->entries;

    // Check if we already have the value
    for (i = 0; i < pHashBucket->count; i++, pHashEntry++) {
        if (pHashEntry->key == key) {
            // Found the entry - update and early success return
            pHashEntry->value = value;
            CHK(FALSE, retStatus);
        }
    }

    // Check if we need to increase the size of the bucket
    if (pHashBucket->count == pHashBucket->length) {
        // Allocate twice larger array or a min allocation whichever is greater and copy items over
        entriesLength = MAX(pHashBucket->length * 2, MIN_HASH_TABLE_ENTRIES_ALLOC_LENGTH);
        allocSize = SIZEOF(HashEntry) * entriesLength;
        pNewHashEntry = (PHashEntry) MEMALLOC(allocSize);
        CHK(pNewHashEntry != NULL, STATUS_NOT_ENOUGH_MEMORY);

        pHashEntry = pHashBucket->entries;

        // Copy the values over
        MEMCPY(pNewHashEntry, pHashEntry, SIZEOF(HashEntry) * pHashBucket->count);

        // Free the old entry allocation if it was allocated
        if (pHashBucket->length != pHashTable->bucketLength) {
            MEMFREE(pHashEntry);
        }

        // Set the bucket length
        pHashBucket->length = entriesLength;

        // Set the allocation
        pHashBucket->entries = pNewHashEntry;
    }

    // Put the value and increment the count
    pHashBucket->entries[pHashBucket->count].key = key;
    pHashBucket->entries[pHashBucket->count].value = value;
    pHashBucket->count++;

    // Increment the item count
    pHashTable->itemCount++;

CleanUp:

    return retStatus;
}

/**
 * Removes an item from the hash table. If the bucket is empty it's deleted. The existing items will be shifted.
 */
STATUS hashTableRemove(PHashTable pHashTable, UINT64 key)
{
    STATUS retStatus = STATUS_SUCCESS;
    BOOL found = FALSE;
    PHashBucket pHashBucket;
    PHashEntry pHashEntry;
    UINT32 i;

    CHK(pHashTable != NULL, STATUS_NULL_ARG);

    // Get the bucket
    pHashBucket = getHashBucket(pHashTable, key);
    CHK(pHashBucket != NULL, STATUS_INTERNAL_ERROR);

    // Check if we already have the value
    pHashEntry = pHashBucket->entries;
    for (i = 0; !found && i < pHashBucket->count; i++) {
        if (pHashEntry->key == key) {
            found = TRUE;
        } else {
            pHashEntry++;
        }
    }

    CHK(found, STATUS_HASH_KEY_NOT_PRESENT);

    // Move the rest of the items
    MEMMOVE(pHashEntry, pHashEntry + 1, (pHashBucket->count - i) * SIZEOF(HashEntry));

    // Decrement the count as we have removed and item
    pHashBucket->count--;

    // Decrement the count of items
    pHashTable->itemCount--;

CleanUp:

    return retStatus;
}

/**
 * Gets all the entries from the hash table
 */
STATUS hashTableGetAllEntries(PHashTable pHashTable, PHashEntry pHashEntries, PUINT32 pHashCount)
{
    STATUS retStatus = STATUS_SUCCESS;
    PHashBucket pHashBucket;
    PHashEntry pHashEntry;
    UINT32 bucketIndex;

    CHK(pHashTable != NULL && pHashCount != NULL, STATUS_NULL_ARG);

    // See if we are looking for the count only or if we have no elements
    if (pHashEntries == NULL || pHashTable->itemCount == 0) {
        // Early return
        CHK(FALSE, retStatus);
    }

    // Check if the size is big enough
    CHK(*pHashCount >= pHashTable->itemCount, STATUS_BUFFER_TOO_SMALL);

    pHashEntry = pHashEntries;

    // Copy the items into the array
    pHashBucket = (PHashBucket) (pHashTable + 1);
    for (bucketIndex = 0; bucketIndex < pHashTable->bucketCount; bucketIndex++) {
        if (pHashBucket[bucketIndex].count != 0) {
            // Copy into the array
            MEMCPY(pHashEntry, pHashBucket[bucketIndex].entries, pHashBucket[bucketIndex].count * SIZEOF(HashEntry));

            // Move the pointer
            pHashEntry += pHashBucket[bucketIndex].count;
        }
    }

CleanUp:

    if (STATUS_SUCCEEDED(retStatus)) {
        // Set the count
        *pHashCount = pHashTable->itemCount;
    }

    return retStatus;
}

/**
 * Gets all the entries from the hash table
 */
STATUS hashTableIterateEntries(PHashTable pHashTable, UINT64 callerData, HashEntryCallbackFunc hashEntryFn)
{
    STATUS retStatus = STATUS_SUCCESS;
    PHashBucket pHashBucket;
    PHashEntry pHashEntry;
    UINT32 bucketIndex, entryIndex;

    CHK(pHashTable != NULL && hashEntryFn != NULL, STATUS_NULL_ARG);

    // Iterate over buckets and entries
    pHashBucket = (PHashBucket) (pHashTable + 1);
    for (bucketIndex = 0; bucketIndex < pHashTable->bucketCount; bucketIndex++) {
        if (pHashBucket[bucketIndex].count != 0) {
            // Iterate and call the callback function on entries
            pHashEntry = pHashBucket[bucketIndex].entries;
            for (entryIndex = 0; entryIndex < pHashBucket[bucketIndex].count; entryIndex++, pHashEntry++) {
                retStatus = hashEntryFn(callerData, pHashEntry);

                // Check if there was an error
                CHK(retStatus == STATUS_HASH_ENTRY_ITERATION_ABORT || retStatus == STATUS_SUCCESS, retStatus);

                // Check if we need to abort
                CHK(retStatus != STATUS_HASH_ENTRY_ITERATION_ABORT, STATUS_SUCCESS);
            }
        }
    }

CleanUp:

    return retStatus;
}

/////////////////////////////////////////////////////////////////////////////////
// Internal operations
/////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the bucket for the key
 */
PHashBucket getHashBucket(PHashTable pHashTable, UINT64 key)
{
    PHashBucket pHashBucket = (PHashBucket) (pHashTable + 1);
    UINT32 index = getKeyHash(key) % pHashTable->bucketCount;

    return &pHashBucket[index];
}

/**
 * Calculates the hash of a key.
 * NOTE: This implementation uses public domain FNV-1a hash
 * https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
 */
UINT64 getKeyHash(UINT64 key)
{
    // set to FNV_offset_basis
    UINT64 hash = 0xcbf29ce484222325;
    UINT32 i;

    for (i = 0; i < SIZEOF(UINT64); i++) {
        // XOR hash with a byte of data and multiply with FNV prime
        hash ^= (key >> i * 8) & 0x00000000000000ff;
        hash *= 0x100000001b3;
    }

    return hash;
}
