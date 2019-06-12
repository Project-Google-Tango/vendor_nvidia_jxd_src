/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __DW9718_H__
#define __DW9718_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include "focuser_common.h"

#define DW9718_IOCTL_GET_CONFIG  _IOR('o', 1, struct nv_focuser_config)
#define DW9718_IOCTL_SET_CONFIG _IOW('o', 2, struct nv_focuser_config)
#define DW9718_IOCTL_SET_POSITION _IOW('o', 3, __u32)

#endif  /* __DW9718_H__ */

