/*
 * Copyright (c) 2007-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_pinmux.h"
#include "nvrm_drf.h"
#include "nvassert.h"
#include "nvrm_hwintf.h"
#include "ap15/ap15rm_private.h"
#include "nvrm_pinmux_utils.h"
#include "nvodm_query.h"

/*  Each of the pin mux configurations defined in the pin mux spreadsheet are
 *  stored in chip-specific tables.  For each configuration, every pad group
 *  that must be programmed is stored as a single 32b entry, where the register
 *  offset (for both the tristate and pin mux control registers), field bit
 *  position (ditto), pin mux mask, and new pin mux state are programmed.
 *
 *  The tables are microcode for a simple state machine.  The state machine
 *  supports subroutine call/return (up to 4 levels of nesting), so that
 *  pin mux configurations which have substantial repetition can be
 *  represented compactly by separating common portion of the configurations
 *  into a subroutine.  Additionally, the state machine supports
 *  "unprogramming" of the pin mux registers, so that pad groups which are
 *  incorrectly programmed to mux from a controller may be safely disowned,
 *  ensuring that no conflicts exist where multiple pad groups are muxing
 *  the same set of signals.
 *
 *  Each module instance array has a reserved "reset" configuration at index
 *  zero.  This special configuration is used in order to disown all pad
 *  groups whose reset state refers to the module instance.  When a module
 *  instance configuration is to be applied, the reset configuration will
 *  first be applied, to ensure that no conflicts will arise between register
 *  reset values and the new configuration, followed by the application of
 *  the requested configuration.
 *
 *  Furthermore, for controllers which support dynamic pinmuxing (i.e.,
 *  the "Multiplexed" pin map option), the last table entry is reserved for
 *  a "global unset," which will ensure that all configurations are disowned.
 *  This Multiplexed configuration should be applied before transitioning
 *  from one configuration to a second one.
 *
 *  The table data has been packed into a single 32b entry to minimize code
 *  footprint using macros similar to the hardware register definitions, so
 *  that all of the shift and mask operations can be performed with the DRF
 *  macros.
 */

NvError NvRmSetOdmModuleTristate(
    NvRmDeviceHandle hDevice,
    NvU32 OdmModule,
    NvU32 OdmInstance,
    NvBool EnableTristate)
{
    return NvError_NotSupported;
}

NvU32 NvRmPrivRmModuleToOdmModule(
    NvU32 RmModule,
    NvOdmIoModule *pOdmModules,
    NvU32 *pOdmInstances)
{
    NvU32 Cnt = 0;
    NvBool Result = NV_FALSE;

    NV_ASSERT(pOdmModules && pOdmInstances);

    Result = NvRmPrivChipSpecificRmModuleToOdmModule(RmModule,pOdmModules,
                                                           pOdmInstances, &Cnt);

    /*  A default mapping is provided for all standard I/O controllers,
     *  if the chip-specific implementation does not implement a mapping */
    if (!Result)
    {
        NvRmModuleID Module = NVRM_MODULE_ID_MODULE(RmModule);
        NvU32 Instance = NVRM_MODULE_ID_INSTANCE(RmModule);

        Cnt = 1;
        switch (Module) {
        case NvRmModuleID_Display:
            *pOdmModules = NvOdmIoModule_Display;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Ide:
            *pOdmModules = NvOdmIoModule_Ata;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Vi:
            *pOdmModules = NvOdmIoModule_VideoInput;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Usb2Otg:
            *pOdmModules = NvOdmIoModule_Usb;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Pwm:
            *pOdmModules = NvOdmIoModule_Pwm;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Twc:
            *pOdmModules = NvOdmIoModule_Twc;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Hsmmc:
            *pOdmModules = NvOdmIoModule_Hsmmc;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Sdio:
            *pOdmModules = NvOdmIoModule_Sdio;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Nand:
            *pOdmModules = NvOdmIoModule_Nand;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_I2c:
            *pOdmModules = NvOdmIoModule_I2c;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Spdif:
            *pOdmModules = NvOdmIoModule_Spdif;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Uart:
            *pOdmModules = NvOdmIoModule_Uart;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Csi:
            *pOdmModules = NvOdmIoModule_Csi;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Hdmi:
            *pOdmModules = NvOdmIoModule_Hdmi;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Mipi:
            *pOdmModules = NvOdmIoModule_Hsi;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Tvo:
            pOdmModules[0] = NvOdmIoModule_Tvo;
            pOdmModules[1] = NvOdmIoModule_Crt;
            pOdmInstances[0] = 0;
            pOdmInstances[1] = 0;
            Cnt = 2;
            break;
        case NvRmModuleID_Dsi:
            *pOdmModules = NvOdmIoModule_Dsi;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Dvc:
            *pOdmModules = NvOdmIoModule_I2c_Pmu;
            *pOdmInstances = Instance;
            break;
        case NvRmPrivModuleID_Mio_Exio:
            *pOdmModules = NvOdmIoModule_Mio;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Xio:
            *pOdmModules = NvOdmIoModule_Xio;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Spi:
            *pOdmModules = NvOdmIoModule_Sflash;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Slink:
            *pOdmModules = NvOdmIoModule_Spi;
            *pOdmInstances = Instance;
            break;
        case NvRmModuleID_Kbc:
            *pOdmModules = NvOdmIoModule_Kbd;
            *pOdmInstances = Instance;
            break;
        default:
            //  all the RM modules which have no ODM analogs (like 3d)
            Cnt = 0;
            break;
        }
    }
    return Cnt;
}

/* RmSetModuleTristate will enable / disable the pad tristates for the
 * selected pin mux configuration of an IO module.  */
NvError NvRmSetModuleTristate(
    NvRmDeviceHandle hDevice,
    NvRmModuleID RmModule,
    NvBool EnableTristate)
{
    return NvError_NotSupported;
}

void NvRmSetGpioTristate(
    NvRmDeviceHandle hDevice,
    NvU32 Port,
    NvU32 Pin,
    NvBool EnableTristate)
{
    NvRmPrivSetGpioTristate(hDevice, Port, Pin, EnableTristate);
}

NvU32 NvRmExternalClockConfig(
    NvRmDeviceHandle hDevice,
    NvU32 IoModule,
    NvU32 Instance,
    NvU32 Config,
    NvBool EnableTristate)
{
    return NvError_NotSupported;
}

NvError NvRmGetModuleInterfaceCapabilities(
    NvRmDeviceHandle hRm,
    NvRmModuleID ModuleId,
    NvU32 CapStructSize,
    void *pCaps)
{
#ifdef BUILD_FOR_COSIM
    return NvError_NotSupported;
#endif
    NvOdmIoModule OdmModules[4];
    NvU32 OdmInstances[4];
    NvU32 NumOdmModules;

    NV_ASSERT(hRm);
    NV_ASSERT(pCaps);

    if (!hRm || !pCaps)
        return NvError_BadParameter;

    NumOdmModules =
        NvRmPrivRmModuleToOdmModule(ModuleId, (NvOdmIoModule *)OdmModules, OdmInstances);
    NV_ASSERT(NumOdmModules<=1);

    if (!NumOdmModules)
        return NvError_NotSupported;

    switch (OdmModules[0]) {
    case NvOdmIoModule_Hsmmc:
    case NvOdmIoModule_Sdio:
        if (CapStructSize != sizeof(NvRmModuleSdmmcInterfaceCaps))
        {
            NV_ASSERT(!"Invalid cap struct size");
            return NvError_BadParameter;
        }
        break;
    case NvOdmIoModule_Pwm:
        if (CapStructSize != sizeof(NvRmModulePwmInterfaceCaps))
        {
            NV_ASSERT(!"Invalid cap struct size");
            return NvError_BadParameter;
        }
        break;
    case NvOdmIoModule_Nand:
        if (CapStructSize != sizeof(NvRmModuleNandInterfaceCaps))
        {
            NV_ASSERT(!"Invalid cap struct size");
            return NvError_BadParameter;
        }
        break;

    case NvOdmIoModule_Uart:
        if (CapStructSize != sizeof(NvRmModuleUartInterfaceCaps))
        {
            NV_ASSERT(!"Invalid cap struct size");
            return NvError_BadParameter;
        }
        break;

    default:
        return NvError_NotSupported;
    }
    return NvOdmQueryModuleInterfaceCaps(OdmModules[0], OdmInstances[0], pCaps);
}

