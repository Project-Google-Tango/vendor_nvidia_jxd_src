/*
 * Copyright (c) 2009 - 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "max8907b.h"
#include "max8907b_i2c.h"
#include "max8907b_reg.h"

#define MAX8907B_I2C_SPEED_KHZ   400
#define MAX8907B_I2C_RETRY_CNT     2

NvBool Max8907bI2cWrite8(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU8 Data)
{
    NvU32 i;
    NvU8 WriteBuffer[2];
    NvOdmI2cTransactionInfo TransactionInfo;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        WriteBuffer[0] = Addr & 0xFF;   // PMU offset
        WriteBuffer[1] = Data & 0xFF;   // written data

        TransactionInfo.Address = hPmu->DeviceAddr;
        TransactionInfo.Buf = &WriteBuffer[0];
        TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
        TransactionInfo.NumBytes = 2;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
            return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bI2cRead8(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU8 *Data)
{
    NvU32 i;
    NvU8 ReadBuffer = 0;
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        // Write the PMU offset
        ReadBuffer = Addr & 0xFF;

        TransactionInfo[0].Address = hPmu->DeviceAddr;
        TransactionInfo[0].Buf = &ReadBuffer;
        TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                                   NVODM_I2C_USE_REPEATED_START;
        TransactionInfo[0].NumBytes = 1;

        TransactionInfo[1].Address = (hPmu->DeviceAddr | 0x1);
        TransactionInfo[1].Buf = &ReadBuffer;
        TransactionInfo[1].Flags = 0;
        TransactionInfo[1].NumBytes = 1;

        // Read data from PMU at the specified offset
        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo[0], 2,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
        {
            *Data = ReadBuffer;
            return NV_TRUE;
        }
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bAdcI2cWrite8(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU8 Data)
{
    NvU32 i;
    NvU8 WriteBuffer[2];
    NvOdmI2cTransactionInfo TransactionInfo;
    NvOdmI2cStatus status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        WriteBuffer[0] = Addr & 0xFF;   // PMU offset
        WriteBuffer[1] = Data & 0xFF;   // written data

        TransactionInfo.Address = MAX8907B_ADC_SLAVE_ADDR;
        TransactionInfo.Buf = &WriteBuffer[0];
        TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
        TransactionInfo.NumBytes = 2;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
            return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite8 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bAdcI2cRead8(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU8 *Data)
{
    NvU32 i;
    NvU8 ReadBuffer = 0;
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        // Write the PMU offset
        ReadBuffer = Addr & 0xFF;

        TransactionInfo[0].Address = MAX8907B_ADC_SLAVE_ADDR;
        TransactionInfo[0].Buf = &ReadBuffer;
        TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                                   NVODM_I2C_USE_REPEATED_START;
        TransactionInfo[0].NumBytes = 1;

        TransactionInfo[1].Address = (MAX8907B_ADC_SLAVE_ADDR | 0x1);
        TransactionInfo[1].Buf = &ReadBuffer;
        TransactionInfo[1].Flags = 0;
        TransactionInfo[1].NumBytes = 1;

        // Read data from PMU at the specified offset
        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo[0], 2,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
        {
            *Data = ReadBuffer;
            return NV_TRUE;
        }
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead8 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bI2cWrite32(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU32 Data)
{
    NvU32 i;
    NvU8 WriteBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo;

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        WriteBuffer[0] = (NvU8)(Addr & 0xFF);
        WriteBuffer[1] = (NvU8)((Data >> 24) & 0xFF);
        WriteBuffer[2] = (NvU8)((Data >> 16) & 0xFF);
        WriteBuffer[3] = (NvU8)((Data >>  8) & 0xFF);
        WriteBuffer[4] = (NvU8)(Data & 0xFF);

        TransactionInfo.Address = hPmu->DeviceAddr;
        TransactionInfo.Buf = &WriteBuffer[0];
        TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
        TransactionInfo.NumBytes = 5;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
            return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite32 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cWrite32 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bI2cRead32(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU32 *Data)
{
    NvU32 i;
    NvU8 ReadBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo[2];

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        ReadBuffer[0] = Addr & 0xFF;

        TransactionInfo[0].Address = hPmu->DeviceAddr;
        TransactionInfo[0].Buf = &ReadBuffer[0];
        TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                                   NVODM_I2C_USE_REPEATED_START;
        TransactionInfo[0].NumBytes = 1;

        TransactionInfo[1].Address = (hPmu->DeviceAddr | 0x1);
        TransactionInfo[1].Buf = &ReadBuffer[0];
        TransactionInfo[1].Flags = 0;
        TransactionInfo[1].NumBytes = 4;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo[0], 2,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
        {
            *Data = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) |
                    (ReadBuffer[2] << 8) | ReadBuffer[3];

            return NV_TRUE;
        }
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead32 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("NvOdmPmuI2cRead32 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bRtcI2cWriteTime(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU32 Data)
{
    NvU32 i;
    NvU8 WriteBuffer[5];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo;

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        WriteBuffer[0] = (NvU8)(Addr & 0xFF);
        WriteBuffer[1] = (NvU8)((Data >> 24) & 0xFF);
        WriteBuffer[2] = (NvU8)((Data >> 16) & 0xFF);
        WriteBuffer[3] = (NvU8)((Data >>  8) & 0xFF);
        WriteBuffer[4] = (NvU8)(Data & 0xFF);

        TransactionInfo.Address = MAX8907B_RTC_SLAVE_ADDR;
        TransactionInfo.Buf = &WriteBuffer[0];
        TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
        TransactionInfo.NumBytes = 5;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo, 1,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
            return NV_TRUE;
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("Max8907bRtcI2cWrite32 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("Max8907bRtcI2cWrite32 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

NvBool Max8907bRtcI2cReadTime(
   NvOdmPmuDeviceHandle hDevice,
   NvU8 Addr,
   NvU32 *Data)
{
    NvU32 i;
    NvU8 ReadBuffer[4];
    NvOdmI2cStatus  status = NvOdmI2cStatus_Success;
    Max8907bPrivData *hPmu = (Max8907bPrivData*)hDevice->pPrivate;
    NvOdmI2cTransactionInfo TransactionInfo[4];

    for (i = 0; i < MAX8907B_I2C_RETRY_CNT; i++)
    {
        ReadBuffer[0] = Addr++ & 0xFF;
        ReadBuffer[1] = Addr++ & 0xFF;
        ReadBuffer[2] = Addr++ & 0xFF;
        ReadBuffer[3] = 0;

        TransactionInfo[0].Address = MAX8907B_RTC_SLAVE_ADDR;
        TransactionInfo[0].Buf = &ReadBuffer[0];
        TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE |
                                   NVODM_I2C_USE_REPEATED_START;
        TransactionInfo[0].NumBytes = 1;

        // Seconds
        TransactionInfo[1].Address = (MAX8907B_RTC_SLAVE_ADDR | 0x1);
        TransactionInfo[1].Buf = &ReadBuffer[0];
        TransactionInfo[1].Flags = 0;
        TransactionInfo[1].NumBytes = 1;

        // Minutes
        TransactionInfo[2].Address = (MAX8907B_RTC_SLAVE_ADDR | 0x1);
        TransactionInfo[2].Buf = &ReadBuffer[1];
        TransactionInfo[2].Flags = 0;
        TransactionInfo[2].NumBytes = 1;

        // Hours
        TransactionInfo[3].Address = (MAX8907B_RTC_SLAVE_ADDR | 0x1);
        TransactionInfo[3].Buf = &ReadBuffer[2];
        TransactionInfo[3].Flags = 0;
        TransactionInfo[3].NumBytes = 1;

        status = NvOdmI2cTransaction(hPmu->hOdmI2C, &TransactionInfo[0], 4,
            MAX8907B_I2C_SPEED_KHZ, NV_WAIT_INFINITE);

        if (status == NvOdmI2cStatus_Success)
        {
            *Data = (ReadBuffer[0] << 24) | (ReadBuffer[1] << 16) |
                    (ReadBuffer[2] << 8) | ReadBuffer[3];

            return NV_TRUE;
        }
    }

    // Transaction Error
    switch (status)
    {
        case NvOdmI2cStatus_Timeout:
            NVODMPMU_PRINTF(("Max8907bRtcI2cRead32 Failed: Timeout\n"));
            break;
        case NvOdmI2cStatus_SlaveNotFound:
        default:
            NVODMPMU_PRINTF(("Max8907bRtcI2cRead32 Failed: SlaveNotFound\n"));
            break;
    }
    return NV_FALSE;
}

