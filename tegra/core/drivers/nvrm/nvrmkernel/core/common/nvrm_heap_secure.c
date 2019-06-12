/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_memmgr.h"
#include "nvrm_heap.h"
#include "nvrm_heap_simple.h"
#include "nvrm_hardware_access.h"

static NvRmPrivHeap   gs_SecuredHeap;
static NvRmPhysAddr   gs_SecuredBaseAddr;
static void          *gs_SecuredVaddr;
static NvBool         gs_SecuredGloballyMapped;

static NvRmHeapSimple gs_SecuredAllocator;

NvError NvRmPrivHeapSecuredAlloc(
    NvU32 size,
    NvU32 align,
    NvRmPhysAddr *PAddr)
{
    return NvRmPrivHeapSimpleAlloc(&gs_SecuredAllocator, size, align, PAddr);
}

NvError NvRmPrivHeapSecuredPreAlloc(NvRmPhysAddr Address, NvU32 Length)
{
    return NvRmPrivHeapSimplePreAlloc(&gs_SecuredAllocator, Address, Length);
}

void NvRmPrivHeapSecuredFree(NvRmPhysAddr addr)
{
    NvRmPrivHeapSimpleFree(&gs_SecuredAllocator, addr);
}

NvS32 NvRmPrivHeapSecuredMemoryUsed(void)
{
    return NvRmPrivHeapSimpleMemoryUsed(&gs_SecuredAllocator);
}

NvS32 NvRmPrivHeapSecuredLargestFreeBlock(void)
{
    return NvRmPrivHeapSimpleLargestFreeBlock(&gs_SecuredAllocator);
}

NvS32 NvRmPrivHeapSecuredTotalSize(void)
{
    return gs_SecuredHeap.length;
}


void *NvRmPrivHeapSecuredMemMap(
    NvRmPhysAddr base,
    NvU32 length,
    NvOsMemAttribute attribute)
{
    NvU32  StartOffset = base - gs_SecuredBaseAddr;
    NvU32  EndOffset   = StartOffset + length - 1;

    if (!gs_SecuredVaddr)
        return NULL;

    NV_ASSERT(length != 0);

    // sanity checking
    if (StartOffset < gs_SecuredHeap.length &&
        EndOffset   < gs_SecuredHeap.length)
    {
        NvUPtr uptr = (NvUPtr)gs_SecuredVaddr;
        return (void *)(uptr + StartOffset);
    }

    NV_ASSERT(!"Attempt to map something that is not part of the Secured");
    return NULL;
}


NvRmPrivHeap *NvRmPrivHeapSecuredInit(NvU32 length, NvRmPhysAddr base)
{
    NvError err;
    NvBool  bGloballyMapped = NV_FALSE;
    void    *vAddr = NULL;

#if !NVOS_IS_LINUX
    /* try to map the memory, if we can't map it then bail out */
    err = NvRmPhysicalMemMap(base, length,
              NVOS_MEM_READ_WRITE | NVOS_MEM_GLOBAL_ADDR,
              NvOsMemAttribute_Uncached, &vAddr);

    if (err == NvSuccess)
    {
        bGloballyMapped = NV_TRUE;
    }
    else
    {
        // try again to map Secured, but with global flag gone
        err = NvRmPhysicalMemMap(base, length, NVOS_MEM_READ_WRITE,
                  NvOsMemAttribute_Uncached, &vAddr);

        if (err != NvSuccess)
            return NULL;
    }
#endif

    err = NvRmPrivHeapSimple_HeapAlloc(base, length, &gs_SecuredAllocator, NV_TRUE);

    if (err != NvSuccess)
    {
        if (vAddr)
            NvOsPhysicalMemUnmap(vAddr, length);
        return NULL;
    }

    gs_SecuredHeap.heap            = NvRmHeap_Secured;
    gs_SecuredHeap.length          = length;
    gs_SecuredHeap.PhysicalAddress = base;
    gs_SecuredBaseAddr             = base;
    gs_SecuredVaddr                = vAddr;
    gs_SecuredGloballyMapped       = bGloballyMapped;

    return &gs_SecuredHeap;
}


void NvRmPrivHeapSecuredDeinit(void)
{
    // deinit the Secured allocator
    if (gs_SecuredVaddr)
    {
        NvOsPhysicalMemUnmap(gs_SecuredVaddr, gs_SecuredHeap.length);
        gs_SecuredVaddr = NULL;
    }

    NvRmPrivHeapSimple_HeapFree(&gs_SecuredAllocator);
    NvOsMemset(&gs_SecuredHeap, 0, sizeof(gs_SecuredHeap));
    NvOsMemset(&gs_SecuredAllocator, 0, sizeof(gs_SecuredAllocator));
}


void NvRmPrivHeapSecuredGetInfo(
    NvU32 *SecuredPhysBase,
    void  **pSecured,
    NvU32 *SecuredSize)
{
    if (gs_SecuredGloballyMapped)
    {
        *SecuredPhysBase = gs_SecuredHeap.PhysicalAddress;
        *pSecured = gs_SecuredVaddr;
        *SecuredSize = gs_SecuredHeap.length;
    }
    else
    {
        *SecuredPhysBase = 0;
        *pSecured        = NULL;
        *SecuredSize     = 0;
    }
}

