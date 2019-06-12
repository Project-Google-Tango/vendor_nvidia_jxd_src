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
#include "nvintrhandler_gpio.h"
#include "nvrm_module.h"
#include "nvrm_interrupt.h"
#include "argpio.h"
#include "nvrm_processor.h"
#include "nvrm_chipid.h"
#include "nvrm_structure.h"

NvIntrSubIrq g_NvIntrGpioInst = {0x0, 0x0, 0x0};
static NvU32 *s_pGpioIntrIrqIndex = NULL;
static NvU32 s_MaxGpioIrqEntry = 0;

NvBool
NvIntrHandleGpioIrqT30(
    NvRmIntrHandle hRmDevice,
    NvIntr *pNvIntr,
    NvU32 *irq );


static void InitGpio( NvRmIntrHandle hRmDevice)
{
    g_NvIntrGpioInst.Instances = NvRmModuleGetNumInstances(hRmDevice, NvRmPrivModuleID_Gpio);
    g_NvIntrGpioInst.LowerIrqIndex = NvRmGetIrqForLogicalInterrupt(hRmDevice,
                    NVRM_MODULE_ID(NvRmPrivModuleID_Gpio, 0),0);

    g_NvIntrGpioInst.UpperIrqIndex = g_NvIntrGpioInst.LowerIrqIndex +
                                g_NvIntrGpioInst.Instances * NV_GPIO_INT_PER_CONTROLLER
                                - 1;
}

static void
GpioGetLowHighIrqIndex(
    NvRmIntrHandle hRmDevice,
    NvU32 *pLowerIrq,
    NvU32 *pUpperIrq)
{
    *pLowerIrq = g_NvIntrGpioInst.LowerIrqIndex;
    *pUpperIrq = g_NvIntrGpioInst.UpperIrqIndex;
}

NvBool
NvIntrGpioIrqToInstance(
    NvRmIntrHandle hRmDevice,
    NvU32 irq,
    NvU32 *pInst)
{
    NvU32 i;
    NvU32 MaxInst = g_NvIntrGpioInst.Instances;
    if (MaxInst != s_MaxGpioIrqEntry)
        return NV_FALSE;

    for (i = 0; i < s_MaxGpioIrqEntry; ++i)
    {
        if (irq == s_pGpioIntrIrqIndex[i])
        {
            *pInst = i;
            return NV_TRUE;
        }
    }
    return NV_FALSE;
}

static NvU32 GpioInstanceToIrq(NvRmIntrHandle hRmDevice, NvU32 inst)
{
    if(inst >= s_MaxGpioIrqEntry)
        return NVRM_IRQ_INVALID;
    return s_pGpioIntrIrqIndex[inst];
}

static NvBool
HandleGpioIrq(
    NvRmIntrHandle hRmDevice,
    NvIntr *pNvIntr,
    NvU32 *irq )
{
    NvU32   port = 0xFF;       // Controller port number
    NvU32   bit= 0xFF;        // Port bit number
    NvU32   instance = 0;
    NvU32   intEnable;  // Port interrupt enable bits
    NvU32   intStatus;  // Port interrupt status bits
    NvBool  status = NV_FALSE;
    NvU32   mask;       // mask of interrupting bit within the port

    if(NvIntrGpioIrqToInstance(hRmDevice, *irq, &instance))
    {
        for (port = 0; port < NV_GPIO_PORTS_PER_CONTROLLER; ++port)
        {
            intEnable = NV_GPIO_REGR(hRmDevice, instance, port, INT_ENB);
            intStatus = NV_GPIO_REGR(hRmDevice, instance, port, INT_STA);

            if ((intStatus = (intEnable & intStatus)) != 0)
            {
                bit  = 31 - CountLeadingZeros(intStatus);
                mask = 1 << bit;

                NV_GPIO_REGW(hRmDevice, instance, port, MSK_INT_ENB,
                                                        NV_GPIO_MASK(bit, 0));
                NV_GPIO_REGW(hRmDevice, instance, port, INT_CLR, mask);

                *irq = g_NvIntrGpioInst.LowerIrqIndex +
                            (instance * NV_GPIO_INT_PER_CONTROLLER) +
                                    ((port * NV_GPIO_PINS_PER_PORT) + bit);

                status = NV_TRUE;

                // Wait for the register write to propogate through the FPGA.
                if (NVRM_IS_CAP_SET(hRmDevice, NvRmCaps_Has128bitInterruptSerializer))
                    NvOsWaitUS(50);

                break;
            }
        }
    }
    return status;
}

static void
GpioSetLogicalIrq(
    NvRmIntrHandle hRmDevice,
    NvIntr *pNvIntr,
    NvU32 irq,
    NvU32 *pIrq,
    NvBool IsEnable)
{
    NvU32 port = 0;
    NvU32 bit   = 0;
    NvU32 inst = 0;
    NvU32 LowIrqIndex, HighIrqIndex;

    GpioGetLowHighIrqIndex(hRmDevice, &LowIrqIndex, &HighIrqIndex);
    if((LowIrqIndex <= irq) && (irq <= HighIrqIndex))
    {
        inst = (irq - LowIrqIndex) / NV_GPIO_INT_PER_CONTROLLER;
        bit =  (irq - LowIrqIndex) % NV_GPIO_PINS_PER_PORT;
        port = (((irq - LowIrqIndex) % NV_GPIO_INT_PER_CONTROLLER) /
                                                        NV_GPIO_PINS_PER_PORT);
        NV_GPIO_REGW(hRmDevice, inst, port, MSK_INT_ENB, NV_GPIO_MASK(bit, IsEnable));
        *pIrq = GpioInstanceToIrq(hRmDevice, inst);
    }
}

static NvBool GpioIsIrqSupported(NvRmIntrHandle hRmDevice, NvU32 irq)
{
    NvU32 LowIrqIndex, HighIrqIndex;
    GpioGetLowHighIrqIndex(hRmDevice, &LowIrqIndex, &HighIrqIndex);
    if((LowIrqIndex <= irq) && (irq <= HighIrqIndex))
        return NV_TRUE;
    return NV_FALSE;
}

void NvIntrGetGpioInterface(NvIntr *pNvIntr, NvU32 Id)
{
    InitGpio(pNvIntr->hRmIntrDevice);
    pNvIntr->NvGpioGetLowHighIrqIndex = GpioGetLowHighIrqIndex;
    pNvIntr->NvGpioIrq = HandleGpioIrq;
    pNvIntr->NvGpioSetLogicalIrq = GpioSetLogicalIrq;
    pNvIntr->NvIsGpioSubIntSupported = GpioIsIrqSupported;

    //Assume T30 and chips in future have the same GPIO IRQ indices.
    //Branches per chip can be added later on if there are differences.
    NvIntrGetGpioInstIrqTableT30(&s_pGpioIntrIrqIndex, &s_MaxGpioIrqEntry);
    pNvIntr->NvGpioIrq = NvIntrHandleGpioIrqT30;
}
