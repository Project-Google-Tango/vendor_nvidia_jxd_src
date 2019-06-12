/*
 * Copyright (c) 2009-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVODM_PMU_TPS6586X_H_HH
#define NVODM_PMU_TPS6586X_H_HH

#include "nvodm_pmu.h"
#include "nvodm_services.h"
#include "nvodm_pmu_tps6586x_supply_info_table.h"
#include "tps6586x_reg.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#if (NV_DEBUG)
#define NVODMPMU_PRINTF(x)   NvOdmOsDebugPrintf x
#else
#define NVODMPMU_PRINTF(x)
#endif

typedef struct NvOdmPmuDeviceTPSRec
{
    /* The handle to the I2C */
    NvOdmServicesI2cHandle hOdmI2C;

    /* The odm pmu service handle */
    NvOdmServicesPmuHandle hOdmPmuSevice;

    /* Gpio Handles (for external supplies) */
    NvOdmServicesGpioHandle hGpio;
    NvOdmGpioPinHandle hPin[TPS6586x_EXTERNAL_SUPPLY_AP_GPIO_NUM];

    /* the PMU I2C device Address */
    NvU32 DeviceAddr;

    /* Device's private data */
    void *priv;

    /* The current voltage */
    NvU32 curVoltageTable[VRAILCOUNT];

    /* The ref cnt table of the power supplies */
    NvU32 supplyRefCntTable[TPS6586xPmuSupply_Num];

} NvOdmPmuDeviceTPS;

void Tps6586xGetCapabilities( NvU32 vddRail, NvOdmPmuVddRailCapabilities* pCapabilities);
NvBool Tps6586xGetVoltage( NvOdmPmuDeviceHandle hDevice, NvU32 vddRail, NvU32* pMilliVolts);
NvBool Tps6586xSetVoltage( NvOdmPmuDeviceHandle hDevice, NvU32 vddRail, NvU32 MilliVolts, NvU32* pSettleMicroSeconds);
NvBool Tps6586xSetup(NvOdmPmuDeviceHandle hDevice);
void Tps6586xRelease(NvOdmPmuDeviceHandle hDevice);
NvBool Tps6586xGetAcLineStatus( NvOdmPmuDeviceHandle hDevice, NvOdmPmuAcLineStatus *pStatus);
NvBool Tps6586xGetBatteryStatus( NvOdmPmuDeviceHandle hDevice, NvOdmPmuBatteryInstance batteryInst, NvU8 *pStatus);
NvBool Tps6586xGetBatteryData( NvOdmPmuDeviceHandle hDevice, NvOdmPmuBatteryInstance batteryInst, NvOdmPmuBatteryData *pData); 
void Tps6586xGetBatteryFullLifeTime( NvOdmPmuDeviceHandle hDevice, NvOdmPmuBatteryInstance batteryInst, NvU32 *pLifeTime);
void Tps6586xGetBatteryChemistry( NvOdmPmuDeviceHandle hDevice, NvOdmPmuBatteryInstance batteryInst, NvOdmPmuBatteryChemistry *pChemistry);
NvBool Tps6586xSetChargingCurrent( NvOdmPmuDeviceHandle hDevice, NvOdmPmuChargingPath chargingPath, NvU32 chargingCurrentLimitMa, NvOdmUsbChargerType ChargerType); 
void Tps6586xInterruptHandler( NvOdmPmuDeviceHandle  hDevice);
NvBool Tps6586xReadRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 *Count);
NvBool Tps6586xWriteRtc( NvOdmPmuDeviceHandle  hDevice, NvU32 Count);
NvBool Tps6586xIsRtcInitialized( NvOdmPmuDeviceHandle  hDevice);
NvBool Tps6586xPowerOff( NvOdmPmuDeviceHandle  hDevice);

#if defined(__cplusplus)
}
#endif

#endif /* NVODM_PMU_TPS6586X_H_HH */
