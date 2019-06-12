/*
 * Tegra GPU Driver
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVHOST_GPU_DEVCTLS_H
#define __NVHOST_GPU_DEVCTLS_H

#include <qnx/rmos_types.h>
#include <devctl.h>

/*
 * /dev/nvhost-ctrl-gr3d devices
 *
 * Opening a '/dev/nvhost-ctrl-gr3d' device node creates a way to send
 * ctrl devctl to gpu driver.
 *
 * /dev/nvhost-gr3d is for channel (context specific) operations. We use
 * /dev/nvhost-ctrl-gr3d for global (context independent) operations on
 * gpu device.
 */

/* return zcull ctx size */
struct nvhost_gpu_zcull_get_ctx_size_args {
    rmos_u32 size;
};

/* return zcull info */
struct nvhost_gpu_zcull_get_info_args {
    rmos_u32 width_align_pixels;
    rmos_u32 height_align_pixels;
    rmos_u32 pixel_squares_by_aliquots;
    rmos_u32 aliquot_total;
    rmos_u32 region_byte_multiplier;
    rmos_u32 region_header_size;
    rmos_u32 subregion_header_size;
    rmos_u32 subregion_width_align_pixels;
    rmos_u32 subregion_height_align_pixels;
    rmos_u32 subregion_count;
};

#define NVHOST_ZBC_COLOR_VALUE_SIZE 4
#define NVHOST_ZBC_TYPE_INVALID     0
#define NVHOST_ZBC_TYPE_COLOR       1
#define NVHOST_ZBC_TYPE_DEPTH       2

struct nvhost_gpu_zbc_set_table_args {
    rmos_u32 color_ds[NVHOST_ZBC_COLOR_VALUE_SIZE];
    rmos_u32 color_l2[NVHOST_ZBC_COLOR_VALUE_SIZE];
    rmos_u32 depth;
    rmos_u32 format;
    rmos_u32 type; /* color or depth */
};
/* TBD: remove this once mobilerm removed old references */
#define nvhost_zbc_set_table_args nvhost_gpu_zbc_set_table_args

struct nvhost_gpu_zbc_query_table_args {
    rmos_u32 color_ds[NVHOST_ZBC_COLOR_VALUE_SIZE];
    rmos_u32 color_l2[NVHOST_ZBC_COLOR_VALUE_SIZE];
    rmos_u32 depth;
    rmos_u32 ref_cnt;
    rmos_u32 format;
    rmos_u32 type;         /* color or depth */
    rmos_u32 index_size;   /* [out] size, [in] index */
};

#define NVHOST_GPU_IOCTL_ZCULL_GET_CTX_SIZE \
    NVHOST_GPU_DEVCTL_ZCULL_GET_CTX_SIZE
#define NVHOST_GPU_IOCTL_ZCULL_GET_INFO     \
    NVHOST_GPU_DEVCTL_ZCULL_GET_INFO
#define NVHOST_GPU_IOCTL_ZBC_SET_TABLE      \
    NVHOST_GPU_DEVCTL_ZBC_SET_TABLE
#define NVHOST_GPU_IOCTL_ZBC_QUERY_TABLE    \
    NVHOST_GPU_DEVCTL_ZBC_QUERY_TABLE

#define NVHOST_GPU_DEVCTL_ZCULL_GET_CTX_SIZE    \
    (int)__DIOF(_DCMD_MISC, 1, struct nvhost_gpu_zcull_get_ctx_size_args)
#define NVHOST_GPU_DEVCTL_ZCULL_GET_INFO        \
    (int)__DIOF(_DCMD_MISC, 2, struct nvhost_gpu_zcull_get_info_args)
#define NVHOST_GPU_DEVCTL_ZBC_SET_TABLE         \
    (int)__DIOT(_DCMD_MISC, 3, struct nvhost_gpu_zbc_set_table_args)
#define NVHOST_GPU_DEVCTL_ZBC_QUERY_TABLE       \
    (int)__DIOTF(_DCMD_MISC, 4, struct nvhost_gpu_zbc_query_table_args)

#define NVHOST_GPU_DEVCTL_MAX_ARG_SIZE sizeof(struct nvhost_gpu_zbc_query_table_args)

#endif
