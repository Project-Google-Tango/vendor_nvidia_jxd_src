/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_PMU_LOKI_SUPPLY_INFO_TABLE_H
#define INCLUDED_NVODM_PMU_LOKI_SUPPLY_INFO_TABLE_H

#if defined(__cplusplus)
extern "C"
{
#endif

typedef enum
{
    LokiPowerRails_Invalid = 0x0,
    LokiPowerRails_VDD_CPU,
    LokiPowerRails_VDD_CORE,
    LokiPowerRails_VDDIO_SDMMC4,
    LokiPowerRails_VDDIO_UART,
    LokiPowerRails_AVDD_DSI_CSI, //ldo3
    LokiPowerRails_AVDD_LCD, //ldo2
    LokiPowerRails_VDDIO_SDMMC3,
    LokiPowerRails_VPP_FUSE,
    LokiPowerRails_VDD_3V3_SYS,

    LokiPowerRails_Num,
    LokiPowerRails_Force32 = 0x7FFFFFFF
} LokiPowerRails;

#if defined(__cplusplus)
}
#endif

#endif /* INCLUDED_NVODM_PMU_LOKI_SUPPLY_INFO_TABLE_H */
