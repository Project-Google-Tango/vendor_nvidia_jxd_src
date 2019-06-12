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
#include "nvintrhandler_gpio.h"
#include "nvrm_module.h"
#include "nvrm_processor.h"
#include "nvrm_chipid.h"
#include "nvrm_structure.h"
#include "argpio.h"

/**
 * Defines T30 interrupt index for the GPIO.
 */
typedef enum
{
    // the first 32 interrupts are processor local. system interrupts start
    // at 32.
    NvIrqGpioT30_0 = 0x40,
    NvIrqGpioT30_1 = 0x41,
    NvIrqGpioT30_2 = 0x42,
    NvIrqGpioT30_3 = 0x43,
    NvIrqGpioT30_4 = 0x57,
    NvIrqGpioT30_5 = 0x77,
    NvIrqGpioT30_6 = 0x79,
    // bit 29 in the 4th interrupt controller
    NvIrqGpioT30_7 = 32 + (3 * 32) + 29,
}NvIrqGpioT30;


extern NvIntrSubIrq g_NvIntrGpioInst;

NvBool
NvIntrHandleGpioIrqT30(
    NvRmIntrHandle hRmDevice,
    NvIntr *pNvIntr,
    NvU32 *irq );

static const NvU32 s_IrqIndex[] = {NvIrqGpioT30_0, NvIrqGpioT30_1, NvIrqGpioT30_2,
                                    NvIrqGpioT30_3, NvIrqGpioT30_4, NvIrqGpioT30_5,
                                    NvIrqGpioT30_6, NvIrqGpioT30_7};

void NvIntrGetGpioInstIrqTableT30(NvU32 **pIrqIndexList, NvU32 *pMaxIrqIndex)
{

    *pIrqIndexList = (NvU32 *)&s_IrqIndex[0];
    *pMaxIrqIndex = sizeof(s_IrqIndex)/sizeof(s_IrqIndex[0]);
}

NvBool
NvIntrHandleGpioIrqT30(
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

