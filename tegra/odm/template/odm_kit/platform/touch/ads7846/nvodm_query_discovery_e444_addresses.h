/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
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
 *                 for the E444 LCD Module.
 */

#include "../../adaptations/pmu/nvodm_pmu_pcf50626_supply_info.h"

// ADS7846 TouchPanel
static const NvOdmIoAddress s_ffaAds7846TouchPanelAddresses[] = 
{
    { NvOdmIoModule_Spi, 1, 0x1, 0 },/* SPI2 CS1 */
    { NvOdmIoModule_Gpio, 0x15, 0x03, 0 }, /* GPIO Port V and Pin 3 */
    //Not decided yet
    //{ NvOdmIoModule_Vdd, 0x00, ADD_POWER_RAIL_NUMBER}
};
