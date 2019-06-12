/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_TPS6586X_ADC_HEADER
#define INCLUDED_TPS6586X_ADC_HEADER

/* the ADC is used for battery voltage conversion */
#include "nvodm_pmu_tps6586x.h"


#if defined(__cplusplus)
extern "C"
{
#endif

/* read voltage from adc */
NvBool 
Tps6586xAdcVBatSenseRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 *volt);

/* read bat temperature voltage from ADC */
NvBool 
Tps6586xAdcVBatTempRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 *volt);

/* Calculate the battery temperature */
NvU32
Tps6586xBatteryTemperature(
    NvU32 VBatSense,
    NvU32 VBatTemp);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_TPS6586X_ADC_HEADER
