/**
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*  NVIDIA Tegra ODM Kit Sample Display Adaptation for the Samsung
 *  AMOLED-AMAF002 WQVGA LCD
 */

/*  This panel adaptation expects to find a discovery database entry
 *  in the following format:
 *
 *  PeripheralConnectivity[] =
 *   { NvOdmIoModule_Display,       DisplayInstance,             0 },
 *   { NvOdmIoModule_Gpio,            ResetGpioPort,  ResetGpioPin },
 *   { NvOdmIoModule_Spi,     SpiControllerInstance,    ChipSelect },
 *   { NvOdmIoModule_Vdd,              PMU Instance,        RailId }
 *   {...  more Vdds }
 * 
 *  Backlight functionality is not implemented
 */

#ifndef DISPLAY_ADAPTATION_AMOLED_AMAF002_H
#define DISPLAY_ADAPTATION_AMOLED_AMAF002_H

#include "display_hal.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define AMOLED_AMAF002_GUID NV_ODM_GUID('A','M','A','-','F','0','0','2')

/* Fills in the function pointer table for the AMOLED AMA-F002 LCD.
 *  The backlight functions are not modified, to allow a variety of
 *  backlight selections to work with the same code */

void AmoLedAmaF002_GetHal(NvOdmDispDeviceHandle hDevice);

#if defined(__cplusplus)
}
#endif

#endif
