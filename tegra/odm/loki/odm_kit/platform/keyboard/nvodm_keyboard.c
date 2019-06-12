/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_keyboard.h"
#include "nvodm_query_discovery.h"
#include "nvodm_services.h"
#include "nvodm_query_gpio.h"
#include "nvodm_pmu.h"

static NvOdmPowerKeyHandler ReleaseKeyCallBack = NULL;
NvOdmServicesGpioIntrHandle s_hGpioIntr = NULL;

NvBool NvOdmKeyboardInit(void)
{
    // Unused code. This will be removed soon
    return NV_FALSE;
}

void NvOdmKeyboardDeInit(void)
{
    // Unused code. This will be removed soon
}

NvBool NvOdmKeyboardGetKeyData(
    NvU32 *pCode,
    NvU8 *pFlags,
    NvU32 Timeout)
{
    // Unused code. This will be removed soon
    return NV_FALSE;
}

NvBool
NvOdmHandlePowerKeyEvent(
    NvOdmPowerKeyEvents Events
    )
{
    if (Events == NvOdmPowerKey_Released)
    {
        NvOdmGpioInterruptDone(s_hGpioIntr);
        if(ReleaseKeyCallBack)
            ReleaseKeyCallBack();
    }
    else
    {
        NvOdmOsPrintf("%s::Wrong Event Type or PowerKey not registered\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

NvBool
NvOdmRegisterPowerKey(
    NvOdmPowerKeyHandler Callback
    )
{
    NvOdmGpioPinHandle hOdmPins;
    NvOdmServicesGpioHandle hOdmGpio;
    if(Callback)
    {
        hOdmGpio = NvOdmGpioOpen();
        hOdmPins = NvOdmGpioAcquirePinHandle(hOdmGpio, 'q' - 'a', 0);
        NvOdmGpioConfig(hOdmGpio, hOdmPins, NvOdmGpioPinMode_InputData );
        if (!NvOdmGpioInterruptRegister(hOdmGpio,
                    &s_hGpioIntr, hOdmPins,
                    NvOdmGpioPinMode_InputInterruptRisingEdge,
                    (void *) NvOdmHandlePowerKeyEvent, (void *)(NvOdmPowerKey_Released), 0x00))
            NvOdmOsDebugPrintf("NvOdmRegisterPowerKey:Gpio Interrupt Registration failed\n");
        ReleaseKeyCallBack = Callback;
        NvOdmOsPrintf("%s::Registered the callback\n",__func__);
    }
    else
    {
        NvOdmOsPrintf("%s::NULL callback. Registeration failed\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}
