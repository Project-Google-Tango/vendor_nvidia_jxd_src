/*
 * Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
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
#include "nvaboot.h"
#include "nvodm_services.h"
#include "t12x/arahb_arbc.h"
#include "t12x/arapbpm.h"
#include "t12x/arapb_misc.h"
#include "t12x/arclk_rst.h"
#include "t12x/aremc.h"
#include "t12x/armc.h"
#include "t12x/arfuse.h"
#include "t12x/arvde2x.h"
#include "t12x/nvboot_bct.h"
#include "t12x/nvboot_bit.h"
#include "t12x/nvboot_pmc_scratch_map.h"
#include "t12x/nvboot_wb0_sdram_scratch_list.h"
#include "nvbl_memmap_nvap.h"
#if ENABLE_NVTBOOT
#include "nvaboot_memlayout.h"
#endif
#include "nvml_usbf.h"
#if ENABLE_NVDUMPER
#include "nvbit.h"
#endif

#define MAX_DEBUG_CARVEOUT_SIZE_ALLOWED (256*1024*1024)

#define VPR_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define TSEC_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define XUSB_CARVEOUT_ADDR_ALIGNMENT 0X100000
#define DEBUG_CARVEOUT_ADDR_ALIGNMENT 0X100000

#if ENABLE_NVDUMPER
NvBool NvAbootUpdateBctRamDumpAddress(NvU32 Address);
#endif

#if ENABLE_NVTBOOT
#define MEMINFO_START_ADDR    0x80A00000  /**< Offset @ 10 MB. */

/* MemInfo will be allocated at 10MB SDRAM OFFSET */
NvAbootMemInfo *s_MemInfo = (NvAbootMemInfo *)(MEMINFO_START_ADDR);
#endif

// Would be better to pull in this nvboot header file, but it isn't released
// for customer source builds, so just provide the sole prototype we need.
void NvBootWb0PackSdramParams(NvBootSdramParams *pData);

extern NvAbootMemLayoutRec NvAbootMemLayout[NvAbootMemLayout_Num];

void NvAbootReset(NvAbootHandle hAboot)
{
    NvU32 reg = NV_DRF_DEF(APBDEV_PMC, CNTRL, MAIN_RST, ENABLE);
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_CNTRL_0, reg);
}

NvError NvAbootPrivSaveSdramParams(NvAbootHandle hAboot)
{
    NvError                 e = NvSuccess;
    NvU32                   RamCode;
    NvBctHandle             hBct = NULL;
    NvU32                   Size, Instance;
    NvBootSdramParams       SdramParams;
    NvU32                   Reg;
    NvU32                   RegVal;
    NvU32                   Reg_base0;
    NvU32                   Reg_misc1;
    NvU32                   Reg_misc2;
    Size = 0;
    Instance = 0;

    // Read strap register and extract the RAM selection code.
    // NOTE: The boot ROM limits the RamCode values to the range 0-3.
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),
        APBDEV_PMC_STRAPPING_OPT_A_0);
    RamCode = NV_DRF_VAL(APBDEV_PMC, STRAPPING_OPT_A, RAM_CODE, Reg) & 3;

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

    NvBootWb0PackSdramParams(&SdramParams);

    // Locking PMC secure scratch register (8 ~ 15) for writing
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0);

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
        /* End-of-List*/

    #define SECURE_SCRATCH_REG(n)\
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE2,WRITE##n,ON,Reg);
    SECURE_SCRATCH_REGS()
    #undef SECURE_SCRATCH_REG
    #undef SECURE_SCRATCH_REGS

    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE2_0, Reg);

    // Locking PMC secure scratch register (34 ~ 35) for writing
    Reg = NV_REGR(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE3_0);
#define SECURE_SCRATCH_REGS()  \
        SECURE_SCRATCH_REG(34) \
        SECURE_SCRATCH_REG(35) \

    #define SECURE_SCRATCH_REG(n)\
        Reg = NV_FLD_SET_DRF_DEF(APBDEV_PMC,SEC_DISABLE3,WRITE##n,ON,Reg);
    SECURE_SCRATCH_REGS()
    #undef SECURE_SCRATCH_REG
    #undef SECURE_SCRATCH_REGS

    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
                                                APBDEV_PMC_SEC_DISABLE3_0, Reg);

    // Locking PMC secure scratch register (4~ 7) for both reading and writing
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

    // Irrespective of whether the auto restart mode is selected or not,
    // PLLM configuration registers are backed up in PMC space.
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
    // FIXME:- Implement this function
    return NvSuccess;
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
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_VIDEO_PROTECT_BOM_0,NvAbootMemLayout[NvAbootMemLayout_Vpr].Base);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_VIDEO_PROTECT_SIZE_MB_0,NvAbootMemLayout[NvAbootMemLayout_Vpr].Size >> 20);

        //Specify GPU unit access rights
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_VIDEO_PROTECT_GPU_OVERRIDE_0_0, 1);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_VIDEO_PROTECT_GPU_OVERRIDE_1_0, 0);
        // Disable further write access
        regval = NV_DRF_DEF(MC,VIDEO_PROTECT_REG_CTRL,VIDEO_PROTECT_WRITE_ACCESS,DISABLED);
        regval |= NV_DRF_DEF(MC,VIDEO_PROTECT_REG_CTRL,VIDEO_PROTECT_ALLOW_TZ_WRITE_ACCESS,ENABLED);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_VIDEO_PROTECT_REG_CTRL_0,regval);

        // PMC (will be updated to PMC registers in NvAbootPrivSaveSdramParams())
        SdramParams.McVideoProtectWriteAccess = regval; //Save VPR Write access regval
        SdramParams.McVideoProtectBom = NvAbootMemLayout[NvAbootMemLayout_Vpr].Base;
        SdramParams.McVideoProtectSizeMb = NvAbootMemLayout[NvAbootMemLayout_Vpr].Size >> 20;
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
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_SEC_CARVEOUT_BOM_0,NvAbootMemLayout[NvAbootMemLayout_Tsec].Base);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
            MC_SEC_CARVEOUT_SIZE_MB_0,NvAbootMemLayout[NvAbootMemLayout_Tsec].Size >> 20);
        // Disable further write access
        regval = NV_DRF_DEF(MC,SEC_CARVEOUT_REG_CTRL,SEC_CARVEOUT_WRITE_ACCESS,DISABLED);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
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

#if ENABLE_NVDUMPER
NvBool NvAbootUpdateBctRamDumpAddress(NvU32 Address)
{
    NvError e = NvSuccess;
    NvBctHandle hBct = NULL;
    NvBitHandle hBit = NULL;
    NvU32 Size = sizeof(NvU32);
    NvU32 Instance = 0;
    NvBool Ret= NV_FALSE;
    NvU8 *BctBuf = NULL;
    NvU8 *New;
    NvU32 BctSize = 0;
    NvU32 BctId = 0;
    NvU32 La;
    NvBootConfigTable * test;
    NvBctAuxInternalData *Tes;
    NvU32 AuxDataOffset = 0;


    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName( "BCT", &BctId )
    );

    // BCT Handle Init
    NV_CHECK_ERROR_CLEANUP(NvBitInit(&hBit));

    // Gets the BCT length
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, NULL));

    // BCT Handle Init
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctSize, NULL, &hBct));

    if ((BctSize%4096) == 0)
        BctBuf = NvOsAlloc(BctSize);
    else
        BctBuf = NvOsAlloc(((BctSize/4096)+1)*4096);

    NV_CHECK_ERROR_CLEANUP(
       NvBctGetData(hBct, NvBctDataType_OdmOption,
                 &Size, &Instance,&La)
    );
    NvOsDebugPrintf("odmdata is %x\n",La);

    NV_CHECK_ERROR_CLEANUP(
    NvBuBctRead( hBct, hBit, BctId,BctBuf )
    );
    test= (NvBootConfigTable *)BctBuf;

    AuxDataOffset = NVBOOT_BCT_CUSTOMER_DATA_SIZE -
    sizeof(NvBctAuxInternalData);
    Tes = (NvBctAuxInternalData *) (test->CustomerData + AuxDataOffset);
    Tes->DumpRamCarveOut = Address;

    NV_CHECK_ERROR_CLEANUP(
       NvBuBctUpdate( hBct, hBit, BctId,BctSize,BctBuf )
    );

    Ret = NV_TRUE;
fail:
    if (BctBuf)
        NvOsFree(BctBuf);
return Ret;
}
#endif

NvBool NvAbootConfigChipSpecificMem(NvAbootHandle hAboot,
                                  NvOdmOsOsInfo *pOsInfo,
                                  NvU32 *pBankSize,
                                  NvU32 *pMemSize,
                                  NvU32 DramBottom)
{
    const NvBootInfoTable*  pBootInfo = (const NvBootInfoTable*)(T12X_BASE_PA_BOOT_INFO);
#if ENABLE_NVDUMPER
    NvBool Ret = NV_TRUE;
#endif

#if ENABLE_NVTBOOT
    NvAbootCarveoutInfo *pCarInfo = &s_MemInfo->CarInfo[0];
#endif

    if (!hAboot || !pOsInfo || !pBankSize || !pMemSize)
    {
        return NV_FALSE;
    }
#if (ENABLE_NVTBOOT && ENABLE_NVDUMPER)
    NvAbootMemLayout[NvAbootMemLayout_RamDump].Size =
        pCarInfo[NvTbootMemLayout_RamDump].Size;
    NvAbootMemLayout[NvAbootMemLayout_RamDump].Base =
        pCarInfo[NvTbootMemLayout_RamDump].Base;

     /* Update BCT with Dram Base address */
    Ret = NvAbootUpdateBctRamDumpAddress(
           (NvU32)NvAbootMemLayout[NvAbootMemLayout_RamDump].Base);
    if (!Ret)
        return Ret;
#endif

#if VPR_EXISTS
#if ENABLE_NVTBOOT
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Size =
        pCarInfo[NvTbootMemLayout_Vpr].Size;
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Base =
        pCarInfo[NvTbootMemLayout_Vpr].Base;
#else
    /* Reduce the available by what VPR is using */
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Vpr, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Vpr].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Vpr].Size) & (~(VPR_CARVEOUT_ADDR_ALIGNMENT - 1)));

    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Vpr].Base - DramBottom;
    *pMemSize = *pBankSize;
#endif
#endif

#if TSEC_EXISTS
#if ENABLE_NVTBOOT
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Size =
        pCarInfo[NvTbootMemLayout_Tsec].Size;
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Base =
        pCarInfo[NvTbootMemLayout_Tsec].Base;
#else
    /* Reduce the available by what TSEC is using */
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Tsec, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Tsec].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Tsec].Size) & (~(TSEC_CARVEOUT_ADDR_ALIGNMENT - 1)));

    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Tsec].Base - DramBottom;
    *pMemSize = *pBankSize;
#endif
#endif

#if XUSB_EXISTS
#if ENABLE_NVTBOOT
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Size =
        pCarInfo[NvTbootMemLayout_Xusb].Size;
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Base =
        pCarInfo[NvTbootMemLayout_Xusb].Base;
#else
    /* Reduce the available by what XUSB is using */
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Size =
                NvOdmQueryOsMemSize(NvOdmMemoryType_Xusb, pOsInfo);
    NvAbootMemLayout[NvAbootMemLayout_Xusb].Base = (NvU64)(((NvU64)DramBottom + (NvU64)*pBankSize
        - NvAbootMemLayout[NvAbootMemLayout_Xusb].Size) & (~(XUSB_CARVEOUT_ADDR_ALIGNMENT - 1)));
    *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Xusb].Base - DramBottom;
    *pMemSize = *pBankSize;
#endif
#endif

#ifdef NV_USE_NCT
#if ENABLE_NVTBOOT
    NvAbootMemLayout[NvAbootMemLayout_Nck].Size =
        pCarInfo[NvTbootMemLayout_Nck].Size;
    NvAbootMemLayout[NvAbootMemLayout_Nck].Base =
        pCarInfo[NvTbootMemLayout_Nck].Base;
#else
    /* Reduce the available by what NCK is using */
    NvAbootMemLayout[NvAbootMemLayout_Nck].Size = 1 * 1024 * 1024; // 1MB
    *pBankSize -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
    *pMemSize  -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
    NvAbootMemLayout[NvAbootMemLayout_Nck].Base =
                DramBottom + *pBankSize;
#endif
#endif

#ifndef BOOT_MINIMAL_BL
#if ENABLE_NVTBOOT
    NvAbootMemLayout[NvAbootMemLayout_Debug].Size =
        pCarInfo[NvTbootMemLayout_Debug].Size;
    NvAbootMemLayout[NvAbootMemLayout_Debug].Base =
        pCarInfo[NvTbootMemLayout_Debug].Base;
#else
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
        NvAbootMemLayout[NvAbootMemLayout_Debug].Base = (DramBottom + *pBankSize
            - NvAbootMemLayout[NvAbootMemLayout_Debug].Size) & (~(DEBUG_CARVEOUT_ADDR_ALIGNMENT - 1));
        *pBankSize = NvAbootMemLayout[NvAbootMemLayout_Debug].Base - DramBottom;
        *pMemSize = *pBankSize;
    }
    else
    {
        NvAbootMemLayout[NvAbootMemLayout_Debug].Base = 0;
        NvAbootMemLayout[NvAbootMemLayout_Debug].Size = 0;
    }
#endif

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

    // Set PMC_WEAK_BIAS to save power in LPx mode
    {
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Pmif, 0),\
                                                  APBDEV_PMC_WEAK_BIAS_0, 0x3CF0F);
    }

#endif
    // Configure BSEV_CYA_SECURE, BSEV_VPR_CONFIG, BSEV_TZ_VPR_CONFIG & BSEV_SECURE_SECURITY for secure playback
    {
        CLOCK_ENABLE( hAboot->hRm, H, VDE, ModuleClockState_Enable );
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_CYA_SECURE_0, 0x0e);

        /* set ON_THE_FLY_STICKY to 1 */
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_VPR_CONFIG_0, 0x01);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_TZ_VPR_CONFIG_0, 0x00);
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID(NvRmModuleID_Vde, 0), ARVDE_BSEV_SECURE_SECURITY_0, 0x04);
        CLOCK_ENABLE( hAboot->hRm, H, VDE, ModuleClockState_Disable );
    }
    return e;
}

NvU64 NvAbootPrivGetDramRankSize(NvAbootHandle hAboot,
            NvU64 TotalRamSize, NvU32 RankNo)
{
    NvU64 DeviceSize = 0;
    NvU32 RegVal = 0;
    NvU32 SubPartitionsInDevice = 0;

    /** 1) Get number of sub-partions per device
          2) Get each device sub-partion size and calculate total size.
          3) Check total size compare with BCT total size
          4) make command line
    **/
    RegVal = NV_REGR(hAboot->hRm,\
      NVRM_MODULE_ID(NvRmPrivModuleID_ExternalMemoryController, 0), EMC_FBIO_CFG5_0);
    SubPartitionsInDevice = NV_DRF_VAL(EMC, FBIO_CFG5, DRAM_WIDTH, RegVal);

#define RD_PARTITION_SIZE(rm, DevNo) \
    RegVal = NV_REGR(rm, NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), \
                             MC_EMEM_ADR_CFG_DEV##DevNo##_0); \
    RegVal = NV_DRF_VAL(MC, EMEM_ADR_CFG_DEV##DevNo, EMEM_DEV##DevNo##_DEVSIZE, RegVal)

    switch(RankNo)
    {
        case 0:
            RD_PARTITION_SIZE(hAboot->hRm, 0);
            break;
        case 1:
            RD_PARTITION_SIZE(hAboot->hRm, 1);
            break;
        default:
            break;
    }
        // Get the subpartition size in MB
        switch(RegVal)
        {
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D16MB:
                DeviceSize = 16;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D32MB:
                DeviceSize = 32;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D64MB:
                DeviceSize = 64;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D128MB:
                DeviceSize = 128;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D256MB:
                DeviceSize = 256;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D512MB:
                DeviceSize = 512;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D1024MB:
                DeviceSize = 1024;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D2048MB:
                DeviceSize = 2048;
                break;
            case MC_EMEM_ADR_CFG_DEV0_0_EMEM_DEV0_DEVSIZE_D4096MB:
                DeviceSize = 4096;
                break;
            default:
                break;
        }
        // Get total rank size in MB
        DeviceSize = DeviceSize << (NvU64)SubPartitionsInDevice;

        return DeviceSize;
}

NvError NvAbootConfigureClusterSwitch(NvAbootHandle hAboot, NvBool target_cluster)
{
    // FIXME:- Implement this function
    return NvSuccess;
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
    NvMlUsbfDisableInterruptsT1xx();
}

