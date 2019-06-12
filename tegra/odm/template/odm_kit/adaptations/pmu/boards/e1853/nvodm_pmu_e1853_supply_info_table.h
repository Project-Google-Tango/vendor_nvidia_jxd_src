/*
 * Copyright (c) 2012, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_E1853_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_E1853_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    E1853PowerRails_Invalid = 0x0,
    E1853PowerRails_VDD_RTC,
    E1853PowerRails_VDD_CORE,
    E1853PowerRails_AVDD_PLLU_D,
    E1853PowerRails_AVDD_PLLU_D2,
    E1853PowerRails_AVDD_PLLX,
    E1853PowerRails_AVDD_PLLM,
    E1853PowerRails_AVDD_DSI_CSI,
    E1853PowerRails_AVDD_OSC,
    E1853PowerRails_AVDD_USB,
    E1853PowerRails_AVDD_USB_PLL,
    E1853PowerRails_AVDD_HDMI,
    E1853PowerRails_AVDD_HDMI_PLL,
    E1853PowerRails_AVDD_PEXB,
    E1853PowerRails_AVDD_PEXA,
    E1853PowerRails_VDD_PEXB,
    E1853PowerRails_VDD_PEXA,
    E1853PowerRails_AVDD_PEX_PLL,
    E1853PowerRails_AVDD_PLLE,
    E1853PowerRails_VDDIO_PEXCTL,
    E1853PowerRails_HVDD_PEX,
    E1853PowerRails_VDD_DDR_RX,
    E1853PowerRails_VDD_DDR_HS,
    E1853PowerRails_VDDIO_GMI_PMU,
    E1853PowerRails_VDDIO_SYS,
    E1853PowerRails_VDDIO_BB,
    E1853PowerRails_VDDIO_SDMMC4,
    E1853PowerRails_VDDIO_SDMMC1,
    E1853PowerRails_VDDIO_SDMMC3,
    E1853PowerRails_VDDIO_LCD_PMU,
    E1853PowerRails_VDDIO_UART,
    E1853PowerRails_VDDIO_VI,
    E1853PowerRails_VDDIO_AUDIO,
    E1853PowerRails_AVDD_PLL_A_P_C_S,
    E1853PowerRails_MEM_VDDIO_DDR,
    E1853PowerRails_VDD_1V8_PLL,
    E1853PowerRails_EN_PMU_3V3,
    E1853PowerRails_EN_VDD_CPU_DVS,
    E1853PowerRails_VDDIO_CAM,
    E1853PowerRails_VPP_FUSE,
    E1853PowerRails_VDD_TEGRA,
    E1853PowerRails_PGOOD,
    E1853PowerRails_Num,
    E1853PowerRails_Force32 = 0x7FFFFFFF
} E1853PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_E1853_SUPPLY_INFO_TABLE_H */
