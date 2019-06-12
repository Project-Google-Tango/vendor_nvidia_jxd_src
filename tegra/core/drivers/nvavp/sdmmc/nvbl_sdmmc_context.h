/*
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */


#ifndef INCLUDED_NVAVP_SDMMC_CONTEXT_H
#define INCLUDED_NVAVP_SDMMC_CONTEXT_H

#include "nvboot_device_int.h"

#if defined(__cplusplus)
extern "C"
{
#endif
/// Defines various data widths supported.
typedef enum
{
    /**
     * Specifies a 1 bit interface to eMMC.
     * Note that 1-bit data width is only for the driver's internal use.
     * Fuses doesn't provide option to select 1-bit data width.
     * The driver selects 1-bit internally based on need.
     * It is used for reading Extended CSD and when the power class
     * requirements of a card for 4-bit or 8-bit transfers are not
     * supported by the target board.
     */
    NvBootSdmmcDataWidth_1Bit = 0,

    /// Specifies a 4 bit interface to eMMC.
    NvBootSdmmcDataWidth_4Bit = 1,

    /// Specifies a 8 bit interface to eMMC.
    NvBootSdmmcDataWidth_8Bit = 2,

    /// Specifies a 4 bit Ddr interface to eMMC.
    NvBootSdmmcDataWidth_Ddr_4Bit = 5,

    /// Specifies a 8 bit Ddr interface to eMMC.
    NvBootSdmmcDataWidth_Ddr_8Bit = 6,

    NvBootSdmmcDataWidth_Num,
    NvBootSdmmcDataWidth_Force32 = 0x7FFFFFFF
} NvBootSdmmcDataWidth;

/// Sdmmc Response buffer size.
#define NVBOOT_SDMMC_RESPONSE_BUFFER_SIZE_IN_BYTES 16
/// Buffer size for reading Extended CSD.
#define NVBOOT_SDMMC_ECSD_BUFFER_SIZE_IN_BYTES 512
/// Buffer size for reading data in boot mode.
#define NVBOOT_SDMMC_BOOT_MODE_BUFFER_SIZE_IN_BYTES 512
/// Number of SD controllers supported
#define NVBOOT_SDMMC_MAX 2

/// Defines Command Responses of Emmc/Esd.
typedef enum
{
    SdmmcResponseType_NoResponse = 0,
    SdmmcResponseType_R1 = 1,
    SdmmcResponseType_R2 =2,
    SdmmcResponseType_R3 =3,
    SdmmcResponseType_R4 = 4,
    SdmmcResponseType_R5 = 5,
    SdmmcResponseType_R6 = 6,
    SdmmcResponseType_R7 = 7,
    SdmmcResponseType_R1B = 8,
    SdmmcResponseType_Num,
    SdmmcResponseType_Force32 = 0x7FFFFFFF
} SdmmcResponseType;

/**
 * Defines Emmc/Esd Commands as per Emmc/Esd spec's.
 * Emmc specific Commands starts with prefix Emmc and .Esd specific Commands
 * starts with prefix Esd. Common commands has no prefix.
 */
typedef enum
{
    SdmmcCommand_GoIdleState = 0,
    SdmmcCommand_EmmcSendOperatingConditions = 1,
    SdmmcCommand_AllSendCid = 2,
    SdmmcCommand_EmmcSetRelativeAddress = 3,
    SdmmcCommand_EsdSendRelativeAddress = 3,
    SdmmcCommand_Switch = 6,
    SdmmcCommand_SelectDeselectCard = 7,
    SdmmcCommand_EsdSendInterfaceCondition = 8,
    SdmmcCommand_EmmcSendExtendedCsd = 8,
    SdmmcCommand_SendCsd = 9,
    SdmmcCommand_StopTransmission =12,
    SdmmcCommand_SendStatus = 13,
    SdmmcCommand_SetBlockLength = 16,
    SdmmcCommand_ReadSingle = 17,
    SdmmcCommand_ReadMulti = 18,
    SdmmcCommand_SendTuningPattern = 19,
    SdmmcCommand_SetBlockCount = 23,
    SdmmcCommand_EsdAppSendOperatingCondition = 41,
    SdmmcCommand_EsdSelectPartition = 43,
    SdmmcCommand_EsdAppSendScr = 51,
    SdmmcCommand_EsdAppCommand = 55,
    SdmmcCommand_Force32 = 0x7FFFFFFF
} SdmmcCommand;


/// Defines Emmc/Esd card states.
typedef enum
{
    SdmmcState_Idle = 0,
    SdmmcState_Ready,
    SdmmcState_Ident,
    SdmmcState_Stby,
    SdmmcState_Tran,
    SdmmcState_Data,
    SdmmcState_Rcv,
    SdmmcState_Prg,
    SdmmcState_Force32 = 0x7FFFFFFF
} SdmmcState;

/// Defines Emmc card partitions.
typedef enum
{
    SdmmcAccessRegion_UserArea = 0,
    SdmmcAccessRegion_BootPartition1,
    SdmmcAccessRegion_BootPartition2,
    SdmmcAccessRegion_Num,
    SdmmcAccessRegion_Unknown,
    SdmmcAccessRegion_Force32 = 0x7FFFFFFF
} SdmmcAccessRegion;

/// Defines various card types supported.
typedef enum
{
    NvBootSdmmcCardType_Emmc = 0,
    NvBootSdmmcCardType_Esd,
    NvBootSdmmcCardType_Num,
    NvBootSdmmcCardType_Force32 = 0x7FFFFFFF
} NvBootSdmmcCardType;

/// Defines various clock rates to use for accessing the Emmc/Esd card at.
typedef enum
{
    NvBootSdmmcCardClock_Identification = 0,
    NvBootSdmmcCardClock_DataTransfer,
    NvBootSdmmcCardClock_20MHz,
    NvBootSdmmcCardClock_Num,
    NvBootSdmmcCardClock_Force32 = 0x7FFFFFFF
} NvBootSdmmcCardClock;

/**
 * The context structure for the Sdmmc driver.
 * A pointer to this structure is passed into the driver's Init() routine.
 * This pointer is the only data that can be kept in a global variable for
 * later reference.
 */
typedef struct NvBootSdmmcContextRec
{
    /// Block size.
    NvU8 BlockSizeLog2;
    /// Page size.
    NvU8 PageSizeLog2;
    /// No of pages per block;
    NvU8 PagesPerBlockLog2;
    /**
     * Clock divisor for the Sdmmc Controller Clock Source.
     * Sdmmc Controller gets PLL_P clock, which is 432MHz, as a clock source.
     * If it is set to 18, then clock Frequency at which Emmc controller runs
     * is 432/18 = 24MHz.
     */
    NvU8 ClockDivisor;
    /// Card's Relative Card Address.
    NvU32 CardRca;
    /// Data bus width.
    NvBootSdmmcDataWidth DataWidth;
    /// Response buffer. Define it as NvU32 to make it word aligned.
    NvU32 SdmmcResponse[NVBOOT_SDMMC_RESPONSE_BUFFER_SIZE_IN_BYTES / sizeof(NvU32)];
    /// Device status.
    NvBootDeviceStatus DeviceStatus;
    /// Holds the Movi Nand Read start time.
    NvU32 ReadStartTime;
    /**
     * This sub divides the clock that goes to card from controller.
     * If the controller clock is 24 Mhz and if this set to 2, then
     * the clock that goes to card is 12MHz.
     */
    NvU8 CardClockDivisor;
    /// Indicates whether to access the card in High speed mode.
    NvBool HighSpeedMode;
    /// Indicates whether the card is high capacity card or not.
    NvBool IsHighCapacityCard;
    /// Spec version.
    NvU8 SpecVersion;
    /// Holds Emmc Boot Partition size.
    NvU32 EmmcBootPartitionSize;
    /// Holds the current access region.
    SdmmcAccessRegion CurrentAccessRegion;
    /// Holds the current clock rate.
    NvBootSdmmcCardClock CurrentClockRate;
    /// Buffer for selecting High Speed, reading extended CSD and SCR.
    NvU8 SdmmcInternalBuffer[NVBOOT_SDMMC_ECSD_BUFFER_SIZE_IN_BYTES];
    /// Read access time1.
    NvU8 taac;
    /// Read access time2.
    NvU8 nsac;
    /// The clock frequency when not in high speed mode.
    NvU8 TranSpeed;
    /// Transfer speed in MHz.
    NvU8 TranSpeedInMHz;
    /// Indicates whether host supports high speed mode.
    NvBool HostSupportsHighSpeedMode;
    /// Indicates whether card supports high speed mode.
    NvBool CardSupportsHighSpeedMode;
    /// Indicates the page size to use for card capacity calculation.
    NvU8 PageSizeLog2ForCapacity;
    /// Power class for 26MHz at 3.6V.
    NvU8 PowerClass26MHz360V;
    /// Power class for 52MHz at 3.6V.
    NvU8 PowerClass52MHz360V;
    /// Power class for 26MHz at 1.95V.
    NvU8 PowerClass26MHz195V;
    /// Power class for 52MHz at 1.95V.
    NvU8 PowerClass52MHz195V;
    /// Boot Config from ExtCSD.
    NvU8 BootConfig;
    /// Indicates whether high voltage range is used for Card identification.
    NvBool IsHighVoltageRange;
    /// Max Power class supported by target board.
    NvU8 MaxPowerClassSupported;
    /// Number of blocks present in card.
    NvU32 NumOfBlocks;
    /// Holds read time out at current card clock frequency.
    NvU32 ReadTimeOutInUs;
    /// Buffer for storing the data read in boot mode.
    NvU8 SdmmcBootModeBuffer[NVBOOT_SDMMC_BOOT_MODE_BUFFER_SIZE_IN_BYTES];
    /// Flag to indicate whether reading is boot mode.
    NvBool BootModeReadInProgress;
    /// Flag to indicate the card speed and operating voltage level
    NvU8 CardSupportSpeed;
    /// Flag identicates card's bus width.
    NvU8 CardBusWidth;
    /// Spec 3 version or higher
    NvU8 Spec3Version;
    /// Power class for 52 Mhz DDR @ 3.6V.
    NvU8 PowerClass52MHzDdr360V;//PWR_CL_DDR_52_360
    /// Power class for 52 Mhz DDR @ 1.95V.
    NvU8 PowerClass52MHzDdr195V;//PWR_CL_DDR_52_195
    /// Indicates whether Ddr mode is used for data transfer
    NvU8 IsDdrMode;
    /// Indicates Sd controller in use
    NvU8 SdControllerId;
    NvU32 SdControllerBaseAddress;
    /// Store the current address of the external memory buffer being used.
    NvU8 *CurrentReadBufferAddress;
} NvBootSdmmcContext;

#if defined(__cplusplus)
}
#endif

#endif /* #ifndef INCLUDED_NVAVP_SDMMC_CONTEXT_H */

