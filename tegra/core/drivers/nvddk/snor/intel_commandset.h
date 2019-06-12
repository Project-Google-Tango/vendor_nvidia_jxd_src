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
 *           Intel functions for SNOR device driver
 *
 * @b Description:  Defines the Intel functions for SNOR device driver
 *
 */

#ifndef INCLUDED_INTEL_COMMANDSET_H
#define INCLUDED_INTEL_COMMANDSET_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define INTEL_BLOCK_ADDRESS_MASK  0xfffe0000
#define INTEL_SECTOR_ADDRESS_MASK  0xfffffe00
#define INTEL_BUFFER_SIZE 512
#define INTEL_SECTOR_SIZE 1024
#define INTEL_LOCK_ATTEMPTS 25
#define INTEL_ASYNC_TIMING0 0xA0A05585
#define INTEL_ASYNC_TIMING1 0x200A0406

/* Erase max timeout > program max timeout. 4s */
#define INTEL_MAX_TIMEOUT 4000000
/* Min typical time is single word program. 64us */
#define INTEL_MIN_TIME 64


/* Read Configuration Register Masks */
#define SYNC_MODE_MASK          (~(1 << 15))
#define LATENCY_MASK            (~(0xf << 11))
#define BURST_LENGTH_MASK       (~(7))

/* Read Configuration Register Values */
#define SYNC_BURST_MODE         (0)
#define ASYNC_MODE              (1 << 15)

#define LATENCY_COUNT_87_0      (8 << 11)
#define LATENCY_COUNT_108_7     (10 << 11)
#define LATENCY_COUNT_133_3     (13 << 11)

#define BURST_LENGTH_8WD        (2)
#define BURST_LENGTH_16WD       (3)
#define BURST_LENGTH_CNTBURST   (7)


/* Status Register Checks */
#define READY_BIT_CHECK(x)  (((x)>>7) & 0x1)
#define VPP_RANGE_CHECK(x)  (((x)>>3) & 0x1)
#define PRGRM_ERR_CHECK(x)  (((x)>>4) & 0x1)
#define OBJ_OVRWRT_CHECK(x) (((x)>>8) & 0x1 && !(((x)>>9 & 0x1)))
#define BLCK_ERASE_CHECK(x) (((x)>>5) & 0x1)
#define CMND_SEQ_CHECK(x)   ((((x)>>4) & 0x1) && (((x)>>5) & 0x1))
#define BLCK_LOCK_CHECK(x)  (((x)>>1) & 0x1)


/* Lock Checks */
#define LOCK_CHECK(x)       ((x) & 0x1)
#define UNLOCK_CHECK(x)     (!LOCK_CHECK(x))


typedef enum
{
    // Register Operations
    SnorCmd_Intel_Program_Config_Reg_Setup          = 0x60,
    SnorCmd_Intel_Program_Read_Config_Reg           = 0x03, // Needs Program Config Reg Setup
    SnorCmd_Intel_Program_Ext_Config_Reg            = 0x04, // Needs Program Config Reg Setup
    SnorCmd_Intel_Program_OTP_Area                  = 0xc0,
    SnorCmd_Intel_Clear_Status_Register             = 0x50,
    // Read Mode Operations
    SnorCmd_Intel_Read_Array                        = 0xff,
    SnorCmd_Intel_Read_Status_Register              = 0x70,
    SnorCmd_Intel_Read_Id                           = 0x90,
    SnorCmd_Intel_Read_CFI                          = 0x98,
    // Array Programming Operations
    SnorCmd_Intel_Single_Word_Program               = 0x41,
    SnorCmd_Intel_Buffered_Program_Setup            = 0xe9,
    SnorCmd_Intel_Buffered_Program                  = 0xd0, // Needs Buffered Program Setup
    SnorCmd_Intel_Buffered_Factory_Program_Setup    = 0x80,
    SnorCmd_Intel_Buffered_Factory_Program          = 0xd0, // Needs Buffered Factory Setup
    // Block Erase Operations
    SnorCmd_Intel_Block_Erase_Setup                 = 0x20,
    SnorCmd_Intel_Block_Erase                       = 0xd0, // Needs Block Erase Setup
    // Security Operations
    SnorCmd_Intel_Security_Setup                    = 0x60,
    SnorCmd_Intel_Lock_Block                        = 0x01, // Needs Security Setup
    SnorCmd_Intel_Unlock_Block                      = 0xd0, // Needs Security Setup
    SnorCmd_Intel_Lock_Down_Block                   = 0x2f, // Needs Security Setup
    // Other Operations
    SnorCmd_Intel_Suspend                           = 0xb0,
    SnorCmd_Intel_Resume                            = 0xd0,
    SnorCmd_Intel_Blank_Check_Setup                 = 0xbc,
    SnorCmd_Intel_Blank_Check                       = 0xd0 // Needs Blank Check Setup
} SnorCmd_Intel; // Intel Command Set (x16)

SnorCommandStatus IntelFormatDevice (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvBool IsWaitForCompletion);

SnorCommandStatus IntelEraseSector (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 StartOffset,
    NvU32 Length,
    NvBool IsWaitForCompletion);

SnorCommandStatus IntelProtectSectors(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 Offset,
    NvU32 Size,
    NvBool IsLastPartition);

SnorCommandStatus IntelUnprotectAllSectors(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo);

SnorCommandStatus IntelProgram (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 ByteOffset,
    NvU32 SizeInBytes,
    const NvU8* Buffer,
    NvBool IsWaitForCompletion);

SnorCommandStatus IntelReadMode(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvBool IsWaitForCompletion);



#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_INTEL_COMMANDSET_H


