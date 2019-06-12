/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __QNX_NVMAP_DEVCTL_H
#define __QNX_NVMAP_DEVCTL_H

#include <devctl.h>
#include <qnx/rmos_types.h>

#define NVMAP_DEV_NODE  "/dev/nvmap"

enum {
    NVMAP_HANDLE_PARAM_SIZE = 1,
    NVMAP_HANDLE_PARAM_ALIGNMENT,
    NVMAP_HANDLE_PARAM_BASE,
    NVMAP_HANDLE_PARAM_HEAP,
    NVMAP_HANDLE_PARAM_KIND,
};

enum {
    NVMAP_CACHE_OP_WB = 0,
    NVMAP_CACHE_OP_INV,
    NVMAP_CACHE_OP_WB_INV,
};

#define NVMAP_HANDLE_UNCACHEABLE     (0x0ul << 0)
#define NVMAP_HANDLE_WRITE_COMBINE   (0x1ul << 0)
#define NVMAP_HANDLE_INNER_CACHEABLE (0x2ul << 0)
#define NVMAP_HANDLE_CACHEABLE       (0x3ul << 0)
#define NVMAP_HANDLE_CACHE_FLAG      (0x3ul << 0)
#define NVMAP_HANDLE_SECURE          (0x1ul << 2)

#define NVMAP_HEAP_SYSMEM  (1ul<<31)
#define NVMAP_HEAP_IOVMM   (1ul<<30)

/* common carveout heaps */
#define NVMAP_HEAP_CARVEOUT_IRAM    (1ul<<29)
#define NVMAP_HEAP_CARVEOUT_GENERIC (1ul<<0)

#define NVMAP_SHM_NAME_MAX_LENGTH          40
#define NVMAP_SHM_NAME_PREFIX              "/NVMAP-shm-"
#define NVMAP_SHM_FORMAT_STRING            NVMAP_SHM_NAME_PREFIX "%p"

struct nvmap_create_handle {
    union {
        rmos_u32 key;   /* ClaimPreservedHandle */
        rmos_u32 id;    /* FromId */
        rmos_u32 size;  /* CreateHandle */
    };
    rmos_u32 handle;
};

struct nvmap_alloc_handle {
    rmos_u32 handle;
    rmos_u32 heap_mask;
    rmos_u32 flags;
    rmos_u32 align;
    rmos_u8 kind;
};

struct nvmap_map_caller {
    rmos_u32 handle;    /* hmem */
    rmos_u32 offset;    /* offset into hmem; should be page-aligned */
    rmos_u32 length;    /* number of bytes to map */
    rmos_u32 flags;
    unsigned long addr; /* user pointer */
    pid_t pid;
    int prot;
};

struct nvmap_rw_handle {
    unsigned long addr;     /* user pointer */
    rmos_u32 handle;        /* hmem */
    rmos_u32 offset;        /* offset into hmem */
    rmos_u32 elem_size;     /* individual atom size */
    rmos_u32 hmem_stride;   /* delta in bytes between atoms in hmem */
    rmos_u32 user_stride;   /* delta in bytes between atoms in user */
    rmos_u32 count;         /* number of atoms to copy */
};

struct nvmap_pin_handle {
    unsigned long handles;  /* array of handles to pin/unpin */
    unsigned long addr;     /* array of addresses to return */
    rmos_u32 count;         /* number of entries in handles */
};

struct nvmap_handle_param {
    rmos_u32 handle;
    rmos_u32 param;
    unsigned long result;
};

struct nvmap_cache_op {
    rmos_u32 handle;
    rmos_u32 cache_flags;
};

/* Creates a new memory handle. On input, the argument is the size of the new
 * handle; on return, the argument is the name of the new handle
 */
#define NVMAP_IOC_CREATE  (int)__DIOTF(_DCMD_MISC, 0, struct nvmap_create_handle)
#define NVMAP_IOC_CLAIM   (int)__DIOTF(_DCMD_MISC, 1, struct nvmap_create_handle)
#define NVMAP_IOC_FROM_ID (int)__DIOTF(_DCMD_MISC, 2, struct nvmap_create_handle)

/* Actually allocates memory for the specified handle */
#define NVMAP_IOC_ALLOC    (int)__DIOT(_DCMD_MISC, 3, struct nvmap_alloc_handle)

/* Frees a memory handle, unpinning any pinned pages and unmapping any mappings
 */
#define NVMAP_IOC_FREE       (int)__DIOT(_DCMD_MISC, 4, unsigned)

/* Maps the region of the specified handle into a user-provided virtual address
 * that was previously created via an mmap syscall on this fd */
#define NVMAP_IOC_MMAP       (int)__DIOTF(_DCMD_MISC, 5, struct nvmap_map_caller)

/* Reads/writes data (possibly strided) from a user-provided buffer into the
 * hmem at the specified offset */
#define NVMAP_IOC_WRITE      (int)__DIOT(_DCMD_MISC, 6, struct nvmap_rw_handle)
#define NVMAP_IOC_READ       (int)__DIOTF(_DCMD_MISC, 7, struct nvmap_rw_handle)

#define NVMAP_IOC_PARAM (int)__DIOTF(_DCMD_MISC, 8, struct nvmap_handle_param)

/* Pins a list of memory handles into IO-addressable memory (either IOVMM
 * space or physical memory, depending on the allocation), and returns the
 * address. Handles may be pinned recursively. */
#define NVMAP_IOC_PIN_MULT   (int)__DIOTF(_DCMD_MISC, 9, struct nvmap_pin_handle)
#define NVMAP_IOC_UNPIN_MULT (int)__DIOT(_DCMD_MISC, 10, struct nvmap_pin_handle)

#define NVMAP_IOC_CACHE      (int)__DIOTF(_DCMD_MISC, 11, struct nvmap_cache_op)

#define NVMAP_IOC_GET_CLIENT (int)__DIOF(_DCMD_MISC, 12, unsigned long)

/* Returns a global ID usable to allow a remote process to create a handle
 * reference to the same handle */
#define NVMAP_IOC_GET_ID  (int)__DIOTF(_DCMD_MISC, 13, struct nvmap_create_handle)
#define NVMAP_IOC_MUNMAP     (int)__DIOT(_DCMD_MISC, 14, unsigned)

#define NVMAP_IOC_GET_FD  (int)__DIOT(_DCMD_MISC, 15, struct nvmap_create_handle)
#define NVMAP_IOC_FROM_FD (int)__DIOTF(_DCMD_MISC, 16, struct nvmap_create_handle)

#endif
