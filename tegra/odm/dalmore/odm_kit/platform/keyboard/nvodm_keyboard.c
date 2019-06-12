/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
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

#define KEY_PRESS_FLAG 2
#define KEY_RELEASE_FLAG 1
#define MAX_GPIO_KEYS 10
#define PROC_BOARD_E1611 0x064B
#define E1611_SKU_1001 1001

typedef struct NvOdmKbdContextRec
{
    const NvOdmGpioPinInfo *GpioPinInfo; // GPIO info struct
    NvOdmGpioPinHandle hOdmPins[MAX_GPIO_KEYS]; // array of gpio pin handles
    NvU32 PinCount;
    NvOdmServicesGpioHandle hOdmGpio;
    NvOdmOsMutexHandle hKeyEventMutex;
    NvOdmOsSemaphoreHandle hEventSema;
    NvOdmServicesGpioIntrHandle IntrHandle[MAX_GPIO_KEYS];
    NvU32 NewState[MAX_GPIO_KEYS];
    NvU32 ReportedState[MAX_GPIO_KEYS];
} NvOdmKbdContext, *NvOdmKbdContextHandle;

static NvOdmKbdContext s_OdmKbdContext;
static NvOdmPowerKeyHandler IsrCallBack = NULL;

static void GpioIrq(void *arg)
{
    NvOdmKbdContext *ctx = &s_OdmKbdContext;
    NvU32 val;
    int pin = (int)arg;

    NvOdmOsMutexLock(ctx->hKeyEventMutex);
    NvOdmGpioGetState(ctx->hOdmGpio, ctx->hOdmPins[pin], &val);

    if (val ^ (ctx->GpioPinInfo[pin].activeState==NvOdmGpioPinActiveState_High))
        ctx->NewState[pin] = KEY_RELEASE_FLAG;
    else
        ctx->NewState[pin] = KEY_PRESS_FLAG;

    NvOdmOsMutexUnlock(ctx->hKeyEventMutex);
    NvOdmOsSemaphoreSignal(ctx->hEventSema);
    NvOdmGpioInterruptDone(ctx->IntrHandle[pin]);
}

NvBool NvOdmKeyboardInit(void)
{
    NvBool status;
    NvOdmBoardInfo BoardInfo;
    NvU32 i;

    NvOdmOsMemset(&BoardInfo, 0, sizeof(NvOdmBoardInfo));
    status = NvOdmPeripheralGetBoardInfo(0, &BoardInfo);

    if (!status)
        goto fail;

    if ((BoardInfo.BoardID == PROC_BOARD_E1611) && (BoardInfo.SKU == E1611_SKU_1001))
    {
        NvOdmKbdContext *ctx = &s_OdmKbdContext;

        NvOdmOsMemset(ctx, 0, sizeof(*ctx));

        ctx->GpioPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_keypadMisc,
                                                0, &ctx->PinCount);

        if (!ctx->GpioPinInfo || ctx->PinCount > MAX_GPIO_KEYS)
            return NV_FALSE;

        ctx->hKeyEventMutex = NvOdmOsMutexCreate();
        if (!ctx->hKeyEventMutex)
            return NV_FALSE;

        ctx->hEventSema = NvOdmOsSemaphoreCreate(0);
        if (!ctx->hEventSema)
            goto fail;

        ctx->hOdmGpio = NvOdmGpioOpen();
        if (!ctx->hOdmGpio)
            goto fail;

        for (i=0; i < ctx->PinCount; i++)
        {
            const NvOdmGpioPinKeyInfo *info;

            ctx->hOdmPins[i] =
                NvOdmGpioAcquirePinHandle(ctx->hOdmGpio,
                                        ctx->GpioPinInfo[i].Port,
                                        ctx->GpioPinInfo[i].Pin);
            if (!ctx->hOdmPins[i])
                goto fail;

            NvOdmGpioConfig(ctx->hOdmGpio, ctx->hOdmPins[i],
                            NvOdmGpioPinMode_InputData);

            info = ctx->GpioPinInfo[i].GpioPinSpecificData;

            if (!NvOdmGpioInterruptRegister(ctx->hOdmGpio,
                        &ctx->IntrHandle[i], ctx->hOdmPins[i],
                        NvOdmGpioPinMode_InputInterruptAny,
                        GpioIrq, (void *)(i), info->DebounceTimeMs))
            goto fail;
        }

        return NV_TRUE;
    }
    else
        return NV_FALSE;

fail:
    NvOdmKeyboardDeInit();
    return NV_FALSE;
}

void NvOdmKeyboardDeInit(void)
{
    NvOdmKbdContext *ctx = &s_OdmKbdContext;
    NvU32 i;
    for (i=0; i < ctx->PinCount; i++)
    {
        if (ctx->IntrHandle[i])
            NvOdmGpioInterruptUnregister(ctx->hOdmGpio,
                                         ctx->hOdmPins[i], ctx->IntrHandle[i]);

        if (ctx->hOdmPins[i])
            NvOdmGpioReleasePinHandle(ctx->hOdmGpio, ctx->hOdmPins[i]);
    }

    if (ctx->hOdmGpio)
        NvOdmGpioClose(ctx->hOdmGpio);
    if (ctx->hEventSema)
        NvOdmOsSemaphoreDestroy(ctx->hEventSema);
    if (ctx->hKeyEventMutex)
        NvOdmOsMutexDestroy(ctx->hKeyEventMutex);
    NvOdmOsMemset(ctx, 0, sizeof(*ctx));
}

NvBool NvOdmKeyboardGetKeyData(
    NvU32 *pCode,
    NvU8 *pFlags,
    NvU32 Timeout)
{
    NvOdmKbdContext *ctx = &s_OdmKbdContext;
    NvU32 i;

    if (!pCode || !pFlags || !ctx->hEventSema)
        return NV_FALSE;

    *pCode = 0;
    *pFlags = 0;

    if (!Timeout)
        Timeout = NV_WAIT_INFINITE;

    if (!NvOdmOsSemaphoreWaitTimeout(ctx->hEventSema, Timeout))
        return NV_FALSE;

    NvOdmOsMutexLock(ctx->hKeyEventMutex);
    for (i=0; i<ctx->PinCount; i++)
    {
        const NvOdmGpioPinKeyInfo *info;

        if (ctx->NewState[i] == ctx->ReportedState[i])
            continue;

        info = ctx->GpioPinInfo[i].GpioPinSpecificData;
        *pCode = info->Code;
        *pFlags = ctx->NewState[i];
        ctx->ReportedState[i] = ctx->NewState[i];
        NvOdmOsMutexUnlock(ctx->hKeyEventMutex);
        return NV_TRUE;
    }
    NvOdmOsMutexUnlock(ctx->hKeyEventMutex);
    return NV_FALSE;
}

NvBool
NvOdmHandlePowerKeyEvent(
    NvOdmPowerKeyEvents Events
    )
{
    if (Events == NvOdmPowerKey_Pressed && IsrCallBack)
        IsrCallBack();
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
    if(Callback)
    {
        IsrCallBack = Callback;
        NvOdmOsPrintf("%s::Registered the callback\n",__func__);
    }
    else
    {
        NvOdmOsPrintf("%s::NULL callback. Registeration failed\n", __func__);
        return NV_FALSE;
    }
    return NV_TRUE;
}

