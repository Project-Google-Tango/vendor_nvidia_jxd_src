/**
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV5693_H__
#define __OV5693_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

/* IOCTL to set the operating resolution of camera */
#define OV5693_IOCTL_SET_MODE               _IOW('o', 1, struct ov5693_mode)
#define OV5693_IOCTL_SET_FRAME_LENGTH       _IOW('o', 2, __u32)
#define OV5693_IOCTL_SET_COARSE_TIME        _IOW('o', 3, __u32)
#define OV5693_IOCTL_SET_GAIN               _IOW('o', 4, __u16)
#define OV5693_IOCTL_GET_STATUS             _IOR('o', 5, __u8)
#define OV5693_IOCTL_SET_BINNING            _IOW('o', 6, __u8)
#define OV5693_IOCTL_TEST_PATTERN           _IOW('o', 7, enum ov5693_test_pattern)
#define OV5693_IOCTL_SET_GROUP_HOLD         _IOW('o', 8, struct ov5693_ae)
/* IOCTL to set the operating mode of camera.
 * This can be either stereo , leftOnly or rightOnly */
#define OV5693_IOCTL_SET_CAMERA_MODE        _IOW('o', 10, __u32)
#define OV5693_IOCTL_SYNC_SENSORS           _IOW('o', 11, __u32)
#define OV5693_IOCTL_GET_FUSEID             _IOR('o', 12, struct nvc_fuseid)
#define OV5693_IOCTL_SET_HDR_COARSE_TIME    _IOW('o', 13, struct ov5693_hdr)
#define OV5693_IOCTL_GET_EEPROM_DATA        _IOR('o', 20, __u8 *)
#define OV5693_IOCTL_GET_CAPS               _IOR('o', 22, struct nvc_imager_cap)
#define OV5693_IOCTL_SET_POWER              _IOW('o', 23, __u32)


#define OV5693_INVALID_COARSE_TIME    -1
enum ov5693_test_pattern {
    TEST_PATTERN_NONE,
    TEST_PATTERN_COLORBARS,
    TEST_PATTERN_CHECKERBOARD
};

struct ov5693_mode {
    int xres;
    int yres;
    int fps;
    __u32 frame_length;
    __u32 coarse_time;
    __u32 coarse_time_short;
    __u16 gain;
    __u8 hdr_en;
};

struct ov5693_ae {
    __u32 frame_length;
    __u8  frame_length_enable;
    __u32 coarse_time;
    __u32 coarse_time_short;
    __u8  coarse_time_enable;
    __s32 gain;
    __u8  gain_enable;
};

struct ov5693_hdr {
    __u32 coarse_time_long;
    __u32 coarse_time_short;
};

#endif  /* __OV5693_H__ */
