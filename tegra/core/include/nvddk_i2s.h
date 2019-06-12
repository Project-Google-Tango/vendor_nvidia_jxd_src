/*
 * Copyright (c) 2007-2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * @brief <b>NVIDIA Driver Development Kit:
 *         I2S NvDDK APIs</b>
 *
 * @b Description: Interface file for NvDdk I2S driver.
 *
 */
#ifndef INCLUDED_NVDDK_I2S_H
#define INCLUDED_NVDDK_I2S_H

/**
 * @defgroup nvddk_i2s Inter IC Sound interface (I2S) Driver API
 *
 * This is the Inter IC Sound (I2S) driver interface. There may be more than
 * one I2S in the SOC, which can communicate with other audio codec system.
 * This interface provides the I2s data communication channel configuration,
 * basic data transfer (receive and transmit), receive and transmit data flow.
 * The read and write interface can be blocking/non blocking type based on the
 * passed parameter. The read and write request can be queued to make the data
 * receive/transmit continuation.
 *
 * @ingroup nvddk_modules
 * @{
 */

#include "nvcommon.h"
#include "nvos.h"
#include "nvrm_init.h"

#if defined(__cplusplus)
extern "C" {
#endif

/** An opaque context to the \c NvDdkI2sRec interface.
 */
typedef struct NvDdkI2sRec *NvDdkI2sHandle;

/**
 * @brief Defines the placing of valid bits in an I2S data word. I2S data word
 * is the length of NvDdkI2sDriverCapability::I2sDataWordSizeInBits. The valid
 * bits can start from LSb side or MSb side, or it can be in packed format.
 *
 */
typedef enum
{
    /// Valid bit starts from LSb in a given I2S data word.
    NvDdkI2SDataWordValidBits_StartFromLsb = 0x1,

    /// Valid bit starts from MSb in a given I2S data word.
    NvDdkI2SDataWordValidBits_StartFromMsb,

    /// The I2S data word contain the right and left channel of data in the
    /// packed format. The half of the data is for the left channel and half of
    /// the data is for right channel.
    /// The Valid bits in the I2S data word should be half of the I2S data word
    // for the packed format.
    NvDdkI2SDataWordValidBits_InPackedFormat,

    NvDdkI2SDataWordValidBits_Force32 = 0x7FFFFFFF
} NvDdkI2SDataWordValidBits;

/**
 * @brief Defines the channel type record or play.
 */
typedef enum
{
    /// Specifies a record channel.
    NvDdkI2sChannelType_In,

    /// Specifies a playback channel.
    NvDdkI2sChannelType_Out,

    /// Specifies playback and record on the same channel.
    NvDdkI2sChannelType_InOut,

    /// Ignore -- Forces compilers to make 32-bit enums.
    NvDdkI2sChannelType_Force32 = 0x7FFFFFFF

}NvDdkI2sChannelType;
/**
 * @brief Structure that defines the needed information regarding the channel
 * used I2S port. Currently, it has the physical address, virtual address and
 * index for the channel.
 */
typedef struct
{
    /// Holds the channel physical address.
    NvRmPhysAddr ChannelPhysAddress;

    /// Holds the channel virtual address.
    NvU32  ChannelVirtAddress;

    /// Holds the channel index.
    NvU32 ChannelIndex;

    /// HOlds the channel used for playback, or record, or both.
    NvDdkI2sChannelType ChannelType;

}NvDdkI2sChannelInfo;

/**
 * @brief Combines the buffer address, requested size, and the transfer status.
 *
 */
typedef struct
{
    /// Holds the physical address of the data buffer.
    NvRmPhysAddr BufferPhysicalAddress;

    /// Holds the number of bytes requested for transfer.
    NvU32 BytesRequested;

    /// Holds the number of bytes transferred yet.
    NvU32 BytesTransferred;

    /// Hold the information about the recent transfer status.
    NvError TransferStatus;
} NvDdkI2sClientBuffer;

/**
 * @brief Combines the capability and limitation of the I2S driver.
 */
typedef struct
{
    /// Holds the total number of I2S channels supported by the driver.
    NvU32 TotalNumberOfI2sChannel;

    /// Holds the I2s channel instance ID for which this capability structure
    /// contains the data.
    NvU32 I2sChannelId;

    /// Holds whether packed mode is supported or not, i.e., half of the data is
    /// for right channel and half of the data is for left channel. The I2S
    /// data word size will be of
    ///  NvDdkI2sDriverCapability::I2sDataWordSizeInBits.
    NvBool IsValidBitsInPackedModeSupported;

    /// Holds whether valid bits in the I2S data word start from MSb is
    /// supported or not.
    NvBool IsValidBitsStartsFromMsbSupported;

    /// Holds whether valid bits in the I2S data word start from LSb is
    /// supported or not.
    NvBool IsValidBitsStartsFromLsbSupported;

    /// Holds whether Mono data format is supported or not, i.e., single I2S
    /// data word which can go to both right and left channel.
    NvBool IsMonoDataFormatSupported;

    /// Holds whether loopback mode is supported or not. This can be used for
    /// self test.
    NvBool IsLoopbackSupported;

    /// Holds the information for the I2S data word size in bits. The client
    /// should passed the data buffer such that each I2S data word should match
    /// with this value.
    /// All passed physical address and bytes requested size should be aligned
    /// to this value.
    NvU32 I2sDataWordSizeInBits;
} NvDdkI2sDriverCapability;

/**
 * @brief Defines the various modes of operation supported by the I2S
 *    controller.
 */
typedef enum
{

    NvDdkI2SMode_PCM = 0x1,

    NvDdkI2SMode_Network,

    NvDdkI2SMode_TDM,

    NvDdkI2SMode_DSP,

    NvDdkI2SMode_LeftJustified,

    NvDdkI2SMode_RightJustified,

    NvDdkI2SMode_Force32 = 0x7FFFFFFF
} NvDdkI2SMode;

/**
 * @brief Defines the various Highz modes of operation supported by the I2S
 *    controller.
 */
typedef enum
{
    NvDdkI2SHighzMode_No_PosEdge = 0x0,

    NvDdkI2SHighzMode_PosEdge,

    NvDdkI2SHighzMode_No_NegEdge,

    NvDdkI2SHighzMode_NegEdge,

    NvDdkI2SHighzMode_Force32 = 0x7FFFFFFF
} NvDdkI2SHighzMode;

/**
 * @brief Defines the configuration for I2S PCM mode of operation
 * This is the default mode supported by the I2S driver.
 */
typedef struct
{
    /// Holds whether the data is mono data format or not.
    NvBool IsMonoDataFormat;

    /// Holds whether the valid data bits in the data buffer starts from
    /// the LSb or MSb or in packed format. If it is packed format
    /// (::NvDdkI2SDataWordValidBits_Packed) then half of the data of size
    /// \c DataBufferWidth (in caps structure) is for the left channel
    /// and half of the data of size \c DataBufferWidth (in caps structure) is
    /// for the right channel. In this case the \c I2sDataWIdth should be half
    /// of the \c DataBufferWidth.
    NvDdkI2SDataWordValidBits I2sDataWordFormat;

    /// Holds the valid bits in I2S data for one sample.
    /// If packed mode is selected then it should be half of the
    /// \c DataBufferWidth.
    /// If non-packed mode is selected then the valid data bits will be taken
    /// from the LSb side of data buffer or MSb side of the data buffer, based
    /// on ::I2sDataBufferFormat.
    NvU32 ValidBitsInI2sDataWord;

    /// Holds the sampling rate. Based on sampling rate it generates the clock
    /// and sends the data if it is in master mode. This parameter will be
    /// ignored when slave mode is selected.
    NvU32 SamplingRate;

    /// Holds the BCLK data bits per LRCLK when I2S is in Master mode
    /// This parameter is given from the codec.
    NvU32 DatabitsPerLRCLK;
} NvDdkI2sPcmDataProperty;

/**
 * @brief Defines the configuration for I2S DSP mode of operation
 * This is a new feature supported by v1.1 of the driver.
 */
typedef struct
{
    /// Holds the whether the valid data bits in data buffer start from
    /// LSb or MSb or in packed format. If is packed format
    /// (::NvDdkI2SDataWordValidBits_Packed) then half of the data of size
    /// \c DataBufferWidth (in caps structure) is for the left channel
    /// and half of the data of size \c DataBufferWidth (in caps structure) is
    /// for the right channel. In this case the \c I2sDataWIdth should be half
    /// of the \c DataBufferWidth.
    NvDdkI2SDataWordValidBits I2sDataWordFormat;

    /// Holds the valid bits in I2S data for one sample.
    /// If packed mode is selected then it should be half of the
    /// \c DataBufferWidth.
    /// If non-packed mode is selected then the valid data bits will be taken
    /// from the LSb side of data buffer or MSb side of the data buffer based
    /// on ::I2sDataBufferFormat.
    NvU32 ValidBitsInI2sDataWord;

    /// Holds the sampling rate. Based on sampling rate it generates the clock
    /// and sends the data if it is in master mode. This parameter will be
    /// ignored when slave mode is selected.
    NvU32 SamplingRate;
    /// Mask used for Tx;
    /// Max maskbit = 7.
    NvU32 TxMaskBits;
    /// Mask used for Rx;
    /// Max maskbit = 7.
    NvU32 RxMaskBits;
    /// Fsync width.
    /// 0 - Short (1bit clock wide)
    /// 1 - Long (2bit clock wide)
    NvU32 FsyncWidth;
    /// Holds Highz control.
    /// Edge on positive or negative Highz.
    NvDdkI2SHighzMode HighzCtrl;
    /// Holds the BCLK data bits per LRCLK when I2S is in master mode.
    /// This parameter is given from the codec.
    NvU32 DatabitsPerLRCLK;
}NvDdkI2sDspDataProperty;

/**
 * @brief Defines the configuration for I2S network mode of operation
 * This is a new feature supported by v1.1 of the driver.
 */
typedef struct
{
    /// Holds the whether the valid data bits in data buffer start from
    /// LSb or MSb or in packed format. If is packed format
    /// (::NvDdkI2SDataWordValidBits_Packed) then half of the data of size
    /// \c DataBufferWidth (in caps structure) is for the left channel
    /// and half of the data of size \c DataBufferWidth (in caps structure) is
    /// for the right channel. In this case the \c I2sDataWIdth should be half
    /// of the \c DataBufferWidth.
    NvDdkI2SDataWordValidBits I2sDataWordFormat;

    /// Holds the valid bits in I2s data for one sample.
    /// If packed mode is selected then it should be half of the
    /// \c DataBufferWidth.
    /// If non-packed mode is selected then the valid data bits will be taken
    /// from the LSb side of data buffer or MSb side of the data buffer based
    /// on ::I2sDataBufferFormat.
    NvU32 ValidBitsInI2sDataWord;

    /// Holds the sampling rate. Based on sampling rate it generates the clock
    /// and sends the data if it is in master mode. This parameter will be
    /// ignored when slave mode is selected.
    NvU32 SamplingRate;
    /// Mask used for slot selection for Tx;
    /// Max slots = 4, the rest of the slots will be ignored.
    NvU8 ActiveTxSlotSelectionMask;
    /// Mask used for slot selection for Rx;
    /// Max slots = 4, the rest of the slots will be ignored.
    NvU8 ActiveRxSlotSelectionMask;
    /// Holds the BCLK data bits per LRCLK when I2S is in master mode
    /// This parameter is given from the codec.
    NvU32 DatabitsPerLRCLK;
}NvDdkI2sNetworkDataProperty;

/**
 * @brief Defines the configuration for I2S TDM mode of operation
 * This is a new feature supported by v1.1 of the driver.
 */
typedef struct
{
    /// Holds whether the valid data bits in data buffer start from
    /// LSb or MSb or in packed format. If is packed format
    /// (NvDdkI2SDataWordValidBits_Packed) then half of the data of size
    /// \c DataBufferWidth (in caps structure) is for the left channel
    /// and half of the data of size \c DataBufferWidth (in caps structure) is
    /// for the right channel. In this case the \c I2sDataWIdth should be half
    /// of the \c DataBufferWidth.
    NvDdkI2SDataWordValidBits I2sDataWordFormat;

    /// Holds the valid bits in I2S data for one sample.
    /// If packed mode is selected then it should be half of the
    /// \c DataBufferWidth.
    /// If non-packed mode is selected then the valid data bits will be taken
    /// from the LSb side of data buffer or MSb side of the data buffer based
    /// on ::I2sDataBufferFormat.
    NvU32 ValidBitsInI2sDataWord;

    /// Holds the sampling rate. Based on sampling rate it generates the clock
    /// and sends the data if it is in master mode. This parameter will be
    /// ignored when slave mode is selected.
    NvU32 SamplingRate;
    /// Total bits per slot. It should be multiples of four - 1;
    /// Size can vary between 8 to 32 bits.
    NvU32 SlotSize;
    /// Number of active slots (ranging between 1 to 8).
    NvU8 NumberOfSlotsPerFSync;
    /// Whether the valid data bits in data buffer start from LSb or MSb ;
    /// This property can be set individualy for Rx and Tx.
    NvDdkI2SDataWordValidBits TxDataWordFormat;
    NvDdkI2SDataWordValidBits RxDataWordFormat;
    /// Holds the BCLK data bits per LRCLK when I2S is in master mode;
    /// This parameter is given from the codec.
    NvU32 DatabitsPerLRCLK;
}NvDdkI2sTdmDataProperty;


/**
 * @brief  Gets the I2S driver capabilities. Client can get the capabilities of
 * the driver to support the different functionalities and configure the data
 * communication.
 *
 * Client will allocate the memory for the capability structure and send the
 * pointer to this function. This function will copy the capabilities into
 * the capability structure pointed to by the passed pointer.
 *
 * Client can get the total number of I2S channel by passing the \a ChanenlId
 * as 0 (zero). If the client passed \a ChannelId as 0 with a valid pointer for
 * \a pI2sDriverCapabilities then this function will fill the \a TotalNumber Of
 * I2S channels in the corresponding member of the \a pI2sDriverCapabilities
 * structure.
 *
 * @sa NvDdkI2sDriverCapability
 *
 * @param hDevice Handle to the Rm device which is required by DDK to acquire
 * the capability from the device capability structure.
 * @param ChannelId Specifies a channel ID for which capabilities are required.
 * Pass 0 if the total number of I2S channels is required to be queryied. Valid
 * instance starts from the 0.
 * @param pI2sDriverCapabilities Specifies a pointer to a variable in which the
 * I2S driver capability will be stored.
 *
 * @retval NvSuccess Indicates the operation success succeeded.
 * @retval NvError_NotSupported Indicates that the channel ID is more than
 * supported channel ID.
 *
 */
NvError
NvDdkI2sGetCapabilities(
    NvRmDeviceHandle hDevice,
    NvU32 ChannelId,
    NvDdkI2sDriverCapability * const pI2sDriverCapabilities);

/**
 * @brief  Opens the I2S channel and returns a handle of the opened channel to
 * the client. This is specific to an I2S channel ID. This handle should be
 * passed to other APIs. This function initializes the controller, enables the
 * clock to the I2S controller, and puts the driver into determined state.
 *
 * It returns the same existing handle if it is already opened.
 *
 * @sa NvDdkI2sClose()
 *
 * @param hDevice Handle to the Rm device which is required by DDK to
 * acquire the resources from the Resource Manager.
 * @param I2sChannelId Specifies the I2S channel ID for which handle is
 * required. Valid channel ID starts from 0 for the I2S channel.
 * @param phI2s Specifies a pointer to a variable in which the handle is
 * stored.
 *
 * @retval NvSuccess Indicates the controller successfully initialized.
 * @retval NvError_InsufficientMemory Indicates that function failed to allocate
 * the memory for handle.
 * @retval NvError_BadValue Indicates that the channel ID is not valid. It may
 * be more than supported channel ID.
 * @retval NvError_MemoryMapFailed Indicates that the memory mapping for
 * controller register failed.
 * @retval NvError_MutexCreateFailed Indicates that the creation of mutex
 * failed. Mutex is required to provide the thread safety.
 * @retval NvError_SemaphoreCreateFailed Indicates that the creation of
 * semaphore failed. Semaphore is required to provide the synchronous
 * operation.
 */
NvError
NvDdkI2sOpen(
    NvRmDeviceHandle hDevice,
    NvU32 I2sChannelId,
    NvDdkI2sHandle *phI2s);

/**
 * Deinitializes the I2S channel, disables the clock, and releases the I2S
 * handle. It also releases all the OS resources which were allocated when
 * creating the handle.
 *
 * @note The number of calls to \c close() should be same as number to \c open()
 * to completely release the resources. If the number of calls to \c close()
 * is not equal to the number of calls to \c open()
 * then it will not relase the allocated resources.
 *
 * @sa NvDdkI2sOpen()
 *
 * @param hI2s The I2S handle from \c NvDdkI2sOpen. If \a hI2s is NULL, this API
 *     has no effect.
 */
void NvDdkI2sClose(NvDdkI2sHandle hI2s);

/**
 * @brief  Gets the default or current I2S data configuration for a specified
 *    mode.
 * @sa NvDdkI2sOpen, NvDdkI2sSetDataProperty
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param I2sMode The I2S operating mode (PCM/NW/TDM/DSP).
 * @param pI2SDataProperty A pointer to the I2S data property structure where
 * the current configured data property will be stored.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 * @retval NvError_InvalidState Indicates the I2S is busy and currently it is
 * transmitting/receiving the data.
 *
 */
NvError
NvDdkI2sGetDataProperty(
    NvDdkI2sHandle hI2s,
    NvDdkI2SMode I2sMode,
    void * const pI2SDataProperty);

/**
 * @brief  Sets the I2S data property for a particular I2S mode. This configures
 * the driver to send/receive the data as per client requirement.
 * The client is advised first to call \c NvDdkI2sGetDataProperty to get
 * the default value of configration and change only those members that
 * need to be configured, and then call this function.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sGetDataProperty, NvDdkI2sGetDataProperty
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param I2sMode The I2S operating mode  (PSM/NW/TDM).
 * @param pI2SDataProperty A pointer to the I2S data property structure that
 * has the data property parameters.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 * @retval NvError_InvalidState Indicates the I2S is busy and currently it is
 * transmitting/receiving the data.
 *
 */
NvError
NvDdkI2sSetDataProperty(
    NvDdkI2sHandle hI2s,
    NvDdkI2SMode I2sMode,
    const void *pI2SDataProperty);

/**
 * @brief  Starts the read operation and stores the received data in the buffer
 * provided. This is blocking type/non blocking type call based on the argument
 * passed. If no wait is selected then notification (through signalling the
 * semaphore) will be generated to the caller after the data transfer
 * completes.
 *
 * The read request will be queued, means the client can call the read() even
 * if the previous request is not completed.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sReadAbort, NvDdkI2sGetAsynchReadTransferInfo
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param ReceiveBufferPhyAddress Physical address of the receive buffer where
 * the received data will be stored. The address should be alligned to
 * NvDdkI2sDriverCapability::I2sDataWordSizeInBits. The data is directly
 * filled in the physical memory pointing to this address.
 * @param pBytesRead A pointer to the variable that stores the number of bytes
 * requested to read when it is called and stores the actual number of bytes
 * read when it returns from the function.
 * @param WaitTimeoutInMilliSeconds The time need to wait in milliseconds. If
 * it is zero then it will be returned immediately as asynchronous operation.
 * If is non zero then it will wait for a requested timeout. If it is
 * ::NV_WAIT_INFINITE then it will wait for infinitely until transaction
 * completes.
 * @param AsynchSemaphoreId The semaphore ID that must be signalled if client
 * is requested for asynchronous operation. If it is NULL then it will not
 * notify to the client and the next request will be continued from the request
 * queue.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_Timeout Indicates the operation is not completed in a given
 * timeout.
 * @retval NvError_I2SOverrun Indicates that overrun error occured while
 * receiving of the data.
 */
NvError
NvDdkI2sRead(
    NvDdkI2sHandle hI2s,
    NvRmPhysAddr ReceiveBufferPhyAddress,
    NvU32 *pBytesRead,
    NvU32 WaitTimeoutInMilliSeconds,
    NvOsSemaphoreHandle AsynchSemaphoreId);

/**
 * @brief  Aborts the I2S read operation.
 *
 * This routine stop the ongoing data operation and kills all read requests
 * related to this handle.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sRead, NvDdkI2sGetAsynchReadTransferInfo
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 *
 */
void NvDdkI2sReadAbort(NvDdkI2sHandle hI2s);

/**
 * @brief Gets the receiving information that happened recently. This
 * function can be called if the client wants the asynchronous operation and
 * after getting the signal of the semaphore from DDK, he wants to know the
 * receiving status and other information. The client need to pass the pointer
 * to the client buffer where the last received buffer information will be
 * copied.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sRead, NvDdkI2sReadAbort
 *
 * @note This function does not allocate the memory to store the last buffer
 * information. So the client must allocate the memory and pass the pointer so
 * that this function can copy the information.
 *
 * @param hI2s Handle to the I2S context which is allocated from \c GetContext.
 * @param pI2SReceivedBufferInfo A pointer to the structure variable where
 * the recent receiving status regarding the buffer will be stored.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_InvalidState Indicates that there is no read call made by
 * the client.
 */

NvError
NvDdkI2sGetAsynchReadTransferInfo(
    NvDdkI2sHandle hI2s,
    NvDdkI2sClientBuffer *pI2SReceivedBufferInfo);

/**
 * @brief  Starts transmitting the data that is available at the buffer
 * provided.
 * This is blocking type/non blocking type call based on the argument passed.
 * If zero timeout is selected then notification will be generated to the
 * caller after it completes the data transmission.
 * If non-zero timeout is selected then it will wait maximum for a given
 * timeout for data transfer completion. It can also wait for forever based on
 * the argument passed.
 *
 * The write operation can be called even if previous requests have not been
 * completed. It will queue the request.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWriteAbort, NvDdkI2sWritePause,
 * NvDdkI2sWriteResume, NvDdkI2sGetAsynchWriteTransferInfo
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param TransmitBufferPhyAddress Physical address of the transmit buffer
 * where transmitted data are stored. The address should be alligned to
 * NvDdkI2sDriverCapability::I2sDataWordSizeInBits. The data is directly
 * taken from the physical memory pointing to this address.
 * @param pBytesWritten A pointer to the variable that stores the number of
 * bytes requested to write when it is called and stores the actual number of
 * bytes write when it returns from the function.
 * @param WaitTimeoutInMilliSeconds The time to wait in milliseconds. If
 * it is zero then it will be returned immediately as an asynchronous operation.
 * If is non-zero then it will wait for a requested timeout. If it is
 * ::NV_WAIT_INFINITE then it will wait for infinitely until the transaction
 * completes.
 * @param AsynchSemaphoreId The semaphore ID that must be signalled if client
 * is requested for asynchronous operation.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_Timeout Indicates the operation is not completed in a given
 * timeout.
 */

NvError
NvDdkI2sWrite(
    NvDdkI2sHandle hI2s,
    NvRmPhysAddr TransmitBufferPhyAddress,
    NvU32 *pBytesWritten,
    NvU32 WaitTimeoutInMilliSeconds,
    NvOsSemaphoreHandle AsynchSemaphoreId);

/**
 * @brief  Aborts the I2S write.
 * This routine stops transmitting the data, and kills all requests for
 * data sends.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWrite, NvDdkI2sWritePause, NvDdkI2sWriteResume,
 * NvDdkI2sGetAsynchWriteTransferInfo
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 *
 */
void NvDdkI2sWriteAbort(NvDdkI2sHandle hI2s);

/**
 * @brief Gets the transmitting information that happened recently. This
 * function can be called if the client wants the asynchronous operation and
 * after getting the signal of the semaphore from DDK, he wants to know the
 * transmit status and other information. The client need to pass the pointer
 * to the client buffer where the last received buffer information will be
 * copied.
 *
 * @note This function does not allocate the memory to store the last buffer
 * information. So the client must allocate the memory and pass the pointer so
 * that this function can copy the information.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWriteAbort, NvDdkI2sWritePause, NvDdkI2sWriteResume
 *
 * @param hI2s Handle to the I2S context that is allocated from \c GetContext.
 * @param pI2SSentBufferInfo A pointer to the structure variable where the
 * recent transmit information regarding the buffer will be stored.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_InvalidState Indicates that there is no write call made by
 * client or write operation was aborted.
 */

NvError
NvDdkI2sGetAsynchWriteTransferInfo(
    NvDdkI2sHandle hI2s,
    NvDdkI2sClientBuffer *pI2SSentBufferInfo);

/**
 * @brief  Pauses the I2S write. The data transmission will be paused and
 * all state will be saved. The user can resume to continue sending the data.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWrite, NvDdkI2sWriteAbort, NvDdkI2sWriteResume
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_InvalidState Indicates that there is no write call made by
 * client or write operation was aborted.
 */
NvError NvDdkI2sWritePause(NvDdkI2sHandle hI2s);

/**
 * @brief  Resumes the I2S write. The data transmission will be continued from
 * paused state. This should be called after calling the NvDdkI2sWritePause() to
 * resume the transmission.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWrite, NvDdkI2sWriteAbort, NvDdkI2sWritePause
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 * @retval NvError_InvalidState Indicates that there is no write call made by
 * client or write operation was aborted.
 */
NvError NvDdkI2sWriteResume(NvDdkI2sHandle hI2s);

/**
 * Enables/disables the loopback mode for the I2S channel.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param IsEnable Tells whether the loop back mode is enabled or not. If it
 * is \c NV_TRUE then the loop back mode is enabled.
 * @retval NvSuccess Indicates the operation succeeded.
 */
NvError NvDdkI2sSetLoopbackMode(NvDdkI2sHandle hI2s, NvBool IsEnable);

/**
 * Sets the continuous double buffering mode of playback for the audio stream.
 * it will inform the client when first half of the buffer is tranferred by
 * signalling the semaphore. It continues the transmitting of the second half
 * of the buffer. Once this part of buffer is transmitted, it signals the
 * another semaphore and continues the transmitting of first part of the buffer
 * and repeats this operation until it stops.
 *
 * @sa NvDdkI2sOpen, NvDdkI2sWrite
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param IsEnable Tells whether the continuous mode is enabled or not. If it
 * is \c NV_TRUE then the same buffer will be continuously sent.
 * @param TransmitBufferPhyAddress The physical address of the the transmit
 * buffer.
 * @param TransferSize The total transfer size. The semaphores will be
 * signalled once half of the transfer is completed.
 * @param AsynchSemaphoreId1 Client-created semaphore ID that will be
 * signalled when first half of the buffer is transmitted.
 * @param AsynchSemaphoreId2 Client-created semaphore ID that will be
 * signalled when second half of the buffer is transmitted.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotInitialized Indicates that the I2S channel is not opened.
 */
NvError
NvDdkI2sSetContinuousDoubleBufferingMode(
    NvDdkI2sHandle hI2s,
    NvBool IsEnable,
    NvRmPhysAddr TransmitBufferPhyAddress,
    NvU32 TransferSize,
    NvOsSemaphoreHandle AsynchSemaphoreId1,
    NvOsSemaphoreHandle AsynchSemaphoreId2);


/**
 * @brief Power-mode suspends the I2S controller.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * NvDdkI2sOpen().
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates that requested functionality is not
 * supported.
 */
NvError NvDdkI2sSuspend(NvDdkI2sHandle hI2s);

/**
 * @brief Power-mode resumes the I2S controller. This resumes the controller
 * from the suspend states.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * NvDdkI2sOpen().
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates that requested functionality is not
 * supported.
 */
NvError NvDdkI2sResume(NvDdkI2sHandle hI2s);

/**
 * @brief Gets the DMA transfer count value.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param IsWrite  Boolean indicating whether we need write or read DMA transfer
 * count.
 * @param pTransferCount  A pointer to the returned DMA transfer count value.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates that requested functionality is not
 * supported.
 */

NvError NvDdkI2sGetTransferCount(
    NvDdkI2sHandle hI2s,
    NvBool IsWrite,
    NvU32 *pTransferCount);

/**
 * @brief Obtains the FIFO count value.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param IsWrite  Boolean that states whether we need FIFO1 or FIFO2 count.
 * @param pFifoCount  Returns variable for the FIFO count value from
 * the NvDdkI2sGetFifoCount().
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates that requested functionality is not
 * supported.
 */

NvError NvDdkI2sGetFifoCount(
    NvDdkI2sHandle hI2s,
    NvBool IsWrite,
    NvU32 *pFifoCount);

/**
 * @brief Obtains the I2S handle based on the \a InstanceId.
 *
 * @param phI2s A pointer to the I2S handle.
 * @param InstanceId Specifies the I2S channel ID for which handle is
 * required.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates that requested \a InstanceId is not
 *    valid.
 */

NvError NvDdkI2sGetHandle(NvDdkI2sHandle *phI2s, NvU32 InstanceId);

/**
 * @brief  Gets the I2S interface property.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param pI2SInterfaceProperty A pointer to the I2s interface property
 *  structure, where the current configured data property will be stored.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 *
 */
NvError
NvDdkI2sGetInterfaceProperty(
    NvDdkI2sHandle hI2s,
    void *pI2SInterfaceProperty);

/**
 * @brief  Sets the I2S interface property. This configures the driver to
 * send/receive the interface information as per client requirement.
 * The client is advised first to call NvDdkI2sGetInterfaceProperty() to get
 * the default value of all the members and change only those members that
 * need to be configured, and then call this function.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * \c NvDdkI2sOpen.
 * @param pI2SInterfaceProperty A pointer to the I2s interface property
 * structure that has the interface property parameters.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 *
 */
NvError
NvDdkI2sSetInterfaceProperty(
    NvDdkI2sHandle hI2s,
    void *pI2SInterfaceProperty);

/**
 * @brief  Sets the APBIF channel information. This configures the APBIF channel
 * virtual and physical address. This information is used by the I2S driver to
 * set the source for APDMA, getting the DMA FIFO count and other status.
 * This function is called from the audio hub that controls all the H/W wiring.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * NvDdkI2sOpen().
 * @param pApbifChannelInfo A pointer to the APBIF channel information structure.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 */
NvError
NvDdkI2sSetApbifChannelInterface(
    NvDdkI2sHandle hI2s,
    void *pApbifChannelInfo);

/**
 * @brief  Sets or gets the I2S ACIF register. This function helps to read
 * and write to the I2S ACIF register. A hub driver calls this function after
 * setting the right values based on the device property in the connection table.
 *
 * @param hI2s The I2S handle that is allocated by the driver after calling
 * NvDdkI2sOpen().
 * @param IsReceive Specifies the variable to receive or transmit the data.
 * @param IsRead Specifies whether or not the operation is a read operation.
 * @param pData A pointer passed in for read or write.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates the requested property is not
 * supported.
 *
 */
NvError
NvDdkI2sSetAcifRegister(
    NvDdkI2sHandle hI2s,
    NvBool IsReceive,
    NvBool IsRead,
    NvU32  *pData);

/** @} */

#if defined(__cplusplus)
}
#endif

#endif  // INCLUDED_NVDDK_I2S_H
