/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVRM_PMU_PRIVATE_H
#define INCLUDED_NVRM_PMU_PRIVATE_H

#include "nvodm_query.h"

#ifdef __cplusplus
extern "C"
{
#endif  /* __cplusplus */

// CPU rail lowering voltage delay (applicable only to the platforms
// with dedicated CPU rail)
#define NVRM_CPU_TO_CORE_DOWN_US (2000)

// Default voltage returned in environment with no PMU support
#define NVRM_NO_PMU_DEFAULT_VOLTAGE (1)

/**
 * Initializes RM PMU interface handle
 *
 * @param hRmDevice The RM device handle
 *
 * @return NvSuccess if initialization completed successfully
 *  or one of common error codes on failure
 */
NvError
NvRmPrivPmuInit(NvRmDeviceHandle hRmDevice);

/**
 * Enables PMU interrupt.
 *
 * @param hRmDevice The RM device handle
 */
void NvRmPrivPmuInterruptEnable(NvRmDeviceHandle hRmDevice);

/**
 * Masks/Unmasks OMU interrupt
 *
 * @param hRmDevice The RM device handle
 * @param mask Set NV_TRUE to maks, and NV_FALSE to unmask PMU interrupt
 */
void NvRmPrivPmuInterruptMask(NvRmDeviceHandle hRmDevice, NvBool mask);

/**
 * Deinitializes RM PMU interface
 *
 * @param hRmDevice The RM device handle
 */
void
NvRmPrivPmuDeinit(NvRmDeviceHandle hRmDevice);

/**
 * Sets new voltage level for the specified PMU voltage rail.
 * Private interface for diagnostic mode only.
 *
 * @param hDevice The Rm device handle.
 * @param vddId The ODM-defined PMU rail ID.
 * @param MilliVolts The new voltage level to be set in millivolts (mV).
 *  Set to ODM_VOLTAGE_OFF to turn off the target voltage.
 * @param pSettleMicroSeconds A pointer to the settling time in microseconds (uS),
 *  which is the time for supply voltage to settle after this function
 *  returns; this may or may not include PMU control interface transaction time,
 *  depending on the ODM implementation. If null this parameter is ignored.
 *
 * @return NV_TRUE if successful, or NV_FALSE otherwise.
 */
NvBool
NvRmPrivDiagPmuSetVoltage(
    NvRmDeviceHandle hDevice,
    NvU32 vddId,
    NvU32 MilliVolts,
    NvU32 * pSettleMicroSeconds);

/**
 * Turns PMU rail On/Off
 *
 * @param hRmDevice The RM device handle
 * @param NvRailId The reserved NV rail GUID
 * @param TurnOn Turn rail ON if True, or turn  rail Off if False
 */
void
NvRmPrivPmuRailControl(
    NvRmDeviceHandle hRmDevice,
    NvU64 NvRailId,
    NvBool TurnOn);

/**
 * Gets PMU rail voltage
 *
 * @param hRmDevice The RM device handle
 * @param NvRailId The reserved NV rail GUID
 *
 * @return PMU rail voltage in mv
 */
NvU32
NvRmPrivPmuRailGetVoltage(
    NvRmDeviceHandle hRmDevice,
    NvU64 NvRailId);

//  Forward declarations for all chip-specific helper functions

/**
 * Sets polarity of dedicated SoC PMU interrupt input
 *
 * @param hRmDevice The RM device handle
 * @param Polarity PMU interrupt polarity to be set
 */
void
NvRmPrivSetPmuIrqPolarity(
    NvRmDeviceHandle hRmDevice,
    NvOdmInterruptPolarity Polarity);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVRM_PMU_PRIVATE_H
