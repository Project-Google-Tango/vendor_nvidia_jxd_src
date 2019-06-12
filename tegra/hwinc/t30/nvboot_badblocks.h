/*
 * Copyright (c) 2007 - 2009 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/**
 * @file
 * <b>NVIDIA Tegra ODM Kit:
 *         Bad Blocks Table (Tegra APX)</b>
 *
 * @b Description: NvBootBadBlockTable contains a description of the known
 * bad blocks, which are skipped over when loading BCTs and boot loaders.
 */

/**
 * @defgroup nvbl_bad_block_group Bad Blocks Table (Tegra APX)
 * @ingroup nvbl_bct_group
 * @{
 *
 * To keep the size of the table manageable, the concept of virtual blocks
 * is used. The \c BadBlocks[] array represents the state of each of the
 * virtual blocks with a single bit that is a '1' if the block is known
 * to be bad.
 *
 * The size of a virtual block must be >= the actual block size
 * of the device. If there is enough space in the \c BadBlocks[] array,
 * the virtual block size should be equal to the actual block size.
 * However, the virtual block size can be increased to ensure that
 * it covers the portion of the secondary boot device read by the 
 * boot ROM. The tradeoff is coarser granularity of bad block information.
 *
 * If the boot ROM attempts to read from a block outside the range of the
 * table, it presumes the block to be good.
 *
 * Given a block number, its status can be given as:
 * <pre>
 *   VirtualBlock = Block >> (Table->BlockSizeLog2 -
 *                            Table->VirtualBlockSizeLog2);
 *   TableEntry   = Table->BadBlocks[VirtualBlock >> 3];
 *   Status       = TableEntry & (1 << (VirtualBlock & 0x7));
 * </pre>
 */

#ifndef INCLUDED_NVBOOT_BADBLOCKS_H
#define INCLUDED_NVBOOT_BADBLOCKS_H

#include "nvcommon.h"
#include "t30/nvboot_config.h"

#if defined(__cplusplus)
extern "C"
{
#endif


/**
 * Defines the bad block table structure stored in the BCT.
 */
typedef struct NvBootBadBlockTableRec
{
    /// Specifies the number of actually used entries in the bad block table.
    NvU32 EntriesUsed;

    /// Specifies the size of a virtual block as log2(# of bytes).
    /// This must be >= the physical block size.
    NvU8  VirtualBlockSizeLog2;

    /// Specifies the actual size of a block as log2(# of bytes).
    NvU8  BlockSizeLog2;

    /// Specifies the state of each virtual block with a single bit.
    /// A '1' bit indicates that the corresponding region in the storage
    /// device is known to be bad.
    NvU8  BadBlocks[NVBOOT_BAD_BLOCK_TABLE_SIZE / 8];
} NvBootBadBlockTable;

#if defined(__cplusplus)
}
#endif

/** @} */
#endif /* #ifndef INCLUDED_NVBOOT_BADBLOCKS_H */
