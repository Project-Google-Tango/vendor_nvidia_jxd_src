/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_ARDBEG_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_ARDBEG_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    ArdbegPowerRails_Invalid = 0x0,
    ArdbegPowerRails_VDD_CPU, // Tps51632
    ArdbegPowerRails_VDD_CORE, //smps1_2
    ArdbegPowerRails_VDDIO_DDR, // smps6
    ArdbegPowerRails_VDD_EMMC_CORE,// smps7
    ArdbegPowerRails_VDDIO_SYS,// smps9
    ArdbegPowerRails_VDDIO_SDMMC4, // smps9
    ArdbegPowerRails_VDDIO_UART, // smps9
    ArdbegPowerRails_VDDIO_BB, // smps9
    ArdbegPowerRails_VD_LCD_HV, //ldo1
    ArdbegPowerRails_VDD_RTC,  // ldo5
    ArdbegPowerRails_AVDD_DSI_CSI, //ldo9
    ArdbegPowerRails_VDDIO_SDMMC3, // ldo10
    ArdbegPowerRails_AVDD_USB, // ldo11
    ArdbegPowerRails_VDD_LCD_DIS, //ldo12
    ArdbegPowerRails_VPP_FUSE, // ldousb
    ArdbegPowerRails_VDDIO_SATA, //sata
    ArdbegPowerRails_LCD_1V8, //lcd
    ArdbegPowerRails_EN_LCD_1V8, //lcd
    ArdbegPowerRails_AVDD_HDMI, // smps9

    ArdbegPowerRails_Num,
    ArdbegPowerRails_Force32 = 0x7FFFFFFF
} ArdbegPowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_ARDBEG_SUPPLY_INFO_TABLE_H */
