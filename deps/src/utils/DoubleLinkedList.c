#include "Include_i.h"

/**
 * Create a new double linked list
 */
STATUS doubleListCreate(PDoubleList* ppList)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleList pList = NULL;

    CHK(ppList != NULL, STATUS_NULL_ARG);

    // Allocate the main structure
    pList = (PDoubleList) MEMCALLOC(1, SIZEOF(DoubleList));
    CHK(pList != NULL, STATUS_NOT_ENOUGH_MEMORY);

    // The list contents are automatically set to 0s by calloc. Just assign and return
    *ppList = pList;

CleanUp:

    return retStatus;
}

/**
 * Frees a double linked list
 */
STATUS doubleListFree(PDoubleList pList)
{
    STATUS retStatus = STATUS_SUCCESS;

    // The call is idempotent so we shouldn't fail
    CHK(pList != NULL, retStatus);

    // We shouldn't fail here even if clear fails
    doubleListClear(pList, FALSE);

    // Free the structure itself
    MEMFREE(pList);

CleanUp:

    return retStatus;
}

/**
 * Clears a double linked list
 */
STATUS doubleListClear(PDoubleList pList, BOOL freeData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pCurNode = NULL;
    PDoubleListNode pNextNode = NULL;

    CHK(pList != NULL, STATUS_NULL_ARG);

    // Iterate through and free individual items without re-linking - faster
    pCurNode = pList->pHead;
    while (pCurNode != NULL) {
        pNextNode = pCurNode->pNext;
        if (freeData && ((PVOID) pCurNode->data != NULL)) {
            MEMFREE((PVOID) pCurNode->data);
        }
        MEMFREE(pCurNode);
        pCurNode = pNextNode;
    }

    // Reset the list
    pList->count = 0;
    pList->pHead = pList->pTail = NULL;

CleanUp:

    return retStatus;
}

/**
 * Insert a node in the head position in the list
 */
STATUS doubleListInsertNodeHead(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListInsertNodeHeadInternal(pList, pNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a new node with the data at the head position in the list
 */
STATUS doubleListInsertItemHead(PDoubleList pList, UINT64 data)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode;

    CHK(pList != NULL, STATUS_NULL_ARG);

    // Allocate the node and insert
    CHK_STATUS(doubleListAllocNode(data, &pNode));
    CHK_STATUS(doubleListInsertNodeHeadInternal(pList, pNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a node in the tail position in the list
 */
STATUS doubleListInsertNodeTail(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListInsertNodeTailInternal(pList, pNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a new node with the data at the tail position in the list
 */
STATUS doubleListInsertItemTail(PDoubleList pList, UINT64 data)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode;

    CHK(pList != NULL, STATUS_NULL_ARG);

    // Allocate the node and insert
    CHK_STATUS(doubleListAllocNode(data, &pNode));
    CHK_STATUS(doubleListInsertNodeTailInternal(pList, pNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a node before a given node
 */
STATUS doubleListInsertNodeBefore(PDoubleList pList, PDoubleListNode pNode, PDoubleListNode pInsertNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL && pInsertNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListInsertNodeBeforeInternal(pList, pNode, pInsertNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a new node with the data before a given node
 */
STATUS doubleListInsertItemBefore(PDoubleList pList, PDoubleListNode pNode, UINT64 data)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pInsertNode;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);

    // Allocate the node and insert
    CHK_STATUS(doubleListAllocNode(data, &pInsertNode));
    CHK_STATUS(doubleListInsertNodeBeforeInternal(pList, pNode, pInsertNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a node after a given node
 */
STATUS doubleListInsertNodeAfter(PDoubleList pList, PDoubleListNode pNode, PDoubleListNode pInsertNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL && pInsertNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListInsertNodeAfterInternal(pList, pNode, pInsertNode));

CleanUp:

    return retStatus;
}

/**
 * Insert a new node with the data after a given node
 */
STATUS doubleListInsertItemAfter(PDoubleList pList, PDoubleListNode pNode, UINT64 data)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pInsertNode;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);

    // Allocate the node and insert
    CHK_STATUS(doubleListAllocNode(data, &pInsertNode));
    CHK_STATUS(doubleListInsertNodeAfterInternal(pList, pNode, pInsertNode));

CleanUp:

    return retStatus;
}

/**
 * Removes and deletes the head
 */
STATUS doubleListDeleteHead(PDoubleList pList)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode;

    CHK(pList != NULL, STATUS_NULL_ARG);

    // Check if we need to do anything
    CHK(pList->pHead != NULL, retStatus);
    pNode = pList->pHead;
    CHK_STATUS(doubleListRemoveNodeInternal(pList, pList->pHead));

    // Delete the node
    MEMFREE(pNode);

CleanUp:

    return retStatus;
}

/**
 * Removes and deletes the tail
 */
STATUS doubleListDeleteTail(PDoubleList pList)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode;

    CHK(pList != NULL, STATUS_NULL_ARG);

    // Check if we need to do anything
    CHK(pList->pTail != NULL, retStatus);
    pNode = pList->pTail;
    CHK_STATUS(doubleListRemoveNodeInternal(pList, pList->pTail));

    // Delete the node
    MEMFREE(pNode);

CleanUp:

    return retStatus;
}

/**
 * Removes the specified node
 */
STATUS doubleListRemoveNode(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListRemoveNodeInternal(pList, pNode));

CleanUp:

    return retStatus;
}

/**
 * Removes and deletes the specified node
 */
STATUS doubleListDeleteNode(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pNode != NULL, STATUS_NULL_ARG);
    CHK_STATUS(doubleListRemoveNodeInternal(pList, pNode));

    // Delete the node
    MEMFREE(pNode);

CleanUp:

    return retStatus;
}

/**
 * Gets the head node
 */
STATUS doubleListGetHeadNode(PDoubleList pList, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && ppNode != NULL, STATUS_NULL_ARG);
    *ppNode = pList->pHead;

CleanUp:

    return retStatus;
}

/**
 * Gets the tail node
 */
STATUS doubleListGetTailNode(PDoubleList pList, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && ppNode != NULL, STATUS_NULL_ARG);
    *ppNode = pList->pTail;

CleanUp:

    return retStatus;
}

/**
 * Gets the node at the specified index
 */
STATUS doubleListGetNodeAt(PDoubleList pList, UINT32 index, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && ppNode != NULL, STATUS_NULL_ARG);
    CHK(index < pList->count, STATUS_INVALID_ARG);

    CHK_STATUS(doubleListGetNodeAtInternal(pList, index, ppNode));

CleanUp:

    return retStatus;
}

/**
 * Gets the node data at the specified index
 */
STATUS doubleListGetNodeDataAt(PDoubleList pList, UINT32 index, PUINT64 pData)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode = NULL;

    CHK(pList != NULL && pData != NULL, STATUS_NULL_ARG);
    CHK(index < pList->count, STATUS_INVALID_ARG);

    CHK_STATUS(doubleListGetNodeAtInternal(pList, index, &pNode));
    *pData = pNode->data;

CleanUp:

    return retStatus;
}

/**
 * Gets the node data
 */
STATUS doubleListGetNodeData(PDoubleListNode pNode, PUINT64 pData)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pNode != NULL && pData != NULL, STATUS_NULL_ARG);
    *pData = pNode->data;

CleanUp:

    return retStatus;
}

/**
 * Gets the next node
 */
STATUS doubleListGetNextNode(PDoubleListNode pNode, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pNode != NULL && ppNode != NULL, STATUS_NULL_ARG);
    *ppNode = pNode->pNext;

CleanUp:

    return retStatus;
}

/**
 * Gets the previous node
 */
STATUS doubleListGetPrevNode(PDoubleListNode pNode, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pNode != NULL && ppNode != NULL, STATUS_NULL_ARG);
    *ppNode = pNode->pPrev;

CleanUp:

    return retStatus;
}

/**
 * Gets the count of nodes in the list
 */
STATUS doubleListGetNodeCount(PDoubleList pList, PUINT32 pCount)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pList != NULL && pCount != NULL, STATUS_NULL_ARG);
    *pCount = pList->count;

CleanUp:

    return retStatus;
}

/////////////////////////////////////////////////////////////////////////////////
// Internal operations
/////////////////////////////////////////////////////////////////////////////////
STATUS doubleListAllocNode(UINT64 data, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode = (PDoubleListNode) MEMCALLOC(1, SIZEOF(DoubleListNode));
    CHK(pNode != NULL, STATUS_NOT_ENOUGH_MEMORY);

    pNode->data = data;
    *ppNode = pNode;

CleanUp:

    return retStatus;
}

STATUS doubleListInsertNodeHeadInternal(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Fix-up the node
    pNode->pPrev = NULL;
    pNode->pNext = pList->pHead;

    // Fix-up the head node
    if (pList->pHead != NULL) {
        pList->pHead->pPrev = pNode;
    } else {
        // In this case the tail should be NULL as well
        CHK(pList->pTail == NULL, STATUS_INTERNAL_ERROR);

        // Fix-up the tail
        pList->pTail = pNode;
    }

    // Fix-up the head
    pList->pHead = pNode;

    // Increment the count
    pList->count++;

CleanUp:

    return retStatus;
}

STATUS doubleListInsertNodeTailInternal(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Fix-up the node
    pNode->pPrev = pList->pTail;
    pNode->pNext = NULL;

    // Fix-up the tail node
    if (pList->pTail != NULL) {
        pList->pTail->pNext = pNode;
    } else {
        // In this case the head should be NULL as well
        CHK(pList->pHead == NULL, STATUS_INTERNAL_ERROR);

        // Fix-up the head
        pList->pHead = pNode;
    }

    // Fix-up the tail
    pList->pTail = pNode;

    // Increment the count
    pList->count++;

CleanUp:

    return retStatus;
}

STATUS doubleListInsertNodeBeforeInternal(PDoubleList pList, PDoubleListNode pNode, PDoubleListNode pInsertNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Fix-up the insert node
    pInsertNode->pPrev = pNode->pPrev;
    pInsertNode->pNext = pNode;

    // Fix-up the prev->next
    if (pNode->pPrev != NULL) {
        pNode->pPrev->pNext = pInsertNode;
    } else {
        // In this case we should have the head pointing to pNode
        CHK(pList->pHead == pNode, STATUS_INTERNAL_ERROR);

        // Fix-up the head
        pList->pHead = pInsertNode;
    }

    // Fix-up the current node prev
    pNode->pPrev = pInsertNode;

    // Increment the count
    pList->count++;

CleanUp:

    return retStatus;
}

STATUS doubleListInsertNodeAfterInternal(PDoubleList pList, PDoubleListNode pNode, PDoubleListNode pInsertNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Fix-up the insert node
    pInsertNode->pPrev = pNode;
    pInsertNode->pNext = pNode->pNext;

    // Fix-up the next->prev
    if (pNode->pNext != NULL) {
        pNode->pNext->pPrev = pInsertNode;
    } else {
        // In this case we should have the tail pointing to pNode
        CHK(pList->pTail == pNode, STATUS_INTERNAL_ERROR);

        // Fix-up the tail
        pList->pTail = pInsertNode;
    }

    // Fix-up the current node next
    pNode->pNext = pInsertNode;

    // Increment the count
    pList->count++;

CleanUp:

    return retStatus;
}

STATUS doubleListRemoveNodeInternal(PDoubleList pList, PDoubleListNode pNode)
{
    STATUS retStatus = STATUS_SUCCESS;

    // Fix-up the prev and next
    if (pNode->pPrev != NULL) {
        pNode->pPrev->pNext = pNode->pNext;
    } else {
        // In this case the head should point to pNode
        CHK(pList->pHead == pNode, STATUS_INTERNAL_ERROR);

        pList->pHead = pNode->pNext;
    }

    if (pNode->pNext != NULL) {
        pNode->pNext->pPrev = pNode->pPrev;
    } else {
        // In this case the tail should point to pNode
        CHK(pList->pTail == pNode, STATUS_INTERNAL_ERROR);

        pList->pTail = pNode->pPrev;
    }

    // Set the node pointers
    pNode->pNext = pNode->pPrev = NULL;

    // Decrement the count
    pList->count--;

CleanUp:

    return retStatus;
}

STATUS doubleListGetNodeAtInternal(PDoubleList pList, UINT32 index, PDoubleListNode* ppNode)
{
    STATUS retStatus = STATUS_SUCCESS;
    PDoubleListNode pNode = pList->pHead;
    UINT32 i;

    for (i = 0; i < index; i++) {
        // We shouldn't have NULL unless the list is corrupted
        CHK(pNode != NULL, STATUS_INTERNAL_ERROR);
        pNode = pNode->pNext;
    }

    *ppNode = pNode;

CleanUp:

    return retStatus;
}

STATUS doubleListAppendList(PDoubleList pDstList, PDoubleList* ppListToAppend)
{
    STATUS retStatus = STATUS_SUCCESS;

    CHK(pDstList != NULL && ppListToAppend != NULL, STATUS_NULL_ARG);
    PDoubleList pListToAppend = *ppListToAppend;

    CHK(pListToAppend != NULL, retStatus);

    if (pDstList->count == 0) {
        pDstList->pHead = pListToAppend->pHead;
        pDstList->pTail = pListToAppend->pTail;
    } else if (pListToAppend->count != 0) {
        pDstList->pTail->pNext = pListToAppend->pHead;
        pListToAppend->pHead->pPrev = pDstList->pTail;
        pDstList->pTail = pListToAppend->pTail;
    }

    pDstList->count += pListToAppend->count;
    MEMFREE(pListToAppend);
    *ppListToAppend = NULL;

CleanUp:

    return retStatus;
}
