/*
 * Copyright (c) 2011-2013 NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */
 /**
  * @file: <b>NVIDIA Tegra ODM Kit: PMU-I2c Interface implementation</b>
  *
  * @b Description: Implements the i2c interface for the pmu drivers.
  *
  */

#include "pmu_i2c/nvodm_pmu_i2c.h"

#define I2C_PMU_TRANSACTION_TIMEOUT 1000
#define MAX_I2C_INSTANCES            5


typedef struct NvOdmPmuI2cRec {
    NvOdmServicesI2cHandle hOdmI2c;
    NvOdmOsMutexHandle hMutex;
    NvU32 OpenCount;
} NvOdmPmuI2c;

static NvOdmPmuI2c s_OdmPmuI2c[MAX_I2C_INSTANCES] = {{NULL, NULL, 0}, {NULL, NULL, 0}, \
                    {NULL, NULL, 0}, {NULL, NULL, 0}, {NULL, NULL, 0}};

NvOdmPmuI2cHandle NvOdmPmuI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance)
{
    NvOdmPmuI2cHandle hOdmPmuI2c = &s_OdmPmuI2c[instance];
    if (!hOdmPmuI2c->OpenCount)
    {
        if (!hOdmPmuI2c->hOdmI2c)
            hOdmPmuI2c->hOdmI2c = NvOdmI2cOpen(OdmIoModuleId, instance);
        if (hOdmPmuI2c->hOdmI2c && !hOdmPmuI2c->hMutex)
            hOdmPmuI2c->hMutex = NvOdmOsMutexCreate();

        if (hOdmPmuI2c->hMutex)
        {
            hOdmPmuI2c->OpenCount++;
            return hOdmPmuI2c;
        }

        if (hOdmPmuI2c->hOdmI2c)
            NvOdmI2cClose(hOdmPmuI2c->hOdmI2c);

        NvOdmOsMemset(hOdmPmuI2c, 0, sizeof(NvOdmPmuI2c));
        return NULL;
    }
    hOdmPmuI2c->OpenCount++;
    return hOdmPmuI2c;
}

void NvOdmPmuI2cClose(NvOdmPmuI2cHandle hOdmPmuI2c)
{
    if (hOdmPmuI2c->OpenCount)
    {
        hOdmPmuI2c->OpenCount--;
        if (!hOdmPmuI2c->OpenCount)
        {
            NvOdmI2cClose(hOdmPmuI2c->hOdmI2c);
            NvOdmOsMutexDestroy(hOdmPmuI2c->hMutex);
            NvOdmOsMemset(hOdmPmuI2c, 0, sizeof(NvOdmPmuI2c));
        }
    }
}

void NvOdmPmuI2cDeInit(void)
{
    NvOdmPmuI2cHandle hOdmPmuI2c;
    NvU32 instance;

    for (instance = 0; instance <= 4; instance++)
    {
        hOdmPmuI2c = &s_OdmPmuI2c[instance];

        while (hOdmPmuI2c->OpenCount)
            NvOdmPmuI2cClose(hOdmPmuI2c);
    }
}

NvBool NvOdmPmuI2cWrite8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data)
{
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo = {0};

    WriteBuffer[0] = RegAddr & 0xFF;    // PMU offset
    WriteBuffer[1] = Data & 0xFF;    // written data

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo, 1,
                      SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

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

NvBool NvOdmPmuI2cRead8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data)
{
    NvU8 ReadBuffer[1];
    NvU8 WriteBuffer[1];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // Write the PMU offset
    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;
    TransactionInfo[1].Address = DevAddr | 1;
    TransactionInfo[1].Buf = &WriteBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    // Read data from PMU at the specified offset
    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

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

    *Data = WriteBuffer[0];
    return NV_TRUE;
}

NvBool NvOdmPmuI2cUpdate8(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask)
{
    NvBool Status;
    NvU8 Data;

    NvOdmOsMutexLock(hOdmPmuI2c->hMutex);

    Status = NvOdmPmuI2cRead8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data = (Data & ~Mask) | (Value & Mask);
    Status = NvOdmPmuI2cWrite8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmPmuI2c->hMutex);
    return Status;
}

NvBool NvOdmPmuI2cSetBits(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    NvBool Status;
    NvU8 Data;

    NvOdmOsMutexLock(hOdmPmuI2c->hMutex);

    Status = NvOdmPmuI2cRead8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data |= BitMask;
    Status = NvOdmPmuI2cWrite8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmPmuI2c->hMutex);
    return Status;
}

NvBool NvOdmPmuI2cClrBits(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    NvBool Status;
    NvU8 Data;

    NvOdmOsMutexLock(hOdmPmuI2c->hMutex);

    Status = NvOdmPmuI2cRead8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data &= ~BitMask;
    Status = NvOdmPmuI2cWrite8(hOdmPmuI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmPmuI2c->hMutex);
    return Status;
}

NvBool NvOdmPmuI2cWrite16(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 Data)
{
    NvU8 WriteBuffer[3];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    WriteBuffer[1] = (NvU8)((Data >>  8) & 0xFF);
    WriteBuffer[2] = (NvU8)(Data & 0xFF);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 3;

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;

    // Transaction Error
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

NvBool NvOdmPmuI2cRead16(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 *Data)
{
    NvU8 ReadBuffer[3];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;

    TransactionInfo[1].Address = (DevAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 2;

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo[0], 2,
                    SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        *Data = ((ReadBuffer[0] << 8) | ReadBuffer[1]);
        return NV_TRUE;
    }

    // Transaction Error
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

NvBool NvOdmPmuI2cWrite32(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 Data)
{
    NvU8 WriteBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    WriteBuffer[1] = (NvU8)((Data >> 24) & 0xFF);
    WriteBuffer[2] = (NvU8)((Data >> 16) & 0xFF);
    WriteBuffer[3] = (NvU8)((Data >>  8) & 0xFF);
    WriteBuffer[4] = (NvU8)(Data & 0xFF);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 5;

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;

    // Transaction Error
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

NvBool NvOdmPmuI2cRead32(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 *Data)
{
    NvU8 ReadBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;

    TransactionInfo[1].Address = (DevAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 4;

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        *Data = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) |
                (ReadBuffer[2] << 8) | ReadBuffer[3];

        return NV_TRUE;
    }


    // Transaction Error
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

NvBool NvOdmPmuI2cWriteBlock(
    NvOdmPmuI2cHandle hOdmPmuI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data,
    NvU32 DataSize)
{
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    NvU8 *WriteBuffer = (NvU8*)NvOdmOsAlloc(DataSize + 1);
    NV_ASSERT(WriteBuffer);

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    NvOdmOsMemcpy((void*)(WriteBuffer + 1),(void*)Data,DataSize);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = (DataSize + 1);

    status = NvOdmI2cTransaction(hOdmPmuI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_PMU_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        NvOdmOsFree(WriteBuffer);
        return NV_TRUE;
    }

    // Transaction Error
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
    NvOdmOsFree(WriteBuffer);
    return NV_FALSE;
}
