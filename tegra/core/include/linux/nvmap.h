/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ***   To edit the content of this header, modify the corresponding
 ***   source file (e.g. under external/kernel-headers/original/) then
 ***   run bionic/libc/kernel/tools/update_all.py
 ***
 ***   Any manual change here will be lost the next time this script will
 ***   be run. You've been warned!
 ***
 ****************************************************************************
 ****************************************************************************/
#include <linux/ioctl.h>
#include <linux/types.h>
#ifndef _LINUX_NVMAP_H
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define _LINUX_NVMAP_H
#define NVMAP_HEAP_SYSMEM (1ul<<31)
#define NVMAP_HEAP_IOVMM (1ul<<30)
#define NVMAP_HEAP_CARVEOUT_IRAM (1ul<<29)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_HEAP_CARVEOUT_VPR (1ul<<28)
#define NVMAP_HEAP_CARVEOUT_TSEC (1ul<<27)
#define NVMAP_HEAP_CARVEOUT_GENERIC (1ul<<0)
#define NVMAP_HEAP_CARVEOUT_MASK (NVMAP_HEAP_IOVMM - 1)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_HANDLE_UNCACHEABLE (0x0ul << 0)
#define NVMAP_HANDLE_WRITE_COMBINE (0x1ul << 0)
#define NVMAP_HANDLE_INNER_CACHEABLE (0x2ul << 0)
#define NVMAP_HANDLE_CACHEABLE (0x3ul << 0)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_HANDLE_CACHE_FLAG (0x3ul << 0)
#define NVMAP_HANDLE_SECURE (0x1ul << 2)
#define NVMAP_HANDLE_ZEROED_PAGES (0x1ul << 3)
struct nvmap_handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
enum {
 NVMAP_HANDLE_PARAM_SIZE = 1,
 NVMAP_HANDLE_PARAM_ALIGNMENT,
 NVMAP_HANDLE_PARAM_BASE,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 NVMAP_HANDLE_PARAM_HEAP,
};
enum {
 NVMAP_CACHE_OP_WB = 0,
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 NVMAP_CACHE_OP_INV,
 NVMAP_CACHE_OP_WB_INV,
};
struct nvmap_create_handle {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 union {
 unsigned long id;
 __u32 size;
 __s32 fd;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 };
 struct nvmap_handle *handle;
};
struct nvmap_alloc_handle {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 struct nvmap_handle *handle;
 __u32 heap_mask;
 __u32 flags;
 __u32 align;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct nvmap_map_caller {
 struct nvmap_handle *handle;
 __u32 offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 length;
 __u32 flags;
 unsigned long addr;
};
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
struct nvmap_rw_handle {
 unsigned long addr;
 struct nvmap_handle *handle;
 __u32 offset;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 elem_size;
 __u32 hmem_stride;
 __u32 user_stride;
 __u32 count;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
struct nvmap_pin_handle {
 struct nvmap_handle **handles;
 unsigned long *addr;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 count;
};
struct nvmap_handle_param {
 struct nvmap_handle *handle;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 __u32 param;
 unsigned long result;
};
struct nvmap_cache_op {
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
 unsigned long addr;
 struct nvmap_handle *handle;
 __u32 len;
 __s32 op;
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
};
#define NVMAP_IOC_MAGIC 'N'
#define NVMAP_IOC_CREATE _IOWR(NVMAP_IOC_MAGIC, 0, struct nvmap_create_handle)
#define NVMAP_IOC_CLAIM _IOWR(NVMAP_IOC_MAGIC, 1, struct nvmap_create_handle)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_IOC_FROM_ID _IOWR(NVMAP_IOC_MAGIC, 2, struct nvmap_create_handle)
#define NVMAP_IOC_ALLOC _IOW(NVMAP_IOC_MAGIC, 3, struct nvmap_alloc_handle)
#define NVMAP_IOC_FREE _IO(NVMAP_IOC_MAGIC, 4)
#define NVMAP_IOC_MMAP _IOWR(NVMAP_IOC_MAGIC, 5, struct nvmap_map_caller)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_IOC_WRITE _IOW(NVMAP_IOC_MAGIC, 6, struct nvmap_rw_handle)
#define NVMAP_IOC_READ _IOW(NVMAP_IOC_MAGIC, 7, struct nvmap_rw_handle)
#define NVMAP_IOC_PARAM _IOWR(NVMAP_IOC_MAGIC, 8, struct nvmap_handle_param)
#define NVMAP_IOC_PIN_MULT _IOWR(NVMAP_IOC_MAGIC, 10, struct nvmap_pin_handle)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_IOC_UNPIN_MULT _IOW(NVMAP_IOC_MAGIC, 11, struct nvmap_pin_handle)
#define NVMAP_IOC_CACHE _IOW(NVMAP_IOC_MAGIC, 12, struct nvmap_cache_op)
#define NVMAP_IOC_GET_ID _IOWR(NVMAP_IOC_MAGIC, 13, struct nvmap_create_handle)
#define NVMAP_IOC_SHARE _IOWR(NVMAP_IOC_MAGIC, 14, struct nvmap_create_handle)
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
#define NVMAP_IOC_GET_FD _IOWR(NVMAP_IOC_MAGIC, 15, struct nvmap_create_handle)
#define NVMAP_IOC_FROM_FD _IOWR(NVMAP_IOC_MAGIC, 16, struct nvmap_create_handle)
#define NVMAP_IOC_MAXNR (_IOC_NR(NVMAP_IOC_FROM_FD))
#endif
/* WARNING: DO NOT EDIT, AUTO-GENERATED CODE - SEE TOP FOR INSTRUCTIONS */
