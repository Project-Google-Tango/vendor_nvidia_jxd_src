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
#include "nvodm_query.h"
#include "nvodm_query_discovery.h"
#include "artimerus.h"
#include "arclk_rst.h"
#include "arapbpm.h"
#include "nvddk_arspi.h"
#include "nvddk_spi.h"
#include "nvddk_spi_hw.h"
#include "nvddk_spi_debug.h"
#include "nvrm_drf.h"
#include "nvddk_spi_soc.h"

extern NvDdkSpiHandle s_hDdkSpiHandle[MAX_SPI_INSTANCES];

static NvBool
SpiGetDeviceInfo(
    NvU32 Instance,
    NvU32 ChipSelect,
    NvOdmQuerySpiDeviceInfo *pDeviceInfo)
{
    const NvOdmQuerySpiDeviceInfo *pSpiDevInfo = NULL;
    pSpiDevInfo = NvOdmQuerySpiGetDeviceInfo(NvOdmIoModule_Spi, Instance,
                      ChipSelect);
    if (!pSpiDevInfo)
    {
        // No device info in odm, so set it on default info
        pDeviceInfo->SignalMode = NvOdmQuerySpiSignalMode_0;
        pDeviceInfo->ChipSelectActiveLow = NV_TRUE;
        pDeviceInfo->CanUseHwBasedCs = NV_FALSE;
        pDeviceInfo->CsHoldTimeInClock = 0;
        pDeviceInfo->CsSetupTimeInClock = 0;
        return NV_FALSE;
    }
    pDeviceInfo->SignalMode = pSpiDevInfo->SignalMode;
    pDeviceInfo->ChipSelectActiveLow = pSpiDevInfo->ChipSelectActiveLow;
    pDeviceInfo->CanUseHwBasedCs = pSpiDevInfo->CanUseHwBasedCs;
    pDeviceInfo->CsHoldTimeInClock = pSpiDevInfo->CsHoldTimeInClock;
    pDeviceInfo->CsSetupTimeInClock = pSpiDevInfo->CsSetupTimeInClock;
    return NV_TRUE;
}

static void
SpiSetChipSelectPolarity(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect,
    NvBool IsHigh)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);

    // Set the chip select level.
    if (IsHigh)
        RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND, CS_SW_VAL, 1, RegValue);
    else
        RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND, CS_SW_VAL, 0, RegValue);

    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);
}

static void
SpiSetChipSelectNumber(
    NvDdkSpiHandle hDdkSpi)
{
    NvU32 Value = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);

    switch (hDdkSpi->ChipSelect)
    {
        case 0:
            Value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SEL, CS0, Value);
            break;
        case 1:
            Value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SEL, CS1, Value);
            break;
        case 2:
            Value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SEL, CS2, Value);
            break;
        case 3:
            Value = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SEL, CS3, Value);
            break;
        default:
            NV_ASSERT(!"Invalid ChipSelect");
            break;
    }
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, Value);
}

static void
SpiWaitForSetUpTime(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect)
{
    NvU32 SetupTime;
    NvU32 RegValue;

    if (hDdkSpi->IsHwChipSelectSupported)
    {
        // Rounding the  clock time to the cycles
        SetupTime = (hDdkSpi->DeviceInfo[ChipSelect].CsSetupTimeInClock +1) / 2;
        SetupTime = (SetupTime > MAX_INACTIVE_CHIPSELECT_SETUP_CYCLES)? \
                        MAX_INACTIVE_CHIPSELECT_SETUP_CYCLES: SetupTime;
        RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, TIMING_REG1);

        switch (hDdkSpi->ChipSelect)
        {
            case 0:
                RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG1, CS_SETUP_0,
                               SetupTime, RegValue);
                break;

            case 1:
                RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG1, CS_SETUP_1,
                               SetupTime, RegValue);
                break;

            case 2:
                RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG1, CS_SETUP_2,
                               SetupTime, RegValue);
                break;

            case 3:
                RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG1, CS_SETUP_3,
                               SetupTime, RegValue);
                break;

            default:
                NV_ASSERT(!"Invalid ChipSelect");
                break;
        }

        SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, TIMING_REG1, RegValue);
    }
}

static void
SpiSetChipSelectSw(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect)
{
    NvU32 RegValue;
    RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);

    // Initialize the chip select bits to select the s/w only
    RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND,  CS_SW_HW, SOFTWARE,
                     RegValue);

    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);
}

static void
SpiSetChipSelectHw(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect,
    NvU32 RefillCount)
{
    NvU32 RegValue;
    RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);
    RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, CS_SW_HW, HARDWARE,
                     RegValue);
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);

    SpiSetChipSelectNumber(hDdkSpi);
    SpiWaitForSetUpTime(hDdkSpi, hDdkSpi->ChipSelect);

    RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, TIMING_REG2);

    switch (hDdkSpi->ChipSelect)
    {
        case 0:
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CYCLES_BETWEEN_PACKETS_0, 0, RegValue);
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CS_ACTIVE_BETWEEN_PACKETS_0, 1, RegValue);
             break;
        case 1:
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CYCLES_BETWEEN_PACKETS_1, 0, RegValue);
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CS_ACTIVE_BETWEEN_PACKETS_1, 1, RegValue);
            break;
        case 2:
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CYCLES_BETWEEN_PACKETS_2, 0, RegValue);
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CS_ACTIVE_BETWEEN_PACKETS_2, 1, RegValue);
            break;

        case 3:
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CYCLES_BETWEEN_PACKETS_3, 0, RegValue);
            RegValue = NV_FLD_SET_DRF_NUM(SPI, TIMING_REG2,
                           CS_ACTIVE_BETWEEN_PACKETS_3, 1, RegValue);
            break;
        default:
            NV_ASSERT(!"Invalid ChipSelect");
            break;
    }
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, TIMING_REG2, RegValue);
}


void
SpiSetChipSelectLevel(
    NvDdkSpiHandle hDdkSpi,
    NvU32 ChipSelect,
    NvBool IsHigh)
{
    SPI_INFO_PRINT("%s: IsHigh = %d\n", __func__, IsHigh);
    SpiSetChipSelectNumber(hDdkSpi);
    SpiSetChipSelectSw(hDdkSpi, ChipSelect);
    SpiSetChipSelectPolarity(hDdkSpi, ChipSelect, IsHigh);
}

void
SpiSetSignalMode(
    NvDdkSpiHandle hDdkSpi,
    NvOdmQuerySpiSignalMode SpiSignalMode)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);
    switch (SpiSignalMode)
    {
        case NvOdmQuerySpiSignalMode_0:
            RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, MODE, Mode0, RegValue);
            SPI_INFO_PRINT("Mode 0\n");
            break;
        case NvOdmQuerySpiSignalMode_1:
            RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, MODE, Mode1, RegValue);
            SPI_INFO_PRINT("Mode 1\n");
            break;
        case NvOdmQuerySpiSignalMode_2:
            RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, MODE, Mode2, RegValue);
            SPI_INFO_PRINT("Mode 2\n");
            break;
        case NvOdmQuerySpiSignalMode_3:
            RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, MODE, Mode3, RegValue);
            SPI_INFO_PRINT("Mode 3\n");
            break;
        default:
            NV_ASSERT(!"Invalid SignalMode");
    }
    hDdkSpi->CurSignalMode = SpiSignalMode;
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);
}

void
SpiSetPacketLength(
    NvDdkSpiHandle hDdkSpi,
    NvU32 PacketLength,
    NvBool IsPackedMode)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);

    RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND,  BIT_LENGTH, (PacketLength -1),
                   RegValue);
    if (IsPackedMode)
        RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND,  PACKED, ENABLE,
                          RegValue);
    else
        RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND,  PACKED, DISABLE,
                          RegValue);

    // Set the Packet Length bits minus 1
    RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND, BIT_LENGTH, (PacketLength - 1),
                   RegValue);

    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);

    hDdkSpi->IsPackedMode = IsPackedMode;
}

void
SpiSetDataFlowDirection(
    NvDdkSpiHandle hDdkSpi,
    NvBool IsEnable)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);
    if (hDdkSpi->CurrentDirection & SerialDataFlow_Rx)
    {
        if (IsEnable)
             RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Rx_EN, ENABLE,
                            RegValue);
        else
             RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Rx_EN, DISABLE,
                            RegValue);
    }

    if (hDdkSpi->CurrentDirection & SerialDataFlow_Tx)
    {
         if (IsEnable)
            RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, ENABLE,
                           RegValue);
         else
             RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, Tx_EN, DISABLE,
                            RegValue);
    }
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);
}

NvBool
IsSpiTransferComplete( NvDdkSpiHandle hDdkSpi)
{
    // Read the Status register
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd,
                        TRANSFER_STATUS);
    SPI_INFO_PRINT("%s: RegValue = %x\n", __func__, RegValue);

    if (!(RegValue & NV_DRF_DEF(SPI, TRANSFER_STATUS, RDY, READY)))
         return NV_FALSE;

    // Make ready clear to 1.
    RegValue = NV_FLD_SET_DRF_NUM(SPI, TRANSFER_STATUS, RDY, 1, RegValue);
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, TRANSFER_STATUS, RegValue);

    RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);

    // Transfer is completed so clear the ready bit by write 1 to clear
    // Clear all the write 1 on clear status.
    RegValue &= ~(NV_DRF_NUM(SPI, FIFO_STATUS, RX_FIFO_UNF, 1) |
                      NV_DRF_NUM(SPI, FIFO_STATUS, TX_FIFO_OVF, 1));

    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, FIFO_STATUS, RegValue);

    return NV_TRUE;
}

NvError
SpiGetTransferStatus(
    NvDdkSpiHandle hDdkSpi,
    SerialDataFlow DataFlow)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);
    // Check for the receive error
    if (DataFlow & SerialDataFlow_Rx)
    {
        if (RegValue & NV_DRF_NUM(SPI, FIFO_STATUS, RX_FIFO_UNF, 1))
            return NvError_SpiReceiveError;
    }
    // Check for the transmit error
    if (DataFlow & SerialDataFlow_Tx)
    {
        if (RegValue & NV_DRF_NUM(SPI, FIFO_STATUS, TX_FIFO_OVF, 1))
            return NvError_SpiTransmitError;
    }
    return NvSuccess;
}

void
SpiClearTransferStatus(
    NvDdkSpiHandle hDdkSpi,
    SerialDataFlow DataFlow)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);

    // Clear all  status bits.
    RegValue &= ~(NV_DRF_NUM(SPI, FIFO_STATUS, RX_FIFO_UNF, 1) |
          NV_DRF_NUM(SPI, FIFO_STATUS,  TX_FIFO_OVF, 1));

    // Check for the receive error
    if (DataFlow & SerialDataFlow_Rx)
    {
        RegValue |= NV_DRF_NUM(SPI, FIFO_STATUS, RX_FIFO_UNF, 1);
    }

    // Check for the transmit error
    if (DataFlow & SerialDataFlow_Tx)
    {
        RegValue |= NV_DRF_NUM(SPI, FIFO_STATUS, TX_FIFO_OVF, 1);

    }
    // Write on slink status register.
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, FIFO_STATUS, RegValue);

    RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, TRANSFER_STATUS);

    // Make ready clear to 1.
    RegValue = NV_FLD_SET_DRF_NUM(SPI, TRANSFER_STATUS, RDY, 1, RegValue);

    // Write on slink status register.
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, TRANSFER_STATUS, RegValue);
}

NvU32
SpiGetTransferdCount(NvDdkSpiHandle hDdkSpi)
{
    NvU32 DmaBlockSize;
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, DMA_BLK_SIZE);
    DmaBlockSize = NV_DRF_VAL(SPI, DMA_BLK_SIZE, DMA_BLOCK_SIZE, RegValue);
    return (DmaBlockSize +1);
}

void
SpiSetChipSelectLevelBasedOnPacket(
    NvDdkSpiHandle hDdkSpi,
    NvBool IsHigh,
    NvU32 PacketRequested,
    NvU32 PacketPerWord)
{
    NvU32 MaxWordReq;
    NvU32 RefillCount = 0;
    NvBool UseSWBaseCS;

    MaxWordReq = (PacketRequested + PacketPerWord -1)/PacketPerWord;
    UseSWBaseCS = (MaxWordReq <= MAX_CPU_TRANSACTION_SIZE_WORD) ?
                      NV_FALSE : NV_TRUE;
    if (UseSWBaseCS)
    {
        SpiSetChipSelectLevel(hDdkSpi, hDdkSpi->ChipSelect, IsHigh);
    }
    else
    {
        RefillCount = (MaxWordReq) / MAX_SPI_FIFO_DEPTH;
        SpiSetChipSelectHw(hDdkSpi, hDdkSpi->ChipSelect, RefillCount);
    }
}


void
SpiFlushFifos(NvDdkSpiHandle hDdkSpi)
{
    NvU32 ResetBits = 0;
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);

    ResetBits = NV_DRF_NUM(SPI, FIFO_STATUS, TX_FIFO_FLUSH, 1);
    ResetBits |= NV_DRF_NUM(SPI, FIFO_STATUS, RX_FIFO_FLUSH, 1);

    RegValue |= ResetBits;
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, FIFO_STATUS, RegValue);

    // Now wait till the flush bits become 0
    do
    {
        RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);
    } while (RegValue & ResetBits);
}

NvBool
PrivSpiInit(
    NvDdkSpiHandle hDdkSpi,
    NvU32 Instance)
{
    NvU32 Index;
    NvU32 RegValue;
    const NvOdmQuerySpiIdleSignalState *pSignalState = NULL;

    hDdkSpi->Instance = Instance;
    hDdkSpi->ClockFreqInKHz = 0;
    SPI_INFO_PRINT("%s: Instance = %d, ChipSelect = %d\n", __func__,
        hDdkSpi->Instance, hDdkSpi->ChipSelect);

    for (Index = 0; Index < MAX_CHIPSELECT_PER_INSTANCE; Index++)
    {
        hDdkSpi->IsChipSelSupported[Index] = SpiGetDeviceInfo(
            hDdkSpi->Instance, Index, &hDdkSpi->DeviceInfo[Index]);
    }
    SpiClockEnable(Instance, NV_TRUE);
    SpiModuleReset(Instance, NV_TRUE);
    NvOsWaitUS(HW_WAIT_TIME_IN_US);
    SpiModuleReset(Instance, NV_FALSE);

    // assign reg base address
    if (hDdkSpi->Instance < SPI_INSTANCE_COUNT)
    {
        hDdkSpi->pRegBaseAdd = (NvU32 *)SpiBaseAddress[hDdkSpi->Instance];
    }
    else
    {
        SPI_ERROR_PRINT("Invalid Spi Instance\n");
        return NV_FALSE;
    }

    pSignalState = NvOdmQuerySpiGetIdleSignalState(NvOdmIoModule_Spi, Instance);
    if (pSignalState)
    {
        hDdkSpi->IsIdleSignalTristate = pSignalState->IsTristate;
        hDdkSpi->IdleSignalMode = pSignalState->SignalMode;
        hDdkSpi->IsIdleDataOutHigh = pSignalState->IsIdleDataOutHigh;
    }
    else
    {
        hDdkSpi->IsIdleSignalTristate = NV_FALSE;
        hDdkSpi->IdleSignalMode = NvOdmQuerySpiSignalMode_0;
        hDdkSpi->IsIdleDataOutHigh = NV_FALSE;
    }

    // Set the CPU Buffers
    hDdkSpi->pRxCpuBuffer = (NvU32 *)NvOsAlloc(MAX_CPU_TRANSACTION_SIZE_WORD*4);
    if (!hDdkSpi->pRxCpuBuffer)
        return NV_FALSE;

    hDdkSpi->pTxCpuBuffer = (NvU32 *)NvOsAlloc(MAX_CPU_TRANSACTION_SIZE_WORD*4);
    if (!hDdkSpi->pTxCpuBuffer)
        return NV_FALSE;

    RegValue = NV_RESETVAL(SPI, COMMAND);

    // Initialize the chip select bits to select the s/w only
    RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND, CS_SW_HW, 1, RegValue);
    RegValue = NV_FLD_SET_DRF_NUM(SPI, COMMAND, CS_SW_VAL, 1, RegValue);

    if (hDdkSpi->IsIdleDataOutHigh)
        RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND,  IDLE_SDA,
                         DRIVE_HIGH, RegValue);
    else
         RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND,  IDLE_SDA,
                          DRIVE_LOW, RegValue);
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);

    // TapValue
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND2, 0x2e0);

    hDdkSpi->RxTransferStatus = NvSuccess;
    hDdkSpi->TxTransferStatus = NvSuccess;
    hDdkSpi->OpenCount = 1;

    // Set the default signal mode of the spi channel.
    SpiSetSignalMode(hDdkSpi, hDdkSpi->IdleSignalMode);

    // Set chip select to non active state.
    for (Index = 0; Index < MAX_CHIPSELECT_PER_INSTANCE; Index++)
    {
        hDdkSpi->IsCurrentChipSelStateHigh[Index] = NV_TRUE;
        if (hDdkSpi->IsChipSelSupported[Index])
        {
            hDdkSpi->IsCurrentChipSelStateHigh[Index] =
                hDdkSpi->DeviceInfo[Index].ChipSelectActiveLow;
            SpiSetChipSelectPolarity(hDdkSpi, Index,
                hDdkSpi->IsCurrentChipSelStateHigh[Index]);
         }
    }
    s_hDdkSpiHandle[Instance] = hDdkSpi;
    SpiFlushFifos(hDdkSpi);

    return NV_TRUE;
}

NvU32
SpiWriteInTransmitFifo(
    NvDdkSpiHandle hDdkSpi,
    NvU32 *pTxBuff,
    NvU32 WordRequested)
{
    NvU32 WordWritten = 0;
    NvU32 WordsRemaining;
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);
    RegValue = NV_DRF_VAL(SPI, FIFO_STATUS, TX_FIFO_EMPTY_COUNT, RegValue);
    WordsRemaining = NV_MIN(WordRequested, RegValue);

    WordWritten = WordsRemaining;
    while (WordsRemaining)
    {
        SPI_DEBUG_PRINT("%x\n", *pTxBuff);
        SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, TX_FIFO, *pTxBuff);
        pTxBuff++;
        WordsRemaining--;
    }
    return WordWritten;
}

NvU32
SpiReadFromReceiveFifo(
    NvDdkSpiHandle hDdkSpi,
    NvU32 *pRxBuff,
    NvU32 WordRequested)
{
    NvU32 WordsRemaining;
    NvU32 WordsRead;
    NvU32 RegValue =  SPI_REG_READ32(hDdkSpi->pRegBaseAdd, FIFO_STATUS);
    RegValue = NV_DRF_VAL(SPI, FIFO_STATUS, RX_FIFO_FULL_COUNT, RegValue);
    WordsRemaining = NV_MIN(WordRequested, RegValue);
    WordsRead = WordsRemaining;
    while (WordsRemaining)
    {
        *pRxBuff = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, RX_FIFO);
        SPI_DEBUG_PRINT("%x\n", *pRxBuff);
        pRxBuff++;
        WordsRemaining--;

    }
    return WordsRead;
}

void
SpiStartTransfer(
    NvDdkSpiHandle hDdkSpi,
    NvBool IsEnable)
{
    NvU32 RegValue;
#ifdef SPI_DMA_EN
    RegValue =  SPI_REG_READ32(hDdkSpi->pRegBaseAdd, DMA_CTL);
    if (IsEnable)
        RegValue = NV_FLD_SET_DRF_DEF(SPI, DMA_CTL, DMA_EN, ENABLE, RegValue);
    else
        RegValue = NV_FLD_SET_DRF_DEF(SPI, DMA_CTL, DMA_EN, DISABLE, RegValue);
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, DMA_CTL, RegValue);
#else
    RegValue =  SPI_REG_READ32(hDdkSpi->pRegBaseAdd, COMMAND);
    if (IsEnable)
        RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PIO, PIO, RegValue);
    else
        RegValue = NV_FLD_SET_DRF_DEF(SPI, COMMAND, PIO, PIO, RegValue);
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, COMMAND, RegValue);
#endif
}

void
SpiSetDmaTransferSize(
    NvDdkSpiHandle hDdkSpi,
    NvU32 DmaBlockSize)
{
    NvU32 RegValue = SPI_REG_READ32(hDdkSpi->pRegBaseAdd, DMA_BLK_SIZE);
    SPI_INFO_PRINT("%s: DMA block size = %d", __func__, DmaBlockSize);
    // Set the DMA size for Transmit and Enable DMA in packed mode
    RegValue = NV_DRF_NUM(SPI, DMA_BLK_SIZE, DMA_BLOCK_SIZE,
                        (DmaBlockSize-1));
    SPI_REG_WRITE32(hDdkSpi->pRegBaseAdd, DMA_BLK_SIZE, RegValue);
}

