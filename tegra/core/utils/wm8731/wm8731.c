/*
 * Copyright (c) 2012, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <string.h>
#include <pthread.h>
#include "nvos.h"

#include "wm8731.h"

#define I2C_DEVICE "/dev/i2c-0"
#define I2C_M_WR        0

static int i2c_write_subaddr(int fd, unsigned int addr, unsigned int offset, unsigned char val)
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

int
CodecConfigure(int clockMode,
               int sampleRate)
{
    int fd;
    int err;

    fd = open(I2C_DEVICE, O_RDWR);

    if(fd < 0) {
        fprintf(stderr, "%s() Error: Could not open I2C device %s.\n", __func__, I2C_DEVICE);
        return 1;
    }

    // Reset device
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_RESET << 1, 0);

    // Digital Audio Interface Format
    if (clockMode == 1) {
        // master mode
        err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_IFACE << 1, 0x52);
    }
    else {
        // slave mode
        err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_IFACE << 1, 0x12);
    }

    // Sampling Control
    if (sampleRate == 44) {
        err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_SRATE << 1, 0x20);
    }
    else {
        // 48
        err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_SRATE << 1, 0x00);
    }

    // Analog Audio Path Control : DAC, enable mic mute
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_APANA << 1, 0x12);
    // Left Line In
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_LINVOL << 1, 0x17);
    // Right Line In
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_RINVOL << 1, 0x17);

    // Digital Audio Path Control : Disable DAC soft mute
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_APDIGI << 1, 0x04);

    // Power Down Control : power down for micro phone and CLKOUT
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_PWR << 1, 0x42);

    // Active Control
    err = i2c_write_subaddr(fd, WM8731_I2C_ADDR, WM8731_ACTIVE << 1, 0x01);

    //ReadCodec(fd);

    return 0;
}
