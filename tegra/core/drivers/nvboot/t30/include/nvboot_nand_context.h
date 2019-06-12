/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * nvboot_nand_context.h - Definitions for the Nand context structure.
 */

#ifndef INCLUDED_NVBOOT_NAND_CONTEXT_H
#define INCLUDED_NVBOOT_NAND_CONTEXT_H

#include "nvboot_device_int.h"
#include "t30/nvboot_nand_param.h"

#if defined(__cplusplus)
extern "C"
{
#endif

// Safe trimmer value for Toggle DDR Nand
#define NVBOOT_NAND_FBIO_DQSIB_DLY_MAX 0X3F

/**
 * Buffer size for Reading the params from Onfi Flash.
 */
#define NVBOOT_NAND_ONFI_PARAM_BUF_SIZE_IN_BYTES 512

/// Defines various nand commands as per Nand spec.
typedef enum
{
    NvBootNandCommands_Reset = 0xFF,
    NvBootNandCommands_ReadId = 0x90,
    NvBootNandCommands_ReadId_Address = 0x20,
    NvBootNandCommands_JedecId_Address = 0x40,
    NvBootNandCommands_Read = 0x00,
    NvBootNandCommands_ReadStart = 0x30,
    NvBootNandCommands_ReadParamPage = 0xEC,
    NvBootNandCommands_ReadParamPageStart = 0xAB,
    NvBootNandCommands_Status = 0x70,
    NvBootNandCommands_SetFeature = 0xEF,
    NvBootNandCommands_SetFeature_TimingMode_Address = 0x01,
    NvBootNandCommands_SetFeature_EZNand_Control_Address = 0x50,
    NvBootNandCommands_CommandNone = 0,
    NvBootNandCommands_Force32 = 0x7FFFFFFF
} NvBootNandCommands;

/// Defines Nand command structure.
typedef struct
{
    NvBootNandCommands Command1;
    NvBootNandCommands Command2;
    NvU16 ColumnNumber;
    NvU16 PageNumber;
    NvU16 BlockNumber;
    NvU8 ChipNumber;
    NvU32 Param32;
} NvBootNandCommand;

/// Defines Data widths that are supported in BootRom.
typedef enum
{
    NvBootNandDataWidth_Discovey = 0,
    NvBootNandDataWidth_8Bit,
    NvBootNandDataWidth_16Bit,
    NvBootNandDataWidth_Num,
    NvBootNandDataWidth_Force32 = 0x7FFFFFFF
} NvBootNandDataWidth;

/**
 * Defines HW Ecc options that can be selected.
 * Ecc Discovery option lets the driver to discover the Ecc used.
 * Rs4 Ecc can correct upto 4 symbol per 512 bytes.
 * Bch8, Bch16 and Bch24 Ecc can correct upto 8, 16 and 24 bits per 512 bytes respectively.
 *
 */
typedef enum
{
    NvBootNandEccSelection_Discovery = 0,
    NvBootNandEccSelection_Bch4,
    NvBootNandEccSelection_Bch8,
    NvBootNandEccSelection_Bch16,
    NvBootNandEccSelection_Bch24,
    NvBootNandEccSelection_Off,
    NvBootNandEccSelection_Num,
    NvBootNandEccSelection_Force32 = 0x7FFFFFFF
} NvBootNandEccSelection;

/// Defines Nand interface types.
typedef enum
{
    NvBootNandDataInterface_Async = 0,
    NvBootNandDataInterface_Sync,
    NvBootNandDataInterface_Force32 = 0x7FFFFFFF
} NvBootNandDataInterface;

/// Defines EZ Nand auto retries options.
typedef enum
{
    NvBootOnfiEZNandAutoRetries_Disable = 0,
    NvBootOnfiEZNandAutoRetries_Enable,
    NvBootOnfiEZNandAutoRetries_Force32 = 0x7FFFFFFF
} NvBootOnfiEZNandAutoRetries;

/// Defines ONFI revision numbers.
typedef enum
{
    NvBootNandOnfiRevNum_None = 0,
    NvBootNandOnfiRevNum_1_0,
    NvBootNandOnfiRevNum_2_0,
    NvBootNandOnfiRevNum_2_1,
    NvBootNandOnfiRevNum_2_2,
    NvBootNandOnfiRevNum_2_3,
    NvBootNandOnfiRevNum_Force32 = 0x7FFFFFFF
} NvBootNandOnfiRevNum;

/// Defines ONFI revision numbers.
typedef enum
{
    NvBootNandToggleDDR_Discovey = 0,
    NvBootNandToggleDDR_Enable,
    NvBootNandToggleDDR_Disable,
    NvBootNandToggleDDR_Force32= 0x7FFFFFFF
} NvBootNandToggleDDR;

/// Defines BCH sector size values
typedef enum
{
    NvBootNandBchSectorSize_512 =0,
    NvBootNandBchSectorSize_1024 = 1,
    NvBootNandBchSectorSize_Force32= 0x7FFFFFFF
}NvBootNandBchSectorSize;

/**
 * NvBootNandContext - The context structure for the Nand driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 */
typedef struct NvBootNandContextRec
{
    /// Maker Code.
    NvU8 MakerCode;
    /// Device Code.
    NvU8 DeviceCode;
    /// Block size in Log2 scale.
    NvU8 BlockSizeLog2;
    /// Page size in Log2 scale.
    NvU8 PageSizeLog2;
    /// No of pages per block in Log 2 scale;
    NvU8 PagesPerBlockLog2;
    /// Number of address cycles.
    NvU8 NumOfAddressCycles;
    /// Nand Data width.
    NvBootNandDataWidth DataWidth;
    /// Nand Command params.
    NvBootNandCommand Command;
    /// Device status.
    NvBootDeviceStatus DeviceStatus;
    /// Holds the Nand Read start time.
    NvU64 ReadStartTime;
    /// Holds the selected ECC.
    NvBootNandEccSelection EccSelection;
    /// Holds the Onfi Revision Number
    NvBootNandOnfiRevNum OnfiRevNum;
    /// Onfi Nand part or not.
    NvBool IsOnfi;
    /// Error Free Nand part or not
    NvBool IsEFNand;
    /// ONFI EZ Nand part or not
    NvBool IsOnfiEZNand;
    /// Specifies EZ Nand supports automatic retries being enabled and disabled explicitly
    /// by the host using Set Feature
    NvBool IsSupportAutoRetries;
    /// Source Synchronous mode supported or not
    NvBool IsSyncDDRModeSupported;
    /// Enable or Disable SyncDDR interface
    NvBool DisableSyncDDR;
    /// Max synchronous timing mode supported
    NvU8 MaxSyncTimingMode;
    /// Max asynchronous timing mode supported
    NvU8 MaxAsyncTimingMode;
    /// Toggle DDR mode suported or not
    NvBool IsToggleModeSupported;
    /// Programmable offset for Main data ecc bytes in spare
    NvU8 MainEccOffset;
    /// Bch sector size selection for ECC computation
    NvU8 BchSectorSize;
    /// Buffer for reading Onfi params. Define as NvU32 to make it word aligned.
    NvU32 OnfiParamsBuffer[NVBOOT_NAND_ONFI_PARAM_BUF_SIZE_IN_BYTES / sizeof(NvU32)];
} NvBootNandContext;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_NAND_CONTEXT_H */
