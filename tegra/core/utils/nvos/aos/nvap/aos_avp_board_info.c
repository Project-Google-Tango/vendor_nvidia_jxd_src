/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "aos_avp_board_info.h"
#include "nvddk_i2c.h"
#include "aos_avp_pmu.h"
#include "nvrm_drf.h"
#include "bootloader.h"

#define EEPROM_I2C_SLAVE_ADDRESS     0xAC
#define PMU_EEPROM_I2C_SLAVE_ADDRESS 0xAA
#define DISPLAY_SLAVE_ADDRESS        0xA2
#define SLAVE_FREQUENCY              400   // KHz
#define SLAVE_TRANSACTION_TIMEOUT    1000  // Ms
#define IO_EXPANDER_SLAVE_ADDRESS    0x40

#define MAX77660_I2C_ADDR  0x46
#define TPS80036_I2C_ADDR  0xB0

#define PINMUX_BASE                   0x70000000
#define PINMUX_AUX_GEN1_I2C_SDA_0     0x31A0
#define PINMUX_AUX_GEN1_I2C_SCL_0     0x31A4
#define PINMUX_AUX_PWR_I2C_SDA_0      0x32B8
#define PINMUX_AUX_PWR_I2C_SCL_0      0x32B4
#define PINMUX_AUX_I2C1_SDA_0         0x346C
#define PINMUX_AUX_I2C1_SCL_0         0x3468

#define APB_MISC_PA_BASE                    0x70000000
#define APB_MISC_GP_HIDREV_0                0x804
#define APB_MISC_GP_HIDREV_0_CHIPID_SHIFT   8
#define APB_MISC_GP_HIDREV_0_CHIPID_RANGE   15:8

/* TODO: Move it to a common lib */
static NvU32 NvBlGetChipId(void)
{
    NvU32 RegData;
    static NvU32 s_ChipId = 0;

    if (s_ChipId == 0)
    {
        RegData = NV_READ32(APB_MISC_PA_BASE + APB_MISC_GP_HIDREV_0);
        s_ChipId = NV_DRF_VAL(APB_MISC, GP_HIDREV, CHIPID, RegData);
    }

    return s_ChipId;
}

static NvDdkI2cSlaveHandle NvBlGetBoardSlaveT30(NvBoardType BoardType)
{
    NvDdkI2cSlaveHandle hSlave = NULL;
    switch (BoardType)
    {
        case NvBoardType_ProcessorBoard:
            NvDdkI2cDeviceOpen(NvDdkI2c5, EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
        default:
            return NULL; // invalid board type
    }
    return hSlave;
}

static NvDdkI2cSlaveHandle NvBlGetBoardSlaveT1xx(NvBoardType BoardType)
{
    NvDdkI2cSlaveHandle hSlave = NULL;
    switch (BoardType)
    {
        case NvBoardType_ProcessorBoard:
            NvDdkI2cDeviceOpen(NvDdkI2c1, EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
        case NvBoardType_PmuBoard:
            NvDdkI2cDeviceOpen(NvDdkI2c1, PMU_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
        case NvBoardType_DisplayBoard:
            NvDdkI2cDeviceOpen(NvDdkI2c1, DISPLAY_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
        default:
            return NULL; // invalid board type
    }
    return hSlave;
}

static NvDdkI2cSlaveHandle NvBlGetBoardSlave(NvBoardType BoardType)
{
    NvU32 ChipId = NvBlGetChipId();
    if (ChipId == 0x30)
        return NvBlGetBoardSlaveT30(BoardType);
    else
        return NvBlGetBoardSlaveT1xx(BoardType);
}

NvError NvBlReadBoardInfo(NvBoardType BoardType, NvBoardInfo *pBoardInfo)
{
    NvError Error = NvSuccess;
    NvDdkI2cSlaveHandle hSlave = NULL;

    if (pBoardInfo == NULL)
    {
        Error = NvError_BadParameter;
        goto fail;
    }

    hSlave = NvBlGetBoardSlave(BoardType);
    if (hSlave == NULL)
    {
        Error = NvError_NotInitialized;
        goto fail;
    }

    Error = NvDdkI2cDeviceRead(hSlave, 0, (NvU8 *)pBoardInfo,
                               sizeof(*pBoardInfo));
fail:
    if (hSlave != NULL)
        NvDdkI2cDeviceClose(hSlave);

    return Error;
}

void NvBlInitI2cPinmux(void)
{
    NvU32 ChipId = NvBlGetChipId();

    switch (ChipId)
    {
        case 0x30:
        case 0x35:
        case 0x40:
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_GEN1_I2C_SDA_0, 0x60);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_GEN1_I2C_SCL_0, 0x60);
            break;
    }

    NV_WRITE32(PINMUX_BASE + PINMUX_AUX_PWR_I2C_SDA_0, 0x60);
    NV_WRITE32(PINMUX_BASE + PINMUX_AUX_PWR_I2C_SCL_0, 0x60);
}

NvError NvBlInitGPIOExpander(void)
{
    NvU32 ChipId = NvBlGetChipId();
    NvDdkI2cSlaveHandle hSlave = NULL;
    NvError Error = NvSuccess;
    NvU8 Value = 0;

    if (ChipId != 0x40)
        goto fail;

    Error = NvDdkI2cDeviceOpen(NvDdkI2c1, IO_EXPANDER_SLAVE_ADDRESS, 0,
                       SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
    if (Error != NvSuccess)
        goto fail;

    //Port Expander P6 -> VDD_LCD_1V8_EN -> Load SW1 -> VDD_LCD_1V8B_LDO12
    Value = 0xB2;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x03, &Value, 1);
    if (Error != NvSuccess)
        goto fail;

    Value = 0;
    Error = NvDdkI2cDeviceRead(hSlave, 0x01, &Value, 1);
    if (Error != NvSuccess)
        goto fail;

    Value = Value | 0x40;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x01, &Value, 1);

    //Port Expander P0 -> EN_DIS_1V2
    Value = 0xB2;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x03, &Value, 1);
    if (Error != NvSuccess)
        goto fail;

    Value = 0;
    Error = NvDdkI2cDeviceRead(hSlave, 0x01, &Value, 1);
    if (Error != NvSuccess)
        goto fail;

    Value = Value | 0x1;
    Error = NvDdkI2cDeviceWrite(hSlave, 0x01, &Value, 1);

fail:
    if (hSlave != NULL)
        NvDdkI2cDeviceClose(hSlave);

    return Error;
}

