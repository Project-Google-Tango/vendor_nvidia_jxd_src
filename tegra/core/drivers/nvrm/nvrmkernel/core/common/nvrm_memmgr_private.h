/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#ifndef INCLUDED_NVDDK_MEMMGR_PRIVATE_H
#define INCLUDED_NVDDK_MEMMGR_PRIVATE_H

#include "nvrm_heap.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

#define NVRM_HMEM_CHECK_MAGIC       NV_DEBUG

#define NV_RM_HMEM_IS_ALLOCATED(hMem) \
        (((hMem)->PhysicalAddress != NV_RM_INVALID_PHYS_ADDRESS) || \
         ((hMem)->VirtualAddress  != NULL) || \
         ((hMem)->hPageHandle     != NULL) )

typedef struct NvRmMemRec 
{
    void                *VirtualAddress;
    NvRmDeviceHandle    hRmDevice;
    NvOsPageAllocHandle hPageHandle;
    NvRmPhysAddr        PhysicalAddress;
    NvU32               size;
    NvU32               alignment;

    /* Used for GART heap to keep track of the number of GART pages
     * in use by this handle.
     */
    NvU32               Pages; 

    NvS32               refcount;
    NvS32               pin_count;

    NvOsMemAttribute    coherency;
    NvRmHeap            heap;

    NvBool              mapped;
    NvU8                priority; 

#if NVRM_HMEM_CHECK_MAGIC
    NvU32               magic;  // set to NVRM_MEM_MAGIC if valid
#endif
} NvRmMem;

#ifdef __cplusplus
}
#endif  /* __cplusplus */


#endif




