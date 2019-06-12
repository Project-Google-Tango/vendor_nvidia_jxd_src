/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 *
 * @file
 * <b>NVIDIA APX ODM Kit::
 *         Implementation of the ODM Peripheral Discovery API</b>
 *
 * @b Description: Specifies the peripheral connectivity database NvOdmIoAddress
 *                 entries for the peripherals on E1580 module.
 */

#include "pmu/boards/pluto/nvodm_pmu_pluto_supply_info_table.h"

// Tps65914
static const NvOdmIoAddress s_Tps65914Addresses[] =
{
    /* Page 1 */
    { NvOdmIoModule_I2c, 0x04, 0xB0, 0 },
    /* Page 2 */
    { NvOdmIoModule_I2c, 0x04, 0xB2, 0 },
    /* Page 3 */
    { NvOdmIoModule_I2c, 0x04, 0xB4, 0 },
};

//Max77665a
static const NvOdmIoAddress s_Max77665aAddresses[]=
{
    { NvOdmIoModule_I2c, 0x04, 0xCC, 0 }
};

//Max17047
static const NvOdmIoAddress s_Max17047Addresses[]=
{
    { NvOdmIoModule_I2c, 0x00, 0x6C, 0 }
};

// VDD_CPU
static const NvOdmIoAddress s_VddCpuAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_CPU, 0 }
};

// VDD_CORE
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_CORE, 0 }
};

// VDD_RTC
static const NvOdmIoAddress s_VddRtcAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_RTC, 0 }
};

//VPP_FUSE
static const NvOdmIoAddress s_VppFuseAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VPP_FUSE, 0 }
};

// VDDIO_SDMMC3
static const NvOdmIoAddress s_VddIoSdmmc3Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDDIO_SDMMC3, 0 }
};

// VDD_DDR3L_1V35
static const NvOdmIoAddress s_VddDdr3L1V35Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

// VDD_DDR_RX
static const NvOdmIoAddress s_VddDdrRxAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

// VDD_EMMC_CORE
static const NvOdmIoAddress s_VddEmmcCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_EMMC, 0 }
};

// VDD_3V3_SYS
static const NvOdmIoAddress s_Vdd3V3SysAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

//FIX ME: Unused. Should be removed
// VDDIO_SYS
static const NvOdmIoAddress s_VddIoSysAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

// VDDIO_UART
static const NvOdmIoAddress s_VddIoUartAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDDIO_UART, 0 }
};

//FIX ME: Unused. Should be removed
// VDDIO_BB
static const NvOdmIoAddress s_VddIoBBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

// VDDIO_LCD
static const NvOdmIoAddress s_VddIoLcdAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_AVDD_LCD, 0 }
};

// VDDIO_VI
static const NvOdmIoAddress s_VddIoViAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_Invalid, 0 }
};

// AVDD_USB
static const NvOdmIoAddress s_AVddUsbAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_AVDD_USB, 0 }
};

// Vcore LCD
static const NvOdmIoAddress s_VcoreLcdAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, PlutoPowerRails_VDD_LCD, 0 }
};


// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_CPU },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_CORE_BB },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDDIO_DDR },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_EMMC },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_USB },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_CSI_DSI_PLL },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_PLLM },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_PLLX },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_VDD_RTC },
    { NvOdmIoModule_Vdd, 0x00,  PlutoPowerRails_AVDD_DSI_CSI },
};

static const NvOdmIoAddress s_LvdsDisplayAddresses[] =
{
    { NvOdmIoModule_Display, 0, 0, 0 },
    { NvOdmIoModule_I2c, 0x00, 0xA0, 0 },
    { NvOdmIoModule_Pwm, 0x00, 0, 0 },
    { NvOdmIoModule_Vdd, 0x00, 0x06, 0 },
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
    { NvOdmIoModule_Kbd, 0x01, NvOdmKbcGpioPin_KBCol2, 0}, // Column 2
};

