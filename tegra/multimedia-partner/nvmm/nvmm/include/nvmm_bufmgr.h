/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef NVMM_BUFMGR_H
#define NVMM_BUFMGR_H

#include "nvrm_init.h"
#include "nvos.h"
#include "nvassert.h"

typedef struct NvMMBufMgrBlockRec
{
    NvBool       IsFree;
    NvRmPhysAddr PhysAddr;
    NvU32        virtualAddr;       
    NvU32        size;

    NvU32        NextIndex;

    // debug info
    NvU32        touched;
}NvMMBufMgrBlock;


typedef struct NvMMBufMgrHeapRec *NvMMBufMgrHandle;

typedef struct NvMMBufMgrHeapRec
{
    NvRmPhysAddr    base;
    NvU32           vbase; 
    NvU32           size;
    NvU32           ArraySize;

    NvMMBufMgrBlock *RawBlockArray;

    NvU32           BlockIndex;
    NvU32           SpareBlockIndex;

    //NvOsMutexHandle       mutex;
} NvMMBufMgrHeap;


// Init APIs
NvError NvMMBufMgrInit(NvMMBufMgrHandle *hNvMMBufMgr, NvRmPhysAddr Base, NvU32 vbase, NvU32 Size);
void NvMMBufMgrDeinit(NvMMBufMgrHandle hNvMMBufMgr);

// Function APIs
NvU32 NvMMBufMgrAlloc(NvMMBufMgrHandle hNvMMBufMgr, NvU32 size, NvU32 align);
void NvMMBufMgrFree(NvMMBufMgrHandle hNvMMBufMgr, NvRmPhysAddr paddr);

// Status APIs
NvU32 NvMMBufMgrGetTotalFreeMemoryAvailable(NvMMBufMgrHandle hNvMMBufMgr);
NvU32 NvMMBufMgrGetLargestFreeChunkAvailable(NvMMBufMgrHandle hNvMMBufMgr);


#endif // INCLUDED_HEAP_H
