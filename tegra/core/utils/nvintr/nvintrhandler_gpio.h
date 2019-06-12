/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVINTRHANDLER_GPIO_H
#define INCLUDED_NVINTRHANDLER_GPIO_H

#include "nvintrhandler.h"

#define NV_GPIO_PORTS_PER_CONTROLLER    (4)  // GPIO ports per controller
#define NV_GPIO_PINS_PER_PORT           (8) // GPIO pins per controller port
#define NV_GPIO_PORT_REG_SIZE           (GPIO_CNF_1 - GPIO_CNF_0) // port size
#define NV_GPIO_PORT_REG_MASK           ((1 << NV_GPIO_PINS_PER_PORT) - 1)
#define NV_GPIO_MASK(bit, val)  ((1 << ((bit) + NV_GPIO_PINS_PER_PORT)) \
                            | (((val) << (bit)) & NV_GPIO_PORT_REG_MASK))

#define NV_GPIO_INT_PER_CONTROLLER (NV_GPIO_PORTS_PER_CONTROLLER * NV_GPIO_PINS_PER_PORT)

// Gpio register read/write macros
#define NV_GPIO_REGR( rm, Instance, Port, Reg ) \
    NV_REGR((rm), NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, (Instance) ),\
        ((Port) * NV_GPIO_PORT_REG_SIZE) + (GPIO_##Reg##_0));

#define NV_GPIO_REGW( rm, Instance, Port, Reg, data ) \
    do \
    { \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, (Instance) ),\
            ((Port) * NV_GPIO_PORT_REG_SIZE) +  (GPIO_##Reg##_0), (data)); \
    } while (0)

void NvIntrGetGpioInstIrqTableT30(NvU32 **pIrqIndexList, NvU32 *pMaxIrqIndex);
NvBool NvIntrGpioIrqToInstance(NvRmIntrHandle hRm, NvU32 irq, NvU32 *pInst);

#endif//INCLUDED_NVINTRHANDLER_GPIO_H
