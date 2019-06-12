/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_PINMUX_H
#define INCLUDED_NVRM_PINMUX_H

#include "nvrm_module.h"
#include "nvrm_init.h"
#include "nvodm_modules.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/** @file
 * @brief <b>NVIDIA Driver Development Kit:
 *           Resource Manager %Pin-Mux Services APIs</b>
 *
 * @b Description: Defines the interface for pin-mux configuration, query,
 *                 and control.
 *
 */

/**
 * @defgroup nvrm_pinmux RM Pin-Mux Services API
 *
 * This file declares the interface for pin-mux configuration, query,
 *     and control.
 *
 * For each module that has pins (an I/O module), there may be several muxing
 * configurations. This allows a driver to select or query a particular
 * configuration per I/O module. I/O modules may be instantiated on the
 * chip multiple times.
 *
 * Certain combinations of modules configurations may not be physically
 * possible; say that a hypothetical SPI controller configuration 3 uses pins
 * that are shared by a hypothectial UART configuration 2. Presently, these
 * conflicting configurations are managed via an external tool provided by
 * SysEng, which identifies the configurations for the ODM pin-mux tables
 * depending upon choices made by the ODM.
 *
 * @ingroup nvddk_rm
 * @{
 */

/**
 * Sets the module to tristate configuration.
 * Use enable to release the pinmux. The pins will be
 * tristated when not in use to save power.
 *
 * @param hDevice The RM instance.
 * @param RmModule The module to set.
 * @param EnableTristate NV_TRUE will tristate the specified pins,
 *                       NV_FALSE will un-tristate.
 */
NvError NvRmSetModuleTristate(
    NvRmDeviceHandle hDevice,
    NvRmModuleID RmModule,
    NvBool EnableTristate);

/**
 * Sets an ODM module ID to tristate configuration. Analagous to
 * NvRmSetModuleTristate(), but indexed based on the ODM module ID,
 * rather than the controller ID.
 *
 * @param hDevice The RM instance.
 * @param OdmModule The module to set (should be of type ::NvOdmIoModule).
 * @param OdmInstance The instance of the module to set.
 * @param EnableTristate NV_TRUE will tristate the specified pins,
 *                       NV_FALSE will un-tristate.
 */
NvError NvRmSetOdmModuleTristate(
    NvRmDeviceHandle hDevice,
    NvU32 OdmModule,
    NvU32 OdmInstance,
    NvBool EnableTristate);

/**
 * Configures modules that can provide clock sources to peripherals.
 * If a Tegra application processor is expected to provide a clock source
 * to an external peripheral, this API should be called to configure the
 * clock source and to ensure that its pins are driven prior to attempting
 * to program the peripheral through a command interface (e.g., SPI).
 *
 * @param hDevice The RM instance.
 * @param IoModule The module to set, must be ::NvOdmIoModule_ExternalClock.
 * @param Instance The instance of the I/O module to be set.
 * @param Config The pin map configuration for the I/O module.
 * @param EnableTristate NV_TRUE will tristate the specified clock source,
 *                       NV_FALSE will drive it.
 *
 * @return The clock frequency, in KHz, that is output on the
 *  designated pin (or '0' if no clock frequency is specified or found).
 */
NvU32 NvRmExternalClockConfig(
    NvRmDeviceHandle hDevice,
    NvU32 IoModule,
    NvU32 Instance,
    NvU32 Config,
    NvBool EnableTristate);

/**
 * Holds the SDMMC interface capabilities.
 */
typedef struct NvRmModuleSdmmcInterfaceCapsRec
{
    /// Maximum bus width supported by the physical interface.
    /// Will be 2, 4, or 8 depending on the selected pin mux.
    NvU32 MmcInterfaceWidth;
} NvRmModuleSdmmcInterfaceCaps;

/**
 * Holds the PCIE interface capabilities.
 */
typedef struct NvRmModulePcieInterfaceCapsRec
{
    /// Maximum bus type supported by the physical interface.
    /// Will be 4X1 or 2X2 depending on the selected pin mux.
    NvU32 PcieNumEndPoints;
    NvU32 PcieLanesPerEp;
} NvRmModulePcieInterfaceCaps;

/**
 * Holds the PWM interface capabilities.
 */
typedef struct NvRmModulePwmInterfaceCapsRec
{
    /// The OR bits value of PWM output IDs supported by the
    /// physical interface depending on the selected pin mux.
    /// Hence:
    /// - PwmOutputId_PWM0 = bit 0
    /// - PwmOutputId_PWM1 = bit 1
    /// - PwmOutputId_PWM2 = bit 2
    /// - PwmOutputId_PWM3 = bit 3
    NvU32 PwmOutputIdSupported;
} NvRmModulePwmInterfaceCaps;

/**
 * Holds the NAND interface capabilities.
 */
typedef struct NvRmModuleNandInterfaceCapsRec
{
    /// Maximum bus width supported by the physical interface.
    /// Will be 8 or 16 depending on the selected pin mux.
    NvU8 NandInterfaceWidth;
    NvBool IsCombRbsyMode;
} NvRmModuleNandInterfaceCaps;

/**
 * Holds the UART interface capabilities.
 */
typedef struct NvRmModuleUartInterfaceCapsRec
{
    /// Max number of the interface lines supported by the physical interface.
    /// Will be 0, 2, 4, or 8 depending on the selected pin mux.
    /// - 0 means there is no physical interface for the uart.
    /// - 2 means only rx/tx lines are supported.
    /// - 4 means only rx/tx/rtx/cts lines are supported.
    /// - 8 means full modem lines are supported.
    NvU32 NumberOfInterfaceLines;
} NvRmModuleUartInterfaceCaps;

/**
 * @brief Queries the board-defined capabilities of an I/O controller.
 *
 * This API returns capabilities for controller modules based on
 * interface properties defined by ODM query interfaces, such as the
 * pin mux query.
 *
 * \a pCap should be a pointer to the matching \c NvRmxxxInterfaceCaps
 * structure for the \a ModuleId, and \a CapStructSize should be
 * the \c sizeof(structure type) and also should be word-aligned.
 *
 * @retval NvError_NotSupported If the specified \a ModuleID does not
 *     exist on the current platform.
 */
NvError NvRmGetModuleInterfaceCapabilities(
    NvRmDeviceHandle hRm,
    NvRmModuleID ModuleId,
    NvU32 CapStructSize,
    void* pCaps);

/**
 * Defines SoC strap groups.
 */
typedef enum
{
    /// Specifies ram_code strap group.
    NvRmStrapGroup_RamCode = 1,
    NvRmStrapGroup_Num,
    NvRmStrapGroup_Force32 = 0x7FFFFFFF
} NvRmStrapGroup;

/** @} */

#if defined(__cplusplus)
}
#endif

#endif
