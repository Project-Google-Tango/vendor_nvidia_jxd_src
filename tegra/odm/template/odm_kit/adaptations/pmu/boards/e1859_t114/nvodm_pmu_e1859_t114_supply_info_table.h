/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVODM_PMU_E1859_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_E1859_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    E1859PowerRails_Invalid = 0x0,
    E1859PowerRails_VDD_CPU,
    E1859PowerRails_VDD_CORE,
    E1859PowerRails_VDDIO_DDR,
    E1859PowerRails_VDDIO_SDMMC1,
    E1859PowerRails_VDDIO_UART, //05
    E1859PowerRails_VDD_EMMC_CORE,
    E1859PowerRails_AVDD_PLLA_P_C,
    E1859PowerRails_AVDD_PLLM,
    E1859PowerRails_VDD_DDR_HS,
    E1859PowerRails_VDD_SENSOR_2V8, //10
    E1859PowerRails_AVDDIO_USB3,
    E1859PowerRails_VDDIO_MIPI,
    E1859PowerRails_VDD_RTC,
    E1859PowerRails_VDDIO_SDMMC3,
    E1859PowerRails_AVDD_CAM1, //15
    E1859PowerRails_AVDD_CAM2,
    E1859PowerRails_VDD_LCD_BL,
    E1859PowerRails_VDD_3V3_MINICARD,
    E1859PowerRails_AVDD_LCD,
    E1859PowerRails_VDD_LVDS, //20
    E1859PowerRails_VDD_3V3_SD,
    E1859PowerRails_VDD_3V3_COM,
    E1859PowerRails_DVDD_LCD,
    E1859PowerRails_VPP_FUSE,
    E1859PowerRails_EN_VDDIO_VID,

    E1859PowerRails_Num,
    E1859PowerRails_Force32 = 0x7FFFFFFF
} E1859PowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_E1859_SUPPLY_INFO_TABLE_H */
