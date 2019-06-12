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
#include "drv_m24m01.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/*************************************************************************************************
 * Private Macros
 ************************************************************************************************/
#define M24M01_WRITE_PAGE_SIZE    (256)
#define M24M01_READ_PAGE_SIZE     (256)

#define M24M01_DEV_ADDR           (0x50)
#define M24M01_DEV_ADDR_CE_SHIFT  (1)
#define M24M01_DEV_ADDR_CE_MASK   (0x03)

#define M24M01_DEV_ADDR_MASK      (0xFE)
#define M24M01_DEV_ADDR_A16_SHIFT (16-2)
#define M24M01_DEV_ADDR_A16_MASK  (0x01)

#define M24M01_WRITE_TIME_US      (5 * 1000)
#define M24M01_WRITE_POLL_RETRIES (3)         /* Arbitrary poll retries */

/* Mapping from os-abs to linux */
#define DEV_ASSERT(a)             assert(a)
#define os_TaskWait(a)            usleep(a)
#define COM_MIN(X,Y)              ((X) < (Y) ? (X) : (Y))

/*************************************************************************************************
 * Private type definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Private function declarations (only used if absolutely necessary)
 ************************************************************************************************/

/*************************************************************************************************
 * Private variable definitions
 ************************************************************************************************/

/*************************************************************************************************
 * Public variable definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Perform I2c data write and block until completion.
 *
 * @param dev_addr  I2c slave address
 * @param hdr       Pointer to data to transfer
 * @param hdr_length Number of bytes to send
 * @param data       Pointer to data to transfer
 * @param data_length Number of bytes to send
 * @return          0 if success, non-zero if error
 */
static int WriteI2CData(uint8_t dev_addr, uint8_t *hdr, int hdr_length, uint8_t *data, int data_length)
{
    uint8_t *ptr;
    uint8_t *buf;
    int err;
    int i;

    /* Allocate space for data */
    ptr = buf = malloc(data_length + hdr_length);
    DEV_ASSERT(ptr != NULL);

    for (i=0; i<hdr_length; i++)
    {
        *ptr++ = *hdr++;
    }

    for (i=0; i<data_length; i++)
    {
        *ptr++ = *data++;
    }

    /* Send request to I2c driver */
    err = drv_i2c_write(dev_addr, buf, data_length + hdr_length);

    free(buf);
    buf = NULL;

    return err;
}

/**
 * Perform I2c data read and block until completion.
 *
 * @param dev_addr  I2c slave address
 * @param data      Address to store read data to
 * @param length    Number of bytes to read
 * @return          0 if success, non-zero if error
 */
static int ReadI2CData(uint8_t dev_addr, uint8_t *data, int length)
{
    int err;

    DEV_ASSERT(data != NULL);

    /* Send request to I2c driver */
    err = drv_i2c_read(dev_addr, data, length);

    return err;
}

/**
 * Poll the device to see if it responds with a ACK (i.e. write cycle complete)
 *
 * @param handle    Handle to driver
 *
 * @return          0 if success, non-zero if device busy (NO ACK)
 */
static int PollEeprom(drv_M24M01Handle_t *handle)
{
    return drv_i2c_device_ready(handle->dev_addr) ? 0 : -1;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

/**** Function separator ************************************************************************/

/**
 * Create a handle to M24M01 driver.
 * @param device_path         Path to i2c device
 * @param chip_enable_addr    Chip Enable address (2 bits)
 *
 * @return                    Handle to driver if success, NULL if error
 */
drv_M24M01Handle_t * drv_M24M01Open(const char *device_path, uint8_t chip_enable_addr)
{
    drv_M24M01Handle_t *handle;
    int err;

    handle = (drv_M24M01Handle_t *)malloc(sizeof(drv_M24M01Handle_t));
    DEV_ASSERT( handle != NULL );

    /* Form device addresss */
    handle->dev_addr = M24M01_DEV_ADDR;
    handle->dev_addr|= ((chip_enable_addr & M24M01_DEV_ADDR_CE_MASK) << M24M01_DEV_ADDR_CE_SHIFT);

    /* Initialise I2C interface */
    err = drv_i2c_init(device_path);
    DEV_ASSERT(!err);

    return handle;
}

/**
 * Destroy a handle to M24M01 driver.
 */
void drv_M24M01Close(drv_M24M01Handle_t *handle)
{
    if (handle)
    {
        free(handle);
        handle = NULL;
    }
}

/**
 * Write a block of data to the EEPROM starting at the specified address
 * @param handle      Handle allocated by drv_M24M01Open()
 * @param address     Byte offset upto the size of the device
 * @param data        Buffer to write
 * @param length      Length in bytes to write
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_M24M01WriteBlock(drv_M24M01Handle_t *handle, uint32_t address, uint8_t *data, uint32_t length)
{
    int err = 0;
    uint8_t dev_addr;
    uint8_t addr_buf[2];
    int timeout = 0;
    uint32_t written = 0;
    uint32_t block_len;

    DEV_ASSERT(data != NULL && length > 0);
    DEV_ASSERT((address + length) <= M24M01_DEVICE_SIZE);

    do
    {
        /* Write upto one page at a time */
        block_len = COM_MIN(M24M01_WRITE_PAGE_SIZE, (length - written));

        /* Check for write in progress (device reports NO_ACK) */
        timeout = 0;
        while (PollEeprom(handle) != 0 && timeout < M24M01_WRITE_POLL_RETRIES)
        {
            os_TaskWait(M24M01_WRITE_TIME_US);
            timeout++;
        }

        if (timeout < M24M01_WRITE_POLL_RETRIES)
        {
            /* Form device address from device select code + A16 of address */
            dev_addr = handle->dev_addr & M24M01_DEV_ADDR_MASK;
            dev_addr+= ((address >> M24M01_DEV_ADDR_A16_SHIFT) & M24M01_DEV_ADDR_A16_MASK);
            addr_buf[0] = address >> 8;
            addr_buf[1] = address >> 0;

            /* Write page to device */
            err = WriteI2CData(dev_addr, addr_buf, sizeof(addr_buf), data, block_len);
            if (err)
            {
                break;
            }

            written += block_len;
            data += block_len;
            address += block_len;
        }
        else
        {
            /* Timeout waiting for previous write to complete (or device missing/failed) */
            break;
        }
    }
    while (written < length);

    return (written == length) ? 0 : -1;
}

/**
 * Read a block of data from the EEPROM starting at the specified address
 * @param handle      Handle allocated by drv_M24M01Open()
 * @param address     Byte offset upto the size of the device
 * @param data        Buffer to read into
 * @param length      Length in bytes to read
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_M24M01ReadBlock(drv_M24M01Handle_t *handle, uint32_t address, uint8_t *data, uint32_t length)
{
    int err;
    uint8_t dev_addr;
    uint8_t addr_buf[2] = { address >> 8, address >> 0 };
    int timeout = 0;

    DEV_ASSERT(data != NULL && length > 0);
    DEV_ASSERT((address + length) <= M24M01_DEVICE_SIZE);

    /* Check for write in progress (device reports NO_ACK) */
    while (PollEeprom(handle) != 0 && timeout < M24M01_WRITE_POLL_RETRIES)
    {
        os_TaskWait(M24M01_WRITE_TIME_US);
        timeout++;
    }

    if (timeout < M24M01_WRITE_POLL_RETRIES)
    {
        /* Form device address from device select code + A16 of address */
        dev_addr = handle->dev_addr & M24M01_DEV_ADDR_MASK;
        dev_addr+= ((address >> M24M01_DEV_ADDR_A16_SHIFT) & M24M01_DEV_ADDR_A16_MASK);

        /* Write address to device */
        err = WriteI2CData(dev_addr, addr_buf, sizeof(addr_buf), NULL, 0);
    }
    else
    {
        /* Timeout waiting for previous write to complete (or device missing/failed) */
        err = 1;
    }

    if (!err)
    {
        uint32_t bytes_read = 0;
        uint32_t block_len = 0;

        do
        {
            /* Read upto one page at a time */
            block_len = COM_MIN(M24M01_READ_PAGE_SIZE, (length - bytes_read));

            /* Now read data block */
            err = ReadI2CData (dev_addr, data, block_len);
            if (err)
            {
                break;
            }

            bytes_read += block_len;
            data += block_len;
        }
        while (bytes_read < length);
    }

    return err;
}
