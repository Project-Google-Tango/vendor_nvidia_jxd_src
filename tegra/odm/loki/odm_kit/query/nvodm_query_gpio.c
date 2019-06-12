/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvodm_query_gpio.h"
#include "nvodm_query_pinmux_gpio.h"
#include "nvodm_services.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query.h"
#include "nvddk_keyboard.h"

#define NVODM_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NVODM_PORT(x) ((x) - 'a')
#define FOSTER_BOARDID 0x09E2

static const NvOdmGpioPinInfo s_LvdsDisplay[] = {
    /* FIXME: update for t124 loki */
};

static const NvOdmGpioPinInfo s_Sdio2[] = {
    // Card detect, not applicable
    {NVODM_GPIO_INVALID_PORT, NVODM_GPIO_INVALID_PIN,
        NvOdmGpioPinActiveState_Low},

    // KB_ROW0, Power rail enable gpio output pin
    {NVODM_PORT('r'), 0, NvOdmGpioPinActiveState_Low},
};

static const NvOdmGpioPinInfo s_oem[] = {
    {NVODM_GPIO_INVALID_PORT, NVODM_GPIO_INVALID_PIN,
        NvOdmGpioPinActiveState_Low},
};

static const NvOdmGpioPinInfo s_test[] = {
    {NVODM_GPIO_INVALID_PORT, NVODM_GPIO_INVALID_PIN,
        NvOdmGpioPinActiveState_Low},
};

// Gpio Pin key info - uses NvEc scan codes
static const NvOdmGpioPinInfo s_GpioPinKeyInfo[] = {
    {KEY_HOLD, 10, NV_TRUE},
};

// Gpio based keypad on E2548, E2549
static const NvOdmGpioPinInfo s_GpioKeyBoard[] = {
    // Shield key, used for hold and select
    {NVODM_PORT('q'), 0, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[0]},
};

// Gpio based keypad on P2530 foster
static const NvOdmGpioPinInfo s_FosterGpioKeyBoard[] = {
    // ONKEY used for hold and select
    {NVODM_PORT('i'), 5, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[0]},
};

static const NvOdmGpioPinInfo s_Bluetooth[] = {
    // Bluetooth Reset
    {NVODM_PORT('u'), 0, NvOdmGpioPinActiveState_Low}
};

static const NvOdmGpioPinInfo s_Wlan[] = {
    // Wlan Power
    {NVODM_PORT('d'), 4, NvOdmGpioPinActiveState_Low},
    // Wlan Reset
    {NVODM_PORT('d'), 3, NvOdmGpioPinActiveState_Low},
};

static const NvOdmGpioPinInfo s_hdmi[] =
{
    /* hdmi hot-plug interrupt pin */
    { NVODM_PORT('n'), 7, NvOdmGpioPinActiveState_High},
};

static const NvOdmGpioPinInfo s_Fuse[] =
{
    /* Enable fuse pin */
    { NVODM_PORT('x'), 4, NvOdmGpioPinActiveState_High},
};
const NvOdmGpioPinInfo *NvOdmQueryGpioPinMap(NvOdmGpioPinGroup Group,
    NvU32 Instance, NvU32 *pCount)
{
    NvOdmQueryBoardPersonality Personality;
    NvOdmBoardInfo BoardInfo;

    NvOdmOsMemset(&BoardInfo, 0, sizeof(NvOdmBoardInfo));

    Personality = NvOdmQueryBoardPersonality_ScrollWheel_WiFi;

    NvOdmPeripheralGetBoardInfo(0, &BoardInfo);

    switch (Group)
    {
        case NvOdmGpioPinGroup_LvdsDisplay:
            *pCount = NVODM_ARRAY_SIZE(s_LvdsDisplay);
            return s_LvdsDisplay;

        case NvOdmGpioPinGroup_keypadColumns:
        case NvOdmGpioPinGroup_keypadRows:
        case NvOdmGpioPinGroup_keypadSpecialKeys:
        case NvOdmGpioPinGroup_Hsmmc:
            *pCount = 0;
            return NULL;

        case NvOdmGpioPinGroup_Sdio:
            if (Instance == 2)
            {
                *pCount = NVODM_ARRAY_SIZE(s_Sdio2);
                return s_Sdio2;
            }
            else
            {
                *pCount = 0;
                return NULL;
            }

        case NvOdmGpioPinGroup_ScrollWheel:
            *pCount = 0;
            return NULL;

        case NvOdmGpioPinGroup_keypadMisc:
            if (BoardInfo.BoardID == FOSTER_BOARDID)
            {
                *pCount = NV_ARRAY_SIZE(s_FosterGpioKeyBoard);
                return s_FosterGpioKeyBoard;
            }
            else
            {
                *pCount = NV_ARRAY_SIZE(s_GpioKeyBoard);
                return s_GpioKeyBoard;
            }

        case NvOdmGpioPinGroup_OEM:
            *pCount = NVODM_ARRAY_SIZE(s_oem);
            return s_oem;

        case NvOdmGpioPinGroup_Test:
            *pCount = NVODM_ARRAY_SIZE(s_test);
            return s_test;

        case NvOdmGpioPinGroup_MioEthernet:
            *pCount = 0;
            return NULL;

        case NvOdmGpioPinGroup_NandFlash:
            // On AP15 Fpga, WP is always tied High. Hence not supported.
            *pCount = 0;
            return NULL;

        case NvOdmGpioPinGroup_Mio:
            *pCount = 0;
            return NULL;

        case NvOdmGpioPinGroup_Bluetooth:
            *pCount = NVODM_ARRAY_SIZE(s_Bluetooth);
            return s_Bluetooth;

        case NvOdmGpioPinGroup_Wlan:
            *pCount = NVODM_ARRAY_SIZE(s_Wlan);
            return s_Wlan;

        case NvOdmGpioPinGroup_Hdmi:
            *pCount = NVODM_ARRAY_SIZE(s_hdmi);
            return s_hdmi;

        case NvOdmGpioPinGroup_Fuse:
             *pCount = NVODM_ARRAY_SIZE(s_Fuse);
             return s_Fuse;

        default:
            *pCount = 0;
            return NULL;
    }
}
