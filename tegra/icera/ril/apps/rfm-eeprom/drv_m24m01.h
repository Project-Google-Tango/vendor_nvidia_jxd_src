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

/**
 * @addtogroup EepromDriver
 * @{
 */

/**
 * @file drv_m24m01.h ST M24M01 1MBit I2C EEPROM
 *
 */

#ifndef DRV_M24M01_H
#define DRV_M24M01_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdint.h>

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
#define M24M01_DEVICE_SIZE      (131072)
#define M24M01_PAGE_SIZE        (256)

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
typedef struct
{
    /**
     * Device slave address
     */
    uint8_t           dev_addr;

} drv_M24M01Handle_t;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 * Create a handle to M24M01 driver.
 * @param chip_enable_addr    Chip Enable address (2 bits)
 *
 * @return                    Handle to driver if success, NULL if error
 */
drv_M24M01Handle_t * drv_M24M01Open(const char *device_path, uint8_t chip_enable_addr);

/**
 * Destroy a handle to M24M01 driver.
 */
void drv_M24M01Close(drv_M24M01Handle_t *handle);

/**
 * Write a block of data to the EEPROM starting at the specified address
 * @param handle      Handle allocated by drv_M24M01Open()
 * @param address     Byte offset upto the size of the device
 * @param data        Buffer to write
 * @param length      Length in bytes to write
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_M24M01WriteBlock(drv_M24M01Handle_t *handle, uint32_t address, uint8_t *data, uint32_t length);

/**
 * Read a block of data from the EEPROM starting at the specified address
 * @param handle      Handle allocated by drv_M24M01Open()
 * @param address     Byte offset upto the size of the device
 * @param data        Buffer to read into
 * @param length      Length in bytes to read
 *
 * @return            0 if ok, != 0 if failed
 */
int drv_M24M01ReadBlock(drv_M24M01Handle_t *handle, uint32_t address, uint8_t *data, uint32_t length);

#endif /* DRV_M24M01_H */

/** @} END OF FILE */

