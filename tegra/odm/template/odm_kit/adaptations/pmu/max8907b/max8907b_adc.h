/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_MAX8907B_ADC_HEADER
#define INCLUDED_MAX8907B_ADC_HEADER

#if defined(__cplusplus)
extern "C"
{
#endif

/* read voltage from ... */
NvBool 
Max8907bAdcVBatSenseRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 *volt);

/* read bat temperature voltage from ADC */
NvBool 
Max8907bAdcVBatTempRead(
    NvOdmPmuDeviceHandle hDevice, 
    NvU32 *volt);

/* Calculate the battery temperature */
NvU32
Max8907bBatteryTemperature(
    NvU32 VBatSense,
    NvU32 VBatTemp);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_MAX8907B_ADC_HEADER
