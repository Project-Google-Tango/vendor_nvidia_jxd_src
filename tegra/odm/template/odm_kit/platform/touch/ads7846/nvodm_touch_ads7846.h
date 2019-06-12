/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_TOUCH_ADS7846_H
#define INCLUDED_NVODM_TOUCH_ADS7846_H

#include "nvodm_touch_int.h"
#include "nvodm_services.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool ADS7846_Open(NvOdmTouchDeviceHandle *hDevice);

void ADS7846_GetCapabilities(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCapabilities* pCapabilities);

NvBool ADS7846_ReadCoordinate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchCoordinateInfo *coord);

NvBool ADS7846_EnableInterrupt(NvOdmTouchDeviceHandle hDevice, NvOdmOsSemaphoreHandle hIntSema);

NvBool ADS7846_HandleInterrupt(NvOdmTouchDeviceHandle hDevice);

NvBool ADS7846_GetSampleRate(NvOdmTouchDeviceHandle hDevice, NvOdmTouchSampleRate* pTouchSampleRate);

NvBool ADS7846_SetSampleRate(NvOdmTouchDeviceHandle hDevice, NvU32 SampleRate);

NvBool ADS7846_PowerControl(NvOdmTouchDeviceHandle hDevice, NvOdmTouchPowerModeType mode);

NvBool ADS7846_GetCalibrationData(NvOdmTouchDeviceHandle hDevice, NvU32 NumOfCalibrationData, NvS32* pRawCoordBuffer);

NvBool ADS7846_PowerOnOff(NvOdmTouchDeviceHandle hDevice, NvBool OnOff);

void ADS7846_Close( NvOdmTouchDeviceHandle hDevice);


#if defined(__cplusplus)
}
#endif


#endif // INCLUDED_NVODM_TOUCH_ADS7846_H
