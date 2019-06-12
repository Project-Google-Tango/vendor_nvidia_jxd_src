/*
 * Copyright (c) 2013 NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVDDK_DDK_I2C_HW_H
#define NVDDK_DDK_I2C_HW_H

#include "nvcommon.h"
#include "nvassert.h"
#include "nverror.h"
#include "nvrm_drf.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define NVDDK_I2C_CHECK_ERROR_CLEANUP(expr, err_str)            \
    do                                                          \
    {                                                           \
        e = (expr);                                             \
        if (e != NvSuccess)                                     \
        {                                                       \
            DDK_I2C_ERROR_PRINT("%s: error code %d %s",         \
                                __func__, e, err_str);          \
            goto fail;                                          \
        }                                                       \
    } while (0)

#define MAX_I2C_TRANSFER_SIZE        0x08
#define MAX_I2C_2SLAVE_TRANSFER_SIZE 0x04
#define CNFG_LOAD_TIMEOUT_US         20
#define BUS_CLEAR_TIMEOUT_US         200
#define BUS_CLEAR_TIMEOUT_STEP       20

#undef NV_READ32
#define NV_READ32(a)     *((volatile NvU32 *)(a))

#undef NV_WRITE32
#define NV_WRITE32(a,d)  *((volatile NvU32 *)(a)) = (d)

#define NV_DDK_I2C_REG_WRITE(i2coffset, reg, value)                            \
    NV_WRITE32((NV_ADDRESS_MAP_I2C_BASE + i2coffset + I2C_##reg##_0), value)

#define NV_DDK_I2C_REG_READ(i2coffset, reg)                                    \
    NV_READ32((NV_ADDRESS_MAP_I2C_BASE + i2coffset + I2C_##reg##_0))

#define NV_CLK_RST_REG_READ(reg, value)                                        \
    value = NV_READ32(CLK_RST_BASE + CLK_RST_CONTROLLER_##reg##_0)

#define NV_CLK_RST_REG_WRITE(reg, value)                                        \
    NV_WRITE32((CLK_RST_BASE + CLK_RST_CONTROLLER_##reg##_0), value)

/* Defines the capabilities of i2c controller */
typedef struct NvDdkI2cCapabilitiesRec
{
    /* Controller supports bus clear logic */
    NvBool SupportsBusClear;
    /* Needs to set the bit fields to load controller registers */
    NvBool NeedLoadBitFields;
    /* Need to enable DVFS clock for communication */
    NvBool NeedsClDvfsEnabled;
}NvDdkI2cCapabilities;

/* Defines the information of i2c controller. */
typedef struct NvDdkI2cRec
{
    /* Controller instance number */
    NvU8 Instance;
    /* Is controller initialized */
    NvBool IsInitialized;
    /* Offset of controller */
    NvU32 Offset;
    /* Capabilities of controller */
    NvDdkI2cCapabilities Capabilities;
}NvDdkI2cInfo;

/* Defines the i2c handle */
typedef NvDdkI2cInfo *NvDdkI2cHandle;

/* Defines the structure holding the slave information */
typedef struct NvDdkI2cSlaveInfoRec
{
    /* Slave Address */
    NvU16 Address;
    /* Flags indicate slave details, like whether slave generates ack or not */
    NvU16 Flags;
    /* Maximum clock frequency supported by slave */
    NvU16 MaxClockFreqency;
    /* Maximum wait time for single transaction */
    NvU16 MaxWaitTime;
    /* Handle to I2c Controller */
    NvDdkI2cHandle hI2c;
}NvDdkI2cSlaveInfo;

/* Returns the time in micro seconds. */
NvU32 NvDdkI2cGetTimeUS(void);

/* Returns the time in milli seconds */
NvU32 NvDdkI2cGetTimeMS(void);

/* Busy loop till specified milliseconds */
void NvDdkI2cStallMs(NvU64 MilliSec);

/* Busy loop till specified microseconds */
void NvDdkI2cStallUs(NvU64 USec);

/* Returns the chip id */
NvU32 NvDdkI2cGetChipId(void);

/* Intializes the i2c controller and returns the handle
 * which can be used later for read/write.
 *
 * @param Instance Controller instance which is to be initialized
 *
 * @return Pointer to i2c controller handle if successful else null
 */
NvDdkI2cHandle NvDdkI2cOpen(NvU32 Instance);

/* Sets the devisor for given instance
 *
 * @param Instance Controller instance
 * @param Divisor value which is to be set as devisor
 */
void NvDdkI2cSetDivisor(NvU32 Instance, NvU32 Divisor);

/* Returns the devisor for given devidend and devisor */
NvU32 NvDdkIntDiv(NvU32 Dividend, NvU32 Divisor);

/* Populates the cpabilities of controller
 *
 * @param hI2c Handle of i2c controller
 */
void NvDdkI2cGetCapabilities(NvDdkI2cHandle hI2c);

/* Loads the shadow register configuration to actual hardware registers
 *
 * @param hI2c Handle to I2C controller
 *
 * @returns NvError_Success if successful
 */
NvError NvDdkI2cConfigLoadBits(const NvDdkI2cHandle hI2c);

/* Enables DVFS clock
 *
 * @param hI2c Handle to I2C controller
 */
void NvDdkI2cEnableClDvfs(void);

/* Sets the divisor for specified freqency and controller
 *
 * @param hI2c Handle to I2c controller
 * @param Frequency
 */
void NvDdkI2cSetFrequency(NvDdkI2cHandle hI2c, NvU32 Frequency);

/* Configures the CNFG register for initiating i2c read/write operation
 *
 * @param hI2c Handle to I2c controller
 * @param hSlave Handle to slave device
 * @param Offset Slave internal offset
 * @param DataLen Length of data to be transferred
 * @param IsWrite True if write operation
 *
 * @returns NvSuccess if successful
 */
NvError
NvDdkI2cInitTransaction(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU32 DataLen,
    NvBool IsWrite);

/* Resets the controller
 *
 * @param Instance Controller instance
 */
void NvDdkI2cResetController(NvU32 Instance);

/* Starts the transaction by setting GO bit of CNFG and waits for
 * read/write to complete
 *
 * @param hI2c I2c controller handle
 *
 * @returns NvSuccess if successful else NvError
 */
NvError NvDdkI2cStartTransaction(const NvDdkI2cHandle hI2c, NvU16 TimeOut);

/* Reads max 8 Bytes
 *
 * @param hI2c Handle to I2c controller
 * @param hSlave Handle to slave device
 * @param Offset Slave internal oofset
 * @param pBuffer in which data is to be read
 * @param NumBytes number of bytes to be read
 *
 * @returns NvSuccess if successful
 */
NvError
NvDdkI2cRead(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 *pBuffer,
    NvU32 NumBytes);

/* Writes max 8 Bytes including slave internal offset
 *
 * @param hI2c Handle to I2c controller
 * @param hSlave Handle to slave device
 * @param Offset Slave internal oofset
 * @param pBuffer containing data to write
 * @param NumBytes number of bytes to write
 *
 * @returns NvSuccess if successful
 */
NvError
NvDdkI2cWrite(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 *pBuffer,
    NvU32 NumBytes);

/* Reads only 1 byte with repeated start transaction
 *
 * @param hI2c Handle to I2c controller
 * @param hSlave Handle to slave device
 * @param Offset Slave internal offset
 * @param pBuffer containing data to read
 *
 * @returns NvSuccess if successful
 */
NvError
NvDdkI2cRepeatedStartRead(
    const NvDdkI2cHandle hI2c,
    const NvDdkI2cSlaveHandle hSlave,
    NvU8 Offset,
    NvU8 *pBuffer);

#if defined(__cplusplus)
}
#endif

#endif // NVDDK_DDK_I2C_HW_H

