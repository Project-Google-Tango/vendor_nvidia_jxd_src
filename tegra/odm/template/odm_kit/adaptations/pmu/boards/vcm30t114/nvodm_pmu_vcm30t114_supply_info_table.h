/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVODM_PMU_VCM30T114_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_VCM30T114_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    VCM30T114PowerRails_Invalid = 0x0,
    VCM30T114PowerRails_VDD_CPU,
    VCM30T114PowerRails_VDD_CORE,
    VCM30T114PowerRails_VDDIO_DDR,
    VCM30T114PowerRails_VDDIO_SDMMC1,
    VCM30T114PowerRails_VDDIO_UART, //05
    VCM30T114PowerRails_VDD_EMMC_CORE,
    VCM30T114PowerRails_AVDD_PLLA_P_C,
    VCM30T114PowerRails_AVDD_PLLM,
    VCM30T114PowerRails_VDD_DDR_HS,
    VCM30T114PowerRails_VDD_SENSOR_2V8, //10
    VCM30T114PowerRails_AVDDIO_USB3,
    VCM30T114PowerRails_VDDIO_MIPI,
    VCM30T114PowerRails_VDD_RTC,
    VCM30T114PowerRails_VDDIO_SDMMC3,
    VCM30T114PowerRails_AVDD_CAM1, //15
    VCM30T114PowerRails_AVDD_CAM2,
    VCM30T114PowerRails_VDD_LCD_BL,
    VCM30T114PowerRails_VDD_3V3_MINICARD,
    VCM30T114PowerRails_AVDD_LCD,
    VCM30T114PowerRails_VDD_LVDS, //20
    VCM30T114PowerRails_VDD_3V3_SD,
    VCM30T114PowerRails_VDD_3V3_COM,
    VCM30T114PowerRails_DVDD_LCD,
    VCM30T114PowerRails_VPP_FUSE,
    VCM30T114PowerRails_EN_VDDIO_VID,

    VCM30T114PowerRails_Num,
    VCM30T114PowerRails_Force32 = 0x7FFFFFFF
} VCM30T114PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_VCM30T114_SUPPLY_INFO_TABLE_H */
