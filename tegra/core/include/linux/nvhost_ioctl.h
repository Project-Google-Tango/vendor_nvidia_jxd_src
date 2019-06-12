/*
 * include/linux/nvhost.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2009-2014, NVIDIA Corporation. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __LINUX_NVHOST_IOCTL_H
#define __LINUX_NVHOST_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#define NVHOST_INVALID_SYNCPOINT 0xFFFFFFFF
#define NVHOST_NO_TIMEOUT (-1)
#define NVHOST_NO_CONTEXT 0x0
#define NVHOST_IOCTL_MAGIC 'H'
#define NVHOST_PRIORITY_LOW 50
#define NVHOST_PRIORITY_MEDIUM 100
#define NVHOST_PRIORITY_HIGH 150

#define NVHOST_TIMEOUT_FLAG_DISABLE_DUMP	0

/* version 0 header (used with write() submit interface) */
struct nvhost_submit_hdr {
	__u32 syncpt_id;
	__u32 syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
};

#define NVHOST_SUBMIT_VERSION_V0		0x0
#define NVHOST_SUBMIT_VERSION_V1		0x1
#define NVHOST_SUBMIT_VERSION_V2		0x2
#define NVHOST_SUBMIT_VERSION_MAX_SUPPORTED	NVHOST_SUBMIT_VERSION_V2

/* version 1 header (used with ioctl() submit interface) */
struct nvhost_submit_hdr_ext {
	__u32 syncpt_id;	/* version 0 fields */
	__u32 syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 submit_version;	/* version 1 fields */
	__u32 num_waitchks;
	__u32 waitchk_mask;
	__u32 pad[5];		/* future expansion */
};

struct nvhost_cmdbuf {
	__u32 mem;
	__u32 offset;
	__u32 words;
} __packed;


/* Convert dmabuf fd to a memory id */
static inline __u32 nvhost_dmabuf_fd_to_mem(int fd)
{
	return ((unsigned int)fd) << 1 | 1;
}

/* Check if memory id is a dma_buf */
static inline __s32 nvhost_mem_is_dmabuf_fd(__u32 mem)
{
	return mem & 1;
}

/* Convert memory id to dmabuf fd */
static inline __s32 nvhost_mem_to_dmabuf_fd(__u32 mem)
{
	return mem >> 1;
}

struct nvhost_reloc {
	__u32 cmdbuf_mem;
	__u32 cmdbuf_offset;
	__u32 target;
	__u32 target_offset;
};

struct nvhost_reloc_shift {
	__u32 shift;
} __packed;

struct nvhost_waitchk {
	__u32 mem;
	__u32 offset;
	__u32 syncpt_id;
	__u32 thresh;
};

struct nvhost_gpfifo {
	__u32 entry0; /* first word of gpfifo entry */
	__u32 entry1; /* second word of gpfifo entry */
};

struct nvhost_syncpt_incr {
	__u32 syncpt_id;
	__u32 syncpt_incrs;
};

struct nvhost_get_param_args {
	__u32 value;
} __packed;

struct nvhost_get_param_arg {
	__u32 param;
	__u32 value;
};

struct nvhost_set_nvmap_fd_args {
	__u32 fd;
} __packed;

struct nvhost_alloc_obj_ctx_args {
	__u32 class_num; /* kepler3d, 2d, compute, etc       */
	__u32 padding;
	__u64 obj_id;    /* output, used to free later       */
};

struct nvhost_free_obj_ctx_args {
	__u64 obj_id; /* obj ctx to free */
};

struct nvhost_alloc_gpfifo_args {
	__u32 num_entries;
#define NVHOST_ALLOC_GPFIFO_FLAGS_VPR_ENABLED	(1 << 0) /* set owner channel of this gpfifo as a vpr channel */
	__u32 flags;
};

struct nvhost_fence {
	__u32 syncpt_id; /* syncpoint id */
	__u32 value;     /* syncpoint value to wait or for other to wait */
};

/* insert a wait on the fence before submitting gpfifo */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_WAIT	(1 << 0)
 /* insert a fence update after submitting gpfifo and
    return the new fence for other to wait on */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_FENCE_GET	(1 << 1)
/* choose between different gpfifo entry format */
#define NVHOST_SUBMIT_GPFIFO_FLAGS_HW_FORMAT	(1 << 2)

struct nvhost_submit_gpfifo_args {
	__u64 gpfifo;
	__u32 num_entries;
	__u32 flags;
	struct nvhost_fence fence;
};

struct nvhost_wait_args {
#define NVHOST_WAIT_TYPE_NOTIFIER	0x0
#define NVHOST_WAIT_TYPE_SEMAPHORE	0x1
	__u32 type;
	__u32 timeout;
	union {
		struct {
			/* handle and offset for notifier memory */
			__u32 nvmap_handle;
			__u32 offset;
			__u32 padding1;
			__u32 padding2;
		} notifier;
		struct {
			/* handle and offset for semaphore memory */
			__u32 nvmap_handle;
			__u32 offset;
			/* semaphore payload to wait for */
			__u32 payload;
			__u32 padding;
		} semaphore;
	} condition; /* determined by type field */
};

/* cycle stats support */
struct nvhost_cycle_stats_args {
	__u32 nvmap_handle;
} __packed;

struct nvhost_read_3d_reg_args {
	__u32 offset;
	__u32 value;
};

enum nvhost_clk_attr {
	NVHOST_CLOCK = 0,
	NVHOST_BW,
};

/*
 * moduleid[15:0]  => module id
 * moduleid[24:31] => nvhost_clk_attr
 */
#define NVHOST_MODULE_ID_BIT_POS	0
#define NVHOST_MODULE_ID_BIT_WIDTH	16
#define NVHOST_CLOCK_ATTR_BIT_POS	24
#define NVHOST_CLOCK_ATTR_BIT_WIDTH	8
struct nvhost_clk_rate_args {
	__u32 rate;
	__u32 moduleid;
};

struct nvhost_set_timeout_args {
	__u32 timeout;
} __packed;

struct nvhost_set_timeout_ex_args {
	__u32 timeout;
	__u32 flags;
};

struct nvhost_set_priority_args {
	__u32 priority;
} __packed;

#define NVHOST_ZCULL_MODE_GLOBAL		0
#define NVHOST_ZCULL_MODE_NO_CTXSW		1
#define NVHOST_ZCULL_MODE_SEPARATE_BUFFER	2
#define NVHOST_ZCULL_MODE_PART_OF_REGULAR_BUF	3

struct nvhost_zcull_bind_args {
	__u64 gpu_va;
	__u32 mode;
	__u32 padding;
};

struct nvhost_set_error_notifier {
	__u64 offset;
	__u64 size;
	__u32 mem;
	__u32 padding;
};

struct nvhost32_ctrl_module_regrdwr_args {
	__u32 id;
	__u32 num_offsets;
	__u32 block_size;
	__u32 offsets;
	__u32 values;
	__u32 write;
};

struct nvhost_ctrl_module_regrdwr_args {
	__u32 id;
	__u32 num_offsets;
	__u32 block_size;
	__u32 write;
	__u64 offsets;
	__u64 values;
};

struct nvhost32_submit_args {
	__u32 submit_version;
	__u32 num_syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 num_waitchks;
	__u32 timeout;
	__u32 syncpt_incrs;
	__u32 cmdbufs;
	__u32 relocs;
	__u32 reloc_shifts;
	__u32 waitchks;
	__u32 waitbases;
	__u32 class_ids;

	__u32 pad[2];		/* future expansion */

	__u32 fences;
	__u32 fence;		/* Return value */
} __packed;

struct nvhost_submit_args {
	__u32 submit_version;
	__u32 num_syncpt_incrs;
	__u32 num_cmdbufs;
	__u32 num_relocs;
	__u32 num_waitchks;
	__u32 timeout;
	__u32 syncpt_incrs;
	__u32 fence;		/* return value */

	__u64 pad[4];		/* future expansion */

	__u64 cmdbufs;
	__u64 relocs;
	__u64 reloc_shifts;
	__u64 waitchks;
	__u64 waitbases;
	__u64 class_ids;
	__u64 fences;
};

struct nvhost_gpfifo_hw {
	__u32 entry0; /* first word of gpfifo entry */
	__u32 entry1; /* second word of gpfifo entry */
};

struct nvhost_set_ctxswitch_args {
	__u32 num_cmdbufs_save;
	__u32 num_save_incrs;
	struct nvhost_syncpt_incr *save_incrs;
	__u32 *save_waitbases;
	struct nvhost_cmdbuf *cmdbuf_save;
	__u32 num_cmdbufs_restore;
	__u32 num_restore_incrs;
	struct nvhost_syncpt_incr *restore_incrs;
	__u32 *restore_waitbases;
	struct nvhost_cmdbuf *cmdbuf_restore;
	__u32 num_relocs;
	struct nvhost_reloc *relocs;
	struct nvhost_reloc_shift *reloc_shifts;

	__u32 pad;
};

#define NVHOST_IOCTL_CHANNEL_FLUSH		\
	_IOR(NVHOST_IOCTL_MAGIC, 1, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINTS	\
	_IOR(NVHOST_IOCTL_MAGIC, 2, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_WAITBASES	\
	_IOR(NVHOST_IOCTL_MAGIC, 3, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_GET_MODMUTEXES	\
	_IOR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_NVMAP_FD	\
	_IOW(NVHOST_IOCTL_MAGIC, 5, struct nvhost_set_nvmap_fd_args)
#define NVHOST_IOCTL_CHANNEL_NULL_KICKOFF	\
	_IOR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SUBMIT_EXT		\
	_IOW(NVHOST_IOCTL_MAGIC, 7, struct nvhost_submit_hdr_ext)
#define NVHOST_IOCTL_CHANNEL_READ_3D_REG \
	_IOWR(NVHOST_IOCTL_MAGIC, 8, struct nvhost_read_3d_reg_args)
#define NVHOST_IOCTL_CHANNEL_GET_CLK_RATE		\
	_IOR(NVHOST_IOCTL_MAGIC, 9, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_CLK_RATE		\
	_IOW(NVHOST_IOCTL_MAGIC, 10, struct nvhost_clk_rate_args)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT	\
	_IOW(NVHOST_IOCTL_MAGIC, 11, struct nvhost_set_timeout_args)
#define NVHOST_IOCTL_CHANNEL_GET_TIMEDOUT	\
	_IOR(NVHOST_IOCTL_MAGIC, 12, struct nvhost_get_param_args)
#define NVHOST_IOCTL_CHANNEL_SET_PRIORITY	\
	_IOW(NVHOST_IOCTL_MAGIC, 13, struct nvhost_set_priority_args)
#define	NVHOST32_IOCTL_CHANNEL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 14, struct nvhost32_ctrl_module_regrdwr_args)
#define NVHOST32_IOCTL_CHANNEL_SUBMIT		\
	_IOWR(NVHOST_IOCTL_MAGIC, 15, struct nvhost32_submit_args)
#define NVHOST_IOCTL_CHANNEL_GET_SYNCPOINT	\
	_IOWR(NVHOST_IOCTL_MAGIC, 16, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_GET_WAITBASE	\
	_IOWR(NVHOST_IOCTL_MAGIC, 17, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_SET_TIMEOUT_EX		\
	_IOWR(NVHOST_IOCTL_MAGIC, 18, struct nvhost_set_timeout_ex_args)
#define NVHOST_IOCTL_CHANNEL_GET_MODMUTEX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 23, struct nvhost_get_param_arg)
#define NVHOST_IOCTL_CHANNEL_SET_CTXSWITCH	\
	_IOWR(NVHOST_IOCTL_MAGIC, 25, struct nvhost_set_ctxswitch_args)

/* ioctls added for 64bit compatibility */
#define NVHOST_IOCTL_CHANNEL_SUBMIT	\
	_IOWR(NVHOST_IOCTL_MAGIC, 26, struct nvhost_submit_args)
#define	NVHOST_IOCTL_CHANNEL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 27, struct nvhost_ctrl_module_regrdwr_args)

/* START of T124 IOCTLS */
#define NVHOST_IOCTL_CHANNEL_ALLOC_GPFIFO	\
	_IOW(NVHOST_IOCTL_MAGIC,  100, struct nvhost_alloc_gpfifo_args)
#define NVHOST_IOCTL_CHANNEL_WAIT		\
	_IOWR(NVHOST_IOCTL_MAGIC, 102, struct nvhost_wait_args)
#define NVHOST_IOCTL_CHANNEL_CYCLE_STATS	\
	_IOWR(NVHOST_IOCTL_MAGIC, 106, struct nvhost_cycle_stats_args)
#define NVHOST_IOCTL_CHANNEL_SUBMIT_GPFIFO	\
	_IOWR(NVHOST_IOCTL_MAGIC, 107, struct nvhost_submit_gpfifo_args)
#define NVHOST_IOCTL_CHANNEL_ALLOC_OBJ_CTX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 108, struct nvhost_alloc_obj_ctx_args)
#define NVHOST_IOCTL_CHANNEL_FREE_OBJ_CTX	\
	_IOR(NVHOST_IOCTL_MAGIC,  109, struct nvhost_free_obj_ctx_args)
#define NVHOST_IOCTL_CHANNEL_ZCULL_BIND		\
	_IOWR(NVHOST_IOCTL_MAGIC, 110, struct nvhost_zcull_bind_args)
#define NVHOST_IOCTL_CHANNEL_SET_ERROR_NOTIFIER  \
	_IOWR(NVHOST_IOCTL_MAGIC, 111, struct nvhost_set_error_notifier)

#define NVHOST_IOCTL_CHANNEL_LAST	\
	_IOC_NR(NVHOST_IOCTL_CHANNEL_SET_ERROR_NOTIFIER)
#define NVHOST_IOCTL_CHANNEL_MAX_ARG_SIZE sizeof(struct nvhost_submit_args)

struct nvhost_ctrl_syncpt_read_args {
	__u32 id;
	__u32 value;
};

struct nvhost_ctrl_syncpt_incr_args {
	__u32 id;
} __packed;

struct nvhost_ctrl_syncpt_wait_args {
	__u32 id;
	__u32 thresh;
	__s32 timeout;
} __packed;

struct nvhost_ctrl_syncpt_waitex_args {
	__u32 id;
	__u32 thresh;
	__s32 timeout;
	__u32 value;
};

struct nvhost_ctrl_syncpt_waitmex_args {
	__u32 id;
	__u32 thresh;
	__s32 timeout;
	__u32 value;
	__u32 tv_sec;
	__u32 tv_nsec;
	__u32 reserved_1;
	__u32 reserved_2;
};

struct nvhost_ctrl_sync_fence_info {
	__u32 id;
	__u32 thresh;
};

struct nvhost32_ctrl_sync_fence_create_args {
	__u32 num_pts;
	__u64 pts; /* struct nvhost_ctrl_sync_fence_info* */
	__u64 name; /* const char* */
	__s32 fence_fd; /* fd of new fence */
};

struct nvhost_ctrl_sync_fence_create_args {
	__u32 num_pts;
	__s32 fence_fd; /* fd of new fence */
	__u64 pts; /* struct nvhost_ctrl_sync_fence_info* */
	__u64 name; /* const char* */
} __packed;

struct nvhost_ctrl_module_mutex_args {
	__u32 id;
	__u32 lock;
};

enum nvhost_module_id {
	NVHOST_MODULE_NONE = -1,
	NVHOST_MODULE_DISPLAY_A = 0,
	NVHOST_MODULE_DISPLAY_B,
	NVHOST_MODULE_VI,
	NVHOST_MODULE_ISP,
	NVHOST_MODULE_MPE,
	NVHOST_MODULE_MSENC,
	NVHOST_MODULE_TSEC,
};

#define NVHOST_IOCTL_CTRL_SYNCPT_READ		\
	_IOWR(NVHOST_IOCTL_MAGIC, 1, struct nvhost_ctrl_syncpt_read_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_INCR		\
	_IOW(NVHOST_IOCTL_MAGIC, 2, struct nvhost_ctrl_syncpt_incr_args)
#define NVHOST_IOCTL_CTRL_SYNCPT_WAIT		\
	_IOW(NVHOST_IOCTL_MAGIC, 3, struct nvhost_ctrl_syncpt_wait_args)

#define NVHOST_IOCTL_CTRL_MODULE_MUTEX		\
	_IOWR(NVHOST_IOCTL_MAGIC, 4, struct nvhost_ctrl_module_mutex_args)

#define NVHOST32_IOCTL_CTRL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 5, struct nvhost32_ctrl_module_regrdwr_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_WAITEX		\
	_IOWR(NVHOST_IOCTL_MAGIC, 6, struct nvhost_ctrl_syncpt_waitex_args)

#define NVHOST_IOCTL_CTRL_GET_VERSION	\
	_IOR(NVHOST_IOCTL_MAGIC, 7, struct nvhost_get_param_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_READ_MAX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 8, struct nvhost_ctrl_syncpt_read_args)

#define NVHOST_IOCTL_CTRL_SYNCPT_WAITMEX	\
	_IOWR(NVHOST_IOCTL_MAGIC, 9, struct nvhost_ctrl_syncpt_waitmex_args)

#define NVHOST32_IOCTL_CTRL_SYNC_FENCE_CREATE	\
	_IOWR(NVHOST_IOCTL_MAGIC, 10, struct nvhost32_ctrl_sync_fence_create_args)

#define NVHOST_IOCTL_CTRL_SYNC_FENCE_CREATE	\
	_IOWR(NVHOST_IOCTL_MAGIC, 11, struct nvhost_ctrl_sync_fence_create_args)

#define NVHOST_IOCTL_CTRL_MODULE_REGRDWR	\
	_IOWR(NVHOST_IOCTL_MAGIC, 12, struct nvhost_ctrl_module_regrdwr_args)

#define NVHOST_IOCTL_CTRL_LAST			\
	_IOC_NR(NVHOST_IOCTL_CTRL_MODULE_REGRDWR)
#define NVHOST_IOCTL_CTRL_MAX_ARG_SIZE	\
	sizeof(struct nvhost_ctrl_module_regrdwr_args)

#endif
