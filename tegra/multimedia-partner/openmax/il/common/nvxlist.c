/*
 * Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvxlist.h"
#include "nvassert.h"

NvError NvxListCreate(NvxListHandle *phList)
{
    NvxList *pList;
    NvError err;

    pList = NvOsAlloc(sizeof(NvxList));
    if (!pList)
        return NvError_InsufficientMemory;

    err = NvOsMutexCreate(&(pList->oLock));
    if (err != NvSuccess)
    {
        NvOsFree(pList);
        return NvError_InsufficientMemory;
    }

    pList->pHead = NULL;
    pList->pEnd = NULL;
    pList->nCount = 0;

    *phList = pList;
    return NvSuccess;
}

void NvxListDestroy(NvxListHandle *phList)
{
    NvxList *pList = *phList;
    if (!pList)
        return;

    NvOsMutexDestroy(pList->oLock);
    NvOsFree(pList);
    *phList = NULL;
}

NvError NvxListEnQ(NvxListHandle hList, void *pElem, NvU32 Timeout)
{
    NvxList *pList = hList;
    NvxListItem *newItem = NvOsAlloc(sizeof(NvxListItem));

    if (!newItem)
        return NvError_InsufficientMemory;

    newItem->pElement = pElem;
    newItem->pNext = NULL;

    NvOsMutexLock(pList->oLock);
    if (!pList->pHead)
    {
        pList->pHead = pList->pEnd = newItem;
    }
    else
    {
        pList->pEnd->pNext = newItem;
        pList->pEnd = newItem;
    }

    pList->nCount++;

    NvOsMutexUnlock(pList->oLock);
    return NvSuccess;
}

NvError NvxListDeQ(NvxListHandle hList, void **pElem)
{
    NvxList *pList = hList;
    NvError ret = NvSuccess;

    NvOsMutexLock(pList->oLock);

    if (!pList->pHead)
        ret = NvError_InvalidSize;
    else
    {
        NvxListItem *cur = pList->pHead;

        *pElem = pList->pHead->pElement;
        pList->pHead = pList->pHead->pNext;
        if (!pList->pHead)
            pList->pEnd = NULL;
        NvOsFree(cur);
        pList->nCount--;
    }

    NvOsMutexUnlock(pList->oLock);
    return ret;
}

NvError NvxListPeek(NvxListHandle hList, void **pElem)
{
    NvxList *pList = hList;
    NvError ret = NvSuccess;

    NvOsMutexLock(pList->oLock);

    if (!pList->pHead)
        ret = NvError_InvalidSize;
    else
    {
        *pElem = pList->pHead->pElement;
    }

    NvOsMutexUnlock(pList->oLock);
    return ret;
}

NvError NvxListPeekEntry(NvxListHandle hList, void **pElem, NvU32 nEntry)
{
    NvxList *pList = hList;
    NvError ret = NvSuccess;

    NvOsMutexLock(pList->oLock);

    if (!pList->pHead)
        ret = NvError_InvalidSize;
    else
    {
        NvU32 i = 0;
        NvxListItem *cur = pList->pHead;

        for (i = 0; i < nEntry; i++)
        {
            cur = cur->pNext;
            if (!cur)
            {
                NvOsMutexUnlock(pList->oLock);
                return NvError_InvalidSize;
            }
        }

        *pElem = cur->pElement;
    }

    NvOsMutexUnlock(pList->oLock);
    return ret;
}

NvError NvxListInsertHead(NvxListHandle hList, void *pElem, NvU32 uTimeout)
{
    NvxList *pList = hList;
    NvxListItem *newItem = NvOsAlloc(sizeof(NvxListItem));

    if (!newItem)
        return NvError_InsufficientMemory;

    newItem->pElement = pElem;

    NvOsMutexLock(pList->oLock);

    newItem->pNext = pList->pHead;
    if (!newItem->pNext)
        pList->pEnd = newItem;
    pList->pHead = newItem;

    pList->nCount++;

    NvOsMutexUnlock(pList->oLock);
    return NvSuccess;

}

NvU32 NvxListGetNumEntries(NvxListHandle hList)
{
    NvxList *pList = hList;
    NvU32 count;

    NvOsMutexLock(pList->oLock);
    count = pList->nCount;
    NvOsMutexUnlock(pList->oLock);

    return count;
}

NvError NvxListRemoveEntry(NvxListHandle hList, void *pElem)
{
    NvxList *pList = hList;
    NvError ret = NvSuccess;

    NvOsMutexLock(pList->oLock);

    if (!pList->pHead)
        ret = NvError_InvalidSize;
    else
    {
        NvxListItem *cur = pList->pHead;
        NvxListItem *prev = NULL;

        while (cur)
        {
            if (cur->pElement == pElem)
                break;
            prev = cur;
            cur = cur->pNext;
        }

        if (cur)
        {
            if (!prev)
                pList->pHead = cur->pNext;
            if (pList->pEnd == cur)
                pList->pEnd = prev;

            if (prev)
                prev->pNext = cur->pNext;

            NvOsFree(cur);
            pList->nCount--;
        }
    }

    NvOsMutexUnlock(pList->oLock);
    return ret;
}

