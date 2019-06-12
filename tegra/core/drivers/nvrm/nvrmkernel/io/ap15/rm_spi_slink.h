/*
 * Copyright (c) 2009 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_RM_SPI_SLINK_H
#define INCLUDED_RM_SPI_SLINK_H

#include "nvrm_spi.h"

typedef struct NvRmSpiTransactionInfoRec
{
    NvU8 *rxBuffer;
    NvU8 *txBuffer;
    NvU32 len;
} NvRmSpiTransactionInfo;


/* Private API to do multiple trasfers keeping the chipselect active. This API 
 * is not yet made public as the NVIDL doesn't support marshalling 
 * scatter/gather buffers. See bug 532966 
 * */
void NvRmSpiMultipleTransactions(
    NvRmSpiHandle hRmSpi,
    NvU32 SpiPinMap,
    NvU32 ChipSelectId,
    NvU32 ClockSpeedInKHz,
    NvU32 PacketSizeInBits,
    NvRmSpiTransactionInfo *t,
    NvU32 NumOfTransactions);

#endif // INCLUDED_RM_SPI_SLINK_H

