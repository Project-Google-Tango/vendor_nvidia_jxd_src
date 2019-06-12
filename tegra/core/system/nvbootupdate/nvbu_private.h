/*
 * Copyright (c) 2009 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

 /**
 * @file nvbu_private.h
 * @brief <b> Nv Boot update Management Framework.</b>
 *
 * @b Description: This file declares chip specifc extention API's for for updating BCT's and
 * boot loaders (BL's).
 */

#ifndef INCLUDED_NVBU_PRIVATE_H
#define INCLUDED_NVBU_PRIVATE_H

#include "nvbu.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Some blocks in the partition at the end may be uninitialized,
 * it finds the last written block in the Bootloader partition.
 * Once last logical block written is found the remaining
 * blocks in partition assumed as good. Adding the remaining
 * logical blocks in partition to last written physical block
 * is taken as the End Physical address of partition.
 * Failure to map logical to physical is interpreted as finding a block
 * which is the first unused block of partition.
 *
 * @param partId PartitonId for the block
 * @param StartLogicalSector starting logical sector number
 * @param NumLogicalSectors number of the logical sectors
 * @param pLastLogicalBlk pointer to the last logical block
 * @param pLastPhysicalBlk pointer to the last physical block
 *
 * @retval NvSuccess Partition id found and reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Partition id doesn't exist in BCT's table
 */
NvError
GetLastBlockWritten(NvU32 partId, NvU32 StartLogicalSector,
    NvU32 NumLogicalSectors, NvU32 *pLastLogicalBlk, NvU32 *pLastPhysicalBlk);

/**
 * Find the partition id for the BL located at the specified index number (aka,
 * slot number) in the BCT's table of boot loaders
 *
 * @param hBct handle to BCT instance
 * @param Slot index number in BCT's table of BL's
 * @param PartitionId pointer to partition containing BL
 *
 * @retval NvSuccess Partition id found and reported successfully
 * @retval NvError_InvalidAddress Illegal pointer value
 * @retval NvError_BadParameter Partition id doesn't exist in BCT's table
 */
NvError
NvBuQueryBlPartitionIdBySlot(
    NvBctHandle hBct,
    NvU32 Slot,
    NvU32 *PartitionId);

/**
 * Create a table indicating the bad blocks on the device.
 * The table is a bit array, where each bit represents a block.
 * A '1' indicates a bad block
 *
 * @param hBct handle to BCT instance
 * @param PartitionId The partition on which the bct is present
 *
 * @retval NvSuccess Bad block table successfully created
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 *
 */
NvError
NvBuCreateBadBlockTable(
    NvBctHandle hBct,
    NvU32 partitionId);

/**
 * This is to address for T12x specifics,
 * Create a table indicating the bad blocks on the device.
 * The table is a bit array, where each bit represents a block.
 * A '1' indicates a bad block
 *
 * @param hBct handle to BCT instance
 * @param PartitionId The partition on which the bct is present
 *
 * @retval NvSuccess Bad block table successfully created
 * @retval NvError_InsufficientMemory not enough memory to carry out operation
 *
 */
NvError
NvBuPrivCreateBadBlockTableT12x(
    NvBctHandle hBct,
    NvU32 partitionId);

/**
 * This is to return the signature offset for T12x specifics,
 * the Bct is divided in to signed and unsigned sections.
 * the signed section starts from RandomAesBlock of BCT
 * @retval Offset of signature from the BCT
 *
 */
NvU32
NvBuPrivGetSignatureOffsetT12x(void);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif // INCLUDED_NVBU_PRIVATE_H
