/**
 * Copyright (c) 2011-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __AR0833_H__
#define __AR0833_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define AR0833_IOCTL_SET_MODE               _IOW('o', 1, struct ar0833_mode)
#define AR0833_IOCTL_SET_FRAME_LENGTH       _IOW('o', 2, __u32)
#define AR0833_IOCTL_SET_COARSE_TIME        _IOW('o', 3, __u32)
#define AR0833_IOCTL_SET_GAIN               _IOW('o', 4, __u16)
#define AR0833_IOCTL_GET_STATUS             _IOR('o', 5, __u8)
#define AR0833_IOCTL_GET_MODE               _IOR('o', 6, struct ar0833_modeinfo)
#define AR0833_IOCTL_SET_HDR_COARSE_TIME    _IOW('o', 7, struct ar0833_hdr)
#define AR0833_IOCTL_SET_GROUP_HOLD         _IOW('o', 8, struct ar0833_ae)


struct ar0833_mode {
    int xres;
    int yres;
    __u32 frame_length;
    __u32 coarse_time;
    __u32 coarse_time_short;
    __u16 gain;
  __u8 hdr_en;
};

struct ar0833_hdr {
    __u32 coarse_time_long;
    __u32 coarse_time_short;
};

struct ar0833_ae {
    __u32 frame_length;
    __u8  frame_length_enable;
    __u32 coarse_time;
    __u32 coarse_time_short;
    __u8  coarse_time_enable;
    __s32 gain;
    __u8  gain_enable;
};

struct ar0833_sensordata {
    __u32 fuse_id_size;
    __u8  fuse_id[16];
};
#endif  /* __AR0833_H__ */

