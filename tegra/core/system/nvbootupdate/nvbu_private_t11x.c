/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
#include "nvbu.h"
#include "nvpartmgr.h"
#include "nvddk_blockdevmgr.h"
#include "nvddk_blockdev.h"
#include "nvboot_bit.h"
#include "nvbu_private.h"

NvError
NvBuBctCreateBadBlockTable(
    NvBctHandle hBct,
    NvU32 PartitionId)
{
    NvError e = NvSuccess;
    NvDdkBlockDevHandle hDev = 0;
    NvU32 bootromBlockSizeLog2, length = sizeof(NvU32);
    NvDdkBlockDevInfo partInfo;
    NvFsMountInfo minfo;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusInputArgs query_arg_in;
    NvDdkBlockDevIoctl_QueryPhysicalBlockStatusOutputArgs query_arg_out;
    NvU32 size = 0, instance = 0, partId;
    NvBootBadBlockTable badBlockTable;
    NvPartInfo PTInfo;
    NvU32 curBlock = 0;
    NvU32 curBit = 0;
    NvU32 curIndex = 0;
    NvU32 partitionSize = 0;
    NvU32 maxBlock = 0;
    NvU32 bootromBlockSize;
    NvU32 devBlockSize;
    NvU32 i;
    NvU32 LastLogicalBlock = 0;
    NvU32 LastPhysicalBlock = 0;
    NvU32 BadBlockCount;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(hBct, NvBctDataType_BootDeviceBlockSizeLog2,
                     &length, &instance, &bootromBlockSizeLog2)
        );

    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetFsInfo( PartitionId, &minfo )
    );

    // By default need partition id specific block driver handle
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, PartitionId, &hDev )
    );

    // partInfo contains partition interleave dependent block size
    hDev->NvDdkBlockDevGetDeviceInfo( hDev, &partInfo);
    // Close for last block driver open
    hDev->NvDdkBlockDevClose(hDev);

    // As exception Block driver open needs part-id==0 access. Reason is
    // we need to call physical block status Query IOCTL
    // Physical IOCTL calls need part-id==0 access
    NV_CHECK_ERROR_CLEANUP(
        NvDdkBlockDevMgrDeviceOpen( (NvDdkBlockDevMgrDeviceId)minfo.DeviceId,
            minfo.DeviceInstance, 0, &hDev )
    );

    bootromBlockSize = 1 << bootromBlockSizeLog2;
    // We get size for non-interleaved partition
    devBlockSize = partInfo.BytesPerSector * partInfo.SectorsPerBlock;

    NvOsMemset(&badBlockTable, 0, sizeof(badBlockTable));

    badBlockTable.BlockSizeLog2 = bootromBlockSizeLog2;
    badBlockTable.VirtualBlockSizeLog2 = bootromBlockSizeLog2;

    //For nand devices, the bootrom block size
    //and devblocksize are always equal. If they're not
    //just skip this function as the device is probably
    //HSMMC which does not have bad blocks
    if (devBlockSize != bootromBlockSize)
    {
        e = NvSuccess;
        goto fail;
    }

    //As an optimization, we only mark bad blocks
    //upto to the bootloader stored furthest on the
    //device (don't have to mark the entire device
    //which would be slow).
    for (i=0;i<NVBOOT_MAX_BOOTLOADERS;i++)
    {
        NV_CHECK_ERROR_CLEANUP(
            NvBuQueryBlPartitionIdBySlot(hBct, i, &partId)
        );

        if (!partId)
            continue;

        e = NvPartMgrGetPartInfo( partId, &PTInfo);

        if (e != NvSuccess)
            continue;

        {
            // Function to return the physical and corresponding logical
            // address of the block last used(written) in the partition
            e = GetLastBlockWritten(partId,
                (NvU32)PTInfo.StartLogicalSectorAddress,
                (NvU32)PTInfo.NumLogicalSectors, &LastLogicalBlock,
                &LastPhysicalBlock);
            if (e == NvSuccess)
            {
                NvU32 StartBlock;
                NvU32 NumBlocks;
                // Get the bad block found within this partition
                BadBlockCount = (LastPhysicalBlock - LastLogicalBlock);
                // Get start logical block global interleaved view
                StartBlock = ((NvU32)PTInfo.StartLogicalSectorAddress) /
                    partInfo.SectorsPerBlock;
                // number of blocks in partition specific interleaving
                NumBlocks = (NvU32)PTInfo.NumLogicalSectors /
                    partInfo.SectorsPerBlock;
                // End physical address is taken as logical block count
                // incremented by the bad blocks found
                curBlock = StartBlock + NumBlocks + BadBlockCount;
                // Found end physical address of this partition
            }
            else
            {
                // Mostly this case is when no block is used yet
                // in the partition
                e = NvError_NandTagAreaSearchFailure;
                goto fail;
            }
        }

        if (curBlock > maxBlock)
            maxBlock = curBlock;
    }

    //FIXME - We'll skip the case where the device has more
    //blocks than we can represent.
    if (maxBlock > NVBOOT_BAD_BLOCK_TABLE_SIZE)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }

    curBlock = 0;
    e = NvSuccess;
    while (curBlock < maxBlock)
    {
        query_arg_in.BlockNum = curBlock;
        NV_CHECK_ERROR_CLEANUP(
            hDev->NvDdkBlockDevIoctl( hDev,
                NvDdkBlockDevIoctlType_QueryPhysicalBlockStatus,
                sizeof(query_arg_in), sizeof(query_arg_out), &query_arg_in,
                &query_arg_out )
        );

        //A '1' indicates a bad block.
        badBlockTable.BadBlocks[curIndex] |=
                           (!query_arg_out.IsGoodBlock << curBit);

        curBit++;
        curBlock++;
        badBlockTable.EntriesUsed++;

        if (curBit == 8)
        {
            curBit = 0;
            curIndex++;
            badBlockTable.BadBlocks[curIndex] = 0;
        }
    }

    //The partition size in the bct needs to match the number
    //of blocks represented in the bad block table
    partitionSize = (1 << badBlockTable.VirtualBlockSizeLog2) * curBlock;
    size = sizeof(NvU32);
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_PartitionSize,
                     &size, &instance, &partitionSize)
    );

    size = sizeof(badBlockTable);
    NV_CHECK_ERROR_CLEANUP(
        NvBctSetData(hBct, NvBctDataType_BadBlockTable,
                     &size, &instance, &badBlockTable)
    );
fail:
    // Close block driver handle opened
    if (hDev)
        hDev->NvDdkBlockDevClose(hDev);
    return e;
}
