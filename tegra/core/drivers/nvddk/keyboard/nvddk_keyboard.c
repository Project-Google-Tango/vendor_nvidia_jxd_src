/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_keyboard.h"
#include "nvodm_query_gpio.h"
#include "nvrm_gpio.h"
#include "nvddk_keyboard_private.h"

static struct NvDdkKeyboardInfoRec
{
    NvBool OdmKbdExists;
    NvDdkKbcHandle hDdkKbc;
    NvOsSemaphoreHandle hDdkKbcSema;
    NvRmDeviceHandle hRm;
}NvDdkKeyboardInfo;

NvError NvDdkKeyboardInit(NvBool *IsKeyboardInitialised)
{
    NvError e = NvSuccess;

    NvDdkKeyboardInfo.OdmKbdExists = NV_FALSE;
    NvDdkKeyboardInfo.hDdkKbc = NULL;
    NvDdkKeyboardInfo.hDdkKbcSema = NULL;

    if (IsKeyboardInitialised == NULL)
    {
        e = NvError_BadParameter;
        goto fail;
    }

    NvDdkKeyboardInfo.OdmKbdExists = NvDdkGpioKeyboardInit();

    if (!NvDdkKeyboardInfo.OdmKbdExists)
    {
        NV_CHECK_ERROR_CLEANUP(NvRmOpen(&NvDdkKeyboardInfo.hRm, 0));
        NV_CHECK_ERROR_CLEANUP(NvDdkKbcOpen(NvDdkKeyboardInfo.hRm,
                                            &NvDdkKeyboardInfo.hDdkKbc));
        NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&NvDdkKeyboardInfo.hDdkKbcSema, 0));
        NV_CHECK_ERROR_CLEANUP(NvDdkKbcStart(NvDdkKeyboardInfo.hDdkKbc,
                                             NvDdkKeyboardInfo.hDdkKbcSema));
    }

fail:
    if (e != NvSuccess)
    {
        NvDdkKeyboardDeinit();
        *IsKeyboardInitialised = NV_FALSE;
    }
    else
    {
        *IsKeyboardInitialised = NV_TRUE;
    }
    return e;
}

void NvDdkKeyboardDeinit(void)
{
    if (NvDdkKeyboardInfo.hDdkKbc)
    {
        NvDdkKbcStop(NvDdkKeyboardInfo.hDdkKbc);
        if (NvDdkKeyboardInfo.hDdkKbcSema)
        {
            NvOsSemaphoreDestroy(NvDdkKeyboardInfo.hDdkKbcSema);
            NvDdkKeyboardInfo.hDdkKbcSema = NULL;
        }
        NvDdkKbcClose(NvDdkKeyboardInfo.hDdkKbc);
        NvDdkKeyboardInfo.hDdkKbc = NULL;
    }
    if(NvDdkKeyboardInfo.OdmKbdExists)
    {
        NvDdkGpioKeyboardDeInit();
    }
    if (NvDdkKeyboardInfo.hRm)
    {
        NvRmClose(NvDdkKeyboardInfo.hRm);
        NvDdkKeyboardInfo.hRm = NULL;
    }
}

NvU8 NvDdkKeyboardGetKeyEvent(NvU8 *KeyEvent)
{
    NvU32 RetKeyVal = KEY_IGNORE;

    if(!KeyEvent)
        goto fail;

    if (!(NvDdkKeyboardInfo.OdmKbdExists ||
          NvDdkKeyboardInfo.hDdkKbc))
    {
        goto fail;
    }

    if (NvDdkKeyboardInfo.OdmKbdExists)
    {
        NvU32 KeyCode;
        NvU8 ScanCodeFlags;
        NvBool RetVal;

        RetVal = NvDdkGpioKeyboardGetKeyData(&KeyCode, &ScanCodeFlags, 14);
        if (RetVal == NV_TRUE)
        {
            RetKeyVal = KeyCode;
            *KeyEvent = ScanCodeFlags;
        }
    }
    else if (NvDdkKeyboardInfo.hDdkKbc)
    {
        NvU32 i;
        NvU32 WaitTime;
        NvU32 KeyCount;
        NvU8 IsKeyPressed;
        // FIXME: Get this max event count from either capabilties or ODM.
        NvU32 KeyCodes[16];
        NvDdkKbcKeyEvent KeyEvents[16];

        NvU64 StartTime;
        NvU64 debounce = 200 * 1000;
        static NvU64 PreviousTime = 0;
        static NvU32 PreviousKey = 0;
        WaitTime = NvDdkKbcGetKeyEvents(NvDdkKeyboardInfo.hDdkKbc,
                                        &KeyCount,
                                        KeyCodes,
                                        KeyEvents);

        for (i = 0; i < KeyCount; i++)
        {
            IsKeyPressed =
                (KeyEvents[i] == NvDdkKbcKeyEvent_KeyPress) ? 1 : 0;
            if (!IsKeyPressed)
                continue;
            else
            {
                StartTime = NvOsGetTimeUS();
                if(StartTime - PreviousTime <= debounce && PreviousKey == KeyCodes[i])
                    continue;
                else
                {
                    PreviousKey=KeyCodes[i];
                    PreviousTime=StartTime;
                }
            }
            RetKeyVal = KeyCodes[i];
            *KeyEvent = KEY_PRESS_FLAG;
        }
        // XXX Nothing has ever checked this timeout condition -- is this a bug?
        (void)NvOsSemaphoreWaitTimeout(NvDdkKeyboardInfo.hDdkKbcSema, WaitTime);
    }

fail:
    return RetKeyVal;
}

void NvDdkLp0ConfigureKeyboard(void)
{
    NvError e = NvSuccess;
    NvBool IsKeyboardInitialised;
    e = NvDdkKeyboardInit(&IsKeyboardInitialised);
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("Bootloader: Initializing KBC failed\n");
    }
    if(NvDdkKeyboardInfo.hDdkKbc)
        NvDdkLp0ConfigureKbc(NvDdkKeyboardInfo.hDdkKbc);
}

void NvDdkLp0KeyboardResume()
{
    NvDdkLp0KbcResume();
}
