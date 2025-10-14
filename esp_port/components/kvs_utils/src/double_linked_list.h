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
#ifndef __AWS_KVS_WEBRTC_DOUBLE_LINKED_LIST_INCLUDE__
#define __AWS_KVS_WEBRTC_DOUBLE_LINKED_LIST_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

#include "common_defs.h"

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

/**
 * Internal Double Linked List operations
 */
STATUS doubleListAllocNode(UINT64, PDoubleListNode*);
STATUS doubleListInsertNodeHeadInternal(PDoubleList, PDoubleListNode);
STATUS doubleListInsertNodeTailInternal(PDoubleList, PDoubleListNode);
STATUS doubleListInsertNodeBeforeInternal(PDoubleList, PDoubleListNode, PDoubleListNode);
STATUS doubleListInsertNodeAfterInternal(PDoubleList, PDoubleListNode, PDoubleListNode);
STATUS doubleListRemoveNodeInternal(PDoubleList, PDoubleListNode);
STATUS doubleListGetNodeAtInternal(PDoubleList, UINT32, PDoubleListNode*);

//////////////////////////////////////////////////////////////////////////////////////////////////////
// Double-linked list functionality
//////////////////////////////////////////////////////////////////////////////////////////////////////
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

#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_DOUBLE_LINKED_LIST_INCLUDE__ */
