/**
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef TEGRA_CAMERA_H
#define TEGRA_CAMERA_H

/* this is to enable VI pattern generator (Null Sensor) */
#define TEGRA_CAMERA_ENABLE_PD2VI_CLK 0x1

enum {
   TEGRA_CAMERA_MODULE_ISP = 0,
   TEGRA_CAMERA_MODULE_VI,
   TEGRA_CAMERA_MODULE_CSI,
   TEGRA_CAMERA_MODULE_EMC,
   TEGRA_CAMERA_MODULE_MAX
};

enum {
   TEGRA_CAMERA_VI_CLK,
   TEGRA_CAMERA_VI_SENSOR_CLK,
   TEGRA_CAMERA_EMC_CLK
};

struct tegra_camera_clk_info {
   unsigned int id;
   unsigned int clk_id;
   unsigned long rate;
   unsigned int flag; /* to inform if any special bits need to enabled/disabled */
};

enum StereoCameraMode {
    Main = 0x0,             /* Sets the default camera to Main */
    StereoCameraMode_Left = 0x01,        /* Only the sensor on the left is on.  */
    StereoCameraMode_Right = 0x02,       /* Only the sensor on the right is on. */
    StereoCameraMode_Stereo = 0x03,          /* Sets the stereo camera to stereo mode. */
    StereoCameraMode_Force32 = 0x7FFFFFFF
};


#define TEGRA_CAMERA_IOCTL_ENABLE               _IOWR('i', 1, uint)
#define TEGRA_CAMERA_IOCTL_DISABLE              _IOWR('i', 2, uint)
#define TEGRA_CAMERA_IOCTL_CLK_SET_RATE         \
    _IOWR('i', 3, struct tegra_camera_clk_info)
#define TEGRA_CAMERA_IOCTL_RESET                _IOWR('i', 4, uint)

#endif
