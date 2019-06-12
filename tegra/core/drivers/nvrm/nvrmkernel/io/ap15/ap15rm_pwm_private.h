/*
 * Copyright (c) 2007 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: Timer API</b>
 *
 * @b Description: Contains the pwm declarations.
 */

#ifndef INCLUDED_PWM_PRIVATE_H
#define INCLUDED_PWM_PRIVATE_H

#include "nvrm_module.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_pwm.h"

#define PWM_BANK_SIZE           16
#define PMC_BANK_SIZE           192
#define MAX_SUPPORTED_PERIOD    16
#define MAX_DATA_ON             0x7FFF
#define MAX_DATA_OFF            0xFFFF
#define DATA_FACTOR             2048 // 1s / (16 * 30.51us)

typedef struct NvRmPwmRec
{
    // RM device handle
    NvRmDeviceHandle RmDeviceHandle;

    // Pwm configuration pin-map.
    NvOdmPwmPinMap PinMap;

    // Pwm open reference count
    NvU32 RefCount;

    // Pwm virtual base address
    NvU32   VirtualAddress[NvRmPwmOutputId_Num-1];

    // Pwm bank size
    NvU32 PwmBankSize;

    // Pmc bank size
    NvU32 PmcBankSize;

    // pmu powerEnabled flag
    NvBool PowerEnabled;
} NvRmPwm;

#define PWM_RESET(r)            NV_RESETVAL(PWM_CONTROLLER_PWM,r)
#define PWM_SETDEF(r,f,c)       NV_DRF_DEF(PWM_CONTROLLER_PWM,r,f,c)
#define PWM_SETNUM(r,f,n)       NV_DRF_NUM(PWM_CONTROLLER_PWM,r,f,n)
#define PWM_GET(r,f,v)          NV_DRF_VAL(PWM_CONTROLLER_PWM,r,f,v)
#define PWM_CLRSETDEF(v,r,f,c)  NV_FLD_SET_DRF_DEF(PWM_CONTROLLER,r,f,c,v)
#define PWM_CLRSETNUM(v,r,f,n)  NV_FLD_SET_DRF_NUM(PWM_CONTROLLER,r,f,n,v)
#define PWM_MASK(x,y)           (1 << (PWM_CONTROLLER_##x##_0 - PWMCONTROLLER_##y##_0))

#define PMC_RESET(r)            NV_RESETVAL(APBDEV_PMC,r)
#define PMC_SETDEF(r,f,c)       NV_DRF_DEF(APBDEV_PMC,r,f,c)
#define PMC_SETNUM(r,f,n)       NV_DRF_NUM(APBDEV_PMC,r,f,n)
#define PMC_GET(r,f,v)          NV_DRF_VAL(APBDEV_PMC,r,f,v)
#define PMC_CLRSETDEF(v,r,f,c)  NV_FLD_SET_DRF_DEF(APBDEV_PMC,r,f,c,v)
#define PMC_CLRSETNUM(v,r,f,n)  NV_FLD_SET_DRF_NUM(APBDEV_PMC,r,f,n,v)

#endif  // INCLUDED_PWM_PRIVATE_H


