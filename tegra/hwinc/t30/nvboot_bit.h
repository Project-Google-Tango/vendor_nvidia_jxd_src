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
 * NvBootInfoTable (BIT) provides information from the Boot ROM (BR)
 * to Bootloaders (BLs).
 *
 * Initially, the BIT is cleared.
 * 
 * As the BR works its way through its boot sequence, it records data in
 * the BIT.  This includes information determined as it boots and a log
 * of the boot process.  The BIT also contains a pointer to an in-memory,
 * plaintext copy of the Boot Configuration Table (BCT).
 *
 * The BIT allows BLs to determine how they were loaded, any errors that
 * occured along the way, and which of the set of BLs was finally loaded.
 *
 * The BIT also serves as a tool for diagnosing boot failures.  The cold boot
 * process is necessarily opaque, and the BIT provides a window into what
 * actually happened.  If the device is completely unable to boot, the BR
 * will enter Recovery Mode (RCM), using which one can load an applet that
 * dumps the BIT and BCT contents for analysis on the host.
 */

#ifndef INCLUDED_NVBOOT_BIT_H
#define INCLUDED_NVBOOT_BIT_H

#include "nvcommon.h"
#include "t30/nvboot_bct.h"
#include "t30/nvboot_config.h"
#include "t30/nvboot_osc.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/// Specifies the amount of status data needed in the BIT.
/// One bit of status is used for each of the journal blocks,
/// and 1 additional bit is needed for the second BCT in block 0.
#define NVBOOT_BCT_STATUS_BITS  (NVBOOT_MAX_BCT_SEARCH_BLOCKS + 1)
#define NVBOOT_BCT_STATUS_BYTES ((NVBOOT_BCT_STATUS_BITS + 7) >> 3)

/**
 * Defines the type of boot.
 * Note: There is no BIT for warm boot.
 */
typedef enum
{
    /// Specifies a default (unset) value.
    NvBootType_None = 0,

    /// Specifies a cold boot
    NvBootType_Cold,

    /// Specifies the BR entered RCM
    NvBootType_Recovery,

    /// Specifies UART boot (only available internal to NVIDIA)
    NvBootType_Uart,

    NvBootType_Force32 = 0x7fffffff    
} NvBootType;

/**
 * Defines the status codes for attempting to load a BL.
 */
typedef enum
{
    /// Specifies a default (unset) value.
    NvBootRdrStatus_None = 0,

    /// Specifies a successful load.
    NvBootRdrStatus_Success,

    /// Specifies validation failure.
    NvBootRdrStatus_ValidationFailure,

    /// Specifies a read error.
    NvBootRdrStatus_DeviceReadError,

    NvBootRdrStatus_Force32 = 0x7fffffff
} NvBootRdrStatus;

/**
 * Defines the information recorded about each BL.
 *
 * BLs that do not become the primary copy to load have status of None.
 * They may still experience Ecc errors if used to recover from ECC
 * errors of another copy of the BL.
 */
typedef struct NvBootBlStateRec
{
    /// Specifies the outcome of attempting to load this BL.
    NvBootRdrStatus Status;

    /// Specifies the first block that experienced an ECC error, if any.
    /// 0 otherwise.
    NvU32           FirstEccBlock;

    /// Specifies the first page that experienced an ECC error, if any.
    /// 0 otherwise.
    NvU32           FirstEccPage;

    /// Specifies the first block that experienced a correctable ECC error,
    /// if any. 0 otherwise. Only correctable errors that push the limits of
    /// the ECC algorithm are recorded (i.e., those very likely to become
    /// uncorrectable errors in the near future).
    NvU32           FirstCorrectedEccBlock;

    /// Specifies the first page that experienced a correctable ECC error,
    /// if any. 0 otherwise. Similar to FirstCorrectedEccBlock.
    NvU32           FirstCorrectedEccPage;

    /// Specifies if the BL experienced any ECC errors.
    NvBool          HadEccError;

    /// Specifies if the BL experienced any CRC errors.
    NvBool          HadCrcError;

    /// Specifies if the BL experienced any corrected ECC errors.
    /// As with FirstCorrectedEcc*, only nearly-uncorrectable errors count.
    NvBool          HadCorrectedEccError;

    /// Specifies if the BL provided data for another BL that experienced an
    /// ECC error.
    NvBool          UsedForEccRecovery;
} NvBootBlState;

/**
 * Defines the status from NAND
 */
typedef struct NvBootNandStatusRec
{
    ///
    /// Parameters specified by fuses or straps.
    ///

    /// Specifies the data width specified by fuses or straps.
    NvU32 FuseDataWidth;

    /// Specifies the # of address cycles specified by fuses or straps.
    NvU32 FuseNumAddressCycles;

    /// Specifies whether ONFI support was disabled by fuses or straps.
    NvU32 FuseDisableOnfiSupport;

    /// Specifies the ECC specified by fuses or straps.
    NvU32 FuseEccSelection;

    /// Specifies the page size offset specified by fuses or straps.
    NvU32 FusePageSizeOffset;

    /// Specifies the block size offset specified by fuses or straps.
    NvU32 FuseBlockSizeOffset;

    /// Specifies the pinmux selection specified by fuses or straps.
    NvU32 FusePinmuxSelection;

    /// Specifies the pin orderspecified by fuses or straps.
    NvU32 FusePinOrder;

    ///
    /// Parameters discovered during operation
    ///

    /// Specifies the discovered data width
    NvU32 DiscoveredDataWidth;

    /// Specifies the discovered # of address cycles
    NvU32 DiscoveredNumAddressCycles;

    /// Specifies the discovered ECC
    NvU32 DiscoveredEccSelection;

    ///
    /// Parameters provided by the device
    ///

    /// Specifies IdRead
    NvU32 IdRead;

    /// Specifies IdRead
    NvU32 IdRead2;

    /// Specifies if the part is an ONFI device
    NvU32 IsPartOnfi;

    ///
    /// Information for driver validation
    ///

    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the # of pages whose read resulted in uncorrectable errors.
    NvU32 NumUncorrectableErrorPages;

    /// Specifies the # of pages whose read resulted in correctable errors.
    NvU32 NumCorrectableErrorPages;

    /// Specifies the max # of correctable errors encountered.
    NvU32 MaxCorrectableErrorsEncountered;
} NvBootNandStatus;

/**
 * Defines the status from mobileLBA NAND
 */
typedef struct NvBootMobileLbaNandStatusRec
{
    ///
    /// Parameters specified by fuses or straps.
    ///

    /// Specifies the data width specified by fuses or straps.
    NvU32 FuseDataWidth;

    /// Specifies the # of address cycles specified by fuses or straps.
    NvU32 FuseNumAddressCycles;

    /// Specifies whether the BCT was read from the SDA region of the device.
    NvU32 FuseReadBctFromSda;

    /// Specifies the pinmux selection specified by fuses or straps.
    NvU32 FusePinmuxSelection;

    /// Specifies the pin orderspecified by fuses or straps.
    NvU32 FusePinOrder;

    ///
    /// Parameters discovered during operation
    ///

    /// Specifies the discovered data width
    NvU32 DiscoveredDataWidth;

    ///
    /// Parameters provided by the device
    ///

    /// Specifies IdRead
    NvU32 IdRead;

    ///
    /// Information for driver validation
    ///

    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the # of pages whose read resulted in uncorrectable errors.
    NvU32 NumUncorrectableErrorPages;
} NvBootMobileLbaNandStatus;

/**
 * Defines the status from eMMC and eSD
 */
typedef struct NvBootSdmmcStatusRec
{
    ///
    /// Parameters specified by fuses or straps.
    ///

    /// Specifies the data width specified by fuses or straps.
    NvU32 FuseDataWidth;

    /// Specifies the card type specified by fuses or straps.
    NvU32 FuseCardType;

    /// Specifies the voltage rangespecified by fuses or straps.
    NvU32 FuseVoltageRange;

    /// Specifies whether boot mode was disabled by fuses or straps.
    NvU32 FuseDisableBootMode;

    /// Specifies the pinmux selection specified by fuses or straps.
    NvU32 FusePinmuxSelection;

    /// Specifies the Ddr mode specified by fuses or straps.
    NvU32 FuseDdrMode;

    /// Specifies the Sd Controller specified by fuses or straps.
    NvU32 FuseSdCntrlSelection;

    ///
    /// Parameters discovered during operation
    ///

    /// Specifies the discovered card type
    NvU32 DiscoveredCardType;

    /// Specifies the discovered voltage range
    NvU32 DiscoveredVoltageRange;

    /// Specifies the data width chosen to conform to power class constraints
    NvU32 DataWidthUnderUse;

    /// Specifies the power class chosen to conform to power class constraints
    NvU32 PowerClassUnderUse;

    ///
    /// Parameters provided by the device
    ///

    /// Specifies the card identification data.
    NvU32 Cid[4];
 
    ///
    /// Information for driver validation
    ///

    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the # of CRC errors
    NvU32 NumCrcErrors;

    /// Specifies whether the boot was attempted from a Boot Partition.
    NvU32 BootFromBootPartition;

    /// Specifies whether the bootmode read is successful.
    NvU32 BootModeReadSuccessful;
} NvBootSdmmcStatus;

/**
 * Defines the status from SNOR
 */
typedef struct 
{
    /// Specifies if 32bit mode was specified by fuses or straps
    NvU32 IsFuse32BitMode;

    /// Specifies if non-muxed mode was specified by fuses or straps
    NvU32 IsFuseNonMuxedMode;

    /// Specifies the chosen clock source
    NvU32 ClockSource;
    /// Specifies the chosen clock divider
    NvU32 ClockDivider;
    
    /// Specifies the block size in use
    NvU32 BlockSize;

    /// Specifies the page size in use
    NvU32 PageSize;

    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the last block read
    NvU32 LastBlockRead;
    /// Specifies the last page read
    NvU32 LastPageRead;
    
    /// Specifies the init status
    NvU32 InitStatus; 

    /// Specifies whether parameters successfully validated
    NvU32 ParamsValidated;
} NvBootSnorStatus;

/**
 * Defines the status from MuxOneNAND and FlexMuxOneNAND devices
 */
typedef struct 
{
    /// Specifies the block size specified by fuses or straps
    NvU32 FuseBlockSize;
    /// Specifies the page size specified by fuses or straps
    NvU32 FusePageSize;

    /// Specifies the chosen clock source
    NvU32 ClockSource;
    /// Specifies the chosen clock divider
    NvU32 ClockDivider;
    
    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the last block read
    NvU32 LastBlockRead;
    /// Specifies the last page read
    NvU32 LastPageRead;

    /// Specifies the boot status
    NvU32 BootStatus; 

    /// Specifies the init status
    NvU32 InitStatus; 

    /// Specifies whether parameters successfully validated
    NvU32 ParamsValidated;
} NvBootMuxOneNandStatus;

/**
 * Defines the status from SPI flash devices
 */
typedef struct 
{
    /// Specifies the chosen clock source
    NvU32 ClockSource;
    /// Specifies the chosen clock divider
    NvU32 ClockDivider;

    /// Specifies whether fast read was selected
    NvU32 IsFastRead;
    
    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the last block read
    NvU32 LastBlockRead;
    /// Specifies the last page read
    NvU32 LastPageRead;

    /// Specifies the boot status
    NvU32 BootStatus; 

    /// Specifies the init status
    NvU32 InitStatus; 

    /// Specifies the read status
    NvU32 ReadStatus;

    /// Specifies whether parameters successfully validated
    NvU32 ParamsValidated;
} NvBootSpiFlashStatus;

/**
 * Defines the status from SATA
 */
typedef struct NvBootSataStatusRec
{

    /// Specifies the sata clock source
    NvU32 SataClockSource;

    /// Specifies the sata clock divider
    NvU32 SataClockDivider;

    /// Specifies the sata oob clock source
    NvU32 SataOobClockSource;

    /// Specifies the sata oob clock divider
    NvU32 SataOobClockDivider;

    /// Specifies the last used mode
    NvU32 SataMode;

    /// Specifies the last used mode of transfer
    NvU32 SataTransferMode;

    /// Specifies whether parameters are successfully validated
    NvU32 ParamsValidated;

    /// Specifies the init status
    /// InitStatus[5:0] : initialization status (makes use of the fact that
    /// NvBootError has notmore than 48 enumerated values)
    /// InitStatus[6] : Sata Reinitialized then 1, else 0
    /// InitStatus[7] : Plle reinitialized then 1, else 0
    /// InitStatus[8] : PlleInitFailed then 1, else 0
    /// InitStatus[9] : ComResetFailed (cominit was not received within the
    /// expected timelimit)  then 1, else 0
    /// InitStatus[10] : SSD detection failed then 1, else 0
    NvU32 InitStatus;

    /// Specifies the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Specifies the last page read
    NvU32 LastBlockRead;

    /// Specifies the last page read
    NvU32 LastPageRead;

    /// Specifies the Port Error that occurred
    NvU32 PortStatus;

    /// Specifies the sata buffers base
    NvU32 AhciSataBuffersBase;

    /// Specifies the data buffers base for ahci dma
    NvU32 AhciDataBuffersBase;

    /// Specifies if Ahci Dma Status
    /// AhciDmaStatus[0] :Ahci Dma transfer could not be completed within a
    /// stipulated time then 1, else 0
    /// AhciDmaStatus[1] : SDB FIS was not received then 1, else 0
    /// AhciDmaStatus[2] : An error occurred during ahci dma transfer then 1,
    /// else 0. If this bit is set, look at other fields of Bit sata status to
    /// ascertain the type of Ahci Dma Error that occurred.
    NvU32 AhciDmaStatus;

    /// Specifies if there was an ahci data transmission error
    NvU32 AhciDataXmissionError;

    /// Specifies if there was a command error in case the last transaction was
    /// of ahci dma type.
    NvU32 AhciCommandError;

    /// Specifies the Task File Data Error that occurred in case the last
    /// transaction was of ahci dma type
    NvU32 AhciTfdError;

} NvBootSataStatus;

/**
 * Defines NvBootSecondaryDeviceStatus to be the union of the individual
 * status structures of all the supported secondary boot device types.
 */
typedef union NvBootSecondaryDeviceStatusRec
{
    /// Specifies the status from NAND
    NvBootNandStatus          NandStatus;

    /// Specifies the status from mobleLBA NAND
    NvBootMobileLbaNandStatus MLbaStatus;

    /// Specifies the status from eMMC and eSD
    NvBootSdmmcStatus         SdmmcStatus;

    /// Specifies the status from SNOR
    NvBootSnorStatus          SnorStatus;

    /// Specifies the status from MuxOneNAND and FlexMuxOneNAND
    NvBootMuxOneNandStatus    MuxOneNandStatus;

    /// Specifies the status from SPI flash
    NvBootSpiFlashStatus      SpiStatus;

    /// Specifies the status for SATA
    NvBootSataStatus    SataStatus;
} NvBootSecondaryDeviceStatus;

/**
 * Defines the BIT.
 *
 * Notes:
 * * SecondaryDevice: This is set by cold boot (and soon UART) processing.
 *   Recovery mode does not alter its value.
 * * BctStatus[] is a bit vector representing the cause of BCT read failures.
 *       A 0 bit indicates a validation failure
 *       A 1 bit indicates a device read error
 *       Bit 0 contains the status for the BCT at block 0, slot 0.
 *       Bit 1 contains the status for the BCT at block 0, slot 1.
 *       Bit N contains the status for the BCT at block (N-1), slot 0, which
 *       is a failed attempt to locate the journal block at block N.
 *       (1 <= N < NVBOOT_MAX_BCT_SEARCH_BLOCKS)
 * * BctLastJournalRead contains the cause of the BCT search within the
 *   journal block. Success indicates the search ended with the successful
 *   reading of the BCT in the last slot.  CRC and ECC failures are indicated
 *   appropriately. 
 */
typedef struct NvBootInfoTableRec
{
    ///
    /// Version information
    ///

    /// Specifies the version number of the BR code.
    NvU32               BootRomVersion;

    /// Specifies the version number of the BR data structure.
    NvU32               DataVersion;

    /// Specifies the version number of the RCM protocol.
    NvU32               RcmVersion;

    /// Specifies the type of boot.
    NvBootType          BootType;

    /// Specifies the primary boot device.
    NvBootDevType       PrimaryDevice;

    /// Specifies the secondary boot device.
    NvBootDevType       SecondaryDevice;

    ///
    /// Hardware status information
    ///

    /// Specifies the measured oscillator frequency.
    NvBootClocksOscFreq OscFrequency;

    /// Specifies whether the device was initialized.
    NvBool              DevInitialized;

    /// Specifies whether SDRAM was initialized.
    NvBool              SdramInitialized;

    /// Specifies whether the ForceRecovery AO bit was cleared.
    NvBool              ClearedForceRecovery;

    /// Specifies whether the FailBack AO bit was cleared.
    NvBool              ClearedFailBack;

    /// Specifies whether FailBack was invoked.
    NvBool              InvokedFailBack;

    ///
    /// BCT information
    ///

    /// Specifies if a valid BCT was found.
    NvBool              BctValid;

    /// Specifies the status of attempting to read BCTs during the
    /// BCT search process.  See the notes above for more details.
    NvU8                BctStatus[NVBOOT_BCT_STATUS_BYTES];

    /// Specifies the status of the last journal block read.
    NvBootRdrStatus     BctLastJournalRead;

    /// Specifies the block number in which the BCT was found.
    NvU32               BctBlock;

    /// Specifies the page number of the start of the BCT that was found.
    NvU32               BctPage; 

    /// Specifies the size of the BCT in bytes.  It is 0 until BCT loading
    /// is attempted.
    NvU32               BctSize;  /* 0 until BCT loading is attempted */

    /// Specifies a pointer to the BCT in memory.  It is NULL until BCT
    /// loading is attempted.  The BCT in memory is the last BCT that
    /// the BR tried to load, regardless of whether the operation was
    /// successful.
    NvBootConfigTable  *BctPtr;

    /// Specifies the state of attempting to load each of the BLs.
    NvBootBlState       BlState[NVBOOT_MAX_BOOTLOADERS];

    /// Specifies device-specific status information from the operation
    /// of the secondary boot device.
    NvBootSecondaryDeviceStatus SecondaryDevStatus;

    /// Specifies the lowest iRAM address that preserves communicated data.
    /// SafeStartAddr starts out with the address of memory following
    /// the BIT.  When BCT loading starts, it is bumped up to the 
    /// memory following the BCT.
    NvU32               SafeStartAddr;

} NvBootInfoTable;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVBOOT_BIT_H */
