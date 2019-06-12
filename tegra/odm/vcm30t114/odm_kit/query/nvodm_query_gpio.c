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

#define NVODM_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NVODM_PORT(x) ((x) - 'a')

static const NvOdmGpioPinInfo s_LvdsDisplay[] = {
    { NVODM_PORT('n'), 6, NvOdmGpioPinActiveState_High }, // Enable (LVDS_SHTDN_L) (LO:OFF, HI:ON)
    { NVODM_PORT('h'), 3, NvOdmGpioPinActiveState_High }, // EN_VDD_BL1
    { NVODM_PORT('h'), 2, NvOdmGpioPinActiveState_High }, // LCD1_BL_EN
    { NVODM_PORT('w'), 1, NvOdmGpioPinActiveState_High }, // EN_VDD_PNL
    { NVODM_PORT('h'), 0, NvOdmGpioPinActiveState_High }, // LCD1_BL_PWM
    { NVODM_PORT('g'), 2, NvOdmGpioPinActiveState_High }, // LCD_MODE0
    { NVODM_PORT('g'), 3, NvOdmGpioPinActiveState_Low },  // LCD_MODE1
    { NVODM_PORT('g'), 6, NvOdmGpioPinActiveState_High }, // BPP (Low:24bpp, High:18bpp)
    { NVODM_PORT('g'), 0, NvOdmGpioPinActiveState_High }, // LCD_UD
    { NVODM_PORT('g'), 1, NvOdmGpioPinActiveState_High }, // LCD_LR
    { NVODM_PORT('g'), 5, NvOdmGpioPinActiveState_Low },  // STBY
    { NVODM_PORT('g'), 7, NvOdmGpioPinActiveState_Low },  // RESET
    { NVODM_PORT('h'), 1, NvOdmGpioPinActiveState_High }, // RS - LVDS swing mode, 0=200mV, 1=350mV
    { NVODM_PORT('h'), 6, NvOdmGpioPinActiveState_High }, // LCD_AVDD_EN
    { NVODM_PORT('h'), 5, NvOdmGpioPinActiveState_High }, // LCD_VGL_EN
    { NVODM_PORT('h'), 4, NvOdmGpioPinActiveState_High }  // LCD_VGH_EN
};

static const NvOdmGpioPinInfo s_Sdio0[] = {
    {NVODM_PORT('i'), 5, NvOdmGpioPinActiveState_Low},    // Card Detect for SDIO instance 0
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
    {0xE04B, 10, NV_TRUE}, /* LEFT */
    {0xE04D, 10, NV_TRUE}, /* RIGHT */
    {0x1C, 10, NV_TRUE},   /* ENTER */
};

// Gpio based keypad for E1611 sku 1001 dalmore board
static const NvOdmGpioPinInfo s_GpioKeyBoard[] = {
    {NVODM_PORT('i'), 5, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[2]}, //KEY SELECT
    {NVODM_PORT('r'), 1, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[0]}, //KEY DOWN
    {NVODM_PORT('r'), 2, NvOdmGpioPinActiveState_Low, (void *)&s_GpioPinKeyInfo[1]}, //KEY UP
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
            if (Instance == 0)
            {
                *pCount = NVODM_ARRAY_SIZE(s_Sdio0);
                return s_Sdio0;
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
            *pCount = NV_ARRAY_SIZE(s_GpioKeyBoard);
            return s_GpioKeyBoard;

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
