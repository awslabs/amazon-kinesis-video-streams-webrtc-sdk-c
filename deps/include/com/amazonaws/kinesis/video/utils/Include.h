/**
 * Various utility functionality
 */
#ifndef __UTILS_H__
#define __UTILS_H__

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>

#define MAX_STRING_CONVERSION_BASE 36

// Max path characters as defined in linux/limits.h
#define MAX_PATH_LEN 4096

// thread stack size to use when running on constrained device like raspberry pi
#define THREAD_STACK_SIZE_ON_CONSTRAINED_DEVICE (512 * 1024)

// Check for whitespace
#define IS_WHITE_SPACE(ch) (((ch) == ' ') || ((ch) == '\t') || ((ch) == '\r') || ((ch) == '\n') || ((ch) == '\v') || ((ch) == '\f'))

/**
 * EMA (Exponential Moving Average) alpha value and 1-alpha value - over appx 20 samples
 */
#define EMA_ALPHA_VALUE           ((DOUBLE) 0.05)
#define ONE_MINUS_EMA_ALPHA_VALUE ((DOUBLE) (1 - EMA_ALPHA_VALUE))

/**
 * Calculates the EMA (Exponential Moving Average) accumulator value
 *
 * a - Accumulator value
 * v - Next sample point
 *
 * @return the new Accumulator value
 */
#define EMA_ACCUMULATOR_GET_NEXT(a, v) (DOUBLE)(EMA_ALPHA_VALUE * (v) + ONE_MINUS_EMA_ALPHA_VALUE * (a))

/**
 * Error values
 */
#define STATUS_UTILS_BASE                            0x40000000
#define STATUS_INVALID_BASE64_ENCODE                 STATUS_UTILS_BASE + 0x00000001
#define STATUS_INVALID_BASE                          STATUS_UTILS_BASE + 0x00000002
#define STATUS_INVALID_DIGIT                         STATUS_UTILS_BASE + 0x00000003
#define STATUS_INT_OVERFLOW                          STATUS_UTILS_BASE + 0x00000004
#define STATUS_EMPTY_STRING                          STATUS_UTILS_BASE + 0x00000005
#define STATUS_DIRECTORY_OPEN_FAILED                 STATUS_UTILS_BASE + 0x00000006
#define STATUS_PATH_TOO_LONG                         STATUS_UTILS_BASE + 0x00000007
#define STATUS_UNKNOWN_DIR_ENTRY_TYPE                STATUS_UTILS_BASE + 0x00000008
#define STATUS_REMOVE_DIRECTORY_FAILED               STATUS_UTILS_BASE + 0x00000009
#define STATUS_REMOVE_FILE_FAILED                    STATUS_UTILS_BASE + 0x0000000a
#define STATUS_REMOVE_LINK_FAILED                    STATUS_UTILS_BASE + 0x0000000b
#define STATUS_DIRECTORY_ACCESS_DENIED               STATUS_UTILS_BASE + 0x0000000c
#define STATUS_DIRECTORY_MISSING_PATH                STATUS_UTILS_BASE + 0x0000000d
#define STATUS_DIRECTORY_ENTRY_STAT_ERROR            STATUS_UTILS_BASE + 0x0000000e
#define STATUS_STRFTIME_FALIED                       STATUS_UTILS_BASE + 0x0000000f
#define STATUS_MAX_TIMESTAMP_FORMAT_STR_LEN_EXCEEDED STATUS_UTILS_BASE + 0x00000010
#define STATUS_UTIL_MAX_TAG_COUNT                    STATUS_UTILS_BASE + 0x00000011
#define STATUS_UTIL_INVALID_TAG_VERSION              STATUS_UTILS_BASE + 0x00000012
#define STATUS_UTIL_TAGS_COUNT_NON_ZERO_TAGS_NULL    STATUS_UTILS_BASE + 0x00000013
#define STATUS_UTIL_INVALID_TAG_NAME_LEN             STATUS_UTILS_BASE + 0x00000014
#define STATUS_UTIL_INVALID_TAG_VALUE_LEN            STATUS_UTILS_BASE + 0x00000015
#define STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE     STATUS_UTILS_BASE + 0x0000002a
#define STATUS_EXPONENTIAL_BACKOFF_RETRIES_EXHAUSTED STATUS_UTILS_BASE + 0x0000002b
#define STATUS_THREADPOOL_MAX_COUNT                  STATUS_UTILS_BASE + 0x0000002c
#define STATUS_THREADPOOL_INTERNAL_ERROR             STATUS_UTILS_BASE + 0x0000002d

/**
 * Base64 encode/decode functionality
 */
PUBLIC_API STATUS base64Encode(PVOID, UINT32, PCHAR, PUINT32);
PUBLIC_API STATUS base64Decode(PCHAR, UINT32, PBYTE, PUINT32);

/**
 * Hex encode/decode functionality
 */
PUBLIC_API STATUS hexEncode(PVOID, UINT32, PCHAR, PUINT32);
PUBLIC_API STATUS hexEncodeCase(PVOID, UINT32, PCHAR, PUINT32, BOOL);
PUBLIC_API STATUS hexDecode(PCHAR, UINT32, PBYTE, PUINT32);

/**
 * Integer to string conversion routines
 */
PUBLIC_API STATUS ulltostr(UINT64, PCHAR, UINT32, UINT32, PUINT32);
PUBLIC_API STATUS ultostr(UINT32, PCHAR, UINT32, UINT32, PUINT32);

/**
 * String to integer conversion routines. NOTE: The base is in [2-36]
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - Pointer to the end of the string. If NULL, the NULL terminator would be used
 * @param 3 - IN - Base of the number (10 - for decimal)
 * @param 4 - OUT - The resulting value
 */
PUBLIC_API STATUS strtoui32(PCHAR, PCHAR, UINT32, PUINT32);
PUBLIC_API STATUS strtoui64(PCHAR, PCHAR, UINT32, PUINT64);
PUBLIC_API STATUS strtoi32(PCHAR, PCHAR, UINT32, PINT32);
PUBLIC_API STATUS strtoi64(PCHAR, PCHAR, UINT32, PINT64);

/**
 * Safe variant of strchr
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - IN - The character to look for
 *
 * @return - Pointer to the first occurrence or NULL
 */
PUBLIC_API PCHAR strnchr(PCHAR, UINT32, CHAR);

/**
 * Safe variant of strstr. This is a default implementation for strnstr when not available.
 *
 * @param 1 - IN - Input string to process
 * @param 3 - IN - The string to look for
 * @param 2 - IN - String length.
 *
 * @return - Pointer to the first occurrence or NULL
 */
PUBLIC_API PCHAR defaultStrnstr(PCHAR, PCHAR, SIZE_T);

/**
 * Left and right trim of the whitespace
 *
 * @param 1 - IN - Input string to process
 * @param 2 - IN/OPT - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 and 4 - OUT - The pointer to the trimmed start and/or end
 *
 * @return Status of the operation
 */
PUBLIC_API STATUS ltrimstr(PCHAR, UINT32, PCHAR*);
PUBLIC_API STATUS rtrimstr(PCHAR, UINT32, PCHAR*);
PUBLIC_API STATUS trimstrall(PCHAR, UINT32, PCHAR*, PCHAR*);

/**
 * To lower and to upper string conversion
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return Status of the operation
 */
PUBLIC_API STATUS tolowerstr(PCHAR, UINT32, PCHAR);
PUBLIC_API STATUS toupperstr(PCHAR, UINT32, PCHAR);

/**
 * To lower/upper string conversion internal function
 *
 * @param 1 - IN - Input string to convert
 * @param 2 - IN - String length. 0 if NULL terminated and the length is calculated.
 * @param 3 - INT - Whether to upper (TRUE) or to lower (FALSE)
 * @param 4 - OUT - The pointer to the converted string - can be pointing to the same location. Size should be enough
 *
 * @return Status of the operation
 */
STATUS tolowerupperstr(PCHAR, UINT32, BOOL, PCHAR);

/**
 * File I/O functionality
 */
PUBLIC_API STATUS readFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, PUINT64 pSize);
PUBLIC_API STATUS readFileSegment(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 readSize);
PUBLIC_API STATUS writeFile(PCHAR filePath, BOOL binMode, BOOL append, PBYTE pBuffer, UINT64 size);
PUBLIC_API STATUS updateFile(PCHAR filePath, BOOL binMode, PBYTE pBuffer, UINT64 offset, UINT64 size);
PUBLIC_API STATUS getFileLength(PCHAR filePath, PUINT64 pSize);
PUBLIC_API STATUS setFileLength(PCHAR filePath, UINT64 size);
PUBLIC_API STATUS fileExists(PCHAR filePath, PBOOL pExists);
PUBLIC_API STATUS createFile(PCHAR filePath, UINT64 size);

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
PUBLIC_API STATUS validateTags(UINT32, PTag);

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
PUBLIC_API STATUS packageTags(UINT32, PTag, UINT32, PTag, PUINT32);

/////////////////////////////////////////
// Directory functionality
/////////////////////////////////////////

typedef enum { DIR_ENTRY_TYPE_FILE, DIR_ENTRY_TYPE_LINK, DIR_ENTRY_TYPE_DIRECTORY, DIR_ENTRY_TYPE_UNKNOWN } DIR_ENTRY_TYPES;

/**
 * Callback function declaration.
 *
 * @UINT64 - the caller passed data
 * @DIR_ENTRY_TYPES - the type of the entry
 * @PCHAR - the full path of the entry
 * @PCHAR - the name of the entry
 */
typedef STATUS (*DirectoryEntryCallbackFunc)(UINT64, DIR_ENTRY_TYPES, PCHAR, PCHAR);

/**
 * Remove a directory - empty or not
 *
 * @PCHAR - directory path
 * @UINT64 - custom caller data passed to the callback
 * @BOOL - whether to iterate
 * @DirectoryEntryCallbackFunc - the callback function called with each entry
 */
PUBLIC_API STATUS traverseDirectory(PCHAR, UINT64, BOOL iterate, DirectoryEntryCallbackFunc);

/**
 * Remove a directory - empty or not
 *
 * @PCHAR - directory path
 */
PUBLIC_API STATUS removeDirectory(PCHAR);

/**
 * Gets the directory size
 *
 * @PCHAR - directory path
 * @PUINT64 - returned combined size
 */
PUBLIC_API STATUS getDirectorySize(PCHAR, PUINT64);

/**
 * Double-linked list definition
 */
typedef struct __DoubleListNode {
    struct __DoubleListNode* pNext;
    struct __DoubleListNode* pPrev;
    UINT64 data;
} DoubleListNode, *PDoubleListNode;

typedef struct {
    UINT32 count;
    PDoubleListNode pHead;
    PDoubleListNode pTail;
} DoubleList, *PDoubleList;

typedef struct __SingleListNode {
    struct __SingleListNode* pNext;
    UINT64 data;
} SingleListNode, *PSingleListNode;

typedef struct {
    UINT32 count;
    PSingleListNode pHead;
    PSingleListNode pTail;
} SingleList, *PSingleList;

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Double-linked list functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Create a new double linked list
 */
PUBLIC_API STATUS doubleListCreate(PDoubleList*);

/**
 * Frees a double linked list and deallocates the nodes
 */
PUBLIC_API STATUS doubleListFree(PDoubleList);

/**
 * Clears and deallocates all the items
 */
PUBLIC_API STATUS doubleListClear(PDoubleList, BOOL);

/**
 * Insert a node in the head position in the list
 */
PUBLIC_API STATUS doubleListInsertNodeHead(PDoubleList, PDoubleListNode);

/**
 * Insert a new node with the data at the head position in the list
 */
PUBLIC_API STATUS doubleListInsertItemHead(PDoubleList, UINT64);

/**
 * Insert a node in the tail position in the list
 */
PUBLIC_API STATUS doubleListInsertNodeTail(PDoubleList, PDoubleListNode);

/**
 * Insert a new node with the data at the tail position in the list
 */
PUBLIC_API STATUS doubleListInsertItemTail(PDoubleList, UINT64);

/**
 * Insert a node before a given node
 */
PUBLIC_API STATUS doubleListInsertNodeBefore(PDoubleList, PDoubleListNode, PDoubleListNode);

/**
 * Insert a new node with the data before a given node
 */
PUBLIC_API STATUS doubleListInsertItemBefore(PDoubleList, PDoubleListNode, UINT64);

/**
 * Insert a node after a given node
 */
PUBLIC_API STATUS doubleListInsertNodeAfter(PDoubleList, PDoubleListNode, PDoubleListNode);

/**
 * Insert a new node with the data after a given node
 */
PUBLIC_API STATUS doubleListInsertItemAfter(PDoubleList, PDoubleListNode, UINT64);

/**
 * Removes and deletes the head
 */
PUBLIC_API STATUS doubleListDeleteHead(PDoubleList);

/**
 * Removes and deletes the tail
 */
PUBLIC_API STATUS doubleListDeleteTail(PDoubleList);

/**
 * Removes the specified node
 */
PUBLIC_API STATUS doubleListRemoveNode(PDoubleList, PDoubleListNode);

/**
 * Removes and deletes the specified node
 */
PUBLIC_API STATUS doubleListDeleteNode(PDoubleList, PDoubleListNode);

/**
 * Gets the head node
 */
PUBLIC_API STATUS doubleListGetHeadNode(PDoubleList, PDoubleListNode*);

/**
 * Gets the tail node
 */
PUBLIC_API STATUS doubleListGetTailNode(PDoubleList, PDoubleListNode*);

/**
 * Gets the node at the specified index
 */
PUBLIC_API STATUS doubleListGetNodeAt(PDoubleList, UINT32, PDoubleListNode*);

/**
 * Gets the node data at the specified index
 */
PUBLIC_API STATUS doubleListGetNodeDataAt(PDoubleList, UINT32, PUINT64);

/**
 * Gets the node data
 */
PUBLIC_API STATUS doubleListGetNodeData(PDoubleListNode, PUINT64);

/**
 * Gets the next node
 */
PUBLIC_API STATUS doubleListGetNextNode(PDoubleListNode, PDoubleListNode*);

/**
 * Gets the previous node
 */
PUBLIC_API STATUS doubleListGetPrevNode(PDoubleListNode, PDoubleListNode*);

/**
 * Gets the count of nodes in the list
 */
PUBLIC_API STATUS doubleListGetNodeCount(PDoubleList, PUINT32);

/**
 * Append a double list to the other and then free the list being appended
 */
PUBLIC_API STATUS doubleListAppendList(PDoubleList, PDoubleList*);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Single-linked list functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Create a new single linked list
 */
PUBLIC_API STATUS singleListCreate(PSingleList*);

/**
 * Frees a single linked list and deallocates the nodes
 */
PUBLIC_API STATUS singleListFree(PSingleList);

/**
 * Clears and deallocates all the items
 */
PUBLIC_API STATUS singleListClear(PSingleList, BOOL);

/**
 * Insert a node in the head position in the list
 */
PUBLIC_API STATUS singleListInsertNodeHead(PSingleList, PSingleListNode);

/**
 * Insert a new node with the data at the head position in the list
 */
PUBLIC_API STATUS singleListInsertItemHead(PSingleList, UINT64);

/**
 * Insert a node in the tail position in the list
 */
PUBLIC_API STATUS singleListInsertNodeTail(PSingleList, PSingleListNode);

/**
 * Insert a new node with the data at the tail position in the list
 */
PUBLIC_API STATUS singleListInsertItemTail(PSingleList, UINT64);

/**
 * Insert a node after a given node
 */
PUBLIC_API STATUS singleListInsertNodeAfter(PSingleList, PSingleListNode, PSingleListNode);

/**
 * Insert a new node with the data after a given node
 */
PUBLIC_API STATUS singleListInsertItemAfter(PSingleList, PSingleListNode, UINT64);

/**
 * Removes and deletes the head
 */
PUBLIC_API STATUS singleListDeleteHead(PSingleList);

/**
 * Removes and deletes the specified node
 */
PUBLIC_API STATUS singleListDeleteNode(PSingleList, PSingleListNode);

/**
 * Removes and deletes the next node of the specified node
 */
PUBLIC_API STATUS singleListDeleteNextNode(PSingleList, PSingleListNode);

/**
 * Gets the head node
 */
PUBLIC_API STATUS singleListGetHeadNode(PSingleList, PSingleListNode*);

/**
 * Gets the tail node
 */
PUBLIC_API STATUS singleListGetTailNode(PSingleList, PSingleListNode*);

/**
 * Gets the node at the specified index
 */
PUBLIC_API STATUS singleListGetNodeAt(PSingleList, UINT32, PSingleListNode*);

/**
 * Gets the node data at the specified index
 */
PUBLIC_API STATUS singleListGetNodeDataAt(PSingleList, UINT32, PUINT64);

/**
 * Gets the node data
 */
PUBLIC_API STATUS singleListGetNodeData(PSingleListNode, PUINT64);

/**
 * Gets the next node
 */
PUBLIC_API STATUS singleListGetNextNode(PSingleListNode, PSingleListNode*);

/**
 * Gets the count of nodes in the list
 */
PUBLIC_API STATUS singleListGetNodeCount(PSingleList, PUINT32);

/**
 * Append a single list to the other and then free the list being appended
 */
STATUS singleListAppendList(PSingleList, PSingleList*);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Stack/Queue functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef SingleList StackQueue;
typedef PSingleList PStackQueue;
typedef PSingleListNode StackQueueIterator;
typedef StackQueueIterator* PStackQueueIterator;

#define IS_VALID_ITERATOR(x) ((x) != NULL)

/**
 * Create a new stack queue
 */
PUBLIC_API STATUS stackQueueCreate(PStackQueue*);

/**
 * Frees and de-allocates the stack queue
 */
PUBLIC_API STATUS stackQueueFree(PStackQueue);

/**
 * Clears and de-allocates all the items
 */
PUBLIC_API STATUS stackQueueClear(PStackQueue, BOOL);

/**
 * Gets the number of items in the stack/queue
 */
PUBLIC_API STATUS stackQueueGetCount(PStackQueue, PUINT32);

/**
 * Gets the item at the given index
 */
PUBLIC_API STATUS stackQueueGetAt(PStackQueue, UINT32, PUINT64);

/**
 * Sets the item value at the given index
 */
PUBLIC_API STATUS stackQueueSetAt(PStackQueue, UINT32, UINT64);

/**
 * Gets the index of an item
 */
PUBLIC_API STATUS stackQueueGetIndexOf(PStackQueue, UINT64, PUINT32);

/**
 * Removes the item at the given index
 */
PUBLIC_API STATUS stackQueueRemoveAt(PStackQueue, UINT32);

/**
 * Removes the item at the given item
 */
PUBLIC_API STATUS stackQueueRemoveItem(PStackQueue, UINT64);

/**
 * Whether the stack queue is empty
 */
PUBLIC_API STATUS stackQueueIsEmpty(PStackQueue, PBOOL);

/**
 * Pushes an item onto the stack
 */
PUBLIC_API STATUS stackQueuePush(PStackQueue, UINT64);

/**
 * Pops an item from the stack
 */
PUBLIC_API STATUS stackQueuePop(PStackQueue, PUINT64);

/**
 * Peeks an item from the stack without popping
 */
PUBLIC_API STATUS stackQueuePeek(PStackQueue, PUINT64);

/**
 * Enqueues an item in the queue
 */
PUBLIC_API STATUS stackQueueEnqueue(PStackQueue, UINT64);

/**
 * Dequeues an item from the queue
 */
PUBLIC_API STATUS stackQueueDequeue(PStackQueue, PUINT64);

/**
 * Gets the iterator
 */
PUBLIC_API STATUS stackQueueGetIterator(PStackQueue, PStackQueueIterator);

/**
 * Iterates to next
 */
PUBLIC_API STATUS stackQueueIteratorNext(PStackQueueIterator);

/**
 * Gets the data
 */
PUBLIC_API STATUS stackQueueIteratorGetItem(StackQueueIterator, PUINT64);

/**
 * Inserts item into queue after given index
 */
PUBLIC_API STATUS stackQueueEnqueueAfterIndex(PStackQueue, UINT32, UINT64);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Hash table functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

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

/**
 * Hash table error values starting from 0x40100000
 */
#define STATUS_HASH_TABLE_BASE            STATUS_UTILS_BASE + 0x00100000
#define STATUS_HASH_KEY_NOT_PRESENT       STATUS_HASH_TABLE_BASE + 0x00000001
#define STATUS_HASH_KEY_ALREADY_PRESENT   STATUS_HASH_TABLE_BASE + 0x00000002
#define STATUS_HASH_ENTRY_ITERATION_ABORT STATUS_HASH_TABLE_BASE + 0x00000003

/**
 * Create a new hash table with default parameters
 */
PUBLIC_API STATUS hashTableCreate(PHashTable*);

/**
 * Create a new hash table with specific parameters
 */
PUBLIC_API STATUS hashTableCreateWithParams(UINT32, UINT32, PHashTable*);

/**
 * Frees and de-allocates the hash table
 */
PUBLIC_API STATUS hashTableFree(PHashTable);

/**
 * Clears all the items and the buckets
 */
PUBLIC_API STATUS hashTableClear(PHashTable);

/**
 * Gets the number of items in the hash table
 */
PUBLIC_API STATUS hashTableGetCount(PHashTable, PUINT32);

/**
 * Whether the hash table is empty
 */
PUBLIC_API STATUS hashTableIsEmpty(PHashTable, PBOOL);

/**
 * Puts an item into the hash table
 */
PUBLIC_API STATUS hashTablePut(PHashTable, UINT64, UINT64);

/**
 * Upserts an item into the hash table
 */
PUBLIC_API STATUS hashTableUpsert(PHashTable, UINT64, UINT64);

/**
 * Gets an item from the hash table
 */
PUBLIC_API STATUS hashTableGet(PHashTable, UINT64, PUINT64);

/**
 * Checks whether an item exists in the hash table
 */
PUBLIC_API STATUS hashTableContains(PHashTable, UINT64, PBOOL);

/**
 * Removes an item from the hash table. If the bucket is empty it's deleted. The existing items will be shifted.
 */
PUBLIC_API STATUS hashTableRemove(PHashTable, UINT64);

/**
 * Gets the number of buckets
 */
PUBLIC_API STATUS hashTableGetBucketCount(PHashTable, PUINT32);

/**
 * Gets all the entries from the hash table
 */
PUBLIC_API STATUS hashTableGetAllEntries(PHashTable, PHashEntry, PUINT32);

/**
 * Iterates over the hash entries. No predefined order
 */
PUBLIC_API STATUS hashTableIterateEntries(PHashTable, UINT64, HashEntryCallbackFunc);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Bitfield functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns the byte count
 */
#define GET_BYTE_COUNT_FOR_BITS(x) ((((x) + 7) & ~7) >> 3)

/**
 * Bit field declaration
 * NOTE: Variable size structure - the bit field follow directly after the main structure
 */
typedef struct {
    UINT32 itemCount;
    /*-- BYTE[byteCount] bits; --*/
} BitField, *PBitField;

/**
 * Create a new bit field with all bits set to 0
 */
PUBLIC_API STATUS bitFieldCreate(UINT32, PBitField*);

/**
 * Frees and de-allocates the bit field object
 */
PUBLIC_API STATUS bitFieldFree(PBitField);

/**
 * Sets or clears all the bits
 */
PUBLIC_API STATUS bitFieldReset(PBitField, BOOL);

/**
 * Gets the bit field size in items
 */
PUBLIC_API STATUS bitFieldGetCount(PBitField, PUINT32);

/**
 * Gets the value of the bit
 */
PUBLIC_API STATUS bitFieldGet(PBitField, UINT32, PBOOL);

/**
 * Sets the value of the bit
 */
PUBLIC_API STATUS bitFieldSet(PBitField, UINT32, BOOL);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// BitBuffer reader functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Bit reader error values starting from 0x41000000
 */
#define STATUS_BIT_READER_BASE         STATUS_UTILS_BASE + 0x01000000
#define STATUS_BIT_READER_OUT_OF_RANGE STATUS_BIT_READER_BASE + 0x00000001
#define STATUS_BIT_READER_INVALID_SIZE STATUS_BIT_READER_BASE + 0x00000002

/**
 * Bit Buffer reader declaration
 */
typedef struct {
    // Bit buffer
    PBYTE buffer;

    // Size of the buffer in bits
    UINT32 bitBufferSize;

    // Current bit
    UINT32 currentBit;
} BitReader, *PBitReader;

/**
 * Resets the bit reader object
 */
PUBLIC_API STATUS bitReaderReset(PBitReader, PBYTE, UINT32);

/**
 * Set current pointer
 */
PUBLIC_API STATUS bitReaderSetCurrent(PBitReader, UINT32);

/**
 * Read a bit from the current pointer
 */
PUBLIC_API STATUS bitReaderReadBit(PBitReader, PUINT32);

/**
 * Read up-to 32 bits from the current pointer
 */
PUBLIC_API STATUS bitReaderReadBits(PBitReader, UINT32, PUINT32);

/**
 * Read the Exponential Golomb encoded number from the current position
 */
PUBLIC_API STATUS bitReaderReadExpGolomb(PBitReader, PUINT32);

/**
 * Read the Exponential Golomb encoded signed number from the current position
 */
PUBLIC_API STATUS bitReaderReadExpGolombSe(PBitReader, PINT32);

////////////////////////////////////////////////////
// Endianness functionality
////////////////////////////////////////////////////
typedef INT16 (*getInt16Func)(INT16);
typedef INT32 (*getInt32Func)(INT32);
typedef INT64 (*getInt64Func)(INT64);
typedef VOID (*putInt16Func)(PINT16, INT16);
typedef VOID (*putInt32Func)(PINT32, INT32);
typedef VOID (*putInt64Func)(PINT64, INT64);

extern getInt16Func getInt16;
extern getInt32Func getInt32;
extern getInt64Func getInt64;
extern putInt16Func putInt16;
extern putInt32Func putInt32;
extern putInt64Func putInt64;

PUBLIC_API BOOL isBigEndian();
PUBLIC_API VOID initializeEndianness();

////////////////////////////////////////////////////
// Unaligned access functionality
////////////////////////////////////////////////////
typedef INT16 (*getUnalignedInt16Func)(PVOID);
typedef INT32 (*getUnalignedInt32Func)(PVOID);
typedef INT64 (*getUnalignedInt64Func)(PVOID);

typedef VOID (*putUnalignedInt16Func)(PVOID, INT16);
typedef VOID (*putUnalignedInt32Func)(PVOID, INT32);
typedef VOID (*putUnalignedInt64Func)(PVOID, INT64);

extern getUnalignedInt16Func getUnalignedInt16;
extern getUnalignedInt32Func getUnalignedInt32;
extern getUnalignedInt64Func getUnalignedInt64;

extern putUnalignedInt16Func putUnalignedInt16;
extern putUnalignedInt32Func putUnalignedInt32;
extern putUnalignedInt64Func putUnalignedInt64;

// These are the specific Big-endian variants needed for most of the formats
extern getUnalignedInt16Func getUnalignedInt16BigEndian;
extern getUnalignedInt32Func getUnalignedInt32BigEndian;
extern getUnalignedInt64Func getUnalignedInt64BigEndian;

extern putUnalignedInt16Func putUnalignedInt16BigEndian;
extern putUnalignedInt32Func putUnalignedInt32BigEndian;
extern putUnalignedInt64Func putUnalignedInt64BigEndian;

extern getUnalignedInt16Func getUnalignedInt16LittleEndian;
extern getUnalignedInt32Func getUnalignedInt32LittleEndian;
extern getUnalignedInt64Func getUnalignedInt64LittleEndian;

extern putUnalignedInt16Func putUnalignedInt16LittleEndian;
extern putUnalignedInt32Func putUnalignedInt32LittleEndian;
extern putUnalignedInt64Func putUnalignedInt64LittleEndian;

// Helper macro for unaligned
#define GET_UNALIGNED(ptr)                                                                                                                           \
    SIZEOF(*(ptr)) == 1       ? *(ptr)                                                                                                               \
        : SIZEOF(*(ptr)) == 2 ? getUnalignedInt16(ptr)                                                                                               \
        : SIZEOF(*(ptr)) == 4 ? getUnalignedInt32(ptr)                                                                                               \
        : SIZEOF(*(ptr)) == 8 ? getUnalignedInt64(ptr)                                                                                               \
                              : 0

#define GET_UNALIGNED_BIG_ENDIAN(ptr)                                                                                                                \
    SIZEOF(*(ptr)) == 1       ? *(ptr)                                                                                                               \
        : SIZEOF(*(ptr)) == 2 ? getUnalignedInt16BigEndian(ptr)                                                                                      \
        : SIZEOF(*(ptr)) == 4 ? getUnalignedInt32BigEndian(ptr)                                                                                      \
        : SIZEOF(*(ptr)) == 8 ? getUnalignedInt64BigEndian(ptr)                                                                                      \
                              : 0

#define PUT_UNALIGNED(ptr, val)                                                                                                                      \
    do {                                                                                                                                             \
        PVOID __pVoid = (ptr);                                                                                                                       \
        switch (SIZEOF(*(ptr))) {                                                                                                                    \
            case 1:                                                                                                                                  \
                *(PINT8) __pVoid = (INT8) (val);                                                                                                     \
                break;                                                                                                                               \
            case 2:                                                                                                                                  \
                putUnalignedInt16(__pVoid, (INT16) (val));                                                                                           \
                break;                                                                                                                               \
            case 4:                                                                                                                                  \
                putUnalignedInt32(__pVoid, (INT32) (val));                                                                                           \
                break;                                                                                                                               \
            case 8:                                                                                                                                  \
                putUnalignedInt64(__pVoid, (INT64) (val));                                                                                           \
                break;                                                                                                                               \
            default:                                                                                                                                 \
                CHECK_EXT(FALSE, "Bad alignment size.");                                                                                             \
                break;                                                                                                                               \
        }                                                                                                                                            \
    } while (0);

#define PUT_UNALIGNED_BIG_ENDIAN(ptr, val)                                                                                                           \
    do {                                                                                                                                             \
        PVOID __pVoid = (ptr);                                                                                                                       \
        switch (SIZEOF(*(ptr))) {                                                                                                                    \
            case 1:                                                                                                                                  \
                *(PINT8) __pVoid = (INT8) (val);                                                                                                     \
                break;                                                                                                                               \
            case 2:                                                                                                                                  \
                putUnalignedInt16BigEndian(__pVoid, (INT16) (val));                                                                                  \
                break;                                                                                                                               \
            case 4:                                                                                                                                  \
                putUnalignedInt32BigEndian(__pVoid, (INT32) (val));                                                                                  \
                break;                                                                                                                               \
            case 8:                                                                                                                                  \
                putUnalignedInt64BigEndian(__pVoid, (INT64) (val));                                                                                  \
                break;                                                                                                                               \
            default:                                                                                                                                 \
                CHECK_EXT(FALSE, "Bad alignment size.");                                                                                             \
                break;                                                                                                                               \
        }                                                                                                                                            \
    } while (0);

////////////////////////////////////////////////////
// Dumping memory functionality
////////////////////////////////////////////////////
VOID dumpMemoryHex(PVOID, UINT32);

////////////////////////////////////////////////////
// Check memory content
////////////////////////////////////////////////////
BOOL checkBufferValues(PVOID, BYTE, SIZE_T);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Time functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * This function generates the time string based on the timestamp format requested and the timestamp provided
 * Example: generateTimestampStrInMilliseconds(1689093566, "%Y-%m-%d %H:%M:%S", timeString, (UINT32) ARRAY_SIZE(timeString), &timeStrLen);
 * @UINT64  - IN - timestamp in 100ns to be converted to string. Sample timestamp input: 1689093566
 * @PCHAR   - IN - timestamp format string. Sample time format input: %Y-%m-%d %H:%M:%S
 * @PCHAR   - IN - buffer to hold the resulting string. Sample timestring output: 2023-07-11 16:37:07
 * @UINT32  - IN - buffer size
 * @PUINT32 - OUT - actual number of characters in the result string not including null terminator
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS generateTimestampStr(UINT64, PCHAR, PCHAR, UINT32, PUINT32);

/**
 * This function generates the millisecond portion of the timestamp and appends to the timestamp format supplied.
 * The output is of the format: <Timestring-format>.ssssss where ssssss is the millisecond format
 * Example: generateTimestampStrInMilliseconds("%Y-%m-%d %H:%M:%S", timeString, (UINT32) ARRAY_SIZE(timeString), &timeStrLen);
 * Formats can be constructed using this: https://man7.org/linux/man-pages/man3/strftime.3.html
 * @PCHAR   - IN - timestamp format string without milliseconds. Sample time format input: %Y-%m-%d %H:%M:%S
 * @PCHAR   - IN - buffer to hold the resulting string. Sample timestring output: 2023-07-11 16:37:07.025527,
 * where 025527 is the appended millisecond value
 * @UINT32  - IN - buffer size
 * @PUINT32 - OUT - actual number of characters in the result string not including null terminator
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS generateTimestampStrInMilliseconds(PCHAR, PCHAR, UINT32, PUINT32);

// yyyy-mm-dd HH:MM:SS
#define MAX_TIMESTAMP_FORMAT_STR_LEN 26

// Length = len (.) + len(sss) + len('\0')
#define MAX_MILLISECOND_PORTION_LENGTH 5

// Max timestamp string length including null terminator
#define MAX_TIMESTAMP_STR_LEN 17

// (thread-0x7000076b3000)
#define MAX_THREAD_ID_STR_LEN 23

// Max log message length
#define MAX_LOG_FORMAT_LENGTH 600

// Set the global log level
#define SET_LOGGER_LOG_LEVEL(l) loggerSetLogLevel((l))

// Get the global log level
#define GET_LOGGER_LOG_LEVEL() loggerGetLogLevel()

/*
 * Set log level
 * @UINT32 - IN - target log level
 */
PUBLIC_API VOID loggerSetLogLevel(UINT32);

/**
 * Get current log level
 * @return - UINT32 - current log level
 */
PUBLIC_API UINT32 loggerGetLogLevel();

/**
 * Prepend log message with timestamp and thread id.
 * @PCHAR - IN - buffer holding the log
 * @UINT32 - IN - buffer length
 * @PCHAR - IN - log format string
 * @UINT32 - IN - log level
 * @return - VOID
 */
PUBLIC_API VOID addLogMetadata(PCHAR, UINT32, PCHAR, UINT32);

/**
 * Updates a CRC32 checksum
 * @UINT32 - IN - initial checksum result from previous update; for the first call, it should be 0.
 * @PBYTE - IN - buffer used to compute checksum
 * @UINT32 - IN - number of bytes to use from buffer
 * @return - UINT32 crc32 checksum
 */
PUBLIC_API UINT32 updateCrc32(UINT32, PBYTE, UINT32);

/**
 * @PBYTE - IN - buffer used to compute checksum
 * @UINT32 - IN - number of bytes to use from buffer
 * @return - UINT32 crc32 checksum
 */
#define COMPUTE_CRC32(pBuffer, len) (updateCrc32(0, pBuffer, len))

//////////////////////////////////////////////////////////////////////////////////////////////////////
// TimerQueue functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Minimum number of timers in the timer queue
 */
#define MIN_TIMER_QUEUE_TIMER_COUNT 1

/**
 * Default timer queue max timer count
 */
#define DEFAULT_TIMER_QUEUE_TIMER_COUNT 32

/**
 * Sentinel value to specify no periodic invocation
 */
#define TIMER_QUEUE_SINGLE_INVOCATION_PERIOD 0

/**
 * Shortest period value to schedule the call
 */
#define MIN_TIMER_QUEUE_PERIOD_DURATION (1 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Definition of the TimerQueue handle
 */
typedef UINT64 TIMER_QUEUE_HANDLE;
typedef TIMER_QUEUE_HANDLE* PTIMER_QUEUE_HANDLE;

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_TIMER_QUEUE_HANDLE_VALUE
#define INVALID_TIMER_QUEUE_HANDLE_VALUE ((TIMER_QUEUE_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_TIMER_QUEUE_HANDLE
#define IS_VALID_TIMER_QUEUE_HANDLE(h) ((h) != INVALID_TIMER_QUEUE_HANDLE_VALUE)
#endif

/**
 * Timer queue callback
 *
 * IMPORTANT!!!
 * The callback should be 'prompt' - any lengthy or blocking operations should be executed
 * in their own execution unit - aka thread.
 *
 * NOTE: To terminate the scheduling of the calls return STATUS_TIMER_QUEUE_STOP_SCHEDULING
 * NOTE: Returning other non-STATUS_SUCCESS status will issue a warning but the timer will
 * still continue to be scheduled.
 *
 * @UINT32 - Timer ID that's fired
 * @UINT64 - Current time the scheduling is triggered
 * @UINT64 - Data that was passed to the timer function
 *
 */
typedef STATUS (*TimerCallbackFunc)(UINT32, UINT64, UINT64);

/**
 * Timer queue error values starting from 0x41100000
 */
#define STATUS_TIMER_QUEUE_BASE            STATUS_UTILS_BASE + 0x01100000
#define STATUS_TIMER_QUEUE_STOP_SCHEDULING STATUS_TIMER_QUEUE_BASE + 0x00000001
#define STATUS_INVALID_TIMER_COUNT_VALUE   STATUS_TIMER_QUEUE_BASE + 0x00000002
#define STATUS_INVALID_TIMER_PERIOD_VALUE  STATUS_TIMER_QUEUE_BASE + 0x00000003
#define STATUS_MAX_TIMER_COUNT_REACHED     STATUS_TIMER_QUEUE_BASE + 0x00000004
#define STATUS_TIMER_QUEUE_SHUTDOWN        STATUS_TIMER_QUEUE_BASE + 0x00000005

/**
 * @param - PTIMER_QUEUE_HANDLE - OUT - Timer queue handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCreate(PTIMER_QUEUE_HANDLE);

/*
 * Frees the Timer queue object
 *
 * NOTE: The call is idempotent.
 *
 * @param - PTIMER_QUEUE_HANDLE - IN/OUT/OPT - Timer queue handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueFree(PTIMER_QUEUE_HANDLE);

/*
 * Add timer to the timer queue.
 *
 * NOTE: The timer period value of TIMER_QUEUE_SINGLE_INVOCATION_PERIOD will schedule the call only once
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - Start duration in 100ns at which to start the first time
 * @param - UINT64 - IN - Timer period value in 100ns to schedule the callback
 * @param - TimerCallbackFunc - IN - Callback to call for the timer
 * @param - UINT64 - IN - Timer callback function custom data
 * @param - PUINT32 - IN - Created timers ID
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueAddTimer(TIMER_QUEUE_HANDLE, UINT64, UINT64, TimerCallbackFunc, UINT64, PUINT32);

/*
 * Cancel the timer. customData is needed to handle case when user 1 add timer and then the timer
 * get cancelled because the callback returned STATUS_TIMER_QUEUE_STOP_SCHEDULING. Then user 2 add
 * another timer but then user 1 cancel timeId it first received. Without checking custom data user 2's timer
 * would be deleted by user 1.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT32 - IN - Timer id to cancel
 * @param - UINT64 - IN - provided customData. CustomData needs to match in order to successfully cancel.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelTimer(TIMER_QUEUE_HANDLE, UINT32, UINT64);

/*
 * Cancel all timers with customData
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - provided customData.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelTimersWithCustomData(TIMER_QUEUE_HANDLE, UINT64);

/*
 * Cancel all timers
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueCancelAllTimers(TIMER_QUEUE_HANDLE);

/*
 * Get active timer count
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - PUINT32 - OUT - pointer to store active timer count
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueGetTimerCount(TIMER_QUEUE_HANDLE, PUINT32);

/*
 * Get timer ids with custom data
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - custom data to match
 * @param - PUINT32 - IN/OUT - size of the array passing in and will store the number of timer ids when the function returns
 * @param - PUINT32* - OUT/OPT - array that will store the timer ids. can be NULL.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueGetTimersWithCustomData(TIMER_QUEUE_HANDLE, UINT64, PUINT32, PUINT32);

/*
 * update timer id's period. Do nothing if timer not found.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT64 - IN - custom data to match
 * @param - UINT32 - IN - Timer id to update
 * @param - UINT32 - IN - new period
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueUpdateTimerPeriod(TIMER_QUEUE_HANDLE, UINT64, UINT32, UINT64);

/*
 * Kick timer id's timer to invoke immediately. Do nothing if timer not found.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 * @param - UINT32 - IN - Timer id to update
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueKick(TIMER_QUEUE_HANDLE, UINT32);

/*
 * stop the timer. Once stopped timer can't be restarted. There will be no more timer callback invocation after
 * timerQueueShutdown returns.
 *
 * @param - TIMER_QUEUE_HANDLE - IN - Timer queue handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS timerQueueShutdown(TIMER_QUEUE_HANDLE);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Semaphore functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Semaphore shutdown timeout value
 */
#define SEMAPHORE_SHUTDOWN_TIMEOUT (200 * HUNDREDS_OF_NANOS_IN_A_MILLISECOND)

/**
 * Definition of the Semaphore handle
 */
typedef UINT64 SEMAPHORE_HANDLE;
typedef SEMAPHORE_HANDLE* PSEMAPHORE_HANDLE;

/**
 * This is a sentinel indicating an invalid handle value
 */
#ifndef INVALID_SEMAPHORE_HANDLE_VALUE
#define INVALID_SEMAPHORE_HANDLE_VALUE ((SEMAPHORE_HANDLE) INVALID_PIC_HANDLE_VALUE)
#endif

/**
 * Checks for the handle validity
 */
#ifndef IS_VALID_SEMAPHORE_HANDLE
#define IS_VALID_SEMAPHORE_HANDLE(h) ((h) != INVALID_SEMAPHORE_HANDLE_VALUE)
#endif

/**
 * Semaphore error values starting from 0x41200000
 */
#define STATUS_SEMAPHORE_BASE                     STATUS_UTILS_BASE + 0x01200000
#define STATUS_SEMAPHORE_OPERATION_AFTER_SHUTDOWN STATUS_SEMAPHORE_BASE + 0x00000001
#define STATUS_SEMAPHORE_ACQUIRE_WHEN_LOCKED      STATUS_SEMAPHORE_BASE + 0x00000002

/**
 * Create a semaphore object
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreCreate(UINT32, PSEMAPHORE_HANDLE);

/**
 * Create a semaphore object that starts with 0 count
 *
 * @param - UINT32 - IN - The permit count
 * @param - PSEMAPHORE_HANDLE - OUT - Semaphore handle
 *
 * @return  - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreEmptyCreate(UINT32, PSEMAPHORE_HANDLE);

/*
 * Frees the semaphore object releasing all the awaiting threads.
 *
 * NOTE: The call is idempotent.
 *
 * @param - PSEMAPHORE_HANDLE - IN/OUT/OPT - Semaphore handle to free
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreFree(PSEMAPHORE_HANDLE);

/*
 * Acquires a semaphore. Will block for the specified amount of time before failing the acquisition.
 *
 * IMPORTANT NOTE: On failed acquire it will not increment the acquired count.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Time to wait to acquire in 100ns
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreAcquire(SEMAPHORE_HANDLE, UINT64);

/*
 * Releases a semaphore. Blocked threads will be released to acquire the available slot
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreRelease(SEMAPHORE_HANDLE);

/*
 * Locks the semaphore for any acquisitions.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreLock(SEMAPHORE_HANDLE);

/*
 * Unlocks the semaphore.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreUnlock(SEMAPHORE_HANDLE);

/*
 * Await for the semaphore to drain.
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - UINT64 - IN - Timeout value to wait for
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreWaitUntilClear(SEMAPHORE_HANDLE, UINT64);

/*
 * Get the current value of the semaphore count
 *
 * @param - SEMAPHORE_HANDLE - IN - Semaphore handle
 * @param - PINT32 - OUT - Value of count
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS semaphoreGetCount(SEMAPHORE_HANDLE, PINT32);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrumented memory allocators functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Sets the global allocators to the instrumented ones.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setInstrumentedAllocators();

/**
 * No-op equivalent of the setInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(SET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS setInstrumentedAllocatorsNoop();

/**
 * Resets the global allocators to the original ones.
 *
 * NOTE: Any attempt to free allocations which were allocated after set call
 * past this API call will result in memory corruption.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS resetInstrumentedAllocators();

/**
 * No-op equivalent of the resetInstrumentedAllocators.
 *
 * NOTE: This is needed to allow the applications to use the macro which evaluates
 * at compile time based on the INSTRUMENTED_ALLOCATORS compiler definition.
 * The reason for the API is due to inability to get a no-op C macro compilable
 * across different languages and compilers with l-values.
 *
 * ex: CHK_STATUS(RESET_INSTRUMENTED_ALLOCATORS);
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS resetInstrumentedAllocatorsNoop();

/**
 * Returns the current total allocation size.
 *
 * @return - Total allocation size
 */
PUBLIC_API SIZE_T getInstrumentedTotalAllocationSize();

#ifdef INSTRUMENTED_ALLOCATORS
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocators()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocators()
#else
#define SET_INSTRUMENTED_ALLOCATORS()   setInstrumentedAllocatorsNoop()
#define RESET_INSTRUMENTED_ALLOCATORS() resetInstrumentedAllocatorsNoop()
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// File logging functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * File logger error values starting from 0x41300000
 */
#define STATUS_FILE_LOGGER_BASE                    STATUS_UTILS_BASE + 0x01300000
#define STATUS_FILE_LOGGER_INDEX_FILE_INVALID_SIZE STATUS_FILE_LOGGER_BASE + 0x00000001

/**
 * File based logger limit constants
 */
#define MAX_FILE_LOGGER_STRING_BUFFER_SIZE (100 * 1024 * 1024)
#define MIN_FILE_LOGGER_STRING_BUFFER_SIZE (10 * 1024)
#define MAX_FILE_LOGGER_LOG_FILE_COUNT     (10 * 1024)

/**
 * Default values used in the file logger
 */
#define FILE_LOGGER_LOG_FILE_NAME               "kvsFileLog"
#define FILE_LOGGER_FILTER_LOG_FILE_NAME        "kvsFileLogFilter"
#define FILE_LOGGER_LAST_INDEX_FILE_NAME        "kvsFileLogIndex"
#define FILE_LOGGER_LAST_FILTER_INDEX_FILE_NAME "kvsFileFilterLogIndex"
#define FILE_LOGGER_STRING_BUFFER_SIZE          (100 * 1024)
#define FILE_LOGGER_LOG_FILE_COUNT              3
#define FILE_LOGGER_LOG_FILE_DIRECTORY_PATH     "./"

/**
 * Creates a file based logger object and installs the global logger callback function
 *
 * @param - UINT64 - IN - Size of string buffer in file logger. When the string buffer is full the logger will flush everything into a new file
 * @param - UINT64 - IN - Max number of log file. When exceeded, the oldest file will be deleted when new one is generated
 * @param - PCHAR - IN - Directory in which the log file will be generated
 * @param - BOOL - IN - Whether to print log to std out too
 * @param - BOOL - IN - Whether to set global logger function pointer
 * @param - logPrintFunc* - OUT/OPT - Optional function pointer to be returned to the caller that contains the main function for actual output
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createFileLogger(UINT64, UINT64, PCHAR, BOOL, BOOL, logPrintFunc*);

/**
 * Creates a file based logger object and installs the global logger callback function
 *
 * @param - UINT64 - IN - Size of string buffer in file logger. When the string buffer is full the logger will flush everything into a new file
 * @param - UINT64 - IN - Max number of log file. When exceeded, the oldest file will be deleted when new one is generated
 * @param - PCHAR - IN - Directory in which the log file will be generated
 * @param - BOOL - IN - Whether to print log to std out too
 * @param - BOOL - IN - Whether to set global logger function pointer
 * @param - BOOL - IN - Whether to enable logging other log levels into a file
 * @param - UINT32 - IN - Log level that needs to be filtered into another file
 * @param - logPrintFunc* - OUT/OPT - Optional function pointer to be returned to the caller that contains the main function for actual output
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS createFileLoggerWithLevelFiltering(UINT64, UINT64, PCHAR, BOOL, BOOL, BOOL, UINT32, logPrintFunc*);

/**
 * Frees the static file logger object and resets the global logging function if it was
 * previously set by the create function.
 *
 * @return - STATUS code of the execution
 */
PUBLIC_API STATUS freeFileLogger();

/**
 * Helper macros to be used in pairs at the application start and end
 */
#define CREATE_DEFAULT_FILE_LOGGER()                                                                                                                 \
    createFileLogger(FILE_LOGGER_STRING_BUFFER_SIZE, FILE_LOGGER_LOG_FILE_COUNT, (PCHAR) FILE_LOGGER_LOG_FILE_DIRECTORY_PATH, TRUE, TRUE, NULL);

#define RELEASE_FILE_LOGGER() freeFileLogger();

//////////////////////////////////////////////////////////////////////////////////////////////////////
// KVS retry strategies
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Retry configuration type
 */
typedef enum { KVS_RETRY_STRATEGY_DISABLED = 0x00, KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT = 0x01 } KVS_RETRY_STRATEGY_TYPE;

// Opaque pointers to hold retry strategy state and configuration
// depending on the underlying implementation
typedef PUINT64 PRetryStrategy;
typedef PUINT64 PRetryStrategyConfig;

/**
 * A generic retry strategy
 */
struct __KvsRetryStrategy {
    // Pointer to metadata/state/details for the retry strategy.
    // The actual data type is abstracted and will be inferred by
    // the RetryHandlerFn
    PRetryStrategy pRetryStrategy;
    // Optional configuration used to build the retry strategy. Once the retry strategy is created,
    // any changes to the config will be useless.
    PRetryStrategyConfig pRetryStrategyConfig;
    // Retry strategy type as defined in the above enum
    KVS_RETRY_STRATEGY_TYPE retryStrategyType;
};

typedef struct __KvsRetryStrategy KvsRetryStrategy;
typedef struct __KvsRetryStrategy* PKvsRetryStrategy;

/**
 * Handler to create retry strategy
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 1 PKvsRetryStrategy - OUT - pRetryStrategy field of KvsRetryStrategy struct will be populated.
 *
 * @return Status of the callback
 */
typedef STATUS (*CreateRetryStrategyFn)(PKvsRetryStrategy);

/**
 * Handler to get retry count
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 2 PUINT32 - OUT - retry count value
 *
 * @return Status of the callback
 */
typedef STATUS (*GetCurrentRetryAttemptNumberFn)(PKvsRetryStrategy, PUINT32);

/**
 * Handler to release resources associated with a retry strategy
 *
 * @param 1 PKvsRetryStrategy - KvsRetryStrategy passed by the caller.
 *
 * @return Status of the callback
 */
typedef STATUS (*FreeRetryStrategyFn)(PKvsRetryStrategy);

/**
 * Handler to execute retry strategy
 *
 * @param 1 PKvsRetryStrategy - IN - KvsRetryStrategy passed by the caller.
 * @param 2 PUINT64 - OUT - wait time value computed by ExecuteRetryStrategyFn
 *
 * @return Status of the callback
 */
typedef STATUS (*ExecuteRetryStrategyFn)(PKvsRetryStrategy, PUINT64);

struct __KvsRetryStrategyCallbacks {
    // Pointer to the function to create new retry strategy
    CreateRetryStrategyFn createRetryStrategyFn;
    // Get retry count
    GetCurrentRetryAttemptNumberFn getCurrentRetryAttemptNumberFn;
    // Pointer to the function to release allocated resources
    // associated with the retry strategy
    FreeRetryStrategyFn freeRetryStrategyFn;
    // Pointer to the actual handler for the given retry strategy
    ExecuteRetryStrategyFn executeRetryStrategyFn;
};

typedef struct __KvsRetryStrategyCallbacks KvsRetryStrategyCallbacks;
typedef struct __KvsRetryStrategyCallbacks* PKvsRetryStrategyCallbacks;
//////////////////////////////////////////////////////////////////////////////////////////////////////
// APIs for exponential backoff retry strategy
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Indicates infinite exponential retries
 */
#define KVS_INFINITE_EXPONENTIAL_RETRIES 0

/**
 * Factor for computing the exponential backoff wait time
 * The larger the value, the slower the retries will be.
 */
#define MIN_KVS_RETRY_TIME_FACTOR_MILLISECONDS     50
#define LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS   1000
#define DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS LIMIT_KVS_RETRY_TIME_FACTOR_MILLISECONDS

/**
 * Factor determining the curve of exponential wait time
 */
#define DEFAULT_KVS_EXPONENTIAL_FACTOR 2

/**
 * Maximum exponential wait time. Once the exponential wait time
 * curve reaches this value, it stays at this value. This is
 * required to put a reasonable upper bound on wait time.
 * required to put a reasonable upper bound on wait time. If not provided
 * in the config, we'll use the default value
 */
#define MIN_KVS_MAX_WAIT_TIME_MILLISECONDS     10000
#define LIMIT_KVS_MAX_WAIT_TIME_MILLISECONDS   25000
#define DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS 16000

/**
 * Maximum time between two consecutive calls to exponentialBackoffBlockingWait
 * after which the retry count will be reset to initial state. This is needed
 * to restart the exponential wait time from base value if
 */
#define MIN_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS     90000
#define LIMIT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS   120000
#define DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS MIN_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS

/**
 * Factor to get a random jitter. Jitter values [0, 300).
 * Only applicable for Fixed jitter variant
 */
#define MIN_KVS_JITTER_FACTOR_MILLISECONDS     50
#define LIMIT_KVS_JITTER_FACTOR_MILLISECONDS   600
#define DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS 300

typedef enum {
    BACKOFF_NOT_STARTED = (UINT16) 0x01,
    BACKOFF_IN_PROGRESS = (UINT16) 0x02,
    BACKOFF_TERMINATED = (UINT16) 0x03
} ExponentialBackoffStatus;

typedef enum {
    FULL_JITTER = (UINT16) 0x01,
    FIXED_JITTER = (UINT16) 0x02,
    NO_JITTER = (UINT16) 0x03,
} ExponentialBackoffJitterType;

typedef struct __ExponentialBackoffRetryStrategyConfig {
    // Max retries after which an error will be returned
    // to the application. For infinite retries, set this
    // to KVS_INFINITE_EXPONENTIAL_RETRIES.
    UINT32 maxRetryCount;
    // Maximum retry wait time in milliseconds. Once the retry wait time
    // reaches this value, subsequent retries will wait for
    // maxRetryWaitTime (plus jitter).
    UINT64 maxRetryWaitTime;
    // Factor for computing the exponential backoff wait time in milliseconds
    UINT64 retryFactorTime;
    // The minimum time (in milliseconds) between two consecutive retries
    // after which retry state will be reset i.e. retries
    // will start from initial retry state.
    UINT64 minTimeToResetRetryState;
    // Jitter type indicating how much jitter to be added
    // Default will be FULL_JITTER
    ExponentialBackoffJitterType jitterType;
    // Factor determining random jitter value (in milliseconds).
    // Jitter will be between [0, jitterFactor)
    // This parameter is only valid for jitter type FIXED_JITTER
    UINT32 jitterFactor;
} ExponentialBackoffRetryStrategyConfig, *PExponentialBackoffRetryStrategyConfig;

#define TO_EXPONENTIAL_BACKOFF_STATE(ptr)  ((PExponentialBackoffRetryStrategyState) (ptr))
#define TO_EXPONENTIAL_BACKOFF_CONFIG(ptr) ((PExponentialBackoffRetryStrategyConfig) (ptr))

typedef struct {
    ExponentialBackoffRetryStrategyConfig exponentialBackoffRetryStrategyConfig;
    ExponentialBackoffStatus status;
    UINT32 currentRetryCount;
    // The system time at which last retry happened
    UINT64 lastRetrySystemTime;
    // The actual wait time for last retry
    UINT64 lastRetryWaitTime;
    // Lock to update operations
    MUTEX retryStrategyLock;
} ExponentialBackoffRetryStrategyState, *PExponentialBackoffRetryStrategyState;

/************************************************************************
 With default exponential values, the wait times will look like following -
    ************************************
    * Retry Count *      Wait time     *
    * **********************************
    *     1       *   1000ms + jitter  *
    *     2       *   2000ms + jitter  *
    *     3       *   4000ms + jitter  *
    *     4       *   8000ms + jitter  *
    *     5       *  16000ms + jitter  *
    *     6       *  16000ms + jitter  *
    *     7       *  16000ms + jitter  *
    *     8       *  16000ms + jitter  *
    ************************************
 for FULL_JITTER variant, jitter = random number between [0, wait time)
 for FIXED_JITTER variant, jitter = random number between [0, DEFAULT_KVS_JITTER_FACTOR_MILLISECONDS)
************************************************************************/
static const ExponentialBackoffRetryStrategyConfig DEFAULT_EXPONENTIAL_BACKOFF_CONFIGURATION = {
    KVS_INFINITE_EXPONENTIAL_RETRIES,                       /* max retry count */
    DEFAULT_KVS_MAX_WAIT_TIME_MILLISECONDS,                 /* max retry wait time */
    DEFAULT_KVS_RETRY_TIME_FACTOR_MILLISECONDS,             /* factor determining exponential curve */
    DEFAULT_KVS_MIN_TIME_TO_RESET_RETRY_STATE_MILLISECONDS, /* minimum time to reset retry state */
    FULL_JITTER,                                            /* use FULL_JITTER variant */
    0                                                       /* jitter value unused for full jitter variant */
};

/**************************************************************************************************
Direct API usage example:

 void sample_configureExponentialBackoffRetryStrategy() {

     KvsRetryStrategy kvsRetryStrategy = {NULL, NULL, 0};

     //
     // [Optional] Configure with some specific exponential backoff configuration?
     //     ExponentialBackoffRetryStrategyConfig someExponentialBackoffRetryStrategyConfig;
     //     kvsRetryStrategy.pRetryStrategyConfig = &someExponentialBackoffRetryStrategyConfig;
     // Note: This config will be deep copied while creating exponential backoff retry strategy. So its okay
     //        if you pass address of a local struct.
     //

     CHK_STATUS(exponentialBackoffRetryStrategyCreate(&kvsRetryStrategy));
     CHK_STATUS(kvsRetryStrategy.pRetryStrategy != NULL);
     CHK_STATUS(kvsRetryStrategy.retryStrategyType == KVS_RETRY_STRATEGY_EXPONENTIAL_BACKOFF_WAIT);

     while (...) {
         CHK_STATUS(exponentialBackoffBlockingWait(&kvsRetryStrategy));
        // some business logic which includes service API call(s)
     }

     CHK_STATUS(exponentialBackoffStateFree(&kvsRetryStrategy));
 }

**************************************************************************************************/

/**
 * @brief Initializes exponential backoff state with provided configuration
 * This should be called once before calling exponentialBackoffBlockingWait/getExponentialBackoffRetryStrategyWaitTime.
 * If unsure about the configuration parameters, it is recommended to initialize
 * the state using initializeExponentialBackoffStateWithDefaultConfig API
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - OUT - pRetryStrategy field of KvsRetryStrategy struct will be populated.
 *                                    If PKvsRetryStrategy->PRetryStrategyConfig not provided, default config will be used
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyCreate(PKvsRetryStrategy);

/**
 * @brief
 * Computes and returns the next exponential backoff wait time
 *
 * THREAD SAFE.
 *
 * Note: This API may return STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE error code if ExponentialBackoffState
 * is not configured correctly or is corrupted. In such case, the application should re-create
 * ExponentialBackoffState using the exponentialBackoffStateCreate OR exponentialBackoffStateWithDefaultConfigCreate
 * API and then call exponentialBackoffBlockingWait
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy
 * @return Status of the function call.
 */
PUBLIC_API STATUS getExponentialBackoffRetryStrategyWaitTime(PKvsRetryStrategy, PUINT64);

/**
 * @brief
 * Computes next exponential backoff wait time and blocks the current thread for that
 * much time. This is identical to getExponentialBackoffRetryStrategyWaitTime API
 * but it waits for the actual wait time before returning. To not block the current
 * thread, use getExponentialBackoffRetryStrategyWaitTime API.
 *
 * THREAD SAFE.
 *
 * Note: This API may return STATUS_EXPONENTIAL_BACKOFF_INVALID_STATE error code if ExponentialBackoffState
 * is not configured correctly or is corrupted. In such case, the application should re-create
 * ExponentialBackoffState using the exponentialBackoffStateCreate OR exponentialBackoffStateWithDefaultConfigCreate
 * API and then call exponentialBackoffBlockingWait
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyBlockingWait(PKvsRetryStrategy);

/**
 * @brief Returns updated exponential backoff retry count when PRetryStrategy object is passed
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy for which retry state is maintained
 * @param 2 PUINT32 - OUT - Retry count
 * @return Status of the function call.
 */
PUBLIC_API STATUS getExponentialBackoffRetryCount(PKvsRetryStrategy, PUINT32);
/**
 * @brief Frees ExponentialBackoffState and its corresponding ExponentialBackoffConfig struct
 *
 * THREAD SAFE.
 *
 * @param 1 PKvsRetryStrategy - IN - Opaque Exponential backoff retry strategy.
 *                              pRetryStrategy field within PKvsRetryStrategy will be released.
 * @return Status of the function call.
 */
PUBLIC_API STATUS exponentialBackoffRetryStrategyFree(PKvsRetryStrategy);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Math Utility APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Computes power
 * Returns error when the base or exponent values are not within limits.
 * Acceptable values:
 *  base: [0, 5], exponent: [0, 27]
 *  base: [6, 100], exponent: [0, 9]
 *  base: [101, 1000], exponent: [0, 6]
 *
 * @param - UINT32 - IN - Base.
 * @param - UINT32 - IN - Exponent.
 * @param - PUINT64 - OUT - Result.
 *
 * @return - STATUS code of the execution.
 *
 */
PUBLIC_API STATUS computePower(UINT64, UINT64, PUINT64);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Threadsafe Blocking Queue APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
    volatile ATOMIC_BOOL terminate;
    volatile SIZE_T atLockCount;
    PStackQueue queue;
    MUTEX mutex;
    SEMAPHORE_HANDLE semaphore;
    CVAR terminationSignal;
} SafeBlockingQueue, *PSafeBlockingQueue;

/**
 * Create a new thread safe blocking queue
 *
 * @param - PSafeBlockingQueue* - OUT - Pointer to PSafeBlockingQueue to create.
 */
PUBLIC_API STATUS safeBlockingQueueCreate(PSafeBlockingQueue*);

/**
 * Frees and de-allocates the thread safe blocking queue
 *
 * @param - PSafeBlockingQueue - OUT - PSafeBlockingQueue to destroy.
 */
PUBLIC_API STATUS safeBlockingQueueFree(PSafeBlockingQueue);

/**
 * Clears and de-allocates all the items
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - BOOL - IN - Free objects stored in queue
 */
PUBLIC_API STATUS safeBlockingQueueClear(PSafeBlockingQueue, BOOL);

/**
 * Gets the number of items in the stack/queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PUINT32 - OUT - Pointer to integer to store count in
 */
PUBLIC_API STATUS safeBlockingQueueGetCount(PSafeBlockingQueue, PUINT32);

/**
 * Whether the thread safe blocking queue is empty
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PBOOL - OUT - Pointer to bool to store whether the queue is empty (true) or not (false)
 */
PUBLIC_API STATUS safeBlockingQueueIsEmpty(PSafeBlockingQueue, PBOOL);

/**
 * Enqueues an item in the queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - UINT64 - IN - casted pointer to object to enqueue
 */
PUBLIC_API STATUS safeBlockingQueueEnqueue(PSafeBlockingQueue, UINT64);

/**
 * Dequeues an item from the queue
 *
 * @param - PSafeBlockingQueue - IN - PSafeBlockingQueue to affect.
 * @param - PUINT64 - OUT - casted pointer to object dequeued
 */
PUBLIC_API STATUS safeBlockingQueueDequeue(PSafeBlockingQueue, PUINT64);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Threadpool APIs
//////////////////////////////////////////////////////////////////////////////////////////////////////

// windows doesn't support INT32_MAX
#define KVS_MAX_BLOCKING_QUEUE_ENTRIES ((INT32) 1024 * 1024 * 1024)

typedef struct __Threadpool {
    volatile ATOMIC_BOOL terminate;
    // threads waiting for a task
    volatile SIZE_T availableThreads;

    // tracks task
    PSafeBlockingQueue taskQueue;

    // tracks threads created
    PStackQueue threadList;

    MUTEX listMutex;
    UINT32 minThreads;
    UINT32 maxThreads;
} Threadpool, *PThreadpool;

/**
 * Create a new threadpool
 *
 * @param - PThreadpool* - OUT - Pointer to PThreadpool to create
 * @param - UINT32 - IN - minimum threads the threadpool must maintain (cannot be 0)
 * @param - UINT32 - IN - maximum threads the threadpool is allowed to create
 *                       (cannot be 0, must be greater than minimum)
 */
PUBLIC_API STATUS threadpoolCreate(PThreadpool*, UINT32, UINT32);

/**
 * Destroy a threadpool
 *
 * @param - PThreadpool - IN - PThreadpool to destroy
 */
PUBLIC_API STATUS threadpoolFree(PThreadpool pThreadpool);

/**
 * Amount of threads currently tracked by this threadpool
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - PUINT32 - OUT - Pointer to integer to store the count
 */
PUBLIC_API STATUS threadpoolTotalThreadCount(PThreadpool pThreadpool, PUINT32 pCount);

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created.
 *          STATUS_FAILED/ if the threadpool is already at its
 *          predetermined max.
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - startRoutine - IN - function pointer to run in thread
 * @param - PVOID - IN - custom data to send to function pointer
 */
PUBLIC_API STATUS threadpoolTryAdd(PThreadpool, startRoutine, PVOID);

/**
 * Create a thread with the given task.
 * returns: STATUS_SUCCESS if a thread was already available
 *          or if a new thread was created
 *          or if the task was added to the queue for the next thread.
 *          STATUS_FAILED/ if the threadpool queue is full.
 *
 * @param - PThreadpool - IN - PThreadpool to modify
 * @param - startRoutine - IN - function pointer to run in thread
 * @param - PVOID - IN - custom data to send to function pointer
 */
PUBLIC_API STATUS threadpoolPush(PThreadpool, startRoutine, PVOID);

/**
 * @brief Checks if an environment variable is enabled.
 *
 * @param - PCHAR - IN - The label of the environment variable to check for.
 *
 * @return - BOOL - TRUE if the environment variable is enabled, FALSE otherwise.
 */
PUBLIC_API BOOL isEnvVarEnabled(PCHAR);

#ifdef __cplusplus
}
#endif

#endif // __UTILS_H__
