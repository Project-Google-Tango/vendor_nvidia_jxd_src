/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_ARFUSE_COMMON_H
#define INCLUDED_NVDDK_ARFUSE_COMMON_H

#if defined(__cplusplus)
extern "C"
{
#endif

#ifndef _MK_SHIFT_CONST
   #define _MK_SHIFT_CONST(_constant_) _constant_
#endif
#ifndef _MK_MASK_CONST
   #define _MK_MASK_CONST(_constant_) _constant_
#endif
#ifndef _MK_ENUM_CONST
   #define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif
#ifndef _MK_ADDR_CONST
   #define _MK_ADDR_CONST(_constant_) _constant_
#endif

// Common fuse macros across all chips
#define NV_ADDRESS_MAP_FUSE_BASE            0x7000F800
#define NV_ADDRESS_MAP_CAR_BASE             0x60006000
#define MISC_PA_BASE        0x70000000UL
#define AR_APB_MISC_BASE_ADDRESS 0x70000000
#define MISC_PA_LEN  4096
#define APB_MISC_PP_STRAPPING_OPT_A_0  0x8
#define APB_MISC_PP_STRAPPING_OPT_A_0_BOOT_SELECT_RANGE  29:26
#define SDMMC_CONTROLLER_INSTANCE_2 2
#define SDMMC_CONTROLLER_INSTANCE_3 3

// Macros for common functions (offsets expected to stay the same across chips)
#define FUSE_RESERVED_SW_0                      _MK_ADDR_CONST(0x1c0)
#define FUSE_RESERVED_SW_0_SKIP_DEV_SEL_STRAPS_RANGE      3:3
#define FUSE_PRIVATE_KEY0_NONZERO_0                     _MK_ADDR_CONST(0x50)
#define FUSE_PRIVATE_KEY1_NONZERO_0                     _MK_ADDR_CONST(0x54)
#define FUSE_PRIVATE_KEY2_NONZERO_0                     _MK_ADDR_CONST(0x58)
#define FUSE_PRIVATE_KEY3_NONZERO_0                     _MK_ADDR_CONST(0x5c)
#define FUSE_PRIVATE_KEY4_NONZERO_0                     _MK_ADDR_CONST(0x60)
// Register FUSE_FA_0
#define FUSE_FA_0                       _MK_ADDR_CONST(0x148)
// Register FUSE_PRODUCTION_MODE_0
#define FUSE_PRODUCTION_MODE_0                  _MK_ADDR_CONST(0x100)
// Register FUSE_RESERVED_ODM0_0
#define FUSE_RESERVED_ODM0_0                    _MK_ADDR_CONST(0x1c8)
#define CLK_RST_CONTROLLER_MISC_CLK_ENB_0                       _MK_ADDR_CONST(0x48)
#define CLK_RST_CONTROLLER_MISC_CLK_ENB_0_CFG_ALL_VISIBLE_RANGE                 28:28
#define FUSE_SECURITY_MODE_0                            _MK_ADDR_CONST(0x1a0)
#define APB_MISC_GP_HIDREV_0                    _MK_ADDR_CONST(0x804)
#define APB_MISC_GP_HIDREV_0_CHIPID_RANGE                       15:8
#define FUSE_BOOT_DEVICE_INFO_0                 _MK_ADDR_CONST(0x1bc)
// Register FUSE_PRIVATE_KEY4_0
#define FUSE_PRIVATE_KEY4_0                     _MK_ADDR_CONST(0x1b4)
// Register FUSE_SKU_INFO_0
#define FUSE_SKU_INFO_0                 _MK_ADDR_CONST(0x110)
#define FUSE_RESERVED_SW_0_BOOT_DEVICE_SELECT_RANGE       2:0
#define FUSE_RESERVED_SW_0_SKIP_DEV_SEL_STRAPS_RANGE      3:3
#define FUSE_RESERVED_SW_0_SW_RESERVED_RANGE              7:4
// Register FUSE_ARM_DEBUG_DIS_0
#define FUSE_ARM_DEBUG_DIS_0                    _MK_ADDR_CONST(0x1b8)


// Utility Macros

#define FUSE_NV_READ32(offset)\
        NV_READ32(NV_ADDRESS_MAP_FUSE_BASE + offset)

#define FUSE_NV_WRITE32(offset, val)\
        NV_WRITE32(NV_ADDRESS_MAP_FUSE_BASE + offset, val)

#define CLOCK_NV_READ32(offset)\
        NV_READ32(NV_ADDRESS_MAP_CAR_BASE + offset)

#define CLOCK_NV_WRITE32(offset, val)\
        NV_WRITE32(NV_ADDRESS_MAP_CAR_BASE + offset, val)

#define EXTRACT_BYTE_NVU32(Data32, ByteNum) \
    (((Data32) >> ((ByteNum)*8)) & 0xFF)


#define SWAP_BYTES_NVU32(Data32)                    \
    do {                                            \
        NV_ASSERT(sizeof(Data32)==4);               \
        (Data32) =                                  \
            (EXTRACT_BYTE_NVU32(Data32, 0) << 24) | \
            (EXTRACT_BYTE_NVU32(Data32, 1) << 16) | \
            (EXTRACT_BYTE_NVU32(Data32, 2) <<  8) | \
            (EXTRACT_BYTE_NVU32(Data32, 3) <<  0);  \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif
