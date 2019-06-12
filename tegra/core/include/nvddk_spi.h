/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_NVDDK_SPI_H
#define INCLUDED_NVDDK_SPI_H

#include "nvrm_init.h"
#include "nvddk_blockdev.h"
#include "nvodm_query.h"

typedef struct
{
    NvU32 *pTxBuff;
    NvU32 *pRxBuff;
    NvU32 BytesPerPacket;
    NvU32 PacketBitLength;
    NvU32 PacketsPerWord;
    NvU32 PacketRequested;
    NvU32 PacketTransferred;
    NvU32 TotalPacketsRemaining;
    NvU32 RxPacketsRemaining;
    NvU32 TxPacketsRemaining;
    NvU32 CurrPacketCount;
} TransferBufferInfo;

typedef enum
{
    // No data transfer.
    SerialDataFlow_None    = 0x0,

    // Receive data flow.
    SerialDataFlow_Rx      = 0x1,

    // Transmit data flow.
    SerialDataFlow_Tx      = 0x2,

    SerialDataFlow_Force32 = 0x7FFFFFFF
} SerialDataFlow;

// Maximum chipselect available for per spi channel.
#define MAX_CHIPSELECT_PER_INSTANCE 4

typedef struct NvDdkSpiRec
{
    // SPI instance Id
    NvU32 Instance;
    // Instance Open Count
    NvU32 OpenCount;
    // Chip select Id
    NvU32 ChipSelect;
    // SPI Clock frequency in KHZ
    NvU32 ClockFreqInKHz;
    // Base address of the controller
    volatile NvU32 *pRegBaseAdd;

    // Idle states for the controller instance
    NvBool IsIdleSignalTristate;
    NvOdmQuerySpiSignalMode IdleSignalMode;
    NvBool IsIdleDataOutHigh;

    // Current signal mode
    NvOdmQuerySpiSignalMode CurSignalMode;
    // SPI Device info
    NvOdmQuerySpiDeviceInfo DeviceInfo[MAX_CHIPSELECT_PER_INSTANCE];
    // Chip select level
    NvBool IsCurrentChipSelStateHigh[MAX_CHIPSELECT_PER_INSTANCE];
    // Is chip select support
    NvBool IsChipSelSupported[MAX_CHIPSELECT_PER_INSTANCE];
    // Is chip select configured
    NvBool IsChipSelConfigured;
    // Is Hw based chip select supported
    NvBool IsHwChipSelectSupported;
    // Is Packed Mode
    NvBool IsPackedMode;
    // Current transfer info
    TransferBufferInfo CurrTransInfo;
    // Current data flow direction
    SerialDataFlow CurrentDirection;
    // pointer to receive CPU buffer
    NvU32 *pRxCpuBuffer;
    // pointer to transmit CPU buffer
    NvU32 *pTxCpuBuffer;
    // Recieve status error
    NvError RxTransferStatus;
    // Transfer status error
    NvError TxTransferStatus;
} NvDdkSpi;

typedef struct NvDdkSpiRec* NvDdkSpiHandle;


NvDdkSpiHandle NvDdkSpiOpen(NvU32 Instance);

void NvDdkSpiClose(NvDdkSpiHandle hDdkSpi);

void
NvDdkSpiTransaction(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect,
    NvU32 ClockSpeedInKHz,
    NvU8 *ReadBuf,
    const NvU8 *WriteBuf,
    NvU32 BytesRequested,
    NvU32 PacketSizeInBits);


#endif

