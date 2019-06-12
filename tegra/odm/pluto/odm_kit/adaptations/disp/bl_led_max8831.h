/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_BL_LED_MAX8831_H
#define INCLUDED_BL_LED_MAX8831_H

#include "nvodm_disp.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"
#include "nvodm_backlight.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Max8831 slave address for write and read
#define MAX8831_I2C_SLAVE_ADDR_WRITE    0x9A
#define MAX8831_I2C_SLAVE_ADDR_READ     0x9B

// I2c Instance, Frequency
#define MAX8831_I2C_INSTANCE               1
#define MAX8831_I2C_SPEED_KHZ            100

// Max8831 register address
#define MAX8831_REG_ONOFF_CNTL          0x00
#define MAX8831_REG_LED1_RAMP_CNTL      0x03
#define MAX8831_REG_LED2_RAMP_CNTL      0x04
#define MAX8831_REG_ILED1_CNTL          0x0B
#define MAX8831_REG_ILED2_CNTL          0x0C
#define MAX8831_REG_STAT1               0x2D
#define MAX8831_REG_STAT2               0x2E
#define MAX8831_REG_CHIP_ID1            0x39
#define MAX8831_REG_CHIP_ID2            0x3A

#define MAX8831_ENB_LED1      0x01
#define MAX8831_ENB_LED2      0x02
#define MAX8831_ILED_CNTL_VALUE_MAX       0x7F

NvBool Max8831_BacklightInit(void);
void Max8831_SetBacklightIntensity(NvU8 Intensity);
void Max8831_BacklightDeinit(void);

#if defined(__cplusplus)
}
#endif
#endif  // INCLUDED_BL_LED_MAX8831_H
