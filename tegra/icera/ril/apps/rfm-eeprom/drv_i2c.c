/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 ************************************************************************************************/

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "drv_i2c.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#if !defined(ANDROID)
#include <linux/i2c-dev.h>
#endif

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/
static char i2c_device_path[FILENAME_MAX+1];

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

int drv_i2c_init(const char *device_path)
{
    strncpy(i2c_device_path, device_path, FILENAME_MAX);
    i2c_device_path[FILENAME_MAX] = '\0';

    return 0;
}

int drv_i2c_write(int device_address, unsigned char *data, int length)
{
    int status = -1;

    /* Open I2C device */
    int fd = open(i2c_device_path, O_RDWR);
    if (fd >=0)
    {
        /* Set device address */
        status = ioctl(fd, I2C_SLAVE, device_address);
        if (status >= 0)
        {
            /* Write I2C data */
            status = write(fd, data, length);
            status = (status == length) ? 0 : -1;
        }

        close(fd);
    }

    return status;
}

int drv_i2c_read(int device_address, unsigned char *data, int length)
{
    int status = -1;

    /* Open I2C device */
    int fd = open(i2c_device_path, O_RDWR);
    if (fd >=0)
    {
        /* Set device address */
        status = ioctl(fd, I2C_SLAVE, device_address);
        if (status >= 0)
        {
            /* Read I2C data */
            status = read(fd, data, length);
            status = (status == length) ? 0 : -1;
        }

        close(fd);
    }

    return status;
}

int drv_i2c_device_ready(int device_address)
{
    unsigned char tmp[1];

    /* Perform dummy read to see if it ACKs */
    return (drv_i2c_read(device_address, tmp, sizeof(tmp)) == 0) ? 1 : 0;
}
