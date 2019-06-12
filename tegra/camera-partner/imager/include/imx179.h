/**
 * Copyright (c) 2014 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __IMX179_H__
#define __IMX179_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define IMX179_IOCTL_SET_MODE           _IOW('o', 1, struct imx179_mode)
#define IMX179_IOCTL_GET_STATUS         _IOR('o', 2, __u8)
#define IMX179_IOCTL_SET_FRAME_LENGTH   _IOW('o', 3, __u32)
#define IMX179_IOCTL_SET_COARSE_TIME    _IOW('o', 4, __u32)
#define IMX179_IOCTL_SET_GAIN           _IOW('o', 5, __u16)
#define IMX179_IOCTL_GET_SENSORDATA     _IOR('o', 6, struct imx179_sensordata)
#define IMX179_IOCTL_SET_GROUP_HOLD     _IOW('o', 7, struct imx179_ae)
#define IMX179_IOCTL_SET_POWER          _IOW('o', 20, __u32)
#define IMX179_IOCTL_GET_FLASH_CAP      _IOR('o', 30, __u32)
#define IMX179_IOCTL_SET_FLASH_MODE     _IOW('o', 31, struct imx179_flash_control)

struct imx179_mode {
    int xres;
    int yres;
    __u32 frame_length;
    __u32 coarse_time;
    __u16 gain;
};

struct imx179_ae {
    __u32 frame_length;
    __u8  frame_length_enable;
    __u32 coarse_time;
    __u8  coarse_time_enable;
    __s32 gain;
    __u8  gain_enable;
};

struct imx179_sensordata {
    __u32 fuse_id_size;
    __u8  fuse_id[16];
};

struct imx179_flash_control {
    __u8 enable;
    __u8 edge_trig_en;
    __u8 start_edge;
    __u8 repeat;
    __u16 delay_frm;
};


#endif  /* __IMX179_H__ */
