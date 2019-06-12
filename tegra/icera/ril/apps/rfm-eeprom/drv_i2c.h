/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 ************************************************************************************************/
#ifndef DRV_I2C_H
#define DRV_I2C_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 * Initialise I2C driver
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_i2c_init(const char *device_path);

/**
 * Write a block of data over I2C
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_i2c_write(int device_address, unsigned char *data, int length);

/**
 * Read a block of data over I2C
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_i2c_read(int device_address, unsigned char *data, int length);

/**
 * Poll device to see if it responds with ACK
 *
 * @return            1 if ACK, 0 if no ACK
 */
int drv_i2c_device_ready(int device_address);

#endif
