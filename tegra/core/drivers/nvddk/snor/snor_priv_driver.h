/*
 * Copyright (c) 2007-2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * @brief <b>nVIDIA Driver Development Kit:
 *           Private functions for SNOR device driver
 *
 * @b Description:  Defines the private interfacing functions for the SNOR
 * device driver.
 *
 */

#ifndef INCLUDED_SNOR_PRIV_DRIVER_H
#define INCLUDED_SNOR_PRIV_DRIVER_H

#include "nvcommon.h"
#include "nvrm_power.h"

#if defined(__cplusplus)
extern "C" {
#endif


/* SNOR APB registers */

#define SNOR_READ32(pSnorHwRegsVirtBaseAdd, reg) \
        NV_READ32((pSnorHwRegsVirtBaseAdd) + ((SNOR_##reg##_0)/4))

#define SNOR_WRITE32(pSnorHwRegsVirtBaseAdd, reg, val) \
    do \
    {  \
        NV_WRITE32((((pSnorHwRegsVirtBaseAdd) + ((SNOR_##reg##_0)/4))), (val)); \
    } while (0)

/* Flash Read Write */

#define SNOR_FLASH_WRITE16(pSnorBaseAdd, Address, Val) \
        do \
        { \
                NV_WRITE16(((NvU32)(pSnorBaseAdd) + (Address <<1)),Val) ; \
        } while(0)

#define SNOR_FLASH_READ16(pSnorBaseAdd, Address) NV_READ16((NvU32)(pSnorBaseAdd) + (Address << 1))

#define SNOR_FLASH_WRITE32(pSnorBaseAdd, Address, Val) \
        do\
        { \
            NV_WRITE32 (((NvU32)(pSnorBaseAdd) + (Address << 2)), Val); \
        } while (0)

#define FLASH_READ_ADDRESS32(pSnorBaseAdd,Address) NV_READ32((NvU32)(pSnorBaseAdd) + (Address << 2))


typedef enum
{
    SnorDeviceType_x16_NonMuxed         = 0x0,
    SnorDeviceType_x32_NonMuxed         = 0x1,
    SnorDeviceType_x16_Muxed            = 0x2,
    SnorDeviceType_x32_Muxed            = 0x3,
    SnorDeviceType_Unknown              = 0x10,
    SnorDeviceType_Force32              = 0x7FFFFFFF
} SnorDeviceType;

typedef enum
{
    SnorCmdID_None                      = 0x0000,
    SnorCmdID_IntelE                    = 0x0001,
    SnorCmdID_AMD                       = 0x0002,
    SnorCmdID_Intel                     = 0x0003,
    SnorCmdID_AMDE                      = 0x0004,
    SnorCmdID_IntelP                    = 0x0200,
    SnorCmdID_IntelD                    = 0x0210,
    SnorCmdID_Reserved                  = 0xffff
} SnorCmdID;

typedef enum
{
    SnorReadMode_Async                  = 0x0,
    SnorReadMode_Page                   = 0x1,
    SnorReadMode_Burst                  = 0x2,
    SnorReadMode_Resv                   = 0x3,
} SnorReadMode;

/**
 * SNOR command status
 */
typedef enum
{
    SnorCommandStatus_Ready             = 0x0,
    SnorCommandStatus_Success           = 0x0,
    SnorCommandStatus_Busy              = 0x1,
    SnorCommandStatus_Error             = 0x2,
    SnorCommandStatus_LockError         = 0x3,
    SnorCommandStatus_EraseError        = 0x4,
    SnorCommandStatus_EccError          = 0x5,
    SnorCommandStatus_Force32           = 0x7FFFFFFF
} SnorCommandStatus;

/**
 * SNOR device information.
 */
typedef struct
{
    NvU32 CmdId;
    SnorDeviceType DeviceType;
    SnorReadMode ReadMode;
    NvRmFreqKHz ClockFreq;
    NvU32 Timing0;
    NvU32 Timing1;
    NvU32 BusWidth;
    NvU32 EraseSize;
    NvU32 TotalBlocks;
    NvU32 PagesPerBlock;
    NvU32 PageSize;
} SnorDeviceInfo;

typedef struct
{
    NvRmPhysAddr NorBaseAddress;
    NvU16 *pNorBaseAdd;
    NvU32 NorAddressSize;
} SnorDeviceIntRegister;


/**
 * SNOR interrupt reason.
 */
typedef enum
{
  SnorInterruptReason_None = 0x0,
  SnorInterruptReason_Force32 = 0x7FFFFFFF
} SnorInterruptReason;


// The structure of the function pointers to provide the interface to
// SNOR device driver
typedef struct
{
    /**
     * Initialize the device interface.
     */
    void (*DevInitializeDeviceInterface)
        (SnorDeviceIntRegister *pDevIntRegs,
         SnorDeviceInfo *pDevInfo,
         NvRmFreqKHz ClockFreq);


    /**
     * Find whether the device is available on this interface or not.
     */
    NvU32 (*DevResetDevice)(SnorDeviceIntRegister *pDevIntRegs, NvU32 DeviceChipSelect);


    NvBool (*DevIsDeviceReady)(SnorDeviceIntRegister *pDevIntRegs);

    /**
     * Get the device information.
     */
    NvBool
    (*DevGetDeviceInfo)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 DeviceChipSelect);

    NvBool
    (*DevConfigureHostInterface)(
        SnorDeviceIntRegister *pDevIntRegs,
        NvBool IsAsynch);

    void
    (*DevAsynchReadFromBufferRam)(
        SnorDeviceIntRegister *pDevIntRegs,
        NvU8 *pReadMainBuffer,
        NvU8 *pReadSpareBuffer,
        NvU32 MainAreaSizeBytes,
        NvU32 SpareAreaSize);

    void
    (*DevAsynchWriteIntoBufferRam)(
        SnorDeviceIntRegister *pDevIntRegs,
        NvU8 *pWriteMainBuffer,
        NvU8 *pWriteSpareBuffer,
        NvU32 MainAreaSizeBytes,
        NvU32 SpareAreaSize);

    NvU32
    (*DevEraseBlock)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 BlockNumber);

    SnorCommandStatus (*DevGetEraseStatus)(SnorDeviceIntRegister *pDevIntRegs);

    NvBool (*DevEraseSuspend)(SnorDeviceIntRegister *pDevIntRegs);

    NvBool (*DevEraseResume)(SnorDeviceIntRegister *pDevIntRegs);

    SnorCommandStatus
    (*DevSetBlockLockStatus)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 BlockNumber,
        NvBool IsLock);

    SnorCommandStatus
    (*DevUnlockAllBlockOfChip)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo);

    NvBool
    (*DevIsBlockLocked)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 BlockNumber);

    SnorCommandStatus
    (*DevGetLoadStatus)(
        SnorDeviceIntRegister *pDevIntRegs);

    SnorCommandStatus
    (*DevLoadBlockPage)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 BlockNumber,
        NvU32 PageNumber,
        NvBool IsWaitForCompletion);

    void
    (*DevSelectDataRamFromBlock)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 BlockNumber);

    SnorCommandStatus
    (*DevGetProgramStatus)(
        SnorDeviceIntRegister *pDevIntRegs);

    SnorCommandStatus
    (*DevFormatDevice)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion);

    SnorCommandStatus
    (*DevEraseSector)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 StartOffset,
        NvU32 Length,
        NvBool IsWaitForCompletion);

    SnorCommandStatus
    (*DevProgram)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 ByteOffset,
        NvU32 SizeInBytes,
        const NvU8* Buffer,
        NvBool IsWaitForCompletion);

    SnorCommandStatus
    (*DevRead)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 ByteOffset,
        NvU32 SizeInBytes,
        NvU8* Buffer,
        NvBool IsWaitForCompletion);

    SnorInterruptReason
    (*DevGetInterruptReason)(
        SnorDeviceIntRegister *pDevIntRegs);

    SnorCommandStatus
    (*DevReadMode)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvBool IsWaitForCompletion);

    SnorCommandStatus
    (*DevProtectSectors)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo,
        NvU32 Offset,
        NvU32 Size,
        NvBool IsLastPartition);

    SnorCommandStatus
    (*DevUnprotectAllSectors)(
        SnorDeviceIntRegister *pDevIntRegs,
        SnorDeviceInfo *pDevInfo);

} SnorDevInterface, *SnorDevInterfaceHandle;


NvBool
IsDeviceAvailable(
    SnorDeviceIntRegister *pDevIntRegs,
    NvU32 DeviceChipSelect);

SnorDeviceType GetDeviceType(SnorDeviceIntRegister *pDevIntRegs);

/**
 * Initialize the SNOR device interface.
 */

void
NvRmPrivSnorDeviceInterface(
    SnorDevInterface *pDevInterface,
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo);

NvBool
IsSnorDeviceAvailable(
    SnorDeviceIntRegister *pDevIntRegs,
    NvU32 DeviceChipSelect);

SnorDeviceType GetSnorDeviceType(SnorDeviceIntRegister *pDevIntRegs);

/** @}*/

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_SNOR_PRIV_DRIVER_H


