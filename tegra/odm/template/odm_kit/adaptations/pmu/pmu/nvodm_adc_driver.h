/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-driver Interface</b>
  *
  * @b Description: Defines the typical interface for Adc driver and some
  * common structure related to Adc driver initialization.
  *
  */

#ifndef INCLUDED_NVODM_ADC_DRIVER_H
#define INCLUDED_NVODM_ADC_DRIVER_H

typedef void *NvOdmAdcDriverInfoHandle;

typedef struct OdmAdcPropsRec
{
    NvBool IsOnce;
    NvU32 SamplingRate;
    NvU32 Resolution;
} OdmAdcProps;


#if defined(__cplusplus)
extern "C"
{
#endif

/* Typical Interface for the battery drivers, battery driver needs to implement
 * following function with same set of argument.*/
typedef NvOdmAdcDriverInfoHandle (*AdcDriverOpen)(NvOdmPmuDeviceHandle hDevice);

typedef void (*AdcDriverClose)(NvOdmAdcDriverInfoHandle hAdcDriverInfo);

typedef void (*AdcDriverSetProperty)(NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId, OdmAdcProps *pAdcProps);

typedef NvBool
(*AdcDriverReadData)(
    NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId,
    NvU32 *pData);

#if defined(__cplusplus)
}
#endif

/** @} */
#endif // INCLUDED_NVODM_ADC_DRIVER_H
