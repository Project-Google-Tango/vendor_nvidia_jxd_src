/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_P1852_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_P1852_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    P1852PowerRails_Invalid = 0x0,
    P1852PowerRails_VDD_RTC,
    P1852PowerRails_VDD_CORE,
    P1852PowerRails_AVDD_PLLU_D,
    P1852PowerRails_AVDD_PLLU_D2,
    P1852PowerRails_AVDD_PLLX,
    P1852PowerRails_AVDD_PLLM,
    P1852PowerRails_AVDD_DSI_CSI,
    P1852PowerRails_AVDD_OSC,
    P1852PowerRails_AVDD_USB,
    P1852PowerRails_AVDD_USB_PLL,
    P1852PowerRails_AVDD_HDMI,
    P1852PowerRails_AVDD_HDMI_PLL,
    P1852PowerRails_AVDD_PEXB,
    P1852PowerRails_AVDD_PEXA,
    P1852PowerRails_VDD_PEXB,
    P1852PowerRails_VDD_PEXA,
    P1852PowerRails_AVDD_PEX_PLL,
    P1852PowerRails_AVDD_PLLE,
    P1852PowerRails_VDDIO_PEXCTL,
    P1852PowerRails_HVDD_PEX,
    P1852PowerRails_VDD_DDR_RX,
    P1852PowerRails_VDD_DDR_HS,
    P1852PowerRails_VDDIO_GMI_PMU,
    P1852PowerRails_VDDIO_SYS,
    P1852PowerRails_VDDIO_BB,
    P1852PowerRails_VDDIO_SDMMC4,
    P1852PowerRails_VDDIO_SDMMC1,
    P1852PowerRails_VDDIO_SDMMC3,
    P1852PowerRails_VDDIO_LCD_PMU,
    P1852PowerRails_VDDIO_UART,
    P1852PowerRails_VDDIO_VI,
    P1852PowerRails_VDDIO_AUDIO,
    P1852PowerRails_AVDD_PLL_A_P_C_S,
    P1852PowerRails_MEM_VDDIO_DDR,
    P1852PowerRails_VDD_1V8_PLL,
    P1852PowerRails_EN_PMU_3V3,
    P1852PowerRails_EN_VDD_CPU_DVS,
    P1852PowerRails_VDDIO_CAM,
    P1852PowerRails_Num,
    P1852PowerRails_Force32 = 0x7FFFFFFF
} P1852PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_P1852_SUPPLY_INFO_TABLE_H */
