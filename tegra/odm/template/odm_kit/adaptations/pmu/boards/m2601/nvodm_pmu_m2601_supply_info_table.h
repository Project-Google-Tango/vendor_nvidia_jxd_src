/*
 * Copyright (c) 2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_M2601_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_M2601_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    M2601PowerRails_Invalid = 0x0,
    M2601PowerRails_VDD_RTC,
    M2601PowerRails_VDD_CORE,
    M2601PowerRails_AVDD_PLLU_D,
    M2601PowerRails_AVDD_PLLU_D2,
    M2601PowerRails_AVDD_PLLX,
    M2601PowerRails_AVDD_PLLM,
    M2601PowerRails_AVDD_DSI_CSI,
    M2601PowerRails_AVDD_OSC,
    M2601PowerRails_AVDD_USB,
    M2601PowerRails_AVDD_USB_PLL,
    M2601PowerRails_AVDD_HDMI,
    M2601PowerRails_AVDD_HDMI_PLL,
    M2601PowerRails_AVDD_PEXB,
    M2601PowerRails_AVDD_PEXA,
    M2601PowerRails_VDD_PEXB,
    M2601PowerRails_VDD_PEXA,
    M2601PowerRails_AVDD_PEX_PLL,
    M2601PowerRails_AVDD_PLLE,
    M2601PowerRails_VDDIO_PEXCTL,
    M2601PowerRails_HVDD_PEX,
    M2601PowerRails_VDD_DDR_RX,
    M2601PowerRails_VDD_DDR_HS,
    M2601PowerRails_VDDIO_GMI_PMU,
    M2601PowerRails_VDDIO_SYS,
    M2601PowerRails_VDDIO_BB,
    M2601PowerRails_VDDIO_SDMMC4,
    M2601PowerRails_VDDIO_SDMMC1,
    M2601PowerRails_VDDIO_SDMMC3,
    M2601PowerRails_VDDIO_LCD_PMU,
    M2601PowerRails_VDDIO_UART,
    M2601PowerRails_VDDIO_VI,
    M2601PowerRails_VDDIO_AUDIO,
    M2601PowerRails_AVDD_PLL_A_P_C_S,
    M2601PowerRails_MEM_VDDIO_DDR,
    M2601PowerRails_VDD_1V8_PLL,
    M2601PowerRails_EN_PMU_3V3,
    M2601PowerRails_EN_VDD_CPU_DVS,
    M2601PowerRails_VDDIO_CAM,
    M2601PowerRails_VDD_MEM_EN,
    M2601PowerRails_VDD_CORE_EN,
    M2601PowerRails_Num,
    M2601PowerRails_Force32 = 0x7FFFFFFF
} M2601PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_M2601_SUPPLY_INFO_TABLE_H */
