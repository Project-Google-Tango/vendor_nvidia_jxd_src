/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvrm_heap.h"
#include "nvrm_heap_simple.h"
#include "nvrm_hardware_access.h"


static NvRmPrivHeap   gs_IramHeap;
static NvUPtr         gs_IramBaseAddr;
static void          *gs_IramVaddr;
static NvRmHeapSimple gs_IramAllocator;

NvError NvRmPrivHeapIramAlloc(NvU32 size, NvU32 align, NvRmPhysAddr *PAddr)
{
    NvError err;
    err = NvRmPrivHeapSimpleAlloc(&gs_IramAllocator, size, align, PAddr);
    return err;
}

NvError NvRmPrivHeapIramPreAlloc(NvRmPhysAddr Address, NvU32 Length)
{
    return NvRmPrivHeapSimplePreAlloc(&gs_IramAllocator, Address, Length);
}

void NvRmPrivHeapIramFree(NvRmPhysAddr addr)
{
    NvRmPrivHeapSimpleFree(&gs_IramAllocator, addr);
}

NvRmPrivHeap *NvRmPrivHeapIramInit(NvU32 length, NvRmPhysAddr base)
{
    void   *vAddr = NULL;
    NvError err;

#if !NVOS_IS_LINUX
    /* try to map the memory, if we can't map it then bail out */
    err = NvRmPhysicalMemMap(base, length, NVOS_MEM_READ_WRITE, 
              NvOsMemAttribute_Uncached, &vAddr);
    if (err != NvSuccess)
        return NULL;
#endif

    err = NvRmPrivHeapSimple_HeapAlloc(base, length, &gs_IramAllocator, NV_FALSE);
    
    if (err != NvSuccess)
    {
        if (vAddr)
            NvOsPhysicalMemUnmap(vAddr, length);
        return NULL;
    }

    gs_IramHeap.heap            = NvRmHeap_IRam;
    gs_IramHeap.length          = length;
    gs_IramHeap.PhysicalAddress = base;
    gs_IramBaseAddr             = (NvUPtr)base;
    gs_IramVaddr                = vAddr;

    return &gs_IramHeap;
}

void NvRmPrivHeapIramDeinit(void)
{
    // deinit the carveout allocator
    if (gs_IramVaddr)
    {
        NvOsPhysicalMemUnmap(gs_IramVaddr, gs_IramHeap.length);
        gs_IramVaddr = NULL;
    }

    NvRmPrivHeapSimple_HeapFree(&gs_IramAllocator);
    NvOsMemset(&gs_IramHeap, 0, sizeof(gs_IramHeap));
    NvOsMemset(&gs_IramAllocator, 0, sizeof(gs_IramAllocator));
}

void *NvRmPrivHeapIramMemMap(
    NvRmPhysAddr base,
    NvU32 length,
    NvOsMemAttribute attribute)
{
    NvU32  StartOffset = base - gs_IramBaseAddr;
    NvU32  EndOffset   = StartOffset + length - 1;

    NV_ASSERT(length != 0);

    if (!gs_IramVaddr)
        return NULL;

    // sanity checking
    if (StartOffset < gs_IramHeap.length &&
        EndOffset   < gs_IramHeap.length)
    {
        NvUPtr uptr = (NvUPtr)gs_IramVaddr;
        return (void *)(uptr + StartOffset);
    }

    NV_ASSERT(!"Attempt to map something that is not part of the iram");
    return NULL;
}
