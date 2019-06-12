/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
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
#define TN8_BOARDID 1761

/* FIXME: update for t124 ardbeg */

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
    {KEY_DOWN, 10, NV_TRUE},
    {KEY_UP, 10, NV_TRUE},
    {KEY_ENTER, 10, NV_TRUE},
    {KEY_POWER,2000,NV_TRUE},
};

// Gpio based keyboard for E1780, E1781, PM358, PM359, PM363
static const NvOdmGpioPinInfo s_GpioKeyBoard[] = {
    {NVODM_PORT('q'), 0, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[3]}, // POWER
    {NVODM_PORT('i'), 5, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[2]}, //KEY SELECT
    {NVODM_PORT('q'), 7, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[0]}, //KEY DOWN
    {NVODM_PORT('q'), 6, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[1]}, //KEY UP
};

// Gpio based keyboard for TN8
static const NvOdmGpioPinInfo s_TN8GpioKeyBoard[] = {
    {NVODM_PORT('q'), 0, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[3]}, // POWER
    {NVODM_PORT('k'), 0, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[2]}, //KEY SELECT
    {NVODM_PORT('q'), 7, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[0]}, //KEY DOWN
    {NVODM_PORT('q'), 6, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[1]}, //KEY UP
};

static const NvOdmGpioPinInfo s_usb[] = {
    {NVODM_PORT('n'), 4, NvOdmGpioPinActiveState_High},
    {NVODM_PORT('n'), 5, NvOdmGpioPinActiveState_High},
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
    NvOdmBoardInfo BoardInfo;
    NvOdmOsMemset(&BoardInfo, 0, sizeof(NvOdmBoardInfo));
    NvOdmPeripheralGetBoardInfo(0, &BoardInfo);

    switch (Group)
    {
        case NvOdmGpioPinGroup_LvdsDisplay:
            *pCount = 0;
            return NULL;

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
            if (BoardInfo.BoardID == TN8_BOARDID)
            {
                *pCount = NV_ARRAY_SIZE(s_TN8GpioKeyBoard);
                return s_TN8GpioKeyBoard;
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
         case NvOdmGpioPinGroup_Usb:
              *pCount = NVODM_ARRAY_SIZE(s_usb);
              return s_usb;

        default:
            *pCount = 0;
            return NULL;
    }
}
