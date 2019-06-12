/*
* Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
*
* NVIDIA Corporation and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA Corporation is strictly prohibited.
*/

#include "nvos.h"
#include "ap15rm_private.h"
#include "nvrm_interrupt.h"
#include "nvrm_hardware_access.h"
#include "nvintr.h"

NvError
NvRmInterruptRegister(
    NvRmDeviceHandle hRmDevice,
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    return NvOsInterruptRegister(IrqListSize,
        pIrqList,
        pIrqHandlerList,
        context,
        handle,
        InterruptEnable);
}

void
NvRmInterruptUnregister(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle)
{
    NvOsInterruptUnregister(handle);
}

NvError
NvRmInterruptEnable(
    NvRmDeviceHandle hRmDevice,
    NvOsInterruptHandle handle)
{
    return NvOsInterruptEnable(handle);
}

void NvRmInterruptDone(NvOsInterruptHandle handle)
{
    NvOsInterruptDone(handle);
}

void NvRmPrivInterruptStart(NvRmDeviceHandle hRmDevice)
{
}

void NvRmPrivInterruptShutdown(NvRmDeviceHandle handle)
{
}
