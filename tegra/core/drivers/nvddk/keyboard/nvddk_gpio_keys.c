/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_keyboard_private.h"
#include "nvddk_keyboard.h"
#include "nvodm_query_gpio.h"
#include "nvrm_gpio.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"

#define DEBOUNCE_TIME_MS 0

#define MAX_GPIO_KEYS 10

typedef struct NvDdkGpioPinInfoRec
{
    NvU32 Port;
    NvU32 Pin;
    NvRmGpioPinState ActiveState;
    void *GpioPinSpecificData;
}NvDdkGpioPinInfo;

typedef struct NvDdkGpioKbdContextRec
{
    const NvDdkGpioPinInfo *GpioPinInfo; // GPIO info struct
    NvRmGpioPinHandle hGpioPins[MAX_GPIO_KEYS]; // array of gpio pin handles
    NvU32 PinCount;
    NvOsMutexHandle hKeyEventMutex;
    NvOsSemaphoreHandle hEventSema;
    NvRmGpioInterruptHandle hGpioIntr[MAX_GPIO_KEYS];
    NvU32 NewState[MAX_GPIO_KEYS];
    NvU32 ReportedState[MAX_GPIO_KEYS];
    NvRmDeviceHandle hRm;
    NvRmGpioHandle hGpio;
} NvDdkGpioKbdContext, *NvDdkKbdContextHandle;

static NvDdkGpioKbdContext s_DdkGpioKbdContext;

static void GpioIrq(void *pArg)
{
    NvDdkGpioKbdContext *pCtx = &s_DdkGpioKbdContext;
    NvU32 Val;
    NvU32 Pin = (NvU32)pArg;

    if (Pin >= pCtx->PinCount)
        return;
    NvOsMutexLock(pCtx->hKeyEventMutex);
    NvRmGpioReadPins(pCtx->hGpio, &pCtx->hGpioPins[Pin], &Val, 1);
    if (Val ^ (pCtx->GpioPinInfo[Pin].ActiveState == NvRmGpioPinState_High))
        pCtx->NewState[Pin] = KEY_RELEASE_FLAG;
    else
        pCtx->NewState[Pin] = KEY_PRESS_FLAG;

    NvOsMutexUnlock(pCtx->hKeyEventMutex);
    NvOsSemaphoreSignal(pCtx->hEventSema);
    NvRmGpioInterruptDone(pCtx->hGpioIntr[Pin]);
}

NvBool NvDdkGpioKeyboardInit(void)
{
    NvError e;
    NvU32 i;
    NvDdkGpioKbdContext *pCtx = &s_DdkGpioKbdContext;
    NvU32 Val;
    NvU8 Flag = 0;
    NvOsInterruptHandler IntrHandler = (NvOsInterruptHandler)GpioIrq;

    NvOsMemset(pCtx, 0, sizeof(*pCtx));

    pCtx->GpioPinInfo = (NvDdkGpioPinInfo *)NvOdmQueryGpioPinMap(
        NvOdmGpioPinGroup_keypadMisc, 0, &pCtx->PinCount);

    if (!pCtx->GpioPinInfo || pCtx->PinCount > MAX_GPIO_KEYS)
        return NV_FALSE;

    NvOsMutexCreate(&pCtx->hKeyEventMutex);

    if (!pCtx->hKeyEventMutex)
        return NV_FALSE;

    NvOsSemaphoreCreate(&pCtx->hEventSema, 0);

    if (!pCtx->hEventSema)
        goto fail;

    // Open RM device and RM GPIO handles
    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&pCtx->hRm, 0));
    NV_CHECK_ERROR_CLEANUP(NvRmGpioOpen(
        pCtx->hRm, &pCtx->hGpio));

    for (i=0; i < pCtx->PinCount; i++)
    {
        const NvOdmGpioPinKeyInfo *pInfo;

        NvRmGpioAcquirePinHandle(pCtx->hGpio,
                                  pCtx->GpioPinInfo[i].Port,
                                  pCtx->GpioPinInfo[i].Pin,
                                  &pCtx->hGpioPins[i]);

        if (!pCtx->hGpioPins[i])
            goto fail;

        NvRmGpioConfigPins(pCtx->hGpio, &pCtx->hGpioPins[i], 1, NvRmGpioPinMode_InputData);

        pInfo = pCtx->GpioPinInfo[i].GpioPinSpecificData;

        NV_CHECK_ERROR_CLEANUP(
            NvRmGpioInterruptRegister(
                pCtx->hGpio,
                pCtx->hRm,
                pCtx->hGpioPins[i],
                IntrHandler,
                NvRmGpioPinMode_InputInterruptAny,
                (void *)(i),
                &pCtx->hGpioIntr[i],
                pInfo->DebounceTimeMs)
        );

        NV_CHECK_ERROR_CLEANUP(
            NvRmGpioInterruptEnable(pCtx->hGpioIntr[i])
        );

        NvRmGpioReadPins(pCtx->hGpio, &pCtx->hGpioPins[i], &Val, 1);
        if (Val ^ (pCtx->GpioPinInfo[i].ActiveState == NvRmGpioPinState_High))
            pCtx->NewState[i] = KEY_RELEASE_FLAG;
        else
        {
            Flag = 1;
            pCtx->NewState[i] = KEY_PRESS_FLAG;
        }
    }
    if (Flag)
        NvOsSemaphoreSignal(pCtx->hEventSema);

    return NV_TRUE;

fail:
    NvDdkGpioKeyboardDeInit();
    return NV_FALSE;
}

void NvDdkGpioKeyboardDeInit(void)
{
    NvDdkGpioKbdContext *pCtx = &s_DdkGpioKbdContext;
    NvU32 i;
    for (i=0; i < pCtx->PinCount; i++)
    {
        if (pCtx->hGpioIntr[i])
            NvRmGpioInterruptUnregister(pCtx->hGpio,
                                         pCtx->hRm, pCtx->hGpioIntr[i]);

        if (pCtx->hGpioPins[i])
            NvRmGpioReleasePinHandles(pCtx->hGpio, &pCtx->hGpioPins[i], 1);
    }

    if (pCtx->hGpio)
    {
        NvRmGpioClose(pCtx->hGpio);
        NvRmClose(pCtx->hRm);
    }
    if (pCtx->hEventSema)
        NvOsSemaphoreDestroy(pCtx->hEventSema);
    if (pCtx->hKeyEventMutex)
        NvOsMutexDestroy(pCtx->hKeyEventMutex);
    NvOsMemset(pCtx, 0, sizeof(*pCtx));
}

NvBool NvDdkGpioKeyboardGetKeyData(
    NvU32 *pCode,
    NvU8 *pFlags,
    NvU32 Timeout)
{
    NvDdkGpioKbdContext *pCtx = &s_DdkGpioKbdContext;
    NvU32 i,state=0;
    NvU32 KeyPressCode = 0;
    NvU32 KeyReleaseCode = 0;
    NvU8 PressFlag = 0;
    const NvOdmGpioPinKeyInfo *pInfo;
	unsigned int power_timeout=10000;
    if (!pCode || !pFlags || !pCtx->hEventSema)
        return NV_FALSE;

    *pCode = 0;
    *pFlags = 0;

    if (!Timeout)
        Timeout = NV_WAIT_INFINITE;

    if ((NvOsSemaphoreWaitTimeout(pCtx->hEventSema, Timeout) ==  NvError_Timeout))
        return NV_FALSE;

    NvOsMutexLock(pCtx->hKeyEventMutex);

    for (i=0; i<pCtx->PinCount; i++)
    {
        pInfo = pCtx->GpioPinInfo[i].GpioPinSpecificData;

        if (pCtx->NewState[i] == KEY_PRESS_FLAG)
        {
            //adding the key codes of pressed keys to support key-combinations
            KeyPressCode += pInfo->Code;
            if (pCtx->NewState[i] != pCtx->ReportedState[i])
            {
                pCtx->ReportedState[i] = pCtx->NewState[i];
                PressFlag = 1;
            }
        }

        else if ((pCtx->NewState[i] != pCtx->ReportedState[i]) &&
                                (pCtx->NewState[i] == KEY_RELEASE_FLAG))
        {
            KeyReleaseCode = pInfo->Code;
            pCtx->ReportedState[i] = pCtx->NewState[i];
        }
    }

    NvOsMutexUnlock(pCtx->hKeyEventMutex);

    if (PressFlag)
    {
        *pCode = KeyPressCode;
        *pFlags = KEY_PRESS_FLAG;
    }
    else
    {
        *pCode = KeyReleaseCode;
        *pFlags = KEY_RELEASE_FLAG;
    }
	
	#define KEY_UNUSE		0XFE

	if (*pCode == 9 && *pFlags == 2){
		while(power_timeout--);
		NvRmGpioReadPins(pCtx->hGpio, &pCtx->hGpioPins[0], &state, 1);
		if (state == 1){
			*pFlags = KEY_RELEASE_FLAG;
			*pCode = KEY_IGNORE;

		}

	}

    return NV_TRUE;
}
