/**
 * Copyright (c) 2011 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __OV9726_H__
#define __OV9726_H__

#include <linux/ioctl.h>

#define OV9726_I2C_ADDR         0x20

#define OV9726_IOCTL_SET_MODE           _IOW('o', 1, struct ov9726_mode)
#define OV9726_IOCTL_SET_FRAME_LENGTH   _IOW('o', 2, __u32)
#define OV9726_IOCTL_SET_COARSE_TIME    _IOW('o', 3, __u32)
#define OV9726_IOCTL_SET_GAIN           _IOW('o', 4, __u16)
#define OV9726_IOCTL_GET_STATUS         _IOR('o', 5, __u8)
#define OV9726_IOCTL_SET_GROUP_HOLD     _IOW('o', 6, struct ov9726_ae)
#define OV9726_IOCTL_GET_FUSEID         _IOR('o', 7, struct nvc_fuseid)

struct ov9726_mode {
    int mode_id;
    int xres;
    int yres;
    __u32 frame_length;
    __u32 coarse_time;
    __u16 gain;
};

struct ov9726_ae {
    __u32 frame_length;
    __u32 coarse_time;
    __u16 gain;
    __u8 frame_length_enable;
    __u8 coarse_time_enable;
    __u8 gain_enable;
};

struct ov9726_reg {
    __u16   addr;
    __u16   val;
};

#endif  /* __OV9726_H__ */

