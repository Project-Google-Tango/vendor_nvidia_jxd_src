/*
 * tegra/core/include/linux/nvavp_ioctl.h
 *
 * Declarations for nvavp driver ioctls
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#ifndef __NVAVP_IOCTL_H
#define __NVAVP_IOCTL_H

#define NVAVP_MAX_RELOCATION_COUNT 64

/* avp submit flags */
#define NVAVP_FLAG_NONE            0x00000000
#define NVAVP_UCODE_EXT            0x00000001 /*use external ucode provided */

struct nvavp_cmdbuf {
        __u32 mem;
        __u32 offset;
        __u32 words;
};

struct nvavp_reloc {
        __u32 cmdbuf_mem;
        __u32 cmdbuf_offset;
        __u32 target;
        __u32 target_offset;
};

struct nvavp_syncpt {
        __u32 id;
        __u32 value;
};

struct nvavp_pushbuffer_submit_hdr {
        struct nvavp_cmdbuf       cmdbuf;
        struct nvavp_reloc        *relocs;
        __u32                     num_relocs;
        struct nvavp_syncpt       *syncpt;
        __u32                     flags;
};

struct nvavp_set_nvmap_fd_args {
        __u32 fd;
};

struct nvavp_clock_args {
        __u32 id;
        __u32 rate;
};

enum nvavp_clock_stay_on_state {
        NVAVP_CLOCK_STAY_ON_DISABLED = 0,
        NVAVP_CLOCK_STAY_ON_ENABLED
};

struct nvavp_clock_stay_on_state_args {
        enum nvavp_clock_stay_on_state       state;
};

struct nvavp_num_cpus_args {
        __u32 min_online_cpus;
};

struct nvavp_map_args {
        __s32 mem;
        __u32 addr;
};

#define NVAVP_IOCTL_MAGIC              'n'

#define NVAVP_IOCTL_SET_NVMAP_FD       _IOW(NVAVP_IOCTL_MAGIC, 0x60, struct nvavp_set_nvmap_fd_args)
#define NVAVP_IOCTL_GET_SYNCPOINT_ID   _IOR(NVAVP_IOCTL_MAGIC, 0x61, __u32)
#define NVAVP_IOCTL_PUSH_BUFFER_SUBMIT _IOWR(NVAVP_IOCTL_MAGIC, 0x63, struct nvavp_pushbuffer_submit_hdr)
#define NVAVP_IOCTL_SET_CLOCK          _IOWR(NVAVP_IOCTL_MAGIC, 0x64, struct nvavp_clock_args)
#define NVAVP_IOCTL_GET_CLOCK          _IOR(NVAVP_IOCTL_MAGIC, 0x65, struct nvavp_clock_args)
#define NVAVP_IOCTL_WAKE_AVP           _IOR(NVAVP_IOCTL_MAGIC, 0x66, __u32)
#define NVAVP_IOCTL_FORCE_CLOCK_STAY_ON    _IOW(NVAVP_IOCTL_MAGIC, 0x67, struct nvavp_clock_stay_on_state_args)
#define NVAVP_IOCTL_ENABLE_AUDIO_CLOCKS    _IOWR(NVAVP_IOCTL_MAGIC, 0x68, struct nvavp_clock_args)
#define NVAVP_IOCTL_DISABLE_AUDIO_CLOCKS   _IOWR(NVAVP_IOCTL_MAGIC, 0x69, struct nvavp_clock_args)
#define NVAVP_IOCTL_SET_MIN_ONLINE_CPUS   _IOWR(NVAVP_IOCTL_MAGIC, 0x70, struct nvavp_num_cpus_args)
#define NVAVP_IOCTL_MAP_IOVA           _IOWR(NVAVP_IOCTL_MAGIC, 0x71, struct nvavp_map_args)
#define NVAVP_IOCTL_UNMAP_IOVA         _IOW(NVAVP_IOCTL_MAGIC, 0x72, struct nvavp_map_args)

#define NVAVP_IOCTL_MIN_NR             _IOC_NR(NVAVP_IOCTL_SET_NVMAP_FD)
#define NVAVP_IOCTL_MAX_NR             _IOC_NR(NVAVP_IOCTL_UNMAP_IOVA)

#endif
