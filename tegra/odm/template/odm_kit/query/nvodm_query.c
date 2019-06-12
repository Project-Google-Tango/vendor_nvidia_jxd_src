/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA APX ODM Kit:
 *         Implementation of the ODM Query API</b>
 *
 * @b Description: Implements the query functions for ODMs that may be
 *                 accessed at boot-time, runtime, or anywhere in between.
 */

#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_memc.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_pins.h"
#include "nvodm_keylist_reserved.h"
#include "nvrm_drf.h"


NvError
NvOdmQueryModuleInterfaceCaps(
    NvOdmIoModule Module,
    NvU32 Instance,
    void *pCaps)
{
    return NvSuccess;
}

void NvOdmQueryClockLimits(
    NvOdmIoModule IoModule,
    const NvU32 **pClockSpeedLimits,
    NvU32 *pCount)
{
}

NvOdmDebugConsole NvOdmQueryDebugConsole(void)
{
    return NvOdmDebugConsole_None;
}

NvBool NvOdmQueryUartOverSDEnabled(void)
{
    return NV_FALSE;
}

NvOdmDownloadTransport NvOdmQueryDownloadTransport(void)
{
    return NvOdmDownloadTransport_None;
}

const NvU8* NvOdmQueryDeviceNamePrefix(void)
{
    return NULL;
}

const NvOdmQuerySpiDeviceInfo * NvOdmQuerySpiGetDeviceInfo(
    NvOdmIoModule OdmIoModule,
    NvU32 ControllerId,
    NvU32 ChipSelect)
{
    return NULL;
}

const NvOdmQuerySpiIdleSignalState *NvOdmQuerySpiGetIdleSignalState(
    NvOdmIoModule OdmIoModule,
    NvU32 ControllerId)
{
    return NULL;
}

const NvOdmQueryI2sInterfaceProperty *NvOdmQueryI2sGetInterfaceProperty(
    NvU32 I2sInstanceId)
{
    return NULL;
}

const NvOdmQueryDapPortProperty *NvOdmQueryDapPortGetProperty(
    NvU32 DapPortId)
{
    return NULL;
}

const NvOdmQueryDapPortConnection *NvOdmQueryDapPortGetConnectionTable(
    NvU32 ConnectionIndex)
{
    return NULL;
}

const NvOdmQuerySpdifInterfaceProperty *NvOdmQuerySpdifGetInterfaceProperty(
    NvU32 SpdifInstanceId)
{
    return NULL;
}

const NvOdmQueryAc97InterfaceProperty *NvOdmQueryAc97GetInterfaceProperty(
    NvU32 Ac97InstanceId)
{
    return NULL;
}

const NvOdmQueryI2sACodecInterfaceProp *NvOdmQueryGetI2sACodecInterfaceProperty(
    NvU32 AudioCodecId)
{
    return NULL;
}

/**
 * This function is called from early boot process.
 * Therefore, it cannot use global variables.
 */
NvBool NvOdmQueryAsynchMemConfig(
    NvU32 ChipSelect,
    NvOdmAsynchMemConfig *pMemConfig)
{
    return NV_FALSE;
}

const void *NvOdmQuerySdramControllerConfigGet(NvU32 *pEntries, NvU32 *pRev)
{
    if (pEntries)
        *pEntries = 0;
    return NULL;
}

NvOdmQueryOscillator NvOdmQueryGetOscillatorSource(void)
{
    return NvOdmQueryOscillator_Xtal;
}

NvU32 NvOdmQueryGetOscillatorDriveStrength(void)
{
    return 0;
}

const NvOdmWakeupPadInfo *NvOdmQueryGetWakeupPadTable(NvU32 *pSize)
{
    if (pSize)
        *pSize = 0;
    return NULL;
}

const NvU8* NvOdmQueryManufacturer(void)
{
    return NULL;
}

const NvU8* NvOdmQueryModel(void)
{
    return NULL;
}

const NvU8* NvOdmQueryPlatform(void)
{
    return NULL;
}

const NvU8* NvOdmQueryProjectName(void)
{
    return NULL;
}

NvU32 NvOdmQueryPinAttributes(const NvOdmPinAttrib** pPinAttributes)
{
    return NvError_NotSupported;
}

NvBool NvOdmQueryGetPmuProperty(NvOdmPmuProperty* pPmuProperty)
{
    return NV_FALSE;
}

/**
 * Gets the lowest soc power state supported by the hardware
 *
 * @returns information about the SocPowerState
 */
const NvOdmSocPowerStateInfo *NvOdmQueryLowestSocPowerState(void)
{
    return NULL;
}

const NvOdmUsbProperty *NvOdmQueryGetUsbProperty(
    NvOdmIoModule OdmIoModule,
    NvU32 Instance)
{
    return NULL;
}

const NvOdmQuerySdioInterfaceProperty* NvOdmQueryGetSdioInterfaceProperty(
    NvU32 Instance)
{
    return NULL;
}

const NvOdmQueryHsmmcInterfaceProperty* NvOdmQueryGetHsmmcInterfaceProperty(
    NvU32 Instance)
{
    return NULL;
}

NvU32 NvOdmQueryGetBlockDeviceSectorSize(NvOdmIoModule OdmIoModule)
{
    return 0;
}

const NvOdmQueryOwrDeviceInfo* NvOdmQueryGetOwrDeviceInfo(NvU32 Instance)
{
    return NULL;
}

const NvOdmGpioWakeupSource *NvOdmQueryGetWakeupSources(NvU32 *pCount)
{
    *pCount = 0;
    return NULL;
}

NvU32 NvOdmQueryMemSize(NvOdmMemoryType MemType)
{
    return 0;
}

NvU32 NvOdmQueryCarveoutSize(void)
{
    return 0;
}

NvU32 NvOdmQuerySecureRegionSize(void)
{
    return 0;
}

const NvOdmQuerySpifConfig*
NvOdmQueryGetSpifConfig(
    NvU32 Instance,
    NvU32 DeviceId)
{
    return NULL;
}

NvU32
NvOdmQuerySpifWPStatus(
    NvU32 DeviceId,
    NvU32 StartBlock,
    NvU32 NumberOfBlocks)
{
    return 0;
}

NvU32 NvOdmQueryDataPartnEncryptFooterSize(const char *name)
{
    return 0;
}

NvBool NvOdmQueryGetModemId(NvU8 *ModemId)
{
    /* Implement this function based on the need */
    return NV_FALSE;
}

NvBool NvOdmQueryGetPmicWdtDisableSetting(NvBool *PmicWdtDisable)
{
    return NV_FALSE;
}

NvBool NvOdmQueryIsClkReqPinUsed(NvU32 Instance)
{
    return NV_FALSE;
}

#ifdef LPM_BATTERY_CHARGING
NvBool NvOdmQueryChargingConfigData(NvOdmChargingConfigData *pData)
{
    return NV_FALSE;
}
#endif

NvU8
NvOdmQueryGetSdCardInstance(void)
{
    return 0;
}

NvU32
NvOdmQueryGetWifiOdmData(void)
{
    // No Odm bits reserved for Wifi Chip Selection
    return 0;
}

NvU8
NvOdmQueryBlUnlockBit(void)
{
    return 8;
}

NvU8
NvOdmQueryBlWaitBit(void)
{
    return 12;
}

NvBool NvOdmQueryGetSkuOverride(NvU8 *SkuOverride)
{
    return NV_FALSE;
}

NvU8 NvOdmQueryGetDebugConsoleOptBits(void)
{
    return 15;
}

NvU32 NvOdmQueryGetOdmdataField(NvU32 CustOpt, NvU32 Field)
{
     return 0;
}

NvU32 NvOdmQueryGetUsbData(void)
{
    return 0;
}

NvU32 NvOdmQueryGetPLLMFreqData(void)
{
    return 0;
}

NvU32 NvOdmQueryRotateDisplay (void)
{
    return 0;
}

NvU32 NvOdmQueryRotateBMP(void)
{
    return 0;
}

void NvOdmQueryGetSnorTimings(SnorTimings *timings, enum NorTiming NorTimings)
{
    timings->timing0 = 0xA0400273;
    timings->timing1 = 0x30402;
}

NvBool NvOdmQueryLp0Supported(void)
{
    return NV_FALSE;
}

void NvOdmQueryLP0WakeConfig(NvU32 *wake_mask, NvU32 *wake_level)
{
    // Not implemented
}
