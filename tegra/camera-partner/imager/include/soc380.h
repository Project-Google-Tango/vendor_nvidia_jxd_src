/**
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __SOC380_H__
#define __SOC380_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define SOC380_IOCTL_SET_MODE		_IOW('o', 1, struct soc380_mode)
#define SOC380_IOCTL_GET_STATUS		_IOR('o', 2, struct soc380_status)

struct soc380_mode {
	int xres;
	int yres;
};

struct soc380_status {
	int data;
	int status;
};
#endif  /* __SOC380_H__ */

