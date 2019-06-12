
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_power.h"

void NvRmDfsSetLowVoltageThreshold(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsVoltageRailId RailId,
    NvRmMilliVolts LowMv)
{
}

void NvRmDfsGetLowVoltageThreshold(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsVoltageRailId RailId,
    NvRmMilliVolts *pLowMv,
    NvRmMilliVolts *pPresentMv)
{
}

NvError NvRmDfsLogBusyGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 *pSampleIndex,
    NvU32 *pClientId,
    NvU32 *pClientTag,
    NvRmDfsBusyHint *pBusyHint)
{
    return NvError_NotSupported;
}

NvError NvRmDfsLogStarvationGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 *pSampleIndex,
    NvU32 *pClientId,
    NvU32 *pClientTag,
    NvRmDfsStarvationHint *pStarvationHint)
{
    return NvError_NotSupported;
}

NvError NvRmDfsLogActivityGetEntry(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 EntryIndex,
    NvU32 LogDomainsCount,
    NvU32 *pIntervalMs,
    NvU32 * pLp2TimeMs,
    NvU32 *pActiveCyclesList,
    NvRmFreqKHz *pAveragesList,
    NvRmFreqKHz *pFrequenciesList)
{
    return NvError_NotSupported;
}

NvError NvRmDfsLogGetMeanFrequencies(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 LogMeanFreqListCount,
    NvRmFreqKHz *pLogMeanFreqList,
    NvU32 *pLogLp2TimeMs,
    NvU32 *pLogLp2Entries )
{
    return NvError_NotSupported;
}

void NvRmDfsLogStart(NvRmDeviceHandle hRmDeviceHandle)
{
}

NvError NvRmDfsSetAvHighCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsSystemHighKHz,
    NvRmFreqKHz DfsAvpHighKHz,
    NvRmFreqKHz DfsVpipeHighKHz)
{
    return NvSuccess;
}

NvError NvRmDfsSetCpuEmcHighCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsCpuHighKHz,
    NvRmFreqKHz DfsEmcHighKHz)
{
    return NvSuccess;
}

NvError NvRmDfsSetEmcEnvelope(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsEmcLowCornerKHz,
    NvRmFreqKHz DfsEmcHighCornerKHz)
{
    return NvSuccess;
}

NvError NvRmDfsSetCpuEnvelope(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmFreqKHz DfsCpuLowCornerKHz,
    NvRmFreqKHz DfsCpuHighCornerKHz)
{
    return NvSuccess;
}

NvError NvRmDfsSetTarget(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 DfsFreqListCount,
    const NvRmFreqKHz * pDfsTargetFreqList)
{
    return NvSuccess;
}

NvError NvRmDfsSetLowCorner(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 DfsFreqListCount,
    const NvRmFreqKHz * pDfsLowFreqList)
{
    return NvSuccess;
}

NvError NvRmDfsSetState(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsRunState NewDfsRunState)
{
    return NvSuccess;
}

NvError NvRmDfsGetClockUtilization(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvRmDfsClockUsage *pClockUsage)
{
    return NvError_NotSupported;
}

NvRmDfsRunState NvRmDfsGetState(NvRmDeviceHandle hRmDeviceHandle)
{
    return NvRmDfsRunState_Disabled;
}

NvError NvRmPowerStarvationHint(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvU32 ClientId,
    NvBool Starving)
{
    return NvSuccess;
}

NvError NvRmPowerBusyHintMulti(
    NvRmDeviceHandle hRmDeviceHandle,
    NvU32 ClientId,
    const NvRmDfsBusyHint *pMultiHint,
    NvU32 NumHints,
    NvRmDfsBusyHintSyncMode Mode)
{
    return NvSuccess;
}

NvError NvRmPowerBusyHint(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmDfsClockId ClockId,
    NvU32 ClientId,
    NvU32 BoostDurationMs,
    NvRmFreqKHz BoostKHz)
{
    return NvSuccess;
}

NvError NvRmPowerVoltageControl(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmMilliVolts MinVolts,
    NvRmMilliVolts MaxVolts,
    const NvRmMilliVolts *PrefVoltageList,
    NvU32 PrefVoltageListCount,
    NvRmMilliVolts *CurrentVolts)
{
    if (CurrentVolts)
    {
        if (PrefVoltageListCount)
            *CurrentVolts = PrefVoltageList[0];
        else
            *CurrentVolts = MinVolts;
    }

    return NvSuccess;
}

NvError NvRmPowerModuleClockControl(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvBool Enable)
{
    if (NVRM_MODULE_ID_MODULE(ModuleId)!=NvRmModuleID_GraphicsHost)
        NvOsDebugPrintf("%s %s MOD[%u] INST[%u]\n", __func__,
                        (Enable)?"on" : "off",
                        NVRM_MODULE_ID_MODULE(ModuleId),
                        NVRM_MODULE_ID_INSTANCE(ModuleId));
    return NvSuccess;
}

NvError NvRmPowerModuleClockConfig(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId,
    NvU32 ClientId,
    NvRmFreqKHz MinFreq,
    NvRmFreqKHz MaxFreq,
    const NvRmFreqKHz *PrefFreqList,
    NvU32 PrefFreqListCount,
    NvRmFreqKHz *CurrentFreq,
    NvU32 flags)
{
    if (CurrentFreq) {
        if (PrefFreqList)
            *CurrentFreq = PrefFreqList[0];
        else
            *CurrentFreq = MinFreq;
    }

    return NvSuccess;
}

NvRmFreqKHz NvRmPowerModuleGetMaxFrequency(
    NvRmDeviceHandle hRmDeviceHandle,
    NvRmModuleID ModuleId)
{
    if(ModuleId == NvRmModuleID_Mpe)
        return 250000;
    if(ModuleId == NvRmModuleID_Vde)
        return 250000;
    return 0;
}

NvRmFreqKHz NvRmPowerGetPrimaryFrequency(NvRmDeviceHandle hRmDeviceHandle)
{
    return 0;
}

void NvRmPowerUnRegister(NvRmDeviceHandle hRmDeviceHandle, NvU32 ClientId)
{
}

NvError NvRmPowerRegister(
    NvRmDeviceHandle hRmDeviceHandle,
    NvOsSemaphoreHandle hEventSemaphore,
    NvU32 *pClientId)
{
    if (pClientId)
        *pClientId = 0xdeadbeef;

    return NvSuccess;
}
