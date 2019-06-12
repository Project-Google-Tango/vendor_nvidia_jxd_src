/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


#ifndef NVRM_HEAP_H
#define NVRM_HEAP_H

#include "nvrm_memmgr.h"
#include "nvassert.h"
#include "nvos.h"


#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */


typedef NvRmPhysAddr (*NvRmHeapAlloc)(NvU32 size);
typedef void (*NvRmHeapFree)(NvRmPhysAddr);

typedef struct NvRmPrivHeapRec
{
    NvRmHeap   heap;
    NvRmPhysAddr PhysicalAddress;
    NvU32      length;

} NvRmPrivHeap;

NvRmPrivHeap *
NvRmPrivHeapCarveoutInit(NvU32      length, 
                         NvRmPhysAddr base);

void
NvRmPrivHeapCarveoutDeinit(void);

NvError
NvRmPrivHeapCarveoutPreAlloc(NvRmPhysAddr Address, NvU32 Length);

NvError
NvRmPrivHeapCarveoutAlloc(NvU32 size, NvU32 align, NvRmPhysAddr *PAddr);

void 
NvRmPrivHeapCarveoutFree(NvRmPhysAddr addr);

void *
NvRmPrivHeapCarveoutMemMap(NvRmPhysAddr base, NvU32 length, NvOsMemAttribute attribute);

void
NvRmPrivHeapCarveoutGetInfo(NvU32 *CarveoutPhysBase, 
                            void  **pCarveout,
                            NvU32 *CarveoutSize);

NvS32 
NvRmPrivHeapCarveoutMemoryUsed(void);

NvS32 
NvRmPrivHeapCarveoutLargestFreeBlock(void);

/**
 * \Note    Not necessarily same as CarveoutSize returned by
 *          NvRmPrivHeapCarveoutGetInfo. No dependency on 
 *          carveout being mapped in.
 */
NvS32
NvRmPrivHeapCarveoutTotalSize(void);

NvRmPrivHeap *
NvRmPrivHeapSecuredInit(NvU32      length,
                         NvRmPhysAddr base);

void
NvRmPrivHeapSecuredDeinit(void);

NvError
NvRmPrivHeapSecuredPreAlloc(NvRmPhysAddr Address, NvU32 Length);

NvError
NvRmPrivHeapSecuredAlloc(NvU32 size, NvU32 align, NvRmPhysAddr *PAddr);

void
NvRmPrivHeapSecuredFree(NvRmPhysAddr addr);

void *
NvRmPrivHeapSecuredMemMap(NvRmPhysAddr base, NvU32 length, NvOsMemAttribute attribute);

void
NvRmPrivHeapSecuredGetInfo(NvU32 *SecuredPhysBase,
                            void  **pSecured,
                            NvU32 *SecuredSize);

NvS32
NvRmPrivHeapSecuredMemoryUsed(void);

NvS32
NvRmPrivHeapSecuredLargestFreeBlock(void);

NvS32
NvRmPrivHeapSecuredTotalSize(void);


NvRmPrivHeap *
NvRmPrivHeapIramInit(NvU32      length,
                         NvRmPhysAddr base);

void
NvRmPrivHeapIramDeinit(void);

NvError
NvRmPrivHeapIramAlloc(NvU32 size, NvU32 align, NvRmPhysAddr *PAddr);

NvError
NvRmPrivHeapIramPreAlloc(NvRmPhysAddr Address, NvU32 Length);

void 
NvRmPrivHeapIramFree(NvRmPhysAddr addr);

void *
NvRmPrivHeapIramMemMap(NvRmPhysAddr base, NvU32 length, NvOsMemAttribute attribute);


// -- GART --

#define GART_PAGE_SIZE (4096)
#define GART_MAX_PAGES (4096)

/** 
 * Initialize the GART heap.  This identifies the GART heap's base address
 * and total size to the internal heap manager, so that it may allocate 
 * pages appropriately.
 * 
 * @param hDevice An RM device handle.
 * Size of the GART heap (bytes) and Base address of the GART heap space
 * are in GartMemoryInfo substructure of hDevice
 * 
 * @retval Pointer to the heap data structure, with updated values.
 */
NvRmPrivHeap *
NvRmPrivHeapGartInit(NvRmDeviceHandle hDevice);

void
NvRmPrivHeapGartDeinit(void);

/** 
 * Allocate GART storage space of the specified size (in units of GART_PAGE_SIZE).
 * Alignment is handled internally by this API, since it must align with the 
 * GART_PAGE_SIZE.  This API also updates the GART registers and returns the base
 * address pointer of the space allocated within the GART heap.
 * 
 * @see NvRmPrivHeapGartFree()
 * 
 * @param hDevice An RM device handle.
 * @param pPhysAddrArray Contains an array of page addresses.  This array should
 *  be created using an NVOS call that acquires the underlying memory address
 *  for each page to be mapped by the GART.
 * @param NumberOfPages The size (in pages, not bytes) of mapping requested.  Must
 *  be greater than 0.
 * @param PAddr Points to variable that will be updated with the base address of
 *  the next available GART page.
 * 
 * @retval The address of the first available GART page of the requested size.
 */
NvError
NvRmPrivT30HeapSMMUAlloc(
    NvRmDeviceHandle hDevice,
    NvOsPageAllocHandle hPageHandle,
    NvU32 NumberOfPages,
    NvRmPhysAddr *PAddr);

/**
 * Free the specified GART memory pages.
 * 
 * @see NvRmPrivHeapGartAlloc()
 * 
 * @param hDevice An RM device handle.
 * @param addr Base address (GART space) of the memory page(s) to free.
 *  NULL address pointers are ignored.
 * @param NumberOfPages The size (in pages, not bytes) of mapping to free.
 *  This needs to match the size indicated when allocated.
 */
void
NvRmPrivT30HeapSMMUFree(
    NvRmDeviceHandle hDevice,
    NvRmPhysAddr addr,
    NvU32 NumberOfPages);

/**
 * Suspend GART.
 */
void
NvRmPrivT30SMMUSuspend(NvRmDeviceHandle hDevice);

/**
 * Resume GART.
 */
void
NvRmPrivT30SMMUResume(NvRmDeviceHandle hDevice);

/**
 * Physical to virtual memory mapper for T30 SMMU heap
 */
NvError
NvRmPrivT30SMMUMap(
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
