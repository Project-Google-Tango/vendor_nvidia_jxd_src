
#define NV_IDL_IS_STUB

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
#include "nvrm_pwm.h"

NvError NvRmPwmConfig(
    NvRmPwmHandle hPwm,
    NvRmPwmOutputId OutputId,
    NvRmPwmMode Mode,
    NvU32 DutyCycle,
    NvU32 RequestedFreqHzOrPeriod,
    NvU32 *pCurrentFreqHzOrPeriod)
{
    if (!pCurrentFreqHzOrPeriod)
        return NvError_BadParameter;

    *pCurrentFreqHzOrPeriod = RequestedFreqHzOrPeriod;
    return NvSuccess;
}

void NvRmPwmClose(NvRmPwmHandle hPwm)
{
}

NvError NvRmPwmOpen(NvRmDeviceHandle hDevice, NvRmPwmHandle *phPwm)
{
    if (!phPwm)
        return NvError_BadParameter;

    *phPwm = (void *)0xdeadbeef;
    return NvSuccess;
}
