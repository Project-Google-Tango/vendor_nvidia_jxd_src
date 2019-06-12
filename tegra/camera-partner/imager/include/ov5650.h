/**
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV5650_H__
#define __OV5650_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

/* IOCTL to set the operating resolution of camera */
#define OV5650_IOCTL_SET_MODE               _IOW('o', 1, struct ov5650_mode)
#define OV5650_IOCTL_SET_FRAME_LENGTH       _IOW('o', 2, __u32)
#define OV5650_IOCTL_SET_COARSE_TIME        _IOW('o', 3, __u32)
#define OV5650_IOCTL_SET_GAIN               _IOW('o', 4, __u16)
#define OV5650_IOCTL_GET_STATUS             _IOR('o', 5, __u8)
#define OV5650_IOCTL_SET_BINNING            _IOW('o', 6, __u8)
#define OV5650_IOCTL_TEST_PATTERN           _IOW('o', 7, enum ov5650_test_pattern)
#define OV5650_IOCTL_SET_GROUP_HOLD         _IOW('o', 8, struct ov5650_ae)
/* IOCTL to set the operating mode of camera.
 * This can be either stereo , leftOnly or rightOnly */
#define OV5650_IOCTL_SET_CAMERA_MODE        _IOW('o', 10, __u32)
#define OV5650_IOCTL_SYNC_SENSORS           _IOW('o', 11, __u32)
#define OV5650_IOCTL_GET_FUSEID         _IOR('o', 12, struct nvc_fuseid)

enum ov5650_test_pattern {
    TEST_PATTERN_NONE,
    TEST_PATTERN_COLORBARS,
    TEST_PATTERN_CHECKERBOARD
};

struct ov5650_mode {
    int xres;
    int yres;
    __u32 frame_length;
    __u32 coarse_time;
    __u16 gain;
};

struct ov5650_ae {
    __u32 frame_length;
    __u8 frame_length_enable;
    __u32 coarse_time;
    __u8 coarse_time_enable;
    __s32 gain;
    __u8 gain_enable;
};

#endif  /* __OV5650_H__ */
