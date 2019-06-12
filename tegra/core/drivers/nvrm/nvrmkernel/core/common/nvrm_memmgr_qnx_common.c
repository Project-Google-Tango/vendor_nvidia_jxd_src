/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


#include <sys/mman.h>
#include <sys/cache.h>
#include <sys/neutrino.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <qnx/nvmap_devctl.h>
#include <nvrm_heap_simple.h>
#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvrm_memmgr_private.h"
#include "nvrm_arm_cp.h"

extern int g_nvmapfd;

#undef CONVERT_CARVEOUT_TO_GART
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
static NvU32 s_PageSize;

int NvRm_MemmgrGetIoctlFile(void)
{
    if (g_nvmapfd < 0)
    {
        g_nvmapfd = open("/dev/nvmap", O_RDWR);
        NV_ASSERT(g_nvmapfd >= 0);
    }
    return g_nvmapfd;
}

static NvError ConvertError(int err)
{
    switch (err)
    {
        case EPERM:
            return NvError_AccessDenied;
        case ENOMEM:
            return NvError_InsufficientMemory;
        case EINVAL:
            return NvError_BadValue;
        default:
            return NvError_IoctlFailed;
    }
}

NvError NvRmMemHandleCreate(
    NvRmDeviceHandle hRm,
    NvRmMemHandle *hMem,
    NvU32 Size)
{
    struct nvmap_create_handle op;
    int err;

    if (g_nvmapfd < 0)
    {
        g_nvmapfd = open("/dev/nvmap", O_RDWR);
        NV_ASSERT(g_nvmapfd >= 0);
    }

    if (hMem == NULL || g_nvmapfd < 0)
        return NvError_NotInitialized;

    op.size = Size;
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_CREATE, &op, sizeof(op), NULL)) != EOK)
        return ConvertError(err);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

NvError NvRmMemHandleFromId(NvU32 Id, NvRmMemHandle *hMem)
{
    struct nvmap_create_handle op = {{0}};
    int err;

    if (hMem == NULL)
        return NvError_NotInitialized;

    op.id = Id;
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_FROM_ID, &op, sizeof(op), NULL)) != EOK)
        return ConvertError(err);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

NvError NvRmMemHandleFromFd(int fd, NvRmMemHandle *hMem)
{
    struct nvmap_create_handle op = {{0}};
    int err;

    if (hMem == NULL || fd <= 0)
        return NvError_NotInitialized;

    if ((err = devctl(fd, NVMAP_IOC_FROM_FD, &op, sizeof(op), NULL)) != EOK)
        return ConvertError(err);
    *hMem = (NvRmMemHandle)op.handle;
    return NvSuccess;
}

static NvError NvRmMemAllocInternal(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvRmMemKind Kind)
{
    struct nvmap_alloc_handle op = {0};
    int err;

    if (Alignment & (Alignment-1)) {
        printf("bad alloc %08x\n", Alignment);
        return NvError_BadValue;
    }

    op.handle = (rmos_u32)hMem;
    op.align = Alignment;

    if (Coherency == NvOsMemAttribute_InnerWriteBack)
        op.flags = NVMAP_HANDLE_INNER_CACHEABLE;
    else if (Coherency == NvOsMemAttribute_WriteBack)
        op.flags = NVMAP_HANDLE_CACHEABLE;
    else if (Coherency == NvOsMemAttribute_WriteCombined)
        op.flags = NVMAP_HANDLE_WRITE_COMBINE;
    else
        op.flags = NVMAP_HANDLE_UNCACHEABLE;

    op.kind = (NvU8)Kind;

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

        err = devctl(g_nvmapfd, NVMAP_IOC_ALLOC, &op, sizeof(op), NULL);
        if (err == EOK)
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
                op.heap_mask = NVMAP_HEAP_IOVMM;
                break;
            case NvRmHeap_IRam:
                op.heap_mask = NVMAP_HEAP_CARVEOUT_IRAM;
                break;
            case NvRmHeap_External:
                 op.heap_mask = NVMAP_HEAP_SYSMEM;
                 break;
            default:
                op.heap_mask = 0;
                break;
            }

            if ((err = devctl(g_nvmapfd, NVMAP_IOC_ALLOC, &op, sizeof(op), NULL)) == EOK)
                return NvSuccess;

            if (err != ENOMEM)
                break;
        }
    }

    if (err == EPERM)
        return NvError_AccessDenied;
    else if (err == EINVAL)
        return NvError_BadValue;
    else
        return NvError_InsufficientMemory;
}

NvError NvRmMemAlloc(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency)
{
    return NvRmMemAllocInternal(hMem, Heaps, NumHeaps, Alignment, Coherency,
             NvRmMemKind_Pitch);
}

NvError NvRmMemAllocTagged(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvU16 Tags)
{
    return NvRmMemAllocInternal(hMem, Heaps, NumHeaps, Alignment, Coherency,
             NvRmMemKind_Pitch);
}

void NvRmMemHandleFree(NvRmMemHandle hMem)
{
    unsigned int handle = (unsigned int)hMem;
    int err;
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_FREE, &handle, sizeof(unsigned), NULL)) != EOK)
        NvOsDebugPrintf("NVMAP_IOC_FREE failed: %d\n", err);
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
    NvError e;
    e = NvRmMemHandleCreate(hRm, hMem, attr->Size);
    if (e != NvSuccess)
            goto exit;
    e = NvRmMemAllocInternal(*hMem, attr->Heaps, attr->NumHeaps,
            attr->Alignment, attr->Coherency, attr->Kind);
    if (e != NvSuccess) {
        NvRmMemHandleFree(*hMem);
        *hMem = NULL;
    }
exit:
    return e;
}

NvU32 NvRmMemGetId(NvRmMemHandle hMem)
{
    struct nvmap_create_handle op = {{0}};
    int err;
    op.handle = (rmos_u32)hMem;
    op.id = 0;

    if ((err = devctl(g_nvmapfd, NVMAP_IOC_GET_ID, &op, sizeof(op), NULL)) != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_GET_ID failed: %d\n", err);
        return 0;
    }
    return (NvU32)op.id;
}

int NvRmMemGetFd(NvRmMemHandle hMem)
{
    int fd;
    struct nvmap_create_handle op = {{0}};
    int err;
    op.handle = (rmos_u32)hMem;

    fd = open("/dev/nvmap", O_RDWR);
    if (fd == -1)
        return 0;

    if ((err = devctl(fd, NVMAP_IOC_GET_FD, &op, sizeof(op), NULL)) != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_GET_FD failed: %d\n", err);
        close(fd);
        fd = 0;
    }
    return fd;
}

void NvRmMemUnpinMult(NvRmMemHandle *hMems, NvU32 Count)
{
    int err;
    if (hMems == NULL)
        return;

    if (Count == 1)
    {
        struct nvmap_pin_handle op = {0};
        op.count = Count;
        op.handles = (unsigned long)hMems[0];

        if ((err = devctl(g_nvmapfd, NVMAP_IOC_UNPIN_MULT, &op, sizeof(op), NULL)) != EOK)
            NvOsDebugPrintf("NVMAP_IOC_UNPIN_MULT failed: %d\n", err);
    }
    else
    {
        NvU8 *ptr;
        NvU32 len;
        struct nvmap_pin_handle *op;
        int err;

        len = sizeof(*op) + Count * sizeof(op->handles);
        op = NvOsAlloc(len);
        if (op == NULL)
        {
            NvOsDebugPrintf("%s: insufficient memory\n", __func__);
            return;
        }

        NvOsMemset(op, 0, len);

        op->count = Count;

        ptr = (NvU8 *)op + sizeof(*op);
        NvOsMemcpy(ptr, hMems, Count * (sizeof(op->handles)));

        if ((err = devctl(g_nvmapfd, NVMAP_IOC_UNPIN_MULT, op, len, NULL)) != EOK)
            NvOsDebugPrintf("NVMAP_IOC_UNPIN_MULT failed: %d\n", err);
        NvOsFree(op);
    }
}

void NvRmMemPinMult(NvRmMemHandle *hMems, NvU32 *addrs, NvU32 Count)
{
    if (hMems == NULL)
        return;

    if (Count == 1)
    {
        struct nvmap_pin_handle op = {0};
        int err;
        op.count = Count;
        op.handles = (unsigned long)hMems[0];
        op.addr = 0;

        if ((err = devctl(g_nvmapfd, NVMAP_IOC_PIN_MULT, &op, sizeof(op), NULL)) != EOK)
            NvOsDebugPrintf("NVMAP_IOC_PIN_MULT failed: %d\n", err);
        *addrs = op.addr;
    }
    else
    {
        NvU8 *ptr;
        NvU32 len;
        struct nvmap_pin_handle *op;
        int err;

        len = sizeof(*op) + Count * (sizeof(op->handles) + sizeof(op->addr));
        op = NvOsAlloc(len);
        if (op == NULL)
        {
            NvOsDebugPrintf("%s: insufficient memory\n", __func__);
            return;
        }

        NvOsMemset(op, 0, len);

        op->count = Count;

        ptr = (NvU8 *)op + sizeof(*op);
        NvOsMemcpy(ptr, hMems, Count * (sizeof(op->handles)));

        if ((err = devctl(g_nvmapfd, NVMAP_IOC_PIN_MULT, op, len, NULL)) != EOK) {
            NvOsDebugPrintf("NVMAP_IOC_PIN_MULT failed: %d\n", err);
            NvOsFree(op);
            return;
        }

        ptr = (NvU8 *)op + sizeof(*op) + op->count * sizeof(op->handles);
        NvOsMemcpy(addrs, ptr, Count * (sizeof(op->addr)));
        NvOsFree(op);
    }
}

NvU32 NvRmMemPin(NvRmMemHandle hMem)
{
    NvU32 addr = ~0;
    NvRmMemPinMult(&hMem, &addr, 1);
    return addr;
}

void NvRmMemUnpin(NvRmMemHandle hMem)
{
    NvRmMemUnpinMult(&hMem, 1);
}

NvError NvRmMemMap(
    NvRmMemHandle hMem,
    NvU32         Offset,
    NvU32         Size,
    NvU32         Flags,
    void        **pVirtAddr)
{
    struct nvmap_map_caller op = {0};
    unsigned int page_size = s_PageSize;
    size_t adjust_len;

    if (!pVirtAddr || !hMem)
        return NvError_BadParameter;

    if (g_nvmapfd < 0)
        return NvError_KernelDriverNotFound;

    adjust_len = Size + (Offset & (page_size - 1));
    adjust_len += (size_t)(page_size - 1);
    adjust_len &= (size_t)~(page_size - 1);

    op.handle = (uintptr_t)hMem;
    op.addr   = (unsigned long)*pVirtAddr;
    op.offset = Offset & ~(page_size - 1);
    op.length = adjust_len;
    op.flags = 0;
    op.pid = getpid();
    op.prot = PROT_NONE;
    if (Flags & NVOS_MEM_READ)
        op.prot |= PROT_READ;
    if (Flags & NVOS_MEM_WRITE)
        op.prot |= PROT_WRITE;
    if (Flags & NVOS_MEM_EXECUTE)
        op.prot |= PROT_EXEC;

    if (devctl(g_nvmapfd, NVMAP_IOC_MMAP, &op, sizeof(op), NULL) != EOK) {
        *pVirtAddr = NULL;
        return NvError_InvalidAddress;
    }

    *pVirtAddr = (void *)(op.addr + (Offset & (page_size - 1)));
    return NvSuccess;
}

void NvRmMemCacheMaint(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size,
    NvBool        Writeback,
    NvBool        Invalidate)
{
    struct nvmap_cache_op op = {0};
    int err;

    if ((pMapping == NULL) || (hMem == NULL))
        return;

    op.handle = (rmos_u32)hMem;
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_CACHE, &op, sizeof(op), NULL))
            != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_CACHE failed: %d\n", err);
        return;
    }

    if (op.cache_flags == NVMAP_HANDLE_CACHEABLE ||
        op.cache_flags == NVMAP_HANDLE_INNER_CACHEABLE)
    {
        int flags = 0;

        if (Writeback)
            flags |= MS_SYNC;

        if (Invalidate)
            flags |= MS_INVALIDATE;

        if (!flags)
            return;

        flags |= MS_CACHE_ONLY;
        msync(pMapping, Size, flags);
    }
    else if (op.cache_flags == NVMAP_HANDLE_WRITE_COMBINE)
    {
        NvOsFlushWriteCombineBuffer();
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

void NvRmMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 Size)
{
    unsigned int page_size = s_PageSize;
    size_t adjust_len;
    void *adjust_addr;
    unsigned int handle = (unsigned int)hMem;
    int err;

    if (!hMem || !pVirtAddr || !Size)
        return;

    adjust_addr = (void *)((uintptr_t)pVirtAddr & ~(page_size - 1));
    adjust_len = Size;
    adjust_len += ((uintptr_t)pVirtAddr & (page_size - 1));
    adjust_len += (size_t)(page_size - 1);
    adjust_len &= ~(page_size - 1);

    munmap_flags(adjust_addr, adjust_len, UNMAP_INIT_OPTIONAL);
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_MUNMAP, &handle, sizeof(unsigned int), NULL))
            != EOK)
        NvOsDebugPrintf("NVMAP_IOC_UNMAP failed: %d\n", err);
}

void NvRmMemWrite(NvRmMemHandle hMem,NvU32 Offset,const void *pSrc, NvU32 Size)
{
    NvRmMemWriteStrided(hMem, Offset, Size, pSrc, Size, Size, 1);
}

void NvRmMemRead(NvRmMemHandle hMem, NvU32 Offset, void *pDst, NvU32 Size)
{
    NvRmMemReadStrided(hMem, Offset, Size, pDst, Size, Size, 1);
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
    iov_t sv[1];
    iov_t rv[2];
    struct nvmap_rw_handle op;
    int ret;

    op.handle = (rmos_u32)hMem;
    op.elem_size = ElementSize;
    op.hmem_stride = SrcStride;
    op.user_stride = DstStride;
    op.count = Count;
    op.offset = Offset;

    SETIOV(&sv[0], &op, sizeof(op));
    SETIOV(&rv[0], &op, sizeof(op));
    SETIOV(&rv[1], pDst, (DstStride * (Count - 1) + ElementSize));
    /* FIXME: add support for EINTR */
    ret = devctlv(g_nvmapfd, NVMAP_IOC_READ, NV_ARRAY_SIZE(sv),
                  NV_ARRAY_SIZE(rv), sv, rv, NULL);
    if (ret != EOK)
        NvOsDebugPrintf("NVMAP_IOC_READ failed: %d\n", ret);
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
    iov_t sv[2];
    struct nvmap_rw_handle op;
    int ret;

    op.handle = (rmos_u32)hMem;
    op.elem_size = ElementSize;
    op.hmem_stride = DstStride;
    op.user_stride = SrcStride;
    op.count = Count;
    op.offset = Offset;

    SETIOV(&sv[0], &op, sizeof(op));
    SETIOV(&sv[1], pSrc, (SrcStride * (Count - 1) + ElementSize));
    ret = devctlv(g_nvmapfd, NVMAP_IOC_WRITE, NV_ARRAY_SIZE(sv), 0, sv, NULL,
                  NULL);
    if (ret != EOK)
        NvOsDebugPrintf("NVMAP_IOC_WRITE failed: %d\n", ret);
}

NvU32 NvRmMemGetSize(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};
    int err;

    op.handle = (unsigned long)hMem;
    op.param = NVMAP_HANDLE_PARAM_SIZE;
    err = devctl(g_nvmapfd, NVMAP_IOC_PARAM, &op, sizeof(op), NULL);
    if (err != EOK) {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %d\n", err);
        return 0;
    }
    return (NvU32)op.result;
}

void NvRmMemMove(NvRmMemHandle hDstMem, NvU32 DstOffset,
    NvRmMemHandle hSrcMem, NvU32 SrcOffset, NvU32 Size)
{
    NvU8 Buffer[8192];

    while (Size)
    {
        NvU32 Count = NV_MIN(sizeof(Buffer), Size);
        NvRmMemRead(hSrcMem, SrcOffset, Buffer, Count);
        NvRmMemWrite(hDstMem, DstOffset, Buffer, Count);
        Size -= Count;
        DstOffset += Count;
        SrcOffset += Count;
    }
}

NvU32 NvRmMemGetAlignment(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};
    int err;

    op.handle = (unsigned long)hMem;
    op.param = NVMAP_HANDLE_PARAM_ALIGNMENT;
    if ((err = devctl(g_nvmapfd, NVMAP_IOC_PARAM, &op, sizeof(op), NULL))
            != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %d\n", err);
        return 0;
    }
    return (NvU32)op.result;
}

NvU32 NvRmMemGetCacheLineSize(void)
{
    static NvS32 s_IsCortexA9 = -1;
    NvU32 tmp = 0;
    if (s_IsCortexA9 == -1)
    {
        __asm__ __volatile__("mrc p15, 0, %0, c0, c0, 0" : "=r" (tmp));
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

NvU32 NvRmMemGetAddress(NvRmMemHandle hMem, NvU32 Offset)
{
    struct nvmap_handle_param op = {0};
    NvU32 addr;
    int err;

    op.handle = (unsigned long)hMem;
    op.param = NVMAP_HANDLE_PARAM_BASE;

    if ((err = devctl(g_nvmapfd, NVMAP_IOC_PARAM, &op, sizeof(op), NULL))
            != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %d\n", err);
        return ~0ul;
    }

    addr = (NvU32)op.result;

    if (addr == ~0ul)
        return addr;

    return addr + Offset;
}

NvRmHeap NvRmMemGetHeapType(NvRmMemHandle hMem)
{
    struct nvmap_handle_param op = {0};
    int err;

    op.handle = (unsigned long)hMem;
    op.param = NVMAP_HANDLE_PARAM_HEAP;

    if ((err = devctl(g_nvmapfd, NVMAP_IOC_PARAM, &op, sizeof(op), NULL))
            != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %d\n", err);
        return (NvRmHeap) 0;
    }

    if (op.result & NVMAP_HEAP_SYSMEM)
        return NvRmHeap_External;
    else if (op.result & NVMAP_HEAP_IOVMM)
        return NvRmHeap_GART;
    else if (op.result & NVMAP_HEAP_CARVEOUT_IRAM)
        return NvRmHeap_IRam;
    else
        return NvRmHeap_ExternalCarveOut;
}

NvError NvRmMemGetStat(NvRmMemStat Stat, NvS32 *Result)
{
    /* FIXME: implement */
    return NvError_NotSupported;
}

NvError NvRmMemAllocBlocklinear(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvRmMemKind Kind,
    NvRmMemCompressionTags CompressionTags)
{
    if (CompressionTags != NvRmMemCompressionTags_None)
        return NvError_NotSupported;

    return NvRmMemAllocInternal(hMem, Heaps, NumHeaps, Alignment, Coherency,
             Kind);
}

NvError NvRmMemGetKind(NvRmMemHandle hMem, NvRmMemKind *Result)
{
    struct nvmap_handle_param op = {0};
    int err;

    op.handle = (rmos_u32)hMem;
    op.param = NVMAP_HANDLE_PARAM_KIND;

    err = devctl(g_nvmapfd, NVMAP_IOC_PARAM, &op, sizeof(op), NULL);
    if (err != EOK)
    {
        NvOsDebugPrintf("NVMAP_IOC_PARAM failed: %d\n", err);
        return ConvertError(err);
    }

    *Result = (NvRmMemKind)op.result;

    return NvSuccess;
}

NvError NvRmMemGetCompressionTags(NvRmMemHandle hMem, NvRmMemCompressionTags *Result)
{
    *Result = NvRmMemCompressionTags_None;
    return NvSuccess;
}

static void __attribute__((constructor)) nvmap_constructor(void)
{
    g_nvmapfd = open(NVMAP_DEV_NODE, O_RDWR);
    if (g_nvmapfd < 0)
        NvOsDebugPrintf("\n\n\n****nvmap device open failed****\n\n\n");

    s_PageSize = getpagesize();
}

static void __attribute__((destructor)) nvmap_destructor(void)
{
    if (g_nvmapfd >= 0)
        close(g_nvmapfd);
}
