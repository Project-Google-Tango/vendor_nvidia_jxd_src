/*
 * Copyright (c) 2008 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_CLOCKS_LIMITS_PRIVATE_H
#define INCLUDED_NVRM_CLOCKS_LIMITS_PRIVATE_H

#include "nvrm_power_private.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// Maximum supported SoC process corners
#define NVRM_PROCESS_CORNERS (4)

// Maximum supported core and/or CPU voltage characterization steps
#define NVRM_VOLTAGE_STEPS (8)

// Minimum required core voltage resolution
#define NVRM_CORE_RESOLUTION_MV (25)

/// Maximum safe core voltage step
#define NVRM_SAFE_VOLTAGE_STEP_MV (100)

// Minimum system bus frequency
#define NVRM_BUS_MIN_KHZ (32)

// Minimum SDRAM bus frequency
#define NVRM_SDRAM_MIN_KHZ (12000)

// ID used by RM to record clock sources V/F dependencies
#define NVRM_DEVID_CLK_SRC (1000)

/**
 * Oscillator (main) clock doubler configuration record
 */
typedef struct NvRmOscDoublerConfigRec
{
    NvRmFreqKHz OscKHz;
    NvU32 Taps[NVRM_PROCESS_CORNERS];
} NvRmOscDoublerConfig;

/**
 * Module clocks limits arranged according to the HW module IDs.
 */
typedef struct NvRmScaledClkLimitsRec
{
    NvU32 HwDeviceId;
    NvU32 SubClockId;
    NvRmFreqKHz MinKHz;
    NvRmFreqKHz MaxKHzList[NVRM_VOLTAGE_STEPS];
} NvRmScaledClkLimits;

/**
 * Combines maximum limits for modules depended on SoC SKU
 */
typedef struct NvRmSKUedLimitsRec
{
    NvRmFreqKHz CpuMaxKHz;
    NvRmFreqKHz AvpMaxKHz;
    NvRmFreqKHz VdeMaxKHz;
    NvRmFreqKHz McMaxKHz;
    NvRmFreqKHz Emc2xMaxKHz;
    NvRmFreqKHz TDMaxKHz;
    NvRmFreqKHz DisplayAPixelMaxKHz;
    NvRmFreqKHz DisplayBPixelMaxKHz;
    NvRmMilliVolts NominalCoreMv;   // for common core rail
    NvRmMilliVolts NominalCpuMv;    // for dedicated CPU rail
} NvRmSKUedLimits;

/**
 * Combines SoC frequency/voltage shmoo data
 * (includes data for CPU on the common core rail)
 */
typedef struct NvRmSocShmooRec
{
    const NvU32* ShmooVoltages;
    NvU32 ShmooVmaxIndex;

    const NvRmScaledClkLimits* ScaledLimitsList;
    NvU32 ScaledLimitsListSize;

    const NvRmSKUedLimits* pSKUedLimits;

    const NvRmOscDoublerConfig* OscDoublerCfgList;
    NvU32 OscDoublerCfgListSize;

    NvU32 DqsibOffset;
    NvRmMilliVolts SvopLowVoltage;
    NvU32 SvopLowSetting;
    NvU32 SvopHighSetting;
} NvRmSocShmoo;

/**
 * Combines frequency/voltage shmoo data for CPU on the dedicated voltage rail
 * (separated from common SoC core rail)
 */
typedef struct NvRmCpuShmooRec
{
    const NvU32* ShmooVoltages;
    NvU32 ShmooVmaxIndex;

    const NvRmScaledClkLimits* pScaledCpuLimits;
} NvRmCpuShmoo;

/**
 * Combines chip SKU and process corner records with shmoo data
 */
typedef struct NvRmChipFlavorRec
{
    NvU16 sku;

    NvU16 corner;
    const NvRmSocShmoo* pSocShmoo; // shmoo core rail (may include CPU)

    NvU16 CpuCorner;
    const NvRmCpuShmoo* pCpuShmoo; // shmoo dedicated CPU rail (NULL if none)
} NvRmChipFlavor;

/**
 * Combines module clock frequency limits
 */
typedef struct NvRmModuleClockLimitsRec
{
    NvRmFreqKHz MinKHz;
    NvRmFreqKHz MaxKHz;
} NvRmModuleClockLimits;

/**
 * Initializes module clock limits table.
 * 
 * @param hRmDevice The RM device handle. 
 * 
 * @return A pointer to the module clock limits table
 */
const NvRmModuleClockLimits*
NvRmPrivClockLimitsInit(NvRmDeviceHandle hRmDevice);

/**
 * Gets list of maximum frequencies for the specified module clock in
 * ascending order of scaling voltage levels.
 * 
 * @param hRmDevice The RM device handle.
 * @param Module  The targeted module ID.
 * @param pListSize A pointer to a variable filled with list size (i.e.,
 *  number of scaling voltage levels)
 * 
 * @return Pointer to the frequencies list (NULL if the module is not present,
 *  or the list does not exist)
 */
const NvRmFreqKHz*
NvRmPrivModuleVscaleGetMaxKHzList(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID Module,
    NvU32* pListSize);

/**
 * Gets core voltage level required for operation of the specified module
 * at the specified frequency.
 * 
 * @param hRmDevice The RM device handle.
 * @param Module  The targeted module ID.
 * @param FreqKHz The trageted module frequency in kHz.
 * 
 * @return Core voltage level in mV.
 */
NvRmMilliVolts
NvRmPrivModuleVscaleGetMV(
    NvRmDeviceHandle hRmDevice,
    NvRmModuleID Module,
    NvRmFreqKHz FreqKHz);

/**
 * Gets minimum core voltage level required for operation of all non-DFS
 * modules at current frequencies.
 * 
 * @param hRmDevice The RM device handle.
 * 
 * @return Core voltage level in mV.
 */
NvRmMilliVolts
NvRmPrivModulesGetOperationalMV(NvRmDeviceHandle hRmDevice);

/**
 * Gets minimum core voltage level required to use module clock source with
 * specified frequency.
 * 
 * @param hRmDevice The RM device handle.
 * 
 * @return Core voltage level in mV.
 */
NvRmMilliVolts
NvRmPrivSourceVscaleGetMV(NvRmDeviceHandle hRmDevice, NvRmFreqKHz FreqKHz);

/**
 * Gets SoC nominal core voltage.
 * 
 * @param hRmDevice The RM device handle.
 * 
 * @return Nominal core voltage in mV.
 */
NvRmMilliVolts
NvRmPrivGetNominalMV(NvRmDeviceHandle hRmDevice);

/**
 * Gets number of delay taps for Oscillator Doubler.
 * 
 * @param hRmDevice The RM device handle.
 * @param OscKHz Oscillator (main) frequency in KHz.
 * @param pTaps A pointer to the variable, filled with number of delay taps.
 * 
 * @return NvSuccess if the specified oscillator frequency is supported, and
 * NvError_NotSupported, otherwise.
 */
NvError
NvRmPrivGetOscDoublerTaps(
    NvRmDeviceHandle hRmDevice,
    NvRmFreqKHz OscKHz,
    NvU32* pTaps);

/**
 * Gets RAM SVOP low voltage parameters.
 * 
 * @param hRmDevice The RM device handle.
 * @param pSvopLowMv A pointer to a variable filled with SVOP low voltage
 *  threshold in mv.
 * @param pSvopLvSetting A pointer to a variable filled with SVOP low voltage
 *  settings.
 * @param pSvopHvSetting A pointer to a variable filled with SVOP high voltage
 *  settings.
 */
void
NvRmPrivGetSvopParameters(
    NvRmDeviceHandle hRmDevice,
    NvRmMilliVolts* pSvopLowMv,
    NvU32* pSvopLvSetting,
    NvU32* pSvopHvSetting);

/**
 * Gets 32-bit offset to ODM EMC DQSIB settings.
 * 
 * @param hRmDevice The RM device handle.
 * 
 * @return DQSIB offset.
 */
NvU32
NvRmPrivGetEmcDqsibOffset(NvRmDeviceHandle hRmDevice);

/**
 * Verifies if SoC has dedicated CPU voltage rail.
 * 
 * @param hRmDevice The RM device handle.
 * 
 * @return NV_TRUE if SoC has dedicated CPU voltage rail,
 *  and NV_FALSE if CPU is on common SoC core rail.
 */
NvBool NvRmPrivIsCpuRailDedicated(NvRmDeviceHandle hRmDevice);

/**
 * Initializes SoC characterization data base
 * 
 * @param hRmDevice The RM device handle.
 * @param pChipFlavor a pointer to the chip "flavor" structure
 *  that this function fills in
 * 
 * @return NvSuccess if completed successfully, or NvError_NotSupported,
 *  otherwise.
 */
NvError
NvRmPrivChipShmooDataInit(
    NvRmDeviceHandle hRmDevice,
    NvRmChipFlavor* pChipFlavor);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif  // INCLUDED_NVRM_CLOCKS_LIMITS_PRIVATE_H
