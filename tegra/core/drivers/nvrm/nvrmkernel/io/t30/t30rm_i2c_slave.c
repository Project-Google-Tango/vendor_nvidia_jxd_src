/*
 * Copyright (c) 2010-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** @file
 * @brief <b>NVIDIA Driver Development Kit: I2C API</b>
 *
 * @b Description: Contains the NvRM I2C slave implementation for T30
 */

#include "nvrm_i2c.h"
#include "nvrm_i2c_private.h"
#include "nvrm_drf.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "t30/ari2c.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_pinmux.h"
#include "nvrm_hwintf.h"

/* Register access Macros */
#define I2C_REGR(c, reg) \
    NV_REGR((c)->hRmDevice, NVRM_MODULE_ID( (c)->ModuleId, (c)->Instance ), \
                        I2C_##reg##_0 )

#define I2C_REGW(c, reg, val) \
    do {    \
        NV_REGW((c)->hRmDevice, NVRM_MODULE_ID((c)->ModuleId, (c)->Instance ),\
                        (I2C_##reg##_0), (val) ); \
    } while(0)

#define DEBUG_ENABLE 0

#if DEBUG_ENABLE
#define DEBUG_PRINT(Expr, Format) \
    do { \
        if (Expr) \
        {   \
            NvOsDebugPrintf Format; \
        }   \
    } while(0)
#else
#define DEBUG_PRINT(Expr, Format)
#endif

/* Transfer state of the i2c slave */
#define TRANSFER_STATE_NONE  0
#define TRANSFER_STATE_READ  1
#define TRANSFER_STATE_WRITE 2

/* local rx and tx buffer to queue the incoming data and outgoing data */
#define MAX_RX_BUFF_SIZE 4096
#define MAX_TX_BUFF_SIZE 4096

#define SPACE_AVAILABLE(rInd,wInd,maxsize) \
            (((wInd > rInd)?(maxsize-wInd+rInd):(rInd-wInd)) -1)

#define DATA_AVAILABLE(rInd,wInd,maxsize) \
            ((wInd >= rInd)?(wInd-rInd):(maxsize - rInd + wInd -1))

#define I2C_FIFO_DEPTH 8
#define I2C_SLV_TRANSACTION_STATUS_ERRORS \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_TFIFO_OVF, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_RFIFO_UNF, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PKT_XFER_ERR, SET))

#define I2C_SLV_TRANSACTION_STATUS_FIFO_ERRORS \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_TFIFO_OVF, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_RFIFO_UNF, SET))

#define I2C_SLV_TRANSACTION_END_ERRORS \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PKT_XFER_ERR, SET))

#define I2C_SLV_TRANSACTION_PREMATURE_END \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PKT_XFER_ERR, SET))

#define I2C_SLV_TRANSACTION_ALL_XFER_END \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PACKET_XFER_COMPLETE, SET))

#define I2C_SLV_TRANSACTION_END \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PKT_XFER_ERR, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_PACKET_XFER_COMPLETE, SET))

#define I2C_INT_STATUS_RX_BUFFER_FILLED \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_RX_BUFFER_FILLED, SET))

#define I2C_INT_STATUS_RX_DATA_AVAILABLE \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_RX_BUFFER_FILLED, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_RFIFO_DATA_REQ, SET))

#define I2C_INT_STATUS_TX_BUFFER_REQUEST \
    (NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_TX_BUFFER_REQ, SET) | \
     NV_DRF_DEF(I2C, INTERRUPT_STATUS_REGISTER, SLV_TFIFO_DATA_REQ, SET))

#define I2C_SLV_ERRORS_INTERRUPT_MASK \
    (NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_TFIFO_OVF_REQ_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_RFIFO_UNF_REQ_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_PKT_XFER_ERR_INT_EN, ENABLE))

#define I2C_SLV_DEFAULT_INTERRUPT_MASK \
    (NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_TFIFO_OVF_REQ_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_RFIFO_UNF_REQ_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_PKT_XFER_ERR_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_RX_BUFFER_FILLED_INT_EN, ENABLE) | \
     NV_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER, SLV_TX_BUFFER_REQ_INT_EN, ENABLE))

// Convert the number of bytes to word.
#define BYTES_TO_WORD(ReqSize) (((ReqSize) + 3) >> 2)

static void
SetI2cSlaveTxFifoTriggerLevel(
    NvRmI2cControllerHandle hRmI2cCont,
    NvU32 TrigLevel)
{
    NvU32 FifoControlReg;
    NvU32 ActualTriggerLevel = NV_MIN(I2C_FIFO_DEPTH, TrigLevel);

    if (!ActualTriggerLevel)
        return;

    FifoControlReg = I2C_REGR (hRmI2cCont, FIFO_CONTROL);
    FifoControlReg = NV_FLD_SET_DRF_NUM(I2C, FIFO_CONTROL, SLV_TX_FIFO_TRIG,
                                                ActualTriggerLevel - 1, FifoControlReg);
    I2C_REGW (hRmI2cCont, FIFO_CONTROL, FifoControlReg);
}

static void
SetI2cSlaveRxFifoTriggerLevel(
    NvRmI2cControllerHandle hRmI2cCont,
    NvU32 TrigLevel)
{
    NvU32 FifoControlReg;
    NvU32 ActualTriggerLevel = NV_MIN(I2C_FIFO_DEPTH, TrigLevel);

    if (!ActualTriggerLevel)
        return;

    FifoControlReg = I2C_REGR (hRmI2cCont, FIFO_CONTROL);
    FifoControlReg = NV_FLD_SET_DRF_NUM(I2C, FIFO_CONTROL, SLV_RX_FIFO_TRIG,
                                                ActualTriggerLevel - 1, FifoControlReg);
    I2C_REGW (hRmI2cCont, FIFO_CONTROL, FifoControlReg);
}

static void ResetI2cSlaveTxFifo(NvRmI2cControllerHandle hRmI2cCont)
{
    NvU32 FifoControlReg;
    NvU32 TimeoutCount = 1000;

    FifoControlReg = I2C_REGR (hRmI2cCont, FIFO_CONTROL);
    FifoControlReg = NV_FLD_SET_DRF_DEF(I2C, FIFO_CONTROL, SLV_TX_FIFO_FLUSH,
                                                SET, FifoControlReg);
    I2C_REGW (hRmI2cCont, FIFO_CONTROL, FifoControlReg);
    while(TimeoutCount--)
    {
        FifoControlReg = I2C_REGR (hRmI2cCont, FIFO_CONTROL);
        if (!(FifoControlReg & NV_DRF_DEF(I2C, FIFO_CONTROL, SLV_TX_FIFO_FLUSH, SET)))
            break;
        NvOsWaitUS(1);
    }
    if (!TimeoutCount)
        NV_ASSERT(0);
}

static void
GetI2cSlavePacketHeaders(
    NvRmI2cControllerHandle hRmI2cCont,
    NvRmI2cTransactionInfo *pTransaction,
    NvU32 PacketId,
    NvU32 *pPacketHeader1,
    NvU32 *pPacketHeader2,
    NvU32 *pPacketHeader3)
{
    NvU32 PacketHeader1;
    NvU32 PacketHeader2;
    NvU32 PacketHeader3;

    // prepare Generic header1
    // Header size = 0 Protocol  = I2C,pktType = 0
    PacketHeader1 = NV_DRF_DEF(I2C, IO_PACKET_HEADER, HDRSZ, ONE) |
                            NV_DRF_DEF(I2C, IO_PACKET_HEADER, PROTOCOL, I2C);

    // Set pkt id as 1
    PacketHeader1 = NV_FLD_SET_DRF_NUM(I2C, IO_PACKET_HEADER, PKTID, PacketId,
                            PacketHeader1);

    // Controller id is according to the instance of the i2c/dvc
    PacketHeader1 = NV_FLD_SET_DRF_NUM(I2C, IO_PACKET_HEADER,
                            CONTROLLER_ID, hRmI2cCont->ControllerId,
                            PacketHeader1);

    PacketHeader2 = NV_FLD_SET_DRF_NUM(I2C, IO_PACKET_HEADER,
                            PAYLOADSIZE, (pTransaction->NumBytes - 1), 0);

    // prepare IO specific header
    // Configure the slave address
    PacketHeader3 = pTransaction->Address;

    // 10 bit address mode: Set address mode to 10 bit
    if (pTransaction->Is10BitAddress)
        PacketHeader3 = NV_FLD_SET_DRF_DEF(I2C, IO_PACKET_HEADER, ADDR_MODE,
                            TEN_BIT, PacketHeader3);

    // Enable Read if it is required
    if (pTransaction->Flags & NVRM_I2C_READ)
        PacketHeader3 = NV_FLD_SET_DRF_DEF(I2C, IO_PACKET_HEADER, READ, READ,
                                                PacketHeader3);
    *pPacketHeader1 = PacketHeader1;
    *pPacketHeader2 = PacketHeader2;
    *pPacketHeader3 = PacketHeader3;
}

static void ConfigureI2cSlavePacketMode(NvRmI2cControllerHandle hRmI2cCont)
{
    NvU32 I2cConfig;

    // PACKET_MODE_TRANSFER_EN field of I2C Controller configuration Register
    I2cConfig = NV_DRF_DEF(I2C, I2C_CNFG, NEW_MASTER_FSM, ENABLE);
    I2cConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_CNFG, PACKET_MODE_EN, GO, I2cConfig);
    I2cConfig = NV_FLD_SET_DRF_NUM(I2C, I2C_CNFG, DEBOUNCE_CNT, 2, I2cConfig);
    I2C_REGW(hRmI2cCont, I2C_CNFG, I2cConfig);
}

static void
ConfigureI2cSlaveAddress(
    NvRmI2cControllerHandle hRmI2cCont,
    NvU32 Address,
    NvBool IsTenBit)
{
    NvU32 SlaveAddReg;
    NvU32 I2cSlaveConfig;
    NvU32 SlaveAdd;

    if (IsTenBit)
    {
        SlaveAdd = Address & 0xFF;
        SlaveAddReg = I2C_REGR(hRmI2cCont, I2C_SL_ADDR1);
        SlaveAddReg = NV_FLD_SET_DRF_NUM(I2C, I2C_SL_ADDR1, SL_ADDR0, SlaveAdd, SlaveAddReg);
        I2C_REGW(hRmI2cCont, I2C_SL_ADDR1, SlaveAddReg);

        SlaveAdd = (Address >> 8) & 0x3;
        SlaveAddReg = I2C_REGR(hRmI2cCont, I2C_SL_ADDR2);
        SlaveAddReg = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_ADDR2, VLD, TEN_BIT_ADDR_MODE, SlaveAddReg);
        SlaveAddReg = NV_FLD_SET_DRF_NUM(I2C, I2C_SL_ADDR2, SL_ADDR_HI, SlaveAdd, SlaveAddReg);
        I2C_REGW(hRmI2cCont, I2C_SL_ADDR2, SlaveAddReg);
    }
    else
    {
        SlaveAdd = (Address >> 1) & 0x3FF;
        SlaveAddReg = I2C_REGR(hRmI2cCont, I2C_SL_ADDR1);
        SlaveAddReg = NV_FLD_SET_DRF_NUM(I2C, I2C_SL_ADDR1, SL_ADDR0, SlaveAdd, SlaveAddReg);
        I2C_REGW(hRmI2cCont, I2C_SL_ADDR1, SlaveAddReg);

        SlaveAddReg = I2C_REGR(hRmI2cCont, I2C_SL_ADDR2);
        SlaveAddReg = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_ADDR2, VLD, SEVEN_BIT_ADDR_MODE, SlaveAddReg);
        I2C_REGW(hRmI2cCont, I2C_SL_ADDR2, SlaveAddReg);
    }

    if (!Address)
    {
        I2cSlaveConfig = 0;
        I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG, NEWSL, ENABLE, I2cSlaveConfig);
        I2C_REGW(hRmI2cCont, I2C_SL_CNFG, I2cSlaveConfig);
        return;
    }

    I2cSlaveConfig = 0;
    I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG, NEWSL, ENABLE, I2cSlaveConfig);
    I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG, ENABLE_SL, ENABLE, I2cSlaveConfig);
    I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG, PKT_MODE_EN, ENABLE, I2cSlaveConfig);
    I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG, FIFO_XFER_EN, ENABLE, I2cSlaveConfig);
    I2C_REGW(hRmI2cCont, I2C_SL_CNFG, I2cSlaveConfig);
}

static void
DoI2cSlaveTxFifoEmpty(
    NvRmI2cControllerHandle hRmI2cCont,
    NvU32 *pFifoEmptyCount)
{
    NvU32 EmptyCount = 0;
    NvU32 FifoStatus;

    // Tx Fifo should be empty. If not force to make it empty
    FifoStatus = I2C_REGR(hRmI2cCont, FIFO_STATUS);
    EmptyCount = NV_DRF_VAL(I2C, FIFO_STATUS, SLV_TX_FIFO_EMPTY_CNT, FifoStatus);
    if (EmptyCount < I2C_FIFO_DEPTH)
        ResetI2cSlaveTxFifo(hRmI2cCont);

    *pFifoEmptyCount = EmptyCount;
}

static void copyRxData(NvRmI2cControllerHandle hRmI2cCont, NvU8 rxChar)
{
    if (SPACE_AVAILABLE(hRmI2cCont->rRxIndex, hRmI2cCont->wRxIndex, MAX_RX_BUFF_SIZE))
    {
        hRmI2cCont->pI2cSlvRxBuffer[hRmI2cCont->wRxIndex++] = rxChar;
        if (hRmI2cCont->wRxIndex == MAX_RX_BUFF_SIZE)
            hRmI2cCont->wRxIndex = 0;
    }
    else
    {
        NvOsDebugPrintf("I2c slave rx buffer filled \n");
    }
}

static void WriteIntMaksReg(NvRmI2cControllerHandle hRmI2cCont)
{
    I2C_REGW (hRmI2cCont, INTERRUPT_MASK_REGISTER, hRmI2cCont->IntMaskReg);
}

static void HandleI2cSlaveReadInt(NvRmI2cControllerHandle hRmI2cCont)
{
    NvU32 FifoStatus;
    NvU32 i, j;
    NvU32 FilledSlots;
    NvU32 I2cSlaveConfig;
    NvU32 rxData;
    NvU32 currPacketBytes;

    FifoStatus = I2C_REGR(hRmI2cCont, FIFO_STATUS);
    FilledSlots = NV_DRF_VAL(I2C, FIFO_STATUS, SLV_RX_FIFO_FULL_CNT,
                                    FifoStatus);
    // Get updated status of the interrupt status
    hRmI2cCont->ControllerStatus = I2C_REGR(hRmI2cCont, INTERRUPT_STATUS_REGISTER);
    if (hRmI2cCont->ReadFirstByteWait)
    {
        I2C_REGW(hRmI2cCont, INTERRUPT_STATUS_REGISTER,
                                      I2C_INT_STATUS_RX_DATA_AVAILABLE);
        NV_ASSERT(FilledSlots == 1);
        rxData = I2C_REGR(hRmI2cCont, I2C_SLV_RX_FIFO);
        copyRxData(hRmI2cCont, (NvU8)rxData);
        hRmI2cCont->ReadFirstByteWait = NV_FALSE;
        hRmI2cCont->CurrentI2cSlvTransferType = TRANSFER_STATE_READ;
        hRmI2cCont->I2cSlvCurrRxPacketBytesRead = 0;

        // Write  packet Header
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO,
                                    hRmI2cCont->I2cSlvRxPacketHeader1);
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO,
                                    hRmI2cCont->I2cSlvRxPacketHeader2);
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO,
                                    hRmI2cCont->I2cSlvRxPacketHeader3);

        SetI2cSlaveRxFifoTriggerLevel(hRmI2cCont, 4);
        hRmI2cCont->IntMaskReg = NV_FLD_SET_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER,
                                    SLV_RFIFO_DATA_REQ_INT_EN, ENABLE,
                                    hRmI2cCont->IntMaskReg);
        WriteIntMaksReg(hRmI2cCont);

        /* Ack the master */
        I2cSlaveConfig = I2C_REGR(hRmI2cCont, I2C_SL_CNFG);
        I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG,
                                    ACK_LAST_BYTE, ENABLE, I2cSlaveConfig);
        I2cSlaveConfig = NV_FLD_SET_DRF_DEF(I2C, I2C_SL_CNFG,
                                    ACK_LAST_BYTE_VALID, ENABLE, I2cSlaveConfig);
        I2C_REGW(hRmI2cCont, I2C_SL_CNFG, I2cSlaveConfig);
    }
    else
    {
        currPacketBytes = 4*FilledSlots;
        if (hRmI2cCont->ControllerStatus & I2C_SLV_TRANSACTION_PREMATURE_END)
        {
            FifoStatus = I2C_REGR(hRmI2cCont, FIFO_STATUS);
            FilledSlots = NV_DRF_VAL(I2C, FIFO_STATUS, SLV_RX_FIFO_FULL_CNT,
                                    FifoStatus);
            hRmI2cCont->CurrentI2cSlvTransferType = 0;
            currPacketBytes = I2C_REGR(hRmI2cCont, I2C_SLV_PACKET_STATUS);
            currPacketBytes = NV_DRF_VAL(I2C, I2C_SLV_PACKET_STATUS,
                                        TRANSFER_BYTENUM, currPacketBytes);
            NV_ASSERT(FilledSlots == ((currPacketBytes -
                            hRmI2cCont->I2cSlvCurrRxPacketBytesRead +3) >> 2));
            currPacketBytes -= hRmI2cCont->I2cSlvCurrRxPacketBytesRead;
        }
        hRmI2cCont->I2cSlvCurrRxPacketBytesRead += currPacketBytes;
        for (i = 0; i< FilledSlots; ++i)
        {
            rxData = I2C_REGR(hRmI2cCont, I2C_SLV_RX_FIFO);
            for (j = 0; j < 4; ++j)
            {
                copyRxData(hRmI2cCont, (NvU8)(rxData >> j*8));
                currPacketBytes--;
                if (!currPacketBytes)
                    break;
            }
        }
        if (hRmI2cCont->ControllerStatus & I2C_SLV_TRANSACTION_PREMATURE_END)
        {
            I2C_REGW(hRmI2cCont, INTERRUPT_STATUS_REGISTER,
                                   (I2C_SLV_TRANSACTION_END |
                                         I2C_INT_STATUS_RX_BUFFER_FILLED));
            hRmI2cCont->ReadFirstByteWait = NV_TRUE;
            hRmI2cCont->CurrentI2cSlvTransferType = TRANSFER_STATE_NONE;
            hRmI2cCont->I2cSlvCurrRxPacketBytesRead = 0;
            SetI2cSlaveRxFifoTriggerLevel(hRmI2cCont, 1);
            I2C_REGW(hRmI2cCont, I2C_SL_INT_MASK, 0);
            hRmI2cCont->IntMaskReg = I2C_SLV_DEFAULT_INTERRUPT_MASK;
            WriteIntMaksReg(hRmI2cCont);
        }
    }
    if (hRmI2cCont->IsReadWaiting)
    {
        NvOsSemaphoreSignal(hRmI2cCont->hI2cSlvRxSyncSema);
        hRmI2cCont->IsReadWaiting = NV_FALSE;
    }
}

static void HandleI2cSlaveWriteInt(NvRmI2cControllerHandle hRmI2cCont)
{
    NvU32 FifoStatus;
    NvU32 EmptySlots;
    NvU32 i, j;
    NvU32 currPacketBytes;
    NvU32 DataAvailable;
    NvU32 header1, header2, header3;
    NvU32 txData;
    NvU32 WordToBeWrite;
    NvU32 BytesRemain;
    NvU32 BytesInCurrWord;
    NvU32 rTxIndex;

    if (hRmI2cCont->ControllerStatus & I2C_SLV_TRANSACTION_END)
    {
        hRmI2cCont->CurrentI2cSlvTransferType = TRANSFER_STATE_NONE;
        currPacketBytes = I2C_REGR(hRmI2cCont, I2C_SLV_PACKET_STATUS);
        currPacketBytes = NV_DRF_VAL(I2C, I2C_SLV_PACKET_STATUS,
                                    TRANSFER_BYTENUM, currPacketBytes);

        // Get transfer count from request size.
        if ((currPacketBytes == 0) &&
                  (hRmI2cCont->ControllerStatus & I2C_SLV_TRANSACTION_ALL_XFER_END) &&
                  (!(hRmI2cCont->ControllerStatus & I2C_SLV_TRANSACTION_PREMATURE_END)))
        {
            if (!hRmI2cCont->IsDummyCharCycle)
                hRmI2cCont->rTxIndex = hRmI2cCont->I2cSlvCurrTxPacketrTxIndex;
        }
        else
        {
            if (!hRmI2cCont->IsDummyCharCycle)
            {
                hRmI2cCont->rTxIndex += currPacketBytes;
                if (hRmI2cCont->rTxIndex >= MAX_TX_BUFF_SIZE)
                    hRmI2cCont->rTxIndex -= MAX_TX_BUFF_SIZE;
            }
        }
        I2C_REGW(hRmI2cCont, INTERRUPT_STATUS_REGISTER, I2C_SLV_TRANSACTION_END);

        hRmI2cCont->CurrentI2cSlvTransferType = TRANSFER_STATE_NONE;
        SetI2cSlaveTxFifoTriggerLevel(hRmI2cCont, 1);
        I2C_REGW(hRmI2cCont, I2C_SL_INT_MASK, 0);
        hRmI2cCont->IntMaskReg = I2C_SLV_DEFAULT_INTERRUPT_MASK;
        WriteIntMaksReg(hRmI2cCont);
        if (hRmI2cCont->IsWriteWaiting)
        {
            NvOsSemaphoreSignal(hRmI2cCont->hI2cSlvTxSyncSema);
            hRmI2cCont->IsWriteWaiting = NV_FALSE;
        }
        return;
    }

    FifoStatus = I2C_REGR(hRmI2cCont, FIFO_STATUS);
    EmptySlots = NV_DRF_VAL(I2C, FIFO_STATUS, SLV_TX_FIFO_EMPTY_CNT, FifoStatus);
    NV_ASSERT(EmptySlots > 3);
    if (hRmI2cCont->CurrentI2cSlvTransferType == TRANSFER_STATE_NONE)
    {
        // Clear the tfifo request.
        I2C_REGW(hRmI2cCont, INTERRUPT_STATUS_REGISTER,
                                I2C_INT_STATUS_TX_BUFFER_REQUEST);

        // Get Number of bytes it can transfer in current
        DataAvailable = DATA_AVAILABLE(hRmI2cCont->rTxIndex,
                                hRmI2cCont->wTxIndex, MAX_TX_BUFF_SIZE);
        if (DataAvailable)
            hRmI2cCont->I2cSlvTxTransaction.NumBytes =
                                       NV_MIN((EmptySlots -3)*4, DataAvailable);
        else
            hRmI2cCont->I2cSlvTxTransaction.NumBytes = (EmptySlots -3)*4;

        GetI2cSlavePacketHeaders(hRmI2cCont, &hRmI2cCont->I2cSlvTxTransaction,
                                    2,  &header1, &header2, &header3);
        // Write  packet Header
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO, header1);
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO, header2);
        I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO, header3);
        FifoStatus = I2C_REGR(hRmI2cCont, FIFO_STATUS);
        EmptySlots -= 3;
        if (DataAvailable)
        {
            WordToBeWrite = (hRmI2cCont->I2cSlvTxTransaction.NumBytes + 3) >> 2;
            BytesRemain = hRmI2cCont->I2cSlvTxTransaction.NumBytes;
            rTxIndex = hRmI2cCont->rTxIndex;
            for (i =0; i < WordToBeWrite; i++)
            {
                BytesInCurrWord = NV_MIN(BytesRemain, 4);
                txData = 0;
                for (j=0; j < BytesInCurrWord; ++j)
                {
                    txData |= (hRmI2cCont->pI2cSlvTxBuffer[rTxIndex++] << (j*8));
                    if (rTxIndex >= MAX_TX_BUFF_SIZE)
                        rTxIndex = 0;
                }
                I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO, txData);
                BytesRemain -= BytesInCurrWord;
            }
            hRmI2cCont->I2cSlvCurrTxPacketrTxIndex = rTxIndex;
            hRmI2cCont->IsDummyCharCycle = NV_FALSE;
        }
        else
        {
            hRmI2cCont->I2cSlvCurrTxPacketrTxIndex = hRmI2cCont->rTxIndex;
            for (i =0; i < EmptySlots; i++)
                I2C_REGW(hRmI2cCont, I2C_SLV_TX_PACKET_FIFO, hRmI2cCont->DummyWord);
            hRmI2cCont->IsDummyCharCycle = NV_TRUE;
        }
        hRmI2cCont->CurrentI2cSlvTransferType = TRANSFER_STATE_WRITE;
        hRmI2cCont->IntMaskReg = NV_FLD_SET_DRF_DEF(I2C, INTERRUPT_MASK_REGISTER,
                                    SLV_TFIFO_DATA_REQ_INT_EN, DISABLE,
                                    hRmI2cCont->IntMaskReg);
        hRmI2cCont->IntMaskReg |= I2C_SLV_TRANSACTION_END;
        WriteIntMaksReg(hRmI2cCont);
    }
    else
    {
        NvOsDebugPrintf("I2cSlaveIsr(): Illegal transfer at this point\n");
        NV_ASSERT(0);
    }
}

static void I2cSlaveIsr(void* args)
{
    NvRmI2cControllerHandle hRmI2cCont = (NvRmI2cControllerHandle)args;

    // Read the Interrupt status register & PKT_STATUS
    hRmI2cCont->ControllerStatus = I2C_REGR(hRmI2cCont, INTERRUPT_STATUS_REGISTER);

    DEBUG_PRINT(1, ("ISR ContStatus 0x%08x FifoStatus 0x%08x\n",
                                    hRmI2cCont->ControllerStatus, FifoStatus));

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if ((hRmI2cCont->ControllerStatus & I2C_INT_STATUS_RX_DATA_AVAILABLE) ||
         (hRmI2cCont->CurrentI2cSlvTransferType == TRANSFER_STATE_READ))
    {
        HandleI2cSlaveReadInt(hRmI2cCont);
        goto Done;
    }

    if ((hRmI2cCont->ControllerStatus & I2C_INT_STATUS_TX_BUFFER_REQUEST) ||
         (hRmI2cCont->CurrentI2cSlvTransferType == TRANSFER_STATE_WRITE))
    {
        HandleI2cSlaveWriteInt(hRmI2cCont);
        goto Done;
    }

    NvOsDebugPrintf("AP20 Slave I2c Isr got unwanted interrupt IntStatus 0x%08x\n",
                                                    hRmI2cCont->ControllerStatus);
    NV_ASSERT(0);

Done:
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    NvRmInterruptDone(hRmI2cCont->I2CInterruptHandle);
}

static NvError T30I2cSlaveStart(NvRmI2cControllerHandle hRmI2cCont,
            NvU32 Address, NvBool IsTenBitAdd, NvU32 maxRecvPacketSize,
            NvU8 DummyCharTx)
{
    NvRmI2cTransactionInfo Transaction;
    NvU32 TxFifoEmptyCount;

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is already started\n", __func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveAlreadyStarted;
    }
    hRmI2cCont->rRxIndex = 0;
    hRmI2cCont->wRxIndex = 0;
    hRmI2cCont->ReadFirstByteWait = NV_TRUE;
    hRmI2cCont->IsReadWaiting = NV_FALSE;

    hRmI2cCont->I2cSlvTxTransaction.Flags = NVRM_I2C_READ;
    hRmI2cCont->I2cSlvTxTransaction.NumBytes  = 0;
    hRmI2cCont->I2cSlvTxTransaction.Address = Address;
    hRmI2cCont->I2cSlvTxTransaction.Is10BitAddress = IsTenBitAdd;

    Transaction.Flags = NVRM_I2C_WRITE;
    Transaction.NumBytes = maxRecvPacketSize - 1;
    Transaction.Address = Address;
    Transaction.Is10BitAddress = IsTenBitAdd;

    hRmI2cCont->DummyWord = (DummyCharTx) | (DummyCharTx <<8);
    hRmI2cCont->DummyWord |= hRmI2cCont->DummyWord << 16;

    GetI2cSlavePacketHeaders(hRmI2cCont, &Transaction, 1,
                                &hRmI2cCont->I2cSlvRxPacketHeader1,
                                &hRmI2cCont->I2cSlvRxPacketHeader2,
                                &hRmI2cCont->I2cSlvRxPacketHeader3);
    ConfigureI2cSlavePacketMode(hRmI2cCont);
    ConfigureI2cSlaveAddress(hRmI2cCont, Address, IsTenBitAdd);
    DoI2cSlaveTxFifoEmpty(hRmI2cCont, &TxFifoEmptyCount);

    hRmI2cCont->CurrentI2cSlvTransferType = 0;

    hRmI2cCont->IsReadWaiting = NV_FALSE;
    hRmI2cCont->IsUsingApbDma = NV_FALSE;
    hRmI2cCont->IsI2cSlaveStarted = NV_TRUE;
    SetI2cSlaveRxFifoTriggerLevel(hRmI2cCont, 1);
    I2C_REGW(hRmI2cCont, I2C_SL_INT_MASK, 0);
    hRmI2cCont->IntMaskReg = I2C_SLV_DEFAULT_INTERRUPT_MASK;
    WriteIntMaksReg(hRmI2cCont);
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    return NvSuccess;
}

static NvError T30I2cSlaveStop(NvRmI2cControllerHandle hRmI2cCont)
{
    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (!hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is not started\n",__func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveNotStarted;
    }

    ConfigureI2cSlaveAddress(hRmI2cCont, 0x0, NV_FALSE);
    I2C_REGW(hRmI2cCont, I2C_SL_INT_MASK, 0);
    I2C_REGW(hRmI2cCont, INTERRUPT_MASK_REGISTER, 0);
    hRmI2cCont->IsI2cSlaveStarted = NV_FALSE;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    return NvSuccess;
}

/*
 * Timeoutms = 0, MinBytesRead = 0, read without waiting.
 * Timeoutms = 0, MinBytesRead != 0, block till min bytes read.
 * Timeoutms != 0, wait till timeout to read data..
 * Timeoutms  = INF, wait till all req bytes read.
 */
static NvError T30I2cSlaveRead(NvRmI2cControllerHandle hRmI2cCont,
    NvU8 *pRxBuffer, NvU32 BytesRequested, NvU32 *pBytesRead, NvU8 MinBytesRead,
            NvU32 Timeoutms)
{
    NvU32 DataAvailable;
    NvU32 BytesRead = 0;
    NvU32 i, BytesToCopy;
    NvU32 WaitTime = Timeoutms;
    NvError Err = NvSuccess;
    NvU32 BytesRemaining = BytesRequested;

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (!hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is not started\n",__func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveNotStarted;
    }
    do {
        if (hRmI2cCont->rRxIndex > hRmI2cCont->wRxIndex)
            DataAvailable = MAX_RX_BUFF_SIZE - hRmI2cCont->rRxIndex + hRmI2cCont->wRxIndex;
        else
            DataAvailable = hRmI2cCont->wRxIndex - hRmI2cCont->rRxIndex;
        BytesToCopy = NV_MIN(DataAvailable, BytesRemaining);
        for (i = 0; i < BytesToCopy; i++)
        {
            *pRxBuffer = hRmI2cCont->pI2cSlvRxBuffer[hRmI2cCont->rRxIndex++];
            if (hRmI2cCont->rRxIndex >= MAX_RX_BUFF_SIZE)
                hRmI2cCont->rRxIndex = 0;
            BytesRead++;
            pRxBuffer++;
        }
        if (!Timeoutms)
        {
            if ((MinBytesRead == 0) || (BytesRead >= MinBytesRead))
                break;
            WaitTime = NV_WAIT_INFINITE;
        }
        else
        {
            if (BytesRead >= BytesRequested)
                break;
            if ((Timeoutms != NV_WAIT_INFINITE) && (Err == NvError_Timeout))
                break;
        }
        hRmI2cCont->IsReadWaiting = NV_TRUE;
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        Err = NvOsSemaphoreWaitTimeout(hRmI2cCont->hI2cSlvRxSyncSema, WaitTime);
        NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    } while(1);
    hRmI2cCont->IsReadWaiting = NV_FALSE;
    *pBytesRead = BytesRead;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    return Err;
}

static NvError T30I2cSlaveWriteAsynch(NvRmI2cControllerHandle hRmI2cCont,
    NvU8 *pTxBuffer, NvU32 BytesRequested, NvU32 *pBytesWritten)
{
    NvU32 i, SpaceAvailable;

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (!hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is not started\n",__func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveNotStarted;
    }
    SpaceAvailable = SPACE_AVAILABLE(hRmI2cCont->rTxIndex, hRmI2cCont->wTxIndex,
                                                                MAX_TX_BUFF_SIZE);
    if (SpaceAvailable < BytesRequested)
    {
        NvOsDebugPrintf("%s(): No space in Tx fifo\n", __func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        *pBytesWritten = 0;
        return NvError_I2cSlaveTxBufferFull;
    }
    for (i = 0; i < BytesRequested; i++)
    {
        hRmI2cCont->pI2cSlvTxBuffer[hRmI2cCont->wTxIndex++] = *pTxBuffer;
        pTxBuffer++;
        if (hRmI2cCont->wTxIndex >= MAX_TX_BUFF_SIZE)
            hRmI2cCont->wTxIndex = 0;
    }
    hRmI2cCont->IsWriteWaiting = NV_TRUE;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    *pBytesWritten = BytesRequested;
    return NvSuccess;
}

static NvError T30I2cSlaveGetWriteStatus(NvRmI2cControllerHandle hRmI2cCont,
                                 NvU32 TimeoutMs, NvU32 *pBytesRemaining)
{
    NvError Err = NvSuccess;
    NvU32 DataAvailable;

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (!hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is not started\n",__func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveNotStarted;
    }

    DataAvailable = DATA_AVAILABLE(hRmI2cCont->rTxIndex, hRmI2cCont->wTxIndex,MAX_TX_BUFF_SIZE);
    if (!DataAvailable)
    {
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        *pBytesRemaining = 0;
        return NvSuccess;
    }

    hRmI2cCont->IsWriteWaiting = NV_TRUE;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);

    if (TimeoutMs)
        Err = NvOsSemaphoreWaitTimeout(hRmI2cCont->hI2cSlvRxSyncSema, TimeoutMs);

    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    hRmI2cCont->IsWriteWaiting = NV_FALSE;
    *pBytesRemaining = DATA_AVAILABLE(hRmI2cCont->rTxIndex,
                                  hRmI2cCont->wTxIndex,MAX_TX_BUFF_SIZE);
    if (hRmI2cCont->CurrentI2cSlvTransferType == TRANSFER_STATE_WRITE)
        Err = NvError_I2cSlaveWriteProgress;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    return Err;
}

static void T30I2cSlaveFlushWriteBuffer(NvRmI2cControllerHandle hRmI2cCont)
{
    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (hRmI2cCont->IsI2cSlaveStarted)
    {
        hRmI2cCont->rTxIndex = 0;
        hRmI2cCont->wTxIndex = 0;
    }
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
}

static NvU32 T30I2cSlaveGetNACKCount(NvRmI2cControllerHandle hRmI2cCont,
                         NvBool IsResetCount)
{
    NvU32 NackCount;
    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (!hRmI2cCont->IsI2cSlaveStarted)
    {
        NvOsDebugPrintf("%s(): Slave is not started\n",__func__);
        NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
        return NvError_I2cSlaveNotStarted;
    }

    NackCount = hRmI2cCont->I2cSlvNackCount;
    if (IsResetCount)
        hRmI2cCont->I2cSlvNackCount = 0;
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);
    return NackCount;
}

static void T30RmI2cClose(NvRmI2cControllerHandle hRmI2cCont)
{
    NvOsIntrMutexLock(hRmI2cCont->hI2cSlvTransferMutex);
    if (hRmI2cCont->IsI2cSlaveStarted)
    {
        I2C_REGW(hRmI2cCont, I2C_SL_INT_MASK, 0);
        I2C_REGW(hRmI2cCont, INTERRUPT_MASK_REGISTER, 0);
        hRmI2cCont->IsI2cSlaveStarted = NV_FALSE;
    }

    if (hRmI2cCont->I2CInterruptHandle)
    {
        NvRmInterruptUnregister(hRmI2cCont->hRmDevice, hRmI2cCont->I2CInterruptHandle);
        hRmI2cCont->I2CInterruptHandle = NULL;
    }
    if (hRmI2cCont->hI2cSlvRxSyncSema)
    {
        NvOsSemaphoreDestroy(hRmI2cCont->hI2cSlvRxSyncSema);
        hRmI2cCont->hI2cSlvRxSyncSema = NULL;
    }

    if (hRmI2cCont->hI2cSlvTxSyncSema)
    {
        NvOsSemaphoreDestroy(hRmI2cCont->hI2cSlvTxSyncSema);
        hRmI2cCont->hI2cSlvTxSyncSema = NULL;
    }
    NvOsIntrMutexUnlock(hRmI2cCont->hI2cSlvTransferMutex);

    if (hRmI2cCont->hI2cSlvTransferMutex)
    {
        NvOsIntrMutexDestroy(hRmI2cCont->hI2cSlvTransferMutex);
        hRmI2cCont->hI2cSlvTransferMutex = NULL;
    }

    if (hRmI2cCont->pI2cSlvTxBuffer)
    {
        NvOsFree(hRmI2cCont->pI2cSlvTxBuffer);
        hRmI2cCont->pI2cSlvTxBuffer = NULL;
    }

    if (hRmI2cCont->pI2cSlvRxBuffer)
    {
        NvOsFree(hRmI2cCont->pI2cSlvRxBuffer);
        hRmI2cCont->pI2cSlvRxBuffer = NULL;
    }

    hRmI2cCont->hRmDma = NULL;
    hRmI2cCont->hRmMemory = NULL;
    hRmI2cCont->DmaBuffPhysAdd = 0;
    hRmI2cCont->pDmaBuffer = NULL;

    hRmI2cCont->receive = 0;
    hRmI2cCont->send = 0;
    hRmI2cCont->repeatStart = 0;
    hRmI2cCont->close = 0;
    hRmI2cCont->GetGpioPins = 0;
}

NvError T30RmSlaveI2cOpen(NvRmI2cControllerHandle hRmI2cCont)
{
    NvError Error = NvSuccess;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers = I2cSlaveIsr;
    NvRmPhysAddr BaseAdd;
    NvU32 BaseSize;

    NV_ASSERT(hRmI2cCont);
    DEBUG_PRINT(1, ("T30RmSlaveI2cOpen\n"));

    // Polulate the structures
    hRmI2cCont->receive = NULL;
    hRmI2cCont->send = NULL;
    hRmI2cCont->repeatStart = NULL;
    hRmI2cCont->close = T30RmI2cClose;
    hRmI2cCont->GetGpioPins =  NULL;
    hRmI2cCont->I2cSlaveStart =  T30I2cSlaveStart;
    hRmI2cCont->I2cSlaveStop =  T30I2cSlaveStop;
    hRmI2cCont->I2cSlaveRead = T30I2cSlaveRead;
    hRmI2cCont->I2cSlaveWriteAsynch = T30I2cSlaveWriteAsynch;
    hRmI2cCont->I2cSlaveGetWriteStatus = T30I2cSlaveGetWriteStatus;
    hRmI2cCont->I2cSlaveFlushWriteBuffer = T30I2cSlaveFlushWriteBuffer;
    hRmI2cCont->I2cSlaveGetNACKCount =  T30I2cSlaveGetNACKCount;

    hRmI2cCont->I2cSlvNackCount = 0;

    hRmI2cCont->ControllerId = hRmI2cCont->Instance;

    hRmI2cCont->hRmDma = NULL;
    hRmI2cCont->hRmMemory = NULL;
    hRmI2cCont->DmaBuffPhysAdd = 0;
    hRmI2cCont->pDmaBuffer = NULL;

    hRmI2cCont->pCpuBuffer = NULL;
    hRmI2cCont->pDataBuffer = NULL;
    hRmI2cCont->pClientBuffer = NULL;
    hRmI2cCont->MaxClientBufferLen = 0;
    hRmI2cCont->I2cSyncSemaphore = NULL;
    hRmI2cCont->I2CInterruptHandle = NULL;
    hRmI2cCont->TransCountFromLastDmaUsage = 0;
    hRmI2cCont->hI2cSlvRxSyncSema = NULL;
    hRmI2cCont->hI2cSlvTxSyncSema = NULL;
    hRmI2cCont->hI2cSlvTransferMutex = NULL;

    hRmI2cCont->IsMasterMode = NV_FALSE;
    hRmI2cCont->IsApbDmaAllocated = NV_FALSE;
    hRmI2cCont->hRmDma = NULL;

    hRmI2cCont->pI2cSlvRxBuffer = NULL;
    hRmI2cCont->rRxIndex = 0;
    hRmI2cCont->wRxIndex = 0;
    hRmI2cCont->ReadFirstByteWait = NV_TRUE;
    hRmI2cCont->IsReadWaiting = NV_FALSE;

    hRmI2cCont->pI2cSlvTxBuffer = NULL;
    hRmI2cCont->rTxIndex = 0;
    hRmI2cCont->wTxIndex = 0;
    hRmI2cCont->IsWriteWaiting = NV_FALSE;

    // Allocate the dma buffer
    hRmI2cCont->DmaBufferSize = 0;

    NvRmModuleGetBaseAddress(hRmI2cCont->hRmDevice,
                NVRM_MODULE_ID(hRmI2cCont->ModuleId, hRmI2cCont->Instance),
                    &BaseAdd, &BaseSize);

    hRmI2cCont->pI2cSlvRxBuffer = NvOsAlloc(MAX_RX_BUFF_SIZE);
    if (!hRmI2cCont->pI2cSlvRxBuffer)
    {
        Error = NvError_InsufficientMemory;
        goto fail;
    }

    hRmI2cCont->pI2cSlvTxBuffer = NvOsAlloc(MAX_TX_BUFF_SIZE);
    if (!hRmI2cCont->pI2cSlvTxBuffer)
    {
        Error = NvError_InsufficientMemory;
        goto fail;
    }

    // Create the Rx and Tx sync semaphore for the interrupt synchrnoisation
    if (!Error)
        Error = NvOsSemaphoreCreate( &hRmI2cCont->hI2cSlvRxSyncSema, 0);

    if (!Error)
        Error = NvOsSemaphoreCreate( &hRmI2cCont->hI2cSlvTxSyncSema, 0);

    if (!Error)
        Error = NvOsIntrMutexCreate(&hRmI2cCont->hI2cSlvTransferMutex);

    if (!Error)
    {
        IrqList = NvRmGetIrqForLogicalInterrupt(
                hRmI2cCont->hRmDevice, NVRM_MODULE_ID(hRmI2cCont->ModuleId, hRmI2cCont->Instance), 0);

        Error = NvRmInterruptRegister(hRmI2cCont->hRmDevice, 1, &IrqList, &IntHandlers,
                hRmI2cCont, &hRmI2cCont->I2CInterruptHandle, NV_TRUE);
    }
    // Packet mode initialization
    hRmI2cCont->RsTransfer =  NV_FALSE;

fail:
    // If error then destroy all the allocation done here.
    if (Error)
        T30RmI2cClose(hRmI2cCont);
    return Error;
}
