/*
 * Copyright (c) 2008 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvflash_verifylist.h"
#include "nvutil.h"
#include "nvassert.h"


/*
 * Structure to store the partition Ids for
 * which data is to be verified.
 */
typedef struct PartitionsToVerifyRec
{
    NvU32 PartitionId;
    const char *PartitionName;
    struct PartitionsToVerify *pNext;
}PartitionsToVerify;

PartitionsToVerify *LstPartitions;

static PartitionsToVerify *pListIterator;

void
NvFlashVerifyListInitLstPartitions(void)
{
    LstPartitions = NULL;
}

void
NvFlashVerifyListDeInitLstPartitions(void)
{
    PartitionsToVerify *nextElem = NULL;

    if (LstPartitions)
    {
        nextElem = (PartitionsToVerify *)LstPartitions->pNext;
        while(nextElem)
        {
            nextElem = (PartitionsToVerify *)LstPartitions->pNext;
            NvOsFree(LstPartitions);
            LstPartitions = nextElem;
        }
        LstPartitions = NULL;
    }
}

NvError
NvFlashVerifyListAddPartition(const char* Partition)
{
    NvError e = NvError_Success;
    PartitionsToVerify *pListElem;
    NvU32 PartitionId;

    pListElem = LstPartitions;

    PartitionId = NvUStrtoul(Partition, 0, 0);
    if (pListElem)
    {
        // Traverse the list
        while(pListElem->pNext)
        {
            pListElem = (PartitionsToVerify *)pListElem->pNext;
        }
        pListElem->pNext = NvOsAlloc(sizeof(PartitionsToVerify));
        /// If there's no more room for storing the partition Ids
        /// return an error.
        if (!pListElem->pNext)
        {
                e = NvError_InsufficientMemory;
                goto fail;
        }
        if (!PartitionId)
        {
            ((PartitionsToVerify *)(pListElem->pNext))->PartitionName = Partition;
            ((PartitionsToVerify *)(pListElem->pNext))->PartitionId = 0;
        }
        else
        {
            ((PartitionsToVerify *)(pListElem->pNext))->PartitionId = PartitionId;
            ((PartitionsToVerify *)(pListElem->pNext))->PartitionName = NULL;
        }
        ((PartitionsToVerify *)(pListElem->pNext))->pNext = NULL;
    }
    else
    {
        //There's no node in the list
        LstPartitions = NvOsAlloc(sizeof(PartitionsToVerify));
        /// If there's no more room for storing the partition Ids
        /// return an error.
        if (!LstPartitions)
        {
                e = NvError_InsufficientMemory;
                goto fail;
        }
        if (!PartitionId)
        {
            ((PartitionsToVerify *)LstPartitions)->PartitionName = Partition;
            ((PartitionsToVerify *)LstPartitions)->PartitionId = 0;
        }
        else
        {
            ((PartitionsToVerify *)LstPartitions)->PartitionId = PartitionId;
            ((PartitionsToVerify *)LstPartitions)->PartitionName = NULL;
        }
        ((PartitionsToVerify *)LstPartitions)->pNext = NULL;
    }

fail:
    return e;
}

NvError
NvFlashVerifyListIsPartitionToVerify(NvU32 PartitionId, const char* PartitionName)
{
    NvError e = NvError_BadParameter;
    PartitionsToVerify *pLookupPart;

    pLookupPart = LstPartitions;
    // Lookup the list of partitions marked for verification
    // for the given partition Id.
    while (pLookupPart)
    {
        if ((pLookupPart->PartitionId == PartitionId) ||
        (pLookupPart->PartitionId == 0xFFFFFFFF) ||
        ((pLookupPart->PartitionName) &&
         (!NvOsStrcmp(pLookupPart->PartitionName, PartitionName))))
        {
            e = NvSuccess;
            break;
        }
        pLookupPart = (PartitionsToVerify *)pLookupPart->pNext;
    }

    return e;
}

NvError NvFlashVerifyListGetNextPartition(char *pPartition, NvBool SeekStart)
{
    NvError e = NvError_BadValue;
    // The iterator is to be reset to the beginning of the
    // list, as indicated.
    if (SeekStart)
    {
        pListIterator = LstPartitions;
    }
    if (pListIterator)
    {
        if (pListIterator->PartitionId)
            NvOsSnprintf(pPartition, sizeof(pPartition), "%d", pListIterator->PartitionId);
        else
            NvOsSnprintf(pPartition, sizeof(pPartition), "%s", pListIterator->PartitionName);

        pListIterator = (PartitionsToVerify *)pListIterator->pNext;
        e = NvSuccess;
    }
    return e;
}

