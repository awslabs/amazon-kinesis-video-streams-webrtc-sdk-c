/**
 * Main public include file
 */
#ifndef __CONTENT_VIEW_INCLUDE__
#define __CONTENT_VIEW_INCLUDE__

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <com/amazonaws/kinesis/video/common/CommonDefs.h>
#include <com/amazonaws/kinesis/video/common/PlatformUtils.h>

// IMPORTANT! Some of the headers are not tightly packed!
////////////////////////////////////////////////////
// Public headers
////////////////////////////////////////////////////
#include <com/amazonaws/kinesis/video/utils/Include.h>
#include <com/amazonaws/kinesis/video/heap/Include.h>

/**
 * Minimum number of items in the window
 */
#define MIN_CONTENT_VIEW_ITEMS 10

/**
 * Minimum buffer duration value
 */
#define MIN_CONTENT_VIEW_BUFFER_DURATION 0

/**
 * Current version of the content view object
 */
#define CONTENT_VIEW_CURRENT_VERSION 0

////////////////////////////////////////////////////
// Status return codes
////////////////////////////////////////////////////
#define STATUS_VIEW_BASE                      0x30000000
#define STATUS_MIN_CONTENT_VIEW_ITEMS         STATUS_VIEW_BASE + 0x00000001
#define STATUS_INVALID_CONTENT_VIEW_DURATION  STATUS_VIEW_BASE + 0x00000002
#define STATUS_CONTENT_VIEW_NO_MORE_ITEMS     STATUS_VIEW_BASE + 0x00000003
#define STATUS_CONTENT_VIEW_INVALID_INDEX     STATUS_VIEW_BASE + 0x00000004
#define STATUS_CONTENT_VIEW_INVALID_TIMESTAMP STATUS_VIEW_BASE + 0x00000005
#define STATUS_INVALID_CONTENT_VIEW_LENGTH    STATUS_VIEW_BASE + 0x00000006

////////////////////////////////////////////////////
// Main structure declarations
////////////////////////////////////////////////////

/**
 * Flags definitions
 *
 * NOTE: The high order 16 bits will be used to store the data offset
 */
#define ITEM_FLAG_NONE               0
#define ITEM_FLAG_STREAM_START       (0x1 << 0)
#define ITEM_FLAG_FRAGMENT_START     (0x1 << 1)
#define ITEM_FLAG_BUFFERING_ACK      (0x1 << 2)
#define ITEM_FLAG_RECEIVED_ACK       (0x1 << 3)
#define ITEM_FLAG_FRAGMENT_END       (0x1 << 4)
#define ITEM_FLAG_PERSISTED_ACK      (0x1 << 5)
#define ITEM_FLAG_SKIP_ITEM          (0x1 << 6)
#define ITEM_FLAG_STREAM_START_DEBUG (0x1 << 15)

/**
 * Macros for checking/setting/clearing for various flags
 */
#define CHECK_ITEM_FRAGMENT_START(f)     (((f) & ITEM_FLAG_FRAGMENT_START) != ITEM_FLAG_NONE)
#define CHECK_ITEM_BUFFERING_ACK(f)      (((f) & ITEM_FLAG_BUFFERING_ACK) != ITEM_FLAG_NONE)
#define CHECK_ITEM_RECEIVED_ACK(f)       (((f) & ITEM_FLAG_RECEIVED_ACK) != ITEM_FLAG_NONE)
#define CHECK_ITEM_STREAM_START(f)       (((f) & ITEM_FLAG_STREAM_START) != ITEM_FLAG_NONE)
#define CHECK_ITEM_FRAGMENT_END(f)       (((f) & ITEM_FLAG_FRAGMENT_END) != ITEM_FLAG_NONE)
#define CHECK_ITEM_PERSISTED_ACK(f)      (((f) & ITEM_FLAG_PERSISTED_ACK) != ITEM_FLAG_NONE)
#define CHECK_ITEM_SKIP_ITEM(f)          (((f) & ITEM_FLAG_SKIP_ITEM) != ITEM_FLAG_NONE)
#define CHECK_ITEM_STREAM_START_DEBUG(f) (((f) & ITEM_FLAG_STREAM_START_DEBUG) != ITEM_FLAG_NONE)

#define SET_ITEM_FRAGMENT_START(f)     ((f) |= ITEM_FLAG_FRAGMENT_START)
#define SET_ITEM_BUFFERING_ACK(f)      ((f) |= ITEM_FLAG_BUFFERING_ACK)
#define SET_ITEM_RECEIVED_ACK(f)       ((f) |= ITEM_FLAG_RECEIVED_ACK)
#define SET_ITEM_STREAM_START(f)       ((f) |= ITEM_FLAG_STREAM_START)
#define SET_ITEM_FRAGMENT_END(f)       ((f) |= ITEM_FLAG_FRAGMENT_END)
#define SET_ITEM_PERSISTED_ACK(f)      ((f) |= ITEM_FLAG_PERSISTED_ACK)
#define SET_ITEM_SKIP_ITEM(f)          ((f) |= ITEM_FLAG_SKIP_ITEM)
#define SET_ITEM_STREAM_START_DEBUG(f) ((f) |= ITEM_FLAG_STREAM_START_DEBUG)

#define CLEAR_ITEM_FRAGMENT_START(f)     ((f) &= ~ITEM_FLAG_FRAGMENT_START)
#define CLEAR_ITEM_BUFFERING_ACK(f)      ((f) &= ~ITEM_FLAG_BUFFERING_ACK)
#define CLEAR_ITEM_RECEIVED_ACK(f)       ((f) &= ~ITEM_FLAG_RECEIVED_ACK)
#define CLEAR_ITEM_STREAM_START(f)       ((f) &= ~ITEM_FLAG_STREAM_START)
#define CLEAR_ITEM_FRAGMENT_END(f)       ((f) &= ~ITEM_FLAG_FRAGMENT_END)
#define CLEAR_ITEM_PERSISTED_ACK(f)      ((f) &= ~ITEM_FLAG_PERSISTED_ACK)
#define CLEAR_ITEM_SKIP_ITEM(f)          ((f) &= ~ITEM_FLAG_SKIP_ITEM)
#define CLEAR_ITEM_STREAM_START_DEBUG(f) ((f) &= ~ITEM_FLAG_STREAM_START_DEBUG)

#define GET_ITEM_DATA_OFFSET(f)    ((UINT16) ((f) >> 16))
#define SET_ITEM_DATA_OFFSET(f, o) ((f) = ((f) & 0x0000ffff) | (((UINT16) (o)) << 16))

/**
 * This is a sentinel indicating an invalid index value
 */
#define INVALID_VIEW_INDEX_VALUE (0xFFFFFFFFFFFFFFFFULL)

/**
 * Checks for the index validity
 */
#define IS_VALID_VIEW_INDEX(h) ((h) != INVALID_VIEW_INDEX_VALUE)

/**
 * The representation of the item in the cache.
 *
 * IMPORTANT!!! This structure should be tightly packed without explicit compiler directives
 */
typedef struct {
    // Id of the item
    UINT64 index;

    // The timestamp of the item
    UINT64 timestamp;

    // The timestamp used to map acks from backend
    UINT64 ackTimestamp;

    // The duration of the item
    UINT64 duration;

    // Length of the data in bytes
    UINT32 length;

    // Whether this item depends on others (ex. IFrame for frames)
    UINT32 flags;

    // The data allocation handle
    ALLOCATION_HANDLE handle;
} ViewItem, *PViewItem;

/*
 * Determine how content view drop frames when overflow is detected
 */
typedef enum {
    // drop single view item from the buffer
    CONTENT_VIEW_OVERFLOW_POLICY_DROP_TAIL_VIEW_ITEM,

    // drop entire fragment
    CONTENT_VIEW_OVERFLOW_POLICY_DROP_UNTIL_FRAGMENT_START,
} CONTENT_VIEW_OVERFLOW_POLICY;

/**
 * ContentView structure
 */
typedef struct __ContentView ContentView;
struct __ContentView {
    UINT32 version;
    // NOTE: The internal structure follows
};

typedef struct __ContentView* PContentView;

/**
 * Callback functions definitions
 */

/**
 * Callback to notify about an item falling out of the window
 *
 * param 1 PContentView - Content view which notifies the caller - this pointer
 * param 2 UINT64 - Custom pass-through value
 * param 3 PViewItem - item that's about to fall off from the window
 * param 4 BOOL - whether the item was current - i.e. it wasn't consumed
 *
 * @return VOID
 */
typedef VOID (*ContentViewItemRemoveNotificationCallbackFunc)(PContentView, UINT64, PViewItem, BOOL);

////////////////////////////////////////////////////
// Public functions
////////////////////////////////////////////////////

/**
 * Create the ContentView object
 *
 * @UINT32 - Max number of items in the window
 * @UINT64 - Duration of items to keep in the window
 * @ContentViewItemRemoveNotificationCallbackFunc - Optional - Callback function
 * @UINT64 - Custom data to pass to the callback
 * @CONTENT_VIEW_OVERFLOW_STRATEGY - how content view will drop frame when overflow happens
 * @PContentView* - returns the newly created object
 *
 * @return - STATUS code of the execution
 **/
PUBLIC_API STATUS createContentView(UINT32, UINT64, ContentViewItemRemoveNotificationCallbackFunc, UINT64, CONTENT_VIEW_OVERFLOW_POLICY,
                                    PContentView*);

/**
 * Frees and de-allocates the memory of the ContentView and it's sub-objects
 *
 * NOTE: This function is idempotent - can be called at various stages of construction.
 *
 * @PContentView - the object to free
 */
PUBLIC_API STATUS freeContentView(PContentView);

/**
 * Checks whether an item with a given index exists
 *
 * PContentView - Content view
 * UINT64 - item index
 * PBOOL - whether the item exists
 *
 */
PUBLIC_API STATUS contentViewItemExists(PContentView, UINT64, PBOOL);

/**
 * Checks whether an item with a given timestamp is in the range
 *
 * PContentView - Content view
 * UINT64 - item timestamp
 * BOOL - whether check against ackTimestamp or contentView timestamp
 * PBOOL - whether the item exists
 *
 */
PUBLIC_API STATUS contentViewTimestampInRange(PContentView, UINT64, BOOL, PBOOL);

/**
 * Gets an item from the current index and advances the index. Iterates to the next item.
 *
 * PContentView - Content view
 * PViewItem* - The current item pointer
 *
 */
PUBLIC_API STATUS contentViewGetNext(PContentView, PViewItem*);

/**
 * Gets an item from the given index. Current remains untouched.
 *
 * PContentView - Content view
 * UINT64 - the index of the item
 * PViewItem* - The current item pointer
 *
 */
PUBLIC_API STATUS contentViewGetItemAt(PContentView, UINT64, PViewItem*);

/**
 * Finds an item corresponding to the timestamp. Current remains untouched.
 *
 * PContentView - Content view
 * UINT64 - the index of the item
 * BOOL - whether check against ackTimestamp or contentView timestamp
 * PViewItem* - The current item pointer
 *
 */
PUBLIC_API STATUS contentViewGetItemWithTimestamp(PContentView, UINT64, BOOL, PViewItem*);

/**
 * Gets the current index
 *
 * PContentView - Content view
 * PUINT64 - current item index
 *
 */
PUBLIC_API STATUS contentViewGetCurrentIndex(PContentView, PUINT64);

/**
 * Sets the current item at a given index
 *
 * PContentView - Content view
 * UINT64 - new current item index
 *
 */
PUBLIC_API STATUS contentViewSetCurrentIndex(PContentView, UINT64);

/**
 * Rolls back the current item by a specified duration or as far as the tail.
 * Optionally looks for a key-framed item.
 *
 * PContentView - Content view.
 * UINT64 - Timestamp to set the current to. NOTE: the timestamp will be inclusive.
 * BOOL - Whether to look for a key framed item.
 * BOOL - Whether to stop at the last buffering ACK.
 *
 */
PUBLIC_API STATUS contentViewRollbackCurrent(PContentView, UINT64, BOOL, BOOL);

/**
 * Resets the current item to the tail position
 *
 * PContentView - Content view
 *
 */
PUBLIC_API STATUS contentViewResetCurrent(PContentView);

/**
 * Gets the tail item
 *
 * PContentView - Content view
 * PViewItem - The tail item
 *
 */
PUBLIC_API STATUS contentViewGetTail(PContentView, PViewItem*);

/**
 * Gets the head item
 *
 * PContentView - Content view
 * PViewItem - The head item
 *
 */
PUBLIC_API STATUS contentViewGetHead(PContentView, PViewItem*);

/**
 * Gets the overall allocation size
 *
 * PContentView - Content view
 * PUINT32 - OUT - The overall allocation size
 *
 */
PUBLIC_API STATUS contentViewGetAllocationSize(PContentView, PUINT32);

/**
 * Adds an item to the head of the view
 *
 * PContentView - Content view
 * UINT64 - item timestamp (decoding timestamp)
 * UINT64 - item presentation timestamp
 * UINT64 - item duration
 * ALLOCATION_HANDLE - Allocation handle
 * UINT32 - Byte offset of the data from the beginning of the allocation
 * UINT32 - Size of the data in bytes
 * UINT32 - Item flags.
 *
 */
PUBLIC_API STATUS contentViewAddItem(PContentView, UINT64, UINT64, UINT64, ALLOCATION_HANDLE, UINT32, UINT32, UINT32);

/**
 * Gets the view's temporal window duration
 *
 * PContentView - Content view
 * PUINT64 - current window duration - from the head timestamp to the current timestamp
 * PUINT64 - Optional - entire window duration - from the head timestamp to the tail timestamp
 *
 */
PUBLIC_API STATUS contentViewGetWindowDuration(PContentView, PUINT64, PUINT64);

/**
 * Gets the view's window item count and entire window item count (optionally)
 *
 * PContentView - Content view
 * PUINT64 - current window item count - the number of elements from the current to the head
 * PUINT64 - Optional - entire window item count - the number of elements in the entire window
 *
 */
PUBLIC_API STATUS contentViewGetWindowItemCount(PContentView, PUINT64, PUINT64);

/**
 * Gets the view's window allocation size and entire window allocation size (optionally)
 *
 * PContentView - Content view
 * PUINT64 - current window allocation size - the size of all allocations from the current to the head
 * PUINT64 - Optional - entire window size - the size of all allocations in the entire window
 *
 */
PUBLIC_API STATUS contentViewGetWindowAllocationSize(PContentView, PUINT64, PUINT64);

/**
 * Trims the tail till the given item. The remove callbacks will be fired and the current shifted if needed.
 *
 * PContentView - Content view
 * UINT64 - Trim the tail to this item
 *
 */
PUBLIC_API STATUS contentViewTrimTail(PContentView, UINT64);

/**
 * Trims the tail based on CONTENT_VIEW_OVERFLOW_POLICY
 *
 * PContentView - Content view
 *
 */
PUBLIC_API STATUS contentViewTrimTailItems(PContentView);

/**
 * Removes all items in the content view. Calls the remove callback if specified.
 *
 * PContentView - Content view
 */
PUBLIC_API STATUS contentViewRemoveAll(PContentView);

/**
 * Checks whether there is an available slot both temporary and depth-wise
 * in the content view without evicting a tail item.
 *
 * PContentView - Content view
 * PBOOL - OUT OPT - Whether there is available space to put an item without tail eviction.
 */
PUBLIC_API STATUS contentViewCheckAvailability(PContentView, PBOOL);

#ifdef __cplusplus
}
#endif
#endif /* __CONTENT_VIEW_INCLUDE__ */
