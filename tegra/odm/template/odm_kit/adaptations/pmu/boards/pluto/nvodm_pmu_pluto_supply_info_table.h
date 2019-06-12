/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_PLUTO_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_PLUTO_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    PlutoPowerRails_Invalid = 0x0,
    PlutoPowerRails_VDD_CPU,
    PlutoPowerRails_VDD_CORE,
    PlutoPowerRails_VDD_CORE_BB,
    PlutoPowerRails_VDDIO_DDR,
    PlutoPowerRails_VDDIO_SDMMC3, //05
    PlutoPowerRails_VDD_EMMC,
    PlutoPowerRails_VDDIO_UART,
    PlutoPowerRails_AVDD_USB,
    PlutoPowerRails_VDD_LCD,
    PlutoPowerRails_AVDD_CSI_DSI_PLL, //10
    PlutoPowerRails_AVDD_PLLM,
    PlutoPowerRails_AVDD_PLLX,
    PlutoPowerRails_AVDD_LCD,
    PlutoPowerRails_VDD_RTC,
    PlutoPowerRails_AVDD_DSI_CSI, //15
    PlutoPowerRails_VPP_FUSE,
    PlutoPowerRails_EN_VDD_1V8_LCD,

    PlutoPowerRails_Num,
    PlutoPowerRails_Force32 = 0x7FFFFFFF
} PlutoPowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_PLUTO_SUPPLY_INFO_TABLE_H */
