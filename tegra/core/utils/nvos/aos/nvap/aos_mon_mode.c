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
#include "nvrm_arm_cp.h"
#include "nvbl_arm_cpsr.h"
#include "nvbl_arm_cp15.h"

#include "aos_mon_mode.h"
#include "aos_ns_mode.h"
#include "aos_common.h"

#include "t11x/areic_proc_if.h"

#if AOS_MON_MODE_ENABLE
static NvU32 s_MonStack[NVAOS_MON_STACK_SIZE / sizeof(NvU32)];
#endif

/*
 * Function to switch to secure state
 * Input: smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void nvaosSwitchToSecureState(NvU32 smc_dfc, NvU32 smc_param)
{
    nvaosSendSMC(SMC_TYPE_TO_S, smc_dfc, smc_param);
}

/*
 * Function to switch to non-secure state
 * Input: smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void nvaosSwitchToNonSecureState(NvU32 smc_dfc, NvU32 smc_param)
{
    nvaosSendSMC(SMC_TYPE_TO_NS, smc_dfc, smc_param);
}

/*
 * Set up MMU for Secure State
 */
void nvaosSetNonSecureStateMMU(void)
{
    NvU32    ttb_addr;

    // read secure state ttbr0
    MRC(p15, 0, ttb_addr, c2, c0, 0);

    nvaosSwitchToNonSecureState(SMC_DFC_ENABLE_MMU, ttb_addr);
}

void nvaosConfigureGrp1InterruptController(void)
{
#if AOS_MON_MODE_ENABLE
    NvU32 reg;

    /* Enable the CPU interface to get interrupts */
    reg = AOS_REGR(GIC_PA_BASE + EIC_PROC_IF_GICC_CTLR_0);
    reg = NV_FLD_SET_DRF_NUM(EIC_PROC_IF, GICC_CTLR, EnableGrp1, 1, reg);
    AOS_REGW( GIC_PA_BASE + EIC_PROC_IF_GICC_CTLR_0, reg);
#endif
}

/*
 * Monitor Mode Initialization
 */
void    nvaosMonModeInit(void)
{
#if AOS_MON_MODE_ENABLE
    NvU32    stack;

    stack = (NvU32)s_MonStack;
    stack += NVAOS_MON_STACK_SIZE - 4;
    SetUpMonStack(stack);
#endif
}

/*
 * Function to dispatch SMC Dispatched Function Call
 * Input: smc_type - indicates switch to either secure or non-secure state
 *        smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
void SmcFuncDispatch(NvU32 smc_type, NvU32 smc_dfc, NvU32 smc_param)
{
    switch (smc_dfc)
    {
        case SMC_DFC_ENABLE_MMU:
            nvaosEnableMMU(smc_param);
            break;
        default:
            break;
    }
}

#if !__GNUC__
/*
 * Set up stack for monitor mode
 */
NV_NAKED void    SetUpMonStack(NvU32 stack)
{
    /* MON mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_MON )
    mov sp, r0

    /* system mode */
    msr cpsr_fsxc, #( MODE_DISABLE_INTR | MODE_SYS )

    bx lr
}

/*
 * Issue SMC instruction which switches cpu between secure or non-secure state
 * Input: smc_type - indicates switch to either secure or non-secure state
 *        smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
NV_NAKED void nvaosSendSMC(NvU32 smc_type, NvU32 smc_dfc, NvU32 smc_param)
{
    PRESERVE8

    DCD 0xE1600070    // Opcode to SMC instruction

    bx lr
}

/*
 * SMC Handler, it takes care the switch between secure and non-secure state
 */
NV_NAKED void SmcHandler(void)
{
    PRESERVE8

    IMPORT  SmcFuncDispatch

    // save registers r4, r14. r0-r3 are used for passing parameters.
    stmfd   sp!, {r4, r14}

    // Set NS bit, r0 has input parameter of NS bit
    MRC p15, 0, r4, c1, c1, 0
    BIC r4, r4, #0x1
    ORR r4, r4, r0
    MCR p15, 0, r4, c1, c1, 0

    // Flush the instruction pipeline so as to avoid any security hole
    DCD     0xF57FF06F // encoding for ISB SY

    bl      SmcFuncDispatch

    // restore registers
    ldmfd   sp!, {r4, r14}

    /* return from exception. This will restore LR and SPSR */
    movs    pc, r14
}
#else
/*
 * Set up stack for monitor mode
 */
NV_NAKED void    SetUpMonStack(NvU32 stack)
{
    asm volatile(
    /* MON mode */
    "msr cpsr_fsxc, #0xd6    \n"
    "mov sp, r0              \n"

    /* system mode */
    "msr cpsr_fsxc, #0xdf    \n"

    "bx lr                   \n"
    );
}

/*
 * Issue SMC instruction which switches cpu between secure or non-secure state
 * Input: smc_type - indicates switch to either secure or non-secure state
 *        smc_dfc - SMC Dispatched Function Call, it is used to do
 *                  neccessary set up in Monitor mode before the state switch.
 *        smc_param - Parameter for SMC Dispatched Function Call
 */
NV_NAKED void nvaosSendSMC(NvU32 smc_type, NvU32 smc_dfc, NvU32 smc_param)
{
    asm volatile(

    ".word  0xE1600070       \n"    // Opcode to SMC instruction

    "bx lr                   \n"
    );
}

/*
 * SMC Handler, it takes care the switch between secure and non-secure state
 */
NV_NAKED void SmcHandler(void)
{

    asm volatile(

    // save registers r4, r14. r0-r3 are used for passing parameters.
    "stmfd   sp!, {r4, r14}         \n"

    // Set NS bit, r0 has input parameter of NS bit
    "MRC p15, 0, r4, c1, c1, 0          \n"
    "BIC r4, r4, #0x1                   \n"
    "ORR r4, r4, r0                     \n"
    "MCR p15, 0, r4, c1, c1, 0          \n"

    // Flush the instruction pipeline so as to avoid any security hole
    ".word  0xF57FF06F                  \n" // encoding for ISB SY

    "bl      SmcFuncDispatch            \n"

    // restore registers
    "ldmfd   sp!, {r4, r14}             \n"

    /* return from exception. This will restore LR and SPSR */
    "movs    pc, r14                    \n"
    );
}

#endif

