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
#ifndef DRV_EEPROM_NVRAM_H
#define DRV_EEPROM_NVRAM_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/
#include <stdint.h>

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/

/*************************************************************************************************
 * Macros
 ************************************************************************************************/

#define RFM_PLAT_CFG_FILENAME        "rfmConfig.bin"                                   /** File containing RFM module platform config */
#define CAL_0_FILENAME               "calibration_0.bin"
#define CAL_1_FILENAME               "calibration_1.bin"

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
 * Read file in EEPROM to specified file
 *
 * @param device_path         Path to i2c device
 * @param filepath            File to write to

 * @return                    0 if ok, != 0 if failed
 */
int drv_eeprom_nvram_read(const char *device_path, const char * eeprom_filename, const char *fs_filename);

/**
 * Store file in EEPROM from specified file
 *
 * @param device_path         Path to i2c device
 * @param filepath            File to read from
 *
 * @return                    0 if ok, != 0 if failed
 */

int drv_eeprom_nvram_write(const char *device_path, const char *fs_filename, const char *eeprom_filename);

/**
 * Erase data stored in EEPROM.
 *
 * @param device_path         Path to i2c device
 *
 * @return                    0 if ok, != 0 if failed
 */
int drv_eeprom_nvram_erase(const char *device_path);

/**
 * Dump EEPROM contents to file
 *
 * @param device_path         Path to i2c device
 * @param filepath            File to write to

 * @return                    0 if ok, != 0 if failed
 */
int drv_eeprom_nvram_dump(const char *device_path, const char *fs_filename);

/**
 * Check if EEPROM contains valid file
 *
 * @param device_path         Path to i2c device

 * @return                    1 if it does, 0 if not/error
 */
int drv_eeprom_valid_contents(const char *device_path);

#endif /* DRV_EEPROM_NVRAM_H */

/** @} END OF FILE */

