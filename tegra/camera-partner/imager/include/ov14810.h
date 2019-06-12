/**
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV14810_H__
#define __OV14810_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

/* IOCTL to set the operating resolution of camera */
#define OV14810_IOCTL_SET_MODE               _IOW('o', 1, struct ov14810_mode)
#define OV14810_IOCTL_SET_FRAME_LENGTH       _IOW('o', 2, __u32)
#define OV14810_IOCTL_SET_COARSE_TIME        _IOW('o', 3, __u32)
#define OV14810_IOCTL_SET_GAIN               _IOW('o', 4, __u16)
#define OV14810_IOCTL_GET_STATUS             _IOR('o', 5, __u8)
/* IOCTL to set the operating mode of camera.
 * This can be either stereo , leftOnly or rightOnly */
#define OV14810_IOCTL_SET_CAMERA_MODE        _IOW('o', 10, __u32)
#define OV14810_IOCTL_SYNC_SENSORS           _IOW('o', 11, __u32)

enum ov14810_test_pattern {
    TEST_PATTERN_NONE,
    TEST_PATTERN_COLORBARS,
    TEST_PATTERN_CHECKERBOARD
};


struct ov14810_mode {
	int xres;
	int yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u16 gain;
};

#endif  /* __OV14810_H__ */
