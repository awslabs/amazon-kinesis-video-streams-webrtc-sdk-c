#include "Include_i.h"

/**
 * Create a new stack/queue
 */
STATUS stackQueueCreate(PStackQueue* ppStackQueue)
{
    return singleListCreate(ppStackQueue);
}

/**
 * Frees and de-allocates the stack queue
 */
STATUS stackQueueFree(PStackQueue pStackQueue)
{
    return singleListFree(pStackQueue);
}

/**
 * Clears and de-allocates all the items
 */
STATUS stackQueueClear(PStackQueue pStackQueue, BOOL freeData)
{
    return singleListClear(pStackQueue, freeData);
}

/**
 * Gets the number of items in the stack/queue
 */
STATUS stackQueueGetCount(PStackQueue pStackQueue, PUINT32 pCount)
{
    return singleListGetNodeCount(pStackQueue, pCount);
}

/**
 * Whether the stack queue is empty
 */
STATUS stackQueueIsEmpty(PStackQueue pStackQueue, PBOOL pIsEmpty)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 count;

    // The call is idempotent so we shouldn't fail
    CHK(pStackQueue != NULL && pIsEmpty != NULL, STATUS_NULL_ARG);

    CHK_STATUS(singleListGetNodeCount(pStackQueue, &count));

    *pIsEmpty = (count == 0);

CleanUp:

    return retStatus;
}

/**
 * Pushes an item onto the stack
 */
STATUS stackQueuePush(PStackQueue pStackQueue, UINT64 item)
{
    return singleListInsertItemHead(pStackQueue, item);
}

/**
 * Pops an item from the stack
 */
STATUS stackQueuePop(PStackQueue pStackQueue, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK_STATUS(stackQueuePeek(pStackQueue, pItem));
    CHK_STATUS(singleListDeleteHead(pStackQueue));

CleanUp:

    return retStatus;
}

/**
 * Peeks an item from the stack without popping
 */
STATUS stackQueuePeek(PStackQueue pStackQueue, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pHead;

    CHK_STATUS(singleListGetHeadNode(pStackQueue, &pHead));
    CHK(pHead != NULL, STATUS_NOT_FOUND);
    CHK_STATUS(singleListGetNodeData(pHead, pItem));

CleanUp:

    return retStatus;
}

/**
 * Gets the index of an item
 */
STATUS stackQueueGetIndexOf(PStackQueue pStackQueue, UINT64 item, PUINT32 pIndex)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 index = 0;
    UINT64 data;
    BOOL found = FALSE;
    PSingleListNode pCurNode = NULL;

    CHK(pStackQueue != NULL && pIndex != NULL, STATUS_NULL_ARG);

    CHK_STATUS(singleListGetHeadNode(pStackQueue, &pCurNode));

    while (pCurNode != NULL) {
        CHK_STATUS(singleListGetNodeData(pCurNode, &data));
        if (data == item) {
            found = TRUE;
            break;
        }

        pCurNode = pCurNode->pNext;
        index++;
    }

    CHK(found, STATUS_NOT_FOUND);

    *pIndex = index;

CleanUp:

    return retStatus;
}

/**
 * Gets an item at the given index from the stack without popping
 */
STATUS stackQueueGetAt(PStackQueue pStackQueue, UINT32 index, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK(pItem != NULL, STATUS_NOT_FOUND);
    CHK_STATUS(singleListGetNodeDataAt(pStackQueue, index, pItem));

CleanUp:

    return retStatus;
}

/**
 * Sets an item value at the given index from the stack without popping
 */
STATUS stackQueueSetAt(PStackQueue pStackQueue, UINT32 index, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pNode = NULL;
    CHK_STATUS(singleListGetNodeAt(pStackQueue, index, &pNode));

    // Sets the data.
    // NOTE: If the data is an allocation then it might be
    // lost so it's up to the called to ensure it's handled properly
    pNode->data = item;

CleanUp:

    return retStatus;
}

/**
 * Removes an item from the stack/queue at the given index
 */
STATUS stackQueueRemoveAt(PStackQueue pStackQueue, UINT32 index)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pNode;

    CHK_STATUS(singleListGetNodeAt(pStackQueue, index, &pNode));
    CHK_STATUS(singleListDeleteNode(pStackQueue, pNode));

CleanUp:

    return retStatus;
}

/**
 * Removes the item at the given item
 */
STATUS stackQueueRemoveItem(PStackQueue pStackQueue, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    UINT32 index;

    CHK_STATUS(stackQueueGetIndexOf(pStackQueue, item, &index));
    CHK_STATUS(stackQueueRemoveAt(pStackQueue, index));

CleanUp:

    return retStatus;
}

/**
 * Enqueues an item in the queue
 */
STATUS stackQueueEnqueue(PStackQueue pStackQueue, UINT64 item)
{
    return singleListInsertItemTail(pStackQueue, item);
}

/**
 * Dequeues an item from the queue
 */
STATUS stackQueueDequeue(PStackQueue pStackQueue, PUINT64 pItem)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pHead;

    CHK_STATUS(singleListGetHeadNode(pStackQueue, &pHead));
    CHK(pHead != NULL, STATUS_NOT_FOUND);
    CHK_STATUS(singleListGetNodeData(pHead, pItem));
    CHK_STATUS(singleListDeleteHead(pStackQueue));

CleanUp:

    return retStatus;
}

/**
 * Gets the iterator
 */
STATUS stackQueueGetIterator(PStackQueue pStackQueue, PStackQueueIterator pIterator)
{
    STATUS retStatus = STATUS_SUCCESS;
    CHK_STATUS(singleListGetHeadNode(pStackQueue, pIterator));

CleanUp:

    return retStatus;
}

/**
 * Iterates to next
 */
STATUS stackQueueIteratorNext(PStackQueueIterator pIterator)
{
    STATUS retStatus = STATUS_SUCCESS;
    StackQueueIterator nextIterator;

    CHK(pIterator != NULL, STATUS_NULL_ARG);
    CHK(*pIterator != NULL, STATUS_NOT_FOUND);
    CHK_STATUS(singleListGetNextNode(*pIterator, &nextIterator));
    *pIterator = nextIterator;

CleanUp:

    return retStatus;
}

/**
 * Gets the item and the index
 */
STATUS stackQueueIteratorGetItem(StackQueueIterator iterator, PUINT64 pData)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(iterator != NULL, STATUS_NOT_FOUND);
    CHK_STATUS(singleListGetNodeData(iterator, pData));

CleanUp:

    return retStatus;
}

/**
 * Inserts item into queue after given index
 */
STATUS stackQueueEnqueueAfterIndex(PStackQueue pStackQueue, UINT32 index, UINT64 item)
{
    STATUS retStatus = STATUS_SUCCESS;
    PSingleListNode pNode;

    CHK_STATUS(singleListGetNodeAt(pStackQueue, index, &pNode));
    CHK_STATUS(singleListInsertItemAfter(pStackQueue, pNode, item));

CleanUp:

    return retStatus;
}
