/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "testsources.h"
#include "nvbdk_pcb_api.h"
#include "nvbdk_pcb_interface.h"

static NvBdkKbdContext s_BdkKbdContext;

static NvBdkKbdContext *NvPcbKeyGetCtx(void);
static void NvPcbKeyDeinit(void);
static NvError NvPcbKeyInit(void);

static NvBdkKbdContext *NvPcbKeyGetCtx(void)
{
    return &s_BdkKbdContext;
}

static void NvPcbKeyDeinit(void)
{
    NvBdkKbdContext *ctx = &s_BdkKbdContext;
    NvU32 i;

    for (i = 0; i < ctx->PinCount; i++)
    {
        if (ctx->hOdmPins[i])
            NvOdmGpioReleasePinHandle(ctx->hOdmGpio, ctx->hOdmPins[i]);
    }

    if (ctx->hOdmGpio)
        NvOdmGpioClose(ctx->hOdmGpio);
    NvOdmOsMemset(ctx, 0, sizeof(*ctx));
}

static NvError NvPcbKeyInit(void)
{
    NvBdkKbdContext *ctx = &s_BdkKbdContext;
    NvU32 i;

    NvOdmOsMemset(ctx, 0, sizeof(*ctx));

    ctx->GpioPinInfo = NvOdmQueryGpioPinMap(NvOdmGpioPinGroup_keypadMisc,
                    0, &ctx->PinCount);

    if (!ctx->GpioPinInfo || ctx->PinCount > MAX_GPIO_KEYS)
        goto fail;

    ctx->hOdmGpio = NvOdmGpioOpen();
    if (!ctx->hOdmGpio)
    {
        NvOsDebugPrintf("%s: Gpio open failed\n", __func__);
        goto fail;
    }

    for (i = 0; i < ctx->PinCount; i++)
    {
        ctx->hOdmPins[i] = NvOdmGpioAcquirePinHandle(ctx->hOdmGpio,
                            ctx->GpioPinInfo[i].Port,
                            ctx->GpioPinInfo[i].Pin);

        if (!ctx->hOdmPins[i])
        {
            NvOsDebugPrintf("%s: Pinhandle could not be acquired\n", __func__);
            goto fail;
        }

        NvOdmGpioConfig(ctx->hOdmGpio, ctx->hOdmPins[i],
                        NvOdmGpioPinMode_InputData);
    }
    return  NvSuccess;

fail:
    NvPcbKeyDeinit();
    return NvError_ResourceError;
}

void NvPcbKeyTest(NvBDKTest_TestPrivData param)
{
    PcbTestData *pPcbData = (PcbTestData *)param.pData;
    NvBool b;
    NvBdkKbdContext *ctx;
    NvError e = NvSuccess;
    NvU32 i;
    NvU32 val;
    NvU32 retry = 2;
    NvOdmGpioPinKeyInfo *p_keyInfo;

    ctx = NvPcbKeyGetCtx();

    e = NvPcbKeyInit();
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("%s failed : NvError 0x%x\n", __func__, e);
        TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", "NvPcbKeyInit Failed");
    }

    while (retry--)
    {
        for (i = 0; i < ctx->PinCount; i++)
        {
            p_keyInfo = (NvOdmGpioPinKeyInfo *)ctx->GpioPinInfo[i].GpioPinSpecificData;
            NvOdmGpioGetState(ctx->hOdmGpio, ctx->hOdmPins[i], &val);
            if (ctx->GpioPinInfo[i].activeState == val) {
                char szAssertBuf[1024];
                e  = NvError_InvalidState;

                NvOsSnprintf(szAssertBuf, 1024,
                             "(Key is shorted. "
                             "State:%d, Port:%d, Pin:%d, Code:0x%x)",
                             val,
                             ctx->GpioPinInfo[i].Port,
                             ctx->GpioPinInfo[i].Pin,
                             p_keyInfo->Code);

                TEST_ASSERT_MESSAGE(TEST_BOOL_FALSE, pPcbData->name, "pcb", szAssertBuf);
                break;
            }
        }
        NvOsSleepMS(5);
    }
    NvPcbKeyDeinit();

fail:
    return;
}
