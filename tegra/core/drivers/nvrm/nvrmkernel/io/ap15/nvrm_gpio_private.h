/*
 * Copyright (c) 2007 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVRM_GPIO_PRIVATE_H
#define INCLUDED_NVRM_GPIO_PRIVATE_H

#include "nvrm_gpio.h"
#include "ap20/argpio.h"
#include "nvrm_structure.h"
#include "ap15/ap15rm_private.h"
#include "nvrm_hwintf.h"
#include "nvodm_query_discovery.h"
#include "nvrm_pmu.h"

#define GPIO_INTR_MAX 32
#define GPIO_PORT(x) ((x) - 'a')
#define GET_PIN(h)      ((((NvU32)(h))) & 0xFF)
#define GET_PORT(h)     ((((NvU32)(h)) >> 8) & 0xFF)
#define GET_INSTANCE(h) ((((NvU32)(h)) >> 16) & 0xFF)

// Size of a port register.
#define NV_GPIO_PORT_REG_SIZE   (GPIO_CNF_1 - GPIO_CNF_0)
#define GPIO_INT_LVL_UNSHADOWED_MASK \
            (GPIO_INT_LVL_0_BIT_7_FIELD | GPIO_INT_LVL_0_BIT_6_FIELD | \
            GPIO_INT_LVL_0_BIT_5_FIELD | GPIO_INT_LVL_0_BIT_4_FIELD | \
            GPIO_INT_LVL_0_BIT_3_FIELD | GPIO_INT_LVL_0_BIT_2_FIELD | \
            GPIO_INT_LVL_0_BIT_1_FIELD | GPIO_INT_LVL_0_BIT_0_FIELD)

#define GPIO_INT_LVL_SHADOWED_MASK (~GPIO_INT_LVL_UNSHADOWED_MASK)

// Gpio register read/write macros

#define GPIO_PINS_PER_PORT  8

#define GPIO_MASKED_WRITE(rm, mskoff, Instance, Port, Reg, Pin, value) \
    do \
    { \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, (Instance) ), \
            ((Port) * NV_GPIO_PORT_REG_SIZE) + mskoff + \
            GPIO_##Reg##_0, \
            (((1<<((Pin)+ GPIO_PINS_PER_PORT)) | ((value) << (Pin))))); \
    } while (0)

// Gpio register read/write macros
#define GPIO_REGR( rm, Instance, Port, Reg, ReadData) \
    do  \
    {   \
        ReadData = NV_REGR((rm), \
            NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, (Instance)), \
            ((Port) * NV_GPIO_PORT_REG_SIZE) + (GPIO_##Reg##_0)); \
    } while (0)

#define GPIO_REGW( rm, Instance, Port, Reg, Data2Write ) \
    do \
    { \
        NV_REGW((rm), NVRM_MODULE_ID( NvRmPrivModuleID_Gpio, (Instance) ), \
            ((Port) * NV_GPIO_PORT_REG_SIZE) + \
            (GPIO_##Reg##_0), (Data2Write)); \
    } while (0)

/* Bit mask of hardware features present in GPIO controller. */
typedef enum {
    NVRM_GPIO_CAP_FEAT_NONE = 0,
    NVRM_GPIO_CAP_FEAT_EDGE_INTR = 0x000000001
} NvRmGpioCapFeatures;

typedef struct NvRmGpioCapsRec {
    NvU32 Instances;
    NvU32 PortsPerInstances;
    NvU32 PinsPerPort;
    NvU32 Features;
    NvU32 MaskedOffset;
} NvRmGpioCaps;

typedef struct NvRmGpioIoPowerInfoRec
{
    // SoC Power rail GUID
    NvU64 PowerRailId;

    // PMU Rail Address
    NvU32 PmuRailAddress;

} NvRmGpioIoPowerInfo;

/**
 * GPIO wrapper for NvRmModuleGetCapabilities().
 *
 * @param hRm The RM device handle
 * @param Capability Out parameter: the cap that maches the current hardware
 */
NvError
NvRmGpioGetCapabilities(
    NvRmDeviceHandle hRm,
    void **Capability );

NvError NvRmGpioIoPowerConfig(
    NvRmDeviceHandle hRm,
    NvU32 port,
    NvU32 pinNumber,
    NvBool Enable);

#endif  // INCLUDED_NVRM_GPIO_PRIVATE_H

