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
#ifndef __AWS_KVS_WEBRTC_SINGLE_LINKED_LIST_INCLUDE__
#define __AWS_KVS_WEBRTC_SINGLE_LINKED_LIST_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif
/******************************************************************************
 * HEADERS
 ******************************************************************************/
#include "common_defs.h"

/******************************************************************************
 * DEFINITIONS
 ******************************************************************************/
typedef struct __SingleListNode {
    struct __SingleListNode* pNext;
    UINT64 data;
} SingleListNode, *PSingleListNode;

typedef struct {
    UINT32 count;
    PSingleListNode pHead;
    PSingleListNode pTail;
} SingleList, *PSingleList;

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/
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


#ifdef __cplusplus
}
#endif
#endif /* __AWS_KVS_WEBRTC_SINGLE_LINKED_LIST_INCLUDE__ */
