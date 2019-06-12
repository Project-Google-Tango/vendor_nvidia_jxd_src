/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database NvOdmIoAddress entries
 *                 for the E906 LCD Module.
 */

#include "../../adaptations/pmu/pcf50626/nvodm_pmu_pcf50626_supply_info.h"

#define NVODM_PORT(x) ((x) - 'a')

// Main LCD
static const NvOdmIoAddress s_ffaMainDisplayAddresses[] = 
{
    { NvOdmIoModule_Display, 0, 0, 0 },
    { NvOdmIoModule_Spi, 0x2, 0x2, 0 },
    { NvOdmIoModule_Gpio, NVODM_PORT('v'), 7, 0 },
    { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_D5REG, 0 }, /* VDDIO_LCD -> D5REG */
    { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_D6REG, 0 }, /* LCD_CORE   -> D6REG */
    { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_VBAT, 0  } /* LCD_BL -> VBAT */
};

// TouchPanel
static const NvOdmIoAddress s_ffaTouchPanelAddresses[] = 
{
    { NvOdmIoModule_I2c, 0x00, 0x20, 0 },/* I2C device address is 0x20 */
    { NvOdmIoModule_Gpio, 0x15, 0x03, 0 }, /* GPIO Port V and Pin 3 */
    { NvOdmIoModule_Vdd, 0x00, PCF50626PmuSupply_D6REG, 0 } /* Touch Vdd   -> D6REG */
};
