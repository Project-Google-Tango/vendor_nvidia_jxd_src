/*
 * include/linux/nvhost_vi_ioctl.h
 *
 * Tegra VI driver
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __LINUX_NVHOST_VI_IOCTL_H
#define __LINUX_NVHOST_VI_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#if !defined(__KERNEL__)
#define __user
#endif

#define NVHOST_VI_IOCTL_MAGIC 'V'

/*
 * /dev/nvhost-ctrl-vi devices
 *
 * Opening a '/dev/nvhost-ctrl-vi' device node creates a way to send
 * ctrl ioctl to vi driver.
 *
 * /dev/nvhost-vi is for channel (context specific) operations. We use
 * /dev/nvhost-ctrl-vi for global (context independent) operations on
 * vi device.
 */

#define NVHOST_VI_IOCTL_ENABLE_TPG _IOW(NVHOST_VI_IOCTL_MAGIC, 1, uint)
#define NVHOST_VI_IOCTL_SET_EMC_INFO _IOW(NVHOST_VI_IOCTL_MAGIC, 2, uint)

#endif
