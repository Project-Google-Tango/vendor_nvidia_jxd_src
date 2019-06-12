/*
 * Copyright (c) 2007 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Boot Information Table (BIT) for code-name Logan (T12x)</b>
 *
 */

/**
 * @defgroup nvbdk_bit Boot Information Table APIs for code-name Logan (T12x)
 *
 * The \c NvBootInfoTable structure (BIT) provides information from the
 * Boot ROM (BR) to boot loaders (BLs).
 *
 * The BIT is initially cleared.
 *
 * As the BR works its way through the boot sequence, it records data in
 * the BIT. This includes information determined as it boots and a log
 * of the boot process. The BIT also contains a pointer to an in-memory,
 * plain text copy of the Boot Configuration Table (BCT).
 *
 * The BIT allows the BLs to determine how they were loaded, any errors that
 * occured along the way, and which of the set of BLs was finally loaded.
 *
 * The BIT also serves as a tool for diagnosing boot failures. The cold boot
 * process is necessarily opaque, and the BIT provides a window into what
 * actually happened. If the device is completely unable to boot, the BR
 * will enter Recovery Mode (RCM), and load an applet that dumps the BIT
 * and BCT contents for analysis on the host.
 *
 * @ingroup nvbdk_modules
 *
 * @{
 */

#ifndef INCLUDED_NVBOOT_BIT_H
#define INCLUDED_NVBOOT_BIT_H

#include "nvcommon.h"
#include "nvboot_bct.h"
#include "nvboot_config.h"
#include "nvboot_osc.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/// Specifies the amount of status data needed in the BIT.
/// One bit of status is used for each of the journal blocks,
/// and 1 additional bit is needed for the second BCT in block 0.
#define NVBOOT_BCT_STATUS_BITS  (NVBOOT_MAX_BCT_SEARCH_BLOCKS + 1)
/// Specifies the number of status bytes in the BIT.
#define NVBOOT_BCT_STATUS_BYTES ((NVBOOT_BCT_STATUS_BITS + 7) >> 3)

/**
 * Defines the type of boot.
 * @note There is no BIT for warm boot.
 */
typedef enum
{
    /// Specifies a default (unset) value.
    NvBootType_None = 0,

    /// Specifies a cold boot.
    NvBootType_Cold,

    /// Specifies the BR entered RCM.
    NvBootType_Recovery,

    /// Specifies UART boot (only available internal to NVIDIA).
    NvBootType_Uart,

    /// Specifies that the BR immediately exited for debugging
    /// purposes.
    /// This can only occur when NOT in ODM production mode,
    /// and when a special BOOT_SELECT value is set.
    NvBootType_ExitRcm,

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

    /// Specifies a validation failure.
    NvBootRdrStatus_ValidationFailure,

    /// Specifies a read error.
    NvBootRdrStatus_DeviceReadError,

    NvBootRdrStatus_Force32 = 0x7fffffff
} NvBootRdrStatus;

/**
 * Defines the information recorded about each BL.
 *
 * BLs that do not become the primary copy to load have status of
 * None. They may still experience ECC errors if used to recover from
 * ECC errors of another copy of the BL.
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
 * Defines the status from NAND.
 */
typedef struct NvBootNandStatusRec
{
    //
    // Parameters specified by fuses or straps.
    //

    /// Holds the data width specified by fuses or straps.
    NvU32 FuseDataWidth;

    /// Holds whether ONFI support was disabled by fuses or straps.
    NvBool FuseDisableOnfiSupport;

    /// Holds the ECC specified by fuses or straps.
    NvU32 FuseEccSelection;

    /// Holds the page or block size offset specified by fuses or straps.
    NvU8 FusePageBlockSizeOffset;

    //
    // Parameters discovered during operation.
    //

    /// Holds the discovered data width.
    NvU32 DiscoveredDataWidth;

    /// Holds the discovered ECC.
    NvU32 DiscoveredEccSelection;

    //
    // Parameters provided by the device.
    //

    /// Holds IdRead.
    NvU32 IdRead;

    /// Holds IdRead2.
    NvU32 IdRead2;

    /// Holds if the part is an ONFI device.
    NvBool IsPartOnfi;

    //
    // Information for driver validation.
    //

    /// Holds the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Holds the number of pages whose read resulted in uncorrectable errors.
    NvU32 NumUncorrectableErrorPages;

    /// Holds the number of pages whose read resulted in correctable errors.
    NvU32 NumCorrectableErrorPages;

    /// Holds the maximum number of correctable errors encountered.
    NvU32 MaxCorrectableErrorsEncountered;
} NvBootNandStatus;

/**
 * Defines the status from USB3.
 */
typedef struct NvBootUsb3StatusRec
{
    //
    // Parameters specified by fuses or straps.
    //

    NvU8                PortNum;
    //
    // Parameters provided by the device.
    //

    /// Holds the sense key as returned by mode sense command.
    NvU8                SenseKey;

    /// Holds the CSW status.
    NvU32              CurrCSWTag;
    NvU32              CurrCmdCSWStatus;
    NvU32              CurrEpBytesNotTransferred;

    /// Holds the inquiry response.
    NvU32               PeripheralDevTyp;

    /// Holds the read format capacity.
    NvU32               NumBlocks;

    /// Holds the read capacity response.
    NvU32               LastLogicalBlkAddr;
    NvU32               BlockLenInByte;

    /// Holds the pointer to \c NvBootUsb3Context.
    NvU32               Usb3Context;

    NvU32               InitReturnVal;
    NvU32               ReadPageReturnVal;
    NvU32               XusbDriverStatus;

    /// Holds the device status.
    NvU32               DeviceStatus;

    /// Holds the endpoint status.
    NvU32               EpStatus;

   } NvBootUsb3Status;

/**
 * Defines the status from eMMC and eSD.
 */
typedef struct NvBootSdmmcStatusRec
{
    ///
    /// Parameters specified by fuses or straps.
    ///

    /// Holds the data width specified by fuses or straps.
    NvU8 FuseDataWidth;

    /// Holds the voltage range specified by fuses or straps.
    NvU8 FuseVoltageRange;

    /// Holds whether boot mode was disabled by fuses or straps.
    NvBool FuseDisableBootMode;

    /// Holds the DDR mode specified by fuses or straps.
    NvBool FuseDdrMode;

    ///
    /// Parameters discovered during operation.
    ///

    /// Holds the discovered card type.
    NvU8 DiscoveredCardType;

    /// Holds the discovered voltage range.
    NvU32 DiscoveredVoltageRange;

    /// Holds the data width chosen to conform to power class constraints.
    NvU8 DataWidthUnderUse;

    /// Holds the power class chosen to conform to power class constraints.
    NvU8 PowerClassUnderUse;

    /// Holds the auto cal status.
    NvBool AutoCalStatus;

    //
    // Parameters provided by the device.
    //

    /// Holds the card identification data.
    NvU32 Cid[4];

    //
    // Information for driver validation.
    //

    /// Holds the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Holds the number of CRC errors.
    NvU32 NumCrcErrors;

    /// Holds whether the boot was attempted from a boot partition.
    NvU8 BootFromBootPartition;

    /// Holds whether the boot mode read is successful.
    NvBool BootModeReadSuccessful;
} NvBootSdmmcStatus;

/**
 * Defines the status from SNOR.
 */
typedef struct
{

    /// Holds the chosen clock source.
    NvU32 ClockSource;
    /// Holds the chosen clock divider.
    NvU32 ClockDivider;

    /**
     *
     * Holds the SNOR configuration.
     *
     * - [7:0]    : BlockSizeLog2
     * - [15:8]   : XferSizeLog2
     * - [17:16]    : SnorPageSize : 2 bit value to indicate the page size
     *                for page mode reads.
     * - [19:18]    : SnorBurstLength : 2 bit value to indicate the burstlength
     * - [21:20]    : DeviceMode : 2 bit value to indicate the nor device mode used.
     * - [22:22]    : DataXferMode : Specifies Pio or dma mode is used.
     *
     */
    NvU32 SnorConfig;

    /// Holds the SNOR timing configuration reg 0.
    NvU32 TimingCfg0;

    /// Holds the SNOR timing configuration reg 1.
    NvU32 TimingCfg1;

    /// Holds the SNOR timing configuration reg 2.
    NvU32 TimingCfg2;

    /// Holds the last block read.
    NvU32 LastBlockRead;
    /// Holds the last page read.
    NvU32 LastPageRead;

    /**
     *
     * Holds the initialization status.
     * - [0:0]   : ParamsValidated : NV_TRUE if parameters were validated and are
     *           valid.
     * - [8:1]   : InitStatus : status of snor controller initialization. 8 bits
     *           are enough to hold 256 error codes.
     * - [16:9]   : ReadPageStatus : NvBootError status returned by the current
     *           page read operation. 8 bits are enough to hold 256 error codes.
     * - [17:17]   : WaitForControllerBsy : NV_TRUE if wait for BSY bit was initiated.
     * - [18:18] : WaitForDmaDone : NV_TRUE if wait for DMA_DONE bit was initiated.
     * - [19:19] : ControllerIdle : NV_TRUE if BSY bit was cleared after a read
     *           operation was initiated.
     * - [20:20] : DmaDone : NV_TRUE if DMA operation completed successfully.
     *
     */
    NvU32 SnorDriverStatus;

} NvBootSnorStatus;

/**
 * Defines the status from SPI flash devices.
 */
typedef struct
{
    /// Holds the chosen clock source.
    NvU32 ClockSource;

    /// Holds the chosen clock divider.
    NvU32 ClockDivider;

    /// Holds whether fast read was selected.
    NvU32 IsFastRead;

    /// Holds the number of pages read from the beginning.
    NvU32 NumPagesRead;

    /// Holds the last block read.
    NvU32 LastBlockRead;

    /// Holds the last page read.
    NvU32 LastPageRead;

    /// Holds the boot status.
    NvU32 BootStatus;

    /// Holds the init status.
    NvU32 InitStatus;

    /// Holds the read status.
    NvU32 ReadStatus;

    /// Holds whether parameters successfully validated.
    NvU32 ParamsValidated;
} NvBootSpiFlashStatus;

/*
 * Defines status structures of all the supported secondary boot device types.
 */
typedef union NvBootSecondaryDeviceStatusRec
{
    /// Holds the status from NAND.
    NvBootNandStatus          NandStatus;

    /// Holds the status from USB3.
    NvBootUsb3Status Usb3Status;

    /// Holds the status from eMMC and eSD.
    NvBootSdmmcStatus         SdmmcStatus;

    /// Holds the status from SNOR.
    NvBootSnorStatus          SnorStatus;

    /// Holds the status from SPI flash.
    NvBootSpiFlashStatus      SpiStatus;

} NvBootSecondaryDeviceStatus;

/**
 * Defines boot time logging.
 */
typedef struct NvBootTimeLogRec
{
    /// Holds the initialization timestamp.
    NvU32	NvBootTimeLogInit;

    /// Holds the exit timestamp.
    NvU32	NvBootTimeLogExit;

    /// Holds the BCT read tick count.
    NvU32	NvBootReadBctTickCnt;

    /// Holds the BL read tick count.
    NvU32	NvBootReadBLTickCnt;
} NvBootTimeLog;

/**
 * Defines the BIT.
 *
 * <b>Notes</b>:
 * - \c SecondaryDevice: This is set by cold boot (and soon UART) processing.
 *   Recovery mode does not alter its value.
 * - \c BctStatus[] is a bit vector representing the cause of BCT read failures.
 *   -    A 0 bit indicates a validation failure
 *   -    A 1 bit indicates a device read error
 *   -    Bit 0 contains the status for the BCT at block 0, slot 0.
 *   -    Bit 1 contains the status for the BCT at block 0, slot 1.
 *   -    Bit N contains the status for the BCT at block (N-1), slot 0, which
 *        is a failed attempt to locate the journal block at block N.
 *        <br>(1 <= N < NVBOOT_MAX_BCT_SEARCH_BLOCKS)
 * - \c BctLastJournalRead contains the cause of the BCT search within the
 *   journal block. Success indicates the search ended with the successful
 *   reading of the BCT in the last slot.  CRC and ECC failures are indicated
 *   appropriately.
 */
typedef struct NvBootInfoTableRec
{
    //
    // Version information.
    //

    /// Holds the version number of the BR code.
    NvU32               BootRomVersion;

    /// Holds the version number of the BR data structure.
    NvU32               DataVersion;

    /// Holds the version number of the RCM protocol.
    NvU32               RcmVersion;

    /// Holds the type of boot.
    NvBootType          BootType;

    /// Holds the primary boot device.
    NvBootDevType       PrimaryDevice;

    /// Holds the secondary boot device.
    NvBootDevType       SecondaryDevice;

    /// Holds the boot time logging.
    NvBootTimeLog	BootTimeLog;

    //
    // Hardware status information.
    //

    /// Holds the measured oscillator frequency.
    NvBootClocksOscFreq OscFrequency;

    /// Holds whether the device was initialized.
    NvBool              DevInitialized;

    /// Holds whether SDRAM was initialized.
    NvBool              SdramInitialized;

    /// Holds whether the force recovery AO bit was cleared.
    NvBool              ClearedForceRecovery;

    /// Holds whether the fail back AO bit was cleared.
    NvBool              ClearedFailBack;

    /// Holds whether fail back was invoked.
    NvBool              InvokedFailBack;

    /// Holds the IROM patch status.
    /// - [3:0] - Hamming decode status.
    /// - [6:4] - Reserved '0'.
    /// - [7:7] - IROM patch fuse payload present.
    NvU8                IRomPatchStatus;

    //
    // BCT information.
    //

    /// Holds if a valid BCT was found.
    NvBool              BctValid;

    /// Holds the status of attempting to read BCTs during the
    /// BCT search process.  See the notes above for more details.
    NvU8                BctStatus[NVBOOT_BCT_STATUS_BYTES];

    /// Holds the status of the last journal block read.
    NvBootRdrStatus     BctLastJournalRead;

    /// Holds the block number in which the BCT was found.
    NvU32               BctBlock;

    /// Holds the page number of the start of the BCT that was found.
    NvU32               BctPage;

    /// Holds the size of the BCT in bytes. It is 0 until BCT loading
    /// is attempted.
    NvU32               BctSize;

    /// Holds a pointer to the BCT in memory. It is NULL until BCT
    /// loading is attempted. The BCT in memory is the last BCT that
    /// the BR tried to load, regardless of whether the operation was
    /// successful.
    NvBootConfigTable  *BctPtr;

    /// Holds the state of attempts to load each of the BLs.
    NvBootBlState       BlState[NVBOOT_MAX_BOOTLOADERS];

    /// Holds device-specific status information from the operation.
    /// of the secondary boot device.
    NvBootSecondaryDeviceStatus SecondaryDevStatus;

    /// Holds the status of USB charger detection.
    /// - [0:0] = ChargerDetectionEnabled (0 if disabled, 1 if enabled).
    /// - [1:1] = IsBatteryLowDetected (1 if low, 0 if high).
    /// - [2:2] = IsUsbCableConnected (1 if USB cable is connected, 0 if not).
    /// - [3:3] = IsChargingPort (1 if DCP/CDP, 0 if not).
    /// - [4:4] = PmicHighCurrentAsserted (1 if driven high, 0 if low).
    NvU32 UsbChargingStatus;

    /// Holds the lowest iRAM address that preserves communicated data.
    /// \a SafeStartAddr starts out with the address of memory following
    /// the BIT. When BCT loading starts, it is bumped up to the
    /// memory following the BCT.
    NvU32               SafeStartAddr;

} NvBootInfoTable;

#if defined(__cplusplus)
}
#endif

/** @} */
#endif /* #ifndef INCLUDED_NVBOOT_BIT_H */
