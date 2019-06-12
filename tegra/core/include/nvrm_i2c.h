/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_nvrm_i2c_H
#define INCLUDED_nvrm_i2c_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "nvrm_pinmux.h"
#include "nvrm_module.h"
#include "nvrm_init.h"
#include "nvos.h"
#include "nvcommon.h"

/**
 * NvRmI2cHandle is an opaque handle to the NvRmI2cStructRec interface
 */
typedef struct NvRmI2cRec *NvRmI2cHandle;

/**
 * @brief Defines the I2C capability structure. It contains the
 * capabilities/limitations (like maximum bytes transferred,
 * supported clock speed) of the hardware.
 */
typedef struct NvRmI2cCapabilitiesRec
{
    /**
     * Maximum number of packet length in bytes which can be transferred
     * between start and the stop pulses.
     */
    NvU32 MaximumPacketLengthInBytes;

    /// Maximum speed which I2C controller can support.
    NvU32 MaximumClockSpeed;

    /// Minimum speed which I2C controller can support.
    NvU32 MinimumClockSpeed;
} NvRmI2cCapabilities;

/**
 * @brief Initializes and opens the i2c channel. This function allocates the
 * handle for the i2c channel and provides it to the client.
 *
 * Assert encountered in debug mode if passed parameter is invalid.
 *
 * @param hDevice Handle to the Rm device which is required by Rm to acquire
 * the resources from RM.
 * @param IoModule The IO module to set, it is either NvOdmIoModule_I2c
 * or NvOdmIoModule_I2c_Pmu
 * @param instance Instance of the i2c driver to be opened.
 * @param phI2c Points to the location where the I2C handle shall be stored.
 *
 * @retval NvSuccess Indicates that the I2c channel has successfully opened.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate
 * the memory.
 * @retval NvError_NotInitialized Indicates the I2c initialization failed.
 */
NvError NvRmI2cOpen(
    NvRmDeviceHandle hDevice,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle * phI2c);

/**
 * @brief Closes the i2c channel. This function frees the memory allocated for
 * the i2c handle for the i2c channel.
 * This function de-initializes the i2c channel. This API never fails.
 *
 * @param hI2c A handle from NvRmI2cOpen().  If hI2c is NULL, this API does
 *     nothing.
 */
void NvRmI2cClose(NvRmI2cHandle hI2c);

// Maximum number of bytes that can be sent between the i2c start and stop conditions
#define NVRM_I2C_PACKETSIZE (8)

// Maximum number of bytes that can be sent between the i2c start and repeat start condition.
#define NVRM_I2C_PACKETSIZE_WITH_NOSTOP (4)

/// Indicates a I2C read transaction.
#define NVRM_I2C_READ (0x1)

/// Indicates that it is a write transaction
#define NVRM_I2C_WRITE (0x2)

/// Indicates that there is no STOP following this transaction. This also implies
/// that there is always one more transaction following a transaction with
/// NVRM_I2C_NOSTOP attribute.
#define NVRM_I2C_NOSTOP (0x4)

//  Some devices doesn't support ACK. By, setting this flag, master will not
//  expect the generation of ACK from the device.
#define NVRM_I2C_NOACK (0x8)

// Software I2C using GPIO. Doesn't use the hardware controllers. This path
// should be used only for testing.
#define NVRM_I2C_SOFTWARE_CONTROLLER (0x10)

typedef struct NvRmI2cTransactionInfoRec
{
    /// Flags to indicate the transaction details, like write/read or read
    /// without a stop or write without a stop.
    NvU32 Flags;

    /// Number of bytes to be transferred.
    NvU32 NumBytes;

    /// I2C slave device address
    NvU32 Address;

    /// Indicates that the address is a 10-bit address.
    NvBool Is10BitAddress;
} NvRmI2cTransactionInfo;

/**
 * @brief Does multiple I2C transactions. Each transaction can be a read or write.
 *
 *  AP15 I2C controller has the following limitations:
 *  - Any read/write transaction is limited to NVRM_I2C_PACKETSIZE
 *  - All transactions will be terminated by STOP unless NVRM_I2C_NOSTOP flag
 *  is specified. Specifying NVRM_I2C_NOSTOP means, *next* transaction will start
 *  with a repeat start, with NO stop between transactions.
 *  - When NVRM_I2C_NOSTOP is specified for a transaction -
 *      1. Next transaction will start with repeat start.
 *      2. Next transaction is mandatory.
 *      3. Next Next transaction cannot have NVRM_I2C_NOSTOP flag set. i.e no
 *         back to back repeat starts.
 *      4. Current and next transactions are limited to size
 *         NVRM_I2C_PACKETSIZE_WITH_NOSTOP.
 *      5. Finally, current transactions and next Transaction should be of same
 *         size.
 *
 *  This imposes some limitations on how the hardware can be used. However, the
 *  API itself doesn't have any limitations. If the HW cannot be used, it falls
 *  back to GPIO based I2C. Gpio I2C bypasses Hw controller and bit bangs the
 *  SDA/SCL lines of I2C.
 *
 * @param hI2c Handle to the I2C channel.
 * @param I2cPinMap for I2C controllers which are being multiplexed across
 *        multiple pin mux configurations, this specifies which pin mux configuration
 *        should be used for the transaction.  Must be 0 when the ODM pin mux query
 *        specifies a non-multiplexed configuration for the controller.
 * @param WaitTimeoutInMilliSeconds Timeout for the transcation.
 * @param ClockSpeedKHz Clock speed in KHz.
 * @param Data Continous stream of data
 * @param DataLength Length of the data stream
 * @param Transcations Pointer to the NvRmI2cTransactionInfo structure
 * @param NumOfTransactions Number of transcations
 *
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_NotSupported Indicates assumption on parameter values violated.
 * @retval NvError_InvalidState Indicates that the last read call is not
 * completed.
 * @retval NvError_ControllerBusy Indicates controller is presently busy with an
 * i2c transaction.
 * @retval NvError_InvalidDeviceAddress Indicates that the slave device address
 * is invalid
 */
NvError NvRmI2cTransaction(
    NvRmI2cHandle hI2c,
    NvU32 I2cPinMap,
    NvU32 WaitTimeoutInMilliSeconds,
    NvU32 ClockSpeedKHz,
    NvU8 * Data,
    NvU32 DataLen,
    NvRmI2cTransactionInfo * Transaction,
    NvU32 NumOfTransactions);

/**
 * @brief Initializes and opens the i2c channel in slave mode. This function
 * allocates the handle for the i2c channel and provides it to the client.
 * Client can do only i2c slave mode transaction with this handle.
 * The i2c channel works in either master mode or in slave mode. It does
 * not work in both mode at a time. So if channel is already open in
 * master mode then this api will fail.
 * If channel is already open in slave mode then the allocation fails. This
 * means only one client can use the i2c channel in slave mode.
 * Assert encountered in debug mode if passed parameter is invalid.
 *
 * @param hDevice Handle to the Rm device which is required by Rm to acquire
 * the resources from RM.
 * @param IoModule The IO module to set, it is either NvOdmIoModule_I2c
 * or NvOdmIoModule_I2c_Pmu.
 * @param instance Instance of the i2c driver to be opened.
 * @param phI2c Points to the location where the I2C handle shall be stored.
 *
 * @retval NvSuccess Indicates that the I2c channel has successfully opened.
 * @retval NvError_InsufficientMemory Indicates that function fails to allocate
 * the memory.
 * @retval NvError_NotInitialized Indicates the I2c initialization failed.
 * @retval NvError_AlreadyAllocated Indicates the I2c channel is already open
 * by some other client.
 */
NvError NvRmI2cSlaveOpen(
    NvRmDeviceHandle hDevice,
    NvU32 IoModule,
    NvU32 instance,
    NvRmI2cHandle * phI2c);

/**
 * @brief Start the i2c slave so that it can receive/send data from/to master
 * when master initiates the transfer.
 * After calling this function, slave responds to master on read/write cycle.
 * If the slave is not started and master initiates the transfer, master will
 * get NACK error.
 *
 * @param hI2c Handle to the I2C channel.
 * @param I2cPinMap for I2C controllers which are being multiplexed across
 *        multiple pin mux configurations, this specifies which pin mux
 *        configuration should be used for the transaction.  Must be 0 when
 *        the ODM pin mux query specifies a non-multiplexed configuration
 *        for the controller.
 * @param ClockSpeedKHz Clock speed in KHz.
 * @param Address Slave address. AP will get configured with this address and
 *         does communication with master if master start transfer with this
 *         address.
 * @param IsTenBitAdd Tells whether AP is configured in 10 bit address mode or
 *         not.
 * @param MaxRecvPacketSize maximum size of the receive packet. Master should
 *        not send any packet more than this size.
 * @param TxDummyChar The data which is sent to master if master start read
 *        cycle and slave does not have any data to send. The NACK count
 *        increment with every dummy data sent to master.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_I2cSlaveAlreadyStarted Indicates the slave is already started
 */
NvError NvRmI2cSlaveStart(
    NvRmI2cHandle hI2c,
    NvU32 I2cPinMap,
    NvU32 ClockSpeedKHz,
    NvU32 Address,
    NvBool IsTenBitAdd,
    NvU32 MaxRecvPacketSize,
    NvU8 TxDummyChar);

/**
 * @brief Stop the i2c slave to do any more transaction.
 * If any transaction is already in progress, it will wait till transfer
 * completes. After calling this api, the slave will not ack to master.
 *
 * @param hI2c Handle to the I2C channel.
 */
void NvRmI2cSlaveStop(
    NvRmI2cHandle hI2c);

/**
 * @brief Read data from slave i2c. Slave i2c driver keep buffering of incoming
 * data from master into driver's local buffer.
 * Once clients want to read data, the driver copies the receive data into
 * client's buffer. If data is not available then this call will block/unblock
 * based on parameter passed.
 * The blocking behavior of this api will depends on two parameter, MinBytesRead
 * and TimeoutMS.
 * TimeoutMS = 0:
 *         MinBytesRead = 0, Driver will copy the available data to client
 *                           buffer. Client will not wait till requested amount
 *                           of data available.
 *         MinBytesRead != 0, Client will wait till "MinBytesRead" of data
 *                            available. If data is already in local buffer,
 *                            driver will copy the data to client buffer and it
 *                            will not block the client.If data is not available
 *                            then it will block till MinBytesRead of data read.
 * TimeoutMS is non-zero but not NV_WAIT_INFINITE:
 *         The parameter MinBytesRead will not be used in this case.
 *         The client will be block till requested amount of data read or
 *         timeout happen, whatever is first.
 *
 *TimeoutMS is NV_WAIT_INFINITE:
 *         Wait infinitely till requested amount of data arrive.
 *
 * @param hI2c Handle to the I2C channel.
 * @param pRxBuffer The client buffer on which received data will be copied.
 * @param BytesRequested Number of bytes requested by client.
 * @param BytesRead Pointer to variable on which the amount of data read will
 *        be returned.
 * @param MinBytesRead Minimum amount of data which need to be read. It is used
 *        with combination of parameter TimeoutMS. Refer above description.
 * @param TimeoutMS The timeout to block the client if data is not available.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_I2cSlaveNotStarted Indicates the slave is not started.
 * @retval NvError_Timeout Indicates that data is not available in given time.
 */
NvError NvRmI2cSlaveRead(
    NvRmI2cHandle hI2c,
    NvU8 * pRxBuffer,
    NvU32 BytesRequested,
    NvU32 * pBytesRead,
    NvU8 MinBytesRead,
    NvU32 TimeoutMS);

/**
 * @brief Push the data into driver's transmit buffer so that it can send to
 * master once master initiates the transfer.
 * This will just copy the data and return to client. It will not wait for
 * data to actually transfer to master.
 *
 * @param hI2c Handle to the I2C channel.
 * @param pTxBuffer The client buffer on which transmitted data are available.
 * @param BytesRequested Number of bytes requested by client to transmit.
 * @param BytesWritten Pointer to a variable on which the amount of data written
 * into transmit buffer will be returned.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_I2cSlaveNotStarted Indicates the slave is not started.
 * @retval NvError_I2cSlaveTxBufferFull Indicates that driver's transmit buffer
 *                 is full and the is no space to write new data.
 **/
NvError NvRmI2cSlaveWriteAsynch(
    NvRmI2cHandle hI2c,
    NvU8 * pTxBuffer,
    NvU32 BytesRequested,
    NvU32 * pBytesWritten);

/**
 * @brief Wait for the driver to send all transmit data to the master.
 * Data will be sent to master if master initiates read cycle.
 *
 * @param hI2c Handle to the I2C channel.
 * @param TimeoutMS The wait time in milliseconds to wait for write to be
 *                  complete.
 * @param BytesRemaining Pointer to varaible which stores number of bytes
 *                  remaining in the write buffer.
 *
 * @retval NvSuccess Indicates the operation succeeded.
 * @retval NvError_I2cSlaveNotStarted Indicates the slave is not started.
 * @retval NvError_Timeout Indicates that driver has not send data on given
 *                 time.
 */
NvError NvRmI2cSlaveGetWriteStatus(
    NvRmI2cHandle hI2c,
    NvU32 TimeoutMS,
    NvU32 * BytesRemaining);

/**
 * @brief Flushing write buffer.
 *
 * @param hI2c Handle to the I2C channel.
 *
 */
void NvRmI2cSlaveFlushWriteBuffer(
    NvRmI2cHandle hI2c);

/**
 * @brief Get number of dummy character sent to master.
 * When master initiates a read cycle and if slave does not have any data
 * to send then it sends the dummy character and increment the NACK count.
 *
 * @param hI2c Handle to the I2C channel.
 * @param IsResetCount If NV_TRUE then reset the NACK count in the driver.
 *
 * @retval Number of dummy character send to master from last reset.
 */
NvU32 NvRmI2cSlaveGetNACKCount(
    NvRmI2cHandle hI2c,
    NvBool IsResetCount);

#if defined(__cplusplus)
}
#endif

#endif
