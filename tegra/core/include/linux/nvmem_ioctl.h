/*
 * Copyright (c) 2009-2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#ifndef _MACH_TEGRA_NVMEM_IOCTL_H_
#define _MACH_TEGRA_NVMEM_IOCTL_H_

struct nvmem_create_handle {
    union {
        __u32 key;   /* ClaimPreservedHandle */
        __u32 id;    /* FromId */
        __u32 size;  /* CreateHandle */
        __s32 fd;    /* DmaBufFd or FromFd */
    };
    __u32 handle;
};

#define NVMEM_HEAP_SYSMEM  (1ul<<31)
#define NVMEM_HEAP_IOVMM   (1ul<<30)

/* common carveout heaps */
#define NVMEM_HEAP_CARVEOUT_IRAM    (1ul<<29)
#define NVMEM_HEAP_CARVEOUT_VPR     (1ul<<28)
#define NVMEM_HEAP_CARVEOUT_GENERIC (1ul<<0)

#define NVMEM_HEAP_CARVEOUT_MASK    (NVMEM_HEAP_IOVMM - 1)

#define NVMEM_HANDLE_UNCACHEABLE     (0x0ul << 0)
#define NVMEM_HANDLE_WRITE_COMBINE   (0x1ul << 0)
#define NVMEM_HANDLE_INNER_CACHEABLE (0x2ul << 0)
#define NVMEM_HANDLE_CACHEABLE       (0x3ul << 0)

#define NVMEM_HANDLE_SECURE          (0x1ul << 2)
#define NVMEM_HANDLE_KIND_SPECIFIED  (0x1ul << 3)
#define NVMEM_HANDLE_COMPR_SPECIFIED (0x1ul << 4)

struct nvmem_alloc_handle {
    __u32 handle;
    __u32 heap_mask;
    __u32 flags;
    __u32 align;
};

struct nvmem_map_caller {
    __u32 handle;       /* hmem */
    __u32 offset;       /* offset into hmem; should be page-aligned */
    __u32 length;       /* number of bytes to map */
    __u32 flags;
    unsigned long addr; /* user pointer */
};

struct nvmem_rw_handle {
    unsigned long addr; /* user pointer */
    __u32 handle;       /* hmem */
    __u32 offset;       /* offset into hmem */
    __u32 elem_size;    /* individual atom size */
    __u32 hmem_stride;  /* delta in bytes between atoms in hmem */
    __u32 user_stride;  /* delta in bytes between atoms in user */
    __u32 count;        /* number of atoms to copy */
};

struct nvmem_pin_handle {
    unsigned long handles; /* array of handles to pin/unpin */
    unsigned long addr;    /* array of addresses to return */
    unsigned long count;   /* number of entries in handles */
};

enum {
    NVMEM_HANDLE_PARAM_SIZE = 1,
    NVMEM_HANDLE_PARAM_ALIGNMENT,
    NVMEM_HANDLE_PARAM_BASE,
    NVMEM_HANDLE_PARAM_HEAP,
#define NVMEM_KIND_SUPPORTED 1
    NVMEM_HANDLE_PARAM_KIND,
    NVMEM_HANDLE_PARAM_COMPR,
};

struct nvmem_handle_param {
    __u32 handle;
    __u32 param;
    unsigned long result;
};

enum {
    NVMEM_CACHE_OP_WB = 0,
    NVMEM_CACHE_OP_INV,
    NVMEM_CACHE_OP_WB_INV,
};

struct nvmem_cache_op {
    unsigned long addr;
    __u32 handle;
    __u32 len;
    __s32 op;
};

#define NVMEM_IOC_MAGIC 'N'

/* Creates a new memory handle. On input, the argument is the size of the new
 * handle; on return, the argument is the name of the new handle 
 */
#define NVMEM_IOC_CREATE  _IOWR(NVMEM_IOC_MAGIC, 0, struct nvmem_create_handle)
#define NVMEM_IOC_CLAIM   _IOWR(NVMEM_IOC_MAGIC, 1, struct nvmem_create_handle)
#define NVMEM_IOC_FROM_ID _IOWR(NVMEM_IOC_MAGIC, 2, struct nvmem_create_handle)

/* Returns a global ID usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMEM_IOC_GET_ID  _IOWR(NVMEM_IOC_MAGIC, 13, struct nvmem_create_handle)

/* Actually allocates memory for the specified handle */
#define NVMEM_IOC_ALLOC    _IOW (NVMEM_IOC_MAGIC, 3, struct nvmem_alloc_handle)

/* Frees a memory handle, unpinning any pinned pages and unmapping any mappings
 */
#define NVMEM_IOC_FREE       _IO (NVMEM_IOC_MAGIC, 4)

/* Maps the region of the specified handle into a user-provided virtual address
 * that was previously created via an mmap syscall on this fd */
#define NVMEM_IOC_MMAP       _IOWR(NVMEM_IOC_MAGIC, 5, struct nvmem_map_caller)

/* Reads/writes data (possibly strided) from a user-provided buffer into the
 * hmem at the specified offset */
#define NVMEM_IOC_WRITE      _IOW (NVMEM_IOC_MAGIC, 6, struct nvmem_rw_handle)
#define NVMEM_IOC_READ       _IOW (NVMEM_IOC_MAGIC, 7, struct nvmem_rw_handle)

#define NVMEM_IOC_PARAM _IOWR(NVMEM_IOC_MAGIC, 8, struct nvmem_handle_param)

/* Pins a list of memory handles into IO-addressable memory (either IOVMM
 * space or physical memory, depending on the allocation), and returns the
 * address. Handles may be pinned recursively. */
#define NVMEM_IOC_PIN_MULT   _IOWR(NVMEM_IOC_MAGIC, 10, struct nvmem_pin_handle)
#define NVMEM_IOC_UNPIN_MULT _IOW (NVMEM_IOC_MAGIC, 11, struct nvmem_pin_handle)

#define NVMEM_IOC_CACHE      _IOW (NVMEM_IOC_MAGIC, 12, struct nvmem_cache_op)

/* Returns a dma-buf fd usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMEM_IOC_SHARE  _IOWR(NVMEM_IOC_MAGIC, 14, struct nvmem_create_handle)

/* Returns a file id that allows a remote process to create a handle
 * reference to the same handle */
#define NVMEM_IOC_GET_FD  _IOWR(NVMEM_IOC_MAGIC, 15, struct nvmem_create_handle)

/* Create a new memory handle from file id passed */
#define NVMEM_IOC_FROM_FD _IOWR(NVMEM_IOC_MAGIC, 16, struct nvmem_create_handle)

struct nvmem_alloc_kind_handle {
    __u32 handle;
    __u32 heap_mask;
    __u32 flags;
    __u32 align;
    __u8  kind;
    __u8  comp_tags; /* deprecated, to be removed */
};
#define NVMEM_IOC_ALLOC_KIND _IOW(NVMEM_IOC_MAGIC, 100, struct nvmem_alloc_kind_handle)

#define NVMEM_IOC_MAXNR (_IOC_NR(NVMEM_IOC_FROM_FD))
#endif
