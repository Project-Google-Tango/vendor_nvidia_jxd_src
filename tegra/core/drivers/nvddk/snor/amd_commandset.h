/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
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
 *           AMD functions for SNOR device driver
 *
 * @b Description:  Defines the AMD functions for SNOR device driver
 *
 */

#ifndef INCLUDED_AMD_COMMANDSET_H
#define INCLUDED_AMD_COMMANDSET_H

#include "nvcommon.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define AMD_SECTOR_SIZE (128 * 1024)
#define AMD_BUFFER_SIZE 16
#define AMD_SECTOR_ADDRESS_MASK  0xfffffff0
#define AMD_LOCK_WAIT_TIME 500
#define AMD_UNLOCK_WAIT_TIME 500000

typedef enum
{
    // Command Sequence Setup - These precede any Sequence Commands
    SnorCmd_AMD_Command_Sequence_0                = 0xaa, // Addr 0x555
    SnorCmd_AMD_Command_Sequence_1                = 0x55, // Addr 0x2aa
    SnorCmd_AMD_Exit_0                            = 0x90, // Addr XX
    SnorCmd_AMD_Exit_1                            = 0x00, // Addr XX
    // Modes
    SnorCmd_AMD_Read_Array                        = 0xf0, // Addr XX, AKA Reset
    SnorCmd_AMD_Auto_Select                       = 0x90, // Addr 0x555, Sequence
    SnorCmd_AMD_Read_CFI                          = 0x98, // Addr 0x55
    // Programming
    SnorCmd_AMD_Program                           = 0xa0, // Addr 0x555, Sequence
    SnorCmd_AMD_Write_to_Buffer                   = 0x25, // Addr PA, Sequence
    SnorCmd_AMD_Program_Buffer_to_Flash           = 0x29, // Addr SA
    SnorCmd_AMD_Buffer_Abort_Reset                = 0xf0, // Addr 0x555, Sequence
    // Unlock Bypass Mode
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_UB_Entry                          = 0x20, // Addr 0x555, Sequence
    SnorCmd_AMD_UB_Program                        = 0xa0, // Addr XX, Req. UB Entry
    SnorCmd_AMD_UB_Erase_Setup                    = 0x80, // Addr XX, Req. UB Entry
    SnorCmd_AMD_UB_Sector_Erase                   = 0x30, // Addr SA, Req. UB Erase Setup
    SnorCmd_AMD_UB_Chip_Erase                     = 0x10, // Addr SA, Req. UB Erase Setup
    // Erase
    SnorCmd_AMD_Erase_Setup                       = 0x80, // Addr 0x555, Sequence
    SnorCmd_AMD_Chip_Erase                        = 0x10, // Addr 0x555, Req. Erase Setup+Sequence
    SnorCmd_AMD_Sector_Erase                      = 0x30, // Addr SA, Req. Erase Setup+Sequence
    // Erase/Program Suspend/Resume
    SnorCmd_AMD_Suspend                           = 0xb0, // Addr XX
    SnorCmd_AMD_Resume                            = 0x30, // Addr 0x30
    // Secured Silicon Sector - SSS_READ Addr 0x00, DATA
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_SSS_Entry                         = 0x88, // Addr 0x555, Sequence
    SnorCmd_AMD_SSS_Program                       = 0xa0, // Addr 0x555, Req. SSS Entry+Sequence
    SnorCmd_AMD_SSS_Exit_0                        = 0x90, // Addr 0x555, Sequence
    SnorCmd_AMD_SSS_Exit_1                        = 0x00, // Addr XX
    // Lock Register Bits - LRB_READ Addr 0x00, DATA
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_LRB_Entry                         = 0x40, // Addr 0x555, Sequence
    SnorCmd_AMD_LRB_Program                       = 0xa0, // Addr XX, Req. LRB Entry
    // Password Protection
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_PP_Entry                          = 0x60, // Addr 0x555, Sequence
    SnorCmd_AMD_PP_Program                        = 0xa0, // Addr XX, Req. PP Entry
    SnorCmd_AMD_PP_Unlock_0                       = 0x25, // Addr 0x00, Req. PP Entry
    SnorCmd_AMD_PP_Unlock_1                       = 0x03, // Addr 0x00
    /* Next 4 Writes are of the password to their respective addresses (like below) */
        // Data = PWD0, // Addr 0x00
        // Data = PWD1, // Addr 0x01
        // Data = PWD2, // Addr 0x02
        // Data = PWD2, // Addr 0x03
    SnorCmd_AMD_PP_Unlock_2                       = 0x29, // Addr 0x00
    // Non-Volatile Sector Protection (PPB) - PPB_READ Addr SA, Data RD(0)
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_PPB_Entry                         = 0xc0, // Addr 0x555, Sequence
    SnorCmd_AMD_PPB_Program_0                     = 0xa0, // Addr XX, Req. PPB Entry
    SnorCmd_AMD_PPB_Program_1                     = 0x00, // Addr SA
    SnorCmd_AMD_PPB_All_Erase_0                   = 0x80, // Addr XX, Req. PPB Entry
    SnorCmd_AMD_PPB_All_Erase_1                   = 0x30, // Addr 00
    // Global Volatile Sector Protection Freeze (PPB Lock) - PPBL_READ Addr XX, Data RD(0)
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_PPBL_Entry                        = 0x50, // Addr 0x555, Sequence
    SnorCmd_AMD_PPBL_Set_0                        = 0xa0, // Addr XX, Req. PPBL Entry
    SnorCmd_AMD_PPBL_Set_1                        = 0x00, // Addr XX
    // Volatile Sector Protection (DYB) - DYB_READ Addr SA, Data RD(0)
        /* Must Exit - Else unknown state */
    SnorCmd_AMD_DYB_Entry                         = 0xe0, // Addr 0x555, Sequence
    SnorCmd_AMD_DYB_Setup                         = 0xa0, // Addr XX, Req. DYB Entry
    SnorCmd_AMD_DYB_Set                           = 0x00, // Addr SA, Req. DYB Setup
    SnorCmd_AMD_DYB_Clear                         = 0x01  // Addr SA, Req. DYB Setup
} SnorCmd_AMD; // AMD Command Set (x16)


SnorCommandStatus AMDFormatDevice (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvBool IsWaitForCompletion);

SnorCommandStatus AMDEraseSector (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 StartOffset,
    NvU32 Length,
    NvBool IsWaitForCompletion);

SnorCommandStatus AMDProtectSectors(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 Offset,
    NvU32 Size,
    NvBool IsLastPartition);

SnorCommandStatus AMDUnprotectAllSectors(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo);

SnorCommandStatus AMDProgram (
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvU32 ByteOffset,
    NvU32 SizeInBytes,
    const NvU8* Buffer,
    NvBool IsWaitForCompletion);

SnorCommandStatus AMDReadMode(
    SnorDeviceIntRegister *pDevIntRegs,
    SnorDeviceInfo *pDevInfo,
    NvBool IsWaitForCompletion);



#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_AMD_COMMANDSET_H


