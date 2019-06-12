/*
 * Copyright (c) 2008-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

/**
 * @file
 * <b> NVIDIA Driver Development Kit: SDIO Interface</b>
 *
 * @b Description: SD memory (standard capacity as well
 * as high capacity), SDIO, MMC interface.
 */

/**
 * @defgroup nvddk_sdio SDIO Controller Interface
 *
 * This defines SD memory (standard capacity as well
 * as high capacity), SDIO, and MMC interfaces.
 *
 * SD memory (standard capacity as well as high capacity), SDIO, MMC
 * interface for controlling and managing SD/SDIO/MMC cards present
 * in SD slots on handheld and embedded devices.

 * This interface provides methods to initialize the SD module, identify
 * the type of the card present in the slot, and read or write data to the
 * SD memory/SDIO card. NvDdkSdio also has power management capabilities
 *
 * @ingroup nvddk_modules
 *
 * @{
 */

#ifndef INCLUDED_NVDDKSDIO_H
#define INCLUDED_NVDDKSDIO_H

#include "nvos.h"
#include "nvrm_init.h"
#include "nvrm_power.h"
#include "nvddk_blockdev.h"

#if defined(__cplusplus)
extern "C"
{
#endif  /* __cplusplus */

/**
 * Opaque handle for NvDdkSdio context.
 */
typedef struct NvDdkSdioInfoRec *NvDdkSdioDeviceHandle;

/**
 * Indicates the interface being used for the card,
 * i.e., number of data lines used for data transfer.
 */
typedef enum
{
    /** Only D0 is used for the data transferl */
    NvDdkSdioDataWidth_1Bit = 0,
    /** Data Lines D0-D3 used for data transfer. */
    NvDdkSdioDataWidth_4Bit,
    /** Data Lines D0-D7 used for data transfer. */
    NvDdkSdioDataWidth_8Bit,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioDataWidth_Force32 = 0x7FFFFFFF
} NvDdkSdioDataWidth;

/**
 * Indicates the uhs mode set for the controller and
 * the card.
 */
typedef enum
{
    /* Max frequency is 25MHz */
    NvDdkSdioUhsMode_SDR12 = 0,
    /* Max frequency is 50MHz */
    NvDdkSdioUhsMode_SDR25,
    /* Max frequency is 104MHz */
    NvDdkSdioUhsMode_SDR50,
    /* Max frequency is 208MHz */
    NvDdkSdioUhsMode_SDR104,
    /* Max frequency is 50MHz. 2 bits sampled for each clock cycle */
    NvDdkSdioUhsMode_DDR50,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioUhsMode_Force32 = 0x7FFFFFFF
} NvDdkSdioUhsMode;

/**
 * Defines constants that indicate types of command
 * for SDMMC/MMC cards.
 */
typedef enum
{
    /** Normal. */
    NvDdkSdioCommandType_Normal = 0,
    /** Suspend. */
    NvDdkSdioCommandType_Suspend,
    /** Resume. */
    NvDdkSdioCommandType_Resume,
    /** Abort data operations. */
    NvDdkSdioCommandType_Abort,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioCommandType_Force32 = 0x7FFFFFFF
} NvDdkSdioCommandType;

/**
 * Defines constants that indicate types of command
 * responses for SDMMC/MMC cards.
 */
typedef enum
{
    /// No response type.
    NvDdkSdioRespType_NoResp = 1,
    /// R1 response type.
    NvDdkSdioRespType_R1,
    /// R1b response type.
    NvDdkSdioRespType_R1b,
    /// R2 response type.
    NvDdkSdioRespType_R2,
    /// R3 response type.
    NvDdkSdioRespType_R3,
    /** Responses R4 and R5 are applicable only to SDIO devices. */
    NvDdkSdioRespType_R4,
    /** Responses R4 and R5 are applicable only to SDIO devices. */
    NvDdkSdioRespType_R5,
    /// R6 response type.
    NvDdkSdioRespType_R6,
    /// R7 response type.
    NvDdkSdioRespType_R7,
    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkSdioRespType_Force32 = 0x7FFFFFFF
} NvDdkSdioRespType;

/**
 * Defines constants that indicate the bus voltage for
 * SDMMC card.
 */
typedef enum
{
    /** Invalid. */
    NvDdkSdioSDBusVoltage_invalid = 0,
     /** 1.8 V. */
    NvDdkSdioSDBusVoltage_1_8 = 5,
     /** 3.0 V. */
    NvDdkSdioSDBusVoltage_3_0 = 6,
    /** 3.3 V. */
    NvDdkSdioSDBusVoltage_3_3 = 7,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioSDBusVoltage_Force32 = 0x7FFFFFFF
} NvDdkSdioSDBusVoltage;

/**
 * Defines constants that indicate status of the last sent
 * SD command/operation.
 */
typedef enum
{
    /** No error. */
    NvDdkSdioCommandStatus_None = 0,
    /** Error.  */
    NvDdkSdioCommandStatus_Error = 0x200,
    /** Card interrupt. */
    NvDdkSdioCommandStatus_Card = 0x100,
    /** Card removal. */
    NvDdkSdioCommandStatus_CardRemoval = 0x80,
    /** Card insertion. */
    NvDdkSdioCommandStatus_CardInsertion = 0x40,
    /** Buffer read ready. */
    NvDdkSdioCommandStatus_BufferReadReady = 0x20,
    /** Buffer write ready. */
    NvDdkSdioCommandStatus_BufferWriteReady = 0x10,
    /** DMA boundary detected.  */
    NvDdkSdioCommandStatus_DMA = 0x8,
    /** Block gap event. */
    NvDdkSdioCommandStatus_BlockGapEvent = 0x4,
    /** Transfer complete. */
    NvDdkSdioCommandStatus_TransferComplete = 0x2,
    /** Command complete. */
    NvDdkSdioCommandStatus_CommandComplete = 0x1,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioCommandStatus_Force32 = 0x7FFFFFFF
} NvDdkSdioControllerStatus;

/**
 * Defines constants that indicate error status information as a result
 * of the last sent SD command/operation.
 */
 typedef enum
{
    /** No error. */
    NvDdkSdioError_None = 0,
    /** Error interrupt. */
    NvDdkSdioError_Err = 0x8000,
    /** Command timeout error. */
    NvDdkSdioError_CommandTimeout = 0x10000,
    /** Command CRC error. */
    NvDdkSdioError_CommandCRC = 0x20000,
    /** Command end bit error. */
    NvDdkSdioError_CommandEndBit = 0x40000,
    /** Command index error. */
    NvDdkSdioError_CommandIndex  = 0x80000,
    /** Data timeout error. */
    NvDdkSdioError_DataTimeout = 0x100000,
    /** Data CRC error. */
    NvDdkSdioError_DataCRC = 0x200000,
    /** Data endbit error. */
    NvDdkSdioError_DataEndBit = 0x400000,
    /** Current limit error. */
    NvDdkSdioError_CurrentLimit = 0x800000,
    /** Auto CMD12 error. */
    NvDdkSdioError_AutoCMD12 = 0x1000000,
    /** ADMA error. */
    NvDdkSdioError_ADMA = 0x2000000,
    /** Target response error. */
    NvDdkSdioError_TargetResponse = 0x10000000,
    /** Ceata error. */
    NvDdkSdioError_Ceata = 0x20000000,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioError_Force32 = 0x7FFFFFFF
} NvDdkSdioErrorStatus;

/**
 * SD memory command contains the command-related information.
 * Will be passed from the upper layers using NvDdkSdio.
 */
 typedef struct
 {
   /** Command code to be used for the current operation. */
    NvU32 CommandCode;
    /** Command type (normal/resume/abort/suspend, etc.)*/
   NvDdkSdioCommandType CommandType;
   /** Indicates whether this is a data/non-data command request. */
    NvBool IsDataCommand;
   /**
    * Command argument: In case of memory cards, argument will consist
    * the card address and in case of SDIO cards the argument will consist of
    * the argument value to be sent along with the commands CMD52/CMD53.
    */
    NvU32 CmdArgument;
    /**
     * Command response: type of response expected to this command R1/R2/R3, etc.
     * A response type also indicates the length of the reponse. For example,
     * R1 type of response is 48 bits long.
     */
    NvDdkSdioRespType ResponseType;
    /**
     * Block size in bytes.
     */
    NvU32  BlockSize;
 } NvDdkSdioCommand;

/**
 * SDIO status contains the read/write information.
 * Will be passed from the upper layers using NvDdkSdio while read/write.
 */
 typedef struct NvDdkSdioStatusRec
 {
   /** Error information for the current read/write operation. It can contain
   * multiple values.
   */
    volatile NvDdkSdioErrorStatus    SDErrorStatus;
   /** Status Information for the current read/write operation. It can contain
   * multiple values.
   */
    volatile NvDdkSdioControllerStatus  SDControllerStatus;
 } NvDdkSdioStatus;

/**
 * Indicates the data block size.
 */
typedef enum
{
    NvDdkSdioBlockSize_512Bytes = 0,
    NvDdkSdioBlockSize_1024Bytes,
    NvDdkSdioBlockSize_2048Bytes,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioBlockSize_Force32 = 0x7FFFFFFF
} NvDdkSdioBlockSize;

/**
 * Indicates the timeout clock unit.
 */
typedef enum
{
    NvDdkSdioTimeoutClk_KHz= 0,
    NvDdkSdioTimeoutClk_MHz= 1,
    /** Ignore -- Forces compilers to make 32-bit enums. */
    NvDdkSdioTimeoutClkUnit_Force32 = 0x7FFFFFFF
} NvDdkSdioTimeoutClkUnit;


typedef enum
{
    NvDdkSdioClkDivider_DIV_BASE = 0x0,
    /// Base clock divided by 2.
    NvDdkSdioClkDivider_DIV_2 = 0x1,
    /// Base clock divided by 4.
    NvDdkSdioClkDivider_DIV_4 = 0x2,
    /// Base clock divided by 6.
    NvDdkSdioClkDivider_DIV_8 = 0x4,
    /// Base clock divided by 16.
    NvDdkSdioClkDivider_DIV_16 = 0x8,
    /// Base clock divided by 32.
    NvDdkSdioClkDivider_DIV_32 = 0x10,
    /// Base clock divided by 64.
    NvDdkSdioClkDivider_DIV_64 = 0x20,
    /// Base clock divided by 128.
    NvDdkSdioClkDivider_DIV_128 = 0x40,
    /// Base clock divided by 256.
    NvDdkSdioClkDivider_DIV_256 = 0x80
}NvDdkSdioClkDivider;

typedef enum
{
    NVDDK_SDIO_NORMAL_REQUEST = 0,
    NVDDK_SDIO_MEMORY_ABORT_REQUEST,
    NVDDK_SDIO_IO_ABORT_REQUEST,
    NVDDK_SDIO_SUSPEND_REQUEST,
    NVDDK_SDIO_RESUME_REQUEST
} SdioRequestType;

typedef enum
{
    HWINTERFACE_AP20 = 0,
    HWINTERFACE_T30,
    HWINTERFACE_T1xx,
} SdioHwInterface;

/**
 * SD host capabilities contains the information specific to the host controller
 * implementation. The host controller may implement these values as fixed or
 * loaded from flash memory during power on initialization.
 */
 typedef struct NvDdkSdioHostCapabilitiesRec
 {
    /// Holds the max instances supported.
    NvU8 MaxInstances;
    /// Holds the chip version
    SdioHwInterface HwInterface;
    /// Flag to indicate if the host supports Auto CMD 12.
    NvBool IsAutoCMD12Supported;
    /// Holds the bus voltage supported by the host controller.
    NvDdkSdioSDBusVoltage BusVoltage;
    /// Holds the maximum block size supported by the controller.
    NvU32 MaxBlockLength;
    /// Is 8-bit mode supported
    NvBool Is8bitmodeSupported;
    /// Flag to indicate if the host supports ADMA2.
    NvBool IsAdma2Supported;
    /// Flag to indicate if the host supports high speed.
    NvBool IsHighSpeedSupported;
    /// Flag to indicate whether the host supports DDR mode.
    NvBool IsSDR50modeSupported;
    NvBool IsDDR50modeSupported;
    NvBool IsSDR104modeSupported;
 }NvDdkSdioHostCapabilities;

 typedef struct NvDdkSdioInterfaceCapabilitiesRec
{
    ///  Maximum bus width supported by the physical interface.
    ///  Will be 1, 4, or 8 depending on the selected pin mux.
    NvU32 MmcInterfaceWidth;

    /// SDIO card HW settling time after reset, i.e. before reading the OCR.
    NvU32 SDIOCardSettlingDelayMSec;
} NvDdkSdioInterfaceCapabilities;

/// Gets log2 of a power of 2.
/// When argument \a Val is not a power of 2, returns log2 of the next
/// smaller power of 2.
NvU8 SdUtilGetLog2(NvU32 Val);

/**
 * Get the SD host controller and SD interface capabilities.
 *
 * This API fills the SDIO host controller capabilities and
 * SDIO interface capabilities.
 *
 * @param hSdio       Sdio device handle
 * @param pHostCap      A pointer to SDIO host controlelr capabilities struct.
 * @param pInterfaceCap    A pointer to SDIO interface capabilities struct.
 * @param instance        The particular SDIO instance.
 * @retval NvSuccess                 Success.
 */
NvError
NvDdkSdioGetCapabilities(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioHostCapabilities *pHostCap,
    NvDdkSdioInterfaceCapabilities *pInterfaceCap,
    NvU32 instance);

/**
 * Initializes the HW SD module.
 * Allocates the resources needed for the SD data transfer operations.
 * Allocates the device handle for subsequent use. This API can be called
 * multiple times, to open various instances of the SDIO controller. This API
 * associates an instance of the SDIO controller to an SDIO handle. After the
 * client acquires an SDIO handle, it can use it for all further operations.
 *
 * @see NvDdkSdioClose
 *
 * @pre This method must be called before any other NvDdkSdio APIs.
 *
 * @param  hDevice       The RM device handle.
 * @param phSdio         A pointer to the device handle
 * @param pNotificationSema  A pointer to the sempahore that will be used by
 *                              SDIO module to signal notifications to the
 *                              client.
 * @param Instance   The particular SDIO instance.
 * To the client, this covers command completion event, error event,
 * and card removal event notification to the client.
 *
 * @retval NvSuccess                          Open device successful.
 * @retval NvError_SdioInstanceTaken          The requested instance is
 *                                            already been taken.
 * @retval NvError_InsufficientMemory         Cannot allocate memory.
 */
NvError
NvDdkSdioOpen(
    NvRmDeviceHandle hDevice,
    NvDdkSdioDeviceHandle *phSdio,
    NvU8 Instance);

/**
 * Deinitializes the HW SD module.
 * Closes all the opened handles and release all the allocated resources for SD.
 * @see NvDdkSdioOpen
 * @param hSdio The SDIO device handle.
 */
void NvDdkSdioClose(NvDdkSdioDeviceHandle hSdio);

/**
 * Sends non-data commands to the card. If the command is an abort
 * command, DAT and CMD lines will be reset using software reset register.
 * This command returns after succesfully sending a command. This API
 * waits until command inhibit (DAT) and command inhibit (CMD) both become
 * 0 and then sends the current command.
 * @param hSdio             The device handle.
 * @param pCommand           A pointer to the ::NvDdkSdioCommand struct,
 *                             which contains information like command index,
 *                               response type, argument, etc.
 * @param SdioStatus         The SDIO status.
 * @retval NvSuccess                 Success.
 * @retval NvError_SdioDeviceNotMounted Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent   Card not present in slot.
 */
NvError
NvDdkSdioSendCommand(
        NvDdkSdioDeviceHandle hSdio,
        NvDdkSdioCommand *pCommand,
        NvU32* SdioStatus);

/**
 * Reads the response from SDMMC controller.
 *
 * @param hSdio             The device handle.
 * @param CommandNumber     Command number.
 * @param ResponseType      Type of response.
 * @param pResponse         A pointer to  the response buffer.
 *
 * @retval NvSuccess                 Success.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError
NvDdkSdioGetCommandResponse(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 CommandNumber,
    NvDdkSdioRespType ResponseType,
    NvU32 *pResponse);

/**
 * Reads the data from SD/SDIO card into \a pReadBuffer.
 *
 * @pre The SD module must be mounted using NvDdkSdioOpen() before
 * using this method.
 *
 * @param hSdio                 The device handle.
 * @param NumOfBytesToRead      Number of bytes to read from the card.
 * @param pReadBuffer           A pointer to the application buffer in which data
 *                                 is read.
 * @param pRWRequest            A pointer to SD RW request struct.
 * @param HWAutoCMD12Enable     Flag to indicate if auto CMD12 is to be used.
 *                                 This flag can be ignored for SDIO cards.
 * @param SdioStatus         The SDIO status.
 * @retval NvSuccess                 Identification successful.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError
NvDdkSdioRead(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToRead,
    void  *pReadBuffer,
    NvDdkSdioCommand *pRWRequest,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus);

/**
 * Used to write data from \a pWriteBuffer on SD/SDIO card.
 *
 * @pre The SD module needs to be mounted using NvDdkSdioOpen() before
 * using this method.
 *
 * @param hSdio                 The device handle.
 * @param NumOfBytesToWrite     Number of bytes to write.
 * @param pWriteBuffer          A pointer to the application buffer from which
 *                                 data is taken and written on card.
 * @param pRWCommand            A pointer to SD read/write command.
 * @param HWAutoCMD12Enable     Flag to indicate if auto CMD12 is to be used.
 *                                 This flag can be ignored for SDIO cards.
 * @param SdioStatus         The SDIO status.
 * @retval NvSuccess                 Identification successful.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen first().
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 * @retval NvError_SdioCardWriteProtected Card is write protected
 *                                  (for SD memory cards only).
 */
NvError
NvDdkSdioWrite(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 NumOfBytesToWrite,
    void *pWriteBuffer,
    NvDdkSdioCommand *pRWCommand,
    NvBool HWAutoCMD12Enable,
    NvU32* SdioStatus);

/**
 * Gets write protect status (for memory/combo cards only).
 * There is a write protect switch present for SD memory cards
 * or mini/micro SD card adapters. These switches are used to restrict
 * writes to the memory card. The upper layers using NvDdkSdio have to
 * use this function to see whether the memory card is write-protected.
 * @param hSdio            The device handle.
 * @param IsWriteprotected   A pointer to information whether the card is
 *                               write protected (NV_TRUE)
                                 or write enabled (NV_FALSE).
 * @retval NvSuccess                      Operation successful.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError
NvDdkSdioIsWriteProtected(
        NvDdkSdioDeviceHandle hSdio,
        NvBool *IsWriteprotected);


/**
 * Set the voltage level for the card
 * operations.
 * @param hSdio            The device handle.
 * @param Voltage          Enum for voltage.
 */
void
NvDdkSdioSetSDBusVoltage(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioSDBusVoltage Voltage);

/**
 * Enables high speed mode for SD high speed card
 * operations. Before setting this bit, the host driver shall check the high
 * speed support in the capabilities register. if this bit is set to 0
 * (default), the host controller outputs CMD line and DAT lines at the falling
 * edge of the SD clock (up to 25 MHz). If this bit is set to 1, the host
 * controller outputs CMD line and DAT lines at the rising edge of the SD clock
 * (up to 50 MHz).
 * @param hSdio            The device handle.
 * @retval NvSuccess                 Success.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError NvDdkSdioHighSpeedEnable(NvDdkSdioDeviceHandle hSdio);

/**
 * Disables high speed mode for SD high speed cards.
 * The host controller outputs CMD line and DAT lines at the falling
 * edge of the SD Clock (up to 25 MHz).
 *
 * @param hSdio            The device handle.
 * @retval NvSuccess                 Success.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError NvDdkSdioHighSpeedDisable(NvDdkSdioDeviceHandle hSdio);

/**
 * Set clock rate for the card. Upper layers using
 * NvDdkSdio have to use this function to set the clock rate
 * to be used for subsequent data transfer
 * operations.
 * @param hSdio            The device handle.
 * @param FrequencyKHz   SD Clock Frequency to be configured.
 * @param pConfiguredFrequencyKHz This field is updated with the
 * actual frequency configured by the driver..
 * @retval NvSuccess                 Success
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError
NvDdkSdioSetClockFrequency(
    NvDdkSdioDeviceHandle hSdio,
    NvRmFreqKHz FrequencyKHz,
    NvRmFreqKHz* pConfiguredFrequencyKHz);

/**
 * Sets host controller bus width for the subsequent data operations.
 * By default the bus width is set to 1-bit mode.
 * SD Memory card is set to 4-bit mode when the it has to go in the
 * data transfer mode for fast data transfer.
 * Low speed SDIO cards do not support high data rate operations
 * (4-bit data mode) and so card capability must
 * be checked to see if card is a low speed card before setting the bus
 * width for the card to 4 bits. If a card does not support 4-bit mode
 * and this API is used to set its bus width to 4 bit mode,
 * assert is encountered in debug mode.
 *
 *
 * @param hSdio              The device handle.
 * @param CardDataWidth      1 or 4 bit mode.
 * @retval NvSuccess                Success.
 * @retval NvError_SdioDeviceNotMounted   Module not mounted. Need to
 *                                  call NvDdkSdioOpen() first.
 * @retval NvError_SdioCardNotPresent     Card not present in slot.
 */
NvError
NvDdkSdioSetHostBusWidth(
    NvDdkSdioDeviceHandle hSdio,
    NvDdkSdioDataWidth CardDataWidth);

/**
 * Sets the uhs mode of the host controller.
 * This API should be called after the uhs mode is selected on the device.
 *
 * @param hSdio         The device handle.
 * @param uhsmode    The uhs mode to be set.
 *
 * @retval      NvSuccess Indicates the operation succeeded.
 * @retval      NvError_NotInitialized Indicates that the SDIO was not opened.
 * @retval      NvError_SdioControllerBusy Indicates the controller is busy
 */
NvError NvDdkSdioSetUhsmode(NvDdkSdioDeviceHandle hSdio, NvDdkSdioUhsMode Uhsmode);

/**
 * Checks the present state register to see if the card has been
 * actually inserted. If NV_TRUE is returned, then it is safe to apply SD clock
 * to the card.
 * @param hSdio                 The device handle.
 * @param IsCardInserted    A pointer to information whether the card is
 *                                          inserted (NV_TRUE)
                                            or removed (NV_FALSE).
 * @retval NvSuccess        Card detect feature present.
 * @retval NvError_SdioCardAlwaysPresent   Card is soldered on the board.
 * @retval NvError_SdioAutoDetectCard Auto detect the card by sending the commands.
 */
NvError
NvDdkSdioIsCardInserted(
        NvDdkSdioDeviceHandle hSdio,
        NvBool *IsCardInserted);

/**
 * Resets the host controller by setting the software reset
 * for all bit in the software reset register.
 * @param hSdio            The device handle.
 */
void NvDdkSdioResetController(NvDdkSdioDeviceHandle hSdio);

/**
 * Sets the blocksize of the host controller.
 * This API should be called after the blocksize is set on the SD device
 * using CMD16.
 *
 * @param hSdio                  The device handle.
 * @param Blocksize              The blocksize to be set in bytes.
 *
 * @retval  NvSuccess Indicates the operation succeeded.
 * @retval  NvError_NotInitialized Indicates that the SDIO was not opened.
 * @retval  NvError_SdioControllerBusy Indicates the controller is busy carrying out
 *                               read/write operation.
 */
NvError NvDdkSdioSetBlocksize(NvDdkSdioDeviceHandle hSdio, NvU32 Blocksize);

/**
 * Part of static power management, call this API to put the SDIO controller
 * into suspend state. This API is a mechanism for the client to augment OS power
 * management policy.
 *
 * The H/W context of the SDIO controller is saved. Clock is disabled and power
 * is also disabled to the controller.
 *
 *
 * @param hSdio             The SDIO device handle.
 * @param SwitchOffSDDevice      If NV_TRUE, switches off the voltage to the SD device.
 * (like sd card or wifi module). If NV_FALSE, does not switch off the voltage to the SD device.
 *
 */
void NvDdkSdioSuspend(NvDdkSdioDeviceHandle hSdio, NvBool SwitchOffSDDevice);


/**
 * Part of static power management, call this API to wake the SDIO controller
 * from suspend state. This API is a mechanism for the client to augment OS
 * power management policy.
 *
 * The H/W context of the SDIO controller is restored. Clock is enabled and power
 * is also enabled to the controller.
 *
 *
 * @param hSdio             The SDIO device handle.
 * @param SwitchOnSDDevice      If NV_TRUE, switches on the voltage to the SD device.
 * (like SD card or Wi-Fi module). If NV_FALSE, does not change the voltage to the SD device.
 *
 * @retval NvSuccess                 Success.
 */
NvError
NvDdkSdioResume(
    NvDdkSdioDeviceHandle hSdio,
    NvBool SwitchOnSDDevice);

/**
 * Aborts the SDIO memory or I/O operation.
 *
 * @param hSdio            The SDIO device handle.
 * @param RequestType      The type of abort request.
 * @param FunctionNumber   Function number of the SDIO module.
 */
 void
NvDdkSdioAbort(
    NvDdkSdioDeviceHandle hSdio,
    SdioRequestType RequestType,
    NvU32 FunctionNumber);

/**
 * Allocates the required resources, powers on the device, and
 * prepares the device for I/O operations.
 * Client gets a valid handle only if the device is found.
 * The same handle must be used for further operations.
 * The device can be opened by only one client at a time.
 *
 * @pre This method must be called once before using other
 *           NvDdkBlockDev APIs.
 *
 * @param Instance Instance of specific device.
 * @param MinorInstance Minor instance of specific device.
 * @param phBlockDev Returns pointer to device handle.
 *
 * @retval NvSuccess Device is present and ready for I/O operations.
 * @retval NvError_BadParameter One or more of the arguments provided
 * are incorrect.
 * @retval NvError_SdioCardNotPresent SD card is not present or unsupported.
 * @retval NvError_NotSupported The requested instance is not supported.
 * @retval NvError_SdioCommandFailed SD card is not responding.
 * @retval NvError_SdioInstanceTaken The requested instance is unavailable
 * or already in use.
 * @retval NvError_InsufficientMemory Cannot allocate memory.
 */
NvError
NvDdkSdBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError
NvDdkSdBlockDevInit(NvRmDeviceHandle hDevice);

void
NvDdkSdBlockDevDeinit(void);
void NvDdkSdioConfigureTapAndTrimValues(
    NvDdkSdioDeviceHandle hSdio,
    NvU32 TapValue,
    NvU32 TrimValue);

void T1xxSdioOpenHWInterface(NvDdkSdioDeviceHandle hSdio);

void T30SdioOpenHWInterface(NvDdkSdioDeviceHandle hSdio);

void AP20SdioOpenHWInterface(NvDdkSdioDeviceHandle hSdio);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

/** @} */

#endif  /* INCLUDED_NVDDKSDIO_H */

