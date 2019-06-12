/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */


#include "nvcommon.h"
#include "nvrm_drf.h"
#include "nvrm_module.h"
#include "nvrm_clocks.h"
#include "nvrm_hardware_access.h"
#include "nvaboot_private.h"
#include "nvodm_services.h"
#include "nvboot_warm_boot_0.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_cipher.h"
#include "t11x/nvboot_sdram_param.h"
#include "t11x/arapbpm.h"
#include "t11x/arapb_misc.h"
#include "t11x/arclk_rst.h"
#include "arflow_ctlr.h"
#include "t11x/aremc.h"
#include "t11x/armc.h"
#include "t11x/arfuse.h"
#include "t11x/arvde2x.h"
#include "t11x/nvboot_bct.h"
#include "t11x/nvboot_bit.h"
#include "t11x/nvboot_pmc_scratch_map.h"
#include "t11x/nvboot_wb0_sdram_scratch_list.h"
#include "nvbl_memmap_nvap.h"
#include "nvbl_query.h"

#define NV_ADDRESS_MAP_PMC_BASE         1879106560


#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KVCO_RANGE 20:20
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KCP_RANGE  22:21
//Correct name->Broken name from nvboot_pmc_scratch_map.h (see bug 738161)
#define CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KVCO_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KVCO_RANGE
#define CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KCP_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_MISC_3_0_PLLX_KCP_RANGE

#define MAX_DEBUG_CARVEOUT_SIZE_ALLOWED (256*1024*1024)

#define VPR_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define TSEC_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define XUSB_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define DEBUG_CARVEOUT_ADDR_ALIGNMENT 0X100000

/* Register prefix to RM module id mapping macros. */
#define AHB_ModuleID                NvRmPrivModuleID_Ahb_Arb_Ctrl
#define CLK_RST_CONTROLLER_ModuleID NvRmPrivModuleID_ClockAndReset
#define EMC_ModuleID                NvRmPrivModuleID_ExternalMemoryController
#define MC_ModuleID                 NvRmPrivModuleID_MemoryController

extern void NvBlRamRepairCluster0(void);
extern NvAbootMemLayoutRec NvAbootMemLayout[NvAbootMemLayout_Num];

/** SECURE_SCRATCH_REGS() - PMC secure scratch registers
    (list of SECURE_SCRATCH_REG() macros).
    SECURE_SCRATCH_REG(n) - PMC secure scratch register name:

    @param n Scratch register number (APBDEV_PMC_SECURE_SCRATCHs)
 */
#define SECURE_SCRATCH_REGS()  \
        SECURE_SCRATCH_REG(8)  \
        SECURE_SCRATCH_REG(9)  \
        SECURE_SCRATCH_REG(10) \
        SECURE_SCRATCH_REG(11) \
        SECURE_SCRATCH_REG(12) \
        SECURE_SCRATCH_REG(13) \
        SECURE_SCRATCH_REG(14) \
        SECURE_SCRATCH_REG(15) \
        SECURE_SCRATCH_REG(16) \
        SECURE_SCRATCH_REG(17) \
        SECURE_SCRATCH_REG(18) \
        SECURE_SCRATCH_REG(19) \
        SECURE_SCRATCH_REG(20) \
        /* End-of-List*/

void NvAbootReset(NvAbootHandle hAboot)
{
    NvU32 reg = NV_DRF_DEF(APBDEV_PMC, CNTRL, MAIN_RST, ENABLE);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_CNTRL_0, reg);
}

NvError NvAbootPrivSaveSdramParams(NvAbootHandle hAboot)
{
    NvError                 e = NvSuccess;
    NvU32                   Reg;            //Module register contents
    NvU32                   RamCode;        //BCT RAM selector
    NvBctHandle             hBct = NULL;
    NvU32                   Size, Instance;
    NvU32                   Reg_base0 = 0;
    NvU32                   Reg_misc1 = 0;
    NvU32                   Reg_misc2 = 0;
    NvBootSdramParams       SdramParams;
    NvU32                   RegVal;
    Size = 0;
    Instance = 0;

    // Read strap register and extract the RAM selection code.
    // NOTE: The boot ROM limits the RamCode values to the range 0-3.
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Misc, 0),
        APB_MISC_PP_STRAPPING_OPT_A_0);
    RamCode = NV_DRF_VAL(APB_MISC, PP_STRAPPING_OPT_A, RAM_CODE, Reg) & 3;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
            &Size, &Instance, NULL)
    );

    if (RamCode > Instance)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
            &Size, &RamCode, (NvU8 *)&SdramParams)
    );

/**
  * PACK_FIELD - Reads a PMC scratch register value to a local variable,
  *              modifies it by pulling a field out of an SDRAM
  *              parameter structure and writes back the register value
  *              to that PMC scratch register.
  *
  *     Note: For best efficiency, all fields for a scratch register should
  *           be grouped together in the list macro.
  *     Note: The compiler should optimize away redundant work in optimized
  *           builds.
  *
  *   @param n scratch register number
  *   @param d register domain (hardware block)
  *   @param r register name
  *   @param f register field
  *   @param p field name in NvBootSdramParams structure
  */
#define PACK_FIELD(n, c, d, r, f, p)                                          \
do {                                                                          \
    RegVal = NV_READ32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SCRATCH##n##_0);  \
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH##n, c##_##d##_##r##_0_##f,\
                                NV_DRF_VAL(d, r, f, SdramParams.p),           \
                                RegVal);                                      \
    NV_WRITE32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SCRATCH##n##_0, RegVal);  \
} while (0);

/**
  * PACK_VALUE - Reads a PMC scratch register value to a local variable,
  *              modifies it by pulling a variable out of an SDRAM
  *              parameter structure and writes back the register value
  *              to that PMC scratch register.
  *
  * @param n scratch register number
  * @param t data type of the SDRAM parameter (unused)
  * @param f the name of the SDRAM parameter
  * @param p field name in NvBootSdramParams structure
  */
#define PACK_VALUE(n, t, f, p)                                                 \
do {                                                                           \
    RegVal = NV_READ32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SCRATCH##n##_0);   \
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH##n, f,                     \
                                SdramParams.p,                                 \
                                RegVal);                                       \
    NV_WRITE32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SCRATCH##n##_0, RegVal);   \
} while (0);

/**
  * PACK_FIELD - Reads a PMC secure scratch register value to a local
  *              variable, modifies it by pulling a field out of an SDRAM
  *              parameter structure and writes back the register value
  *              to that PMC secure scratch register.
  *
  *   @param n scratch register number
  *   @param d register domain (hardware block)
  *   @param r register name
  *   @param f register field
  *   @param p field name in NvBootSdramParams structure
  */
#define PACK_SECURE_FIELD(n, c, d, r, f, p)                                          \
do {                                                                                 \
    RegVal = NV_READ32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SECURE_SCRATCH##n##_0);  \
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SECURE_SCRATCH##n, c##_##d##_##r##_0_##f,\
                                NV_DRF_VAL(d, r, f, SdramParams.p),                  \
                                RegVal);                                             \
    NV_WRITE32(NV_ADDRESS_MAP_PMC_BASE + APBDEV_PMC_SECURE_SCRATCH##n##_0, RegVal);  \
} while (0);

/**
  * Encode bit positions and set SwizzleRankByteEncode, if
  * bit 6 select is greater than bit 7 select
  */
#define ENCODE_BIT_POS_C(_c_, _c2_, _r_, _b_) \
    SdramParams.SwizzleRankByteEncode = \
      NV_FLD_SET_DRF_NUM(SWIZZLE, BIT6_GT_BIT7, _c2_##_RANK##_r_##_BYTE##_b_,              \
      NV_DRF_VAL(EMC, SWIZZLE_RANK##_r_##_BYTE##_b_, SWZ_RANK##_r_##_BYTE##_b_##_BIT6_SEL, \
                 SdramParams. _c_##EmcSwizzleRank##_r_##Byte##_b_) >                       \
      NV_DRF_VAL(EMC, SWIZZLE_RANK##_r_##_BYTE##_b_, SWZ_RANK##_r_##_BYTE##_b_##_BIT7_SEL, \
                 SdramParams. _c_##EmcSwizzleRank##_r_##Byte##_b_),                        \
      SdramParams.SwizzleRankByteEncode);
#define ENCODE_BIT_POS_R(_r_,_b_) \
    ENCODE_BIT_POS_C(,CH0,_r_,_b_) \
    ENCODE_BIT_POS_C(Ch1,CH1,_r_,_b_)
#define ENCODE_BIT_POS_B(_b_) \
    ENCODE_BIT_POS_R(0, _b_) \
    ENCODE_BIT_POS_R(1, _b_)
#define ENCODE_BIT_POS \
    ENCODE_BIT_POS_B(0) \
    ENCODE_BIT_POS_B(1) \
    ENCODE_BIT_POS_B(2) \
    ENCODE_BIT_POS_B(3)

    SdramParams.SwizzleRankByteEncode = 0;
    ENCODE_BIT_POS

#undef ENCODE_BIT_POS
#undef ENCODE_BIT_POS_B
#undef ENCODE_BIT_POS_R
#undef ENCODE_BIT_POS_C

    SDRAM_PMC_FIELDS(PACK_FIELD);
    SDRAM_PMC_VALUES(PACK_VALUE);

    if (SdramParams.MemoryType == NvBootMemoryType_Ddr3)
    {
        DDR3_PMC_FIELDS(PACK_FIELD);
    }
    else if (SdramParams.MemoryType == NvBootMemoryType_LpDdr2)
    {
        LPDDR2_PMC_FIELDS(PACK_FIELD);
    }

    #undef PACK_FIELD
    #undef PACK_VALUE

    // Pack secure scratch registers
    SDRAM_PMC_SECURE_FIELDS(PACK_SECURE_FIELD);
    #undef PACK_SECURE_FIELD

    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_PLLP_WB0_OVERRIDE_0);
    if (!(NV_DRF_VAL(APBDEV_PMC, PLLP_WB0_OVERRIDE, PLLM_OVERRIDE_ENABLE, Reg)))
    {
        Reg_base0 = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),\
                CLK_RST_CONTROLLER_PLLM_BASE_0);
        Reg_misc2 = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),\
                CLK_RST_CONTROLLER_PLLM_MISC2_0);

        Reg = NV_DRF_NUM(APBDEV_PMC, SCRATCH2, CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVM, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVM, Reg_base0)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH2, CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVN, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIVN, Reg_base0)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH2, CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIV2,
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_BASE, PLLM_DIV2, Reg_base0)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH2, CLK_RST_CONTROLLER_PLLM_MISC2_0_PLLM_KVCO, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC2, PLLM_KVCO, Reg_misc2)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH2, CLK_RST_CONTROLLER_PLLM_MISC2_0_PLLM_KCP, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC2, PLLM_KCP, Reg_misc2));
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
            APBDEV_PMC_SCRATCH2_0, Reg);

        Reg_misc1 = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),\
                CLK_RST_CONTROLLER_PLLM_MISC1_0);
        Reg = NV_DRF_NUM(APBDEV_PMC, SCRATCH35, CLK_RST_CONTROLLER_PLLM_MISC1_0_PLLM_SETUP, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_SETUP, Reg_misc1)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH35, CLK_RST_CONTROLLER_PLLM_MISC1_0_PLLM_PD_LSHIFT_PH135, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH135, Reg_misc1)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH35, CLK_RST_CONTROLLER_PLLM_MISC1_0_PLLM_PD_LSHIFT_PH90, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH90, Reg_misc1)) |
              NV_DRF_NUM(APBDEV_PMC, SCRATCH35, CLK_RST_CONTROLLER_PLLM_MISC1_0_PLLM_PD_LSHIFT_PH45, \
                  NV_DRF_VAL(CLK_RST_CONTROLLER, PLLM_MISC1, PLLM_PD_LSHIFT_PH45, Reg_misc1));
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
            APBDEV_PMC_SCRATCH35_0, Reg);
    }
    RegVal = 0;
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH3, CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVM, \
                                SdramParams.PllMInputDivider, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH3, CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVN, \
                                0x3e, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH3, CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVP, \
                                0 /* SdramParams.PllMSelectDiv2 */, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH3, CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_KVCO, \
                                SdramParams.PllMKVCO, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH3, CLK_RST_CONTROLLER_PLLX_MISC_3_0_PLLX_KCP, \
                                SdramParams.PllMKCP, RegVal);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH3_0, RegVal);

    RegVal = 0;
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH36, CLK_RST_CONTROLLER_PLLX_MISC_1_0_PLLX_SETUP, \
                                SdramParams.PllMSetupControl, RegVal);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH36_0, RegVal);


    RegVal = 0;
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH4, PLLX_STABLE_TIME, \
                                SdramParams.PllMStableTime, RegVal);
    RegVal = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH4, PLLM_STABLE_TIME, \
                                SdramParams.PllMStableTime, RegVal);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),        \
        APBDEV_PMC_SCRATCH4_0, RegVal);


    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_PLLM_WB0_OVERRIDE2_0);
    Reg = NV_FLD_SET_DRF_NUM(APBDEV_PMC,PLLM_WB0_OVERRIDE2,DIV2,SdramParams.PllMSelectDiv2,Reg);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_PLLM_WB0_OVERRIDE2_0, Reg);

    // Enable PLLM auto restart
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_PLLP_WB0_OVERRIDE_0);
    Reg |= NV_DRF_NUM(APBDEV_PMC, PLLP_WB0_OVERRIDE, PLLM_OVERRIDE_ENABLE, 1);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_PLLP_WB0_OVERRIDE_0, Reg);

fail:
    NvBctDeinit(hBct);
    return e;
}

NvError NvAbootLockSecureScratchRegs(NvAbootHandle hAboot)
{
    NvU32 Reg;
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    // Locking PMC secure scratch register (4 ~ 20) for writing
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0);

#define SECURE_SCRATCH_REG(n)\
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE2,WRITE##n,ON,Reg);
    SECURE_SCRATCH_REGS()
#undef SECURE_SCRATCH_REG
#undef SECURE_SCRATCH_REGS

    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0, Reg);
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE_0);
#define SECURE_SCRATCH_REGS()  \
        SECURE_SCRATCH_REG(4)  \
        SECURE_SCRATCH_REG(5)  \
        SECURE_SCRATCH_REG(6) \
        SECURE_SCRATCH_REG(7) \

#define SECURE_SCRATCH_REG(n)\
       Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE,WRITE##n,ON,Reg);\
       Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE,READ##n,ON,Reg);
    SECURE_SCRATCH_REGS()
#undef SECURE_SCRATCH_REG
#undef SECURE_SCRATCH_REGS

    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                           APBDEV_PMC_SEC_DISABLE_0, Reg);

fail:
    return e;
}

NvError NvAbootPrivSaveFusePrivateKeyDisable(NvAbootHandle hAboot)
{
    NvError                 e = NvSuccess;
    NvU32                   Reg;            //Module register contents


    // Fuse PrivateKeyDisable
    Reg = 0;
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Fuse, 0),\
                  FUSE_PRIVATEKEYDISABLE_0);
    Reg &= NV_DRF_NUM(FUSE, PRIVATEKEYDISABLE, TZ_STICKY_BIT_VAL, 1);
    Reg |= NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),        \
        APBDEV_PMC_SECURE_SCRATCH21_0);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),        \
        APBDEV_PMC_SECURE_SCRATCH21_0, Reg);

    // Lock PMC secure scratch register (21) for writing
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0);

    #define SECURE_SCRATCH_REG(n)\
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE2,WRITE##n,ON,Reg);
    SECURE_SCRATCH_REG(21)
    #undef SECURE_SCRATCH_REG

    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0, Reg);

    return e;
}

NvDdkBlockDevMgrDeviceId NvAbootGetSecBootDevId(NvU32 *pInstance)
{
    NvDdkBlockDevMgrDeviceId DdkDeviceId = NvDdkBlockDevMgrDeviceId_Invalid;
    NvU32 Instance;
    NvU32 BootDevice;
    NvU32 Size;
    NvError e;

    if (pInstance == NULL)
        return NvDdkBlockDevMgrDeviceId_Invalid;

    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDeviceSelect,
                (void *)&BootDevice, (NvU32 *)&Size));
    Size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(NvDdkFuseGet(NvDdkFuseDataType_SecBootDevInst,
                (void *)&Instance, (NvU32 *)&Size));

    switch (BootDevice)
    {
    case NvBootDevType_Nand_x8:
    case NvBootDevType_Nand_x16:
        DdkDeviceId = NvDdkBlockDevMgrDeviceId_Nand;
        break;
    case NvBootDevType_Sdmmc:
        DdkDeviceId = NvDdkBlockDevMgrDeviceId_SDMMC;
        break;
    case NvBootDevType_Spi:
        DdkDeviceId = NvDdkBlockDevMgrDeviceId_Spi;
        break;
    case NvBootDevType_Nor:
        DdkDeviceId = NvDdkBlockDevMgrDeviceId_Nor;
        break;
    case NvBootDevType_Usb3:
        DdkDeviceId = NvDdkBlockDevMgrDeviceId_Usb3;
        break;
    default:
        break;
    }

    *pInstance = Instance;
fail:
    return DdkDeviceId;
}

NvError NvAbootReadSign(NvCryptoHashAlgoHandle pAlgo,
                        NvU32 *pHashSize,
                        NvU32 Source)
{
    NvError e = NvError_NotInitialized;
    e = pAlgo->QueryHash(pAlgo, pHashSize,
            (NvU8*)(Source + offsetof(NvBootWb0RecoveryHeader, Signature.CryptoHash)));
    return e;
}

void NvAbootClearSign(NvBootWb0RecoveryHeader *pDstHeader)
{
    NvOsMemset(&(pDstHeader->Signature), 0, sizeof(pDstHeader->Signature));
}

void LockVprAperture(NvAbootHandle hAboot)
{
    NvBootSdramParams SdramParams;
    NvError e = NvSuccess;
    NvBctHandle hBct = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU32 regval = 0;
    NvU32 i = 0;

    // Get SDParams from BCT
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
            &Size, &Instance, NULL)
    );

    for (i = 0; i < Instance; i++)
    {
        NvOsMemset((void*)&SdramParams, 0x0, sizeof(SdramParams));
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
                &Size, &i, (NvU8 *)&SdramParams)
        );

        // Check if the Write access to VPR aperture in MC is disabled
        if (SdramParams.McVideoProtectWriteAccess)
        {
            /* --------> BOOM
             * Control should not come here. As per the bug http://nvbugs/1028903
             * BCT should always allow bootloader to update the VPR aperture
             * in MC and PMC registers.
             */
            goto fail;
        }

        // Update the aperture information of VPR to MC & PMC
        // MC
        // Instance 0       : MC0
        // Instance 1 & 2   : MCB (Writes to this will be broadcasted to both MC0 and MC1
        // Instance 3       : MC1
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_VIDEO_PROTECT_BOM_0,NvAbootMemLayout[NvAbootMemLayout_Vpr].Base);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_VIDEO_PROTECT_SIZE_MB_0,NvAbootMemLayout[NvAbootMemLayout_Vpr].Size >> 20);
        // Disable further write access
        regval = NV_DRF_DEF(MC,VIDEO_PROTECT_REG_CTRL,VIDEO_PROTECT_WRITE_ACCESS,DISABLED);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_VIDEO_PROTECT_REG_CTRL_0,regval);

        // PMC (will be updated to PMC registers in NvAbootPrivSaveSdramParams())
        SdramParams.McVideoProtectBom = NvAbootMemLayout[NvAbootMemLayout_Vpr].Base;
        SdramParams.McVideoProtectSizeMb = NvAbootMemLayout[NvAbootMemLayout_Vpr].Size >> 20;
        SdramParams.McVideoProtectWriteAccess =
                    MC_VIDEO_PROTECT_REG_CTRL_0_VIDEO_PROTECT_WRITE_ACCESS_DISABLED;
        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(hBct, NvBctDataType_SdramConfigInfo,
                &Size, &i, (NvU8 *)&SdramParams)
        );
    }
    return;
fail:
    NvAbootReset(hAboot);
}

void LockTsecAperture(NvAbootHandle hAboot)
{
    NvBootSdramParams SdramParams;
    NvError e = NvSuccess;
    NvBctHandle hBct = NULL;
    NvU32 Size = 0;
    NvU32 Instance = 0;
    NvU32 regval = 0;
    NvU32 i = 0;

    // Get SDParams from BCT
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&Size, NULL, &hBct));

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
            &Size, &Instance, NULL)
    );

    for (i = 0; i < Instance; i++)
    {

        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(hBct, NvBctDataType_SdramConfigInfo,
                &Size, &i, (NvU8 *)&SdramParams)
        );
        // Check if the Write access to TSEC aperture in MC is disabled
        if (SdramParams.McSecCarveoutProtectWriteAccess)
        {
            /* --------> BOOM
             * Control should not come here. As per the bug http://nvbugs/1028903
             * BCT should always allow bootloader to update the TSEC aperture
             * in MC and PMC registers.
             */
            goto fail;
        }

        // Update the aperture information of TSEC to MC & PMC
        // MC
        // Instance 0       : MC0
        // Instance 1 & 2   : MCB (Writes to this will be broadcasted to both MC0 and MC1
        // Instance 3       : MC1
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_SEC_CARVEOUT_BOM_0,NvAbootMemLayout[NvAbootMemLayout_Tsec].Base);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_SEC_CARVEOUT_SIZE_MB_0,NvAbootMemLayout[NvAbootMemLayout_Tsec].Size >> 20);
        // Disable further write access
        regval = NV_DRF_DEF(MC,SEC_CARVEOUT_REG_CTRL,SEC_CARVEOUT_WRITE_ACCESS,DISABLED);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 1), \
            MC_SEC_CARVEOUT_REG_CTRL_0,regval);

        // PMC (will be updated to PMC registers in NvAbootPrivSaveSdramParams())
        SdramParams.McSecCarveoutBom = NvAbootMemLayout[NvAbootMemLayout_Tsec].Base;
        SdramParams.McSecCarveoutSizeMb = NvAbootMemLayout[NvAbootMemLayout_Tsec].Size >> 20;
        SdramParams.McSecCarveoutProtectWriteAccess =
                      MC_SEC_CARVEOUT_REG_CTRL_0_SEC_CARVEOUT_WRITE_ACCESS_DISABLED;
        NV_CHECK_ERROR_CLEANUP(
            NvBctSetData(hBct, NvBctDataType_SdramConfigInfo,
                &Size, &i, (NvU8 *)&SdramParams)
        );
    }
    return;
fail:
    NvAbootReset(hAboot);
}

NvBool NvAbootConfigChipSpecificMem(NvAbootHandle hAboot,
                                  NvOdmOsOsInfo *pOsInfo,
                                  NvU32 *pBankSize,
                                  NvU32 *pMemSize,
                                  NvU32 DramBottom)
{
#ifndef BOOT_MINIMAL_BL
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T11X_BASE_PA_BOOT_INFO);

    if (!hAboot || !pOsInfo || !pBankSize || !pMemSize)
    {
        return NV_FALSE;
    }

#if ENABLE_NVDUMPER
    /* Reduce the available by what RAM dump is using */
    NvAbootMemLayout[NvAbootMemLayout_RamDump].Size = 1*1024*1024; // 1MB
    *pBankSize -= NvAbootMemLayout[NvAbootMemLayout_RamDump].Size;
    *pMemSize  -= NvAbootMemLayout[NvAbootMemLayout_RamDump].Size;
    NvAbootMemLayout[NvAbootMemLayout_RamDump].Base =
        DramBottom + *pBankSize;
#endif

    /* Reduce the available by what VPR is using */
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Vpr, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Vpr].Size) & (~(VPR_CARVEOUT_ADDR_ALIGNMENT - 1)));
    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Vpr].Base - DramBottom;
    *pMemSize = *pBankSize;

    /* Reduce the available by what TSEC is using */
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Tsec, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Tsec].Size) & (~(TSEC_CARVEOUT_ADDR_ALIGNMENT - 1)));
    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Tsec].Base - DramBottom;
    *pMemSize = *pBankSize;

#ifdef NV_USE_NCT
	/* Reduce the available by what NCK is using */
	NvAbootMemLayout[NvAbootMemLayout_Nck].Size = 1*1024*1024; // 1MB
	*pBankSize -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
	*pMemSize  -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
	NvAbootMemLayout[NvAbootMemLayout_Nck].Base =
				DramBottom + *pBankSize;
#endif

    /* Reduce the available by what XUSB is using */
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Xusb, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Xusb].Size) & (~(XUSB_CARVEOUT_ADDR_ALIGNMENT - 1)));
    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Xusb].Base - DramBottom;
    *pMemSize = *pBankSize;

    /* Reduce the available by what is required for Debug */
    // Reserve only in NvProd mode or when JTAG is enabled
    if ((!(NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Fuse, 0 ), FUSE_SECURITY_MODE_0)) || \
        !pBootInfo->BctPtr->SecureJtagControl) && NvOdmQueryGetDebugCarveoutPresence())
    {
        // reserve the carveout for Debug
        NvAbootMemLayout[NvAbootMemLayout_Debug].Size =
                    NvOdmQueryOsMemSize(NvOdmMemoryType_Debug, pOsInfo);
        // Limit the max size to MAX_DEBUG_CARVEOUT_SIZE_ALLOWED
        NvAbootMemLayout[NvAbootMemLayout_Debug].Size  =
        (NvAbootMemLayout[NvAbootMemLayout_Debug].Size > MAX_DEBUG_CARVEOUT_SIZE_ALLOWED) ? \
                                                        MAX_DEBUG_CARVEOUT_SIZE_ALLOWED : \
                                    NvAbootMemLayout[NvAbootMemLayout_Debug].Size;
        NvAbootMemLayout[NvAbootMemLayout_Debug].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
            - NvAbootMemLayout[NvAbootMemLayout_Debug].Size) & (~(DEBUG_CARVEOUT_ADDR_ALIGNMENT - 1)));
        *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Debug].Base - DramBottom;
        *pMemSize = *pBankSize;
    }
    else
    {
        NvAbootMemLayout[NvAbootMemLayout_Debug].Base = 0;
        NvAbootMemLayout[NvAbootMemLayout_Debug].Size = 0;
    }

    // Configure PMC registers
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH116_0, NvAbootMemLayout[NvAbootMemLayout_Debug].Base);
    // Size should be written interms of MBs
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH117_0, (NvAbootMemLayout[NvAbootMemLayout_Debug].Size>> 20));
#endif
    return NV_TRUE;
}

NvError NvAbootChipSpecificConfig(NvAbootHandle hAboot)
{
    NvError e = NvSuccess;
#ifndef BOOT_MINIMAL_BL
    if (NV_REGR(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Fuse, 0),FUSE_SECURITY_MODE_0))
    {
        NvU32 regval = 0;
        regval = NV_REGR(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),\
                                                          APBDEV_PMC_STICKY_BITS_0);
        regval = NV_DRF_VAL(APBDEV_PMC,STICKY_BITS,HDA_LPBK_DIS,0x1);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),\
                                                  APBDEV_PMC_STICKY_BITS_0, regval);
    }
#endif
    // Configure BSEV_CYA_SECURE & BSEV_VPR_CONFIG for secure playback
    {
        CLOCK_ENABLE( hAboot->hRm, H, VDE, ModuleClockState_Enable );
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_CYA_SECURE_0, 0x07);
        /* set ON_THE_FLY_STICKY to 1 */
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_VPR_CONFIG_0, 0x01);
        CLOCK_ENABLE( hAboot->hRm, H, VDE, ModuleClockState_Disable );
    }
    return e;
}

NvU64 NvAbootPrivGetDramRankSize(NvAbootHandle hAboot,
            NvU64 TotalRamSize, NvU32 RankNo)
{
    NvU32 NumDevices;

    NumDevices = NV_REGR(hAboot->hRm,\
       NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_EMEM_ADR_CFG_0);
    NumDevices &= 1;

    return (TotalRamSize >> NumDevices);
}

static NvError NvAbootConfigureLPClusterStateIdle(NvAbootHandle hAboot)
{
    NvU32 Reg;
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    Reg = NV_REGR(hAboot->hRm,
            NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CCLKLP_BURST_POLICY_0);
    Reg = NV_FLD_SET_DRF_NUM(CLK_RST_CONTROLLER, CCLKLP_BURST_POLICY,
            CPULP_STATE, 0x1, Reg);
    NV_REGW(hAboot->hRm,
            NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
            CLK_RST_CONTROLLER_CCLKLP_BURST_POLICY_0, Reg);

fail:
    return e;
}

static NvError NvAbootConfigureGClusterClk(NvAbootHandle hAboot)
{
    NvU32 Reg;
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    NvBlInitPllX_t11x();
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_STATE, 0x2)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_FIQ_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_IRQ_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_RUN_SOURCE, PLLX_OUT0_LJ)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_IDLE_SOURCE, PLLX_OUT0_LJ);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CCLKG_BURST_POLICY_0, Reg);

    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_ENB, 0x1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIS_FROM_COP_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIS_FROM_COP_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIS_FROM_CPU_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIVIDEND, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, SUPER_CCLKG_DIVIDER, SUPER_CDIV_DIVISOR, 0x0);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_SUPER_CCLKG_DIVIDER_0, Reg);

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_RST_DEV_V_CLR_0);
    Reg |= NV_DRF_NUM(CLK_RST_CONTROLLER, RST_DEV_V_CLR, CLR_CPUG_RST, 0x1);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_RST_DEV_V_CLR_0, Reg);

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0);
    Reg |= NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_OUT_ENB_V, CLK_ENB_CPUG, 0x1);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CLK_OUT_ENB_V_0, Reg);

    // Clear software controlled reset of fast cluster
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CPURESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DBGRESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CORERESET0, 1)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_CXRESET0, 1);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_RST_CPUG_CMPLX_CLR_0, Reg);

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CLK_CPUG_CMPLX_0);
    Reg |= NV_DRF_NUM(CLK_RST_CONTROLLER, CLK_CPUG_CMPLX, CPUG0_CLK_STP, 0x0);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmPrivModuleID_ClockAndReset, 0),
        CLK_RST_CONTROLLER_CLK_CPUG_CMPLX_0, Reg);

fail:
    return e;
}

static NvError NvAbootConfigureFlowCtrlForClusterSwitch(NvAbootHandle hAboot)
{
    NvU32 Reg;
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_CPU_CSR_0);
    Reg |=  NV_DRF_DEF(FLOW_CTLR, CPU_CSR, SWITCH_CLUSTER, DEFAULT_MASK)
         | NV_DRF_NUM(FLOW_CTLR, CPU_CSR, WAIT_WFI_BITMAP, 0x1)
         | NV_DRF_DEF(FLOW_CTLR, CPU_CSR, ENABLE, DEFAULT_MASK)
         | NV_DRF_DEF(FLOW_CTLR, CPU_CSR, ENABLE_EXT, POWERGATE_BOTH_CPU_NONCPU)
         | NV_DRF_DEF(FLOW_CTLR, CPU_CSR, IMMEDIATE_WAKE, DEFAULT_MASK);
    NV_REGW(hAboot->hRm,
         NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_CPU_CSR_0, Reg);

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_HALT_CPU_EVENTS_0);
    Reg |= NV_DRF_DEF(FLOW_CTLR, HALT_CPU_EVENTS, MODE, FLOW_MODE_WAITEVENT);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_HALT_CPU_EVENTS_0, Reg);

    // Clear Ram repair BYPASS_EN to allow HW to do ram repair automatically after switching the cluster
    // Note: BYPASS_EN should be cleared only on parts that have SPARE_BIT_10 or 11 burnt
    if (NvBlRamRepairRequired())
    {
        Reg = NV_REGR(hAboot->hRm,
                  NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_RAM_REPAIR_0);
        Reg |= NV_DRF_DEF(FLOW_CTLR, RAM_REPAIR, BYPASS_EN, DISABLE);
        NV_REGW(hAboot->hRm,
            NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_RAM_REPAIR_0, Reg);
    }

fail:
    return e;
}

NvError NvAbootConfigureClusterSwitch(NvAbootHandle hAboot, NvBool target_cluster)
{
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    if ((target_cluster != NvAbootActiveCluster(hAboot))
        && (target_cluster == FLOW_CTLR_CLUSTER_CONTROL_0_ACTIVE_G))
    {
        NV_CHECK_ERROR_CLEANUP(NvAbootConfigureGClusterClk(hAboot));
        NV_CHECK_ERROR_CLEANUP(NvAbootConfigureLPClusterStateIdle(hAboot));
        NV_CHECK_ERROR_CLEANUP(NvAbootConfigureFlowCtrlForClusterSwitch(hAboot));
    }

fail:
    return e;
}

#if LPM_BATTERY_CHARGING
void NvAbootClocksInterface(NvAbootClocksId Id, NvRmDeviceHandle hRm)
{
    // Dummy implementation
}
#endif

NvBool NvAbootGetWb0Params(NvU32 *Wb0Address,
                 NvU32 *Instances, NvU32 *BlockSize)
{
    if ( (Instances == NULL) || (Wb0Address == NULL) || (BlockSize == NULL) )
       return NV_FALSE;

    *Instances = 0;
    *Wb0Address = 0;
    *BlockSize = 0;
    return NV_TRUE;
}

void NvAbootEnableFuseMirror(NvAbootHandle hAboot)
{
    //Dummy Implementation
    return;
}

void NvAbootDisableUsbInterrupts(NvAbootHandle hAboot)
{
    //Dummy Implementation
    return;
}

