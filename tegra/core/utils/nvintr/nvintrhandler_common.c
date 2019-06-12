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
#include "nvintrhandler.h"

static NvIntrSubIrq MainIntInst = {0x0,0x0, 0x0};
static NvBool MainIntIsIrqSupported(NvRmIntrHandle hRmDevice, NvU32 irq)
{
    if (irq <= MainIntInst.UpperIrqIndex)
        return NV_TRUE;
    return NV_FALSE;
}

void NvIntrGetMainIntInterface(NvIntr *pNvIntr, NvU32 ChipId)
{
    pNvIntr->NvIsMainIntSupported = MainIntIsIrqSupported;

    NvIntrGetInterfaceGic(pNvIntr, ChipId);

    MainIntInst.Instances = pNvIntr->NvMaxMainIntIrq/NVRM_IRQS_PER_INTRCTLR;
    MainIntInst.LowerIrqIndex = 0;
    MainIntInst.UpperIrqIndex = pNvIntr->NvMaxMainIntIrq -1;
}
