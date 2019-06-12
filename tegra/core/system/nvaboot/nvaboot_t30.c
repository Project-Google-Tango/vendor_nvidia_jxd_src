/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
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
#include "nvrm_hardware_access.h"
#include "nvaboot_private.h"
#include "nvodm_services.h"
#include "nvboot_warm_boot_0.h"
#include "nvcrypto_hash.h"
#include "nvcrypto_cipher.h"
#include "t30/arahb_arbc.h"
#include "t30/arapbpm.h"
#include "t30/arapb_misc.h"
#include "t30/arclk_rst.h"
#include "t30/arflow_ctlr.h"
#include "t30/aremc.h"
#include "t30/armc.h"
#include "t30/nvboot_bct.h"
#include "t30/nvboot_bit.h"
#include "t30/nvboot_pmc_scratch_map.h"
#include "t30/nvboot_wb0_sdram_scratch_list.h"

// Correct name->Broken name from nvboot_pmc_scratch_map.h (see bug 738161)
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVM_RANGE\
    APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVM_RANGE
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVN_RANGE\
    APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVN_RANGE
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_CONTROLLER_PLLM_BASE_0_PLLM_DIVP_RANGE\
    APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_BASE_PLLM_DIVP_RANGE
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_CONTROLLER_PLLM_MISC_0_PLLM_CPCON_RANGE\
    APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_MISC_CPCON_RANGE
#define APBDEV_PMC_SCRATCH2_0_CLK_RST_CONTROLLER_PLLM_MISC_0_PLLM_LFCON_RANGE\
    APBDEV_PMC_SCRATCH2_0_CLK_RST_PLLM_MISC_LFCON_RANGE
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVM_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVM_RANGE
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVN_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVN_RANGE
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_BASE_0_PLLX_DIVP_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_BASE_PLLX_DIVP_RANGE
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_0_PLLX_CPCON_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_MISC_CPCON_RANGE
#define APBDEV_PMC_SCRATCH3_0_CLK_RST_CONTROLLER_PLLX_MISC_0_PLLX_LFCON_RANGE\
    APBDEV_PMC_SCRATCH3_0_CLK_RST_PLLX_MISC_LFCON_RANGE

/* Register prefix to RM module id mapping macros. */
#define AHB_ModuleID                NvRmPrivModuleID_Ahb_Arb_Ctrl
#define CLK_RST_CONTROLLER_ModuleID NvRmPrivModuleID_ClockAndReset
#define EMC_ModuleID                NvRmPrivModuleID_ExternalMemoryController
#define MC_ModuleID                 NvRmPrivModuleID_MemoryController

extern NvAbootMemLayoutRec NvAbootMemLayout[NvAbootMemLayout_Num];

/** REGS()       - Scratch mapping registers (list of REG() or CONSTANT() macros).

    REG(n,d,r,f) - Scratch mapping register entry:
    @param n scratch register number (APBDEV_PMC_SCRATCHn)
    @param d register domain (hardware block)
    @param r register name
    @param f register field
 */
#define REGS() \
        REG(3, CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVM) \
        REG(3, CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVN) \
        REG(3, CLK_RST_CONTROLLER, PLLX_BASE, PLLX_DIVP) \
        REG(3, CLK_RST_CONTROLLER, PLLX_MISC, PLLX_LFCON) \
        REG(3, CLK_RST_CONTROLLER, PLLX_MISC, PLLX_CPCON) \
        CONSTANT(3, CLK_RST_PLLX_CHOICE, 0) /* UNUSED */  \
        /**/ \
        CONSTANT(4, PLLX_STABLE_TIME, ~0) \
        CONSTANT(4, CPU_WAKEUP_CLUSTER, 1) \
        /* End-of-List */

/** SCRATCH_REGS() - PMC scratch registers (list of SCRATCH_REG() macros).
    SCRATCH_REG(n) - PMC scratch register name:

    @param n Scratch register number (APBDEV_PMC_SCRATCHs)
 */
#define SCRATCH_REGS()  \
        SCRATCH_REG(2)  \
        SCRATCH_REG(3)  \
        SCRATCH_REG(4)  \
        SCRATCH_REG(5)  \
        SCRATCH_REG(6)  \
        SCRATCH_REG(7)  \
        SCRATCH_REG(8)  \
        SCRATCH_REG(9)  \
        SCRATCH_REG(10) \
        SCRATCH_REG(11) \
        SCRATCH_REG(12) \
        SCRATCH_REG(13) \
        SCRATCH_REG(14) \
        SCRATCH_REG(15) \
        SCRATCH_REG(16) \
        SCRATCH_REG(17) \
        SCRATCH_REG(18) \
        SCRATCH_REG(19) \
        SCRATCH_REG(22) \
        SCRATCH_REG(23) \
        SCRATCH_REG(24) \
        SCRATCH_REG(25) \
        SCRATCH_REG(26) \
        SCRATCH_REG(27) \
        SCRATCH_REG(28) \
        SCRATCH_REG(29) \
        SCRATCH_REG(30) \
        SCRATCH_REG(31) \
        SCRATCH_REG(32) \
        SCRATCH_REG(33) \
        SCRATCH_REG(40) \
        SCRATCH_REG(42) \
        SCRATCH_REG(44) \
        SCRATCH_REG(45) \
        SCRATCH_REG(46) \
        SCRATCH_REG(47) \
        SCRATCH_REG(48) \
        /* End-of-List*/


/* Instantiate local static variables for each of the scratch registers. */
#define SCRATCH_REG(n) static NvU32 SCRATCH##n = 0;
SCRATCH_REGS()
#undef SCRATCH_REG

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
    NvU32                   Val;            //Register field contents
    NvU32                   RamCode;        //BCT RAM selector
    NvBctHandle             hBct = NULL;
    NvU32                   Size, Instance;
    NvBootSdramParams       SdramParams;
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

/* !!!FIXME!!! THE FOLLOWING IS A HODGEPODGE OF METHODS FOR POPULATING THE
               PMC SCRATCH REGISTERS -- SEE BUGS 738241 and 738244. */

    // REG(n,d,r,f)
    // Transformation macro that will stuff a PMC scratch register field with
    // a value from a register.
    //  n = destination Scratch register
    //  d = Device name
    //  r = Register name
    //  f = register Field
    #define REG(n,d,r,f)  \
        Reg = NV_REGR(hAboot->hRm,\
        NVRM_MODULE_ID( d##_ModuleID, 0 ),\
        d##_##r##_0);\
        Val = NV_DRF_VAL(d,r,f,Reg); \
        SCRATCH##n = NV_FLD_SET_SDRF_NUM(SCRATCH##n,d,r,f,Val);

    // CONSTANT(n,f,v)
    // Transformation macro that will stuff a PMC scratch register field with
    // a constant.
    // n = destination scratch register Number
    // f = register Field
    // v = constant Value
    #define CONSTANT(n,f,v) \
        SCRATCH##n = NV_FLD_SET_SF_NUM(SCRATCH##n,f,v);

    /**
     * PACK_FIELD - Pull a field out of a field of an SDRAM parameter structure
     *     into a field of a scratch register. When needed, it writes out
     *     the scratch register value and clears the RegVal cache.
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
    #define PACK_FIELD(n, d, r, f, p)                                            \
    do {                                                                         \
        SCRATCH##n = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH##n, d##_##r##_0_##f, \
                                    NV_DRF_VAL(d, r, f, SdramParams.p),          \
                                    SCRATCH##n);                                 \
    } while (0);

    /**
     * PACK_FIELD - Pull a variable out of an SDRAM parameter structure into
     *     a field of a scratch register. Other considerations are the same
     *     as PACK_FIELD.
     *
     *   @param n scratch register number
     *   @param t data type of the SDRAM parameter (unused)
     *   @param f the name of the SDRAM parameter
     *   @param p field name in NvBootSdramParams structure
     */
    #define PACK_VALUE(n, t, f, p)                                  \
    do {                                                            \
        SCRATCH##n = NV_FLD_SET_DRF_NUM(APBDEV_PMC, SCRATCH##n, f,  \
                                    SdramParams.p,                  \
                                    SCRATCH##n);                    \
    } while (0);

    // Instantiate all of the register transformations.
    REGS()
    SDRAM_PMC_FIELDS(PACK_FIELD);
    SDRAM_PMC_VALUES(PACK_VALUE);

    if (SdramParams.MemoryType == NvBootMemoryType_LpDdr2)
    {
        LPDDR2_PMC_FIELDS(PACK_FIELD);
    }

    #undef REG
    #undef CONSTANT
    #undef PACK_FIELD
    #undef PACK_VALUE

    // Generate writes to the PMC scratch registers to copy the local
    // variables to the actual registers.
    #define SCRATCH_REG(n)\
        NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH##n##_0, SCRATCH##n);
    SCRATCH_REGS()
    #undef SCRATCH_REG

    // Scratch 2 can't be zero (required by boorom)
    NV_REGW(hAboot->hRm, NVRM_MODULE_ID( NvRmModuleID_Pmif, 0 ),\
        APBDEV_PMC_SCRATCH2_0, 0xFFFFFFFF);

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

    // Locking PMC secure scratch registers
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
    return NvSuccess;
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
    default:
        break;
    }

    *pInstance = Instance;
fail:
    return DdkDeviceId;
}

NvBool NvAbootConfigChipSpecificMem(NvAbootHandle hAboot,
                                  NvOdmOsOsInfo *pOsInfo,
                                  NvU32 *pBankSize,
                                  NvU32 *pMemSize,
                                  NvU32 DramBottom)
{
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

#ifdef NV_USE_NCT
    /* Reduce the available by what NCK is using */
    NvAbootMemLayout[NvAbootMemLayout_Nck].Size = 1*1024*1024; // 1MB
    *pBankSize -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
    *pMemSize  -= NvAbootMemLayout[NvAbootMemLayout_Nck].Size;
    NvAbootMemLayout[NvAbootMemLayout_Nck].Base =
                DramBottom + *pBankSize;
#endif

    return NV_TRUE;
}

NvError NvAbootChipSpecificConfig(NvAbootHandle hAboot)
{
    // NOT IMPLEMENTED
    return NvSuccess;
}

NvError NvAbootReadSign(NvCryptoHashAlgoHandle pAlgo,
                        NvU32 *pHashSize,
                        NvU32 Source)
{
    NvError e = NvError_NotInitialized;
    e = pAlgo->QueryHash(pAlgo, pHashSize,
            (NvU8*)(Source + offsetof(NvBootWb0RecoveryHeader, Hash)));
    return e;
}

void NvAbootClearSign(NvBootWb0RecoveryHeader *pDstHeader)
{
    NvOsMemset(&(pDstHeader->Hash), 0, sizeof(pDstHeader->Hash));
}

static NvError NvAbootConfigureGClusterClk(NvAbootHandle hAboot)
{
    NvU32 Reg;
    NvError e = NvSuccess;

    if (!hAboot || !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    NvBlInitPllX_t30();
    Reg = NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_STATE, 0x2)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_FIQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, COP_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_NUM(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CPU_AUTO_CWAKEUP_FROM_IRQ, 0x0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_FIQ_SOURCE, PLLX_OUT0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_IRQ_SOURCE, PLLX_OUT0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_RUN_SOURCE, PLLX_OUT0)
        | NV_DRF_DEF(CLK_RST_CONTROLLER, CCLKG_BURST_POLICY, CWAKEUP_IDLE_SOURCE, PLLX_OUT0);
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
        | NV_DRF_NUM(CLK_RST_CONTROLLER, RST_CPUG_CMPLX_CLR, CLR_DERESET0,  1);
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
         | NV_DRF_DEF(FLOW_CTLR, CPU_CSR, IMMEDIATE_WAKE, DEFAULT_MASK);
    NV_REGW(hAboot->hRm,
         NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_CPU_CSR_0, Reg);

    Reg = NV_REGR(hAboot->hRm,
        NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_HALT_CPU_EVENTS_0);
    Reg |= NV_DRF_DEF(FLOW_CTLR, HALT_CPU_EVENTS, MODE, FLOW_MODE_WAITEVENT);
    NV_REGW(hAboot->hRm,
        NVRM_MODULE_ID(NvRmModuleID_FlowCtrl, 0), FLOW_CTLR_HALT_CPU_EVENTS_0, Reg);

fail:
    return e;
}

NvError NvAbootConfigureClusterSwitch(NvAbootHandle hAboot, NvBool target_cluster)
{
    NvError e = NvSuccess;

    if (!hAboot && !hAboot->hRm) {
        e = NvError_NotInitialized;
        goto fail;
    }

    if ((target_cluster != NvAbootActiveCluster(hAboot))
        && (target_cluster == FLOW_CTLR_CLUSTER_CONTROL_0_ACTIVE_G))
    {
        NV_CHECK_ERROR_CLEANUP(NvAbootConfigureGClusterClk(hAboot));
        NV_CHECK_ERROR_CLEANUP(NvAbootConfigureFlowCtrlForClusterSwitch(hAboot));
    }

fail:
    return e;
}

#ifdef LPM_BATTERY_CHARGING
static void NvAbootPrivT30EnableClocks(NvRmDeviceHandle hRm)
{
    NvU32 Reg;            // Temporary register

    //-------------------------------------------------------------------------
    // Enable clocks to required peripherals.
    //-------------------------------------------------------------------------
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CACHE2, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S0, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VCP, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_HOST1X, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_3D, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_ISP, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_USBD, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_2D, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_EPP, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_PWM, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TWC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC4, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_NDFLASH, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2C1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SPDIF, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC2, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_GPIO, ENABLE, Reg); //MUST_HAVE
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTB, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTA, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TMR, ENABLE, Reg); //MUST_HAVE
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_RTC, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CPU, ENABLE, Reg); //MUST_HAVE
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, Reg);

    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEV, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEA, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_VDE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MPE, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_EMC, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_UARTC, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVDAC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_CSI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_HDMI, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MIPI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVO, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, ENABLE, Reg);
#if PMU_IGNORE_PWRREQ
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, ENABLE, Reg);
#else
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, ENABLE, Reg);
#endif
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC3, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_XIO, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC2, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_NOR, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC1, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SLC1, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_FUSE, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_PMC, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_STAT_MON, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_KBC, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_APBDMA, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MEM, ENABLE, Reg);
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, Reg);

    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV1_OUT, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV2_OUT, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SUS_OUT, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_M_DOUBLER_ENB, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, SYNC_CLK_DOUBLER_ENB, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CRAM2, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMD, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMC, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMB, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMA, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DTV, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_LA, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AVPUCQ, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CSITE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AFI, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_OWR, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_PCIE, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SDMMC3, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SBC4, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_I2C3, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTE, ENABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTD, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SPEEDO, ENABLE, Reg);
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0, Reg);

}

static void NvAbootPrivT30DisableClocks(NvRmDeviceHandle hRm)
{
    NvU32 Reg;            // Temporary register

    //-------------------------------------------------------------------------
    // Disable clocks to required peripherals.
    //-------------------------------------------------------------------------
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CACHE2, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S0, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VCP, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_HOST1X, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP1, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP2, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_3D, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_ISP, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_USBD, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_2D, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_VI, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_EPP, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S2, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_PWM, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TWC, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC4, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC1, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_NDFLASH, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2C1, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_I2S1, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SPDIF, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_SDMMC2, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_GPIO, DISABLE, Reg); //MUST_HAVE
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTB, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_UARTA, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_TMR, DISABLE, Reg); //MUST_HAVE
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_RTC, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_CPU, DISABLE, Reg); //MUST_HAVE
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, Reg);

    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEV, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_BSEA, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_VDE, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MPE, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_EMC, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_UARTC, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_I2C2, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVDAC, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_CSI, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_HDMI, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MIPI, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_TVO, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, DISABLE, Reg);
#if PMU_IGNORE_PWRREQ
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, DISABLE, Reg);
#else
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DVC_I2C, DISABLE, Reg);
#endif
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC3, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_XIO, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC2, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_NOR, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SBC1, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_SLC1, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_FUSE, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_PMC, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_STAT_MON, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_KBC, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_APBDMA, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_MEM, DISABLE, Reg);
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, Reg);

    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV1_OUT, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DEV2_OUT, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SUS_OUT, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_M_DOUBLER_ENB, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, SYNC_CLK_DOUBLER_ENB, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CRAM2, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMD, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMC, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMB, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_IRAMA, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_DTV, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_LA, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AVPUCQ, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_CSITE, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_AFI, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_OWR, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_PCIE, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SDMMC3, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SBC4, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_I2C3, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTE, DISABLE, Reg);
    //Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_UARTD, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_U, CLK_ENB_SPEEDO, DISABLE, Reg);
    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_U_0, Reg);

    NvOsWaitUS(30);
}

static void NvAbootPrivT30EnableDisplayClocks(NvRmDeviceHandle hRm)
{
    NvU32 Reg;            // Temporary register
    /* L_ register */
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP1, ENABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP2, ENABLE, Reg);

    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, Reg);

    /* H_ register */
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, ENABLE, Reg);

    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, Reg);

    NvOsWaitUS(30);
}

static void NvAbootPrivT30DisableDisplayClocks(NvRmDeviceHandle hRm)
{
    NvU32 Reg;            // Temporary register

    /* L_ register */
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP1, DISABLE, Reg);
    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_L, CLK_ENB_DISP2, DISABLE, Reg);

    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_L_0, Reg);

    /* H_ register */
    Reg = NV_REGR(hRm,
            NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0);

    Reg = NV_FLD_SET_DRF_DEF(CLK_RST_CONTROLLER, CLK_OUT_ENB_H, CLK_ENB_DSI, DISABLE, Reg);

    NV_REGW(hRm, NVRM_MODULE_ID( NvRmPrivModuleID_ClockAndReset, 0 ),
            CLK_RST_CONTROLLER_CLK_OUT_ENB_H_0, Reg);

    NvOsWaitUS(30);
}

void NvAbootClocksInterface(NvAbootClocksId Id, NvRmDeviceHandle hRm)
{
    switch(Id)
    {
        case NvAbootClocksId_EnableClocks:
            NvAbootPrivT30EnableClocks(hRm);
            break;
        case NvAbootClocksId_DisableClocks:
            NvAbootPrivT30DisableClocks(hRm);
            break;
        case NvAbootClocksId_EnableDisplayClocks:
            NvAbootPrivT30EnableDisplayClocks(hRm);
            break;
        case  NvAbootClocksId_DisableDisplayClocks:
            NvAbootPrivT30DisableDisplayClocks(hRm);
            break;
    }
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

NvU64 NvAbootPrivGetDramRankSize(NvAbootHandle hAboot,
            NvU64 TotalRamSize, NvU32 RankNo)
{
    NvU32 NumDevices;

    NumDevices = NV_REGR(hAboot->hRm,\
       NVRM_MODULE_ID(NvRmPrivModuleID_MemoryController, 0), MC_EMEM_ADR_CFG_0);
    NumDevices &= 1;

    return (TotalRamSize >> NumDevices);
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

