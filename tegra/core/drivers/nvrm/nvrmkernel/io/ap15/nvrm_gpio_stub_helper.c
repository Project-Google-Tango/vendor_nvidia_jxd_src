/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvassert.h"
#include "nvos.h"
#include "nvrm_gpio.h"
#include "nvrm_interrupt.h"
#include "nvrm_moduleids.h"

struct NvRmGpioInterruptRec 
{
    NvRmDeviceHandle hRm;
    NvRmGpioHandle hGpio;
    NvRmGpioPinHandle hPin;
    NvRmGpioPinMode Mode;
    NvOsInterruptHandler Callback;
    void *arg;
    NvU32 IrqNumber;
    NvOsInterruptHandle NvOsIntHandle;
    NvU32 DebounceTime;
};

static 
void NvRmPrivGpioIsr(void *arg);

NvError 
NvRmGpioInterruptRegister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioPinHandle hPin, 
    NvOsInterruptHandler Callback,
    NvRmGpioPinMode Mode,
    void *CallbackArg,
    NvRmGpioInterruptHandle *hGpioInterrupt,
    NvU32 DebounceTime)
{
    /* Get all these from the handle and/or gpio caps API */
    NvError err;
    struct NvRmGpioInterruptRec *h = NULL;
    NvOsInterruptHandler GpioIntHandler = NvRmPrivGpioIsr;

    NV_ASSERT(Mode == NvRmGpioPinMode_InputInterruptLow || 
                Mode == NvRmGpioPinMode_InputInterruptRisingEdge ||
                Mode == NvRmGpioPinMode_InputInterruptFallingEdge || 
                Mode == NvRmGpioPinMode_InputInterruptHigh || 
                Mode == NvRmGpioPinMode_InputInterruptAny);

    /* Allocate memory for the  NvRmGpioInterruptHandle */
    h = (NvRmGpioInterruptHandle)NvOsAlloc(sizeof(struct NvRmGpioInterruptRec));
    if (h == NULL) 
    {
        err = NvError_InsufficientMemory;
        goto fail;
    }

    NvOsMemset(h, 0, sizeof(struct NvRmGpioInterruptRec));

    h->hPin = hPin;
    h->Mode = Mode;
    h->Callback = Callback;
    h->hRm = hRm;
    h->hGpio = hGpio;
    h->arg = CallbackArg;
    h->DebounceTime = DebounceTime;

    err = NvRmGpioConfigPins(hGpio, &hPin, 1, Mode);
    if (err != NvSuccess)
    {
        goto fail;
    }

    if (!h->NvOsIntHandle)
    {
        NvRmGpioGetIrqs(hRm, &hPin, &(h->IrqNumber), 1);

        err = NvRmInterruptRegister(hRm, 1, &h->IrqNumber, &GpioIntHandler, 
                h, &h->NvOsIntHandle, NV_FALSE);

        if (err != NvSuccess)
        {
            NvError e;
            e = NvRmGpioConfigPins(hGpio, &hPin, 1, NvRmGpioPinMode_Inactive);
            NV_ASSERT(!e);
            (void)e;
            goto fail;
        }
    }

    NV_ASSERT(h->NvOsIntHandle);

    *hGpioInterrupt = h;
    return NvSuccess;

fail:
    NvOsFree(h);
    *hGpioInterrupt = 0;
    return err;
}

NvError
NvRmGpioInterruptEnable(NvRmGpioInterruptHandle hGpioInterrupt)
{
    NV_ASSERT(hGpioInterrupt);

    if (!hGpioInterrupt)
    {
        return NvError_BadParameter;
    }

    return NvRmInterruptEnable(hGpioInterrupt->hRm, hGpioInterrupt->NvOsIntHandle);
}

void
NvRmGpioInterruptMask(NvRmGpioInterruptHandle hGpioInterrupt, NvBool mask)
{
    NvOsInterruptMask(hGpioInterrupt->NvOsIntHandle, mask);
    return;
}

static 
void NvRmPrivGpioIsr(void *arg)
{
    NvU32 i = 0;
    NvRmGpioInterruptHandle info = (NvRmGpioInterruptHandle)arg;

    if (info->DebounceTime)
    {
        NvOsSleepMS(info->DebounceTime);
        for (i = 0; i < 100; i++)
            ;
    }
    /* Call the clients callback function */
    (*info->Callback)(info->arg);

    return;
}

void 
NvRmGpioInterruptUnregister(
    NvRmGpioHandle hGpio,
    NvRmDeviceHandle hRm,
    NvRmGpioInterruptHandle handle)
{
    if (handle == NULL)
        return;

    NV_ASSERT(hGpio);
    NV_ASSERT(hRm);

    NV_ASSERT(NvRmGpioConfigPins(hGpio, &handle->hPin, 1, NvRmGpioPinMode_Inactive) 
            == NvSuccess);
    NvRmInterruptUnregister(hRm, handle->NvOsIntHandle);
    handle->NvOsIntHandle = NULL;

    NvOsFree(handle);
    return;
}

void
NvRmGpioInterruptDone( NvRmGpioInterruptHandle handle )
{
    if (!(handle->NvOsIntHandle)) 
    {
        NV_ASSERT(!"Make sure that interrupt source is enabled AFTER the interrupt is succesfully registered.");
    }
    NvRmInterruptDone(handle->NvOsIntHandle);
}

