/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
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
 *                 entries for the peripherals on E1612 module.
 */

#include "pmu/boards/dalmore/nvodm_pmu_dalmore_supply_info_table.h"

// MAX77663
static const NvOdmIoAddress s_Max77663Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x78, 0 },
};

// TPS65090
static const NvOdmIoAddress s_Tps65090Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x90, 0 },
};

// TPS51632
static const NvOdmIoAddress s_Tps51632Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x86, 0 },
};

// TPS65914
static const NvOdmIoAddress s_Tps65914Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0xB0, 0 },
    { NvOdmIoModule_I2c, 0x4, 0xB2, 0 },
};

// VDD_CPU
static const NvOdmIoAddress s_VddCpuAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_CPU, 0 }
};

// VDD_CORE
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_CORE, 0 }
};

// VDD_DDR_HS
static const NvOdmIoAddress s_VddDdrHsAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_DDR_HS, 0 }
};

// VDD_EMMC_CORE
static const NvOdmIoAddress s_VddEmmcCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_EMMC_CORE, 0 }
};

// VDD_SENSOR_2V8
static const NvOdmIoAddress s_VddSensor2V8Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_SENSOR_2V8, 0 }
};

// VDD_RTC
static const NvOdmIoAddress s_VddRtcAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_RTC, 0 }
};

//VPP_FUSE
static const NvOdmIoAddress s_VppFuseAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VPP_FUSE, 0 }
};

// VDDIO_SDMMC1
static const NvOdmIoAddress s_VddIoSdmmc1Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDDIO_SDMMC1, 0 }
};

//FIX ME: Unused. Should be removed
// VDDIO_SYS
static const NvOdmIoAddress s_VddIoSysAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_Invalid, 0 }
};

//FIX ME: Unused. Should be removed
// VDDIO_BB
static const NvOdmIoAddress s_VddIoBBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_Invalid, 0 }
};

// VDDIO_UART
static const NvOdmIoAddress s_VddIoUartAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDDIO_UART, 0 }
};

// VDDIO_SDMMC3
static const NvOdmIoAddress s_VddIoSdmmc3Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDDIO_SDMMC3, 0 },
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDD_3V3_SD, 0 }
};

// PLLM
static const NvOdmIoAddress s_AVddPllmAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_AVDD_PLLM, 0 }
};

// PLLA_P_C
static const NvOdmIoAddress s_AVddPllApcAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_AVDD_PLLA_P_C, 0 }
};

// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_CPU },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_DDR },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_EMMC_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLA_P_C },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLM },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_DDR_HS },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_SENSOR_2V8 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDDIO_USB3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_MIPI },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_RTC },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM2 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_LCD_BL },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_MINICARD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_LVDS },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_SD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_COM },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_DVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VPP_FUSE},
};

// Dalmore TPS51632 rails
static const NvOdmIoAddress s_DalmoreTps51632Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_CPU },
};

// Dalmore MAX77663 rails
static const NvOdmIoAddress s_DalmoreMax77663Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_DDR },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_EMMC_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLA_P_C },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLM },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_DDR_HS },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_SENSOR_2V8 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDDIO_USB3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_MIPI },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_RTC },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM2 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_DVDD_LCD },
};

// Dalmore TPS65914 rails
static const NvOdmIoAddress s_DalmoreTps65914Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_DDR },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_EMMC_CORE },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLA_P_C },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_PLLM },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_DDR_HS },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_SENSOR_2V8 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDDIO_USB3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_MIPI },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_RTC },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM1 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_CAM2 },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_DVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VPP_FUSE},
};

// Dalmore TPS65090 rails
static const NvOdmIoAddress s_DalmoreTps65090Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_LCD_BL },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_MINICARD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_AVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_LVDS },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_SD },
    { NvOdmIoModule_Vdd, 0x00,  DalmorePowerRails_VDD_3V3_COM },
};

// VDD_DDR3L_1V35
static const NvOdmIoAddress s_VddDdr3L1V35Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_VDDIO_DDR, 0 }
};

// VDDIO_VI
static const NvOdmIoAddress s_VddIoViAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_Invalid, 0 }
};

static const NvOdmIoAddress s_AVddUsbAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_AVDDIO_USB3, 0 }
};

// Vcore LCD
static const NvOdmIoAddress s_VcoreLcdAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, DalmorePowerRails_AVDD_LCD, 0 }
};

// Key Pad
static const NvOdmIoAddress s_KeyPadAddresses[] =
{
    // instance = 1 indicates Column info.
    // instance = 0 indicates Row info.
    // address holds KBC pin number used for row/column.

    // All Row info has to be defined contiguously from 0 to max.
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow0, 0}, // Row 0
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow1, 0}, // Row 1
    { NvOdmIoModule_Kbd, 0x00, NvOdmKbcGpioPin_KBRow2, 0}, // Row 2

    // All Column info has to be defined contiguously from 0 to max.
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol0, 0}, // Column 0
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol1, 0}, // Column 1
};
