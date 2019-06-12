/**
 * Copyright (c) 2011 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __SH532U_H__
#define __SH532U_H__

#if (BUILD_FOR_AOS == 0)
#include <linux/ioctl.h>  /* For IOCTL macros */
#endif

#define SH532U_IOCTL_GET_CONFIG		_IOR('o', 1, struct sh532u_config)
#define SH532U_IOCTL_SET_POSITION 	_IOW('o', 2, uint32_t)
#define SH532U_IOCTL_GET_MOVE_STATUS	_IOW('o', 3, unsigned char)
#define SH532U_IOCTL_SET_CAMERA_MODE	_IOW('o', 4, unsigned char)

enum sh532u_move_status {
 SH532U_STATE_UNKNOWN = 1,
 SH532U_WAIT_FOR_MOVE_END,
 SH532U_WAIT_FOR_SETTLE,
 SH532U_LENS_SETTLED,
 SH532U_Forced32 = 0x7FFFFFFF
};

struct sh532u_config {
 unsigned int settle_time;
 float focal_length;
 float fnumber;
 short pos_low;
 short pos_high;
 short limit_low;
 short limit_high;
};

#endif
