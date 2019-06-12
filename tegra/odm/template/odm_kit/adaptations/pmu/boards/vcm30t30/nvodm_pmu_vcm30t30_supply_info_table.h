/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_VCM30T30_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_VCM30T30_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    Vcm30T30PowerRails_Invalid = 0x0,
    Vcm30T30PowerRails_VDD_RTC,
    Vcm30T30PowerRails_VDD_CORE,
    Vcm30T30PowerRails_AVDD_PLLU_D,
    Vcm30T30PowerRails_AVDD_PLLU_D2,
    Vcm30T30PowerRails_AVDD_PLLX,
    Vcm30T30PowerRails_AVDD_PLLM,
    Vcm30T30PowerRails_AVDD_DSI_CSI,
    Vcm30T30PowerRails_AVDD_OSC,
    Vcm30T30PowerRails_AVDD_USB,
    Vcm30T30PowerRails_AVDD_USB_PLL,
    Vcm30T30PowerRails_AVDD_HDMI,
    Vcm30T30PowerRails_AVDD_HDMI_PLL,
    Vcm30T30PowerRails_AVDD_PEXB,
    Vcm30T30PowerRails_AVDD_PEXA,
    Vcm30T30PowerRails_VDD_PEXB,
    Vcm30T30PowerRails_VDD_PEXA,
    Vcm30T30PowerRails_AVDD_PEX_PLL,
    Vcm30T30PowerRails_AVDD_PLLE,
    Vcm30T30PowerRails_VDDIO_PEXCTL,
    Vcm30T30PowerRails_HVDD_PEX,
    Vcm30T30PowerRails_VDD_DDR_RX,
    Vcm30T30PowerRails_VDD_DDR_HS,
    Vcm30T30PowerRails_VDDIO_GMI_PMU,
    Vcm30T30PowerRails_VDDIO_SYS,
    Vcm30T30PowerRails_VDDIO_BB,
    Vcm30T30PowerRails_VDDIO_SDMMC4,
    Vcm30T30PowerRails_VDDIO_SDMMC1,
    Vcm30T30PowerRails_VDDIO_SDMMC3,
    Vcm30T30PowerRails_VDDIO_LCD_PMU,
    Vcm30T30PowerRails_VDDIO_UART,
    Vcm30T30PowerRails_VDDIO_VI,
    Vcm30T30PowerRails_VDDIO_AUDIO,
    Vcm30T30PowerRails_AVDD_PLL_A_P_C_S,
    Vcm30T30PowerRails_MEM_VDDIO_DDR,
    Vcm30T30PowerRails_VDD_1V8_PLL,
    Vcm30T30PowerRails_EN_PMU_3V3,
    Vcm30T30PowerRails_EN_VDD_CPU_DVS,
    Vcm30T30PowerRails_VDDIO_CAM,
    Vcm30T30PowerRails_Num,
    Vcm30T30PowerRails_Force32 = 0x7FFFFFFF
} Vcm30T30PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_VCM30T30_SUPPLY_INFO_TABLE_H */
