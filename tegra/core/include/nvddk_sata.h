/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * Defines the parameters and data structure for SATA devices.
 */

#ifndef INCLUDED_NVDDK_SATA__H
#define INCLUDED_NVDDK_SATA__H

#include "nvcommon.h"
#include "nverror.h"
#include "nvddk_blockdev.h"
#if defined(__cplusplus)
extern "C"
{
#endif

/* Enable the following flag to enable debug prints */
#define ENABLE_DEBUG_PRINTS 0

#if ENABLE_DEBUG_PRINTS
#define SATA_DEBUG_PRINT(x) NvOsDebugPrintf x
#else
#define SATA_DEBUG_PRINT(x)
#endif

/* Error prints are enabled by default */
#define SATA_ERROR_PRINT(x) NvOsDebugPrintf x

typedef enum
{
    /// Specifies Legacy mode
    NvDdkSataMode_Legacy = 1,

    /// Specifies AHCI mode compliant with AHCI 1.3 spec
    NvDdkSataMode_Ahci,

    NvDdkSataMode_Num,

    NvDdkSataMode_Force32 = 0x7FFFFFF
} NvDdkSataMode;

typedef enum
{
    /// Specifies Pio mode transfer from device to memory by SATA controller
    NvDdkSataTransferMode_Pio = 1,

    /// Specifies DMA transfer from device to memory by SATA controller
    NvDdkSataTransferMode_Dma,

    NvDdkSataTransferMode_Num,

    NvDdkSataTransferMode_Force32 = 0x7FFFFFF
} NvDdkSataTransferMode;

typedef enum
{
    /// Specifies SATA clock source to be PLLC.
    NvDdkSataClockSource_ClkM = 1,

    /// Specifies SATA clock source to be PLLP.
    NvDdkSataClockSource_PllPOut0,

    /// Specifies SATA clock source to be PLLM.
    NvDdkSataClockSource_PllMOut0,

    NvDdkSataClockSource_Num,

    /// Specifies SATA clock source to be PLLC.
    /// Supported as clock source for SATA but not suppoted in bootrom because
    /// PLLC is not enabled in bootrom.
    NvDdkSataClockSource_PllCOut0,

    NvDdkSataClockSource_Force32 = 0x7FFFFFF
} NvDdkSataClockSource;

/**
 * Defines the parameters SATA devices.
 */
typedef struct NvDdkSataParamsRec
{
    /// Specifies the clock source for SATA controller.
    NvDdkSataClockSource SataClockSource;

    /// Specifes the clock divider to be used.
    NvU32 SataClockDivider;

    /// Specifies the clock source for SATA controller.
    NvDdkSataClockSource SataOobClockSource;

    /// Specifes the clock divider to be used.
    NvU32 SataOobClockDivider;

    /// Specifies the SATA mode to be used.
    NvDdkSataMode SataMode;

    /// Specifies mode of transfer from device to memory via controller.
    /// Note: transfer mode doesn't imply SW intervention.
    NvDdkSataTransferMode TransferMode;

    /// If SDRAM is initialized, then AHCI DMA mode can be used.
    /// If SDRAM is not initialized, then always use legacy pio mode.
    NvBool isSdramInitialized;

   /// Start address(in SDRAM) for command and FIS structures.
    NvU32 SataBuffersBase;

   /// Start address(in SDRAM) for data buffers.
    NvU32 DataBufferBase;

    /// Plle ref clk divisors
    NvU8 PlleDivM;

    NvU8 PlleDivN;

    NvU8 PlleDivPl;

    NvU8 PlleDivPlCml;

    /// Plle SS coefficients whcih will take effect only when
    /// Plle SS is enabled through the boot dev config fuse
    NvU16 PlleSSCoeffSscmax;

    NvU8 PlleSSCoeffSscinc;
    NvU8 PlleSSCoeffSscincintrvl;

} NvDdkSataParams;

typedef struct NvDdkSataContextRec
{
    // Currently used Sata mode.
    NvDdkSataMode SataMode;

    // Currently used SATA transfer mode.
    NvDdkSataTransferMode XferMode;

    // Indicates if sata controlelr is up
    NvBool IsSataInitialized;

    // Indicates the log2 of  page size, page size multiplier being
    // selected through boot dev config fuse. Page size is primarily for logical
    // organization of data on sata and not really pertains to the physical layer
    // organization. The transfer unit for SATA is 512byte sector and the data
    // on SATA device is also orgnanized in terms of 512 byte sectors.
    NvU32 PageSizeLog2;

    NvU32 PagesPerBlock;

    NvU32 SectorsPerPage;

    // Base for Cmd and FIS structures(in SDRAM) for AHCI dma
    // This will serve as the command list base
    NvU32 SataBuffersBase;

    // Command tables base
    NvU32 CmdTableBase;

    // FIS base
    NvU32 FisBase;

    // Base for data buffers(in SDRAM) for AHCI dma
    NvU32 DataBufferBase;

} NvDdkSataContext;

typedef struct
{
    NvU32 UsePllpSrcForPlle;

    NvU32 EnbPlleSS;

    NvU32 NumComresetRetries;

    NvU32 ForceGen1;

    NvU32 Enb32BitReads;

    NvU32 ComInitWaitIndex;

    NvU32 PageSizeMultLog2;

} SataFuseParam;

typedef enum
{
    USE_PLLP_SOURCE_FUSE_MASK = 0x1,

    ENB_PLLE_SS_FUSE_MASK = 0x2,

    COMRESET_RETRIES_FUSE_MASK = 0xC,

    FORCE_GEN1_FUSE_MASK = 0x10,

    ENB_DWORD_LEGACY_PIO_READ_FUSE_MASK = 0x20,

    COMINIT_WAIT_IND_FUSE_MASK = 0x1C0,

    PAGE_SIZE_MULT_LOG2_FUSE_MASK = 0xE00

} SataFuseParamMask;

typedef enum
{
    USE_PLLP_SOURCE_FUSE_SHIFT = 0x0,

    ENB_PLLE_SS_FUSE_SHIFT = 0x1,

    COMRESET_RETRIES_FUSE_SHIFT = 0x2,

    FORCE_GEN1_FUSE_SHIFT = 0x4,

    ENB_DWORD_LEGACY_PIO_READ_FUSE_SHIFT = 0x5,

    COMINIT_WAIT_IND_FUSE_SHIFT = 0x6,

    PAGE_SIZE_MULT_LOG2_FUSE_SHIFT = 0x9

} SataFuseParamShift;

typedef enum
{
    ENABLE_ALL_BYTES = 0x0,

    ENABLE_BYTE0 = 0xe,

    ENABLE_BYTE1 = 0xd,

    ENABLE_BYTE2 = 0xb,

    ENABLE_BYTE3 = 0x7,

    ENABLE_BYTE1_BYTE0 = 0xc

} LegacyPioBe;

typedef enum
{

    SataStatus_CommandListRunning = 0x1,

    SataStatus_DrqHigh = 0x2,

    SataStatus_BsyHigh = 0x4,

    SataStatus_PortBsy = 0x8

} SataStatus;

typedef enum
{
    SataInitStatus_SataReinitialized = 0x40,

    SataInitStatus_PlleReinitialized = 0x80,

    SataInitStatus_PlleInitFailed = 0x100,

    SataInitStatus_ComResetFailed = 0x200,

    SataInitStatus_SsdNotdetected = 0x400,

    SataInitStatus_PadPllNotLocked = 0x800

} SataInitStatus;

typedef struct
{
    NvU8 Gen1TxAmp;

    NvU8 Gen1TxPeak;

    NvU8 Gen2TxAmp;

    NvU8 Gen2TxPeak;

} SataPadCntrlReg;

typedef struct NvDdkSataStatusRec
{
    NvBool ParamsValidated;

    NvDdkSataClockSource SataClockSource;

    NvU32 SataClockDivider;

    NvDdkSataClockSource SataOobClockSource;

    NvU32 SataOobClockDivider;

    SataStatus PortStatus;

    NvDdkSataMode SataMode;

    NvDdkSataTransferMode SataTransferMode;

    SataInitStatus InitStatus;

    NvU32 LastBlockRead;

    NvU32 LastPageRead;

    NvU32 NumPagesRead;

} NvDdkSataStatus;

NvError
NvDdkSataInit(void);

NvError NvDdkSataReadPage(
    const NvU32 Block,
    const NvU32 Page,
    NvU8 *Dest);

NvError NvDdkSataRead(
    NvU32 SectorNum,
    NvU8 *pWriteBuffer,
    NvU32 NumberOfSectors);

NvError NvDdkSataWrite(
    NvU32 SectorNum,
    const NvU8 *pWriteBuffer,
    NvU32 NumberOfSectors);

NvError
NvDdkSataBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError NvDdkSataBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkSataBlockDevDeinit(void);

NvError NvDdkSataIdentifyDevice(NvU8 *Buf);

NvError NvDdkSataErase(NvU32 StartSector, NvU32 NumSectors);

#if defined(__cplusplus)
}
#endif

#endif
