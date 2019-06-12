/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvddk_i2c.h"
#include "nvddk_i2c_debug.h"
#include "nvddk_i2c_hw.h"
#include "nvutil.h"

#define MAX_SLAVES 10

/* Defines the pool of slaves. */
static NvDdkI2cSlaveInfo s_SlavePool[MAX_SLAVES];

/* Variable holding the information about free and busy slaves
 * from slave pool.
 */
static NvU16 s_BitsForBusySlaves;

/* Returns the free slave from slave pool. */
static NvDdkI2cSlaveHandle NvDdkI2cGetFreeSlave(void);

/* Marks slave free by resetting the bit for that slave */
static void NvDdkI2cFreeBusySlave(NvDdkI2cSlaveHandle hSlave);

NvDdkI2cSlaveHandle NvDdkI2cGetFreeSlave(void)
{
    NvU32 i;

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);

    for (i = 0; i < MAX_SLAVES; i++)
    {
        if (!(s_BitsForBusySlaves & (1 << i)))
        {
            s_BitsForBusySlaves |= (1 << i);
            return &s_SlavePool[i];
        }
    }

    return NULL;
}

void NvDdkI2cFreeBusySlave(NvDdkI2cSlaveHandle hSlave)
{
    NvU32 i;

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);
    NV_ASSERT(hSlave != NULL);

    for (i = 0; i < MAX_SLAVES; i++)
    {
        if (hSlave == (&s_SlavePool[i]))
        {
            s_BitsForBusySlaves &= ~((1 << i));
            break;
        }
    }

    /* Slave handle should be from slave pool. */
    NV_ASSERT(i < MAX_SLAVES);
}

NvError
NvDdkI2cDeviceOpen(
    NvDdkI2cInstance I2cInstance,
    NvU16 SlaveAddress,
    NvU16 Flags,
    NvU16 MaxClockFreqency,
    NvU16 MaxWaitTime,
    NvDdkI2cSlaveHandle *pSlaveHandle)
{
    NvError e = NvSuccess;
    NvDdkI2cSlaveHandle hSlave = NULL;
    NvDdkI2cHandle hI2c = NULL;

    DDK_I2C_INFO_PRINT("%s: entry NvDdkI2c%d\n", __func__, I2cInstance);
    NV_ASSERT(pSlaveHandle != NULL);
    NV_ASSERT(I2cInstance != 0);

    hSlave = NvDdkI2cGetFreeSlave();
    if (hSlave == NULL)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_OverFlow,
            "Error while getting free slave"
        );
    }

    hSlave->Address = SlaveAddress;
    hSlave->Flags = Flags;
    hSlave->hI2c = hI2c;
    hSlave->MaxWaitTime = MaxWaitTime > 0 ? MaxWaitTime : NV_WAIT_INFINITE;
    hSlave->MaxClockFreqency = MaxClockFreqency > 0 ?
                               MaxClockFreqency : NVDDK_I2C_DEFAULT_FREQUENCY;

    DDK_I2C_INFO_PRINT("Slave Address: %04x I2C Instance %01x Flags %02x "
                       "Max clock frequency: %04x Max wait time: %04x\n",
                       SlaveAddress, I2cInstance, Flags, MaxClockFreqency,
                       MaxWaitTime);

    hI2c = NvDdkI2cOpen(I2cInstance);
    if (hI2c == NULL)
    {
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvError_NotInitialized,
            "Error while opening controller"
        );
    }

    hSlave->hI2c = hI2c;
    *pSlaveHandle = hSlave;

    return e;

fail:
    if (hSlave != NULL)
        NvDdkI2cDeviceClose(hSlave);

    return e;
}

void NvDdkI2cDeviceClose(NvDdkI2cSlaveHandle hSlave)
{
    NV_ASSERT(hSlave != NULL);
    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);
    NvDdkI2cFreeBusySlave(hSlave);
}

NvError
NvDdkI2cDeviceRead(
    NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer,
    NvU32 NumBytes)
{
    NvError e = NvSuccess;
    NvDdkI2cHandle hI2c = NULL;
    NvU32 BytesToRead;

    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(pBuffer != NULL);

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);
    DDK_I2C_INFO_PRINT("Offset: %04x Number of bytes %d\n", Offset, NumBytes);

    hI2c = hSlave->hI2c;
    NV_ASSERT(hI2c != NULL);

    NvDdkI2cSetFrequency(hI2c, hSlave->MaxClockFreqency);

    if (NumBytes == 1)
    {
        NV_CHECK_ERROR(
            NvDdkI2cRepeatedStartRead(hI2c, hSlave, Offset, pBuffer)
        );
        goto fail;
    }

    while (NumBytes)
    {
        /* Write offset */
        NVDDK_I2C_CHECK_ERROR_CLEANUP(
            NvDdkI2cWrite(hI2c, hSlave, &Offset, sizeof(Offset)),
            "Error while sending the offset to slave"
        );

        BytesToRead = NumBytes < MAX_I2C_TRANSFER_SIZE ?
                      NumBytes : MAX_I2C_TRANSFER_SIZE;

        NV_CHECK_ERROR(
            NvDdkI2cRead(hI2c, hSlave, pBuffer, BytesToRead)
        );

        NumBytes -= BytesToRead;
        Offset += BytesToRead;
        pBuffer += BytesToRead;
    }

fail:
    return e;
}

NvError
NvDdkI2cDeviceWrite(
    NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer,
    NvU32 NumBytes)
{
    NvError e = NvSuccess;
    NvU8 Data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    NvDdkI2cHandle hI2c = NULL;
    NvU32 BytesToWrite;
    NvU32 Bytes;

    NV_ASSERT(hSlave != NULL);
    NV_ASSERT(pBuffer != NULL);

    DDK_I2C_INFO_PRINT("%s: entry\n", __func__);
    DDK_I2C_INFO_PRINT("Offset: %04x Number of bytes %d\n", Offset, NumBytes);

    hI2c = hSlave->hI2c;
    NV_ASSERT(hI2c != NULL);

    NvDdkI2cSetFrequency(hI2c, hSlave->MaxClockFreqency);

    while (NumBytes)
    {
        BytesToWrite = NumBytes < (MAX_I2C_TRANSFER_SIZE - 1) ?
                       NumBytes : (MAX_I2C_TRANSFER_SIZE - 1);

        /* First write offset. */
        Bytes   = 0;
        Data[0] = Offset;

        while(Bytes < BytesToWrite)
        {
            Data[Bytes + 1] = pBuffer[Bytes];
            Bytes++;
        }

        NV_CHECK_ERROR(
            NvDdkI2cWrite(hI2c, hSlave, &Data[0], BytesToWrite + 1)
        );

        NumBytes -= BytesToWrite;
        Offset += BytesToWrite;
        pBuffer += BytesToWrite;
    }

    return e;
}

NvError NvDdkI2cDummyRead(NvDdkI2cInstance I2cInstance, NvU16 SlaveAddress)
{
    NvError e = NvSuccess;
    NvDdkI2cSlaveHandle hSlave = NULL;
    NvDdkI2cHandle hI2c = NULL;
    NvU8 Buf;

    NvDdkI2cDeviceOpen(I2cInstance, SlaveAddress, 0, 0, 5, &hSlave);

    if (hSlave == NULL)
        return NvError_NotInitialized;

    hI2c = hSlave->hI2c;
    NV_ASSERT(hI2c != NULL);

    e = NvDdkI2cRead(hI2c, hSlave, &Buf, 1);

    if (hSlave != NULL)
        NvDdkI2cDeviceClose(hSlave);

    return e;
}

