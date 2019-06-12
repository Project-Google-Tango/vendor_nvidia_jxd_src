/*
 * Copyright (c) 2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */


/* Implements I2C interface for the  Display driver */

#include "nvcommon.h"
#include "nvodm_disp_i2c.h"
#include "nvodm_services.h"
#include "nvos.h"

#define TOSHIBA_TC358770_REG_SIZE 2

NvBool NvOdmDispI2cWrite32(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU16 RegAddr,
    NvU32 Data)
{
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvU8 WriteBuffer[6];

    NvOdmI2cTransactionInfo TransactionInfo = {0 , 0, 0, 0};

    WriteBuffer[0] = RegAddr & 0xFF;    // PMU offset
    WriteBuffer[1] = (RegAddr >> 8) & 0xFF;

    NvOsMemcpy(&WriteBuffer[2], &Data, 4); //Copy data

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 6;

    status = NvOdmI2cTransaction(hOdmDispI2c, &TransactionInfo, 1,
                      SpeedKHz, I2C_DISP_TRANSACTION_TIMEOUT);

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

NvBool NvOdmDispI2cRead32(
    NvOdmServicesI2cHandle hOdmDispI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU16 RegAddr,
    NvU32 *Data)
{
    NvU8 ReadBuffer[2];
    NvU8 WriteBuffer[4];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer[0] = RegAddr & 0xFF;
    ReadBuffer[1] = (RegAddr >> 8) & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 2;
    TransactionInfo[1].Address = DevAddr | 1;
    TransactionInfo[1].Buf = &WriteBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 4;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(hOdmDispI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_DISP_TRANSACTION_TIMEOUT);

    if (status != NvOdmI2cStatus_Success)
    {
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
                NvOdmOsPrintf("%s() Failed: status 0x%x\n",  __func__, status);
                break;
        }
        return NV_FALSE;
    }
    NvOsMemcpy (Data, WriteBuffer, 4);
    return NV_TRUE;
}
