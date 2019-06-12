/*
 * Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvrm_gpio.h"
#include "nvrm_pwm.h"
#include "pwm_test.h"
#include "pwm_test_soc.h"
#include "testsources.h"
#include "nvddk_disp.h"

#define NVDDK_DISP_MAX_CONTROLLERS 2
#define FRAMEBUFFER_DOUBLE 0x1
#define FRAMEBUFFER_32BIT  0x2
#define FRAMEBUFFER_CLEAR  0x4


static NvError DisplayInit(void)
{
    NvDdkDispControllerHandle hControllers[NVDDK_DISP_MAX_CONTROLLERS];
    NvDdkDispControllerHandle hController;
    NvDdkDispControllerHandle hController_temp;
    NvDdkDispWindowHandle hWindow;
    NvDdkDispMode Mode;
    NvRmSurface Surf;
    void *pBuf;
    NvU32 Instance;
    NvDdkDispWindowAttribute Attrs[] =
    {
        NvDdkDispWindowAttribute_DestRect_Left,
        NvDdkDispWindowAttribute_DestRect_Top,
        NvDdkDispWindowAttribute_DestRect_Right,
        NvDdkDispWindowAttribute_DestRect_Bottom,
        NvDdkDispWindowAttribute_SourceRect_Left,
        NvDdkDispWindowAttribute_SourceRect_Top,
        NvDdkDispWindowAttribute_SourceRect_Right,
        NvDdkDispWindowAttribute_SourceRect_Bottom,
    };
    NvU32 Vals[NV_ARRAY_SIZE(Attrs)];
    NvU32 Size;
    NvU32 Align;
    NvU32 n;
    NvU32 nControllers;
    NvError e = NvSuccess;
    NvU32 Count;
    NvU32 Type;
    NvBool b = NV_TRUE;
    char *pTest = "NvPwmBasicTest";
    char *pSuite = "pwm";
    static NvRmDeviceHandle s_hRm = NULL;
    static NvDdkDispHandle s_hDdkDisp = NULL;
    static NvDdkDispDisplayHandle s_hDisplay = NULL;
    static NvRmSurface s_Frontbuffer;
    NvU32  Flags;

    Flags = FRAMEBUFFER_DOUBLE | FRAMEBUFFER_CLEAR | FRAMEBUFFER_32BIT;
    n = 1;
    Instance = 0;
    nControllers = 0;

    if (s_hDisplay)
        return NV_TRUE;

    TEST_ASSERT_EQUAL(NvRmOpenNew(&s_hRm),
        NvSuccess, pTest, pSuite, "NvRmOpenNew FAILED");
    TEST_ASSERT_EQUAL(NvDdkDispOpen(s_hRm, &s_hDdkDisp, 0),
        NvSuccess, pTest, pSuite, "NvDdkDispOpen FAILED");
    TEST_ASSERT_EQUAL(NvDdkDispListControllers(s_hDdkDisp, &nControllers, 0),
        NvSuccess, pTest, pSuite, "NvDdkDispListControllers FAILED");
    TEST_ASSERT_EQUAL(NvDdkDispListControllers(s_hDdkDisp,
        &nControllers, hControllers),
        NvSuccess, pTest, pSuite, "NvDdkDispListControllers FAILED");

    for (Count=0; Count<nControllers; Count++)
    {
        hController_temp = hControllers[Count];
        TEST_ASSERT_EQUAL(
            NvDdkDispGetDisplayByUsage(hController_temp,
                NvDdkDispDisplayUsage_Primary, &s_hDisplay),
                    NvSuccess, pTest, pSuite, "NvDdkDispGetDisplayByUsage FAILED"
        );
        TEST_ASSERT_EQUAL(
            NvDdkDispGetDisplayAttribute(s_hDisplay,
                NvDdkDispDisplayAttribute_Type, &Type),
                   NvSuccess, pTest, pSuite, "NvDdkDispGetDisplayAttribute FAILED"
        );
    }
    hController = hControllers[Instance];
    TEST_ASSERT_EQUAL(
           NvDdkDispGetDisplayByUsage(hController,
               NvDdkDispDisplayUsage_Primary, &s_hDisplay),
                    NvSuccess, pTest, pSuite, "NvDdkDispGetDisplayByUsage FAILED"
    );

    TEST_ASSERT_EQUAL(NvDdkDispListWindows(hController, &n, &hWindow),
        NvSuccess, pTest, pSuite, "NvDdkDispListWindows FAILED");
    NvDdkDispAttachDisplay(hController, s_hDisplay, 0);
    e = NvDdkDispGetBestMode(hController, &Mode, 0);
    TEST_ASSERT_EQUAL(
        e, NvSuccess, pTest, pSuite, "NvDdkDispGetBestMode FAILED"
    );

    TEST_ASSERT_EQUAL(NvDdkDispSetMode(hController, &Mode, 0),  NvSuccess,
        pTest, pSuite, "NvDdkDispSetMode FAILED");

    Vals[0] = Vals[1] = Vals[4] = Vals[5] = 0;
    Vals[2] = Vals[6] = Mode.width;
    Vals[3] = Vals[7] = Mode.height;

    TEST_ASSERT_EQUAL(
        NvDdkDispSetWindowAttributes(hWindow, Attrs, Vals,
            NV_ARRAY_SIZE(Attrs), 0), NvSuccess, pTest, pSuite,
            "NvDdkDispSetWindowAttributes FAILED"
    );

    NvOsMemset(&Surf, 0, sizeof(Surf));
    Surf.Width = Mode.width;
    Surf.Height = Mode.height;
    if (Flags & FRAMEBUFFER_DOUBLE)
        Surf.Height*=2;

    if (Flags & FRAMEBUFFER_32BIT)
        Surf.ColorFormat = NvColorFormat_R8G8B8A8;
    else
        Surf.ColorFormat = NvColorFormat_R5G6B5;

    Surf.Layout = NvRmSurfaceLayout_Pitch;

    // dev/fb expects that the framebuffer surface is 4K-aligned, so
    // allocate with the maximum of the RM's computed alignment and 4K
    NvRmSurfaceComputePitch(s_hRm, 0, &Surf);
    Size = NvRmSurfaceComputeSize(&Surf);
    Align = NvRmSurfaceComputeAlignment(s_hRm, &Surf);
    Align = NV_MAX(4096, Align);

    TEST_ASSERT_EQUAL(NvRmMemHandleAlloc(s_hRm, NULL, 0, Align,
        NvOsMemAttribute_Uncached, Size, 0, 1, &Surf.hMem), NvSuccess,
        pTest, pSuite, "NvRmMemHandleAlloc FAILED");

    TEST_ASSERT_EQUAL(NvRmMemMap(Surf.hMem, 0, Size, 0, &pBuf), NvSuccess,
        pTest, pSuite, "NvRmMemMap FAILED");
    s_Frontbuffer = Surf;

    if (Flags & FRAMEBUFFER_CLEAR)
        NvOsMemset(pBuf, 0, Size);

    if (Flags & FRAMEBUFFER_DOUBLE)
        s_Frontbuffer.Height /= 2;

    TEST_ASSERT_EQUAL(
        NvDdkDispSetWindowSurface(hWindow, &s_Frontbuffer, 1, 0), NvSuccess,
        pTest, pSuite, "NvDdkDispSetWindowSurface FAILED");
fail:
    return e;
}

NvError NvPwmBasicTest(void)
{
    NvU32 RequestedFreq = 0;
    NvU32 ReturnedFreq = 0;
    NvU32 DutyCycle = 0;
    NvError ErrStatus;

    NvRmDeviceHandle hRmDev = NULL;
    NvRmGpioHandle hRmGpio = NULL;
    NvRmGpioPinHandle hPwmTestPin;
    NvRmPwmHandle hRmPwm = NULL;
    NvU32 Instance = PWM_TEST_INSTANCE;

    NvBool b = NV_TRUE;
    char *test = "NvPwmBasicTest";
    char *suite = "pwm";

    // Initialize basic dispaly to observe backlight changes.
    ErrStatus = DisplayInit();
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "Display Init for backlight FAILED");

    // Open RM device handle
    ErrStatus = NvRmOpen(&hRmDev, 0);
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "NvRmOpen for Gpio FAILED");

    // Open RM GPIO handle
    ErrStatus = NvRmGpioOpen(hRmDev, &hRmGpio);
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "NvRmGpioOpen FAILED");

    // Get the handle for requested pin
    ErrStatus = NvRmGpioAcquirePinHandle(hRmGpio, PWM_TEST_PIN_PORT,
        PWM_TEST_PIN_NUM, &hPwmTestPin);
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "NvRmGpioAcquirePinHandle FAILED");

    // Configure pin as SFIO in Pwm
    ErrStatus = NvRmGpioConfigPins(hRmGpio, &hPwmTestPin, 1,
        NvRmGpioPinMode_Function);
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "NvRmGpioConfigPins FAILED");

    // Open RM Pwm handle
    ErrStatus = NvRmPwmOpen(hRmDev, &hRmPwm);
    TEST_ASSERT_EQUAL(ErrStatus,
        NvSuccess, test, suite, "NvRmPwmOpen FAILED");


    NvOsDebugPrintf("PWM Test Pin Changes Started ... \n");
    // To increase frequency of the pulses in some time interval
    for (RequestedFreq = PWM_TEST_FREQ_MIN; RequestedFreq <= PWM_TEST_FREQ_MAX;
        RequestedFreq += PWM_TEST_FREQ_CHANGE)
    {
        // To increase dutycycle from 0 to 100 with some time interval
        for (DutyCycle = 0; DutyCycle <= 100; DutyCycle += 10)
        {
            ErrStatus = NvRmPwmConfig(hRmPwm, Instance,
                NvRmPwmMode_Enable, DutyCycle << 16, RequestedFreq,
                    &ReturnedFreq);
            TEST_ASSERT_EQUAL(ErrStatus,
                NvSuccess, test, suite, "NvRmPwmConfig FAILED");

            NvOsWaitUS(PWM_TEST_INTERVAL_US);
        }
        // To decrease dutycycle from 100 to 0 with some time interval
        for (DutyCycle = 100; DutyCycle > 0; DutyCycle -= 10)
        {
            ErrStatus = NvRmPwmConfig(hRmPwm, Instance,
                NvRmPwmMode_Enable, DutyCycle << 16, RequestedFreq,
                    &ReturnedFreq);
            TEST_ASSERT_EQUAL(ErrStatus,
                NvSuccess, test, suite, "NvRmPwmConfig FAILED");


            NvOsWaitUS(PWM_TEST_INTERVAL_US);
        }
        NvOsWaitUS(PWM_TEST_INTERVAL_US);
    }

    NvOsDebugPrintf("PWM Test Pin Changes Completed\n");
    ErrStatus = NvSuccess;

fail:
    // Close the Rm Pwm handle
    if(hRmPwm)
        NvRmPwmClose(hRmPwm);

    // Close the Rm Gpio handle
    if(hRmGpio)
        NvRmGpioClose(hRmGpio);

    // Close the Rm device handle
    if(hRmDev)
        NvRmClose(hRmDev);
    return ErrStatus;
}

NvError pwm_init_reg(void)
{
    NvBDKTest_pSuite pSuite = NULL;
    NvBDKTest_pTest ptest = NULL;
    NvError e = NvSuccess;
    const char * err_str = 0;
    e = NvBDKTest_AddSuite("pwm", &pSuite);
    if (e != NvSuccess)
    {
        err_str = "Adding Suite pwm failed";
        goto fail;
    }
    e = NvBDKTest_AddTest(pSuite, &ptest, "NvPwmBasicTest",
        (NvBDKTest_TestFunc)NvPwmBasicTest, "basic",
        NULL, NULL);
    if (e != NvSuccess)
    {
        err_str = "Adding Test NvPwmBasicTest failed";
        goto fail;
    }
fail:
    if (err_str)
        NvOsDebugPrintf("%s pwm_init_reg failed : NvError 0x%x\n", err_str, e);
    return e;
}

