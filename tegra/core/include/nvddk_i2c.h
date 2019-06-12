/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_I2C_H
#define INCLUDED_NVDDK_I2C_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/* Default operating frequency of i2c controllers */
#define NVDDK_I2C_DEFAULT_FREQUENCY 100 // KHz

typedef enum
{
    NvDdkI2c1 = 1,
    NvDdkI2c2,
    NvDdkI2c3,
    NvDdkI2c4,
    NvDdkI2c5,
    NvDdkPwrI2c = NvDdkI2c5,
    NvDdkI2c6,

    /* Should be last enum. */
    NvDdkMaxI2cInstances
}NvDdkI2cInstance;

/* Defines the flags indicating details about slave */
typedef enum
{
    NVDDK_I2C_NOACK = 1,
    NVDDK_I2C_10BIT_ADDRESS = 2,
    NVDDK_I2C_SEND_START_BYTE = 4
}NvDdkI2cSlaveFlags;

/* Defines the slave handle */
typedef struct NvDdkI2cSlaveInfoRec* NvDdkI2cSlaveHandle;

/* Creates a slave info instance, intialises the controller and
 * returns the handle to slave.
 *
 * @param I2cInstance I2c controller instance to which slave is connected
 * @param SlaveAddress Address of the slave
 * @param Flags set of bits indicating details about slave
 * @param MaxClockFreqency Max clock frequency of slave device in KHz
 *                         Default 100 KHz
 * @param MaxWaitTime Maximum response time of slave for one i2c transaction
 *                    Default NV_WAIT_INFINITE
 * @param pSlaveHandle which will get updated to point to created slave handle
 *
 * @return NvSuccess of successful else NvError
 */
NvError
NvDdkI2cDeviceOpen(
    NvDdkI2cInstance I2cInstance,
    NvU16 SlaveAddress,
    NvU16 Flags,
    NvU16 MaxClockFreqency,
    NvU16 MaxWaitTime,
    NvDdkI2cSlaveHandle *pSlaveHandle);

/* Deallocates the handle */
void NvDdkI2cDeviceClose(NvDdkI2cSlaveHandle hSlave);

/* Reads bytes from specified offset from slave device.
 *
 * @param hSlave Handle to the slave device
 * @param Offset From which bytes to be read
 * @param pBuffer To which read bytes to be written
 * @param NumBytes Number of bytes to be read
 *
 * @return NvSuccess if read is successful
 */
NvError
NvDdkI2cDeviceRead(
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer,
    NvU32 NumBytes);

/* Writes bytes to specified offsets of slave device.
 *
 * @param hSlave Handle to the slave device
 * @param Offset From which bytes to be written
 * @param pBuffer To which contains data to be written
 * @param NumBytes Number of bytes to be written
 *
 * @return NvSuccess if write is successful
 */
NvError
NvDdkI2cDeviceWrite(
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer,
    NvU32 NumBytes);

/* Run bus clear logic for given i2c controller
 *
 * @param Instance I2c controller instance
 *
 * @return NvSuccess if bus clear is successful
 */
NvError NvDdkI2cClearBus(NvDdkI2cInstance Instance);

/* Tries to read one byte from slave
 *
 * @param I2cInstance Controller Instance
 * @param SlaveAddress Slave Address
 *
 * @return NvSuccess if successful else NvError
 */
NvError NvDdkI2cDummyRead(NvDdkI2cInstance I2cInstance, NvU16 SlaveAddress);

#if defined(__cplusplus)
}
#endif

#endif

