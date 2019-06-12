/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvrm_drf.h"
#include "nvrm_arm_cp.h"
#include "nvintrhandler.h"
#include "nvrm_module.h"
#include "arfic_dist.h"
#include "arfic_proc_if.h"
#if GIC_VERSION >= 2
#include "areic_proc_if.h"
#endif

#define NV_INTR_REGW(rm, inst, reg, data) \
    NV_REGW(rm, NVRM_MODULE_ID(NvRmPrivModuleID_ArmPerif, 0), \
        FIC_DIST_##reg##_0 + (inst) * 4, data)

#define NV_CIF_REGW(rm, reg, data) \
    NV_REGW(rm, NVRM_MODULE_ID(NvRmPrivModuleID_ArmPerif, 0), \
        reg##_0, data)

#define NV_CIF_REGR(rm, reg) \
    NV_REGR(rm, NVRM_MODULE_ID(NvRmPrivModuleID_ArmPerif, 0), \
        reg##_0)

static NvU32 NvIntrTimerIrq(void)
{
    return 32; // this is the IRQ for TMR0, which is the same on all our SOCs
}

static void NvIntrSetInterrupt(NvRmIntrHandle hRmDeviceIntr,
                    NvU32 irq, NvBool val)
{
    if (val)
    {
        NV_INTR_REGW(hRmDeviceIntr, (irq / NVRM_IRQS_PER_INTRCTLR),
            ENABLE_SET_0, (1 << (irq % NVRM_IRQS_PER_INTRCTLR)));
    }
    else
    {
        NV_INTR_REGW(hRmDeviceIntr, (irq / NVRM_IRQS_PER_INTRCTLR),
            ENABLE_CLEAR_0, (1 << (irq % NVRM_IRQS_PER_INTRCTLR)));
    }
}

static NvU32 NvIntrDecoder(NvRmIntrHandle hRmDeviceIntr)
{
    NvU32 Virq = 0;
    NvU32 OutstandingIntrMask = 0;

#if GIC_VERSION == 1
    Virq = NV_CIF_REGR(hRmDeviceIntr, FIC_PROC_IF_INT_ACK)
        & FIC_PROC_IF_INT_ACK_0_ACK_INTID_NO_OUTSTANDING_INTR;
    OutstandingIntrMask = FIC_PROC_IF_INT_ACK_0_ACK_INTID_NO_OUTSTANDING_INTR;
#else
    Virq = NV_CIF_REGR(hRmDeviceIntr, EIC_PROC_IF_GICC_IAR)
        & EIC_PROC_IF_GICC_IAR_0_InterruptID_NO_OUTSTANDING_INTR;
    OutstandingIntrMask = EIC_PROC_IF_GICC_IAR_0_InterruptID_NO_OUTSTANDING_INTR;
#endif

    if (Virq != OutstandingIntrMask)
    {
        /* Disable the main IRQ */
        NV_INTR_REGW(hRmDeviceIntr, (Virq / NVRM_IRQS_PER_INTRCTLR),
            ENABLE_CLEAR_0, (1 << (Virq % NVRM_IRQS_PER_INTRCTLR)));

        // FIXME: need a delay here on FPGA of 50us

        /* Done with the interrupt. So write to the EOI register. */
#if GIC_VERSION == 1
        NV_CIF_REGW(hRmDeviceIntr, FIC_PROC_IF_EOI, Virq);
#else
        NV_CIF_REGW(hRmDeviceIntr, EIC_PROC_IF_GICC_EOIR, Virq);
#endif
    }
    return Virq;
}

void NvIntrGetInterfaceGic(NvIntr *pNvIntr, NvU32 ChipId)
{
    pNvIntr->NvSetInterrupt = NvIntrSetInterrupt;
    pNvIntr->NvDecodeInterrupt = NvIntrDecoder;
    pNvIntr->NvTimerIrq = NvIntrTimerIrq;

    // 32 interrupts are processors local interrupts
    pNvIntr->NvMaxMainIntIrq = (5 * NVRM_IRQS_PER_INTRCTLR) + 32;
}
