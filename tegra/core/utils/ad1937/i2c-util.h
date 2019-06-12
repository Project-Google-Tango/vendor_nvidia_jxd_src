/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#ifndef _I2C_UTIL_H_
#define _I2C_UTIL_H_

#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>

#if defined(ANDROID)
/* This is the structure as used in the I2C_RDWR ioctl call */
struct i2c_rdwr_ioctl_data {
     struct i2c_msg *msgs; /* pointers to i2c_msgs */
     int nmsgs;      /* number of i2c_msgs */
};
#else
#include <linux/i2c-dev.h>
#endif

int i2c_write(int fd, unsigned int addr, unsigned int val);
int i2c_write_subaddr(int fd, unsigned int addr, unsigned int offset, unsigned char val);
int i2c_read(int fd, int addr, unsigned char *pval);
int i2c_read_subaddr(int fd, int addr, int offset, unsigned char *pval);

#endif
