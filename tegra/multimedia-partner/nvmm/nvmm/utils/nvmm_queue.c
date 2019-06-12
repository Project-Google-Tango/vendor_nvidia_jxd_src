/*
 * Copyright (c) 2006 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/* =========================================================================
 *                       I N C L U D E S
 * ========================================================================= */

#include "nvos.h"
#include "nvmm_queue.h"
#include "nvassert.h"

/* =========================================================================
 *                        D E F I N E S
 * ========================================================================= */

typedef struct NvMMQueueRec
{
    NvOsMutexHandle oMutexLock;         /* mutex lock for queue */
    NvBool bThreadSafe;                 /* thread safety required */
    NvU32 uMaxEntries;                  /* maximum number of allowed entries */
    NvU32 uEntrySize;                   /* size of individual entry */
    NvU32 uPushIndex;                   /* index of where to push entry */
    NvU32 uPopIndex;                    /* index of where to grab entry */
    NvU8 *pEntryList;                   /* pointer to beginning entry */
} NvMMQueue;

/* =========================================================================
 *                      P R O T O T Y P E S
 * ========================================================================= */


NvError NvMMQueueCreate(NvMMQueueHandle *phQueue, NvU32 uMaxEntries, NvU32 uEntrySize, NvBool bThreadSafe)
{
    NvMMQueue *pQueue;
    NvError eError = NvSuccess;

    NV_ASSERT(phQueue);
    NV_ASSERT(uMaxEntries > 0);
    NV_ASSERT(uEntrySize > 0);

    pQueue = (NvMMQueue *)NvOsAlloc(sizeof(NvMMQueue));
    if (!pQueue)
        return NvError_InsufficientMemory;

    NvOsMemset(pQueue, 0, sizeof(NvMMQueue));
    pQueue->bThreadSafe  = bThreadSafe;
    pQueue->pEntryList   = 0;
    pQueue->uPushIndex   = 0;
    pQueue->uPopIndex    = 0;
    pQueue->uMaxEntries  = uMaxEntries + 1;
    pQueue->uEntrySize   = uEntrySize;

    if (bThreadSafe)
    {
        eError = NvOsMutexCreate(&pQueue->oMutexLock);
        if (eError != NvSuccess)
            goto nvmm_queue_exit;
    }

    /* Allocate entry list */
    pQueue->pEntryList = NvOsAlloc(pQueue->uMaxEntries * uEntrySize);
    if (!pQueue->pEntryList)
    {
        eError = NvError_InsufficientMemory;
        goto nvmm_queue_exit;
    }

    *phQueue = pQueue;
    return NvSuccess;

nvmm_queue_exit:
    if (pQueue)
    {
        NvOsMutexDestroy(pQueue->oMutexLock);
        NvOsFree(pQueue);
    }

    *phQueue = 0;
    return eError;
}

NvError NvMMQueueEnQ(NvMMQueueHandle hQueue, void *pElem, NvU32 uTimeout)
{
    NvMMQueue *pQueue = hQueue;
    NvError eError = NvSuccess;
    NvU32 uPush, uPop;

    if (pQueue->bThreadSafe)
        NvOsMutexLock(pQueue->oMutexLock);

    uPush = pQueue->uPushIndex;
    uPop = pQueue->uPopIndex;

    /* Check if there's room */
    if (uPush + 1 == uPop || uPush + 1 == uPop + pQueue->uMaxEntries)
    {
        eError = NvError_InsufficientMemory;
        goto nvmm_queue_exit;
    }

    NvOsMemcpy(&pQueue->pEntryList[uPush * pQueue->uEntrySize], pElem,
               pQueue->uEntrySize);

    if (++uPush >= pQueue->uMaxEntries)
        uPush = 0;
    pQueue->uPushIndex = uPush;

nvmm_queue_exit:
    if (pQueue->bThreadSafe)
        NvOsMutexUnlock(pQueue->oMutexLock);
    return eError;
}

NvError NvMMQueueDeQ(NvMMQueueHandle hQueue, void *pElem)
{
    NvMMQueue *pQueue = hQueue;
    NvError eError = NvSuccess;
    NvU32 uPop;

    if (pQueue->bThreadSafe)
        NvOsMutexLock(pQueue->oMutexLock);

    uPop = pQueue->uPopIndex;
    if (pQueue->uPushIndex == uPop)
    {
        eError = NvError_InvalidSize;
        goto nvmm_queue_exit;
    }

    NvOsMemcpy(pElem, &pQueue->pEntryList[uPop * pQueue->uEntrySize],
               pQueue->uEntrySize);

    if (++uPop >= pQueue->uMaxEntries)
        uPop = 0;
    pQueue->uPopIndex = uPop;

nvmm_queue_exit:
    if (pQueue->bThreadSafe)
        NvOsMutexUnlock(pQueue->oMutexLock);
    return eError;
}

NvError NvMMQueuePeek(NvMMQueueHandle hQueue, void *pElem)
{
    NvMMQueue *pQueue = hQueue;
    NvError eError = NvSuccess;
    NvU32 uPop;

    if (pQueue->bThreadSafe)
        NvOsMutexLock(pQueue->oMutexLock);

    uPop = pQueue->uPopIndex;
    if (pQueue->uPushIndex == uPop)
    {
        eError = NvError_BadValue;
        goto nvmm_queue_exit;
    }

    NvOsMemcpy(pElem, &pQueue->pEntryList[uPop * pQueue->uEntrySize],
               pQueue->uEntrySize);

nvmm_queue_exit:
    if (pQueue->bThreadSafe)
        NvOsMutexUnlock(pQueue->oMutexLock);
    return eError;
}

NvError NvMMQueuePeekEntry(NvMMQueueHandle hQueue, void *pElem, NvU32 nEntry)
{
    NvMMQueue *pQueue = hQueue;
    NvError eError = NvSuccess;
    NvU32 entry, uPush, uPop, uNumEntries;

    if (pQueue->bThreadSafe)
        NvOsMutexLock(pQueue->oMutexLock);

    uPush = pQueue->uPushIndex;
    uPop = pQueue->uPopIndex;

    uNumEntries = (uPush >= uPop) ? uPush - uPop :
                                    pQueue->uMaxEntries - uPop + uPush;

    if ((uNumEntries == 0) || (uNumEntries <= nEntry))
    {
        eError = NvError_BadValue;
        goto nvmm_queue_exit;
    }

    entry = uPop + nEntry;
    if (entry >= pQueue->uMaxEntries)
        entry -= pQueue->uMaxEntries;

    NvOsMemcpy(pElem, &pQueue->pEntryList[entry * pQueue->uEntrySize],
               pQueue->uEntrySize);

nvmm_queue_exit:
    if (pQueue->bThreadSafe)
        NvOsMutexUnlock(pQueue->oMutexLock);
    return eError;
}

void NvMMQueueDestroy(NvMMQueueHandle *phQueue)
{
    NvMMQueue *pQueue = *phQueue;
    if (!pQueue)
        return;
    NvOsMutexDestroy(pQueue->oMutexLock);
    NvOsFree(pQueue->pEntryList);
    NvOsFree(pQueue);
    *phQueue = NULL;
}

NvU32 NvMMQueueGetNumEntries(NvMMQueueHandle hQueue)
{
    NvMMQueue *pQueue = hQueue;
    NvU32 uPush = pQueue->uPushIndex;
    NvU32 uPop = pQueue->uPopIndex;
    NvU32 uNumEntries = (uPush >= uPop) ? uPush - uPop :
                                          pQueue->uMaxEntries - uPop + uPush;

    return uNumEntries;
}

NvError NvMMQueueInsertHead(NvMMQueueHandle hQueue, void *pElem, NvU32 uTimeout)
{
    NvMMQueue *pQueue = hQueue;
    NvError eError = NvSuccess;
    NvU32 uPush, uPop;

    if (pQueue->bThreadSafe)
        NvOsMutexLock(pQueue->oMutexLock);

    uPush = pQueue->uPushIndex;
    uPop = pQueue->uPopIndex;

    /* Check if there's room */
    if (uPush + 1 == uPop || uPush + 1 == uPop + pQueue->uMaxEntries)
    {
        eError = NvError_InsufficientMemory;
        goto nvmm_queue_exit;
    }

    if (uPop-- == 0)
        uPop = pQueue->uMaxEntries - 1;

    NvOsMemcpy(&pQueue->pEntryList[uPop * pQueue->uEntrySize], pElem,
               pQueue->uEntrySize);

    pQueue->uPopIndex = uPop;

nvmm_queue_exit:
    if (pQueue->bThreadSafe)
        NvOsMutexUnlock(pQueue->oMutexLock);
    return eError;
}

