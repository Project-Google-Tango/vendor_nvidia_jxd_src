/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVHOST_DEVCTLS_H
#define __NVHOST_DEVCTLS_H

#include <qnx/rmos_types.h>
#include <devctl.h>

#define NVHOST_DEV_PREFIX           "/dev/nvhost-"
#define NVHOST_NO_TIMEOUT           (rmos_u32)-1
#define NVHOST_SUBMIT_VERSION_V0    0x0
#define NVHOST_SUBMIT_VERSION_V1    0x1
#define NVHOST_SUBMIT_VERSION_MAX_SUPPORTED NVHOST_SUBMIT_VERSION_V1

/* version 1 header (used with ioctl() submit interface) */
struct nvhost_submit_hdr_ext {
    rmos_u32 syncpt_id;         /* version 0 fields */
    rmos_u32 syncpt_incrs;
    rmos_u32 num_cmdbufs;
    rmos_u32 num_relocs;
    rmos_u32 submit_version;    /* version 1 fields */
    rmos_u32 num_waitchks;
    rmos_u32 waitchk_mask;
    rmos_u32 pad[5];            /* future expansion */
};

struct nvhost_cmdbuf {
    rmos_u32 mem;
    rmos_u32 offset;
    rmos_u32 words;
};

struct nvhost_reloc {
    rmos_u32 cmdbuf_mem;
    rmos_u32 cmdbuf_offset;
    rmos_u32 target;
    rmos_u32 target_offset;
};

struct nvhost_reloc_shift {
    rmos_u32 shift;
};

struct nvhost_waitchk {
    rmos_u32 mem;
    rmos_u32 offset;
    rmos_u32 syncpt_id;
    rmos_u32 thresh;
};

struct nvhost_get_param_arg {
    rmos_u32 param;
    rmos_u32 value;
};

struct nvhost_set_nvmap_fd_args {
    rmos_u32 fd;
};

struct nvhost_read_3d_reg_args {
    rmos_u32 offset;
    rmos_u32 value;
};

struct nvhost_gpfifo {
    rmos_u32 entry0; /* first word of gpfifo entry */
    rmos_u32 entry1; /* second word of gpfifo entry */
};

struct nvhost_alloc_obj_ctx_args {
    rmos_u32 class_num; /* kepler3d, 2d, compute, etc       */
    rmos_u32 padding;
    rmos_u64 obj_id;    /* output, used to free later       */
};

struct nvhost_free_obj_ctx_args {
    rmos_u64 obj_id; /* obj ctx to free */
};

struct nvhost_alloc_gpfifo_args {
    rmos_u32 num_entries;
#define NVHOST_ALLOC_GPFIFO_FLAGS_VPR_ENABLED   (1 << 0) /* set owner channel of this gpfifo as a vpr channel */
    rmos_u32 flags;
};

struct nvhost_fence {
    rmos_u32 syncpt_id; /* syncpoint id */
    rmos_u32 value;     /* syncpoint value to wait or for other to wait */
};

struct nvhost_submit_gpfifo_args {
    rmos_u64 gpfifo;
    rmos_u32 num_entries;
/* insert a wait on the fence before submitting gpfifo */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT   (1 << 0)
 /* insert a fence update after submitting gpfifo and
    return the new fence for other to wait on */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_GET    (1 << 1)
/* choose between different gpfifo entry format */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_HW_FORMAT    (1 << 2)
    rmos_u32 flags;
    struct nvhost_fence fence;
};

struct nvhost_wait_args {
#define NVHOST_WAIT_TYPE_NOTIFIER   0x0
#define NVHOST_WAIT_TYPE_SEMAPHORE  0x1
    rmos_u32 type;
    rmos_u32 timeout;
    union {
        struct {
            /* handle and offset for notifier memory */
            rmos_u32 nvmap_handle;
            rmos_u32 offset;
            rmos_u32 padding1;
            rmos_u32 padding2;
        } notifier;
        struct {
            /* handle and offset for semaphore memory */
            rmos_u32 nvmap_handle;
            rmos_u32 offset;
            /* semaphore payload to wait for */
            rmos_u32 payload;
            rmos_u32 padding;
        } semaphore;
    } condition; /* determined by type field */
};

/* cycle stats support */
struct nvhost_cycle_stats_args {
    rmos_u32 nvmap_handle;
};

#define NVHOST_ZCULL_MODE_GLOBAL    0
#define NVHOST_ZCULL_MODE_NO_CTXSW  1
#define NVHOST_ZCULL_MODE_SEPARATE_BUFFER       2
#define NVHOST_ZCULL_MODE_PART_OF_REGULAR_BUF   3

struct nvhost_zcull_bind_args {
    rmos_u64 gpu_va;
    rmos_u32 mode;
    rmos_u32 padding;
};

struct nvhost_set_error_notifier {
    rmos_u64 offset;
    rmos_u64 size;
    rmos_u32 mem;
    rmos_u32 padding;
};

#define NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD \
    NVHOST_DEVCTL_CHANNEL_SET_NVMAP_FD

#define NVHOST_DEVCTL_CHANNEL_FLUSH             \
    (int)__DIOTF(_DCMD_MISC, 1, struct nvhost_submit_hdr_ext)
#define NVHOST_DEVCTL_CHANNEL_GET_SYNCPOINT     \
    (int)__DIOTF(_DCMD_MISC, 2, struct nvhost_get_param_arg)
#define NVHOST_DEVCTL_CHANNEL_GET_WAITBASE      \
    (int)__DIOTF(_DCMD_MISC, 3, struct nvhost_get_param_arg)
#define NVHOST_DEVCTL_CHANNEL_GET_MODMUTEX      \
    (int)__DIOTF(_DCMD_MISC, 4, struct nvhost_get_param_arg)
#define NVHOST_DEVCTL_CHANNEL_SET_NVMAP_FD      \
    (int)__DIOT(_DCMD_MISC, 5, struct nvhost_set_nvmap_fd_args)
#define NVHOST_DEVCTL_CHANNEL_READ_3D_REG       \
    (int)__DIOTF(_DCMD_MISC, 6, struct nvhost_read_3d_reg_args)

/* START of T124 DEVCTLS */
#define NVHOST_IOCTL_CHANNEL_ALLOC_GPFIFO       \
    NVHOST_DEVCTL_CHANNEL_ALLOC_GPFIFO
#define NVHOST_IOCTL_CHANNEL_SUBMIT_GPFIFO      \
    NVHOST_DEVCTL_CHANNEL_SUBMIT_GPFIFO
#define NVHOST_IOCTL_CHANNEL_ALLOC_OBJ_CTX      \
    NVHOST_DEVCTL_CHANNEL_ALLOC_OBJ_CTX
#define NVHOST_IOCTL_CHANNEL_FREE_OBJ_CTX       \
    NVHOST_DEVCTL_CHANNEL_FREE_OBJ_CTX
#define NVHOST_IOCTL_CHANNEL_ZCULL_BIND         \
    NVHOST_DEVCTL_CHANNEL_ZCULL_BIND
#define NVHOST_IOCTL_CHANNEL_SET_ERROR_NOTIFIER \
    NVHOST_DEVCTL_CHANNEL_SET_ERROR_NOTIFIER

#define NVHOST_DEVCTL_CHANNEL_ALLOC_GPFIFO          \
    (int)__DIOT(_DCMD_MISC, 100, struct nvhost_alloc_gpfifo_args)
#define NVHOST_DEVCTL_CHANNEL_WAIT                  \
    (int)__DIOTF(_DCMD_MISC, 102, struct nvhost_wait_args)
#define NVHOST_DEVCTL_CHANNEL_CYCLE_STATS           \
    (int)__DIOTF(_DCMD_MISC, 106, struct nvhost_cycle_stats_args)
#define NVHOST_DEVCTL_CHANNEL_SUBMIT_GPFIFO         \
    (int)__DIOTF(_DCMD_MISC, 107, struct nvhost_submit_gpfifo_args)
#define NVHOST_DEVCTL_CHANNEL_ALLOC_OBJ_CTX         \
    (int)__DIOTF(_DCMD_MISC, 108, struct nvhost_alloc_obj_ctx_args)
#define NVHOST_DEVCTL_CHANNEL_FREE_OBJ_CTX          \
    (int)__DIOT(_DCMD_MISC, 109, struct nvhost_free_obj_ctx_args)
#define NVHOST_DEVCTL_CHANNEL_ZCULL_BIND            \
    (int)__DIOTF(_DCMD_MISC, 110, struct nvhost_zcull_bind_args)
#define NVHOST_DEVCTL_CHANNEL_SET_ERROR_NOTIFIER    \
    (int)__DIOTF(_DCMD_MISC, 111, struct nvhost_set_error_notifier)

#define NVHOST_DEVCTL_CHANNEL_MAX_ARG_SIZE sizeof(struct nvhost_submit_hdr_ext)

struct nvhost_ctrl_syncpt_read_args {
    rmos_u32 id;
    rmos_u32 value;
};

struct nvhost_ctrl_syncpt_incr_args {
    rmos_u32 id;
};

struct nvhost_ctrl_syncpt_wait_args {
    rmos_u32 id;
    rmos_u32 thresh;
    rmos_s32 timeout;
};

struct nvhost_ctrl_syncpt_waitex_args {
    rmos_u32 id;
    rmos_u32 thresh;
    rmos_s32 timeout;
    rmos_u32 value;
};

struct nvhost_ctrl_module_mutex_args {
    rmos_u32 id;
    rmos_u32 lock;
};

struct nvhost_ctrl_module_regrdwr_args {
    rmos_u32 id;
    rmos_u32 num_offsets;
    rmos_u32 block_size;
    rmos_u32 *offsets;
    rmos_u32 *values;
    rmos_u32 write;
};

#define NVHOST_IOCTL_CTRL_SYNCPT_READ   \
    NVHOST_DEVCTL_CTRL_SYNCPT_READ
#define NVHOST_IOCTL_CTRL_SYNCPT_WAITEX \
    NVHOST_DEVCTL_CTRL_SYNCPT_WAITEX

#define NVHOST_DEVCTL_CTRL_SYNCPT_READ      \
    (int)__DIOTF(_DCMD_MISC, 1, struct nvhost_ctrl_syncpt_read_args)
#define NVHOST_DEVCTL_CTRL_SYNCPT_INCR      \
    (int)__DIOT(_DCMD_MISC, 2, struct nvhost_ctrl_syncpt_incr_args)
#define NVHOST_DEVCTL_CTRL_SYNCPT_WAIT      \
    (int)__DIOT(_DCMD_MISC, 3, struct nvhost_ctrl_syncpt_wait_args)
#define NVHOST_DEVCTL_CTRL_MODULE_MUTEX     \
    (int)__DIOTF(_DCMD_MISC, 4, struct nvhost_ctrl_module_mutex_args)
#define NVHOST_DEVCTL_CTRL_MODULE_REGRDWR   \
    (int)__DIOTF(_DCMD_MISC, 5, struct nvhost_ctrl_module_regrdwr_args)
#define NVHOST_DEVCTL_CTRL_SYNCPT_WAITEX    \
    (int)__DIOTF(_DCMD_MISC, 6, struct nvhost_ctrl_syncpt_waitex_args)
#define NVHOST_DEVCTL_CTRL_SYNCPT_READ_MAX  \
    (int)__DIOTF(_DCMD_MISC, 7, struct nvhost_ctrl_syncpt_read_args)

#define NVHOST_DEVCTL_CTRL_MAX_ARG_SIZE sizeof(struct nvhost_ctrl_module_regrdwr_args)

#endif
