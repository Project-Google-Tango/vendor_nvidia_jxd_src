/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_pmu.h"
#include "nvodm_query_discovery.h"
#include "pmu_hal.h"
#include "nvodm_services.h"
#include "tps6586x/nvodm_pmu_tps6586x.h"
#include "max8907b/max8907b.h"
#include "max8907b/max8907b_rtc.h"
#include "boards/pluto/nvodm_pmu_pluto.h"
#include "boards/p1852/nvodm_pmu_p1852.h"
#include "boards/dalmore/nvodm_pmu_dalmore.h"
#include "boards/e1859_t114/nvodm_pmu_e1859_t114.h"
#include "boards/e1853/nvodm_pmu_e1853.h"
#include "boards/vcm30t30/nvodm_pmu_vcm30t30.h"
#include "boards/vcm30t114/nvodm_pmu_vcm30t114.h"
#include "boards/m2601/nvodm_pmu_m2601.h"
#include "pmu_i2c/nvodm_pmu_i2c.h"
#include "boards/ardbeg/nvodm_pmu_ardbeg.h"
#include "boards/loki/nvodm_pmu_loki.h"

/*
 * This needs to be kept in sync with the conditional in GetPmuInstance()!
 */
#if defined(NV_TARGET_DALMORE)
#elif defined(NV_TARGET_E1859_T114)
#elif defined(NV_TARGET_PLUTO)
#elif defined(NV_TARGET_P1852)
#elif defined(NV_TARGET_E1853)
#elif defined(NV_TARGET_VCM30T30)
#elif defined(NV_TARGET_VCM30T114)
#elif defined(NV_TARGET_M2601)
#elif defined(NV_TARGET_ARDBEG)
#elif defined(NV_TARGET_LOKI)

#else

/*
 * Stub Implementations, which are used if there is no board specific
 * versions available.
 */
static NvBool StubPmuSetup(NvOdmPmuDeviceHandle hPmuDevice)
{
    return NV_TRUE;
}

static void StubPmuRelease(NvOdmPmuDeviceHandle hPmuDevice)
{
}

static void
StubPmuGetCapabilities(
    NvU32 vddId,
    NvOdmPmuVddRailCapabilities* pCapabilities)
{
}

static NvBool
StubPmuGetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddId,
    NvU32* pMilliVolts)
{
    return NV_TRUE;
}

static NvBool
StubPmuSetVoltage(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 vddId,
    NvU32 MilliVolts,
    NvU32* pSettleMicroSeconds)
{
    if ( pSettleMicroSeconds != NULL )
    *pSettleMicroSeconds = 0;
    return NV_TRUE;
}

static NvBool
StubPmuGetAcLineStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuAcLineStatus *pStatus)
{
    *pStatus = NvOdmPmuAcLine_Online;
    return NV_TRUE;
}

static NvBool
StubPmuGetBatteryStatus(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU8 *pStatus)
{
    *pStatus = NVODM_BATTERY_STATUS_UNKNOWN;
    return NV_TRUE;
}

static NvBool
StubPmuGetBatteryData(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryData *pData)
{
    NvOdmPmuBatteryData batteryData;
    batteryData.batteryAverageCurrent  = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryAverageInterval = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryCurrent         = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryLifePercent     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryLifeTime        = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryMahConsumed     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryTemperature     = NVODM_BATTERY_DATA_UNKNOWN;
    batteryData.batteryVoltage         = NVODM_BATTERY_DATA_UNKNOWN;

    *pData = batteryData;

    return NV_TRUE;
}

static void
StubPmuGetBatteryFullLifeTime(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvU32 *pLifeTime)
{
    *pLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
}

static void
StubPmuGetBatteryChemistry(
    NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuBatteryInstance batteryInst,
    NvOdmPmuBatteryChemistry *pChemistry)
{
    *pChemistry = NvOdmPmuBatteryChemistry_LION;
}

static NvBool
StubPmuSetChargingCurrent(
NvOdmPmuDeviceHandle hDevice,
NvOdmPmuChargingPath chargingPath,
NvU32 chargingCurrentLimitMa,
NvOdmUsbChargerType ChargerType)
{
    return NV_TRUE;
}

static void StubPmuInterruptHandler(NvOdmPmuDeviceHandle  hDevice)
{
}

static NvBool StubPmuReadRtc(
    NvOdmPmuDeviceHandle  hDevice,
    NvU32 *Count)
{
    return NV_FALSE;
}

static NvBool StubPmuWriteRtc(
    NvOdmPmuDeviceHandle  hDevice,
    NvU32 Count)
{
    return NV_FALSE;
}

static NvBool
StubPmuIsRtcInitialized(NvOdmPmuDeviceHandle hDevice)
{
    return NV_FALSE;
}

static NvBool
StubPmuPowerOff(NvOdmPmuDeviceHandle hDevice)
{
    return NV_FALSE;
}

static NvBool
StubPmuEnableWdt(NvOdmPmuDeviceHandle hDevice)
{
    return NV_TRUE;
}

static NvBool
StubPmuDisableWdt(NvOdmPmuDeviceHandle hDevice)
{
    return NV_TRUE;
}

void StubPmuSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    return;
}

void StubPmuUnSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    return;
}
#endif /* stubs */

static NvOdmPmuDevice*
GetPmuInstance(NvOdmPmuDeviceHandle hDevice)
{
    static NvOdmPmuDevice Pmu;
    static NvBool         first = NV_TRUE;

    if (first)
    {
        NvOdmOsMemset(&Pmu, 0, sizeof(Pmu));
        first = NV_FALSE;

       /*
        * NOTE: if you add a board here you *MUST* add it above too!
        */
#if defined(NV_TARGET_DALMORE)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = DalmorePmuSetup;
        Pmu.pfnRelease = DalmorePmuRelease;
        Pmu.pfnSuspend = DalmorePmuSuspend;
        Pmu.pfnGetCaps = DalmorePmuGetCapabilities;
        Pmu.pfnGetVoltage = DalmorePmuGetVoltage;
        Pmu.pfnSetVoltage = DalmorePmuSetVoltage;
        Pmu.pfnGetAcLineStatus = DalmorePmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = DalmorePmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = DalmorePmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = DalmorePmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = DalmorePmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = DalmorePmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = DalmorePmuInterruptHandler;
        Pmu.pfnReadRtc = DalmorePmuReadRtc;
        Pmu.pfnWriteRtc = DalmorePmuWriteRtc;
        Pmu.pfnIsRtcInitialized = DalmorePmuIsRtcInitialized;
        Pmu.pfnPowerOff = DalmorePmuPowerOff;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;
#elif defined(NV_TARGET_E1859_T114)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = E1859PmuSetup;
        Pmu.pfnRelease = E1859PmuRelease;
        Pmu.pfnSuspend = E1859PmuSuspend;
        Pmu.pfnGetCaps = E1859PmuGetCapabilities;
        Pmu.pfnGetVoltage = E1859PmuGetVoltage;
        Pmu.pfnSetVoltage = E1859PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = E1859PmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = E1859PmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = E1859PmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = E1859PmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = E1859PmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = E1859PmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = E1859PmuInterruptHandler;
        Pmu.pfnReadRtc = E1859PmuReadRtc;
        Pmu.pfnWriteRtc = E1859PmuWriteRtc;
        Pmu.pfnIsRtcInitialized = E1859PmuIsRtcInitialized;
        Pmu.pfnPowerOff = E1859PmuPowerOff;
#elif defined(NV_TARGET_PLUTO)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = PlutoPmuSetup;
        Pmu.pfnRelease = PlutoPmuRelease;
        Pmu.pfnSuspend = PlutoPmuSuspend;
        Pmu.pfnGetCaps = PlutoPmuGetCapabilities;
        Pmu.pfnGetVoltage = PlutoPmuGetVoltage;
        Pmu.pfnSetVoltage = PlutoPmuSetVoltage;
        Pmu.pfnGetAcLineStatus = PlutoPmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = PlutoPmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = PlutoPmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = PlutoPmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = PlutoPmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = PlutoPmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = PlutoPmuInterruptHandler;
        Pmu.pfnReadRtc = PlutoPmuReadRtc;
        Pmu.pfnWriteRtc = PlutoPmuWriteRtc;
        Pmu.pfnIsRtcInitialized = PlutoPmuIsRtcInitialized;
        Pmu.pfnReadRstReason = PlutoPmuReadRstReason;
        Pmu.pfnPowerOff = PlutoPmuPowerOff;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;
#elif defined(NV_TARGET_P1852)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = P1852PmuSetup;
        Pmu.pfnRelease = P1852PmuRelease;
        Pmu.pfnSuspend = NULL;
        Pmu.pfnGetCaps = P1852PmuGetCapabilities;
        Pmu.pfnGetVoltage = P1852PmuGetVoltage;
        Pmu.pfnSetVoltage = P1852PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = NULL;
        Pmu.pfnGetBatteryStatus = NULL;
        Pmu.pfnGetBatteryData = NULL;
        Pmu.pfnGetBatteryFullLifeTime = NULL;
        Pmu.pfnGetBatteryChemistry = NULL;
        Pmu.pfnSetChargingCurrent = NULL;
        Pmu.pfnInterruptHandler = NULL;
        Pmu.pfnReadRtc = NULL;
        Pmu.pfnWriteRtc = NULL;
        Pmu.pfnIsRtcInitialized = NULL;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;
#elif defined(NV_TARGET_E1853)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = E1853PmuSetup;
        Pmu.pfnRelease = E1853PmuRelease;
        Pmu.pfnSuspend = NULL;
        Pmu.pfnGetCaps = E1853PmuGetCapabilities;
        Pmu.pfnGetVoltage = E1853PmuGetVoltage;
        Pmu.pfnSetVoltage = E1853PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = NULL;
        Pmu.pfnGetBatteryStatus = NULL;
        Pmu.pfnGetBatteryData = NULL;
        Pmu.pfnGetBatteryFullLifeTime = NULL;
        Pmu.pfnGetBatteryChemistry = NULL;
        Pmu.pfnSetChargingCurrent = NULL;
        Pmu.pfnInterruptHandler = NULL;
        Pmu.pfnReadRtc = NULL;
        Pmu.pfnWriteRtc = NULL;
        Pmu.pfnIsRtcInitialized = NULL;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;
#elif defined(NV_TARGET_VCM30T30)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = Vcm30T30PmuSetup;
        Pmu.pfnRelease = Vcm30T30PmuRelease;
        Pmu.pfnSuspend = NULL;
        Pmu.pfnGetCaps = Vcm30T30PmuGetCapabilities;
        Pmu.pfnGetVoltage = Vcm30T30PmuGetVoltage;
        Pmu.pfnSetVoltage = Vcm30T30PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = NULL;
        Pmu.pfnGetBatteryStatus = NULL;
        Pmu.pfnGetBatteryData = NULL;
        Pmu.pfnGetBatteryFullLifeTime = NULL;
        Pmu.pfnGetBatteryChemistry = NULL;
        Pmu.pfnSetChargingCurrent = NULL;
        Pmu.pfnInterruptHandler = NULL;
        Pmu.pfnReadRtc = NULL;
        Pmu.pfnWriteRtc = NULL;
        Pmu.pfnIsRtcInitialized = NULL;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;
#elif defined(NV_TARGET_VCM30T114)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = VCM30T114PmuSetup;
        Pmu.pfnRelease = VCM30T114PmuRelease;
        Pmu.pfnSuspend = VCM30T114PmuSuspend;
        Pmu.pfnGetCaps = VCM30T114PmuGetCapabilities;
        Pmu.pfnGetVoltage = VCM30T114PmuGetVoltage;
        Pmu.pfnSetVoltage = VCM30T114PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = VCM30T114PmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = VCM30T114PmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = VCM30T114PmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = VCM30T114PmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = VCM30T114PmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = VCM30T114PmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = VCM30T114PmuInterruptHandler;
        Pmu.pfnReadRtc = VCM30T114PmuReadRtc;
        Pmu.pfnWriteRtc = VCM30T114PmuWriteRtc;
        Pmu.pfnIsRtcInitialized = VCM30T114PmuIsRtcInitialized;
        Pmu.pfnPowerOff = VCM30T114PmuPowerOff;

#elif defined(NV_TARGET_M2601)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = M2601PmuSetup;
        Pmu.pfnRelease = M2601PmuRelease;
        Pmu.pfnSuspend = NULL;
        Pmu.pfnGetCaps = M2601PmuGetCapabilities;
        Pmu.pfnGetVoltage = M2601PmuGetVoltage;
        Pmu.pfnSetVoltage = M2601PmuSetVoltage;
        Pmu.pfnGetAcLineStatus = NULL;
        Pmu.pfnGetBatteryStatus = NULL;
        Pmu.pfnGetBatteryData = NULL;
        Pmu.pfnGetBatteryFullLifeTime = NULL;
        Pmu.pfnGetBatteryChemistry = NULL;
        Pmu.pfnSetChargingCurrent = NULL;
        Pmu.pfnInterruptHandler = NULL;
        Pmu.pfnReadRtc = NULL;
        Pmu.pfnWriteRtc = NULL;
        Pmu.pfnIsRtcInitialized = NULL;
        Pmu.pfnEnableWdt = NULL;
        Pmu.pfnDisableWdt = NULL;
        Pmu.pfnSetChargingInterrupts = NULL;
        Pmu.pfnUnSetChargingInterrupts = NULL;

#elif defined(NV_TARGET_ARDBEG)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = ArdbegPmuSetup;
        Pmu.pfnRelease = ArdbegPmuRelease;
        Pmu.pfnGetCaps = ArdbegPmuGetCapabilities;
        Pmu.pfnGetVoltage = ArdbegPmuGetVoltage;
        Pmu.pfnSetVoltage = ArdbegPmuSetVoltage;
        Pmu.pfnGetAcLineStatus = ArdbegPmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = ArdbegPmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = ArdbegPmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = ArdbegPmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = ArdbegPmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = ArdbegPmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = ArdbegPmuInterruptHandler;
        Pmu.pfnReadRtc = ArdbegPmuReadRtc;
        Pmu.pfnWriteRtc = ArdbegPmuWriteRtc;
        Pmu.pfnIsRtcInitialized = ArdbegPmuIsRtcInitialized;
        Pmu.pfnPowerOff = ArdbegPmuPowerOff;
        Pmu.pfnGetPowerOnSource = ArdbegPmuGetPowerOnSource;
        Pmu.pfnGetDate = ArdbegPmuGetDate;
        Pmu.pfnSetAlarm  = ArdbegPmuSetAlarm;
        Pmu.pfnClearAlarm = ArdbegPmuClearAlarm;
        Pmu.pfnSetChargingInterrupts= ArdbegPmuEnableInterrupts;
        Pmu.pfnUnSetChargingInterrupts= ArdbegPmuDisableInterrupts;
		Pmu.pfnGetBatteryCapacity = ArdbegPmuGetBatteryCapacity;
		Pmu.pfnGetBatteryAdcVal = ArdbegPmuGetBatteryAdcVal;
		Pmu.pfnReadRstReason = ArdbegPmuReadRstReason;

#elif defined(NV_TARGET_LOKI)
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = LokiPmuSetup;
        Pmu.pfnRelease = LokiPmuRelease;
        Pmu.pfnGetCaps = LokiPmuGetCapabilities;
        Pmu.pfnGetVoltage = LokiPmuGetVoltage;
        Pmu.pfnSetVoltage = LokiPmuSetVoltage;
        Pmu.pfnGetAcLineStatus = LokiPmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = LokiPmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = LokiPmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = LokiPmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = LokiPmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = LokiPmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = LokiPmuInterruptHandler;
        Pmu.pfnReadRtc = LokiPmuReadRtc;
        Pmu.pfnWriteRtc = LokiPmuWriteRtc;
        Pmu.pfnIsRtcInitialized = LokiPmuIsRtcInitialized;
        Pmu.pfnPowerOff = LokiPmuPowerOff;
        Pmu.pfnGetPowerOnSource = LokiPmuGetPowerOnSource;
        Pmu.pfnGetDate = LokiPmuGetDate;
        Pmu.pfnSetAlarm  = LokiPmuSetAlarm;
        Pmu.pfnClearAlarm = LokiPmuClearAlarm;
        Pmu.pfnSetChargingInterrupts= LokiPmuEnableInterrupts;
        Pmu.pfnUnSetChargingInterrupts= LokiPmuDisableInterrupts;

#else
        Pmu.Hal = NV_TRUE;
        Pmu.pfnSetup = StubPmuSetup;
        Pmu.pfnRelease = StubPmuRelease;
        Pmu.pfnGetCaps = StubPmuGetCapabilities;
        Pmu.pfnGetVoltage = StubPmuGetVoltage;
        Pmu.pfnSetVoltage = StubPmuSetVoltage;
        Pmu.pfnGetAcLineStatus = StubPmuGetAcLineStatus;
        Pmu.pfnGetBatteryStatus = StubPmuGetBatteryStatus;
        Pmu.pfnGetBatteryData = StubPmuGetBatteryData;
        Pmu.pfnGetBatteryFullLifeTime = StubPmuGetBatteryFullLifeTime;
        Pmu.pfnGetBatteryChemistry = StubPmuGetBatteryChemistry;
        Pmu.pfnSetChargingCurrent = StubPmuSetChargingCurrent;
        Pmu.pfnInterruptHandler = StubPmuInterruptHandler;
        Pmu.pfnReadRtc = StubPmuReadRtc;
        Pmu.pfnWriteRtc = StubPmuWriteRtc;
        Pmu.pfnIsRtcInitialized = StubPmuIsRtcInitialized;
        Pmu.pfnPowerOff = StubPmuPowerOff;
        Pmu.pfnEnableWdt = StubPmuEnableWdt;
        Pmu.pfnDisableWdt = StubPmuDisableWdt;
        Pmu.pfnSetChargingInterrupts = StubPmuSetChargingInterrupts;
        Pmu.pfnUnSetChargingInterrupts = StubPmuUnSetChargingInterrupts;
#endif
    }

    if (hDevice && Pmu.Hal)
        return &Pmu;

    return NULL;
}

NvBool
NvOdmPmuDeviceOpen(NvOdmPmuDeviceHandle *hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance((NvOdmPmuDeviceHandle)1);

    *hDevice = (NvOdmPmuDeviceHandle)0;

    if (!pmu || !pmu->pfnSetup || pmu->Init)
    {
        *hDevice = (NvOdmPmuDeviceHandle)1;
        return NV_TRUE;
    }

    if (pmu->pfnSetup(pmu))
    {
        *hDevice = (NvOdmPmuDeviceHandle)1;
        pmu->Init = NV_TRUE;
        return NV_TRUE;
    }

    return NV_FALSE;
}

void NvOdmPmuDeviceClose(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (!pmu)
        return;

    if (pmu->pfnRelease)
        pmu->pfnRelease(pmu);

    pmu->Init = NV_FALSE;
}

void NvOdmPmuDeviceSuspend(void)
{
    NvOdmPmuDevice *pmu = GetPmuInstance((NvOdmPmuDeviceHandle)1);

    if (!pmu)
        return;

    if (pmu->pfnSuspend)
        pmu->pfnSuspend();
}

void
NvOdmPmuGetCapabilities(NvU32 vddId,
                        NvOdmPmuVddRailCapabilities* pCapabilities)
{
    //  use a manual handle, since this function doesn't takea  handle
    NvOdmPmuDevice* pmu = GetPmuInstance((NvOdmPmuDeviceHandle)1);

    if (pmu && pmu->pfnGetCaps)
        pmu->pfnGetCaps(vddId, pCapabilities);
    else if (pCapabilities)
    {
        NvOdmOsMemset(pCapabilities, 0, sizeof(NvOdmPmuVddRailCapabilities));
        pCapabilities->OdmProtected = NV_TRUE;
    }
}


NvBool
NvOdmPmuGetVoltage(NvOdmPmuDeviceHandle hDevice,
                   NvU32 vddId,
                   NvU32* pMilliVolts)
{
    NvOdmPmuDevice* pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetVoltage)
        return pmu->pfnGetVoltage(pmu, vddId, pMilliVolts);

    return NV_TRUE;
}

NvBool
NvOdmPmuSetVoltage(NvOdmPmuDeviceHandle hDevice,
                   NvU32 VddId,
                   NvU32 MilliVolts,
                   NvU32* pSettleMicroSeconds)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);
    NvU32 SettlingTime = 0;
    NvBool ErrorVal = NV_TRUE;


    if (pmu && pmu->pfnSetVoltage)
    {
        ErrorVal = pmu->pfnSetVoltage(pmu, VddId, MilliVolts, &SettlingTime);
        if ((!pSettleMicroSeconds) && SettlingTime)
            NvOdmOsWaitUS(SettlingTime);
    }

    if (pSettleMicroSeconds)
        *pSettleMicroSeconds = SettlingTime;

    return ErrorVal;
}


NvBool
NvOdmPmuGetAcLineStatus(NvOdmPmuDeviceHandle hDevice,
                        NvOdmPmuAcLineStatus *pStatus)
{
    NvOdmPmuDevice* pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetAcLineStatus)
        return pmu->pfnGetAcLineStatus(pmu, pStatus);

    return NV_TRUE;
}


NvBool
NvOdmPmuGetBatteryStatus(NvOdmPmuDeviceHandle hDevice,
                         NvOdmPmuBatteryInstance BatteryInst,
                         NvU8 *pStatus)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryStatus)
        return pmu->pfnGetBatteryStatus(pmu, BatteryInst, pStatus);

    return NV_TRUE;
}

NvBool
NvOdmPmuGetBatteryData(NvOdmPmuDeviceHandle hDevice,
                       NvOdmPmuBatteryInstance BatteryInst,
                       NvOdmPmuBatteryData *pData)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryData)
    {
        return pmu->pfnGetBatteryData(pmu, BatteryInst, pData);
     /*   pmu->pfnGetBatteryData(pmu, BatteryInst, pData);
		if(pmu->pfnGetBatteryCapacity)
			return pmu->pfnGetBatteryCapacity(pmu, &pData->batterySoc);*/
    }

    pData->batteryLifePercent = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryVoltage = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryCurrent = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryAverageCurrent = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryAverageInterval = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryMahConsumed = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batteryTemperature = NVODM_BATTERY_DATA_UNKNOWN;
    pData->batterySoc = NVODM_BATTERY_DATA_UNKNOWN;

    return NV_TRUE;
}


void
NvOdmPmuGetBatteryFullLifeTime(NvOdmPmuDeviceHandle hDevice,
                               NvOdmPmuBatteryInstance BatteryInst,
                               NvU32 *pLifeTime)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryFullLifeTime)
        pmu->pfnGetBatteryFullLifeTime(pmu, BatteryInst, pLifeTime);

    else
    {
        if (pLifeTime)
            *pLifeTime = NVODM_BATTERY_DATA_UNKNOWN;
    }
}


void
NvOdmPmuGetBatteryChemistry(NvOdmPmuDeviceHandle hDevice,
                            NvOdmPmuBatteryInstance BatteryInst,
                            NvOdmPmuBatteryChemistry *pChemistry)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryChemistry)
        pmu->pfnGetBatteryChemistry(pmu, BatteryInst, pChemistry);
    else
    {
        if (pChemistry)
            *pChemistry = NVODM_BATTERY_DATA_UNKNOWN;
    }
}

NvBool
NvOdmPmuSetChargingCurrent(NvOdmPmuDeviceHandle hDevice,
                           NvOdmPmuChargingPath ChargingPath,
                           NvU32 ChargingCurrentLimitMa,
                           NvOdmUsbChargerType ChargerType)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnSetChargingCurrent)
        return pmu->pfnSetChargingCurrent(pmu, ChargingPath, ChargingCurrentLimitMa, ChargerType);

    return NV_TRUE;
}

NvBool
NvOdmPmuClearAlarm(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnClearAlarm)
        return pmu->pfnClearAlarm(pmu);

    return NV_FALSE;
}

NvBool
NvOdmPmuGetDate(NvOdmPmuDeviceHandle hDevice, DateTime* time)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetDate)
        return pmu->pfnGetDate(pmu, time);

    return NV_FALSE;
}

NvBool
NvOdmPmuSetAlarm (NvOdmPmuDeviceHandle hDevice, DateTime time)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnSetAlarm)
        return pmu->pfnSetAlarm(pmu, time);

    return NV_FALSE;
}

void NvOdmPmuInterruptHandler(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnInterruptHandler)
        pmu->pfnInterruptHandler(pmu);
}

NvBool NvOdmPmuReadRtc(
    NvOdmPmuDeviceHandle  hDevice,
    NvU32 *Count)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnReadRtc)
        return pmu->pfnReadRtc(pmu, Count);
    return NV_FALSE;
}


NvBool NvOdmPmuWriteRtc(
    NvOdmPmuDeviceHandle  hDevice,
    NvU32 Count)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnWriteRtc)
        return pmu->pfnWriteRtc(pmu, Count);
    return NV_FALSE;
}

NvBool
NvOdmPmuIsRtcInitialized(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnIsRtcInitialized)
        return pmu->pfnIsRtcInitialized(pmu);

    return NV_FALSE;
}

NvBool
NvOdmPmuReadRstReason(NvOdmPmuDeviceHandle hDevice,
    NvOdmPmuResetReason *rst_reason)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnReadRstReason)
        return pmu->pfnReadRstReason(pmu, rst_reason);

    return NV_FALSE;
}

NvBool
NvOdmPmuPowerOff(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnPowerOff)
        return pmu->pfnPowerOff(pmu);

    return NV_FALSE;
}

NvU8
NvOdmPmuGetPowerOnSource(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetPowerOnSource)
        return pmu->pfnGetPowerOnSource(pmu);

    return NVOdmPmuUndefined;
}

void
NvOdmPmuEnableWdt(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnPowerOff)
        pmu->pfnEnableWdt(pmu);
}

void
NvOdmPmuDisableWdt(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnPowerOff)
        pmu->pfnDisableWdt(pmu);
}

void NvOdmPmuI2cShutDown(void)
{
   NvOdmPmuI2cDeInit();
}

void NvOdmPmuSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnSetChargingInterrupts)
        pmu->pfnSetChargingInterrupts(pmu);

    return;
}

void NvOdmPmuUnSetChargingInterrupts(NvOdmPmuDeviceHandle hDevice)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnUnSetChargingInterrupts)
        pmu->pfnUnSetChargingInterrupts(pmu);

    return;
}

NvBool
NvOdmPmuGetBatteryCapacity(NvOdmPmuDeviceHandle hDevice, NvU32* BatCapacity)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryCapacity)
        return pmu->pfnGetBatteryCapacity(pmu ,BatCapacity);

    return NV_FALSE;
}

NvBool
NvOdmPmuGetBatteryAdcVal(NvOdmPmuDeviceHandle hDevice, NvU32 AdcChannelId, NvU32* BatAdcVal)
{
    NvOdmPmuDevice *pmu = GetPmuInstance(hDevice);

    if (pmu && pmu->pfnGetBatteryAdcVal)
        return pmu->pfnGetBatteryAdcVal(pmu ,AdcChannelId ,BatAdcVal);

    return NV_FALSE;
}

void NvOdmPmuBatteryAdc2Voltage(NvU32 BatteryAdcVal ,NvU32* Vol)
{

	NvU32 ref_voltage; //reference_voltage
	NvU32 pullup_res;
	NvU32 pulldown_res;

	ref_voltage = 1250; // 1.25v
	pullup_res = 30;   // 30k
	pulldown_res = 10; // 10k


	*Vol = ((BatteryAdcVal * ref_voltage * (pullup_res + pulldown_res))/ (4095 * pulldown_res));
	if(*Vol < 2800)// avoid pmu adc sample abnormal  after reboot , maybe adb reboot or flash image complete , the voltage value just 0.987 v due to machine not to boot
	{
		*Vol = 3620;
	}
}
