/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
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

#include "pmu/boards/loki/nvodm_pmu_loki_supply_info_table.h"

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
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDD_CPU, 0 }
};

// VDD_CORE
static const NvOdmIoAddress s_VddCoreAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDD_CORE, 0 }
};

//VPP_FUSE
static const NvOdmIoAddress s_VppFuseAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VPP_FUSE, 0 }
};

// VDDIO_UART
static const NvOdmIoAddress s_VddIoUartAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDDIO_UART, 0 }
};

// VDDIO_SDMMC3
static const NvOdmIoAddress s_VddIoSdmmc3Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDDIO_SDMMC3, 0 }
};

// VDDIO_SDMMC4
static const NvOdmIoAddress s_VddIoSdmmc4Addresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_VDDIO_SDMMC4, 0 }
};

// AVdd_DSI_CSI
static const NvOdmIoAddress s_AVddVDsiCsiAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_DSI_CSI, 0 },
    { NvOdmIoModule_Vdd, 0x00, LokiPowerRails_AVDD_LCD, 0 },
};

// All rail
static const NvOdmIoAddress s_AllRailAddresses[] =
{
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDD_CPU },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_SDMMC4 },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_AVDD_DSI_CSI },
    { NvOdmIoModule_Vdd, 0x00,	LokiPowerRails_AVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VPP_FUSE},

};

// Loki TPS65914 rails
static const NvOdmIoAddress s_LokiTps65914Rails[] =
{
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDD_CORE },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_SDMMC4 },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_UART },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_AVDD_DSI_CSI },
    { NvOdmIoModule_Vdd, 0x00,	LokiPowerRails_AVDD_LCD },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VDDIO_SDMMC3 },
    { NvOdmIoModule_Vdd, 0x00,  LokiPowerRails_VPP_FUSE},
};

// ov7695 sensor
static const NvOdmIoAddress s_ffaImagerOV7695Addresses[] =
{
    { NvOdmIoModule_I2c, 0x02, 0x90, 0 },
    { NvOdmIoModule_VideoInput, 0x00, NVODM_CAMERA_SERIAL_CSI_D1A, 0 },
    { NvOdmIoModule_ExternalClock, 2, 0, 0 } // CSUS
};

// mt9m114 sensor
static const NvOdmIoAddress s_ffaImagerMT9M114Addresses[] =
{
    { NvOdmIoModule_I2c, 0x02, 0x42, 0 },
    { NvOdmIoModule_VideoInput, 0x00, NVODM_CAMERA_SERIAL_CSI_D1A, 0 },
    { NvOdmIoModule_ExternalClock, 2, 0, 0 } // CSUS
};
