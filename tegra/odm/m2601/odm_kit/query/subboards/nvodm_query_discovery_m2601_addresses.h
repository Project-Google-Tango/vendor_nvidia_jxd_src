/*
 * Copyright (c) 2011-2012 NVIDIA Corporation.  All rights reserved.
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
 *                 entries for the peripherals on m2601 module.
 */

#include "pmu/boards/m2601/nvodm_pmu_m2601_supply_info_table.h"

// Core voltage rail
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_CORE, 0 }  /* VDD_CTRL */
};

// ADVDD_Osc
static const NvOdmIoAddress s_AVddOscAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_OSC, 0 } /* VDD_1V8 */
};

// VDDIO_SYS
static const NvOdmIoAddress s_VddIoSysAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SYS, 0 } /* VDD_1V8 */
};

// AVdd_PEXB
static const NvOdmIoAddress s_AVddPexBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEXB, 0 } /* PEXVDD 1.05V */
};

// VDD_PEXB
static const NvOdmIoAddress s_VddPexBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_PEXB, 0 } /* PEXVDD 1.05V */
};

// AVdd_PEX_PLL
static const NvOdmIoAddress s_AVddPexPllAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEX_PLL, 0 } /* VDD_PEXPLL 1.05V */
};

// AVdd_PEXA
static const NvOdmIoAddress s_AVddPexAAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEXA, 0 } /* 1.05V */
};

// VDD_PEXA
static const NvOdmIoAddress s_VddPexAAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_PEXA, 0 } /* 1.05V */
};

// AVdd_PLLE
static const NvOdmIoAddress s_AVddPllEAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLE, 0 } /* VDD_PEXPLL 1.05V */
};

// AVdd_PLLA_P_C_S
static const NvOdmIoAddress s_AVddPllAPCSAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLL_A_P_C_S, 0 } /* 1.2V */
};

// AVdd_PLLM
static const NvOdmIoAddress s_AVddPllMAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLM, 0 } /* 1.2V */
};

// AVdd_PLLX
static const NvOdmIoAddress s_AVddPllXAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLX, 0 } /*  1.2V */
};

// VDDIO_UART 
static const NvOdmIoAddress s_VddIoUartAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_UART, 0 } /* 3V3 */
};

// VDDIO_DDR 
static const NvOdmIoAddress s_VddIoDdrAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_MEM_VDDIO_DDR, 0 } 
};

// VDD_DDR_RX
static const NvOdmIoAddress s_VddDdrRxAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_DDR_RX, 0 } /* 3V3 */
};

// VDDIO_SDMMC1
static const NvOdmIoAddress s_VddIoSdmmc1Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SDMMC1, 0 }
};

// VDDIO_SDMMC3
static const NvOdmIoAddress s_VddIoSdmmc3Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SDMMC3, 0 }
};

// PMU0
static const NvOdmIoAddress s_Pmu0Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x5A, 0 },
};

// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_CORE},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_OSC},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SYS},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEXB},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_PEXB},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEX_PLL},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PEXA},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_PEXA},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLE},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLL_A_P_C_S},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLM},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_AVDD_PLLX},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_UART},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_MEM_VDDIO_DDR},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDD_DDR_RX},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SDMMC1},
    { NvOdmIoModule_Vdd, 0x00, M2601PowerRails_VDDIO_SDMMC3},
};

// LCD Display
static const NvOdmIoAddress s_LcdDisplayAddresses[] =
{
    { NvOdmIoModule_Display, 0, 0, 0 }
};

// HDMI
static const NvOdmIoAddress s_HdmiAddresses[] =
{
    { NvOdmIoModule_Hdmi, 0, 0, 0 },

    // dummy entry for DDC
    { NvOdmIoModule_I2c, 0, 0, 0 }
};
