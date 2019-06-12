/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVRM_HEAP_SIMPLE_H
#define NVRM_HEAP_SIMPLE_H

#include "nvcommon.h"
#include "nvrm_init.h"
#include "nvos.h"


typedef struct NvRmHeapSimpleBlockRec NvRmHeapSimpleBlock;
struct NvRmHeapSimpleBlockRec
{
    NvBool       IsFree;
    NvRmPhysAddr PhysAddr;
    NvU32        size;

    NvU32        NextIndex;

    // debug info
    NvU32        touched;
};


typedef struct NvRmHeapSimpleRec
{
    NvRmPhysAddr          base;
    NvU32                 size;
    NvU32                 ArraySize;
    NvBool                IsTopDown;

    NvRmHeapSimpleBlock   *RawBlockArray;

    NvU32   BlockIndex;
    NvU32   SpareBlockIndex;

    NvOsMutexHandle       mutex;
} NvRmHeapSimple;

NvError NvRmPrivHeapSimple_HeapAlloc(NvRmPhysAddr Base, NvU32 Size, NvRmHeapSimple *pNewHeap, NvBool IsTopDown);
void NvRmPrivHeapSimple_HeapFree(NvRmHeapSimple *);

NvError NvRmPrivHeapSimpleAlloc(NvRmHeapSimple *, NvU32 size, NvU32 align, NvRmPhysAddr *paddr);

NvError NvRmPrivHeapSimplePreAlloc(
    NvRmHeapSimple *,
    NvRmPhysAddr Address,
    NvU32 Length);

void NvRmPrivHeapSimpleFree(NvRmHeapSimple *, NvRmPhysAddr paddr);

NvS32 NvRmPrivHeapSimpleMemoryUsed(NvRmHeapSimple* pHeap);

NvS32 NvRmPrivHeapSimpleLargestFreeBlock(NvRmHeapSimple* pHeap);

#endif // INCLUDED_HEAP_H
