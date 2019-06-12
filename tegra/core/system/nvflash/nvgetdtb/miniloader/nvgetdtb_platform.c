/*
 * Copyright (c) 2013, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvddk_i2c.h"
#include "aos_avp_pmu.h"
#include "nvrm_drf.h"
#include "nvgetdtb_platform.h"
#include "nvos.h"

#define NV_READ32(a) *((volatile NvU32 *)(a))
#define NV_WRITE32(a,d) *((volatile NvU32 *)(a)) = (d)

#define EEPROM_I2C_SLAVE_ADDRESS                0xAC
#define PMU_EEPROM_I2C_SLAVE_ADDRESS            0xAA
#define DISPLAY_EEPROM_I2C_SLAVE_ADDRESS        0xA2
#define AUDIO_EEPROM_I2C_SLAVE_ADDRESS          0xA0
#define CAMERA_EEPROM_I2C_SLAVE_ADDRESS         0xA8
#define SENSOR_EEPROM_I2C_SLAVE_ADDRESS         0xA8
#define NFC_EEPROM_I2C_SLAVE_ADDRESS            0xA6
#define DEBUG_EEPROM_I2C_SLAVE_ADDRESS          0xAE

#define SLAVE_FREQUENCY                         400   // KHz
#define SLAVE_TRANSACTION_TIMEOUT               1000  // Ms
#define IO_EXPANDER_SLAVE_ADDRESS               0x40

#define MAX77660_PMU_ADDR                       0x46
#define TPS80036_PMU_ADDR                       0xB0
#define TPS80036_GPIO_ADDR                      0xB2
#define TPS65913_GPIO_ADDR                      0xB2
#define TPS65913_PMU_ADDR                       0xB0

#define PINMUX_BASE                             0x70000000
#define PINMUX_AUX_GEN1_I2C_SDA_0               0x31A0
#define PINMUX_AUX_GEN1_I2C_SCL_0               0x31A4
#define PINMUX_AUX_PWR_I2C_SDA_0                0x32B8
#define PINMUX_AUX_PWR_I2C_SCL_0                0x32B4
#define PINMUX_AUX_I2C1_SDA_0                   0x346C
#define PINMUX_AUX_I2C1_SCL_0                   0x3468
#define PINMUX_AUX_CAM_I2C_SDA_0                0x3294
#define PINMUX_AUX_CAM_I2C_SCL_0                0x3290
#define PINMUX_AUX_I2C6_SCL_0                   0x3470
#define PINMUX_AUX_I2C6_SDA_0                   0x3474

#define APB_MISC_PA_BASE                        0x70000000
#define APB_MISC_GP_HIDREV_0                    0x804
#define APB_MISC_GP_HIDREV_0_CHIPID_SHIFT       8
#define APB_MISC_GP_HIDREV_0_CHIPID_RANGE       15:8

static NvDdkI2cInstance InstanceDataT11x[ ] =
    {
        NvDdkI2c1, // NvGetDtbBoardType_ProcessorBoard
        NvDdkI2c1, // NvGetDtbBoardType_PmuBoard
        NvDdkI2c1, // NvGetDtbBoardType_DisplayBoard
        NvDdkI2c1, // NvGetDtbBoardType_AudioCodec
        NvDdkI2c3, // NvGetDtbBoardType_CameraBoard
        NvDdkI2c1, // NvGetDtbBoardType_Sensor
        NvDdkI2c1, // NvGetDtbBoardType_NFC
        NvDdkI2c1  // NvGetDtbBoardType_Debug
    };

static NvDdkI2cInstance InstanceDataT12x[ ] =
    {
        NvDdkI2c1, // NvGetDtbBoardType_ProcessorBoard
        NvDdkI2c1, // NvGetDtbBoardType_PmuBoard
        NvDdkI2c1, // NvGetDtbBoardType_DisplayBoard
        NvDdkI2c6, // NvGetDtbBoardType_AudioCodec
        NvDdkI2c3, // NvGetDtbBoardType_CameraBoard
        NvDdkI2c1, // NvGetDtbBoardType_Sensor
        NvDdkI2c1, // NvGetDtbBoardType_NFC
        NvDdkI2c1  // NvGetDtbBoardType_Debug
    };

static NvDdkI2cInstance InstanceDataT14x[ ] =
    {
        NvDdkI2c1, // NvGetDtbBoardType_ProcessorBoard
        NvDdkI2c1, // NvGetDtbBoardType_PmuBoard
        NvDdkI2c1, // NvGetDtbBoardType_DisplayBoard
        NvDdkI2c6, // NvGetDtbBoardType_AudioCodec
        NvDdkI2c3, // NvGetDtbBoardType_CameraBoard
        NvDdkI2c1, // NvGetDtbBoardType_Sensor
        NvDdkI2c1, // NvGetDtbBoardType_NFC
        NvDdkI2c1  // NvGetDtbBoardType_Debug
    };

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

static NvDdkI2cSlaveHandle NvBlGetBoardSlaveT30(NvGetDtbBoardType BoardType)
{
    NvDdkI2cSlaveHandle hSlave = NULL;

    switch (BoardType)
    {
        case NvGetDtbBoardType_ProcessorBoard:
            NvDdkI2cDeviceOpen(NvDdkI2c5, EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
    }

    return hSlave;
}

static NvDdkI2cSlaveHandle NvBlGetBoardSlaveT1xx(
                                NvGetDtbBoardType BoardType,
                                NvDdkI2cInstance InstanceData[ ])
{
    NvDdkI2cSlaveHandle hSlave = NULL;

    switch (BoardType)
    {
        case NvGetDtbBoardType_ProcessorBoard:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_ProcessorBoard],
                    EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_PmuBoard:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_PmuBoard],
                    PMU_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_DisplayBoard:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_DisplayBoard],
                    DISPLAY_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_AudioCodec:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_AudioCodec],
                    AUDIO_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_CameraBoard:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_CameraBoard],
                    CAMERA_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_Sensor:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_Sensor],
                    SENSOR_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_NFC:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_NFC],
                    NFC_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;

        case NvGetDtbBoardType_Debug:
            NvDdkI2cDeviceOpen(
                    InstanceData[NvGetDtbBoardType_Debug],
                    DEBUG_EEPROM_I2C_SLAVE_ADDRESS, 0,
                    SLAVE_FREQUENCY, SLAVE_TRANSACTION_TIMEOUT, &hSlave);
            break;
    }

    return hSlave;
}

static NvDdkI2cSlaveHandle NvBlGetBoardSlave(NvGetDtbBoardType BoardType)
{
    NvU32 ChipId = 0;
    NvDdkI2cSlaveHandle hSlave = NULL;

    ChipId = NvBlGetChipId();
    switch (ChipId)
    {
        case 0x30:
            hSlave = NvBlGetBoardSlaveT30(BoardType);
            break;
        case 0x35:
            hSlave = NvBlGetBoardSlaveT1xx(BoardType, InstanceDataT11x);
            break;
        case 0x14:
            hSlave = NvBlGetBoardSlaveT1xx(BoardType, InstanceDataT14x);
            break;
        case 0x40:
            hSlave = NvBlGetBoardSlaveT1xx(BoardType, InstanceDataT12x);
    }

    return hSlave;
}

NvError NvGetdtbReadBoardInfo(NvGetDtbBoardType BoardType, NvGetDtbBoardInfo *pBoardInfo)
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

void NvGetdtbInitI2cPinmux(void)
{
    NvU32 ChipId = NvBlGetChipId();

    switch (ChipId)
    {
        case 0x30:
        case 0x35:
        case 0x40:
            // PinMux for GEN1_I2C
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_GEN1_I2C_SDA_0, 0x60);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_GEN1_I2C_SCL_0, 0x60);

            // PinMux for CAM_I2C
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_CAM_I2C_SDA_0, 0x61);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_CAM_I2C_SCL_0, 0x61);
            break;
        case 0x14:
            // PinMux for I2C1
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_I2C1_SDA_0, 0x60);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_I2C1_SCL_0, 0x60);

            // PinMux for CAMI2C
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_CAM_I2C_SDA_0, 0x60);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_CAM_I2C_SCL_0, 0x60);

            // PinMux for I2C6
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_I2C6_SCL_0, 0x60);
            NV_WRITE32(PINMUX_BASE + PINMUX_AUX_I2C6_SDA_0, 0x60);
            break;
    }

    NV_WRITE32(PINMUX_BASE + PINMUX_AUX_PWR_I2C_SDA_0, 0x60);
    NV_WRITE32(PINMUX_BASE + PINMUX_AUX_PWR_I2C_SCL_0, 0x60);
}

static NvError NvBlWriteToPmu(NvU16 Address, NvU8 Offset, NvU8 Value)
{
    NvDdkI2cSlaveHandle hSlave = NULL;
    NvError Error = NvSuccess;

    Error = NvDdkI2cDeviceOpen(NvDdkI2c5, Address, 0, SLAVE_FREQUENCY,
                               SLAVE_TRANSACTION_TIMEOUT, &hSlave);
    if (Error != NvSuccess)
        goto fail;

    Error = NvDdkI2cDeviceWrite(hSlave, Offset, &Value, 1);

fail:
    if (hSlave != NULL)
        NvDdkI2cDeviceClose(hSlave);

    return Error;
}

NvError NvGetdtbEepromPowerRail(void)
{
    NvU32 ChipId = NvBlGetChipId();
    NvError Error = NvSuccess;

    switch (ChipId)
    {
        case 0x14:
            /* Enable SW3 for Ceres to read board-id */
            Error = NvBlWriteToPmu(MAX77660_PMU_ADDR, 0x43, 0x0D);
            if (Error != NvSuccess)
                goto fail;

            /* Enable RegEN5 for Atlantis to read board-id */
            Error = NvBlWriteToPmu(TPS80036_PMU_ADDR, 0xE8, 0x01);
            break;
        default:
            break;
    }

fail:
    return Error;
}

NvError NvGetdtbInitGPIOExpander(void)
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

void NvGetdtbEnablePowerRail(void)
{
    NvGetDtbBoardInfo BoardInfo;
    NvGetdtbReadBoardInfo(NvGetDtbBoardType_ProcessorBoard, &BoardInfo);
    switch (BoardInfo.BoardId)
    {
        case NvGetDtbPlatformType_Pluto:
              // Enable GPIO1, GPIO2 & GPIO4 for CAM & DSI
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x81, 0x16);
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x82, 0x16);
              break;
        case NvGetDtbPlatformType_Dalmore:
              // Enable GPI05 & GPI06 for AUDIO & CAM
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0xFB, 0x42);
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0xFE, 0x01);
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x81, 0x50);
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x82, 0x40);
              break;
        case NvGetDtbPlatformType_Atlantis:
              // LDO12 for DIS 1V8
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x70, 0x13);
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x6F, 0x41);
              // LDO13 for CAM 1V8
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x72, 0x13);
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x71, 0x41);
              break;
        case NvGetDtbPlatformType_Macallan:
              // Power up panel EEPROM for read/write operation
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0xFB, 0x00);
              // Enable GPIO4 & GPIO6 for DIS & CAM
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x81, 0xD0);
              NvBlWriteToPmu(TPS80036_GPIO_ADDR, 0x82, 0xD0);
              // LDO4 for VD_CAM_LV
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x57, 0x07);
              NvBlWriteToPmu(TPS80036_PMU_ADDR, 0x56, 0x01);
              break;
        case NvGetDtbPlatformType_Ceres:
              // Enable SW 5, 3, 2 & 1
              NvBlWriteToPmu(MAX77660_PMU_ADDR, 0x43, 0x0F);
              break;
        case NvGetDtbPlatformType_Ardbeg:
              // AMP_SHDN, EN_AUD_1V2
              NvBlWriteToPmu(TPS65913_GPIO_ADDR, 0x81, 0xFE);
              NvBlWriteToPmu(TPS65913_GPIO_ADDR, 0x82, 0x42);
              // EN_AVDD_LCD, EN_DIS_1V2, VDD_LCD_1V8_EN
              NvBlWriteToPmu(TPS65913_GPIO_ADDR, 0x82, 0x98);
              //VDD_CAM_1V8=1.8V
              NvBlWriteToPmu(TPS65913_PMU_ADDR, 0x5B, 0x13);
              NvBlWriteToPmu(TPS65913_PMU_ADDR, 0x5A, 0x01);
              break;
        default:
             break;
   }
   // Allow enough delay for power rails to be enabled
   NvOsWaitUS(5000);
}
