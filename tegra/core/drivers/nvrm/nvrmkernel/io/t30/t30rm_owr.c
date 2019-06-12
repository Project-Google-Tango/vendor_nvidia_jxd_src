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
 * @brief <b>NVIDIA Driver Development Kit: OWR API</b>
 *
 * @b Description: Contains the NvRM OWR implementation. for T30.
 */

#include "nvrm_owr.h"
#include "nvrm_drf.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "t30/arowr.h"
#include "nvrm_hardware_access.h"
#include "nvrm_power.h"
#include "nvrm_interrupt.h"
#include "nvassert.h"
#include "nvodm_query_pinmux.h"
#include "nvrm_pinmux.h"
#include "nvrm_hwintf.h"
#include "nvrm_owr_private.h"
#include "nvodm_query.h"
#include "nvrm_module.h"

/* Enable the following flag for debug messages */
#define ENABLE_OWR_DEBUG   0

#if ENABLE_OWR_DEBUG
#define OWR_PRINT(X)    NvOsDebugPrintf X
#else
#define OWR_PRINT(X)
#endif

/* Enabling the following flag for enabling the polling in bit transfer mode */
#define OWR_BIT_TRANSFER_POLLING_MODE   0

/* Timeout for transferring a bit in micro seconds */
#define BIT_TRASNFER_DONE_TIMEOUT_USEC  1000

/* Polling timeout steps for transferring a bit in micro seconds */
#define BIT_TRASNFER_DONE_STEP_TIMEOUT_USEC 10

/* Semaphore timeout for bit/byte transfers */
#define OWR_TRANSFER_TIMEOUT_MILLI_SEC  5000

/* OWR controller errors in byte transfer mode */
#define OWR_BYTE_TRANSFER_ERRORS    0x70F

/* OWR controller errors in bit transfer mode */
#define OWR_BIT_TRANSFER_ERRORS 0x1

/* OWR CRC size in bytes */
#define OWR_CRC_SIZE_BYTES  1

/* OWR ROM command size */
#define OWR_ROM_CMD_SIZE_BYTES  1

/* OWR MEM command size */
#define OWR_MEM_CMD_SIZE_BYTES  1

/* OWR fifo depth */
#define OWR_FIFO_DEPTH  32
/* OWR fifo word size */
#define OWR_FIFO_WORD_SIZE  4


/* default read data clock value */
#define OWR_DEFAULT_READ_DTA_CLK_VALUE   0x7
/* default read presence  clock value */
#define OWR_DEFAULT_PRESENCE_CLK_VALUE   0x50
/* Default OWR device memory offset size */
#define OWR_DEFAULT_OFFSET_SIZE_BYTES   2
/* Default OWR memory size */
#define OWR_DEFAULT_MEMORY_SIZE 0x80
/* Flag to enable generic command */
#define OWR_ENABLE_GENERIC_COMMAND  0

/* Register access Macros */
#define OWR_REGR(OwrVirtualAddress, reg) \
        NV_READ32((OwrVirtualAddress) + ((OWR_##reg##_0)/4))

#define OWR_REGW(OwrVirtualAddress, reg, val) \
    do\
    {\
        NV_WRITE32((((OwrVirtualAddress) + ((OWR_##reg##_0)/4))), (val));\
    }while (0)

/* Default OWR device info */
static const NvOdmQueryOwrDeviceInfo s_OwrDeviceInfo = {
    NV_TRUE,
    0x1, /* Tsu */
    0xF, /* TRelease */
    0xF,  /* TRdv */
    0X3C, /* TLow0 */
    0x1, /* TLow1 */
    0x77, /* TSlot */

    0x78, /* TPdl */
    0x1E, /* TPdh */
    0x1DF, /* TRstl */
    0x1DF, /* TRsth */

    0x1E0, /* Tpp */
    0x5, /* Tfp */
    0x5, /* Trp */
    0x5, /* Tdv */
    0x5, /* Tpd */

    0x7, /* Read data sample clk */
    0x50, /* Presence sample clk */
    2,  /* Memory address size */
    0x80 /* Memory Size */
};

static void PrivOwrEnableInterrupts(NvRmOwrController *pOwrInfo, NvU32 OwrIntStatus);
static NvError PrivOwrSendCommand(NvRmOwrController *pOwrInfo, NvU32 Command);
static NvError PrivOwrWriteBit(NvRmOwrController *pOwrInfo, NvU32 Bit);

static NvError
PrivOwrCheckBitTransferDone(
    NvRmOwrController* pOwrInfo,
    OwrIntrStatus status);

static NvError
PrivOwrReadBits(
    NvRmOwrController *pOwrInfo,
    NvU8* Buffer,
    NvU32 NoOfBytes);

static NvError
PrivOwrReadBit(
    NvRmOwrController *pOwrInfo,
    NvU8* Buffer);

static NvError
PrivOwrCheckPresence(
    NvRmOwrController* pOwrInfo,
    NvU32 ReadDataClk,
    NvU32 PresenceClk);

static NvError
PrivOwrReadFifo(
    NvRmOwrController* pOwrInfo,
    NvU8*  pBuffer,
    NvRmOwrTransactionInfo Transaction,
    const NvOdmQueryOwrDeviceInfo* pOdmInfo,
    NvU32 NumBytes);

static void PrivOwrEnableInterrupts(NvRmOwrController *pOwrInfo, NvU32 OwrIntStatus)
{
    /* Write to the interrupt status register */
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, INTR_MASK, OwrIntStatus);
}

static NvError
PrivOwrCheckBitTransferDone(
    NvRmOwrController* pOwrInfo,
    OwrIntrStatus status)
{
#if OWR_BIT_TRANSFER_POLLING_MODE

    NvU32 timeout = 0;
    NvU32 val = 0;

    /* Check for presence */
    while (timeout < BIT_TRASNFER_DONE_TIMEOUT_USEC)
    {
        val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, INTR_STATUS);
        if (val & status)
        {
             /* clear the bit transfer done status */
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, INTR_STATUS, val);
            break;
        }
        NvOsWaitUS(BIT_TRASNFER_DONE_STEP_TIMEOUT_USEC);
        timeout += BIT_TRASNFER_DONE_STEP_TIMEOUT_USEC;
    }

    if (timeout >= BIT_TRASNFER_DONE_TIMEOUT_USEC)
    {
        return NvError_Timeout;
    }

   return NvSuccess;
#else
    /* wait for the read to complete */
    NvError e = NvOsSemaphoreWaitTimeout(pOwrInfo->OwrSyncSemaphore,
        OWR_TRANSFER_TIMEOUT_MILLI_SEC);
    if (e == NvSuccess)
    {
        if (pOwrInfo->OwrTransferStatus & OWR_BIT_TRANSFER_ERRORS)
        {
            e = NvError_OwrBitTransferFailed;
            NvRmModuleReset(pOwrInfo->hRmDevice,
                NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
            OWR_PRINT(("RM_OWR Bit mode error[0x%x]\n",
                pOwrInfo->OwrTransferStatus));
        }
        else if (!pOwrInfo->OwrTransferStatus)
        {
            e = NvError_OwrBitTransferFailed;
            NvRmModuleReset(pOwrInfo->hRmDevice,
                NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
            OWR_PRINT(("RM_OWR bit mode spurious interrupt [0x%x]\n",
                pOwrInfo->OwrTransferStatus));
            NV_ASSERT(!"RM_OWR spurious interrupt in Bit transfer mode\n");
        }
    }
    pOwrInfo->OwrTransferStatus = 0;
    return e;
#endif
}

static NvError PrivOwrSendCommand(NvRmOwrController *pOwrInfo, NvU32 Command)
{
    NvU32 val = 0;
    NvU32 data = Command;
    NvError e = NvError_Timeout;
    NvU32 i =0;
    NvU32 ControlReg = 0;

    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, 0x7) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, 0x50) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    val = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BIT_TRANSFER_MODE);

    for (i = 0; i < OWR_NO_OF_BITS_PER_BYTE; i++)
    {
        if (data & 0x1)
        {
            ControlReg =
                val | (NV_DRF_DEF(OWR, CONTROL, WR1_BIT, TRANSFER_ONE));
        }
        else
        {
            ControlReg =
                val | (NV_DRF_DEF(OWR, CONTROL, WR0_BIT, TRANSFER_ZERO));
        }
        OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, ControlReg);

        NV_CHECK_ERROR(PrivOwrCheckBitTransferDone(pOwrInfo,
                        OwrIntrStatus_BitTransferDoneIntEnable));
        data = (data >> 1);
    }
    return NvSuccess;
}

static NvError PrivOwrWriteBit(NvRmOwrController *pOwrInfo, NvU32 Bit)
{
    NvU32 val = 0;
    NvU32 data = Bit;
    NvError e = NvError_Timeout;
    NvU32 ControlReg = 0;

    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, 0x7) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, 0x50) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    val = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BIT_TRANSFER_MODE);

    if (data & 0x1)
    {
        ControlReg =
            val | (NV_DRF_DEF(OWR, CONTROL, WR1_BIT, TRANSFER_ONE));
    }
    else
    {
        ControlReg =
            val | (NV_DRF_DEF(OWR, CONTROL, WR0_BIT, TRANSFER_ZERO));
    }
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, ControlReg);

    NV_CHECK_ERROR(PrivOwrCheckBitTransferDone(pOwrInfo,
                    OwrIntrStatus_BitTransferDoneIntEnable));
    return NvSuccess;
}

static NvError
PrivOwrReadBits(
    NvRmOwrController *pOwrInfo,
    NvU8* Buffer,
    NvU32 NoOfBytes)
{
    NvU32 ControlReg = 0;
    NvError e = NvError_Timeout;
    NvU8* pBuf = Buffer;
    NvU32 val = 0;
    NvU32 i =0;
    NvU32 j =0;

    NvOsMemset(pBuf, 0, NoOfBytes);

    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, 0x7) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, 0x50) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    ControlReg = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BIT_TRANSFER_MODE) |
        NV_DRF_DEF(OWR, CONTROL, RD_BIT, TRANSFER_READ_SLOT);

    for (i = 0; i < NoOfBytes; i++)
    {
        for (j = 0; j < OWR_NO_OF_BITS_PER_BYTE; j++)
        {
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, ControlReg);
            NV_CHECK_ERROR(PrivOwrCheckBitTransferDone(pOwrInfo,
                            OwrIntrStatus_BitTransferDoneIntEnable));
            val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, STATUS);
            val = NV_DRF_VAL(OWR, STATUS, READ_SAMPLED_BIT, val);
            *pBuf |= (val << j);
        }
        pBuf++;
    }
    return NvSuccess;
}

static NvError
PrivOwrReadBit(
    NvRmOwrController *pOwrInfo,
    NvU8* Buffer)
{
    NvU32 ControlReg = 0;
    NvError e = NvError_Timeout;
    NvU32 val = 0;

    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, 0x7) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, 0x50) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    ControlReg = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BIT_TRANSFER_MODE) |
        NV_DRF_DEF(OWR, CONTROL, RD_BIT, TRANSFER_READ_SLOT);

    OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, ControlReg);
    NV_CHECK_ERROR(PrivOwrCheckBitTransferDone(pOwrInfo,
        OwrIntrStatus_BitTransferDoneIntEnable));
    val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, STATUS);
    val = NV_DRF_VAL(OWR, STATUS, READ_SAMPLED_BIT, val);
    *Buffer = val;
    return NvSuccess;
}

static NvError
PrivOwrReadFifo(
    NvRmOwrController* pOwrInfo,
    NvU8*  pBuffer,
    NvRmOwrTransactionInfo Transaction,
    const NvOdmQueryOwrDeviceInfo* pOdmInfo,
    NvU32 NumBytes)
{
    NvU32 val = 0;
    NvError e = NvError_OwrReadFailed;
    NvU32 BytesToRead = 0;
    NvU32 WordsToRead = 0;
    NvU32 ReadDataClk = OWR_DEFAULT_READ_DTA_CLK_VALUE;
    NvU32 PresenceClk = OWR_DEFAULT_PRESENCE_CLK_VALUE;
    NvU32 i = 0;
    NvU32 size = OWR_DEFAULT_MEMORY_SIZE;
    NvU32 value = 0;

    if (pOdmInfo)
    {
        ReadDataClk = pOdmInfo->ReadDataSampleClk;
        PresenceClk = pOdmInfo->PresenceSampleClk;
        size = pOdmInfo->MemorySize;
    }

    if (Transaction.Offset >= size)
    {
        e = NvError_OwrInvalidOffset;
        return e;
    }

#if OWR_ENABLE_GENERIC_COMMAND
    if (Transaction.Flags == NvRmOwr_MemRead)
    {
        /* Configure the number of bytes to read */
        value = size - Transaction.Offset - 1;
        OWR_REGW(pOwrInfo->pOwrVirtualAddress,
                    EPROM, value);
    }
    val = NV_DRF_DEF(OWR, CONTROL, GO, START_PRESENCE_PULSE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, val);
#else
    /* Configure the number of bytes to read */
    value = size - Transaction.Offset - 1;
    OWR_REGW(pOwrInfo->pOwrVirtualAddress,
                EPROM, value);
    /** Configure the read, presence sample clock and
     * configure for byte transfer mode.
     */
    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, ReadDataClk) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, PresenceClk) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    val = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BYTE_TRANSFER_MODE) |
        NV_DRF_DEF(OWR, CONTROL, RD_MEM_CRC_REQ, CRC_READ) |
        NV_DRF_DEF(OWR, CONTROL, GO, START_PRESENCE_PULSE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, val);
#endif
    /* wait for the read to complete */
    NV_CHECK_ERROR(NvOsSemaphoreWaitTimeout(pOwrInfo->OwrSyncSemaphore,
                    OWR_TRANSFER_TIMEOUT_MILLI_SEC));
    if (pOwrInfo->OwrTransferStatus & OWR_BYTE_TRANSFER_ERRORS)
    {
        NvRmModuleReset(pOwrInfo->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
        OWR_PRINT(("RM_OWR Byte mode error interrupt[0x%x]\n",
            pOwrInfo->OwrTransferStatus));
        return NvError_OwrReadFailed;
    }
    else if (pOwrInfo->OwrTransferStatus & OwrIntrStatus_MemCmdDoneIntEnable)
    {
        /* Read the data */
        if (Transaction.Flags == NvRmOwr_ReadAddress)
        {
            /* Read and copy and the ROM ID */
            val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, READ_ROM0);
            NvOsMemcpy(pBuffer, &val, 4);
            pBuffer += 4;

            val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, READ_ROM1);
            NvOsMemcpy(pBuffer, &val, 4);
        }
        else if (Transaction.Flags == NvRmOwr_MemRead)
        {
            val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, BYTE_CNT);
            val = NV_DRF_VAL(OWR, BYTE_CNT, RECEIVED, val);
            /** Decrement the number of bytes to read count as it includes
             * one byte CRC.
             */
            val--;

            BytesToRead = (val > NumBytes) ? NumBytes : val;
            WordsToRead =  BytesToRead / OWR_FIFO_WORD_SIZE;
            for (i = 0; i < WordsToRead; i++)
            {
                val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, RX_FIFO);
                NvOsMemcpy(pBuffer, &val, OWR_FIFO_WORD_SIZE);
                pBuffer += OWR_FIFO_WORD_SIZE;
            }
            BytesToRead = (BytesToRead % OWR_FIFO_WORD_SIZE);
            if (BytesToRead)
            {
                val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, RX_FIFO);
                NvOsMemcpy(pBuffer, &val, BytesToRead);
            }
        }
    }
    else
    {
        OWR_PRINT(("RM_OWR Byte mode spurious interrupt[0x%x]\n",
                pOwrInfo->OwrTransferStatus));
        NV_ASSERT(!"RM_OWR spurious interrupt\n");
        NvRmModuleReset(pOwrInfo->hRmDevice,
        NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
        return NvError_OwrReadFailed;
    }
    return e;
}

static NvError
PrivOwrWriteFifo(
    NvRmOwrController* pOwrInfo,
    NvU8*  pBuffer,
    NvRmOwrTransactionInfo Transaction,
    const NvOdmQueryOwrDeviceInfo* pOdmInfo,
    NvU32 NumBytes)
{
    NvU32 val = 0;
    NvError e = NvError_OwrWriteFailed;
    NvU32 ReadDataClk = OWR_DEFAULT_READ_DTA_CLK_VALUE;
    NvU32 PresenceClk = OWR_DEFAULT_PRESENCE_CLK_VALUE;
    NvU32 i = 0;

    if (pOdmInfo)
    {
        ReadDataClk = pOdmInfo->ReadDataSampleClk;
        PresenceClk = pOdmInfo->PresenceSampleClk;
    }

    /* Configure the number of bytes to write */
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, EPROM, (NumBytes - 1));

    /* Write data into the FIFO */
    for (i = NumBytes; i > 0;)
    {
        NvU32 BytesToWrite = NV_MIN(sizeof(NvU32),i);
        val = 0;
        switch (BytesToWrite)
        {
        case 4: val |= pBuffer[3]; i--;           /* fallthrough */
        case 3: val <<=8; val |= pBuffer[2]; i--; /* fallthrough */
        case 2: val <<=8; val |= pBuffer[1]; i--; /* fallthrough */
        case 1: val <<=8; val |= pBuffer[0]; i--;
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, TX_FIFO, val);
            pBuffer += BytesToWrite;
            break;
        }
    }

    /** Configure the read, presence sample clock and
     * configure for byte transfer mode
     */
    val =
        NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, ReadDataClk) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
        NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, PresenceClk) |
        NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

    val = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BYTE_TRANSFER_MODE) |
        NV_DRF_DEF(OWR, CONTROL, GO, START_PRESENCE_PULSE);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, val);

    /* wait for the write to complete */
    NV_CHECK_ERROR(NvOsSemaphoreWaitTimeout(pOwrInfo->OwrSyncSemaphore,
            OWR_TRANSFER_TIMEOUT_MILLI_SEC));
    if (pOwrInfo->OwrTransferStatus & OWR_BYTE_TRANSFER_ERRORS)
    {
        NvRmModuleReset(pOwrInfo->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
        OWR_PRINT(("RM_OWR Byte mode error interrupt[0x%x]\n",
            pOwrInfo->OwrTransferStatus));
        return NvError_OwrWriteFailed;
    }
    else if (pOwrInfo->OwrTransferStatus & OwrIntrStatus_MemCmdDoneIntEnable)
    {
        val = OWR_REGR(pOwrInfo->pOwrVirtualAddress, BYTE_CNT);
        val = NV_DRF_VAL(OWR, BYTE_CNT, RECEIVED, val);

        /** byte count includes ROM, Mem command size and Memory
         * address size. So, subtract ROM, Mem Command size and
         * memory address size from byte count.
         */
        val -= OWR_MEM_CMD_SIZE_BYTES;
        val -= OWR_MEM_CMD_SIZE_BYTES;
        val -= OWR_DEFAULT_OFFSET_SIZE_BYTES;

        /** Assert if the actual bytes written is
         * not equal to the bytes written
         */
        NV_ASSERT(val == NumBytes);
    }
    else
    {
        OWR_PRINT(("RM_OWR Byte mode spurious interrupt[0x%x]\n",
            pOwrInfo->OwrTransferStatus));
        NV_ASSERT(!"RM_OWR spurious interrupt\n");
        NvRmModuleReset(pOwrInfo->hRmDevice,
            NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance));
        return NvError_OwrWriteFailed;
    }
    return e;
}

static NvError
PrivOwrCheckPresence(
        NvRmOwrController* pOwrInfo,
        NvU32 ReadDataClk,
        NvU32 PresenceClk)
{
        NvError status = NvSuccess;
        NvU32 val = 0;

        /* Enable the bit transfer done interrupt */
        PrivOwrEnableInterrupts(pOwrInfo, OwrIntrStatus_PresenceDoneIntEnable);
        pOwrInfo->OwrTransferStatus = 0;

        /* Configure for presence */
        val =
            NV_DRF_NUM(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK, ReadDataClk) |
            NV_DRF_DEF(OWR, SAMPLE_DATA, RD_DATA_SAMPLE_CLK_EN, ENABLE) |
            NV_DRF_NUM(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK, PresenceClk) |
            NV_DRF_DEF(OWR, SAMPLE_DATA, PRESENCE_SAMPLE_CLK_EN, ENABLE);
        OWR_REGW(pOwrInfo->pOwrVirtualAddress, SAMPLE_DATA, val);

        val = NV_DRF_DEF(OWR, CONTROL, DATA_TRANSFER_MODE, BIT_TRANSFER_MODE) |
            NV_DRF_DEF(OWR, CONTROL, GO, START_PRESENCE_PULSE);

        OWR_REGW(pOwrInfo->pOwrVirtualAddress, CONTROL, val);

        /* Check for presence */
        status = PrivOwrCheckBitTransferDone(pOwrInfo,
                                    OwrIntrStatus_PresenceDoneIntEnable);
        return status;
}

/****************************************************************************/

static void OwrIsr(void* args)
{
    NvRmOwrController* pOwrInfo = args;
    NvU32 IntStatus;

    /* Read the interrupt status register */
    IntStatus = OWR_REGR(pOwrInfo->pOwrVirtualAddress, INTR_STATUS);

    /* Save the status */
    pOwrInfo->OwrTransferStatus = IntStatus;

    /* Clear the interrupt status register */
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, INTR_STATUS, IntStatus);

    /* Signal the sema */
    NvOsSemaphoreSignal(pOwrInfo->OwrSyncSemaphore);
    NvRmInterruptDone(pOwrInfo->OwrInterruptHandle);
}

static void T30RmOwrClose(NvRmOwrController *pOwrInfo)
{
    if (pOwrInfo->OwrSyncSemaphore)
    {
#if !OWR_BIT_TRANSFER_POLLING_MODE
        NvRmInterruptUnregister(pOwrInfo->hRmDevice,
                        pOwrInfo->OwrInterruptHandle);
#endif
        NvOsSemaphoreDestroy(pOwrInfo->OwrSyncSemaphore);
        pOwrInfo->OwrSyncSemaphore = NULL;
        pOwrInfo->OwrInterruptHandle = NULL;
    }
    pOwrInfo->read = 0;
    pOwrInfo->write = 0;
    pOwrInfo->close = 0;
    NvOsPhysicalMemUnmap(pOwrInfo->pOwrVirtualAddress, pOwrInfo->OwrBankSize);
}

static NvError
T30RmOwrRead(
    NvRmOwrController* pOwrInfo,
    NvU8*  pBuffer,
    NvRmOwrTransactionInfo Transaction)
{
    NvU32 val = 0;
    NvError e = NvError_BadParameter;
    NvBool IsByteModeSupported = NV_FALSE;
    const NvOdmQueryOwrDeviceInfo* pOwrOdmInfo = NULL;
    NvU32 ReadDataClk = OWR_DEFAULT_READ_DTA_CLK_VALUE;
    NvU32 PresenceClk = OWR_DEFAULT_PRESENCE_CLK_VALUE;
    NvU32 DeviceOffsetSize = OWR_DEFAULT_OFFSET_SIZE_BYTES;
    NvU32 TotalBytesToRead = 0;
    NvU32 BytesRead = 0;
    NvU32 FifoSize = 0;
    NvU32 i = 0;
    NvU8* pReadPtr = pBuffer;

    if ((Transaction.Flags == NvRmOwr_MemRead) && (!Transaction.NumBytes))
    {
        return NvError_BadParameter;
    }

    pOwrOdmInfo = NvOdmQueryGetOwrDeviceInfo(pOwrInfo->Instance);
    if (!pOwrOdmInfo)
    {
        pOwrOdmInfo = &s_OwrDeviceInfo;
    }
    IsByteModeSupported = pOwrOdmInfo->IsByteModeSupported;
    ReadDataClk = pOwrOdmInfo->ReadDataSampleClk;
    PresenceClk = pOwrOdmInfo->PresenceSampleClk;
    DeviceOffsetSize = pOwrOdmInfo->AddressSize;

    /* program the timing registers */
    val = NV_DRF_NUM(OWR, TIME_SLOT_RELEASE_TCTL, TSLOT, pOwrOdmInfo->TSlot) |
            NV_DRF_NUM(OWR, TIME_SLOT_RELEASE_TCTL, TRELEASE, pOwrOdmInfo->TRelease);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, TIME_SLOT_RELEASE_TCTL, val);

    val = NV_DRF_NUM(OWR, WRITE_ONE_ZERO_TCTL, TLOW1, pOwrOdmInfo->TLow1) |
            NV_DRF_NUM(OWR, WRITE_ONE_ZERO_TCTL, TLOW0, pOwrOdmInfo->TLow0);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, WRITE_ONE_ZERO_TCTL, val);

    val = NV_DRF_NUM(OWR, READ_DATA_TCTL, TRDV, pOwrOdmInfo->TRdv) |
            NV_DRF_NUM(OWR, READ_DATA_TCTL, TSU, pOwrOdmInfo->Tsu);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, READ_DATA_TCTL, val);

    val = NV_DRF_NUM(OWR, RST_PRESENCE_DETECT_TCTL, TPDL, pOwrOdmInfo->Tpdl) |
            NV_DRF_NUM(OWR, RST_PRESENCE_DETECT_TCTL, TPDH, pOwrOdmInfo->Tpdh);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, RST_PRESENCE_DETECT_TCTL, val);

    val = NV_DRF_NUM(OWR, RST_TIME_TCTL, TRSTH, pOwrOdmInfo->TRsth) |
            NV_DRF_NUM(OWR, RST_TIME_TCTL, TRSTL, pOwrOdmInfo->TRstl);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, RST_TIME_TCTL, val);

    val = NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TPD, pOwrOdmInfo->Tpd) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TDV, pOwrOdmInfo->Tdv) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TRP, pOwrOdmInfo->Trp) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TFP, pOwrOdmInfo->Tfp);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, PROGRAM_PULSE_TCTL, val);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, PROGRAM_PULSE_WIDTH_TCTL, pOwrOdmInfo->Tpp);

    if (Transaction.Flags == NvRmOwr_CheckPresence)
    {
        e = PrivOwrCheckPresence(pOwrInfo, ReadDataClk, PresenceClk);
    }
    else if (Transaction.Flags == NvRmOwr_ReadBit)
    {
        pOwrInfo->OwrTransferStatus = 0;
        /* Enable the bit transfer done interrupt */
        PrivOwrEnableInterrupts(pOwrInfo,
                OwrIntrStatus_BitTransferDoneIntEnable);
        e = PrivOwrReadBit(pOwrInfo, pReadPtr);
    }
    else if (Transaction.Flags == NvRmOwr_ReadByte)
    {
        pOwrInfo->OwrTransferStatus = 0;
        /* Enable the bit transfer done interrupt */
        PrivOwrEnableInterrupts(pOwrInfo,
            OwrIntrStatus_BitTransferDoneIntEnable);
        e = PrivOwrReadBits(pOwrInfo, pReadPtr, 1);
    }
    else if ((Transaction.Flags == NvRmOwr_MemRead) ||
                 (Transaction.Flags == NvRmOwr_ReadAddress))
    {
        if (!IsByteModeSupported)
        {
            /* Bit transfer mode */
            NV_CHECK_ERROR(PrivOwrCheckPresence(pOwrInfo,
                            ReadDataClk, PresenceClk));
            if (Transaction.Flags == NvRmOwr_ReadAddress)
            {
                /* Send the ROM Read Command */
                NV_ASSERT_SUCCESS(PrivOwrSendCommand(pOwrInfo,
                            OWR_ROM_READ_COMMAND));

                /* Read byte */
                e = PrivOwrReadBits(pOwrInfo, pReadPtr, OWR_ROM_ID_SIZE_BYTES);
            }
            else
            {
                /* Skip the ROM Read Command */
                NV_ASSERT_SUCCESS(
                    PrivOwrSendCommand(pOwrInfo, OWR_ROM_SKIP_COMMAND));

                /* Send the Mem Read Command */
                NV_ASSERT_SUCCESS(
                    PrivOwrSendCommand(pOwrInfo, OWR_MEM_READ_COMMAND));

                /* Send offset in memory */
                for (i = 0; i < DeviceOffsetSize; i++)
                {
                    val = (Transaction.Offset >> i) & 0xFF;
                    NV_ASSERT_SUCCESS(PrivOwrSendCommand(pOwrInfo, val));
                }

                /* Read the CRC */
                NV_ASSERT_SUCCESS(
                    PrivOwrReadBits(pOwrInfo, pReadPtr, OWR_CRC_SIZE_BYTES));

                /* TODO: Need to compute the CRC and compare with the CRC read */

                /* Read Mem data */
                e = PrivOwrReadBits(pOwrInfo, pReadPtr, Transaction.NumBytes);
            }
        }
        else
        {
            /* Byte transfer Mode */
            PrivOwrEnableInterrupts(pOwrInfo,
                (OwrIntrStatus_PresenceErrIntEnable |
                OwrIntrStatus_CrcErrIntEnable |
                OwrIntrStatus_MemWriteErrIntEnable |
                OwrIntrStatus_ErrCommandIntEnable |
                OwrIntrStatus_MemCmdDoneIntEnable|
                OwrIntrStatus_TxfOvfIntEnable |
                OwrIntrStatus_RxfUnrIntEnable));
#if OWR_ENABLE_GENERIC_COMMAND
            /* FIXME: Program the correct timeout vales */
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, CMD_XFER_TCTL, 0x50);
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, ADDR_XFER_TCTL, 0x50);
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, DATA_XFER_TCTL, 0x50);

            if (Transaction.Flags == NvRmOwr_ReadAddress)
            {
                val =
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER, SEND) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER_WDATA, NO_WRITE) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER_TWAIT, NO_WAIT);
                OWR_REGW(pOwrInfo->pOwrVirtualAddress, GENERIC_CTL, val);
            }
            else if (Transaction.Flags == NvRmOwr_MemRead)
            {
                val =
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER, SEND) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER_WDATA, NO_WRITE) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, CMD_XFER_TWAIT, NO_WAIT) |

                    NV_DRF_DEF(OWR, GENERIC_CTL, ADDR_XFER, SEND) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, ADDR_XFER_MODE, EN_8BIT) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, ADDR_XFER_TWAIT, NO_WAIT) |

                    NV_DRF_DEF(OWR, GENERIC_CTL, RD_DATA_XFER, READ) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, WR_DATA_XFER, NO_WRITE) |
                    NV_DRF_DEF(OWR, GENERIC_CTL, DATA_XFER_TWAIT, NO_WAIT);
                OWR_REGW(pOwrInfo->pOwrVirtualAddress, GENERIC_CTL, val);
            }
#else
            /* Configure the Rom command and the eeprom starting address */
            val = (
                NV_DRF_NUM(OWR, COMMAND, ROM_CMD, OWR_ROM_READ_COMMAND) |
                NV_DRF_NUM(OWR, COMMAND, MEM_CMD, OWR_MEM_READ_COMMAND) |
                NV_DRF_NUM(OWR, COMMAND, MEM_ADDR, Transaction.Offset));
            OWR_REGW(pOwrInfo->pOwrVirtualAddress, COMMAND, val);
#endif
            /** We can't porgam ROM ID read alone, memory read should also be given
            * along with ROM ID read. So, preogramming memory read of 1byte even
            * for ROM ID read.
            */
            TotalBytesToRead = (Transaction.NumBytes) ? Transaction.NumBytes : 1;
            FifoSize = (OWR_FIFO_DEPTH * OWR_FIFO_WORD_SIZE);
            while (TotalBytesToRead)
            {
                BytesRead =
                    (TotalBytesToRead > FifoSize) ? FifoSize : TotalBytesToRead;
                pOwrInfo->OwrTransferStatus = 0;
                NV_CHECK_ERROR(PrivOwrReadFifo(pOwrInfo, pReadPtr, Transaction,
                        pOwrOdmInfo, BytesRead));
                TotalBytesToRead -= BytesRead;
                pReadPtr += BytesRead;
            }
        }
    }
    return e;
}

static NvError
T30RmOwrWrite(
    NvRmOwrController *pOwrInfo,
    NvU8*  pBuffer,
    NvRmOwrTransactionInfo Transaction)
{
    NvU32 val = 0;
    NvError e = NvError_BadParameter;
    const NvOdmQueryOwrDeviceInfo* pOwrOdmInfo = NULL;
    NvU32 TotalBytesToWrite = 0;
    NvU32 BytesWritten = 0;
    NvU32 FifoSize = 0;
    NvU8* pWritePtr = pBuffer;

    if ((Transaction.Flags == NvRmOwr_MemWrite) && (!Transaction.NumBytes))
    {
        return NvError_BadParameter;
    }

    pOwrOdmInfo = NvOdmQueryGetOwrDeviceInfo(pOwrInfo->Instance);
    if (!pOwrOdmInfo)
    {
        pOwrOdmInfo = &s_OwrDeviceInfo;
    }

    /* program the timing registers */
    val = NV_DRF_NUM(OWR, WR_RD_TCTL, TSLOT, pOwrOdmInfo->TSlot) |
            NV_DRF_NUM(OWR, WR_RD_TCTL, TLOW1, pOwrOdmInfo->TLow1) |
            NV_DRF_NUM(OWR, WR_RD_TCTL, TLOW0, pOwrOdmInfo->TLow0) |
            NV_DRF_NUM(OWR, WR_RD_TCTL, TRDV, pOwrOdmInfo->TRdv) |
            NV_DRF_NUM(OWR, WR_RD_TCTL, TRELEASE, pOwrOdmInfo->TRelease) |
            NV_DRF_NUM(OWR, WR_RD_TCTL, TSU, pOwrOdmInfo->Tsu);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, WR_RD_TCTL, val);

    val = NV_DRF_NUM(OWR, RST_PRESENCE_DETECT_TCTL, TPDL, pOwrOdmInfo->Tpdl) |
            NV_DRF_NUM(OWR, RST_PRESENCE_DETECT_TCTL, TPDH, pOwrOdmInfo->Tpdh);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, RST_PRESENCE_DETECT_TCTL, val);

    val = NV_DRF_NUM(OWR, RST_TIME_TCTL, TRSTH, pOwrOdmInfo->TRsth) |
            NV_DRF_NUM(OWR, RST_TIME_TCTL, TRSTL, pOwrOdmInfo->TRstl);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, RST_TIME_TCTL, val);

    val = NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TPD, pOwrOdmInfo->Tpd) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TDV, pOwrOdmInfo->Tdv) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TRP, pOwrOdmInfo->Trp) |
            NV_DRF_NUM(OWR, PROGRAM_PULSE_TCTL, TFP, pOwrOdmInfo->Tfp);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, PROGRAM_PULSE_TCTL, val);
    OWR_REGW(pOwrInfo->pOwrVirtualAddress, PROGRAM_PULSE_WIDTH_TCTL,
                    pOwrOdmInfo->Tpp);

    if (Transaction.Flags == NvRmOwr_WriteBit)
    {
        /* Enable the bit transfer done interrupt */
        PrivOwrEnableInterrupts(pOwrInfo, OwrIntrStatus_BitTransferDoneIntEnable);
        pOwrInfo->OwrTransferStatus = 0;
        e = PrivOwrWriteBit(pOwrInfo, (NvU32)(*pWritePtr));
    }
    else if (Transaction.Flags == NvRmOwr_WriteByte)
    {
        /* Enable the bit transfer done interrupt */
        PrivOwrEnableInterrupts(pOwrInfo, OwrIntrStatus_BitTransferDoneIntEnable);
        pOwrInfo->OwrTransferStatus = 0;
        e = PrivOwrSendCommand(pOwrInfo, (NvU32)(*pWritePtr));
    }
    else if (Transaction.Flags == NvRmOwr_MemWrite)
    {
        /* Only Byte transfer Mode is supported for writes */
        NV_ASSERT(pOwrOdmInfo->IsByteModeSupported == NV_TRUE);

        /* Enable the interrupts */
        PrivOwrEnableInterrupts(pOwrInfo,
                        (OwrIntrStatus_PresenceErrIntEnable |
                         OwrIntrStatus_CrcErrIntEnable |
                         OwrIntrStatus_MemWriteErrIntEnable |
                         OwrIntrStatus_ErrCommandIntEnable |
                         OwrIntrStatus_MemCmdDoneIntEnable|
                         OwrIntrStatus_TxfOvfIntEnable |
                         OwrIntrStatus_RxfUnrIntEnable));

        /* Configure the Rom command and the eeprom starting address */
        val = (
                NV_DRF_NUM(OWR, COMMAND, ROM_CMD, OWR_ROM_READ_COMMAND) |
                NV_DRF_NUM(OWR, COMMAND, MEM_CMD, OWR_MEM_WRITE_COMMAND) |
                NV_DRF_NUM(OWR, COMMAND, MEM_ADDR, Transaction.Offset));
        OWR_REGW(pOwrInfo->pOwrVirtualAddress, COMMAND, val);

        TotalBytesToWrite = Transaction.NumBytes;
        FifoSize = (OWR_FIFO_DEPTH * OWR_FIFO_WORD_SIZE);
        while (TotalBytesToWrite)
        {
            BytesWritten =
                (TotalBytesToWrite > FifoSize) ? FifoSize : TotalBytesToWrite;
            pOwrInfo->OwrTransferStatus = 0;
            NV_CHECK_ERROR(PrivOwrWriteFifo(pOwrInfo, pWritePtr, Transaction,
                                    pOwrOdmInfo, BytesWritten));
            TotalBytesToWrite -= BytesWritten;
            pWritePtr += BytesWritten;
        }
    }

    return e;
}

NvError T30RmOwrOpen(NvRmOwrController *pOwrInfo)
{
    NvError e = NvSuccess;
    NvU32 IrqList;
    NvOsInterruptHandler IntHandlers;

    NV_ASSERT(pOwrInfo != NULL);

    /* Polulate the structures */
    pOwrInfo->read = T30RmOwrRead;
    pOwrInfo->write = T30RmOwrWrite;
    pOwrInfo->close = T30RmOwrClose;

    NvRmModuleGetBaseAddress(
        pOwrInfo->hRmDevice,
        NVRM_MODULE_ID(NvRmModuleID_OneWire, pOwrInfo->Instance),
        &pOwrInfo->OwrPhysicalAddress,
        &pOwrInfo->OwrBankSize);

    NV_ASSERT_SUCCESS(NvRmPhysicalMemMap(
        pOwrInfo->OwrPhysicalAddress,
        pOwrInfo->OwrBankSize, NVOS_MEM_READ_WRITE,
        NvOsMemAttribute_Uncached,
        (void **)&pOwrInfo->pOwrVirtualAddress));

    /* Create the sync semaphore */
    NV_CHECK_ERROR_CLEANUP(NvOsSemaphoreCreate(&pOwrInfo->OwrSyncSemaphore, 0));
    IntHandlers = OwrIsr;
    IrqList = NvRmGetIrqForLogicalInterrupt(pOwrInfo->hRmDevice,
                NVRM_MODULE_ID(pOwrInfo->ModuleId, pOwrInfo->Instance), 0);
#if !OWR_BIT_TRANSFER_POLLING_MODE
    NV_CHECK_ERROR_CLEANUP(NvRmInterruptRegister(pOwrInfo->hRmDevice, 1, &IrqList,
            &IntHandlers, pOwrInfo, &pOwrInfo->OwrInterruptHandle, NV_TRUE));
#endif
    return e;
fail:
    NvOsSemaphoreDestroy(pOwrInfo->OwrSyncSemaphore);
    pOwrInfo->OwrSyncSemaphore = 0;
    NvOsPhysicalMemUnmap(pOwrInfo->pOwrVirtualAddress,
        pOwrInfo->OwrBankSize);

    return e;
}

