/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include "nvodm_services_dev_i2c.h"

static NvOdmDevI2c s_OdmDevI2c = {NULL, NULL, 0};

NvOdmDevI2cHandle NvOdmDevI2cOpen(
    NvOdmIoModule OdmIoModuleId,
    NvU32 instance)
{
    NvOdmDevI2cHandle hOdmDevI2c = &s_OdmDevI2c;

    if (!hOdmDevI2c->OpenCount)
    {
        if (!hOdmDevI2c->hOdmI2c)
            hOdmDevI2c->hOdmI2c = NvOdmI2cOpen(OdmIoModuleId, instance);

        if (hOdmDevI2c->hOdmI2c && !hOdmDevI2c->hMutex)
            hOdmDevI2c->hMutex = NvOdmOsMutexCreate();

        if (hOdmDevI2c->hMutex)
        {
            hOdmDevI2c->OpenCount++;
            return hOdmDevI2c;
        }

        if (hOdmDevI2c->hOdmI2c)
            NvOdmI2cClose(hOdmDevI2c->hOdmI2c);

        NvOdmOsMemset(hOdmDevI2c, 0, sizeof(NvOdmDevI2c));
        return NULL;
    }
    hOdmDevI2c->OpenCount++;
    return hOdmDevI2c;
}

void NvOdmDevI2cClose(NvOdmDevI2cHandle hOdmDevI2c)
{
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return;
    }
    hOdmDevI2c->OpenCount--;
    if (!hOdmDevI2c->OpenCount)
    {
        NvOdmI2cClose(hOdmDevI2c->hOdmI2c);
        NvOdmOsMutexDestroy(hOdmDevI2c->hMutex);
        NvOdmOsMemset(hOdmDevI2c, 0, sizeof(NvOdmDevI2c));
    }
}

NvBool NvOdmDevI2cWrite8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Data)
{
    NvU8 WriteBuffer[2];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    WriteBuffer[0] = RegAddr & 0xFF;
    WriteBuffer[1] = Data & 0xFF;    // written data

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 2;

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo, 1,
                      SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;

    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cRead8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 *Data)
{
    NvU8 ReadBuffer[1];
    NvU8 WriteBuffer[1];
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // check the handle
    if ((hOdmDevI2c == NULL) || (Data == NULL))
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    // Write the internal register offset
    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo[0].NumBytes = 1;
    TransactionInfo[1].Address = DevAddr | 1;
    TransactionInfo[1].Buf = &WriteBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 1;

    // Read data from slave at the specified offset
    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        *Data = WriteBuffer[0];
        return NV_TRUE;
    }

    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cUpdate8(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 Value,
    NvU8 Mask)
{
    NvBool Status;
    NvU8 Data;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    NvOdmOsMutexLock(hOdmDevI2c->hMutex);

    Status = NvOdmDevI2cRead8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data = (Data & ~Mask) | (Value & Mask);
    Status = NvOdmDevI2cWrite8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmDevI2c->hMutex);
    return Status;
}

NvBool NvOdmDevI2cSetBits(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    NvBool Status;
    NvU8 Data;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    NvOdmOsMutexLock(hOdmDevI2c->hMutex);

    Status = NvOdmDevI2cRead8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data |= BitMask;
    Status = NvOdmDevI2cWrite8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmDevI2c->hMutex);
    return Status;
}

NvBool NvOdmDevI2cClrBits(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU8 BitMask)
{
    NvBool Status;
    NvU8 Data;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    NvOdmOsMutexLock(hOdmDevI2c->hMutex);

    Status = NvOdmDevI2cRead8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, &Data);
    if (!Status)
        goto End;

    Data &= ~BitMask;
    Status = NvOdmDevI2cWrite8(hOdmDevI2c, DevAddr, SpeedKHz, RegAddr, Data);

End:
    NvOdmOsMutexUnlock(hOdmDevI2c->hMutex);
    return Status;
}

NvBool NvOdmDevI2cWrite16(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 Data)
{
    NvU8 WriteBuffer[3];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    WriteBuffer[1] = (NvU8)((Data >>  8) & 0xFF);
    WriteBuffer[2] = (NvU8)(Data & 0xFF);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 3;

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cRead16(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU16 *Data)
{
    NvU8 ReadBuffer[3];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // check the handle
    if ((hOdmDevI2c == NULL) || (Data == NULL))
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                               NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;

    TransactionInfo[1].Address = (DevAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 2;

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo[0], 2,
                    SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        *Data = ((ReadBuffer[0] << 8) | ReadBuffer[1]);
        return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cWrite32(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 Data)
{
    NvU8 WriteBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo;

    // check the handle
    if (hOdmDevI2c == NULL)
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    WriteBuffer[1] = (NvU8)((Data >> 24) & 0xFF);
    WriteBuffer[2] = (NvU8)((Data >> 16) & 0xFF);
    WriteBuffer[3] = (NvU8)((Data >>  8) & 0xFF);
    WriteBuffer[4] = (NvU8)(Data & 0xFF);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = 5;

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
        return NV_TRUE;

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cRead32(
    NvOdmDevI2cHandle hOdmDevI2c,
    NvU32 DevAddr,
    NvU32 SpeedKHz,
    NvU8 RegAddr,
    NvU32 *Data)
{
    NvU8 ReadBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    // check the handle
    if ((hOdmDevI2c == NULL) || (Data == NULL))
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    ReadBuffer[0] = RegAddr & 0xFF;

    TransactionInfo[0].Address = DevAddr;
    TransactionInfo[0].Buf = &ReadBuffer[0];
    TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                               NVODM_I2C_USE_REPEATED_START;
    TransactionInfo[0].NumBytes = 1;

    TransactionInfo[1].Address = (DevAddr | 0x1);
    TransactionInfo[1].Buf = &ReadBuffer[0];
    TransactionInfo[1].Flags = 0;
    TransactionInfo[1].NumBytes = 4;

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo[0], 2,
                SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

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
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    return NV_FALSE;
}

NvBool NvOdmDevI2cWriteBlock(
    NvOdmDevI2cHandle hOdmDevI2c,
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

    // check the handle
    if ((hOdmDevI2c == NULL) || (Data == NULL))
    {
        NV_ASSERT(0);
        return NV_FALSE;
    }

    WriteBuffer[0] = (NvU8)(RegAddr & 0xFF);
    NvOdmOsMemcpy((void*)(WriteBuffer + 1),(void*)Data,DataSize);

    TransactionInfo.Address = DevAddr;
    TransactionInfo.Buf = &WriteBuffer[0];
    TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
    TransactionInfo.NumBytes = (DataSize + 1);

    status = NvOdmI2cTransaction(hOdmDevI2c->hOdmI2c, &TransactionInfo, 1,
                    SpeedKHz, I2C_DEV_TRANSACTION_TIMEOUT);

    if (status == NvOdmI2cStatus_Success)
    {
        NvOdmOsFree(WriteBuffer);
        return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NvOdmOsPrintf("%s(): Failed: Timeout: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         case NvOdmI2cStatus_SlaveNotFound:
            NvOdmOsPrintf("%s(): Failed: SlaveNotFound: Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, DevAddr, RegAddr);
            break;
         default:
            NvOdmOsPrintf("%s() Failed: Status= 0x%x, Slave Address= 0x%x, "
                          "Offset= 0x%x\n", __func__, status, DevAddr, RegAddr);
            break;
    }
    NvOdmOsFree(WriteBuffer);
    return NV_FALSE;
}
