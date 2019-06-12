/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef INCLUDED_NVAVP_SDMMC_T30_INT_H
#define INCLUDED_NVAVP_SDMMC_T30_INT_H

#if defined(__cplusplus)
extern "C"
{
#endif

#define EMMC_SWITCH_4BIT_DDR_BUS_WIDTH_ARG 0x03b70500
#define EMMC_SWITCH_8BIT_DDR_BUS_WIDTH_ARG 0x03b70600

#define EMMC_SWITCH_CARD_TYPE_ARG 0x03C20000
#define EMMC_SWITCH_INDEX_OFFSET 8

#define EMMC_SWITCH_FUNCTION_TYPE_ARG 0x80000000
#define EMMC_SWITCH_CURRENT_OFFSET 12
#define EMMC_SWITCH_DRIVESTRENGHT_OFFSET 8
#define EMMC_SWITCH_ACCESSMODE_OFFSET 0


#define EMMC_ECSD_BOOT_BUS_WIDTH    177

// Emmc Extended CSD fields v4.4 addition and updates
#define EMMC_ECSD_POWER_CL_DDR_52_360_OFFSET 239
#define EMMC_ECSD_POWER_CL_DDR_52_195_OFFSET 238
#define EMMC_ECSD_MIN_PERF_DDR_W_8_52_OFFSET 235
#define EMMC_ECSD_MIN_PERF_DDR_R_8_52_OFFSET 234

#define EMMC_ECSD_BOOT_INFO_OFFSET       228
#define EMMC_ECSD_CARD_CSD_STRUCT_OFFSET   194
#define EMMC_ECSD_CARD_EXT_CSD_REV_OFFSET   192

// EXT CSD_STRUCTURE REV versions
#define EMMC_ECSD_CARD_EXT_CSD_REV_15   5
#define EMMC_ECSD_CARD_EXT_CSD_REV_14   4
#define EMMC_ECSD_CARD_EXT_CSD_REV_13   3
#define EMMC_ECSD_CARD_EXT_CSD_REV_12   2
#define EMMC_ECSD_CARD_EXT_CSD_REV_11   1
#define EMMC_ECSD_CARD_EXT_CSD_REV_10   0

// High-Speed Dual Data Rate MultimediaCard @ 52MHz - 1.2V I/O
#define EMMC_ECSD_CT_HS_DDR_52_120 8 // card type high speed DDR 52 mhz @ 1.2v
#define EMMC_ECSD_CT_HS_DDR_52_180_300 4 // card type high speed DDR 52 @ either 1.8 or 3.0v
#define EMMC_ECSD_CT_HS_DDR_52_120_MASK             0x8
#define EMMC_ECSD_CT_HS_DDR_52_180_300_MASK     0x4
#define EMMC_ECSD_CT_HS_DDR_RANGE                       3:2
#define EMMC_ECSD_CT_HS_DDR_OFFSET                      2
#define EMMC_ECSD_CT_HS_DDR_MASK                        0xC
#define EMMC_ECSD_CT_HS_52 2
#define EMMC_ECSD_CT_HS_26 1

// BOOT_PARTITION_ENABLE
#define EMMC_ECSD_BC_BPE_RANGE              5:3
#define EMMC_ECSD_BC_BPE_OFFSET             3
#define EMMC_ECSD_BC_BPE_MASK               0x7
#define EMMC_ECSD_BC_BPE_NOTENABLED   0
#define EMMC_ECSD_BC_BPE_BAP1               1
#define EMMC_ECSD_BC_BPE_BAP2               2
#define EMMC_ECSD_BC_BPE_UAE                7

#define EMMC_CSD_UHS50_TRAN_SPEED 0x0B
#define EMMC_CSD_UHS104_TRAN_SPEED 0x2B

/// Emmc CSD fields
#define EMMC_CSD_0_CSD_STRUC_RANGE 23:22 // 119:1118 of response register.


/// Esd CSD Version 2 fields
#define EMMC_CSD_0_C_SIZE_V2_0_RANGE 29:8 // 61:40 of response register.

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVAVP_SDMMC_T30_INT_H */
