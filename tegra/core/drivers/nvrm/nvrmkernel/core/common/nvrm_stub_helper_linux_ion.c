/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
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
#include <stdlib.h>
#include "nvcommon.h"
#include "nvassert.h"
#include "nvos.h"
#include "nvrm_memmgr.h"
#include "nvrm_power.h"
#include "nvrm_rmctrace.h"
#include "nvrm_memmgr.h"
#include "linux/nvmem_ioctl.h"
#include "linux/tegra_ion.h"
#include "nvrm_stub_helper_linux.h"

#if defined(ANDROID) && !defined(PLATFORM_IS_GINGERBREAD) && NV_DEBUG
#include <memcheck.h>
#define NV_USE_VALGRIND 1
#else
#define NV_USE_VALGRIND 0
#endif

static int s_IonFd = -1;
#define DEBUG_ION 0

#define PR_FMT(fmt) "\n%s: " fmt, __func__
#define ER_FMT(fmt) "\n*err* %s:%d " fmt, __func__, __LINE__

#define ION_INFO(fmt, ...) \
do { \
    if (DEBUG_ION) \
        NvOsDebugPrintf(PR_FMT(fmt), ##__VA_ARGS__, "\n"); \
} while (0)

#define ION_ERR(fmt, ...) \
do { \
    NvOsDebugPrintf(ER_FMT(fmt), ##__VA_ARGS__, "\n"); \
} while (0)

#define NVRM_MEM_MAGIC 0xbabecafe

typedef struct NvRmMemRec
{
    struct ion_handle *handle;
    unsigned int magic;
    int map_fd;
    int map_count;
    void *map_addr;
    size_t size;
} NvRmMem;

#define CHECK_MEM_MAGIC() \
do { \
    if (hMem->magic != NVRM_MEM_MAGIC) \
        ION_ERR("magic didn't match."); \
} while (0)

static int IonIoctl(int fd, int req, void *arg)
{
    int ret = ioctl(fd, req, arg);
    if (ret < 0)
    {
        ION_ERR("ioctl 0x%x failed with code 0x%x: %s", req,
           ret, strerror(errno));
        return -errno;
    }
    return ret;
}

static NvError ConvertKernelError(int ret)
{
    if (errno == EPERM)
        return NvError_AccessDenied;
    else if (errno == ENOMEM)
        return NvError_InsufficientMemory;
    else if (errno == EINVAL)
        return NvError_NotInitialized;

    return NvError_AccessDenied;
}

NvError IonMemHandleCreate(
    NvRmDeviceHandle hRm,
    NvRmMemHandle *hMem,
    NvU32 Size)
{
    NvRmMem *mem = malloc(sizeof(*mem));
    unsigned int page_size = (unsigned int)getpagesize();

    ION_INFO("n");
    if (mem == NULL)
    {
        ION_ERR("Out of malloc memory");
        return NvError_InsufficientMemory;
    }
    memset(mem, 0, sizeof(*mem));
    mem->magic = NVRM_MEM_MAGIC;
    mem->size = (Size + (page_size - 1)) & ~(page_size - 1);
    mem->handle = (struct ion_handle *)0xABABABAB;
    *hMem = mem;
    ION_INFO("hMem=0x%x, orig_Size=0x%x, alloc_size=0x%x pid=%d x",
        mem, Size, mem->size, getpid());
    return NvSuccess;
}

NvError IonMemHandleFromId(NvU32 Id, NvRmMemHandle *hMem)
{
    struct tegra_ion_id_data id_data = {
        .handle = 0,
        .id = Id,
        .size = 0,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_ALLOC_FROM_ID,
        .arg = (unsigned long)&id_data,
    };
    NvRmMem *mem = malloc(sizeof(*mem));
    int ret;

    *hMem = NULL;
    ION_INFO("n");
    if (mem == NULL)
    {
        ION_ERR("Out of malloc memory");
        return NvError_InsufficientMemory;
    }
    memset(mem, 0, sizeof(*mem));
    mem->magic = NVRM_MEM_MAGIC;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret) {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return ConvertKernelError(ret);
    }
    mem->handle = id_data.handle;
    mem->size = id_data.size;

    *hMem = mem;
    ION_INFO("Id=0x%x, hMem=0x%x , ih=0x%x pid=%d x",
        Id, mem, mem->handle,getpid());
    return ret;
}

NvError IonMemAllocInternalTagged(
    NvRmMemHandle hMem,
    const NvRmHeap *Heaps,
    NvU32 NumHeaps,
    NvU32 Alignment,
    NvOsMemAttribute Coherency,
    NvRmMemKind Kind,
    NvRmMemCompressionTags CompressionTags,
    NvU16 Tags)
{
    int ret = 0;
    unsigned int i;
    struct ion_allocation_data data = {
        .len = 0,
        .align = Alignment,
        .flags = (1 << TEGRA_ION_HEAP_IOMMU |
                  1 << TEGRA_ION_HEAP_CARVEOUT),
    };

    ION_INFO("hMem=0x%x, n", hMem);
    CHECK_MEM_MAGIC();
    if (Alignment & (Alignment-1))
    {
        ION_ERR("invalid alignment");
        return NvError_BadValue;
    }
    data.len = hMem->size;

    if (NumHeaps == 0)
        ret = IonIoctl(s_IonFd, ION_IOC_ALLOC, &data);

    for (i = 0; i < NumHeaps; i++)
    {
        switch (Heaps[i])
        {
            case NvRmHeap_IRam:
                data.flags = 1 << TEGRA_ION_HEAP_IRAM;
                break;
            case NvRmHeap_VPR:
                data.flags = 1 << TEGRA_ION_HEAP_VPR;
                break;
            case NvRmHeap_ExternalCarveOut:
            case NvRmHeap_GART:
            default:
                data.flags = 1 << TEGRA_ION_HEAP_IOMMU |
                             1 << TEGRA_ION_HEAP_CARVEOUT;
                break;
        }
        ret = IonIoctl(s_IonFd, ION_IOC_ALLOC, &data);
        if (ret == 0)
            break;
    }

    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return ConvertKernelError(ret);
    }
    hMem->handle = data.handle;

    ION_INFO("hMem=0x%x, ionh=0x%x, x", hMem, hMem->handle);
    return NvSuccess;
}

void IonMemHandleFree(NvRmMemHandle hMem)
{
    struct ion_handle_data data;
    int ret;

    if (!hMem)
        return;
    ION_INFO("hMem=0x%x n", hMem);
    CHECK_MEM_MAGIC();
    memset(&data, 0, sizeof(data));
    data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_FREE, &data);
    if (ret != 0)
        ION_ERR("ioctl failed, ret=(%d), %s, h=0x%x, pid=%d",
            ret, strerror(ret), hMem, getpid());
    ION_INFO("hMem=0x%x, ih=0x%x pid=%d x", hMem, hMem->handle, getpid());
    hMem->handle = NULL;
    hMem->magic = 0xEFABCDEF;
    free(hMem);
}

NvU32 IonMemGetId(NvRmMemHandle hMem)
{
    struct tegra_ion_id_data id_data;
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_GET_ID,
        .arg = (unsigned long)&id_data,
    };
    int ret;

    ION_INFO("hMem=0x%x ih=0x%x pid=0x%x n", hMem, hMem->handle, getpid());
    CHECK_MEM_MAGIC();
    memset(&id_data, 0, sizeof(id_data));
    id_data.handle = hMem->handle;

    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return ConvertKernelError(ret);
    }
    ION_INFO("hMem=0x%x, id=0x%x, x", hMem, id_data.id);
    return id_data.id;
}

#define HANDLE_COUNT_ON_STACK 16
void IonMemUnpinMult(NvRmMemHandle *hMems, NvU32 Count)
{
    struct tegra_ion_pin_data unpin_data = {
        .count = Count,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_UNPIN,
        .arg = (unsigned long)&unpin_data,
    };
    int ret;
    unsigned int i;
    struct ion_handle *on_stack[HANDLE_COUNT_ON_STACK];
    struct ion_handle **handles;

    ION_INFO("n");
    if (Count > HANDLE_COUNT_ON_STACK)
    {
        handles = malloc(Count * sizeof(struct ion_handle *));
        if (!handles)
        {
            ION_ERR("Out of malloc memory.");
            return;
        }
    }
    else
        handles = on_stack;

    for (i = 0; i < Count; i++)
    {
        /* Ignore null handles. */
        if (hMems == NULL || hMems[i] == NULL)
            handles[i] = NULL;
        else
            handles[i] = hMems[i]->handle;
    }
    unpin_data.handles = handles;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
    if (handles != on_stack)
        free(handles);
    ION_INFO("x");
}

void IonMemPinMult(NvRmMemHandle *hMems, NvU32 *addrs, NvU32 Count)
{
    struct tegra_ion_pin_data pin_data = {
        .addr = (unsigned long *)addrs,
        .count = Count,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_PIN,
        .arg = (unsigned long)&pin_data,
    };
    int ret;
    unsigned int i;
    struct ion_handle *on_stack[HANDLE_COUNT_ON_STACK];
    struct ion_handle **handles;

    ION_INFO("n");
    if (Count > HANDLE_COUNT_ON_STACK)
    {
            handles = malloc(Count * sizeof(struct ion_handle *));
            if (!handles)
            {
                ION_ERR("Out of malloc memory");
                for (i = 0; i < Count; i++)
                    addrs[i] = 0;
                return;
            }
    }
    else
            handles = on_stack;

    for (i = 0; i < Count; i++)
            handles[i] = hMems[i]->handle;
    pin_data.handles = handles;

    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));

    if (handles != on_stack)
            free(handles);
    ION_INFO("x");
}

NvError IonMemMap(
    NvRmMemHandle hMem,
    NvU32         Offset,
    NvU32         Size,
    NvU32         Flags,
    void        **pVirtAddr)
{
    struct ion_fd_data data;
    int prot;
    int ret;

    ION_INFO(" hMem=0x%x, ionh=0x%x, mc=%d, map_fd=%d n",
            hMem, hMem->handle, hMem->map_count, hMem->map_fd);
    if (!pVirtAddr || !hMem)
            return NvError_BadParameter;

    if (s_IonFd < 0)
            return NvError_KernelDriverNotFound;

    if (hMem->map_count)
    {
        hMem->map_count++;
        ION_INFO("reuse prev mapping");
        goto exit;
    }

    prot = PROT_NONE;
    if (Flags & NVOS_MEM_READ) prot |= PROT_READ;
    if (Flags & NVOS_MEM_WRITE) prot |= PROT_WRITE;
    if (Flags & NVOS_MEM_EXECUTE) prot |= PROT_EXEC;

    memset(&data, 0, sizeof(data));
    data.handle = hMem->handle;

    ret = IonIoctl(s_IonFd, ION_IOC_MAP, &data);
    if (ret < 0)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return ConvertKernelError(ret);
    }
    hMem->map_fd = data.fd;

    hMem->map_addr = mmap(NULL, hMem->size, prot, MAP_SHARED,
                    hMem->map_fd, 0);
    hMem->map_count++;
    ION_INFO("hMmem=0x%x, ionh=0x%x, map_fd=%d, va=0x%x hsize=0x%x,"
        " Size=0x%x off=%d  x", hMem, hMem->handle, data.fd,
        hMem->map_addr, hMem->size, Size, Offset);
    if (!hMem->map_addr) {
        ION_ERR("map error");
        return NvError_InsufficientMemory;
    }

exit:
    *pVirtAddr = (void*)((uintptr_t)hMem->map_addr + Offset);
    ION_INFO("hMem=0x%x, ionh=0x%x, map_fd=%d, va=0x%x x",
            hMem, hMem->handle, data.fd, *pVirtAddr);
    return NvSuccess;
}

void IonMemCacheMaint(
    NvRmMemHandle hMem,
    void         *pMapping,
    NvU32         Size,
    NvBool        Writeback,
    NvBool        Invalidate)
{
    struct tegra_ion_cache_maint_data cache_data = {
        .addr = (unsigned long)pMapping,
        .len = Size,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_CACHE_MAINT,
        .arg = (unsigned long)&cache_data,
    };
    int ret;

    ION_INFO("n");
    cache_data.handle = hMem->handle;
    if (Writeback && Invalidate)
        cache_data.op = TEGRA_ION_CACHE_OP_WB_INV;
    else if (Invalidate)
        cache_data.op = TEGRA_ION_CACHE_OP_INV;
    else
        cache_data.op = TEGRA_ION_CACHE_OP_WB;

    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
    ION_INFO("x");
}

void IonMemUnmap(NvRmMemHandle hMem, void *pVirtAddr, NvU32 Size)
{
    ION_INFO("hMmem=0x%x, ionh=0x%x, va=0x%x n",
            hMem, hMem->handle, pVirtAddr);
    if (!hMem || !pVirtAddr || !Size || !hMem->map_count)
            return;

    hMem->map_count--;
    if (hMem->map_count == 0)
    {
        // FIXME: protect map/unmap.
        munmap(hMem->map_addr, hMem->size);
        close(hMem->map_fd);
        hMem->map_fd = -1;
    }
    ION_INFO("hMmem=0x%x, ionh=0x%x, va=0x%x x",
            hMem, hMem->handle, pVirtAddr);
}

void IonMemReadStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 SrcStride,
    void *pDst,
    NvU32 DstStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    struct tegra_ion_rw_data rw_data = {
        .addr = (unsigned long)pDst,
        .offset = Offset,
        .elem_size = ElementSize,
        .mem_stride = SrcStride,
        .user_stride = DstStride,
        .count = Count,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_READ,
        .arg = (unsigned long)&rw_data,
    };
    int ret;
#if NV_USE_VALGRIND
    NvU32 i = 0;
#endif

    ION_INFO("ih=0x%x n", hMem->handle);
    CHECK_MEM_MAGIC();
    rw_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        ION_INFO(" error x");
        return;
    }
#if NV_USE_VALGRIND
    // Tell Valgrind about memory that is now assumed to have valid data
    for(i = 0; i < Count; i++)
    {
        VALGRIND_MAKE_MEM_DEFINED(pDst + i*DstStride, ElementSize);
    }
#endif
    ION_INFO("x");
}

void IonMemWriteStrided(
    NvRmMemHandle hMem,
    NvU32 Offset,
    NvU32 DstStride,
    const void *pSrc,
    NvU32 SrcStride,
    NvU32 ElementSize,
    NvU32 Count)
{
    struct tegra_ion_rw_data rw_data = {
        .addr = (unsigned long)pSrc,
        .handle = (struct ion_handle *)hMem,
        .offset = Offset,
        .elem_size = ElementSize,
        .mem_stride = DstStride,
        .user_stride = SrcStride,
        .count = Count,
    };
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_WRITE,
        .arg = (unsigned long)&rw_data,
    };
    int ret;

    ION_INFO("ih=0x%x n", hMem->handle);
    CHECK_MEM_MAGIC();
    rw_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);
    if (ret)
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
    ION_INFO("x");
}

NvU32 IonMemGetSize(NvRmMemHandle hMem)
{
    struct tegra_ion_get_params_data param_data;
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_GET_PARAM,
        .arg = (unsigned long)&param_data,
    };
    int ret;

    ION_INFO("n");
    CHECK_MEM_MAGIC();
    memset(&param_data, 0, sizeof(param_data));
    param_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);

    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return 0;
    }
    ION_INFO("x");
    return param_data.size;
}

NvU32 IonMemGetAlignment(NvRmMemHandle hMem)
{
    struct tegra_ion_get_params_data param_data;
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_GET_PARAM,
        .arg = (unsigned long)&param_data,
    };
    int ret;

    ION_INFO("n");
    CHECK_MEM_MAGIC();
    memset(&param_data, 0, sizeof(param_data));
    param_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);

    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        ION_INFO("x");
        return 0;
    }
    ION_INFO("x");
    return param_data.align;
}

NvU32 IonMemGetAddress(NvRmMemHandle hMem, NvU32 Offset)
{
    struct tegra_ion_get_params_data param_data;
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_GET_PARAM,
        .arg = (unsigned long)&param_data,
    };
    NvU32 addr;
    int ret;

    ION_INFO("n");
    CHECK_MEM_MAGIC();
    memset(&param_data, 0, sizeof(param_data));
    param_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);

    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        ION_INFO("x");
        return ~0;
    }
    addr = (NvU32)param_data.addr;
    ION_INFO("x");
    if (addr == ~0ul)
            return addr;
    return addr + Offset;
}

NvRmHeap IonMemGetHeapType(NvRmMemHandle hMem)
{
    struct tegra_ion_get_params_data param_data;
    struct ion_custom_data data = {
        .cmd = TEGRA_ION_GET_PARAM,
        .arg = (unsigned long)&param_data,
    };
    int ret;
    NvRmHeap heap = NvRmHeap_ExternalCarveOut;

    ION_INFO("n");
    CHECK_MEM_MAGIC();
    memset(&param_data, 0, sizeof(param_data));
    param_data.handle = hMem->handle;
    ret = IonIoctl(s_IonFd, ION_IOC_CUSTOM, &data);

    if (ret)
    {
        ION_ERR("ioctl failed, ret=(%d), %s", ret, strerror(ret));
        return ~0;
    }

    switch (param_data.heap) {
        case TEGRA_ION_HEAP_IRAM:
            heap = NvRmHeap_IRam;
            break;
        case TEGRA_ION_HEAP_VPR:
            heap = NvRmHeap_VPR;
            break;
        case TEGRA_ION_HEAP_CARVEOUT:
            heap = NvRmHeap_ExternalCarveOut;
            break;
        case TEGRA_ION_HEAP_IOMMU:
            heap = NvRmHeap_GART;
            break;
    }
    ION_INFO("x");
    return heap;
}

void IonMemSetFileId(int fd)
{
    s_IonFd = fd;
}
