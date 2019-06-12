/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include "nvodm_disp.h"
#include "panel_lg_dsi.h"
#include "nvodm_services.h"
#include "nvodm_query_pinmux.h"
#include "nvodm_query_discovery.h"
#include "nvodm_query_gpio.h"
#include "nvos.h"
#include "nvodm_sdio.h"
#include "nvodm_services.h"
#include "nvodm_pmu.h"
#include "nvodm_backlight.h"
#include "bl_led_max8831.h"

static NvOdmServicesI2cHandle hOdmI2c = NULL;

static NvBool NvOdmDispI2cWrite8(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data)
{
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvU8 WriteBuffer[2];
    NvOdmI2cTransactionInfo TransactionInfo = {0 , 0, 0, 0};

    if (hOdmDispI2c == NULL)
    {
        NvOdmOsPrintf("%s(): Error: Null Handle\n", __func__);
        return NV_FALSE;
    }

    WriteBuffer[0] = RegAddr & 0xFF;    // Internal Register Address
    WriteBuffer[1] = Data & 0xFF;    // data byte to write
    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;
    status = NvOdmI2cTransaction(hOdmDispI2c, &TransactionInfo, 1,
        SpeedKHz, 1000);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout\n", __func__);
            break;
        case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound slave Add 0x%x\n",
                __func__, DevAddr);
            break;
        default:
            NvOdmOsPrintf("%s() Failed: status 0x%x\n", __func__, status);
            break;
    }
    return NV_FALSE;
}

NvBool Max8831_BacklightInit(void)
{
    // check if already initialzed
    if (hOdmI2c != NULL)
       return NV_TRUE;

    // open i2c instance corresponding to Max8831
    hOdmI2c = NvOdmI2cOpen(NvOdmIoModule_I2c, MAX8831_I2C_INSTANCE);
    if (hOdmI2c != NULL)
       return NV_TRUE;
    else
       return NV_FALSE;
}

void Max8831_SetBacklightIntensity(NvU8 Intensity)
{
    NvU32 RegValue;

    if (hOdmI2c == NULL)
    {
        NvOdmOsPrintf("%s(): Error: Null Handle\n", __func__);
        return;
    }

    if (Intensity != 0)
    {
        // Enable LED1 and LED2 by programming ONFF_CNTL reg
        NvOdmDispI2cWrite8(hOdmI2c, MAX8831_I2C_SLAVE_ADDR_WRITE,
                              MAX8831_I2C_SPEED_KHZ,
                              MAX8831_REG_ONOFF_CNTL,
                              MAX8831_ENB_LED1 | MAX8831_ENB_LED2);

        // calculate RegValue(range 0x00 to 0x7F) based on
        // intensity(range 0 to 255)
        RegValue = MAX8831_ILED_CNTL_VALUE_MAX * (Intensity*100/ 255);
        RegValue /= 100;
        if (RegValue > MAX8831_ILED_CNTL_VALUE_MAX)
            RegValue = MAX8831_ILED_CNTL_VALUE_MAX;

        // write the RegValue to ILED1_CNTL register
        NvOdmDispI2cWrite8(hOdmI2c, MAX8831_I2C_SLAVE_ADDR_WRITE,
                              MAX8831_I2C_SPEED_KHZ,
                              MAX8831_REG_ILED1_CNTL,
                              (NvU8)RegValue);

        // write the RegValue to ILED2_CNTL register
        NvOdmDispI2cWrite8(hOdmI2c, MAX8831_I2C_SLAVE_ADDR_WRITE,
                              MAX8831_I2C_SPEED_KHZ,
                              MAX8831_REG_ILED2_CNTL,
                              (NvU8)RegValue);
    }
    else
    {
         // intensity is zero, so disable LED1 and LED2
         NvOdmDispI2cWrite8(hOdmI2c, MAX8831_I2C_SLAVE_ADDR_WRITE,
                              MAX8831_I2C_SPEED_KHZ,
                              MAX8831_REG_ONOFF_CNTL,
                              0);
    }
}

void Max8831_BacklightDeinit(void)
{
    if (hOdmI2c == NULL)
    {
        NvOdmOsPrintf("%s(): Error: Null Handle\n", __func__);
        return;
    }

    // disable LED1 and LED2
    NvOdmDispI2cWrite8(hOdmI2c, MAX8831_I2C_SLAVE_ADDR_WRITE,
                       MAX8831_I2C_SPEED_KHZ,
                       MAX8831_REG_ONOFF_CNTL,
                       0);

    // close i2c
    NvOdmI2cClose(hOdmI2c);
    hOdmI2c = NULL;
}
