/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvassert.h"
#include "nvddk_spi_hw.h"
#include "nvddk_spi_debug.h"
#include "nvddk_spi.h"


NvDdkSpiHandle s_hDdkSpiHandle[MAX_SPI_INSTANCES] = {NULL};

static NvBool SpiHandleTransferComplete(NvDdkSpiHandle hDdkSpi)
{
    NvU32 WordsReq;
    NvU32 WordsRead;
    NvU32 CurrPacketSize;
    NvU32 WordsWritten;

    SPI_INFO_PRINT("%s: entry\n", __func__);
    if (hDdkSpi->CurrentDirection & SerialDataFlow_Tx)
        hDdkSpi->TxTransferStatus = SpiGetTransferStatus(
                                     hDdkSpi, SerialDataFlow_Tx);

    if (hDdkSpi->CurrentDirection & SerialDataFlow_Rx)
        hDdkSpi->RxTransferStatus = SpiGetTransferStatus(
                                     hDdkSpi, SerialDataFlow_Rx);

    // Any error then stop the transfer and return.
    if (hDdkSpi->RxTransferStatus || hDdkSpi->TxTransferStatus)
    {
        SpiSetDataFlowDirection(hDdkSpi, NV_FALSE);
        SpiFlushFifos(hDdkSpi);
        hDdkSpi->CurrTransInfo.PacketTransferred +=
                               SpiGetTransferdCount(hDdkSpi);
        hDdkSpi->CurrentDirection = SerialDataFlow_None;
        return NV_TRUE;
    }

    if ((hDdkSpi->CurrentDirection & SerialDataFlow_Rx) &&
        (hDdkSpi->CurrTransInfo.RxPacketsRemaining))
    {
        WordsReq = ((hDdkSpi->CurrTransInfo.CurrPacketCount) +
                        ((hDdkSpi->CurrTransInfo.PacketsPerWord) -1))/
                        (hDdkSpi->CurrTransInfo.PacketsPerWord);
        WordsRead = SpiReadFromReceiveFifo(hDdkSpi,
                            hDdkSpi->CurrTransInfo.pRxBuff, WordsReq);
        hDdkSpi->CurrTransInfo.RxPacketsRemaining -=
                                hDdkSpi->CurrTransInfo.CurrPacketCount;
        hDdkSpi->CurrTransInfo.PacketTransferred +=
                                hDdkSpi->CurrTransInfo.CurrPacketCount;
        hDdkSpi->CurrTransInfo.pRxBuff += WordsRead;
    }

    if ((hDdkSpi->CurrentDirection & SerialDataFlow_Tx) &&
                            (hDdkSpi->CurrTransInfo.TxPacketsRemaining))
    {
        WordsReq = (hDdkSpi->CurrTransInfo.TxPacketsRemaining +
                   hDdkSpi->CurrTransInfo.PacketsPerWord -1)/
                        hDdkSpi->CurrTransInfo.PacketsPerWord;
        WordsWritten = SpiWriteInTransmitFifo(
                        hDdkSpi, hDdkSpi->CurrTransInfo.pTxBuff, WordsReq);
        CurrPacketSize = NV_MIN(hDdkSpi->CurrTransInfo.PacketsPerWord *
                             WordsWritten,
                             hDdkSpi->CurrTransInfo.TxPacketsRemaining);
        SpiSetDataFlowDirection(hDdkSpi, NV_TRUE);
        SpiStartTransfer(hDdkSpi,NV_FALSE);
        hDdkSpi->CurrTransInfo.CurrPacketCount = CurrPacketSize;
        hDdkSpi->CurrTransInfo.TxPacketsRemaining -= CurrPacketSize;
        hDdkSpi->CurrTransInfo.PacketTransferred += CurrPacketSize;
        hDdkSpi->CurrTransInfo.pTxBuff += WordsWritten;
        return NV_FALSE;
    }

    // If still need to do the transfer for receiving the data then start now.
    if ((hDdkSpi->CurrentDirection & SerialDataFlow_Rx) &&
                            (hDdkSpi->CurrTransInfo.RxPacketsRemaining))
    {
        CurrPacketSize = NV_MIN(hDdkSpi->CurrTransInfo.RxPacketsRemaining,
                                (MAX_CPU_TRANSACTION_SIZE_WORD*
                                    hDdkSpi->CurrTransInfo.PacketsPerWord));
        hDdkSpi->CurrTransInfo.CurrPacketCount = CurrPacketSize;
        SpiStartTransfer(hDdkSpi, NV_FALSE);
        SpiSetDmaTransferSize(hDdkSpi, CurrPacketSize);
        return NV_FALSE;
    }

    // All requested transfer is completed.
    return NV_TRUE;
}

static NvError
SpiWaitForTransferComplete(
    NvDdkSpiHandle hDdkSpi,
    NvU32 WaitTimeOutMS)
{
    NvU32 StartTime;
    NvU32 CurrentTime;
    NvU32 TimeElapsed;
    NvBool IsWait = NV_TRUE;
    NvError Error = NvSuccess;
    NvBool IsReady;
    NvBool IsTransferComplete= NV_FALSE;
    NvU32 CurrentPacketTransfer =0;
    SPI_INFO_PRINT("%s: entry\n", __func__);

    StartTime = NvOsGetTimeMS();
    while (IsWait)
    {
        IsReady = IsSpiTransferComplete(hDdkSpi);
        if (IsReady)
        {
            IsTransferComplete = SpiHandleTransferComplete(hDdkSpi);
            if(IsTransferComplete)
                break;
        }
        if (WaitTimeOutMS != NV_WAIT_INFINITE)
        {
            CurrentTime = NvOsGetTimeMS();
            TimeElapsed = (CurrentTime >= StartTime)? (CurrentTime - StartTime):
                             MAX_TIME_IN_MS - StartTime + CurrentTime;
            IsWait = (TimeElapsed > WaitTimeOutMS)? NV_FALSE: NV_TRUE;
        }
    }

    Error = (IsTransferComplete)? NvError_Success: NvError_Timeout;
    // If timeout happen then stop all transfer and exit.
    if (Error == NvError_Timeout)
    {
        // Data flow direction Unset
        SpiSetDataFlowDirection(hDdkSpi, NV_FALSE);
        hDdkSpi->CurrentDirection = SerialDataFlow_None;

        // Check again whether the transfer is completed or not.
        IsReady = IsSpiTransferComplete(hDdkSpi);
        if (IsReady)
        {
            // All requested transfer has been done.
            CurrentPacketTransfer = hDdkSpi->CurrTransInfo.CurrPacketCount;
            Error = NvSuccess;
        }
        else
        {
            // Get the transfer word count from status register.
            if (hDdkSpi->CurrentDirection & SerialDataFlow_Rx)
            {
                if ((hDdkSpi->CurrTransInfo.PacketsPerWord > 1))
                    CurrentPacketTransfer -= SpiGetTransferdCount(hDdkSpi) *
                        hDdkSpi->CurrTransInfo.PacketsPerWord;
            }
        }
        hDdkSpi->CurrTransInfo.PacketTransferred += CurrentPacketTransfer;
   }

   return Error;
}

static void
SpiSetChipSelectSignalLevel(
    NvDdkSpiHandle hDdkSpi,
    NvBool IsActive)
{
    NvBool IsHigh;
    NvOdmQuerySpiDeviceInfo *pDevInfo =
                 &hDdkSpi->DeviceInfo[hDdkSpi->ChipSelect];

    if (IsActive)
    {
        if (!hDdkSpi->IsHwChipSelectSupported)
        {
            IsHigh = (pDevInfo->ChipSelectActiveLow)? NV_FALSE: NV_TRUE;
            SpiSetChipSelectLevel(hDdkSpi, hDdkSpi->ChipSelect, IsHigh);
            hDdkSpi->IsCurrentChipSelStateHigh[hDdkSpi->ChipSelect] = IsHigh;
            hDdkSpi->IsChipSelConfigured = NV_TRUE;
        }
        else
        {
            hDdkSpi->IsChipSelConfigured = NV_FALSE;
        }
    }
    else
    {
       IsHigh = (pDevInfo->ChipSelectActiveLow)? NV_TRUE: NV_FALSE;
       SpiSetChipSelectLevel(hDdkSpi,hDdkSpi->ChipSelect, IsHigh);
       if(hDdkSpi->IdleSignalMode != hDdkSpi->CurSignalMode)
           SpiSetSignalMode(hDdkSpi, hDdkSpi->IdleSignalMode);
       hDdkSpi->IsCurrentChipSelStateHigh[hDdkSpi->ChipSelect] = IsHigh;
       hDdkSpi->IsChipSelConfigured = NV_FALSE;
    }
}

static void
MakeSpiBufferFromUserBuffer(
    const NvU8 *pTxBuffer,
    NvU32 *pSpiBuffer,
    NvU32 BytesRequested,
    NvU32 PacketBitLength,
    NvU32 IsPackedMode)
{
    NvU32 Shift0;
    NvU32 MSBMaskData = 0xFF;
    NvU32 BytesPerPacket;
    NvU32 Index;
    NvU32 PacketRequest;
    SPI_INFO_PRINT("%s:BytesRequested = %d\n", __func__,BytesRequested);

    if (IsPackedMode)
    {
        if (PacketBitLength == BITS_PER_BYTE)
        {
            NvOsMemcpy(pSpiBuffer, pTxBuffer, BytesRequested);
            return;
        }

        BytesPerPacket = (PacketBitLength + (BITS_PER_BYTE - 1))/BITS_PER_BYTE;
        PacketRequest = BytesRequested / BytesPerPacket;
        if (PacketBitLength == 16)
        {
            NvU16 *pOutBuffer = (NvU16 *)pSpiBuffer;
             for (Index =0; Index < PacketRequest; ++Index)
             {
                 *pOutBuffer++ = (NvU16)(((*(pTxBuffer  )) << 8) |
                                         ((*(pTxBuffer+1))& 0xFF));
                 pTxBuffer += 2;
             }
            return;
        }
    }

    BytesPerPacket = (PacketBitLength + (BITS_PER_BYTE - 1))/BITS_PER_BYTE;
    PacketRequest = BytesRequested / BytesPerPacket;
    Shift0 = (PacketBitLength & 7);
    if (Shift0)
        MSBMaskData = (0xFF >> (8-Shift0));

    if (BytesPerPacket == 1)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            *pSpiBuffer++ = (NvU32)((*(pTxBuffer))& MSBMaskData);
            pTxBuffer++;
        }
        return;
    }

    if (BytesPerPacket == 2)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            *pSpiBuffer++ = (NvU32)((((*(pTxBuffer))& MSBMaskData) << 8) |
                                    ((*(pTxBuffer+1))));
            pTxBuffer += 2;
        }
        return;
    }

    if (BytesPerPacket == 3)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            *pSpiBuffer++ = (NvU32)((((*(pTxBuffer)) & MSBMaskData) << 16) |
                                    ((*(pTxBuffer+1)) << 8) |
                                    ((*(pTxBuffer+2))));
            pTxBuffer += 3;
        }
        return;
    }

    if (BytesPerPacket == 4)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            *pSpiBuffer++ = (NvU32)((((*(pTxBuffer))& MSBMaskData) << 24) |
                                    ((*(pTxBuffer+1)) << 16) |
                                    ((*(pTxBuffer+2)) << 8) |
                                    ((*(pTxBuffer+3))));
            pTxBuffer += 4;
        }
        return;
    }
}

static void
MakeUserBufferFromSpiBuffer(
    NvU8 *pRxBuffer,
    NvU32 *pSpiBuffer,
    NvU32 BytesRequested,
    NvU32 PacketBitLength,
    NvU32 IsPackedMode)
{
    NvU32 Shift0;
    NvU32 MSBMaskData = 0xFF;
    NvU32 BytesPerPacket;
    NvU32 Index;
    NvU32 RxData;
    NvU32 PacketRequest;
    NvU8 *pOutBuffer = NULL;

   SPI_INFO_PRINT("%s: BytesRequested = %d\n", __func__, BytesRequested);

    if (IsPackedMode)
    {
        if (PacketBitLength == 8)
        {
            NvOsMemcpy(pRxBuffer, pSpiBuffer, BytesRequested);
            return;
        }

        BytesPerPacket = (PacketBitLength + (BITS_PER_BYTE - 1))/BITS_PER_BYTE;
        PacketRequest = BytesRequested / BytesPerPacket;
        if (PacketBitLength == 16)
        {
            pOutBuffer = (NvU8 *)pSpiBuffer;
            for (Index =0; Index < PacketRequest; ++Index)
            {
                *pRxBuffer++  = (NvU8) (*(pOutBuffer+1));
                *pRxBuffer++  = (NvU8) (*(pOutBuffer));
                pOutBuffer += 2;
            }
            return;
        }
    }

    BytesPerPacket = (PacketBitLength + (BITS_PER_BYTE - 1))/BITS_PER_BYTE;
    PacketRequest = BytesRequested / BytesPerPacket;
    Shift0 = (PacketBitLength & 7);
    if (Shift0)
        MSBMaskData = (0xFF >> (8-Shift0));

    if (BytesPerPacket == 1)
    {
      for (Index = 0; Index < PacketRequest; ++Index)
        *pRxBuffer++ = (NvU8)((*pSpiBuffer++) & MSBMaskData);
      return;
    }

    if (BytesPerPacket == 2)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            RxData = *pSpiBuffer++;
            *pRxBuffer++ = (NvU8)((RxData >> 8) & MSBMaskData);
            *pRxBuffer++ = (NvU8)((RxData) & 0xFF);
        }
        return;
    }

    if (BytesPerPacket == 3)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            RxData = *pSpiBuffer++;
            *pRxBuffer++ = (NvU8)((RxData >> 16)& MSBMaskData);
            *pRxBuffer++ = (NvU8)((RxData >> 8)& 0xFF);
            *pRxBuffer++ = (NvU8)((RxData) & 0xFF);
        }
        return;
    }

    if (BytesPerPacket == 4)
    {
        for (Index = 0; Index < PacketRequest; ++Index)
        {
            RxData = *pSpiBuffer++;
            *pRxBuffer++ = (NvU8)((RxData >> 24)& MSBMaskData);
            *pRxBuffer++ = (NvU8)((RxData >> 16)& 0xFF);
            *pRxBuffer++ = (NvU8)((RxData >> 8)& 0xFF);
            *pRxBuffer++ = (NvU8)((RxData) & 0xFF);
        }
        return;
    }
}

static NvError
SpiReadWriteCpu(
    NvDdkSpiHandle hDdkSpi,
    NvU8 *pClientRxBuffer,
    const NvU8 *pClientTxBuffer,
    NvU32 PacketsRequested,
    NvU32 *pPacketsTransferred,
    NvU32 IsPackedMode,
    NvU32 PacketBitLength)
{
    NvError Error = NvSuccess;
    NvU32 CurrentTransWord;
    NvU32 BufferOffset = 0;
    NvU32 WordsWritten;
    NvU32 MaxPacketPerTrans;
    NvU32 CurrentTransPacket;
    NvU32 PacketsPerWord;
    NvU32 MaxPacketTrans;

    SPI_INFO_PRINT("%s: entry PacketsRequested = %d\n", __func__,
        PacketsRequested);

    hDdkSpi->CurrTransInfo.BytesPerPacket = (PacketBitLength + 7)/8;
    PacketsPerWord = (IsPackedMode)? 4/hDdkSpi->CurrTransInfo.BytesPerPacket:1;

    MaxPacketPerTrans = MAX_CPU_TRANSACTION_SIZE_WORD*PacketsPerWord;
    hDdkSpi->CurrTransInfo.TotalPacketsRemaining = PacketsRequested;
    // Set the transfer
    hDdkSpi->CurrTransInfo.PacketsPerWord = PacketsPerWord;
    hDdkSpi->CurrTransInfo.pRxBuff = NULL;
    hDdkSpi->CurrTransInfo.RxPacketsRemaining = 0;
    hDdkSpi->CurrTransInfo.pTxBuff = NULL;
    hDdkSpi->CurrTransInfo.TxPacketsRemaining = 0;

    while (hDdkSpi->CurrTransInfo.TotalPacketsRemaining)
    {

        MaxPacketTrans = NV_MIN(hDdkSpi->CurrTransInfo.TotalPacketsRemaining,
                            MaxPacketPerTrans);
        CurrentTransWord = (MaxPacketTrans)/PacketsPerWord;

        if (!CurrentTransWord)
        {
            PacketsPerWord = 1;
            CurrentTransWord = MaxPacketTrans;
            SpiSetPacketLength(hDdkSpi, PacketBitLength, NV_FALSE);
            hDdkSpi->CurrTransInfo.PacketsPerWord = PacketsPerWord;
            IsPackedMode = NV_FALSE;
        }

        CurrentTransPacket = NV_MIN(MaxPacketTrans,
                                 CurrentTransWord*PacketsPerWord);

        hDdkSpi->RxTransferStatus = NvSuccess;
        hDdkSpi->TxTransferStatus = NvSuccess;
        hDdkSpi->CurrTransInfo.PacketTransferred = 0;

        // Do Read/Write in Polling mode only
        if (pClientRxBuffer)
        {
            hDdkSpi->CurrTransInfo.pRxBuff = hDdkSpi->pRxCpuBuffer;
            hDdkSpi->CurrTransInfo.RxPacketsRemaining = CurrentTransPacket;
        }
        if (pClientTxBuffer)
        {
            MakeSpiBufferFromUserBuffer(pClientTxBuffer + BufferOffset,
                hDdkSpi->pTxCpuBuffer,
                CurrentTransPacket*hDdkSpi->CurrTransInfo.BytesPerPacket,
                PacketBitLength, IsPackedMode);
            WordsWritten = SpiWriteInTransmitFifo(hDdkSpi,
                               hDdkSpi->pTxCpuBuffer, CurrentTransWord);
            hDdkSpi->CurrTransInfo.CurrPacketCount =
                NV_MIN(WordsWritten*PacketsPerWord, CurrentTransPacket);
            hDdkSpi->CurrTransInfo.pTxBuff =
                hDdkSpi->pTxCpuBuffer + WordsWritten;
            hDdkSpi->CurrTransInfo.TxPacketsRemaining = CurrentTransPacket -
                hDdkSpi->CurrTransInfo.CurrPacketCount;
        }
        else
        {
            hDdkSpi->CurrTransInfo.CurrPacketCount =
              NV_MIN(MAX_CPU_TRANSACTION_SIZE_WORD*PacketsPerWord,
              CurrentTransPacket);
        }
        if (!hDdkSpi->IsChipSelConfigured)
        {
            SpiSetChipSelectLevelBasedOnPacket(hDdkSpi,
                hDdkSpi->DeviceInfo[hDdkSpi->ChipSelect].ChipSelectActiveLow,
                CurrentTransPacket, PacketsPerWord);
            hDdkSpi->IsChipSelConfigured = NV_TRUE;
        }
        SpiSetDmaTransferSize(hDdkSpi, hDdkSpi->CurrTransInfo.CurrPacketCount);

        // start transfer
        SpiStartTransfer(hDdkSpi, NV_TRUE);

        // Wait for transfer
        Error = SpiWaitForTransferComplete(hDdkSpi, NV_WAIT_INFINITE);

        if (Error)
            break;

        if (pClientRxBuffer)
        {
            MakeUserBufferFromSpiBuffer(pClientRxBuffer + BufferOffset,
                hDdkSpi->pRxCpuBuffer,
                CurrentTransPacket*hDdkSpi->CurrTransInfo.BytesPerPacket,
                PacketBitLength, IsPackedMode);
        }

        BufferOffset += CurrentTransPacket *
                            hDdkSpi->CurrTransInfo.BytesPerPacket;
        hDdkSpi->CurrTransInfo.TotalPacketsRemaining -= CurrentTransPacket;
    }

    *pPacketsTransferred = PacketsRequested -
         hDdkSpi->CurrTransInfo.TotalPacketsRemaining;

     return Error;
}


NvDdkSpiHandle NvDdkSpiOpen(NvU32 Instance)
{
    NvDdkSpiHandle hODdkSpi;

    SPI_INFO_PRINT("%s: Instance = %d\n", __func__, Instance);
    if (s_hDdkSpiHandle[Instance] == NULL)
    {
        hODdkSpi = (NvDdkSpiHandle) NvOsAlloc(sizeof(NvDdkSpi));
        if (hODdkSpi)
        {
            if (PrivSpiInit(hODdkSpi, Instance))
            {
                s_hDdkSpiHandle[Instance] = hODdkSpi;
                return hODdkSpi;
            }
        }
        goto free;
    }
    else
    {
        s_hDdkSpiHandle[Instance]->OpenCount++;
        return s_hDdkSpiHandle[Instance];
    }
free:
    NvOsFree(hODdkSpi);
    return NULL;
}

void NvDdkSpiClose(NvDdkSpiHandle hDdkSpi)
{
    if (hDdkSpi)
    {
        SPI_INFO_PRINT("%s: Instance = %d\n", __func__, hDdkSpi->Instance);
        hDdkSpi->OpenCount--;
        if (!hDdkSpi->OpenCount)
        {
            // Disable CLK
            SpiClockEnable(hDdkSpi->Instance, NV_FALSE);
            s_hDdkSpiHandle[hDdkSpi->Instance] = NULL;
            if (hDdkSpi->pTxCpuBuffer)
                NvOsFree(hDdkSpi->pTxCpuBuffer);
            if (hDdkSpi->pRxCpuBuffer)
                NvOsFree(hDdkSpi->pRxCpuBuffer);
            NvOsFree(hDdkSpi);
        }
    }
}

void
NvDdkSpiTransaction(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect,
    NvU32 ClockSpeedInKHz,
    NvU8 *ReadBuf,
    const NvU8 *WriteBuf,
    NvU32 BytesRequested,
    NvU32 PacketSizeInBits)
{
    NvError Error = NvSuccess;
    NvBool IsPackedMode;
    NvU32 BytesPerPacket;
    NvU32 PacketsTransferred;
    NvU32 PacketsPerWord;
    NvU32 TotalPacketsRequsted;
    NvU32 TotalWordsRequested;

    SPI_INFO_PRINT("%s: ChipSelect = %d, ClockSpeedInKHz = %d,\
        BytesRequested = %d, PacketSizeInBits = %d\n", __func__,
        ChipSelect, ClockSpeedInKHz, BytesRequested, PacketSizeInBits);

    // Sanity checks
    NV_ASSERT(hDdkSpi);
    NV_ASSERT( ReadBuf || WriteBuf);
    NV_ASSERT(ClockSpeedInKHz > 0);
    NV_ASSERT((PacketSizeInBits > 0) && (PacketSizeInBits <= 32));
    // Chip select should be supported by the odm.
    NV_ASSERT(hDdkSpi->IsChipSelSupported[ChipSelect]);

    // Select Packed mode for the 8/16 bit length.
    BytesPerPacket = (PacketSizeInBits + 7)/8;
    TotalPacketsRequsted = BytesRequested/BytesPerPacket;
    IsPackedMode = ((PacketSizeInBits == 8) || ((PacketSizeInBits == 16)));
    PacketsPerWord =  (IsPackedMode)? 4/BytesPerPacket: 1;
    TotalWordsRequested = (TotalPacketsRequsted + PacketsPerWord -1) /
                             PacketsPerWord;
    NV_ASSERT((BytesRequested % BytesPerPacket) == 0);
    NV_ASSERT(TotalPacketsRequsted);

    // Set current chip select Id
    hDdkSpi->ChipSelect = ChipSelect;
    if (hDdkSpi->DeviceInfo[ChipSelect].CanUseHwBasedCs)
        hDdkSpi->IsHwChipSelectSupported = NV_TRUE;
    else
        hDdkSpi->IsHwChipSelectSupported = NV_FALSE;

    if(hDdkSpi->ClockFreqInKHz != ClockSpeedInKHz)
    {
        // set the clock speed
        SpiSetClockSpeed(hDdkSpi->Instance, ClockSpeedInKHz);
        hDdkSpi->ClockFreqInKHz = ClockSpeedInKHz;
    }

    // set the chip select
    SpiSetChipSelectSignalLevel(hDdkSpi,NV_TRUE);

    // Set the data flow direction
    hDdkSpi->CurrentDirection = SerialDataFlow_None;
    if (ReadBuf)
        hDdkSpi->CurrentDirection |= SerialDataFlow_Rx;
    if (WriteBuf)
        hDdkSpi->CurrentDirection |= SerialDataFlow_Tx;
    SpiSetDataFlowDirection(hDdkSpi, NV_TRUE);
    // Set the packet length
    SpiSetPacketLength(hDdkSpi,PacketSizeInBits, IsPackedMode);
    // Do CPU transfer for data
    Error = SpiReadWriteCpu(hDdkSpi, ReadBuf, WriteBuf,
        TotalPacketsRequsted, &PacketsTransferred,
        IsPackedMode,PacketSizeInBits);

    if (Error)
       goto cleanup;

cleanup:
    // Data flow direction Unset
    SpiSetDataFlowDirection(hDdkSpi, NV_FALSE);
    hDdkSpi->CurrentDirection = SerialDataFlow_None;
    // Release the chip select
    SpiSetChipSelectSignalLevel(hDdkSpi, NV_FALSE);
}
