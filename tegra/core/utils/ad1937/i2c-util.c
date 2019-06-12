/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "i2c-util.h"
#define I2C_M_WR        0

int i2c_write(int fd, unsigned int addr, unsigned int val)
{
    struct i2c_rdwr_ioctl_data msg_rdwr;
    struct i2c_msg i2cmsg;
    int i;
    unsigned char _buf[20];

    _buf[0] = val;

    msg_rdwr.msgs = &i2cmsg;
    msg_rdwr.nmsgs = 1;

    i2cmsg.addr = addr;
    i2cmsg.flags = 0;
    i2cmsg.len = 1;
    i2cmsg.buf = _buf;
    if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0) {
        return -1;
    }
    return 0;
}


int i2c_write_subaddr(int fd, unsigned int addr, unsigned int offset, unsigned char val)
{
    struct i2c_rdwr_ioctl_data msg_rdwr;
    struct i2c_msg i2cmsg;
    int i;
    unsigned char _buf[20];

    _buf[0] = offset;
    _buf[1] = val;

    msg_rdwr.msgs = &i2cmsg;
    msg_rdwr.nmsgs = 1;

    i2cmsg.addr = addr;
    i2cmsg.flags = 0;
    i2cmsg.len = 2;
    i2cmsg.buf = _buf;

    if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0) {
        return -1;
    }
    return 0;
}

int i2c_read(int fd, int addr, unsigned char *pval)
{
     struct i2c_rdwr_ioctl_data msg_rdwr;
     struct i2c_msg i2cmsg[2];
     int i;
     unsigned char buf[2];

     msg_rdwr.msgs = i2cmsg;
     msg_rdwr.nmsgs = 2;
     buf[0]=0;
     i2cmsg[0].addr = addr;
     i2cmsg[0].flags = I2C_M_WR | I2C_M_NOSTART;
     i2cmsg[0].len = 1;
     i2cmsg[0].buf = (__u8*) &buf;
     i2cmsg[1].addr = addr;
     i2cmsg[1].flags = I2C_M_RD;
     i2cmsg[1].len = 1;
     i2cmsg[1].buf = (__u8*) &buf;

     if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0) {
          return -1;
     }
     *pval = buf[0] & 0xff;
     return 0;
}

int i2c_read_subaddr(int fd, int addr, int offset, unsigned char *pval)
{
     struct i2c_rdwr_ioctl_data msg_rdwr;
     struct i2c_msg i2cmsg[2];
     int i;
     unsigned char buf[2];

     msg_rdwr.msgs = i2cmsg;
     msg_rdwr.nmsgs = 2;
     buf[0]=offset;
     i2cmsg[0].addr = addr;
     i2cmsg[0].flags = I2C_M_WR | I2C_M_NOSTART;
     i2cmsg[0].len = 1;
     i2cmsg[0].buf = (__u8*)&buf;

     i2cmsg[1].addr = addr;
     i2cmsg[1].flags = I2C_M_RD;
     i2cmsg[1].len = 1;
     i2cmsg[1].buf = (__u8*)&buf;

     if ((i = ioctl(fd, I2C_RDWR, &msg_rdwr)) < 0) {
         return -1;
     }
     *pval = buf[0] & 0xff;
     return 0;
}

