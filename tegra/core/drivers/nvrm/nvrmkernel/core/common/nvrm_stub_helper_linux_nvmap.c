/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_memmgr.h"
#include "nvrm_power.h"
#include "nvrm_rmctrace.h"
#include "nvrm_memmgr.h"
#include "linux/nvmap.h"
#include "nvrm_stub_helper_linux.h"
#include "linux/nvmem_ioctl.h"

#if defined(ANDROID) && !defined(PLATFORM_IS_GINGERBREAD) && NV_DEBUG
#include <memcheck.h>
#define NV_USE_VALGRIND 1
#else
#define NV_USE_VALGRIND 0
#endif

static int s_nvmapfd = -1;

static NvError ConvertError(int err)
{
    switch (err) {
        case 0:
            return NvSuccess;
        case EPERM:
            return NvError_AccessDenied;
        case ENOMEM:
            return NvError_InsufficientMemory;
        case EINVAL:
            return NvError_BadValue;
        case EEXIST:
            return NvError_AlreadyAllocated;
        case ENFILE:
            return NvError_FileOperationFailed;
        default:
            return NvError_IoctlFailed;
    }
}

NvError NvMapMemHandleCreate(
    NvRmDeviceHandle hRm,
    NvRmMemHandle *hMem,
    NvU32 Size)
{
    struct nvmap_create_handle op;

    NvOsMemset(&op, 0, sizeof(op));
    op.size = Size;
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_CREATE, &op))
        return ConvertError(errno);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

NvError NvMapMemHandleFromId(NvU32 Id, NvRmMemHandle *hMem)
{
    struct nvmap_create_handle op = {{0}};

    op.id = Id;
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_FROM_ID, &op))
        return ConvertError(errno);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

NvError NvMapMemHandleFromFd(NvS32 fd, NvRmMemHandle *hMem)
{
    struct nvmap_create_handle op = {{0}};

    op.fd = fd;
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_FROM_FD, &op))
        return ConvertError(errno);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

NvError NvMapMemAllocInternalTagged(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvRmMemKind Kind,
    NvRmMemCompressionTags CompressionTags,
    NvU16 Tags)
{
    NvU32 ioctlCmd = NVMAP_IOC_ALLOC;
    struct nvmap_alloc_handle op = {0};
    void *ioctlArg = &op;
    struct nvmem_alloc_kind_handle op_kind = {0};

    if (Alignment & (Alignment-1)) {
        printf("bad alloc %08x\n", op.heap_mask);
        return NvError_BadValue;
    }

    op.handle = (struct nvmap_handle *)hMem;
    op.align = Alignment;

    if (Coherency == NvOsMemAttribute_InnerWriteBack)
        op.flags = NVMAP_HANDLE_INNER_CACHEABLE;
    else if (Coherency == NvOsMemAttribute_WriteBack)
        op.flags = NVMAP_HANDLE_CACHEABLE;
    else if (Coherency == NvOsMemAttribute_WriteCombined)
        op.flags = NVMAP_HANDLE_WRITE_COMBINE;
    else
        op.flags = NVMAP_HANDLE_UNCACHEABLE;

    op.flags |= ((NvU32) Tags) << 16;

    if ( (Kind != NvRmMemKind_Invalid) && (Kind != NvRmMemKind_Pitch) )
    {
        ioctlCmd = NVMEM_IOC_ALLOC_KIND;
        ioctlArg = &op_kind;

        op_kind.handle = (NvU32)op.handle;
        op_kind.align  = op.align;
        op_kind.flags  = op.flags | NVMEM_HANDLE_KIND_SPECIFIED;
        op_kind.kind = (NvU8)Kind;

        if ( CompressionTags != NvRmMemCompressionTags_None )
        {
            op_kind.flags |= NVMEM_HANDLE_COMPR_SPECIFIED;
            op_kind.comp_tags = (NvU8)CompressionTags;
        }
    }

    if (!NumHeaps)
    {
        // GART is removed due to possible deadlocks when GART memory is pinned
        // outside NvRmChannel. Only drivers using NvRmChannel API to pin memory should use GART.
        op.heap_mask =
            NVMAP_HEAP_CARVEOUT_GENERIC;
#ifdef CONVERT_CARVEOUT_TO_GART
        if (getenv("CONVERT_CARVEOUT_TO_GART"))
            op.heap_mask = NVMAP_HEAP_IOVMM;
#endif
        op_kind.heap_mask = op.heap_mask;
        if (ioctl(s_nvmapfd, ioctlCmd, ioctlArg) == 0)
            return NvSuccess;
    }
    else
    {
        NvU32 i;
        for (i = 0; i < NumHeaps; i++)
        {
            switch (Heaps[i])
            {
            case NvRmHeap_ExternalCarveOut:
#ifdef CONVERT_CARVEOUT_TO_GART
                if (!getenv("CONVERT_CARVEOUT_TO_GART"))
                {
                    op.heap_mask = NVMAP_HEAP_CARVEOUT_GENERIC;
                    break;
                } // else falls through
#else
                op.heap_mask = NVMAP_HEAP_CARVEOUT_GENERIC;
                break;
#endif
            case NvRmHeap_GART:
                op.heap_mask = NVMAP_HEAP_IOVMM; break;
            case NvRmHeap_IRam:
                op.heap_mask = NVMAP_HEAP_CARVEOUT_IRAM; break;
            case NvRmHeap_VPR:
                op.heap_mask = NVMAP_HEAP_CARVEOUT_VPR; break;
            case NvRmHeap_External:
                // NvRmHeap_External heap cannot serve contiguous memory
                // region in a long run due to fragmentation of system
                // memory. Falling down to carveout instead.
                // Single page allocations will be served from
                // system memory automatically, since
                // they are always contiguous.
            case NvRmHeap_Camera:
                op.heap_mask = g_NvRmMemCameraHeapUsage; break;
            default:
                op.heap_mask = 0; break;
            }
            op_kind.heap_mask = op.heap_mask;
            if (ioctl(s_nvmapfd, ioctlCmd, ioctlArg) == 0)
                return NvSuccess;

            if (errno != ENOMEM)
                break;
            if (errno == EEXIST)
                break;
        }
    }

    return ConvertError(errno);
}

void NvMapMemHandleFree(NvRmMemHandle hMem)
{
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_FREE, (unsigned)hMem))
        NvOsDebugPrintf("NVMAP_IOC_FREE failed: %s\n", strerror(errno));
}

NvError
NvMapMemHandleAlloc(
    NvRmDeviceHandle hRm,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvU32 Size,
    NvU16 Tags,
    NvBool ReclaimCache,
    NvRmMemKind Kind,
    NvRmMemCompressionTags CompTags,
    NvRmMemHandle *hMem)
{
    NvError e;

    if (!hMem)
        return NvError_BadParameter;
    e = NvMapMemHandleCreate(hRm, hMem, Size);
    if (e != NvSuccess)
            goto exit;
    e = NvMapMemAllocInternalTagged(*hMem,
            Heaps,
            NumHeaps,
            Alignment,
            Coherency,
            Kind,
            CompTags,
            Tags);
    if (e != NvSuccess) {
        NvMapMemHandleFree(*hMem);
        *hMem = NULL;
    }
exit:
    return e;
}

NvU32 NvMapMemGetId(NvRmMemHandle hMem)
{
    struct nvmap_create_handle op = {{0}};
    op.handle = (struct nvmap_handle *)hMem;
    op.id = 0;

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_GET_ID, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_GET_ID failed: %s\n", strerror(errno));
        return 0;
    }
    return (NvU32)op.id;
}

NvS32 NvMapMemGetFd(NvRmMemHandle hMem)
{
    struct nvmap_create_handle op = {{0}};
    op.handle = (struct nvmap_handle *)hMem;
    op.fd = 0;

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_GET_FD, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_GET_FD failed: %s\n", strerror(errno));
        return -EMFILE;
    }
    return (NvU32)op.fd;
}

void NvMapMemUnpinMult(NvRmMemHandle *hMems, NvU32 Count)
{
    struct nvmap_pin_handle op = {0};

    op.count = Count;
    if (Count == 1)
        op.handles = (struct nvmap_handle **)hMems[0];
    else
        op.handles = (struct nvmap_handle **)hMems;

    if (ioctl(s_nvmapfd, NVMAP_IOC_UNPIN_MULT, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_UNPIN_MULT failed: %s\n", strerror(errno));
    }
}

void NvMapMemPinMult(NvRmMemHandle *hMems, NvU32 *addrs, NvU32 Count)
{
    struct nvmap_pin_handle op = {0};

    op.count = Count;
    if (Count == 1)
    {
        op.handles = (struct nvmap_handle **)hMems[0];
        op.addr = 0;
    }
    else
    {
        op.handles = (struct nvmap_handle **)hMems;
        op.addr = (unsigned long *)addrs;
    }

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_PIN_MULT, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_PIN_MULT failed: %s\n", strerror(errno));
    }
    else if (Count==1)
    {
        *addrs = (NvU32)op.addr;
    }
}

NvError NvMapMemMap(
    NvRmMemHandle hMem,
    NvU32         Offset,
    NvU32         Size,
    NvU32         Flags,
    void        **pVirtAddr)
{
    struct nvmap_map_caller op = {0};
    int prot;
    unsigned int page_size;
    size_t adjust_len;

    if (!pVirtAddr || !hMem)
        return NvError_BadParameter;

    if (s_nvmapfd < 0)
        return NvError_KernelDriverNotFound;

    prot = PROT_NONE;
    if (Flags & NVOS_MEM_READ) prot |= PROT_READ;
    if (Flags & NVOS_MEM_WRITE) prot |= PROT_WRITE;
    if (Flags & NVOS_MEM_EXECUTE) prot |= PROT_EXEC;

    page_size = (unsigned int)getpagesize();
    adjust_len = Size + (Offset & (page_size-1));
    adjust_len += (size_t)(page_size-1);
    adjust_len &= (size_t)~(page_size-1);
    *pVirtAddr = mmap(NULL, adjust_len, prot, MAP_SHARED, s_nvmapfd, 0);
    if (!*pVirtAddr)
        return NvError_InsufficientMemory;

    op.handle = (struct nvmap_handle *)hMem;
    op.addr   = (unsigned long)*pVirtAddr;
    op.offset = Offset & ~(page_size-1);
    op.length = adjust_len;
    op.flags = 0;
    if (Flags & NVOS_MEM_MAP_WRITEBACK)
        op.flags = NVMAP_HANDLE_CACHEABLE;
    else if (Flags & NVOS_MEM_MAP_INNER_WRITEBACK)
        op.flags = NVMAP_HANDLE_INNER_CACHEABLE;

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_MMAP, &op)) {
        munmap(*pVirtAddr, adjust_len);
        *pVirtAddr = NULL;
        return NvError_InvalidAddress;
    }

    *pVirtAddr = (void *)((uintptr_t)*pVirtAddr + (Offset & (page_size-1)));
    return NvSuccess;
}

void NvMapMemCacheMaint(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size,
    NvBool        Writeback,
    NvBool        Invalidate)
{
    struct nvmap_cache_op op = {0};

    if (!pMapping || !hMem || s_nvmapfd < 0 || !(Writeback || Invalidate))
        return;

    op.handle = (struct nvmap_handle *)hMem;
    op.addr = (unsigned long)pMapping;
    op.len  = Size;
    if (Writeback && Invalidate)
        op.op = NVMAP_CACHE_OP_WB_INV;
    else if (Writeback)
        op.op = NVMAP_CACHE_OP_WB;
    else
        op.op = NVMAP_CACHE_OP_WB_INV;

    if (ioctl(s_nvmapfd, NVMAP_IOC_CACHE, &op)) {
        NV_ASSERT(!"Mem handle cache maintenance failed\n");
    }

    if (!Writeback && Invalidate) {
        /* Finish invalidate operation */
        op.handle = (struct nvmap_handle *)hMem;
        op.addr = (unsigned long)pMapping;
        op.len = 1;
        op.op = NVMAP_CACHE_OP_INV;
        if (ioctl(s_nvmapfd, NVMAP_IOC_CACHE, &op)) {
            NV_ASSERT(!"Mem handle cache maintenance failed\n");
        }
    }
}

void NvMapMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 Size)
{
    unsigned int page_size;
    size_t adjust_len;
    void *adjust_addr;

    if (!hMem || !pVirtAddr || !Size)
        return;

    page_size = (unsigned int)getpagesize();

    adjust_addr = (void*)((uintptr_t)pVirtAddr & ~(page_size-1));
    adjust_len = Size;
    adjust_len += ((uintptr_t)pVirtAddr & (page_size-1));
    adjust_len += (size_t)(page_size-1);
    adjust_len &= ~(page_size-1);

    munmap(adjust_addr, adjust_len);
}

void NvMapMemReadStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 SrcStride,
    void *pDst,
    NvU32 DstStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    struct nvmap_rw_handle op = {0};
#if NV_USE_VALGRIND
    NvU32 i = 0;
#endif

    op.handle = (struct nvmap_handle *)hMem;
    op.addr   = (unsigned long)pDst;
    op.elem_size = ElementSize;
    op.hmem_stride = SrcStride;
    op.user_stride = DstStride;
    op.count = Count;
    op.offset = Offset;

    /* FIXME: add support for EINTR */
    if (ioctl(s_nvmapfd, NVMAP_IOC_READ, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_READ failed: %s\n", strerror(errno));
        return;
    }

#if NV_USE_VALGRIND
    // Tell Valgrind about memory that is now assumed to have valid data
    for(i = 0; i < Count; i++)
    {
        VALGRIND_MAKE_MEM_DEFINED(pDst + i*DstStride, ElementSize);
    }
#endif
}

void NvMapMemWriteStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 DstStride,
    const void *pSrc,
    NvU32 SrcStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    struct nvmap_rw_handle op = {0};

    op.handle = (struct nvmap_handle *)hMem;
    op.addr   = (unsigned long)pSrc;
    op.elem_size = ElementSize;
    op.hmem_stride = DstStride;
    op.user_stride = SrcStride;
    op.count = Count;
    op.offset = Offset;

    /* FIXME: add support for EINTR */
    if (ioctl(s_nvmapfd, NVMAP_IOC_WRITE, &op))
        NvOsDebugPrintf("NVMAP_IOC_WRITE failed: %s\n", strerror(errno));
}

NvU32 NvMapMemGetSize(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};
    int e;

    op.handle = (struct nvmap_handle *)hMem;
    op.param = NVMAP_HANDLE_PARAM_SIZE;
    e = ioctl(s_nvmapfd, (int)NVMAP_IOC_PARAM, &op);
    if (e) {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %s\n", strerror(errno));
        return 0;
    }
    return (NvU32)op.result;
}

NvU32 NvMapMemGetAlignment(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};

    op.handle = (struct nvmap_handle *)hMem;
    op.param = NVMAP_HANDLE_PARAM_ALIGNMENT;
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_PARAM, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %s\n", strerror(errno));
        return 0;
    }
    return (NvU32)op.result;
}

NvU32 NvMapMemGetAddress(NvRmMemHandle hMem, NvU32 Offset)
{
    struct nvmap_handle_param op = {0};
    NvU32 addr;

    op.handle = (struct nvmap_handle *)hMem;
    op.param = NVMAP_HANDLE_PARAM_BASE;

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_PARAM, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %s\n", strerror(errno));
        return ~0ul;
    }

    addr = (NvU32)op.result;

    if (addr == ~0ul)
        return addr;

    return addr + Offset;
}

NvRmHeap NvMapMemGetHeapType(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};

    op.handle = (struct nvmap_handle *)hMem;
    op.param = NVMAP_HANDLE_PARAM_HEAP;

    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_PARAM, &op))
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %s\n", strerror(errno));
        return (NvRmHeap) 0;
    }

    if (op.result & NVMAP_HEAP_SYSMEM)
        return NvRmHeap_External;
    else if (op.result & NVMAP_HEAP_IOVMM)
        return NvRmHeap_GART;
    else if (op.result & NVMAP_HEAP_CARVEOUT_IRAM)
        return NvRmHeap_IRam;
    else if (op.result & NVMAP_HEAP_CARVEOUT_VPR)
        return NvRmHeap_VPR;
    else
        return NvRmHeap_ExternalCarveOut;
}

void NvMapMemSetFileId(int fd)
{
    s_nvmapfd = fd;
}

NvError NvMapMemDmaBufFdFromHandle(NvRmMemHandle hMem, int *fd)
{
    struct nvmap_create_handle op = {{0}};

    op.id = (NvU32)hMem;
    if (ioctl(s_nvmapfd, (int)NVMAP_IOC_SHARE, &op))
        return ConvertError(errno);
    *fd = (int)op.fd;
    return NvSuccess;
}
