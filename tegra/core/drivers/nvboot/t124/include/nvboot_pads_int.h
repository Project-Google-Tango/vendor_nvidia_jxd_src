/*
 * Copyright (c) 2006 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file nvboot_pads_int.h
 *
 * Pads and other low level power function for NvBoot
 *
 * NvBootPads is NVIDIA's interface for control of IO pads and power related
 * functions, including pin mux control.
 *
 */

#ifndef INCLUDED_NVBOOT_PADS_INT_H
#define INCLUDED_NVBOOT_PADS_INT_H

#include "nvcommon.h"
#include "t12x/arapbpm.h"
#include "t12x/nvboot_error.h"
#include "t12x/nvboot_fuse.h"

#define NVBOOT_PADS_PWR_DET_DELAY (6)

#if defined(__cplusplus)
extern "C"
{
#endif

// Symbolic constants for Config arguments to
// NvBootPadsConfigureForBootPeripheral()
typedef enum
{
    NvBootPinmuxConfig_None = 0,

    NvBootPinmuxConfig_Usb3_Otg0   = 1,			// USB3 OTG0
    NvBootPinmuxConfig_Usb3_Otg1   = 2,			// USB3 OTG1

    NvBootPinmuxConfig_Nand_Std_x8     = 2, // NAND x8,     standard  pins
    NvBootPinmuxConfig_Nand_Std_x16    = 1, // NAND x16,    standard  pins
    NvBootPinmuxConfig_Nand_Alt        = 3, // NAND x8,x16, alternate pins

    NvBootPinmuxConfig_Sdmmc_Std_x4    = 3, // eMMC/eSD x4,    standard  pins
    NvBootPinmuxConfig_Sdmmc_Std_x8    = 2, // eMMC/eSD x8,    standard  pins
    NvBootPinmuxConfig_Sdmmc_Alt       = 1, // eMMC/eSD x4,x8, alternate pins

    NvBootPinmuxConfig_Snor_Mux_x16     = 1, // SNOR Muxed x16
    NvBootPinmuxConfig_Snor_Non_Mux_x16 = 2, // SNOR Non Muxed x16
    NvBootPinmuxConfig_Snor_Mux_x32     = 3, // SNOR Muxed x32
    NvBootPinmuxConfig_Snor_Non_Mux_x32 = 4, // SNOR Non Muxed x32


    NvBootPinmuxConfig_Spi             = 1, // SPI Flash

    NvBootPinmuxConfig_Force32 = 0x7fffffff
} NvBootPinmuxConfig;

#define NVBOOT_PADCTL_SLAM_VAL_BDPGLP_1V8  \
    (APB_MISC_GP_GMECFGPADCTRL_0_RESET_VAL)

/*
 * Set up of correct path between a controller and external world
 * Also set the correct IO driver strengths
 *
 * @param identification of the boot device
 * @param configuration of the boot device
 *
 * @retval NvBootError_ValidationFailure of passing an incorrect device
 * @retval NvBootError_Success otherwise
 */
NvBootError
NvBootPadsConfigForBootDevice(
    NvBootFuseBootDevice BootDevice,
    NvU32 Config);


/*
 * Bring the pads out of Deep Power Down operation.
 *
 * @param isWarmboot0 For warmboot0, NV_TRUE and otherwise, NV_FALSE
 *
 * @retval none
 */
void
NvBootPadsExitDeepPowerDown(NvBool isWarmboot0);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_PADS_INT_H
