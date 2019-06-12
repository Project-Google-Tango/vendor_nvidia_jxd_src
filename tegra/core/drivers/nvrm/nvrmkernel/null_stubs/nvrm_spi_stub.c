
#define NV_IDL_IS_STUB

/*
 * Copyright (c) 2009-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_spi.h"

void NvRmSpiSetSignalMode(
    NvRmSpiHandle hRmSpi,
    NvU32 ChipSelectId,
    NvU32 SpiSignalMode)
{
}

NvError NvRmSpiGetTransactionData(
    NvRmSpiHandle hRmSpi,
    NvU8 *pReadBuffer,
    NvU32 BytesRequested,
    NvU32 *pBytesTransfererd,
    NvU32 WaitTimeout)
{
    return NvError_NotSupported;
}

NvError NvRmSpiStartTransaction(
    NvRmSpiHandle hRmSpi,
    NvU32 ChipSelectId,
    NvU32 ClockSpeedInKHz,
    NvBool IsReadTransfer,
    NvU8 *pWriteBuffer,
    NvU32 BytesRequested,
    NvU32 PacketSizeInBits)
{
    return NvError_NotSupported;
}

void NvRmSpiTransaction(
    NvRmSpiHandle hRmSpi,
    NvU32 SpiPinMap,
    NvU32 ChipSelectId,
    NvU32 ClockSpeedInKHz,
    NvU8 *pReadBuffer,
    NvU8 *pWriteBuffer,
    NvU32 BytesRequested,
    NvU32 PacketSizeInBits)
{
}

void NvRmSpiClose(NvRmSpiHandle hRmSpi)
{
}

NvError NvRmSpiOpen(
    NvRmDeviceHandle hRmDevice,
    NvU32 IoModule,
    NvU32 InstanceId,
    NvBool IsMasterMode,
    NvRmSpiHandle * phRmSpi)
{
    return NvError_NotSupported;
}
