/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvodm_pmu.h"
#include "nvodm_services.h"
#include "tca6416_expander_i2c.h"
#include "tca6416_expander_reg.h"



#define TCA6416_SLAVE_ADDR      0x40    // (7'h20)
#define TCA6416_I2C_SPEED_KHZ   400


NvBool
Tca6416ConfigPortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 PortNo,
    NvU32 PinNo,
    GpioPinMode Mode)
{
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus Error;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;

    NvOdmI2cTransactionInfo TransactionInfo;
    static NvU8  ConfigPort1Val = 0xFF; //  set to default value
    static NvU8  ConfigPort2Val = 0xFF; //  set to default value

    if (PortNo == TCA6416_PORT_0)
    {
        WriteBuffer[0] =  TCA6416_CONFIG_PORT_0 & 0xFF;

        if (Mode == GpioPinMode_Output)
        {
            WriteBuffer[1] = ((ConfigPort1Val & (0xFF & (~(1 << PinNo)))) | (0x0 << PinNo));
        } else if (Mode == GpioPinMode_InputData)
        {
            WriteBuffer[1] = ((ConfigPort1Val & (0xFF & (~(1 << PinNo)))) | (0x1 << PinNo));
        }
        ConfigPort1Val = WriteBuffer[1];
    }else if (PortNo == TCA6416_PORT_1)
    {
        WriteBuffer[0] = TCA6416_CONFIG_PORT_1 & 0xFF;

        if (Mode == GpioPinMode_Output)
        {
            WriteBuffer[1] = (ConfigPort2Val & (0xFF & (~(1 << PinNo))));
        } else if (Mode == GpioPinMode_InputData)
        {
            WriteBuffer[1] = ((ConfigPort2Val & (0xFF & (~(1 << PinNo)))) | (0x1 << PinNo));
        }
        ConfigPort2Val = WriteBuffer[1];
    }

    TransactionInfo.Address = TCA6416_SLAVE_ADDR;
    TransactionInfo.Buf = WriteBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    // write the pmu Offset (from where data gpio need to be set)
    Error = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
                            TCA6416_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

    if (Error == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        switch (Error)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("Tca6416ConfigPortPin Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("Tca6416ConfigPortPin Failed: SlaveNotFound\n"));
                break;
        }
        return NV_FALSE;
    }
}


NvBool
Tca6416WritePortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 PortNo,
    NvU32 PinNo,
    GpioPinState data)
{
    NvOdmI2cTransactionInfo TransactionInfo;
    NvOdmI2cStatus Error;
    NvU8 WriteBuffer[2];
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;

    static NvU8  OutPut1Val = 0xFF; //  set to default value
    static NvU8  OutPut2Val = 0xFF; //  set to default value


    /* set the command byte */
    if (PortNo == TCA6416_PORT_0)
    {
        WriteBuffer[0] = TCA6416_OUTPUT_PORT_0 & 0xFF;
        // Set the data 
        WriteBuffer[1] =  ((OutPut1Val & (0xFF & (~(1 << PinNo)))) | (data << PinNo));

        OutPut1Val = WriteBuffer[1];

    } else if (PortNo == TCA6416_PORT_1)
    {
        WriteBuffer[0] = TCA6416_OUTPUT_PORT_1 & 0xFF;
        // Set the data
        WriteBuffer[1] =  ((OutPut2Val & (0xFF & (~(1 << PinNo)))) | (data << PinNo));

        OutPut2Val = WriteBuffer[1];
    }

    TransactionInfo.Address = TCA6416_SLAVE_ADDR;
    TransactionInfo.Buf = WriteBuffer;
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    Error = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
                    TCA6416_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

    if (Error == NvOdmI2cStatus_Success)
    {
        return NV_TRUE;
    }
    else
    {
        switch (Error)
        {
            case NvOdmI2cStatus_Timeout:
                NVODMPMU_PRINTF(("Tca6416I2cWrite8 Failed: Timeout\n")); 
                break;
             case NvOdmI2cStatus_SlaveNotFound:
             default:
                NVODMPMU_PRINTF(("Tca6416I2cWrite8 Failed: SlaveNotFound\n"));
                break;
        }
        return NV_FALSE;
    }
}

NvBool
Tca6416ReadPortPin(
    NvOdmPmuDeviceHandle hDevice,
    NvU32 PortNo,
    NvU32 PinNo,
    GpioPinState*State)
{

    // Need to implement
    return NV_TRUE;
}



