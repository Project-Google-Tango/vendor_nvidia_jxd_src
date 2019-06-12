/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

/** @file
 *
 * @b Description: Contains the i2c declarations.
 */

#ifndef INCLUDED_NVRM_I2C_PRIVATE_H
#define INCLUDED_NVRM_I2C_PRIVATE_H

#include "nvrm_module.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_gpio.h"
#include "nvrm_i2c.h"
#include "nvrm_memmgr.h"
#include "nvrm_dma.h"
#include "nvrm_priv_ap_general.h"


#define MAX_I2C_CLOCK_SPEED_KHZ 400

// Maximum number of i2c instances including i2c and dvc and a dummy instance
#define MAX_I2C_INSTANCES   ((MAX_I2C_CONTROLLERS) + (MAX_DVC_CONTROLLERS) + 1)

/* Delay used while polling(in polling mode) for the transcation to complete */
#define I2C_DELAY_USEC 10000

typedef enum
{
    // Specifies a read transaction.
    I2C_READ,
    // Specifies a write transaction.
    I2C_WRITE,
    // Specifies a read transaction using i2c repeat start
    I2C_REPEAT_START_TRANSACTION
} I2cTransactionType;


/* I2C controller state. There are will one instance of this structure for each
 * I2C controller instance */
typedef struct NvRmI2cControllerRec
{
    /* Controller static Information */

    /* Rm device handle */
    NvRmDeviceHandle hRmDevice;
    /* Contains the i2ctransfer status */
    NvError I2cTransferStatus;
    /* Contains the number of opened clients */
    NvU32 NumberOfClientsOpened;
    /* Contains the semaphore id to block the synchronous i2c client calls */
    NvOsSemaphoreHandle I2cSyncSemaphore;
    /* Contains the mutex for providing the thread safety */
    NvOsMutexHandle I2cThreadSafetyMutex;
    /* Power clinet ID */
    NvU32 I2cPowerClientId;
    /* Contoller module ID. I2C is supported via DVS module and I2C module. */
    NvRmModuleID ModuleId;

    // Odm io module name
    NvOdmIoModule OdmIoModule;

    // Current handle is is in slave mode or not
    NvBool IsMasterMode;

    /* Instance of the above specified module */
    NvU32 Instance;

    // I2C interrupt handle for this controller instance
    NvOsInterruptHandle I2CInterruptHandle;

    // I2c Pin mux configuration
    NvU32 PinMapConfig;

    // Current pinmap passed from api
    NvU32 CurrPinMap;

    /* GPIO pin handles for SCL and SDA lines. Used by the GPIO fallback mode */
    NvRmGpioPinHandle hSclPin;
    NvRmGpioPinHandle hSdaPin;
    NvRmGpioHandle hGpio;

    /* Controller run time state. These members will be polulated before the HAL
     * functions are called. HAL functions should only read these members and
     * should not clobber these registers. */

    /* I2c clock freq */
    NvU32 clockfreq;
    /* Slave device address type */
    NvBool Is10BitAddress;
    /* Indictaes that the slave will not generate the ACK */
    NvBool NoACK;
    /* timeout for the transfer */
    NvU32 timeout;

    /* Receive data */
    NvError (*receive)(struct NvRmI2cControllerRec *c, NvU8 * pBuffer,
               const NvRmI2cTransactionInfo *pTransaction,
               NvU32 * pBytesTransferred);

    /* Send data */
    NvError (*send)(struct NvRmI2cControllerRec *c, NvU8 * pBuffer,
                const NvRmI2cTransactionInfo *pTransaction,
                NvU32 * pBytesTransferred);

    /* Repeat start - this is specific to the AP15 and will not be called for
     * later chips */
    NvError (*repeatStart)(struct NvRmI2cControllerRec *c, NvU8 * pBuffer,
                NvRmI2cTransactionInfo *Transactions, NvU32 NoOfTransations);

    /* Return the GPIO pin and port numbers of the SDA and SCL lines for that
     * controller. */
    NvBool (*GetGpioPins)(struct NvRmI2cControllerRec *c,
                NvU32 PinMap, NvU32 *Scl, NvU32 *Sda);

    /* Shutdown the controller */
    void (*close)(struct NvRmI2cControllerRec *c);
    NvError (*setDivisor)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvU32 FreqToController,
                NvU32 I2cBusClockFreq);

    NvError (*I2cSlaveStart)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvU32 Address, NvBool IsTenBitAdd, NvU32 maxRecvPacketSize,
                NvU8 DummyCharTx);

    NvU32 (*I2cSlaveStop)(struct NvRmI2cControllerRec *hRmI2cCont);

    NvError (*I2cSlaveRead)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvU8 *pRxBuffer, NvU32 BytesRequested, NvU32 *pBytesRead,
                NvU8 MinBytesRead, NvU32 Timeoutms);

    NvError (*I2cSlaveWriteAsynch)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvU8 *pTxBuffer, NvU32 BytesRequested, NvU32 *pBytesWritten);

    NvError (*I2cSlaveGetWriteStatus)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvU32 TimeoutMs, NvU32 *pBytesRemaining);

    void (*I2cSlaveFlushWriteBuffer)(struct NvRmI2cControllerRec *hRmI2cCont);

    NvU32 (*I2cSlaveGetNACKCount)(struct NvRmI2cControllerRec *hRmI2cCont,
                NvBool IsResetCount);

    /* Flag to know whether it is a read or a write transaction */
    I2cTransactionType TransactionType;

    /* Though all the controllers have same register spec, their start address
     * doesn't match. DVC contoller I2C register start address differs from the I2C
     * controller. */
    NvU32 I2cRegisterOffset;

    /* Repeat start transfer */
    volatile NvBool RsTransfer;

    /* Clock period in micro-seconds */
    NvU32 I2cClockPeriod;

    /* I2c Controller Status variable */
    NvU32 ControllerStatus;

    /* Indicates whether i2c slave supports packet mode.*/
    NvBool IsPacketModeI2cSlaveSupport;

    NvU32 *pDataBuffer;

    // Amount of word transferred yet
    NvU32 WordTransferred;

    // Remaining words to be transfer.
    NvU32 WordRemaining;

    // Final interrupt condition after that transaction completes.
    NvU32 FinalInterrupt;

    // Content of the interrupt mask register.
    NvU32 IntMaskReg;

    // Controller Id for packet mode information.
    NvU32 ControllerId;

    // Tells whether the current transfer is with NO STOP
    NvBool IsCurrentTransferNoStop;

    // Tells whether the current transfer is with ack or not
    NvBool IsCurrentTransferNoAck;

    // Tells whether current transfer is a read or write type.
    NvBool IsCurrentTransferRead;

    // Tells whether transfer is completed or not
    NvBool IsTransferCompleted;

    // Apb dma related information
    // Tells whether the dma mode is supported or not.
    NvBool IsApbDmaAllocated;

    // Dma handle for the read/write.
    NvRmDmaHandle hRmDma;

    // Memory handle to create the uncached memory.
    NvRmMemHandle hRmMemory;

    // Dma buffer physical address.
    NvRmPhysAddr DmaBuffPhysAdd;

    // Virtual pointer to the dma buffer.
    NvU32 *pDmaBuffer;

    // Current Dma transfer size for the Rx and tx
    NvU32 DmaBufferSize;

    // Dma request for read transaction
    NvRmDmaClientBuffer RxDmaReq;

    // Dma request for write transaction
    NvRmDmaClientBuffer TxDmaReq;

    // Tell whether it is using the apb dma for the transfer or not.
    NvBool IsUsingApbDma;

    // Buffer which will be used when cpu does the data receving.
    NvU32 *pCpuBuffer;

    NvU8 *pClientBuffer;

    NvU32 MaxClientBufferLen;

    NvU32 TransCountFromLastDmaUsage;

    // i2c slave specific member
    NvOsIntrMutexHandle hI2cSlvTransferMutex;

    // I2c slave read/write synchronisation element.
    /* Rx Syncing sema */
    NvOsSemaphoreHandle hI2cSlvRxSyncSema;
    NvBool IsReadWaiting;

    /* Tx Syncing sema */
    NvOsSemaphoreHandle hI2cSlvTxSyncSema;
    NvBool IsWriteWaiting;

    // Current i2c slave transaction type
    NvU32 CurrentI2cSlvTransferType;

    // I2c slave rx buffer fifo
    NvBool ReadFirstByteWait;
    NvU8 *pI2cSlvRxBuffer;
    NvU32 rRxIndex;
    NvU32 wRxIndex;

    // I2c slave tx buffer fifo
    NvU8 *pI2cSlvTxBuffer;
    NvU32 rTxIndex;
    NvU32 wTxIndex;

    // I2c slave receive packet headers.
    NvU32 I2cSlvRxPacketHeader1;
    NvU32 I2cSlvRxPacketHeader2;
    NvU32 I2cSlvRxPacketHeader3;

    // The number of master read cycle on which dummy charater sends.
    NvU32 I2cSlvNackCount;

    // Indicates that slave has started or not.
    NvBool IsI2cSlaveStarted;

    // I2c slave transmit specific parameter
    NvRmI2cTransactionInfo I2cSlvTxTransaction;

    // I2c slave specific receive parameter.
    NvU32 I2cSlvCurrRxPacketBytesRead;

    // I2c slave specific receive parameter.
    NvU32 I2cSlvCurrTxPacketrTxIndex;
    NvBool IsDummyCharCycle;

    // word to be sent if master initiates read and slave does not have data.
    NvU32 DummyWord;
} NvRmI2cController, *NvRmI2cControllerHandle;

NvError NvRmGpioI2cTransaction(
        NvRmI2cController *c,
        NvU32 I2cPinMap,
        NvU8 *Data,
        NvU32 DataLength,
        NvRmI2cTransactionInfo * Transaction,
        NvU32 NumOfTransactions);


/**
 * brief Initialze the controller to start the data transfer
 *
 * This APIs should always return NvSuccess
 *
 * @param c    I2C controller structure.
 * */
NvError AP20RmMasterI2cOpen(NvRmI2cController *c);

NvError T30RmSlaveI2cOpen(NvRmI2cController *c);

#endif  // INCLUDED_NVRM_I2C_PRIVATE_H
