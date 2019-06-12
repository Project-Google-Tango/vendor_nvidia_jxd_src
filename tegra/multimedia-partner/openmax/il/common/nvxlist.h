/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVX_LIST_H
#define INCLUDED_NVX_LIST_H

#include "nvcommon.h"
#include "nverror.h"
#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Not a great API, but need to keep compatability with the nvmm queue
   to minimize changes.  These will always have < 10 elements, so any
   inefficiencies implied in the API don't really matter.
 */

typedef struct _NvxListItem
{
    void *pElement;
    struct _NvxListItem *pNext;
} NvxListItem;

typedef struct NvxListRec
{
    NvOsMutexHandle oLock;
    NvxListItem *pHead;
    NvxListItem *pEnd;
    NvU32 nCount;
} NvxList;

typedef struct NvxListRec *NvxListHandle;

NvError NvxListCreate(NvxListHandle *phList);
void NvxListDestroy(NvxListHandle *phList);
NvError NvxListEnQ(NvxListHandle hList, void *pElem, NvU32 Timeout);
NvError NvxListDeQ(NvxListHandle hList, void **pElem);
NvError NvxListPeek(NvxListHandle hList, void **pElem);
NvError NvxListPeekEntry(NvxListHandle hList, void **pElem, NvU32 nEntry);
NvError NvxListInsertHead(NvxListHandle hList, void *pElem, NvU32 uTimeout);
NvU32 NvxListGetNumEntries(NvxListHandle hList);
NvError NvxListRemoveEntry(NvxListHandle hList, void *pElem);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVX_LIST_H

