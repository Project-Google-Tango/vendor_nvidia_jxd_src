/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_FUSE_COMMON_H
#define INCLUDED_NVDDK_FUSE_COMMON_H

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

#define PMC_BASE        0x7000E400UL
#define MISC_PA_LEN  4096 // TODO: what is this value?
#define APBDEV_PMC_STRAPPING_OPT_A_0         0x464
#define APBDEV_PMC_STRAPPING_OPT_A_0_BOOT_SELECT_RANGE  29:26

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

#define FUSE_RESERVED_SW_0_BOOT_DEVICE_SELECT_RANGE       2:0
#define FUSE_RESERVED_SW_0_SKIP_DEV_SEL_STRAPS_RANGE      3:3
#define FUSE_RESERVED_SW_0_SW_RESERVED_RANGE              7:4
// Register FUSE_ARM_DEBUG_DIS_0
#define FUSE_ARM_DEBUG_DIS_0                    _MK_ADDR_CONST(0x1b8)

// Register FUSE_DISABLEREGPROGRAM_0
#define FUSE_DISABLEREGPROGRAM_0                        _MK_ADDR_CONST(0x2c)
#define FUSE_DISABLEREGPROGRAM_0_DISABLEREGPROGRAM_VAL_DISABLE                  _MK_ENUM_CONST(0)
#define FUSE_DISABLEREGPROGRAM_0_DISABLEREGPROGRAM_VAL_ENABLE                   _MK_ENUM_CONST(1)
#define FUSE_DISABLEREGPROGRAM_0_DISABLEREGPROGRAM_VAL_RANGE                    0:0

// Register FUSE_FUSECTRL_0
#define FUSE_FUSECTRL_0                 _MK_ADDR_CONST(0x0)
#define FUSE_FUSECTRL_0_FUSECTRL_FUSE_SENSE_DONE_RANGE                  30:30
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_RANGE                      1:0
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_SENSE_CTRL                 _MK_ENUM_CONST(3)
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_WRITE                      _MK_ENUM_CONST(2)
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_READ                       _MK_ENUM_CONST(1)
#define FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE                       _MK_ENUM_CONST(4)

// Register FUSE_FUSEADDR_0
#define FUSE_FUSEADDR_0                 _MK_ADDR_CONST(0x4)

// Register FUSE_FUSEWDATA_0
#define FUSE_FUSEWDATA_0                        _MK_ADDR_CONST(0xc)

// Register FUSE_FUSERDATA_0
#define FUSE_FUSERDATA_0                        _MK_ADDR_CONST(0x8)

// Register FUSE_PWR_GOOD_SW_0
#define FUSE_PWR_GOOD_SW_0                      _MK_ADDR_CONST(0x34)
#define FUSE_PWR_GOOD_SW_0_PWR_GOOD_SW_VAL_PWR_GOOD_OK                  _MK_ENUM_CONST(1)
#define FUSE_PWR_GOOD_SW_0_PWR_GOOD_SW_VAL_PWR_GOOD_NOT_OK                      _MK_ENUM_CONST(0)
#define FUSE_PWR_GOOD_SW_0_PWR_GOOD_SW_VAL_RANGE                        0:0

// Register FUSE_PRIV2INTFC_START_0
#define FUSE_PRIV2INTFC_START_0                 _MK_ADDR_CONST(0x20)
#define FUSE_PRIV2INTFC_START_0_PRIV2INTFC_START_DATA_RANGE                     0:0
#define FUSE_PRIV2INTFC_START_0_PRIV2INTFC_SKIP_RECORDS_RANGE                   1:1
#define FUSE_PRIV2INTFC_START_0_PRIV2INTFC_SKIP_RAMREPAIR_RANGE                 1:1

// Register FUSE_FUSETIME_PGM2_0
#define FUSE_FUSETIME_PGM2_0                    _MK_ADDR_CONST(0x1c)
#define FUSE_FUSETIME_PGM2_0_FUSETIME_PGM2_TWIDTH_PGM_RANGE                     15:0

#ifndef NV_ADDRESS_MAP_TMRUS_BASE
#define NV_ADDRESS_MAP_TMRUS_BASE                1610633232
#endif

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

// p = prefix, c = copy, i = index, d = data

#define FUSE_BASE(p, c, i) FUSE_##p##__##c##_ALIAS_##i
#define FUSE_DATA(p, c, i) FUSE_##p##__##c##_ALIAS_##i##_DATA
#define FUSE_WIDTH(p, c, i) FUSE_##p##__##c##_ALIAS_##i##_WIDTH

#define SET_FUSE(p, c, i, d)                       \
    s_FuseArray[FUSE_BASE(p, c, i)] =              \
    (s_FuseArray[FUSE_BASE(p, c, i)] &             \
    ~NV_FIELD_SHIFTMASK(FUSE_DATA(p, c, i))) |     \
    ((d & (NV_FIELD_MASK(FUSE_DATA(p, c, i)))) <<  \
    (NV_FIELD_SHIFT(FUSE_DATA(p, c, i))))

#define SET_MASK(p, c, i) \
    s_MaskArray[FUSE_BASE(p,c,i)] |= NV_FIELD_SHIFTMASK(FUSE_DATA(p,c,i))

#define SET_FUSE_DATA(p, c, i, d) \
    SET_FUSE(p,c,i,d);            \
    SET_MASK(p,c,i);

#define SET_FUSE_PRI(p, i, d) SET_FUSE_DATA(p, PRI, i, d);
#define SET_FUSE_RED(p, i, d) SET_FUSE_DATA(p, RED, i, d);

#define SET_FUSE_BOTH(p, i, d)    \
    SET_FUSE_PRI(p,i,d);  \
    SET_FUSE_RED(p,i,d);

// Update fuses with an explicit mask.
#define SET_FUSE_BOTH_WITH_MASK(p, i, d, md)         \
    SET_FUSE(p, PRI, i, d);                       \
    SET_FUSE(p, RED, i, d);                       \
    s_MaskArray[FUSE_BASE(p, PRI, i)] |=          \
      (md << NV_FIELD_SHIFT(FUSE_DATA(p, PRI, i))); \
    s_MaskArray[FUSE_BASE(p, RED, i)] |=          \
      (md << NV_FIELD_SHIFT(FUSE_DATA(p, RED, i)));

#define UPDATE_DATA(p,i,d) (d >>= FUSE_WIDTH(p,PRI,i));

#define SET_SPARE_BIT(n)                   \
    SET_FUSE_PRI(SPARE_BIT_##n, 0, Data);  \
    UPDATE_DATA (SPARE_BIT_##n, 0, Data);

/* Programming flags */
#define FLAGS_PROGRAM_SECURE_BOOT_KEY     0x001
#define FLAGS_PROGRAM_DEVICE_KEY          0x002
#define FLAGS_PROGRAM_JTAG_DISABLE        0x004
#define FLAGS_PROGRAM_BOOT_DEV_SEL        0x008
#define FLAGS_PROGRAM_BOOT_DEV_CONFIG     0x010
#define FLAGS_PROGRAM_SW_RESERVED         0x020
#define FLAGS_PROGRAM_ODM_PRODUCTION      0x040
#define FLAGS_PROGRAM_SPARE_BITS          0x080
#define FLAGS_PROGRAM_SKIP_DEV_SEL_STRAPS 0x100
#define FLAGS_PROGRAM_RESERVED_ODM 0x200
#define FLAGS_PROGRAM_PKC_DISABLE 0x400
#define FLAGS_PROGRAM_VP8_ENABLE 0x800
#define FLAGS_PROGRAM_ODM_LOCK 0x1000
#define FLAGS_PROGRAM_PUBLIC_KEY_HASH 0x2000
#define FLAGS_PROGRAM_ODM_INFO 0x4000

#define TEGRA_AGE_0_6 FUSE_SPARE_BIT_34_0 /*Spare bit 34*/
#define TEGRA_AGE_1_6 FUSE_SPARE_BIT_49_0 /*Spare bit 49*/
#define TEGRA_AGE_0_5 FUSE_SPARE_BIT_33_0 /*Spare bit 33*/
#define TEGRA_AGE_1_5 FUSE_SPARE_BIT_48_0 /*Spare bit 48*/
#define TEGRA_AGE_0_4 FUSE_SPARE_BIT_32_0 /*Spare bit 32*/
#define TEGRA_AGE_1_4 FUSE_SPARE_BIT_47_0 /*Spare bit 47*/
#define TEGRA_AGE_0_3 FUSE_SPARE_BIT_31_0 /*Spare bit 31*/
#define TEGRA_AGE_1_3 FUSE_SPARE_BIT_46_0 /*Spare bit 46*/
#define TEGRA_AGE_0_2 FUSE_SPARE_BIT_30_0 /*Spare bit 30*/
#define TEGRA_AGE_1_2 FUSE_SPARE_BIT_45_0 /*Spare bit 45*/
#define TEGRA_AGE_0_1 FUSE_SPARE_BIT_29_0 /*Spare bit 29*/
#define TEGRA_AGE_1_1 FUSE_SPARE_BIT_44_0 /*Spare bit 44*/
#define TEGRA_AGE_0_0 FUSE_SPARE_BIT_28_0 /*Spare bit 28*/
#define TEGRA_AGE_1_0 FUSE_SPARE_BIT_43_0 /*Spare bit 43*/

/**
 * Schedule fuses to be programmed to the specified values when the next
 * NvDdkFuseProgram() operation is performed
 *
 * By passing a size of zero, the caller is requesting to be told the
 * expected size.
 */
#define FUSE_SET_PRIV(name, flag)                                              \
    case NvDdkFuseDataType_##name:                                         \
    {                                                                     \
        NvU8 *p = (NvU8 *)&((*p_FuseData).name);                           \
        NvU32 i;                                                          \
        /* read existing fuse value */                                    \
        NV_CHECK_ERROR(NvDdkFuseGet(NvDdkFuseDataType_##name, p, &Size));   \
        /* check consistency between existing and desired fuse values. */ \
        /* fuses cannot be unburned, so desired value cannot specify   */ \
        /* any unburned (0x0) bits where the existing value already    */ \
        /* contains burned (0x1) bits.                                 */ \
        for (i=0; i<Size; i++)                                            \
            if ((p[i] | ((NvU8*)*pData)[i]) != ((NvU8*)*pData)[i])          \
                NV_CHECK_ERROR(NvError_InvalidState);                     \
        /* consistency check passed; schedule fuses to be burned */       \
        fusememcpy(&(s_FuseData.name), *pData, Size);                    \
        s_FuseData.ProgramFlags |= FLAGS_PROGRAM_##flag;                  \
    }                                                                     \
    break

#define FUSE_READ_AGE_BIT(n, bit, age)     \
    bit = FUSE_NV_READ32(TEGRA_AGE_0_##n);\
    bit |= FUSE_NV_READ32(TEGRA_AGE_1_##n);\
    bit = bit << n;\
    age |= bit;\


// Utility function defs
NvBool IsSbkOrDkSet(void);

NvBool NvDdkFuseIsSbkSet(void);

NvBool NvDdkFuseIsFailureAnalysisMode(void);

NvBool NvDdkFuseIsOdmProductionModeFuseSet(void);

NvBool NvDdkFuseIsNvProductionModeFuseSet(void);

NvError NvDdkFuseCopyBytes(NvU32 RegAddress, NvU8 *pByte, const NvU32 nBytes);

NvU32 NvDdkFuseGetBootDevInfo(void);

NvBool NvDdkFuseSkipDevSelStraps(void);

void SetFuseRegVisibility(NvU32 Visibility);

NvU32 NvFuseUtilGetTimeUS( void );

void NvFuseUtilWaitUS( NvU32 usec );

NvBool NvDdkPrivIsDisableRegProgramSet(void);

void fusememset(void* Source, NvU8 val, NvU32 size);

void fusememcpy(void* Destination, void* Source, NvU32 size);

#ifdef __cplusplus
}
#endif

#endif
