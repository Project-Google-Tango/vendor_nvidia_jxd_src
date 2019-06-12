/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/*
 * nvboot_hacks_int.h - Definition of constants that disble or hardcode values
 * for the BootRom implementation to work around missing pieces during
 * development.  None of this code should appear in the final boot rom.
 */

#ifndef INCLUDED_NVBOOT_HACKS_INT_H
#define INCLUDED_NVBOOT_HACKS_INT_H

#if defined(__cplusplus)
extern "C"
{
#endif

/*
 * Temporary conditional compilation to disable the inclusion of new code.
 * Note that, in general, the earlier blocks must be enabled for the later
 * blocks to function.
 */

#define USE_BADBLOCKS   1
#define USE_STRAPS      1
#ifndef TARGET_ASIM
#define USE_FUSES       1
#endif
/* Set the following to 0 to disable USB RCM. */
#define USE_FULL_RCM    1

// By default, skip oscillator frequency measurement to speed RTL sims.
#define NVBOOT_SKIP_OSC_FREQ_IN_RTL 1

#define NVBOOT_SPIN_WAIT_AT_START 0
#define NVBOOT_SPIN_WAIT_AT_END   0
#define NVBOOT_SPIN_WAIT_ON_ERROR 1

#define NVBOOT_DEFAULT_PARAM_STRAP               0
#define NVBOOT_DEFAULT_SDRAM_STRAP               0
#define NVBOOT_DEFAULT_FORCE_RECOVERY_MODE_STRAP NV_FALSE
#define NVBOOT_DEFAULT_DEV_SEL_STRAP             NvBootStrapDevSel_EmmcPrimary

#define NVBOOT_DEFAULT_AVP_WARM_BOOT      NV_FALSE

// Values to use for fuses when USE_FUSES == 0.
#ifdef TARGET_ASIM
#define NVBOOT_DEFAULT_DEV_CONFIG_FUSES         0x11 // SDMMC_x8 & BOOT_MODE_OFF
#else
#define NVBOOT_DEFAULT_DEV_CONFIG_FUSES         0
#endif
#define NVBOOT_DEFAULT_DEV_SEL_FUSES            NvBootFuseBootDevice_Sdmmc
#define NVBOOT_DEFAULT_FA_FUSE                  NV_FALSE
#define NVBOOT_DEFAULT_PREPRODUCTION_FUSE       NV_TRUE
#define NVBOOT_DEFAULT_PRODUCTION_FUSE          NV_FALSE
#ifdef TARGET_ASIM
#define NVBOOT_DEFAULT_SKIP_DEV_SEL_STRAPS_FUSE NV_TRUE
#else
#define NVBOOT_DEFAULT_SKIP_DEV_SEL_STRAPS_FUSE NV_FALSE
#endif
#define NVBOOT_DEFAULT_ENABLE_CHARGER_DETECT_FUSE NV_FALSE
#define NVBOOT_DEFAULT_SW_RESERVED_FUSES        0
#define NVBOOT_DEFAULT_SKU_FUSES                NvBootSku_IdeOn_PcieOn

#define NVBOOT_DEFAULT_DK   0

#define NVBOOT_DEFAULT_SBK0 0
#define NVBOOT_DEFAULT_SBK1 0
#define NVBOOT_DEFAULT_SBK2 0
#define NVBOOT_DEFAULT_SBK3 0

// FUSE_PKC_DISABLE_0
#define NVBOOT_DEFAULT_FUSE_PKC_DISABLE 0

// FUSE_PUBLIC_KEY0_0 - FUSE_PUBLIC_KEY7_0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY0 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY1 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY2 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY3 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY4 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY5 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY6 0
#define NVBOOT_DEFAULT_FUSE_PUBLIC_KEY7 0

// Below defines are for UID
#define NVBOOT_DEFAULT_VENDOR_CODE 0
#define NVBOOT_DEFAULT_FAB_CODE 0
#define NVBOOT_DEFAULT_LOT0_CODE 0
#define NVBOOT_DEFAULT_LOT1_CODE 0
#define NVBOOT_DEFAULT_WAFER_ID 0
#define NVBOOT_DEFAULT_X_COORDINATE 0
#define NVBOOT_DEFAULT_Y_COORDINATE 0
#define NVBOOT_DEFAULT_RSVD1         0

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_HACKS_INT_H */
