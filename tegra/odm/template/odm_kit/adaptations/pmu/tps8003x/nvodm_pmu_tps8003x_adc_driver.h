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
 * @file: <b>NVIDIA Tegra ODM Kit: PMU TPS8003X-driver Interface</b>
 *
 * @b Description: Defines the interface for TPS8003X adc driver.
 *
 */

#ifndef ODM_PMU_TPS8003X_ADC_DRIVER_H
#define ODM_PMU_TPS8003X_ADC_DRIVER_H

#include "nvodm_pmu.h"
#include "pmu/nvodm_adc_driver.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvOdmAdcDriverInfoHandle
Tps8003xAdcDriverOpen(
    NvOdmPmuDeviceHandle hDevice);

void Tps8003xAdcDriverClose(NvOdmAdcDriverInfoHandle hAdcDriverInfo);


NvBool Tps8003xAdcDriverSetProperty(NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId, OdmAdcProps *pAdcProps);

NvBool
Tps8003xAdcDriverReadData(
    NvOdmAdcDriverInfoHandle hAdcDriverInfo,
    NvU32 AdcChannelId,
    NvU32 *pData);


/**
 * Defines GPADC channels.
 */
typedef enum
{
    Tps8003x_BatType,
    Tps8003x_BatTemp,
    Tps8003x_SysSupply = 0x7,
    Tps8003x_BackupBattery,
    Tps8003x_ExternalChargerInput,
    Tps8003x_BatteryChargingCurrent = 0x11,
    Tps8003x_BatteryVoltage,
    Tps8003x_Force32 = 0x7FFFFFFF
}Tps8003xGpadcChannels;


#if defined(__cplusplus)
}
#endif

#endif /* ODM_PMU_TPS8003X_ADC_DRIVER_H */
