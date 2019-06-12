/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef NVDDK_SPI_HW_H
#define NVDDK_SPI_HW_H

#include "nvcommon.h"
#include "nvassert.h"
#include "artimerus.h"
#include "arclk_rst.h"
#include "arapbpm.h"
#include "nvddk_arspi.h"
#include "nvddk_spi.h"


#define NV_ADDR_BASE_TIMER                       0x60005000
#define NV_ADDR_BASE_CLK_RST                     0x60006000
#define NV_ADDR_BASE_MISC                        0x70000000
#define NV_ADDR_BASE_PMC                         0x7000E400
#define NV_ADDR_BASE_SPI1                        0x7000D400
#define NV_ADDR_BASE_SPI2                        0x7000D600
#define NV_ADDR_BASE_SPI3                        0x7000D800
#define NV_ADDR_BASE_SPI4                        0x7000DA00
#define NV_ADDR_BASE_SPI5                        0x7000DC00
#define NV_ADDR_BASE_SPI6                        0x7000DE00

// Maximum SPI instance number
#define MAX_SPI_INSTANCES               6

// Maximum chipselect available for per spi/slink channel.
#define MAX_CHIPSELECT_PER_INSTANCE     4

// Maximum buffer size when transferring the data using the cpu.
enum {MAX_CPU_TRANSACTION_SIZE_WORD  = 0x40};  // 256 bytes

// Maximum Slink Fifo Width
enum {MAX_SPI_FIFO_DEPTH =  64};

// Maximum  PLL_P clock frequncy in KHZ
#define MAX_PLLP_CLOCK_FREQUENY_INKHZ 408000

// Maximum number of cycles for chip select should stay inactive
#define MAX_INACTIVE_CHIPSELECT_SETUP_CYCLES 3

// Hardware wait time after write
#define HW_WAIT_TIME_IN_US 10

// Number of bits per byte
#define BITS_PER_BYTE  8

// Maximum number which  is return by the NvOsGetTimeMS()
#define MAX_TIME_IN_MS 0xFFFFFFFF

#define NV_READ32(a)                            *((volatile NvU32 *)(a))
#define NV_WRITE32(a,d)                         *((volatile NvU32 *)(a)) = (d)

// Wrapper macros for reading/writing from/to SPI registers
#define SPI_REG_READ32(pBaseAdd, reg) \
    NV_READ32((pBaseAdd) + ((SPI_##reg##_0)/4))

#define SPI_REG_WRITE32(pBaseAdd, reg, val) \
    do { \
        NV_WRITE32((((pBaseAdd) + ((SPI_##reg##_0)/4))), (val)); \
    } while (0)

#define NV_CLK_RST_READ(reg, value)                                     \
    do                                                                  \
    {                                                                   \
        value = NV_READ32(NV_ADDR_BASE_CLK_RST              \
                          + CLK_RST_CONTROLLER_##reg##_0);              \
    } while (0)

#define NV_CLK_RST_WRITE(reg, value)                                    \
    do                                                                  \
    {                                                                   \
        NV_WRITE32((NV_ADDR_BASE_CLK_RST                    \
                    + CLK_RST_CONTROLLER_##reg##_0), value);            \
    } while (0)


void SpiClockEnable(NvU32 InstanceId, NvBool Enable);

void SpiSetClockSpeed(NvU32 Instance, NvU32 ClockSpeedInKHz);

void SpiModuleReset(NvU32 InstanceId, NvBool Enable);

void
SpiSetSignalMode(
    NvDdkSpiHandle hOdmSpi,
    NvOdmQuerySpiSignalMode SpiSignalMode);

void
SpiSetChipSelectLevel(
    NvDdkSpiHandle hOdmSpi,
    NvU32 ChipSelectId,
    NvBool IsHigh);

void
SpiSetChipSelectLevelBasedOnPacket(
    NvDdkSpiHandle hOdmSpi,
    NvBool IsHigh,
    NvU32 PacketRequested,
    NvU32 PacketPerWord);

void
SpiSetPacketLength(
    NvDdkSpiHandle hOdmSpi,
    NvU32 PacketLength,
    NvBool IsPackedMode);

void SpiSetDmaTransferSize(NvDdkSpiHandle hOdmSpi, NvU32 DmaBlockSize);

void SpiSetDataFlowDirection(NvDdkSpiHandle hOdmSpi, NvBool IsEnable);

NvBool IsSpiTransferComplete(NvDdkSpiHandle hOdmSpi);

void SpiStartTransfer(NvDdkSpiHandle hOdmSpi, NvBool IsEnable);

NvError SpiGetTransferStatus(NvDdkSpiHandle hOdmSpi, SerialDataFlow DataFlow);

void SpiClearTransferStatus(NvDdkSpiHandle hOdmSpi, SerialDataFlow DataFlow);

void SpiFlushFifos(NvDdkSpiHandle hOdmSpi);

NvBool PrivSpiInit(NvDdkSpiHandle hOdmSpi, NvU32 InstanceId);

NvU32 SpiWriteInTransmitFifo(
    NvDdkSpiHandle hOdmSpi,
    NvU32 *pTxBuff,
    NvU32 WordRequested);

NvU32
SpiReadFromReceiveFifo(
    NvDdkSpiHandle hOdmSpi,
    NvU32 *pRxBuff,
    NvU32 WordRequested);

NvU32 SpiGetTransferdCount(NvDdkSpiHandle hOdmSpi);

#endif // NVDDK_SPI_HW_H

