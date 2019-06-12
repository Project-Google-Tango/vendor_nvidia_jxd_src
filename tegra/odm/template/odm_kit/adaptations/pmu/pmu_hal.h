/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** 
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Abstraction layer stub for audio codec adaptations</b>
 */

#ifndef INCLUDED_NVODM_PMU_ADAPTATION_HAL_H
#define INCLUDED_NVODM_PMU_ADAPTATION_HAL_H

#include "nvcommon.h"
#include "nvodm_pmu.h"
#include "nvodm_services.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef NvBool (*pfnPmuSetup)(NvOdmPmuDeviceHandle);
typedef void   (*pfnPmuRelease)(NvOdmPmuDeviceHandle);
typedef void   (*pfnPmuSuspend)(void);
typedef void   (*pfnPmuGetCaps)(NvU32, NvOdmPmuVddRailCapabilities*);
typedef NvBool (*pfnPmuGetVoltage)(NvOdmPmuDeviceHandle, NvU32, NvU32*);
typedef NvBool (*pfnPmuSetVoltage)(NvOdmPmuDeviceHandle, NvU32, NvU32, NvU32*);
typedef NvBool (*pfnPmuGetAcLineStatus)(NvOdmPmuDeviceHandle, NvOdmPmuAcLineStatus*);
typedef NvBool (*pfnPmuGetBatteryStatus)(NvOdmPmuDeviceHandle, NvOdmPmuBatteryInstance, NvU8*);
typedef NvBool (*pfnPmuGetBatteryData)(NvOdmPmuDeviceHandle, NvOdmPmuBatteryInstance, NvOdmPmuBatteryData*);
typedef void   (*pfnPmuGetBatteryFullLifeTime)(NvOdmPmuDeviceHandle, NvOdmPmuBatteryInstance, NvU32*);
typedef void   (*pfnPmuGetBatteryChemistry)(NvOdmPmuDeviceHandle, NvOdmPmuBatteryInstance, NvOdmPmuBatteryChemistry*);
typedef NvBool (*pfnPmuSetChargingCurrent)(NvOdmPmuDeviceHandle, NvOdmPmuChargingPath, NvU32, NvOdmUsbChargerType);
typedef NvBool (*pfnPmuClearAlarm)(NvOdmPmuDeviceHandle);
typedef void   (*pfnPmuInterruptHandler)(NvOdmPmuDeviceHandle);
typedef NvBool (*pfnPmuGetDate)(NvOdmPmuDeviceHandle, DateTime*);
typedef NvBool (*pfnPmuSetAlarm)(NvOdmPmuDeviceHandle, DateTime);
typedef NvBool (*pfnPmuReadRtc)(NvOdmPmuDeviceHandle, NvU32*);
typedef NvBool (*pfnPmuWriteRtc)(NvOdmPmuDeviceHandle, NvU32);
typedef NvBool (*pfnPmuIsRtcInitialized)(NvOdmPmuDeviceHandle);
typedef NvBool (*pfnPmuReadRstReason)(NvOdmPmuDeviceHandle, NvOdmPmuResetReason *);
typedef NvBool (*pfnPmuPowerOff)(NvOdmPmuDeviceHandle);
typedef NvU8   (*pfnPmuGetPowerOnSource)(NvOdmPmuDeviceHandle);
typedef NvBool (*pfnPmuDisableWdt)(NvOdmPmuDeviceHandle);
typedef NvBool (*pfnPmuEnableWdt)(NvOdmPmuDeviceHandle);
typedef void  (*pfnPmuSetChargingInterrupts)(NvOdmPmuDeviceHandle);
typedef void  (*pfnPmuUnSetChargingInterrupts)(NvOdmPmuDeviceHandle);
typedef NvBool (*pfnPmuGetBatteryCapacity)(NvOdmPmuDeviceHandle, NvU32*);
typedef NvBool (*pfnPmuGetBatteryAdcVal)(NvOdmPmuDeviceHandle, NvU32, NvU32*);



void StubPmuSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice);
void StubPmuUnSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice);

typedef struct NvOdmPmuDeviceRec
{ 
    pfnPmuSetup                  pfnSetup;
    pfnPmuRelease                pfnRelease;
    pfnPmuSuspend                pfnSuspend;
    pfnPmuGetCaps                pfnGetCaps;
    pfnPmuGetVoltage             pfnGetVoltage;
    pfnPmuSetVoltage             pfnSetVoltage;
    pfnPmuGetAcLineStatus        pfnGetAcLineStatus;
    pfnPmuGetBatteryStatus       pfnGetBatteryStatus;
    pfnPmuGetBatteryData         pfnGetBatteryData;
    pfnPmuGetBatteryFullLifeTime pfnGetBatteryFullLifeTime;
    pfnPmuGetBatteryChemistry    pfnGetBatteryChemistry;
    pfnPmuSetChargingCurrent     pfnSetChargingCurrent;
    pfnPmuClearAlarm             pfnClearAlarm;
    pfnPmuInterruptHandler       pfnInterruptHandler;
    pfnPmuReadRtc                pfnReadRtc;
    pfnPmuWriteRtc               pfnWriteRtc;
    pfnPmuIsRtcInitialized       pfnIsRtcInitialized;
    pfnPmuGetDate                pfnGetDate;
    pfnPmuSetAlarm               pfnSetAlarm;
    pfnPmuReadRstReason          pfnReadRstReason;
    pfnPmuPowerOff               pfnPowerOff;
    pfnPmuGetPowerOnSource       pfnGetPowerOnSource;
    pfnPmuEnableWdt              pfnEnableWdt;
    pfnPmuDisableWdt             pfnDisableWdt;
    pfnPmuSetChargingInterrupts  pfnSetChargingInterrupts;
    pfnPmuUnSetChargingInterrupts  pfnUnSetChargingInterrupts;
	pfnPmuGetBatteryCapacity 	 pfnGetBatteryCapacity;
	pfnPmuGetBatteryAdcVal		 pfnGetBatteryAdcVal;
    void                        *pPrivate;
    NvBool                       Hal;
    NvBool                       Init;
    NvOdmOsSemaphoreHandle       hReturnSemaphore;
} NvOdmPmuDevice;

#ifdef __cplusplus
}
#endif

#endif
