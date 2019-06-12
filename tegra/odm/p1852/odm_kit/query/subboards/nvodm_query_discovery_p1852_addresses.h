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
 *                 entries for the peripherals on p1852 module.
 */

#include "pmu/boards/p1852/nvodm_pmu_p1852_supply_info_table.h"

// Core voltage rail
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_CORE, 0 }  /* VDD_CTRL */
};

// ADVDD_Osc
static const NvOdmIoAddress s_AVddOscAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_OSC, 0 } /* VDD_1V8_PLL */
};

// VDDIO_SYS
static const NvOdmIoAddress s_VddIoSysAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDDIO_SYS, 0 } /* VDD_1V8 */
};

// AVdd_PEXB
static const NvOdmIoAddress s_AVddPexBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEXB, 0 } /* PEXVDD 1.05V */
};

// VDD_PEXB
static const NvOdmIoAddress s_VddPexBAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_PEXB, 0 } /* PEXVDD 1.05V */
};

// AVdd_PEX_PLL
static const NvOdmIoAddress s_AVddPexPllAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEX_PLL, 0 } /* VDD_PEXPLL 1.05V */
};

// AVdd_PEXA
static const NvOdmIoAddress s_AVddPexAAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEXA, 0 } /* 1.05V */
};

// VDD_PEXA
static const NvOdmIoAddress s_VddPexAAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_PEXA, 0 } /* 1.05V */
};

// AVdd_PLLE
static const NvOdmIoAddress s_AVddPllEAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLE, 0 } /* VDD_PEXPLL 1.05V */
};

// AVdd_PLLA_P_C_S
static const NvOdmIoAddress s_AVddPllAPCSAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLL_A_P_C_S, 0 } /* VDD_MEM 1.2V */
};

// AVdd_PLLM
static const NvOdmIoAddress s_AVddPllMAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLM, 0 } /* VDD_MEM 1.2V */
};

// AVdd_PLLX
static const NvOdmIoAddress s_AVddPllXAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLX, 0 } /* VDD_MEM 1.2V */
};

// PMU0
static const NvOdmIoAddress s_Pmu0Addresses[] =
{
    { NvOdmIoModule_I2c, 0x4, 0x5A, 0 },
};

// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_CORE},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_OSC},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDDIO_SYS},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEXB},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_PEXB},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEX_PLL},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PEXA},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_VDD_PEXA},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLE},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLL_A_P_C_S},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLM},
    { NvOdmIoModule_Vdd, 0x00, P1852PowerRails_AVDD_PLLX},
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
