/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvrm_memmgr.h"
#include "nvrm_memmgr_private.h"
#include "nvrm_heap_simple.h"
#include "ap15/ap15rm_private.h"
#include "nvos.h"
#include "nvrm_hardware_access.h"
#include "nvrm_rmctrace.h"
#include "nvrm_arm_cp.h"

#ifdef ANDROID
#include <cutils/log.h>
#endif

/*  FIXME: temporary hack to force all Linux allocations to be page-aligned */
#if NVOS_IS_LINUX
#define NVRM_ALLOC_MIN_ALIGN 4096
#else
#define NVRM_ALLOC_MIN_ALIGN 4
#endif

#define NVRM_CHECK_PIN          0

#define NVRM_MEM_MAGIC          0xdead9812
#define NVRM_HMEM_CHECK(hMem) \
        do { \
            if (NVRM_HMEM_CHECK_MAGIC) { \
                NV_ASSERT(((NvU32)(hMem)&1)==0); \
                if (((NvU32)(hMem)&1)) { \
                    (hMem) = idtomem(hMem); \
                } \
                NV_ASSERT((hMem)->magic == NVRM_MEM_MAGIC); \
            } \
        } while(0)

static NvRmMemHandle idtomem(NvRmMemHandle hMem)
{
    NvOsDebugPrintf("RMMEM id->mem %08x\n",(int)hMem);
    return (NvRmMemHandle)((NvU32)hMem&~1UL);
}

#if NVRM_MEM_TRACE
#undef NvRmMemAlloc
#undef NvRmMemAllocTagged
#undef NvRmMemAllocBlocklinear
#undef NvRmMemPin
#undef NvRmMemUnpin
#undef NvRmMemHandleAlloc
#undef NvRmMemHandleCreate
#undef NvRmMemHandleFree
#undef NvRmMemGetId
#undef NvRmMemGetFd
#undef NvRmMemHandleFromId
#undef NvRmMemHandleFromFd
#endif
void NvRmTracePrintf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    NvOsDebugVprintf(format, ap);
    va_end(ap);
}


/* GART related */
NvBool              gs_GartInited = NV_FALSE;
NvRmHeapSimple      gs_GartAllocator;
NvU32               *gs_GartSave = NULL;
static NvRmPrivHeap gs_GartHeap;
static NvUPtr       gs_GartBaseAddr;

static NvError      (*s_HeapGartAlloc)( NvRmDeviceHandle hDevice,
                        NvOsPageAllocHandle hPageHandle,
                        NvU32 NumberOfPages, NvRmPhysAddr *PAddr);
static void         (*s_HeapGartFree)( NvRmDeviceHandle hDevice,
                        NvRmPhysAddr addr, NvU32 NumberOfPages);
static void         (*s_GartSuspend)( NvRmDeviceHandle hDevice ) = NULL;
static void         (*s_GartResume)( NvRmDeviceHandle hDevice ) = NULL;
static NvError      (*s_HeapGartMap)( NvOsPhysAddr phys,
                        size_t size, NvOsMemAttribute attrib,
                        NvU32 flags, void **ptr);

/* Switch to force conversion from CarveOut to GART.
 * Default is to retain original CarveOut request (NV_TRUE).
 * For SMMU test purposes.
 */
#ifndef NVRM_MEMMGR_USECARVEOUT
#define NVRM_MEMMGR_USECARVEOUT NV_TRUE
#endif
#ifndef NVRM_MEMMGR_USEGART
#define NVRM_MEMMGR_USEGART NV_TRUE
#endif

/* Switch to force conversion from CarveOut to GART */
static NvBool s_useCarveOut = NVRM_MEMMGR_USECARVEOUT;
NvBool g_useGART = NVRM_MEMMGR_USEGART;

static NvRmHeap adjustHeap(NvRmHeap heap)
{
    if (!s_useCarveOut && !g_useGART)
    {
        NV_ASSERT(!"Either s_useCarveOut or g_useGART or both must be TRUE");
        return (NvRmHeap)0;    // Invalid heap type
    }
    if (!s_useCarveOut && (heap == NvRmHeap_ExternalCarveOut))
        heap = NvRmHeap_GART;
    if (!g_useGART && (heap == NvRmHeap_GART))
        heap = NvRmHeap_ExternalCarveOut;
    return heap;
}

static void NvRmPrivMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 length);

/*
 * Notes:
 *
 * 1) The allocation of the handles should fall back to a block allocator
 *    that allocates say 1024 at a time to reduce heap fragmentation.
 *
 */

NvError NvRmMemHandleCreate(
    NvRmDeviceHandle  hRmDevice,
    NvRmMemHandle    *phMem,
    NvU32             size)
{
    NvRmMemHandle  pNewHandle = NULL;
    NvError        err        = NvSuccess;

    pNewHandle = NvOsAlloc(sizeof(*pNewHandle));
    if (!pNewHandle)
    {
        err = NvError_InsufficientMemory;
        goto exit_gracefully;
    }

    NV_ASSERT(((NvU32)pNewHandle & 1) == 0);

    NvOsMemset(pNewHandle, 0, sizeof(*pNewHandle));
    pNewHandle->size      = size;
    pNewHandle->hRmDevice = hRmDevice;
    pNewHandle->PhysicalAddress =  NV_RM_INVALID_PHYS_ADDRESS;
    pNewHandle->VirtualAddress  =  NULL;
    pNewHandle->refcount = 1;
    pNewHandle->pin_count = 0;
    pNewHandle->coherency = NvOsMemAttribute_Uncached;
#if NVRM_HMEM_CHECK_MAGIC
    pNewHandle->magic = NVRM_MEM_MAGIC;
#endif

    *phMem                = pNewHandle;

exit_gracefully:
    if (err != NvSuccess)
        NvOsFree(pNewHandle);

    return err;
}

void NvRmPrivMemIncrRef(NvRmMemHandle hMem)
{
    NV_ASSERT(hMem);
    NvOsAtomicExchangeAdd32(&hMem->refcount, 1);
}

/* Attempt to use the pre-mapped carveout or iram aperture on Windows CE */
#if !NVOS_IS_LINUX
static void *NvRmMemMapGlobalHeap(
    NvRmPhysAddr base,
    NvU32 len,
    NvRmHeap heap,
    NvOsMemAttribute coherency)
{
    if (coherency == NvOsMemAttribute_WriteBack)
        return NULL;

    switch (adjustHeap(heap))
    {
    case NvRmHeap_ExternalCarveOut:
    case NvRmHeap_Camera:
        return NvRmPrivHeapCarveoutMemMap(base, len, coherency);
    case NvRmHeap_Secured:
        return NvRmPrivHeapSecuredMemMap(base, len, coherency);
    case NvRmHeap_IRam:
        return NvRmPrivHeapIramMemMap(base, len, coherency);
    default:
        break;
    }

    return NULL;
}
#else
#define NvRmMemMapGlobalHeap(base,len,heap,coherency) NULL
#endif

static void NvRmPrivMemFree(NvRmMemHandle hMem)
{
    if (!hMem)
        return;

    NVRM_HMEM_CHECK(hMem);

    if (!NV_RM_HMEM_IS_ALLOCATED(hMem))
        return;

    if (!NvRmMemMapGlobalHeap(hMem->PhysicalAddress, 4, hMem->heap,
        hMem->coherency) && hMem->VirtualAddress) {
        NvRmPrivMemUnmap(hMem, hMem->VirtualAddress, hMem->size);
        hMem->VirtualAddress = NULL;
    }

    switch (adjustHeap(hMem->heap))
    {
    case  NvRmHeap_ExternalCarveOut:
    case  NvRmHeap_Camera:
        NvRmPrivHeapCarveoutFree(hMem->PhysicalAddress);
        break;
    case NvRmHeap_IRam:
        NvRmPrivHeapIramFree(hMem->PhysicalAddress);
        break;
    case NvRmHeap_GART:
        (*s_HeapGartFree)(hMem->hRmDevice, hMem->PhysicalAddress, hMem->Pages);
        NvOsPageFree(hMem->hPageHandle);
        break;
    case NvRmHeap_External:
        NvOsPageFree(hMem->hPageHandle);
        break;
    default:
        break;
    }

    hMem->PhysicalAddress = NV_RM_INVALID_PHYS_ADDRESS;
    hMem->VirtualAddress = NULL;
    hMem->heap = 0;
    hMem->coherency = NvOsMemAttribute_Uncached;
#if NVRM_HMEM_CHECK_MAGIC
    hMem->magic = 0;
#endif
}

void NvRmMemHandleFree(NvRmMemHandle hMem)
{
    NvS32 old;
    NvOsMutexHandle mutex;

    if( !hMem )
    {
        return;
    }

    NVRM_HMEM_CHECK(hMem);
    old = NvOsAtomicExchangeAdd32(&hMem->refcount, -1);
    if(old > 1)
    {
        return;
    }

    NV_ASSERT(old != 0);

    mutex = hMem->hRmDevice->MemMgrMutex;
    NvOsMutexLock(mutex);

    NvRmPrivMemFree(hMem);
    NV_ASSERT(hMem->mapped == NV_FALSE);
    if (hMem->mapped == NV_TRUE)
    {
        NvRmPrivMemUnmap(hMem, hMem->VirtualAddress, hMem->size);
    }

#if NVRM_HMEM_CHECK_MAGIC
    hMem->magic = 0;
#endif

    NvOsFree(hMem);

    NvOsMutexUnlock( mutex );
}

NvError NvRmMemAllocTagged(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvU16 Tags)
{
    return NvRmMemAlloc(hMem, Heaps, NumHeaps, Alignment, Coherency);
}

NvError NvRmMemAlloc(
    NvRmMemHandle     hMem,
    const NvRmHeap    *Heaps,
    NvU32             NumHeaps,
    NvU32             Alignment,
    NvOsMemAttribute  Coherency)
{
    // Default heap list does not include GART due to AP15 hardware bug.  GART
    // will be re-added to default heap list on AP20 and beyond.
    NvRmHeap DefaultHeaps[3];
    NvU32     i;
    NvError err;


    NV_ASSERT(hMem && (!NumHeaps || Heaps));
    NVRM_HMEM_CHECK(hMem);

    /* FIXME: Windows should support full caching for memory handles.
     * But not yet.
     */
#ifndef NVOS_USE_WRITECOMBINE
    if( Coherency == NvOsMemAttribute_WriteCombined )
        Coherency = NvOsMemAttribute_Uncached;
#endif
    hMem->coherency = Coherency;

    if (NV_RM_HMEM_IS_ALLOCATED(hMem))
        return NvError_AlreadyAllocated;

    NvOsMutexLock(hMem->hRmDevice->MemMgrMutex);

    if (!NumHeaps)
    {
        if (hMem->size <= NVCPU_MIN_PAGE_SIZE || Coherency == NvOsMemAttribute_WriteCombined)
        {
            DefaultHeaps[NumHeaps++] = NvRmHeap_External;
            DefaultHeaps[NumHeaps++] = NvRmHeap_ExternalCarveOut;
        }
        else
        {
            DefaultHeaps[NumHeaps++] = NvRmHeap_ExternalCarveOut;
            DefaultHeaps[NumHeaps++] = NvRmHeap_External;
            if (s_HeapGartAlloc)
                DefaultHeaps[NumHeaps++] = NvRmHeap_GART;
        }
        Heaps = DefaultHeaps;
    }

    // 4 is the minimum alignment for any heap.
    if (Alignment < NVRM_ALLOC_MIN_ALIGN)
        Alignment = NVRM_ALLOC_MIN_ALIGN;
    if ((Alignment & (Alignment - 1)) && (Coherency == NvOsMemAttribute_WriteCombined))
    {
        NV_ASSERT(!"Invalid alignment request, should be a power of 2");
        return NV_FALSE;
    }

    err = NvError_InsufficientMemory;
    for (i = 0; i < NumHeaps; i++)
    {
        switch (adjustHeap(Heaps[i]))
        {
        case NvRmHeap_IRam:
            err = NvRmPrivHeapIramAlloc(hMem->size,
                Alignment, &hMem->PhysicalAddress);
            break;
        case NvRmHeap_External:
           /* NVCPU_MIN_PAGE_SIZE is subtracted from the total size because
            * it is taken care internally in NvOsPageAlloc() API */
           if (Alignment > NVCPU_MIN_PAGE_SIZE)
               hMem->size = hMem->size + (Alignment - 1) - NVCPU_MIN_PAGE_SIZE;
           err = NvOsPageAlloc(hMem->size, Coherency,
                NvOsPageFlags_Contiguous, NVOS_MEM_READ_WRITE,
                 &hMem->hPageHandle);
            break;
        case NvRmHeap_Camera:
            /* Camera special heap only implemented on native Linux
             * memory manager; other operating systems fall back to
             * standard carveout */
        case NvRmHeap_ExternalCarveOut:
            /* Write Combine attribute is not supported in Carveout memory
             * It is supported only in External memory */
            hMem->coherency = NvOsMemAttribute_Uncached;
            err = NvRmPrivHeapCarveoutAlloc(hMem->size,
                          Alignment, &hMem->PhysicalAddress);
            break;
        case NvRmHeap_Secured:
            hMem->coherency = NvOsMemAttribute_Uncached;
            err = NvRmPrivHeapSecuredAlloc(hMem->size,
                          Alignment, &hMem->PhysicalAddress);
                break;
        case NvRmHeap_GART:
            if (!s_HeapGartAlloc)
            {
                // GART not present on device
                if (NumHeaps == 1)
                    return NvError_NotSupported;
                break;
            }
            if (Alignment > NVCPU_MIN_PAGE_SIZE)
                hMem->size = hMem->size + (Alignment - 1) - NVCPU_MIN_PAGE_SIZE;
            err = NvOsPageAlloc(hMem->size, Coherency,
                NvOsPageFlags_NonContiguous, NVOS_MEM_READ_WRITE,
                &hMem->hPageHandle);

            if (err != NvSuccess)
                break;

            hMem->Pages = (hMem->size+(GART_PAGE_SIZE-1))/GART_PAGE_SIZE;

            err = (*s_HeapGartAlloc)(hMem->hRmDevice,
                hMem->hPageHandle, hMem->Pages, &hMem->PhysicalAddress);

            if (err == NvSuccess)
                break;

            hMem->Pages = 0;
            NvOsPageFree(hMem->hPageHandle);
            hMem->hPageHandle = NULL;
            break;

        default:
            NV_ASSERT(!"Invalid heap in heaps array");
        }

        if (err == NvSuccess)
            break;
    }

    NvOsMutexUnlock(hMem->hRmDevice->MemMgrMutex);

    if (err == NvSuccess)
    {
        hMem->alignment = Alignment;
        hMem->heap = Heaps[i];
        hMem->coherency = Coherency;

        #if NVOS_IS_LINUX
        /* Don't cache virtual mappings for cacheable handles in the RM,
         * since there isn't a good way to ensure proper coherency */
        if (Coherency != NvOsMemAttribute_WriteBack)
        #else
        // AOS has a unique mapping between physical and virtual space.
        // It shouldn't have any cache coherency issues.
        #endif
        {
            NvRmMemMap(hMem, 0, hMem->size, NVOS_MEM_READ_WRITE,
                &hMem->VirtualAddress);
        }
    }

    return err;
}

NvError
NvRmMemHandleAlloc(
    NvRmDeviceHandle hRm,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvU32 Size,
    NvU16 Tags,
    NvBool ReclaimCache,
    NvRmMemHandle *hMem)
{
    NvError e;
    e = NvRmMemHandleCreate(hRm, hMem, Size);
    if (e != NvSuccess)
            goto exit;
    e = NvRmMemAllocTagged(*hMem, Heaps,
                NumHeaps, Alignment, Coherency, Tags);
    if (e != NvSuccess) {
        NvRmMemHandleFree(*hMem);
        *hMem = NULL;
    }
exit:
    return e;
}

NvError
NvRmMemHandleAllocAttr(
    NvRmDeviceHandle hRm,
    NvRmMemHandleAttr *attr,
    NvRmMemHandle *hMem)
{
    return NvRmMemHandleAlloc(hRm, attr->Heaps, attr->NumHeaps,
                    attr->Alignment, attr->Coherency, attr->Size,
                    attr->Tags, attr->ReclaimCache, hMem);
}

NvU32 NvRmMemPin(NvRmMemHandle hMem)
{
    NvU32 offset = 0;

    NV_ASSERT(hMem);
    NVRM_HMEM_CHECK(hMem);

    if (NvOsAtomicExchangeAdd32(&hMem->pin_count, 1) < 0)
        NV_ASSERT(!"NvRmMemPin: original pin_count was negative");

    // FIXME: finish implementation
    switch (adjustHeap(hMem->heap))
    {
        case NvRmHeap_External:
            if (hMem->alignment > NVCPU_MIN_PAGE_SIZE)
            {
                NvU32 PageAlignedAddr = 0;
                NvU32 FinalAlignedAddr = 0;
                FinalAlignedAddr =
                    (((NvU32)hMem->hPageHandle + (hMem->alignment - 1)) &
                     (~(hMem->alignment - 1)));
                PageAlignedAddr =
                    (((NvU32)hMem->hPageHandle + (NVCPU_MIN_PAGE_SIZE - 1)) &
                     (~(NVCPU_MIN_PAGE_SIZE - 1)));
                offset = FinalAlignedAddr - PageAlignedAddr;
            }
            return (NvU32)NvOsPageAddress(hMem->hPageHandle, offset);
        case NvRmHeap_ExternalCarveOut:
        case NvRmHeap_Camera:
        case NvRmHeap_GART:
        case NvRmHeap_IRam:
            return hMem->PhysicalAddress;
        default:
            NV_ASSERT(!"Unknown heap");
            return 0xFFFFFFFF;
    }
}

void NvRmMemPinMult(NvRmMemHandle *hMems, NvU32 *Addrs, NvU32 Count)
{
    NvU32 i;
    for( i = 0; i < Count; i++ )
    {
        Addrs[i] = NvRmMemPin( hMems[i] );
    }
}

void NvRmMemUnpin(NvRmMemHandle hMem)
{
    if (!hMem)
        return;
    NVRM_HMEM_CHECK(hMem);
    if (NvOsAtomicExchangeAdd32(&hMem->pin_count, -1) <= 0)
        NV_ASSERT(!"NvRmMemPin: original pin_count was not positive");
}

void NvRmMemUnpinMult(NvRmMemHandle *hMems, NvU32 Count)
{
    NvU32 i;
    for(i = 0; i < Count; i++)
    {
        NvRmMemUnpin(hMems[i]);
    }
}

NvU32 NvRmMemGetAddress(NvRmMemHandle hMem, NvU32 Offset)
{
    NV_ASSERT(hMem != NULL);
    NV_ASSERT(Offset < hMem->size);
    NVRM_HMEM_CHECK(hMem);

#if NVRM_CHECK_PIN
    NV_ASSERT( hMem->pin_count );
#endif

    switch (adjustHeap(hMem->heap))
    {
    case NvRmHeap_External:
        return (NvU32)NvOsPageAddress(hMem->hPageHandle, Offset);

    case NvRmHeap_ExternalCarveOut:
    case NvRmHeap_Camera:
    case NvRmHeap_GART:
    case NvRmHeap_IRam:
        return (hMem->PhysicalAddress + Offset);

    default:
        NV_ASSERT(!"Unknown heap");
        break;
    }

    return (NvU32)-1;
}




NvError NvRmMemMap(
    NvRmMemHandle  hMem,
    NvU32          Offset,
    NvU32          Size,
    NvU32          Flags,
    void          **pVirtAddr)
{
    NV_ASSERT(Offset + Size <= hMem->size);
    NVRM_HMEM_CHECK(hMem);

    if (!hMem->VirtualAddress)
        hMem->VirtualAddress = NvRmMemMapGlobalHeap(
            hMem->PhysicalAddress+Offset, Size, hMem->heap, hMem->coherency);

    if (hMem->VirtualAddress)
    {
        *pVirtAddr = (NvU8 *)hMem->VirtualAddress + Offset;
        return NvSuccess;
    }

    *pVirtAddr = NULL;
    switch (adjustHeap(hMem->heap))
    {
#if !NVOS_IS_LINUX
    case NvRmHeap_GART:
        return s_HeapGartMap(hMem->PhysicalAddress + Offset,
                   Size, hMem->coherency, Flags, pVirtAddr);
#endif
    case NvRmHeap_ExternalCarveOut:
    case NvRmHeap_Camera:
    case NvRmHeap_IRam:
        return NvOsPhysicalMemMap(hMem->PhysicalAddress + Offset,
                   Size, hMem->coherency, Flags, pVirtAddr);
    case NvRmHeap_External:
        {
            NvU32 LocalOffset = 0;
            if (hMem->alignment > NVCPU_MIN_PAGE_SIZE)
            {
                NvU32 PageAlignedAddr = 0;
                NvU32 FinalAlignedAddr = 0;
                FinalAlignedAddr =
                    (((NvU32)hMem->hPageHandle + (hMem->alignment - 1)) &
                     (~(hMem->alignment - 1)));
                PageAlignedAddr =
                    (((NvU32)hMem->hPageHandle + (NVCPU_MIN_PAGE_SIZE - 1)) &
                     (~(NVCPU_MIN_PAGE_SIZE - 1)));
                LocalOffset = FinalAlignedAddr - PageAlignedAddr;
                Offset += LocalOffset;
            }
        }
        return NvOsPageMap(hMem->hPageHandle, Offset, Size, pVirtAddr);
    default:
        break;
    }
    return NvError_NotSupported;
}


static void NvRmPrivMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 length)
{
    if (!hMem || !pVirtAddr || !length) {
        return;
    }

    NVRM_HMEM_CHECK(hMem);

    /* Don't unmap from the global heap on CE */
    if (hMem->VirtualAddress &&
        NvRmMemMapGlobalHeap(hMem->PhysicalAddress, 4, hMem->heap,
                             hMem->coherency))
    {
        return;
    }

    /* Only unmap entire allocations; leaked mappings will be cleaned up
     * when the handle is freed
     */
    if (pVirtAddr != hMem->VirtualAddress || length != hMem->size)
        return;

    hMem->VirtualAddress = NULL;

    switch (adjustHeap(hMem->heap))
    {
    case NvRmHeap_External:
        NvOsPageUnmap(hMem->hPageHandle, pVirtAddr, length);
        break;
    case NvRmHeap_ExternalCarveOut:
    case NvRmHeap_Camera:
    case NvRmHeap_IRam:
        NvOsPhysicalMemUnmap(pVirtAddr, length);
        break;
    default:
        break;
    }
}

void NvRmMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 length)
{
#if NVOS_IS_LINUX
    NvRmPrivMemUnmap(hMem, pVirtAddr, length);
#else
    // For AOS, defer it till NvRmMemFree call.
#endif
}

NvU8 NvRmMemRd08(NvRmMemHandle hMem, NvU32 Offset)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return 0;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 1 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    return NV_READ8(vaddr);
}

NvU16 NvRmMemRd16(NvRmMemHandle hMem, NvU32 Offset)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return 0;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 2 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    return NV_READ16(vaddr);
}

NvU32 NvRmMemRd32(NvRmMemHandle hMem, NvU32 Offset)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return 0;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 4 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    return NV_READ32(vaddr);
}

void NvRmMemWr08(NvRmMemHandle hMem, NvU32 Offset, NvU8 Data)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 1 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    NV_WRITE08(vaddr, Data);
}

void NvRmMemWr16(NvRmMemHandle hMem, NvU32 Offset, NvU16 Data)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 2 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    NV_WRITE16(vaddr, Data);
}

void NvRmMemWr32(NvRmMemHandle hMem, NvU32 Offset, NvU32 Data)
{
    void *vaddr;

    NV_ASSERT(hMem->VirtualAddress != NULL);
    if (!hMem->VirtualAddress)
        return;

    vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + 4 <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    NV_WRITE32(vaddr, Data);
}

void NvRmMemRead(NvRmMemHandle hMem, NvU32 Offset, void *pDst, NvU32 Size)
{
    void *vaddr = (NvU8 *)hMem->VirtualAddress + Offset;
    NV_ASSERT(Offset + Size <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    NV_READ(pDst, vaddr, Size);
}

void NvRmMemWrite(
    NvRmMemHandle hMem,
    NvU32 Offset,
    const void *pSrc,
    NvU32 Size)
{
    void *vaddr = (NvU8 *)hMem->VirtualAddress + Offset;

    NV_ASSERT(Offset + Size <= hMem->size);
    NVRM_HMEM_CHECK(hMem);
    NV_WRITE(vaddr, pSrc, Size);
}

void NvRmMemReadStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 SrcStride,
    void *pDst,
    NvU32 DstStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    if ((ElementSize == SrcStride) && (ElementSize == DstStride))
    {
        NvRmMemRead(hMem, Offset, pDst, ElementSize * Count);
    }
    else
    {
        while (Count--)
        {
            NvRmMemRead(hMem, Offset, pDst, ElementSize);
            Offset += SrcStride;
            pDst = (NvU8 *)pDst + DstStride;
        }
    }
}

void NvRmMemWriteStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 DstStride,
    const void *pSrc,
    NvU32 SrcStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    if ((ElementSize == SrcStride) && (ElementSize == DstStride))
    {
        NvRmMemWrite(hMem, Offset, pSrc, ElementSize * Count);
    }
    else
    {
        while (Count--)
        {
            NvRmMemWrite(hMem, Offset, pSrc, ElementSize);
            Offset += DstStride;
            pSrc = (const NvU8 *)pSrc + SrcStride;
        }
    }
}

void NvRmMemMove(
        NvRmMemHandle dstHMem,
        NvU32 dstOffset,
        NvRmMemHandle srcHMem,
        NvU32 srcOffset,
        NvU32 Size)
{
    NvU32 i;

    NV_ASSERT(dstOffset + Size <= dstHMem->size);
    NV_ASSERT(srcOffset + Size <= srcHMem->size);
    NVRM_HMEM_CHECK(dstHMem);
    NVRM_HMEM_CHECK(srcHMem);

    if (((dstHMem->PhysicalAddress |
          srcHMem->PhysicalAddress |
          dstOffset                       |
          srcOffset                       |
          Size) & 3) == 0)
    {
        // everything is nicely word aligned
        if (dstHMem == srcHMem && srcOffset < dstOffset)
        {
            for (i=Size; i; )
            {
                NvU32 data;
                i -= 4;
                data = NvRmMemRd32(srcHMem, srcOffset+i);
                NvRmMemWr32(dstHMem, dstOffset+i, data);
            }
        }
        else
        {
            for (i=0; i < Size; i+=4)
            {
                NvU32 data = NvRmMemRd32(srcHMem, srcOffset+i);
                NvRmMemWr32(dstHMem, dstOffset+i, data);
            }
        }
    }
    else
    {
        // fall back to writing one byte at a time
        if (dstHMem == srcHMem && srcOffset < dstOffset)
        {
            for (i=Size; i--;)
            {
                NvU8 data = NvRmMemRd08(srcHMem, srcOffset+i);
                NvRmMemWr08(dstHMem, dstOffset+i, data);
            }
        }
        else
        {
            for (i=0; i < Size; ++i)
            {
                NvU8 data = NvRmMemRd08(srcHMem, srcOffset+i);
                NvRmMemWr08(dstHMem, dstOffset+i, data);
            }
        }
    }
}

void NvRmMemCacheMaint(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size,
    NvBool        Writeback,
    NvBool        Invalidate)
{
    if (!hMem || !pMapping || !Size || !(Writeback || Invalidate))
        return;

    NVRM_HMEM_CHECK(hMem);
    NV_ASSERT((NvU8*)pMapping+Size <= (NvU8*)hMem->VirtualAddress+Size);
    if (Writeback && Invalidate)
        NvOsDataCacheWritebackInvalidateRange(pMapping, Size);
    else if (Writeback)
        NvOsDataCacheWritebackRange(pMapping, Size);
    else {
        NvOsDebugPrintf("Invalidate-only cache maint not supported in NvOs\n");
    }
}

void NvRmMemCacheSyncForCpu(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size)
{
    NvRmMemCacheMaint(hMem, pMapping, Size, NV_FALSE, NV_TRUE);
}


void NvRmMemCacheSyncForDevice(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size)
{
    NvRmMemCacheMaint(hMem, pMapping, Size, NV_TRUE, NV_TRUE);
}

NvU32 NvRmMemGetSize(NvRmMemHandle hMem)
{
    NV_ASSERT(hMem);
    NVRM_HMEM_CHECK(hMem);
    return hMem->size;
}

NvU32 NvRmMemGetAlignment(NvRmMemHandle hMem)
{
    NV_ASSERT(hMem);
    NVRM_HMEM_CHECK(hMem);
    return hMem->alignment;
}

NvU32 NvRmMemGetCacheLineSize(void)
{
    static NvS32 s_IsCortexA9 = -1;
    NvU32 tmp = 0;
    if (s_IsCortexA9 == -1)
    {
        MRC(p15, 0, tmp, c0, c0, 0);
        if ((tmp & 0x0000FFF0) != 0x0000C090)
        {
            // Not A9
            s_IsCortexA9 = 0;
        }
        else
        {
            s_IsCortexA9 = 1;
        }
    }

    return ((s_IsCortexA9 == 1) ? 32 : 64);
}

NvRmHeap NvRmMemGetHeapType(NvRmMemHandle hMem)
{
    NV_ASSERT(hMem);
    NVRM_HMEM_CHECK(hMem);
    return hMem->heap;
}


void *NvRmHostAlloc(size_t size)
{
    return NvOsAlloc(size);
}

void NvRmHostFree(void *ptr)
{
    NvOsFree(ptr);
}


NvError NvRmMemMapIntoCallerPtr(
    NvRmMemHandle hMem,
    void  *pCallerPtr,
    NvU32 Offset,
    NvU32 Size)
{
    NvError err;
    NVRM_HMEM_CHECK(hMem);

    // The caller should be asking for an even number of pages.  not strictly
    // required, but the caller has already had to do the work to calculate the
    // required number of pages so they might as well pass in a nice round
    // number, which makes it easier to find bugs.
    NV_ASSERT( (Size & (NVCPU_MIN_PAGE_SIZE-1)) == 0);

    // Make sure the supplied virtual address is page aligned.
    NV_ASSERT( (((NvUPtr)pCallerPtr) & (NVCPU_MIN_PAGE_SIZE-1)) == 0);

    switch (adjustHeap(hMem->heap))
    {
    case NvRmHeap_External:
    case NvRmHeap_GART:
        err = NvOsPageMapIntoPtr(hMem->hPageHandle,
                                 pCallerPtr,
                                 Offset,
                                 Size);
        break;
    case NvRmHeap_ExternalCarveOut:
    case NvRmHeap_Camera:
    case NvRmHeap_IRam:
        // The caller is responsible for sending a size that
        // is the correct number of pages, including this pageoffset
        // at the beginning of the first page.
        err = NvOsPhysicalMemMapIntoCaller(
                            pCallerPtr,
                            (hMem->PhysicalAddress + Offset) &
                                ~(NVCPU_MIN_PAGE_SIZE-1),
                            Size,
                            NvOsMemAttribute_Uncached,
                            NVOS_MEM_WRITE | NVOS_MEM_READ);
        break;
    default:
        return NvError_NotImplemented;
    }

    return err;
}


NvU32 NvRmMemGetId(NvRmMemHandle hMem)
{
    NvU32 id = (NvU32)hMem;

    // !!! FIXME: Need to really create a unique id to handle the case where
    // hMem is freed, and then the next allocated hMem returns the same pointer
    // value.

    NVRM_HMEM_CHECK(hMem);
    NV_ASSERT(((NvU32)hMem & 1) == 0);
    if (!hMem || ((NvU32)hMem & 1))
        return 0;

#if NVRM_MEM_CHECK_ID
    id |= 1;
#endif

    return id;
}

NvError NvRmMemHandleFromId(NvU32 id, NvRmMemHandle *phMem)
{
    NvRmMemHandle hMem;
    // !!! FIXME: (see comment in GetId).  Specifically handle the case where
    // the memory handle has already been freed.

#if NVRM_MEM_CHECK_ID
    *phMem = NULL;
    NV_ASSERT(id & 1);
    if (!(id & 1))
        return NvError_BadParameter;
#endif

    hMem = (NvRmMemHandle)(id & ~1UL);

    NVRM_HMEM_CHECK(hMem);

    NvRmPrivMemIncrRef(hMem);

    *phMem = hMem;
    return NvSuccess;
}

int NvRmMemGetFd(NvRmMemHandle hMem)
{
    return -1;
}

NvError NvRmMemHandleFromFd(int fd, NvRmMemHandle *hMem)
{
    return NvError_NotSupported;
}

NvRmPrivHeap *NvRmPrivHeapGartInit(NvRmDeviceHandle hRmDevice)
{
    NvError         err;
    NvU32           length = hRmDevice->GartMemoryInfo.size;
    NvRmPhysAddr    base = hRmDevice->GartMemoryInfo.base;
    static NvRmModuleCapability caps[] =
        {
            {1, 0, 0, NvRmModulePlatform_Silicon, &caps[0]}, // AP15, AP16
            {1, 1, 0, NvRmModulePlatform_Silicon, &caps[1]}, // AP20/T20
            {2, 0, 0, NvRmModulePlatform_Silicon, &caps[2]}, // T30
            {3, 0, 0, NvRmModulePlatform_Silicon, &caps[3]}, // T12X
        };
    NvRmModuleCapability *pCap = NULL;

    // ZERO base address is a valid address. ~0 indicates an invalid address.
    if (!length || !~base)
        return NULL;

    NV_ASSERT_SUCCESS(NvRmModuleGetCapabilities(
        hRmDevice,
        NvRmPrivModuleID_MemoryController,
        caps,
        NV_ARRAY_SIZE(caps),
        (void**)&pCap));

    err = NvRmPrivHeapSimple_HeapAlloc(
        base,
        length,
        &gs_GartAllocator,
        NV_FALSE);

    if (err != NvSuccess)
        return NULL;

    gs_GartHeap.heap            = NvRmHeap_GART;
    gs_GartHeap.length          = length;
    gs_GartHeap.PhysicalAddress = base;

    gs_GartBaseAddr = (NvUPtr)base;
    (void)gs_GartBaseAddr;

    if ((pCap->MajorVersion == 2) && (pCap->MinorVersion == 0))
    {
        s_HeapGartAlloc = NvRmPrivT30HeapSMMUAlloc;
        s_HeapGartFree = NvRmPrivT30HeapSMMUFree;
        s_GartSuspend = NvRmPrivT30SMMUSuspend;
        s_GartResume = NvRmPrivT30SMMUResume;
        s_HeapGartMap = NvRmPrivT30SMMUMap;
    }
    else if ((pCap->MajorVersion == 3) && (pCap->MinorVersion == 0))
    {
        //FIXME: T12X uses t30 implementation for now.
        //Need update for T12X smmu
        s_HeapGartAlloc = NvRmPrivT30HeapSMMUAlloc;
        s_HeapGartFree = NvRmPrivT30HeapSMMUFree;
        s_GartSuspend = NvRmPrivT30SMMUSuspend;
        s_GartResume = NvRmPrivT30SMMUResume;
        s_HeapGartMap = NvRmPrivT30SMMUMap;
    }
    else
        NV_ASSERT(!"Unsupported Cap");

    return &gs_GartHeap;
}

void NvRmPrivHeapGartDeinit(void)
{
    // deinit the gart allocator

    NvRmPrivHeapSimple_HeapFree(&gs_GartAllocator);
    NvOsMemset(&gs_GartHeap, 0, sizeof(gs_GartHeap));
    NvOsMemset(&gs_GartAllocator, 0, sizeof(gs_GartAllocator));
    NvOsFree( gs_GartSave );
    gs_GartInited = NV_FALSE;
}

void NvRmPrivGartSuspend(NvRmDeviceHandle hDevice)
{
    NV_ASSERT(s_GartSuspend);
    (*s_GartSuspend)( hDevice );
}

void NvRmPrivGartResume(NvRmDeviceHandle hDevice)
{
    NV_ASSERT(s_GartResume);
    (*s_GartResume)( hDevice );
}

NvError NvRmMemGetStat(NvRmMemStat Stat, NvS32* Result)
{
    /* Main point of this function is to be compatible backwards and forwards,
     * i.e., breaking analysis apps is the thing to avoid.
     * Minimum hassle - maximum impact.
     * Performance is not that big of a deal.
     * Could be extended to use NvS64 as return value. However, NvS64 is
     * slightly more challenging in terms of printing etc. at the client side.
     * This function should return counts as raw data as possible; conversions
     * to percentages or anything like that should be left to the client.
     */
    if (Stat == NvRmMemStat_TotalCarveout)
    {
        *Result = NvRmPrivHeapCarveoutTotalSize();
    }
    else if (Stat == NvRmMemStat_UsedCarveout)
    {
        *Result = NvRmPrivHeapCarveoutMemoryUsed();
    }
    else if (Stat == NvRmMemStat_LargestFreeCarveoutBlock)
    {
        *Result = NvRmPrivHeapCarveoutLargestFreeBlock();
    }
    else if (Stat == NvRmMemStat_TotalGart)
    {
        *Result = gs_GartHeap.length;
    }
    else if (Stat == NvRmMemStat_UsedGart)
    {
        *Result = NvRmPrivHeapSimpleMemoryUsed(&gs_GartAllocator);
    }
    else if (Stat == NvRmMemStat_LargestFreeGartBlock)
    {
        *Result = NvRmPrivHeapSimpleLargestFreeBlock(&gs_GartAllocator);
    }
    else
    {
        return NvError_BadParameter;
    }
    return NvSuccess;
}

// Blocklinear is only supported on Linux
NvError NvRmMemAllocBlocklinear(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvRmMemKind Kind,
    NvRmMemCompressionTags CompressionTags)
{
    if (Kind != NvRmMemKind_Pitch)
        return NvError_NotSupported;
    if (CompressionTags != NvRmMemCompressionTags_None)
        return NvError_NotSupported;
    return NvRmMemAlloc(hMem, Heaps, NumHeaps, Alignment, Coherency);
}

NvError NvRmMemGetKind(NvRmMemHandle hMem, NvRmMemKind *Result)
{
    *Result = NvRmMemKind_Pitch;
    return NvSuccess;
}

NvError NvRmMemGetCompressionTags(NvRmMemHandle hMem, NvRmMemCompressionTags *Result)
{
    *Result = NvRmMemCompressionTags_None;
    return NvSuccess;
}

NvError NvRmMemDmaBufFdFromHandle(NvRmMemHandle hMem, int *fd)
{
    return NvError_NotSupported;
}
