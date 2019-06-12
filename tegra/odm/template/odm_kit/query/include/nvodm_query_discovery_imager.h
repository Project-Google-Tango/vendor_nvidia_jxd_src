/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVODM_QUERY_DISCOVERY_IMAGER_H
#define NVODM_QUERY_DISCOVERY_IMAGER_H


#include "nvodm_query_discovery.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// GUIDS
#define SENSOR_BAYER_GUID           NV_ODM_GUID('s', '_', 'S', 'M', 'P', 'B', 'A', 'Y')
#define SENSOR_YUV_GUID             NV_ODM_GUID('s', '_', 'S', 'M', 'P', 'Y', 'U', 'V')
#define SENSOR_ITU656_GUID          NV_ODM_GUID('s', '_', 'I', 'T', 'U', '6', '5', '6')
#define FOCUSER_GUID                NV_ODM_GUID('f', '_', 'S', 'M', 'P', 'F', 'O', 'C')
#define FLASH_GUID                  NV_ODM_GUID('f', '_', 'S', 'M', 'P', 'F', 'L', 'A')

// for specific sensors
#define SENSOR_BYRRI_AR0832_GUID    NV_ODM_GUID('s', 'R', 'A', 'R', '0', '8', '3', '2')
#define SENSOR_BYRLE_AR0832_GUID    NV_ODM_GUID('s', 'L', 'A', 'R', '0', '8', '3', '2')
#define SENSOR_BYRST_AR0832_GUID    NV_ODM_GUID('s', 't', 'A', 'R', '0', '8', '3', '2')
#define SENSOR_BAYER_OV5630_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '5', '6', '3', '0')
#define SENSOR_BAYER_OV5650_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '5', '6', '5', '0')
#define SENSOR_BYRST_OV5630_GUID    NV_ODM_GUID('s', 't', 'O', 'V', '5', '6', '3', '0')
#define SENSOR_BYRST_OV5650_GUID    NV_ODM_GUID('s', 't', 'O', 'V', '5', '6', '5', '0')
#define SENSOR_BAYER_OV9726_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '9', '7', '2', '6')
#define SENSOR_BAYER_OV2710_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '2', '7', '1', '0')
#define SENSOR_YUV_OV5640_GUID      NV_ODM_GUID('s', '_', 'O', 'V', '5', '6', '4', '0')
#define SENSOR_YUV_OV7695_GUID      NV_ODM_GUID('s', '_', 'O', 'V', '7', '6', '9', '5')
#define SENSOR_YUV_SOC380_GUID      NV_ODM_GUID('s', '_', 'S', 'O', 'C', '3', '8', '0')
#define SENSOR_BAYER_OV14810_GUID   NV_ODM_GUID('s', 'O', 'V', '1', '4', '8', '1', '0')
#define SENSOR_BAYER_IMX091_GUID    NV_ODM_GUID('s', '_', 'I', 'M', 'X', '0', '9', '1')
#define SENSOR_BAYER_IMX135_GUID    NV_ODM_GUID('s', '_', 'I', 'M', 'X', '1', '3', '5')
#define SENSOR_BAYER_OV9772_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '9', '7', '7', '2')
#define SENSOR_BAYER_IMX132_GUID    NV_ODM_GUID('s', '_', 'I', 'M', 'X', '1', '3', '2')
#define SENSOR_BAYER_AR0833_GUID    NV_ODM_GUID('s', '_', 'A', 'R', '0', '8', '3', '3')
#define SENSOR_BAYER_AR0261_GUID    NV_ODM_GUID('s', '_', 'A', 'R', '0', '2', '6', '1')
#define SENSOR_BAYER_OV5693_GUID    NV_ODM_GUID('s', '_', 'O', 'V', '5', '6', '9', '3')
#define SENSOR_BAYER_OV5693_FRONT_GUID    NV_ODM_GUID('s', 'O', 'V', '5', '6', '9', '3', 'f')
#define SENSOR_YUV_MT9M114_GUID     NV_ODM_GUID('s', 'M', 'T', '9', 'M', '1', '1', '4')

/* The IMAGER_NVC_ defines are for NVC drivers; the kernel drivers that use the
 * NVC framework and generic NVC user driver.  The IMAGER_NVC_ defines are not
 * GUIDs but really just a mechanism in the ODM system to specify the instance
 * of the imager driver.  The 3:0 bits of the GUID are used to determine which
 * NVC kernel driver to open.  For example, IMAGER_NVC_GUID1 will cause the
 * generic user driver to open /dev/camera.1. IMAGER_NVC_GUID2 = camera.2, etc.
*/
#define IMAGER_NVC_GUID             NV_ODM_GUID('s', '_', 'N', 'V', 'C', 'A', 'M', '0')
#define IMAGER_NVC_GUID1            NV_ODM_GUID('s', '_', 'N', 'V', 'C', 'A', 'M', '1')
#define IMAGER_NVC_GUID2            NV_ODM_GUID('s', '_', 'N', 'V', 'C', 'A', 'M', '2')
#define IMAGER_NVC_GUID3            NV_ODM_GUID('s', '_', 'N', 'V', 'C', 'A', 'M', '3')
#define IMAGER_ST_NVC_GUID1         NV_ODM_GUID('s', 't', 'N', 'V', 'C', 'A', 'M', '1')

// for specific focuser
#define FOCUSER_AR0832_GUID         NV_ODM_GUID('f', '_', 'A', 'R', '0', '8', '3', '2')
#define FOCUSER_AD5820_GUID         NV_ODM_GUID('f', '_', 'A', 'D', '5', '8', '2', '0')
#define FOCUSER_AD5823_GUID         NV_ODM_GUID('f', '_', 'A', 'D', '5', '8', '2', '3')
#define FOCUSER_SH532U_GUID         NV_ODM_GUID('f', '_', 'S', 'H', '5', '3', '2', 'U')
#define FOCUSER_AD5816_GUID         NV_ODM_GUID('f', '_', 'A', 'D', '5', '8', '1', '6')
#define FOCUSER_DW9718_GUID         NV_ODM_GUID('f', '_', 'D', 'W', '9', '7', '1', '8')

/* FOCUSER_NVC_IMAGER is used when the focuser control is in the NVC imager
 * driver.  This define tells the generic NVC focuser user driver to use the
 * imagers file descriptor instead of opening an NVC kernel driver for the
 * focuser.  This allows the normal focuser execution path for the upper SW.
*/
#define FOCUSER_NVC_IMAGER          NV_ODM_GUID('f', '_', 'N', 'V', 'C', 'A', 'M', 'i')
#define FOCUSER_OV5640_GUID         NV_ODM_GUID('f', '_', 'O', 'V', '5', '6', '4', '0')
#define FOCUSER_NVC_GUID            NV_ODM_GUID('f', '_', 'N', 'V', 'C', 'A', 'M', '0')
#define FOCUSER_NVC_GUID1           NV_ODM_GUID('f', '_', 'N', 'V', 'C', 'A', 'M', '1')
#define FOCUSER_NVC_GUID2           NV_ODM_GUID('f', '_', 'N', 'V', 'C', 'A', 'M', '2')
#define FOCUSER_NVC_GUID3           NV_ODM_GUID('f', '_', 'N', 'V', 'C', 'A', 'M', '3')

// for specific flash
#define FLASH_LTC3216_GUID          NV_ODM_GUID('l', '_', 'L', 'T', '3', '2', '1', '6')
#define FLASH_SSL3250A_GUID         NV_ODM_GUID('l', '_', 'L', 'T', '3', '2', '1', '6')
#define FLASH_TPS61050_GUID         NV_ODM_GUID('l', '_', 'T', '6', '1', '0', '5', '0')
#define TORCH_NVC_GUID              NV_ODM_GUID('l', '_', 'N', 'V', 'C', 'A', 'M', '0')

// Pin Use Codes:
// VI/CSI Parallel and Serial Pins and GPIO Pins

// More than one device may be retrieved thru the query
#define NVODM_CAMERA_DEVICE_IS_DEFAULT      (1)

// The imager devices can connect to the parallel bus or the serial bus
// Parallel connections use pins VD0 thru VD9.
// Serial connections use the mipi pins (ex: CSI_D1AN/CSI_D1AP)
#define NVODM_CAMERA_DATA_PIN_SHIFT         (1)
#define NVODM_CAMERA_DATA_PIN_MASK          (0x0F)
#define NVODM_CAMERA_PARALLEL_VD0_TO_VD9    (1 << NVODM_CAMERA_DATA_PIN_SHIFT)
#define NVODM_CAMERA_PARALLEL_VD0_TO_VD7    (2 << NVODM_CAMERA_DATA_PIN_SHIFT)
#define NVODM_CAMERA_SERIAL_CSI_D1A         (4 << NVODM_CAMERA_DATA_PIN_SHIFT)
#define NVODM_CAMERA_SERIAL_CSI_D2A         (5 << NVODM_CAMERA_DATA_PIN_SHIFT)
#define NVODM_CAMERA_SERIAL_CSI_D1A_D2A     (6 << NVODM_CAMERA_DATA_PIN_SHIFT)
#define NVODM_CAMERA_SERIAL_CSI_D1B         (7 << NVODM_CAMERA_DATA_PIN_SHIFT)

// Switching the encoding from the VideoInput module address to use with
// each GPIO module address.
// NVODM_IMAGER_GPIO will tell the nvodm imager how to use each gpio
// A gpio can be used for powerdown (lo, hi) or !powerdown (hi, lo)
// used for reset (hi, lo, hi) or for !reset (lo, hi, lo)
// Or, for mclk or pwm (unimplemented yet)
// We have moved the flash to its own, so it is not needed here
#define NVODM_IMAGER_GPIO_PIN_SHIFT    (24)
#define NVODM_IMAGER_UNUSED            (0x0)
#define NVODM_IMAGER_RESET             (0x1 << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_RESET_AL          (0x2 << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_POWERDOWN         (0x3 << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_POWERDOWN_AL      (0x4 << NVODM_IMAGER_GPIO_PIN_SHIFT)
// only on VGP0
#define NVODM_IMAGER_MCLK              (0x8 << NVODM_IMAGER_GPIO_PIN_SHIFT)
// only on VGP6
#define NVODM_IMAGER_PWM               (0x9 << NVODM_IMAGER_GPIO_PIN_SHIFT)
// If flash code wants the gpio's labelled
// use for any purpose, or not at all
#define NVODM_IMAGER_FLASH0           (0x5 << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_FLASH1           (0x6 << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_FLASH2           (0x7 << NVODM_IMAGER_GPIO_PIN_SHIFT)
// Shutter control
#define NVODM_IMAGER_SHUTTER          (0xA << NVODM_IMAGER_GPIO_PIN_SHIFT)
//

#define NVODM_IMAGER_MASK              (0xF << NVODM_IMAGER_GPIO_PIN_SHIFT)
#define NVODM_IMAGER_CLEAR(_s)         ((_s) & ~(NVODM_IMAGER_MASK))
#define NVODM_IMAGER_IS_SET(_s)        (((_s) & (NVODM_IMAGER_MASK)) != 0)
#define NVODM_IMAGER_FIELD(_s)         ((_s) >> NVODM_IMAGER_GPIO_PIN_SHIFT)


#if defined(__cplusplus)
}
#endif

#endif //NVODM_QUERY_DISCOVERY_IMAGER_H
