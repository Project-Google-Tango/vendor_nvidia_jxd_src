/*
 * Copyright (c) 2013-2014, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database NvOdmIoAddress
 *                 entries for the peripherals on E1780 module.
 */

#include "pmu/boards/ardbeg/nvodm_pmu_ardbeg_supply_info_table.h"

// TPS51632
static const NvOdmIoAddress s_Tps51632Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x86, 0 },
};

// TPS80036
static const NvOdmIoAddress s_Tps80036Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0xB0, 0 },
};

// AS3722
static const NvOdmIoAddress s_As3722Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x80, 0 },
};

// TPS65914
static const NvOdmIoAddress s_Tps65914Addresses[] =
{
    /* Page 1 */
    { NvOdmIoModule_I2c, 0x4, 0xB0, 0 },
    /* Page 2 */
    { NvOdmIoModule_I2c, 0x04, 0xB2, 0 },
    /* Page 3 */
    { NvOdmIoModule_I2c, 0x04, 0xB4, 0 },
};

// VDD_CPU
static const NvOdmIoAddress s_VddCpuAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_CPU, 0 }
};

// VDD_CORE
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_CORE, 0 }
};

// VDD_EMMC_CORE
static const NvOdmIoAddress s_VddEmmcCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_EMMC_CORE, 0 }
};

// VDD_RTC
static const NvOdmIoAddress s_VddRtcAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_RTC, 0 }
};

//VPP_FUSE
static const NvOdmIoAddress s_VppFuseAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VPP_FUSE, 0 }
};

// VDDIO_UART
static const NvOdmIoAddress s_VddIoUartAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDDIO_UART, 0 }
};

// VDDIO_SDMMC3
static const NvOdmIoAddress s_VddIoSdmmc3Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDDIO_SDMMC3, 0 }
};

// VDDIO_SDMMC4
static const NvOdmIoAddress s_VddIoSdmmc4Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDDIO_SDMMC4, 0 }
};

// AVdd_DSI_CSI
static const NvOdmIoAddress s_AVddVDsiCsiAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_DSI_CSI, 0 }
};

// VDD_LCD_DIS
static const NvOdmIoAddress s_VddLcdDisAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_VDD_LCD_DIS, 0 }
};

// AVDD_USB
static const NvOdmIoAddress s_AVddUsbAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, ArdbegPowerRails_AVDD_USB, 0 }
};

// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CPU },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_EMMC_CORE },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC4 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_RTC },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_AVDD_DSI_CSI },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_AVDD_USB },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_LCD_DIS },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VPP_FUSE},

};

// Ardbeg TPS51632 rails
static const NvOdmIoAddress s_ArdbegTps51632Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CPU },
};

//BQ2491X FFD- Battery Charger
static const NvOdmIoAddress s_Bq2419xFFDAddresses[] =
{
    { NvOdmIoModule_I2c, 1,  0xD6, 0 },
};

//BQ2491X - Battery Charger
static const NvOdmIoAddress s_Bq2419xAddresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0xD6, 0 },
};

//BQ2477X - Battery Charger
static const NvOdmIoAddress s_Bq2477xAddresses[] =
{
    { NvOdmIoModule_I2c, 0x1,  0xD4, 0 },
};

//Max17048 FFD - Fuel Gauge
static const NvOdmIoAddress s_Max17048FFDAddresses[] =
{
    { NvOdmIoModule_I2c, 0x1,  0x6C, 0 },
};

//Max17048 - Fuel Gauge
static const NvOdmIoAddress s_Max17048Addresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0x6C, 0 },
};

//Bq24z45 - Fuel Gauge
static const NvOdmIoAddress s_Bq24z45Addresses[] =
{
    { NvOdmIoModule_I2c, 0x1,  0x16, 0 },
};

//CW2015 - Fuel Gauge
static const NvOdmIoAddress s_CW2015Addresses[] =
{
    { NvOdmIoModule_I2c, 0x0,  0xC4, 0 },
};

// Ardbeg TPS80036 rails
static const NvOdmIoAddress s_ArdbegTps80036Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC4 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_AVDD_DSI_CSI },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_LCD_DIS },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VPP_FUSE},
};

// Ardbeg TPS65914 rails
static const NvOdmIoAddress s_ArdbegTps65914Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_AVDD_DSI_CSI },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VPP_FUSE},
};
// AS3722 rails
static const NvOdmIoAddress s_ArdbegAs3722Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CPU },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC4 },
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_AVDD_DSI_CSI},
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VPP_FUSE},
    { NvOdmIoModule_Vdd, 0x00,  ArdbegPowerRails_VDDIO_SDMMC3},
};

