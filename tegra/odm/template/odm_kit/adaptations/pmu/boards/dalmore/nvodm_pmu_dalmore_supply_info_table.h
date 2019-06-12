/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVODM_PMU_DALMORE_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_DALMORE_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    DalmorePowerRails_Invalid = 0x0,
    DalmorePowerRails_VDD_CPU,
    DalmorePowerRails_VDD_CORE,
    DalmorePowerRails_VDDIO_DDR,
    DalmorePowerRails_VDDIO_SDMMC1,
    DalmorePowerRails_VDDIO_UART, //05
    DalmorePowerRails_VDD_EMMC_CORE,
    DalmorePowerRails_AVDD_PLLA_P_C,
    DalmorePowerRails_AVDD_PLLM,
    DalmorePowerRails_VDD_DDR_HS,
    DalmorePowerRails_VDD_SENSOR_2V8, //10
    DalmorePowerRails_AVDDIO_USB3,
    DalmorePowerRails_VDDIO_MIPI,
    DalmorePowerRails_VDD_RTC,
    DalmorePowerRails_VDDIO_SDMMC3,
    DalmorePowerRails_AVDD_CAM1, //15
    DalmorePowerRails_AVDD_CAM2,
    DalmorePowerRails_VDD_LCD_BL,
    DalmorePowerRails_VDD_3V3_MINICARD,
    DalmorePowerRails_AVDD_LCD,
    DalmorePowerRails_VDD_LVDS, //20
    DalmorePowerRails_VDD_3V3_SD,
    DalmorePowerRails_VDD_3V3_COM,
    DalmorePowerRails_DVDD_LCD,
    DalmorePowerRails_VPP_FUSE,
    DalmorePowerRails_EN_VDDIO_VID,

    DalmorePowerRails_Num,
    DalmorePowerRails_Force32 = 0x7FFFFFFF
} DalmorePowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_DALMORE_SUPPLY_INFO_TABLE_H */
