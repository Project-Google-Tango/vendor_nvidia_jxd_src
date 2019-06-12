/*
 * Copyright (c) 2010 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_services.h"
#include "nvcommon.h"
#include "nvassert.h"
#include "nvodm_query.h"
#include "nvodm_query_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvrm_gpio.h"
#include "nvrm_pmu.h"
#include "nvrm_keylist.h"
#include "nvrm_power.h"
#include "nvrm_analog.h"
#include "nvrm_pinmux.h"
#include "nvrm_hardware_access.h"


typedef struct NvOdmServicesGpioRec
{
    NvRmDeviceHandle hRmDev;
    NvRmGpioHandle hGpio;
} NvOdmServicesGpio;


NvOdmServicesGpioHandle NvOdmGpioOpen(void)
{
    NV_ASSERT(! "not implemented");
    return NULL;
}

void NvOdmGpioClose(NvOdmServicesGpioHandle hOdmGpio)
{
    NV_ASSERT(! "not implemented");
}

NvOdmGpioPinHandle
NvOdmGpioAcquirePinHandle(NvOdmServicesGpioHandle hOdmGpio,
        NvU32 Port, NvU32 Pin)
{
    NV_ASSERT(! "not implemented");
    return NULL;
}

void
NvOdmGpioReleasePinHandle(NvOdmServicesGpioHandle hOdmGpio,
        NvOdmGpioPinHandle hPin)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmGpioSetState(
    NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 PinValue)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmGpioGetState(
    NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvU32 *PinValue)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmGpioConfig(
    NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode mode)
{
    NV_ASSERT(! "not implemented");
}

NvBool
NvOdmGpioInterruptRegister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmServicesGpioIntrHandle *hGpioIntr,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmGpioPinMode Mode,
    NvOdmInterruptHandler Callback,
    void *arg,
    NvU32 DebounceTime)
{
    NV_ASSERT(! "not implemented");
    return NV_FALSE;
}

void
NvOdmGpioInterruptMask(NvOdmServicesGpioIntrHandle handle, NvBool mask)
{
    NV_ASSERT(! "not implemented");
}

void
NvOdmGpioInterruptUnregister(NvOdmServicesGpioHandle hOdmGpio,
    NvOdmGpioPinHandle hGpioPin,
    NvOdmServicesGpioIntrHandle handle)
{
    NV_ASSERT(! "not implemented");
}

void NvOdmGpioInterruptDone( NvOdmServicesGpioIntrHandle handle )
{
    NV_ASSERT(! "not implemented");
}
