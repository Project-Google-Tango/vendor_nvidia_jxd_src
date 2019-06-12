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
#include "t30/arapbpm.h"
#include "t30/nvboot_error.h"
#include "t30/nvboot_fuse.h"

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

    NvBootPinmuxConfig_MobileLbaNand   = 1, // MobileLBA NAND

    NvBootPinmuxConfig_MuxOneNand      = 5, // MuxOneNAND (SNOR config 5)
    NvBootPinmuxConfig_OneNand         = 6, // OneNAND    (SNOR config 6)

    NvBootPinmuxConfig_Nand_Std_x8     = 2, // NAND x8,     standard  pins
    NvBootPinmuxConfig_Nand_Std_x16    = 1, // NAND x16,    standard  pins
    NvBootPinmuxConfig_Nand_Alt        = 3, // NAND x8,x16, alternate pins

    NvBootPinmuxConfig_Sdmmc_Std_x4    = 3, // eMMC/eSD x4,    standard  pins
    NvBootPinmuxConfig_Sdmmc_Std_x8    = 2, // eMMC/eSD x8,    standard  pins
    NvBootPinmuxConfig_Sdmmc_Alt       = 1, // eMMC/eSD x4,x8, alternate pins

    NvBootPinmuxConfig_Snor_Mux_x16    = 3, // SNOR Muxed x16
    NvBootPinmuxConfig_Snor_NonMux_x16 = 4, // SNOR NonMuxed x16
    NvBootPinmuxConfig_Snor_Mux_x32    = 2, // SNOR Muxed x32
    NvBootPinmuxConfig_Snor_NonMux_x32 = 1, // SNOR NonMuxed x32

    NvBootPinmuxConfig_Spi             = 1, // SPI Flash

    NvBootPinmuxConfig_Force32 = 0x7fffffff
} NvBootPinmuxConfig;

#define NVBOOT_PADCTL_SLAM_VAL_BDPGLP_1V8  \
    (APB_MISC_GP_GMECFGPADCTRL_0_RESET_VAL)

#define NVBOOT_PADCTL_SLAM_VAL_BDSDMEM_1V8 \
    (NV_DRF_NUM(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_CAL_DRVUP_SLWF, 0) | \
     NV_DRF_NUM(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_CAL_DRVDN_SLWR, 0) | \
     NV_DRF_NUM(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_CAL_DRVUP, 19) | \
     NV_DRF_NUM(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_CAL_DRVDN,24) | \
     NV_DRF_DEF(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_SCHMT_EN, DEFAULT) | \
     NV_DRF_DEF(APB_MISC_GP, SDIO3CFGPADCTRL, CFG2TMC_SDIO3CFG_HSM_EN, DEFAULT))

#define NVBOOT_PADCTL_SLAM_VAL_BDLPDDR2AD_1V2 \
    (NV_DRF_NUM(APB_MISC_GP, GMACFGPADCTRL, CFG2TMC_GMACFG_CAL_DRVUP_SLWF, 2) | \
     NV_DRF_NUM(APB_MISC_GP, GMACFGPADCTRL, CFG2TMC_GMACFG_CAL_DRVDN_SLWR, 2) | \
     NV_DRF_NUM(APB_MISC_GP, GMACFGPADCTRL, CFG2TMC_GMACFG_CAL_DRVUP, 8) | \
     NV_DRF_NUM(APB_MISC_GP, GMACFGPADCTRL, CFG2TMC_GMACFG_CAL_DRVDN, 8))

#define NVBOOT_PADCTL_SLAM_LIST(_op_) \
    /* VDDIO_CAM */ \
    _op_(GMECFG, BDPGLP_1V8) \
    _op_(GMFCFG, BDPGLP_1V8) \
    _op_(GMGCFG, BDPGLP_1V8) \
    _op_(GMHCFG, BDPGLP_1V8) \
    /* VDDIO_GMI */ \
    _op_(ATCFG1, BDPGLP_1V8) \
    _op_(ATCFG2, BDPGLP_1V8) \
    _op_(ATCFG3, BDPGLP_1V8) \
    _op_(ATCFG4, BDPGLP_1V8) \
    _op_(ATCFG5, BDPGLP_1V8) \
    /* VDDIO_SDMMC3 */ \
    _op_(SDIO2CFG, BDSDMEM_1V8) \
    _op_(SDIO3CFG, BDSDMEM_1V8) \
    /* VDDIO_SDMMC4 */ \
    _op_(GMACFG, BDLPDDR2AD_1V2) \
    _op_(GMBCFG, BDLPDDR2AD_1V2) \
    _op_(GMCCFG, BDLPDDR2AD_1V2) \
    _op_(GMDCFG, BDLPDDR2AD_1V2)

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
 * @retval none
 */
void
NvBootPadsExitDeepPowerDown(void);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVBOOT_PADS_INT_H
