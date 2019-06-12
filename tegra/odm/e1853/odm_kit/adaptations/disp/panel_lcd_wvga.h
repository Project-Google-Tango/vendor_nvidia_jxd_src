/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_PANEL_LCD_WVGA_H
#define INCLUDED_PANEL_LCD_WVGA_H

#include "panels.h"

#if defined(__cplusplus)
extern "C"
{
#endif
#define LCD_WVGA_GUID NV_ODM_GUID('L','C','D',' ','W','V','G','A')

void lcd_wvga_GetHal(NvOdmDispDeviceHandle hDevice);
void custom_GetPinPolarities( NvOdmDispDeviceHandle hDevice, NvU32 *nPins,
    NvOdmDispPin *Pins, NvOdmDispPinPolarity *Values );


#if defined(__cplusplus)
}
#endif

#endif
