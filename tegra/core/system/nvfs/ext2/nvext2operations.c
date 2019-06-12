/*
 * Copyright (c) 2009-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

/** nvaboot_ext2fs.c
 *
 * NvAboot support for formatting Ext2fs partitions
 */

#include "nvcommon.h"
#include "nvpartmgr.h"
#include "nverror.h"
#include "nvos.h"
#include "nvrm_module.h"
#include "nvext2filesystem.h"
#include "nvfsmgr_defs.h"

#include "nvodm_query.h"

#define NV_EXT2_INDEX(i) ((i)+fs->pSb->FirstDataBlock)
#define NV_EXT2_OFFSET(i) ((i)-fs->pSb->FirstDataBlock)
#define DIV_CEIL(divisor, dividend) (((divisor)+(dividend)-1) / (dividend))
#define NV_EXT2_HOST_TO_NET_L(x) (((x) << 24) | (((x) >> 24) & 255) | (((x) << 8) & 0xff0000) | (((x) >> 8) & 0xff00))

// Workaround for SD partial erase support
#ifndef WAR_SD_FORMAT_PART
#define WAR_SD_FORMAT_PART 1
#endif

/* FIXME:  For big partitions (e.g., 16-32GB), the code below requires an
 * impractical amount of memory.  The simplest way to fix this would probably
 * be to change the single array of block pointers to be hierarchical
 * (block pointer directory -> block pointer -> block), so that contiguous
 * swaths of unallocated blocks consume minimal memory.  Beyond that,
 * CommitBlock could be modified to writeback the data to mass storage, free
 * the block's memory, set the block pointer entry to -1.  GetBlock would
 * need to check if the pointer value is -1; if so, it should allocate new
 * memory and read the data from mass storage. */
typedef struct FileSystemRec
{
    NvU32 NumBlocks;
    NvU32 GroupDescBlocks;
    NvU32 InodeBlocksPerGroup;
    NvExt2SuperBlock *pSb;
    NvExt2GroupDesc  *pGd;
    NvU8 *BlockPtrs[1];
} FileSystem;

typedef struct BlockContextRec {
    NvU32 BlockCount;
    NvU32 BlockSize;
    NvU32 Flags;
    NvU8 *IndBuf;
    NvU8 *DIndBuf;
    NvU8 *TIndBuf;
    void *PrivData;
} BlockContext;

typedef struct MkJournalRec {
    NvU32 NumBlocks;
    NvU32 NewBlocks;
    NvU8 *Buf;      
} MkJournal;

static NvU32 gs_NextFreeDataBlock = 0;

static NvU8* 
GetBlock(
    NvDdkBlockDevHandle hDev,
    FileSystem *pFs,
    NvU32 Group,
    NvU32 Block,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvU32 Index;

    if (!Group)
    {
        Index = Block;
    }
    else
    {
        Index = pFs->pSb->BlocksPerGroup * Group + Block;
    }

    if ((pFs->BlockPtrs[Index] == 0) || (pFs->BlockPtrs[Index] == (NvU8*)(-1)))
    {
        NvU32 BlockSize = (1 << (10 + pFs->pSb->BlockSizeLog2));
        NvU8* temp = (NvU8 *)NvOsAlloc(BlockSize);

        if (!temp)
        {
            return temp;
        }
        if (pFs->BlockPtrs[Index] == 0)
        {
            NvOsMemset(temp, 0, BlockSize);
        }
        else
        {
            // read from the media
            NvU32 Index = pFs->pSb->BlocksPerGroup * Group + Block;
            NvU32 Addr = Index * SectorsPerBlock +
                            (NvU32)StartLogicalSectorAddress;

            hDev->NvDdkBlockDevReadSector(hDev, Addr, temp, SectorsPerBlock);
        }

        pFs->BlockPtrs[Index] = temp;
    }

    return pFs->BlockPtrs[Index];
}

static NvExt2GroupDesc* 
GetGroupDescriptor(
    NvDdkBlockDevHandle hDev,
    FileSystem *pFs,
    NvU32 Group,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvU32 GroupOffset = Group * sizeof(NvExt2GroupDesc) +
                        NV_MAX(1<<(10+pFs->pSb->BlockSizeLog2),
                        NV_EXT2_SUPERBLOCK_OFFSET + sizeof(NvExt2SuperBlock));

    NvU32 GroupBlock = GroupOffset >> (10 + pFs->pSb->BlockSizeLog2);
    NvU8 *pBlock = GetBlock(hDev, pFs, 0, GroupBlock, SectorsPerBlock,
                            StartLogicalSectorAddress);

    GroupOffset &= ((1<<(10 + pFs->pSb->BlockSizeLog2))-1);
    if (pBlock)
    {
        pBlock += GroupOffset;
    }

    return (NvExt2GroupDesc *)pBlock;
}

static NvExt2Inode*
GetInode(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 Group,
    NvU32 Inode,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
   NvExt2GroupDesc *pGd = GetGroupDescriptor(hDev, fs, Group, SectorsPerBlock,
                                                StartLogicalSectorAddress);
   NvExt2Inode *pIn;
   // Inodes are 128 bytes, log2(128)=7, so BlockSize/InodeSize=BlockSizeLog2-7
   NvU32 InodesPerBlockLog2 = 3+fs->pSb->BlockSizeLog2;
   NvU32 InodeIndex = Inode >> InodesPerBlockLog2;
   NvU32 InodeOffset = Inode & ((1<<InodesPerBlockLog2)-1);

   if (!pGd)
   {
       return 0;
   }

   pIn = (NvExt2Inode *)GetBlock(hDev, fs, 0, fs->pSb->FirstDataBlock +
                                    NV_EXT2_OFFSET(pGd->InodeTableBlock) +
                                    InodeIndex, SectorsPerBlock,
                                    StartLogicalSectorAddress);
    if (pIn)
    {
        pIn += InodeOffset;
    }
    return pIn;
}

static NvError 
CommitBlock(
    NvDdkBlockDevHandle hDev,
    FileSystem *pFs,
    NvU32 Group,
    NvU32 Block,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress,
    NvU32 NumberOfBlocks)
{
    NvError e = NvSuccess;
    NvU32 Index = pFs->pSb->BlocksPerGroup * Group + Block;

    if ((Index != pFs->pSb->FirstDataBlock) &&
        (pFs->BlockPtrs[Index] != 0) &&
        (pFs->BlockPtrs[Index] != (NvU8 *)-1))
    {
        NvU32 Addr = Index * SectorsPerBlock +
                        (NvU32)StartLogicalSectorAddress;

        NV_CHECK_ERROR_CLEANUP(
            hDev->NvDdkBlockDevWriteSector(hDev, Addr,
                                            pFs->BlockPtrs[Index],
                                            NumberOfBlocks)
        );
        NvOsFree(pFs->BlockPtrs[Index]);
        pFs->BlockPtrs[Index] = (NvU8 *)-1;
    }
fail:
    return e;
}

static NvError 
AllocFileSystem(
    NvU32 NumBlocks,
    NvU32 BlockSize,
    FileSystem **pFs)
{
    FileSystem *fs;
    NvError e;
    NvU32 i;

    if (!pFs || *pFs || (BlockSize & (BlockSize-1)) ||
        (BlockSize>4096) || (BlockSize<1024))
    {
        return NvError_BadParameter;
    }

    *pFs = NULL;
    fs = NvOsAlloc(sizeof(FileSystem) + (NumBlocks-1)*sizeof(NvU8*));
    if (!fs)
    {
        return NvError_InsufficientMemory;
    }

    fs->NumBlocks = NumBlocks;
    NvOsMemset(&fs->BlockPtrs, 0, sizeof(NvU8*)*NumBlocks);
    if (BlockSize == 1024)
    {
        fs->pSb = (NvExt2SuperBlock*)NvOsAlloc(sizeof(NvExt2SuperBlock));
        if (!fs->pSb)
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(fs->pSb, 0, sizeof(NvExt2SuperBlock));
        fs->BlockPtrs[1] = (NvU8 *)fs->pSb;
    }
    else
    {
        fs->BlockPtrs[0] = NvOsAlloc(BlockSize);
        if (!fs->BlockPtrs[0])
        {
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(fs->BlockPtrs[0], 0, BlockSize);
        fs->pSb =
            (NvExt2SuperBlock*)&(fs->BlockPtrs[0][NV_EXT2_SUPERBLOCK_OFFSET]);
    }

    *pFs = fs;
    return NvSuccess;

fail:
    if (fs)
    {
        for (i=0; i<fs->NumBlocks; i++)
        {
            if (fs->BlockPtrs[i])
                NvOsFree(fs->BlockPtrs[i]);
        }
        NvOsFree(fs);
    }
    *pFs = NULL;
    return e;
}

static NvError 
SetupGroupBlockBitmap(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvExt2GroupDesc *pGd,
    NvU32 Group,
    NvU32 BlockSize,
    NvU32 Bookkeeping,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvU32 *pTemp;
    NvU32 i;

    pTemp = (NvU32*)GetBlock(hDev, fs, 0,
                            fs->pSb->FirstDataBlock +
                            NV_EXT2_OFFSET(pGd->BlockBitmapBlock),
                            SectorsPerBlock, StartLogicalSectorAddress);
    if (!pTemp)
    {
        return NvError_InsufficientMemory;
    }

    /* Mark all of the spare bits in the block bitmap as already allocated
     * If pGd->FreeBlockCount is equal to (BlockSize * 8) then there will not
     * be any spare bits in the bitmap */
    if (pGd->FreeBlockCount < (BlockSize * 8))
    {
        pTemp[(pGd->FreeBlockCount / 32)] = ~((1 << (pGd->FreeBlockCount & 31)) - 1);
        for (i = (pGd->FreeBlockCount / 32) + 1; i < (BlockSize * 8) / 32; i++)
        {
            pTemp[i] = ~0UL;
        }
    }

    /* Each group has 3 blocks for the bitmaps and superblock copy, and N
     * blocks of book-keeping for Inode tables and group descriptor copies */
    for (i=0; i<3 + Bookkeeping; i++)
    {
        pTemp[i/32] |= (1<<(i&31));
        pGd->FreeBlockCount--;
        fs->pSb->FreeBlockCount--;
    }

    pTemp = (NvU32*)GetBlock(hDev, fs, 0,
                            fs->pSb->FirstDataBlock +
                            NV_EXT2_OFFSET(pGd->InodeBitmapBlock),
                            SectorsPerBlock, StartLogicalSectorAddress);

    if (!pTemp)
    {
        return NvError_InsufficientMemory;
    }

    /* Mark all of the spare bits in the inode bitmap as already allocated
     * If pGd->FreeInodeCount is equal to (BlockSize * 8) then there will not
     * be any spare bits in the bitmap */
    if (pGd->FreeInodeCount < (BlockSize * 8))
    {
        pTemp[(pGd->FreeInodeCount / 32)] = ~((1 << (pGd->FreeInodeCount & 31)) - 1);
        for (i = (pGd->FreeInodeCount / 32) + 1; i < (BlockSize * 8) / 32; i++)
        {
            pTemp[i] = ~0UL;
        }
    }

    if (!Group)
    {
        for (i=0; i<NV_EXT2_FIRST_INODE_INDEX-1; i++)
        {
            pTemp[i/32] |= (1<<(i&31));
            pGd->FreeInodeCount--;
            fs->pSb->FreeInodeCount--;
        }
    }

    return CommitBlock(hDev, fs, 0,
                        fs->pSb->FirstDataBlock +
                        NV_EXT2_OFFSET(pGd->BlockBitmapBlock),
                        SectorsPerBlock, StartLogicalSectorAddress, SectorsPerBlock * 2);
}

/*
 * Updates the index of the next free data block
 */
static void UpdateNextFreeDataBlock(FileSystem *fs)
{
    NvU32 Group = gs_NextFreeDataBlock / fs->pSb->BlocksPerGroup;
    
    if ((gs_NextFreeDataBlock + 1) >= ((Group + 1) * fs->pSb->BlocksPerGroup))
    {
        /* The first block of each group stores the superblock; the next
         * N blocks store the group descriptors.  The next 2 blocks store
         * the group's block-allocation bitmap and Inode allocation bitmap,
         * followed by M blocks of Inode records. 
         */
        gs_NextFreeDataBlock += (1 + fs->GroupDescBlocks + 1 + 1 + fs->InodeBlocksPerGroup);
    }
    gs_NextFreeDataBlock++;
}

/*
 * Updates the block stats - block bitmap table, super block free block count, 
 * group descriptor free block count.
 */
static void 
UpdateBlockStats(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs, 
    NvU32 BlockNum,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvU32 *pTemp;
    NvExt2GroupDesc *pGd;
    NvU32 Group = 0;
    NvU32 TempBlock;
    
    Group = BlockNum / fs->pSb->BlocksPerGroup;
    pGd = GetGroupDescriptor(hDev, fs, Group, SectorsPerBlock, 
            StartLogicalSectorAddress);
    if (pGd)
    {
        pTemp = (NvU32 *)GetBlock(hDev, fs, 0,
                    (fs->pSb->FirstDataBlock + NV_EXT2_OFFSET(pGd->BlockBitmapBlock)), 
                    SectorsPerBlock, StartLogicalSectorAddress);
                    
        if (pTemp)
        {
            TempBlock = BlockNum % fs->pSb->BlocksPerGroup;
            pTemp[TempBlock / 32] |= (1 << (TempBlock & 31));
        }
        pGd->FreeBlockCount--;
        fs->pSb->FreeBlockCount--;
    }
}

/*
 * Writes journal data blocks
 */
static NvBool 
MakeJournal(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 *pBlockNum,
    NvU32 BlockCount,
    NvU32 RefBlock,
    NvU32 RefOffset,
    void *pPrivData,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    MkJournal *pEs = (MkJournal *)pPrivData;
    NvU32 NewBlock = gs_NextFreeDataBlock;
    static NvU32 LastBlock = 0;
    NvU8* pBlock;
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));
                
    if (BlockCount > 0)
            pEs->NumBlocks--;

    pEs->NewBlocks++;
        
    if (BlockCount == 0)
    {
        // Journalling super block is only written at the first journalling block.
        pBlock = (NvU8*)GetBlock(hDev, fs, 0, (fs->pSb->FirstDataBlock + NewBlock), 
                            SectorsPerBlock, StartLogicalSectorAddress);
        if (!pBlock)
            return NV_FALSE;
        NvOsMemcpy(pBlock, pEs->Buf, BlockSize);
        CommitBlock(hDev, fs, 0, (fs->pSb->FirstDataBlock + NewBlock), 
            SectorsPerBlock, StartLogicalSectorAddress, SectorsPerBlock);
    }

    *pBlockNum = NewBlock;
    LastBlock = NewBlock;
        
    UpdateBlockStats(hDev, fs, NewBlock, SectorsPerBlock, StartLogicalSectorAddress);        
    UpdateNextFreeDataBlock(fs);

    if (pEs->NumBlocks == 0)
        return NV_FALSE;
    else
        return NV_TRUE; 
}

/*
 * Writes indirect block of journal inode.
 */
static NvBool 
BlockIterateInd(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 *pIndBlock, 
    NvU32 RefBlock,
    NvU32 RefOffset, 
    BlockContext *pCtx,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvBool Ret = NV_TRUE;
    NvU32 i;
    NvU32 Limit;
    NvU32 Offset;
    NvU32 *pBlockNum;
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));

    Limit = BlockSize >> 2;
        
    pCtx->IndBuf = (NvU8*)GetBlock(hDev, fs, 0, 
                        (fs->pSb->FirstDataBlock + gs_NextFreeDataBlock), 
                        SectorsPerBlock, StartLogicalSectorAddress);
    if (!pCtx->IndBuf)
        return NV_FALSE;
            
    *pIndBlock = gs_NextFreeDataBlock;
    UpdateBlockStats(hDev, fs, gs_NextFreeDataBlock, SectorsPerBlock, 
        StartLogicalSectorAddress);
    UpdateNextFreeDataBlock(fs);
        
    pBlockNum = (NvU32 *)pCtx->IndBuf;
    Offset = 0;
        
    for (i = 0; i < Limit; i++, pCtx->BlockCount++, pBlockNum++)
    {
        Ret = MakeJournal(hDev, fs, pBlockNum, pCtx->BlockCount, *pIndBlock, 
                Offset, pCtx->PrivData, SectorsPerBlock, 
                StartLogicalSectorAddress);                                                                     
        if (!Ret)
            break;              
        Offset += sizeof(NvU32);
    }
        
    return Ret;
}

/*
 * Writes double indirect block of journal inode.
 */
static NvBool 
BlockIterateDind(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 *pDindBlock, 
    NvU32 RefBlock,
    NvU32 RefOffset,
    BlockContext *pCtx,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvBool Ret = NV_TRUE;
    NvU32 i;
    NvU32 Limit;
    NvU32 Offset;
    NvU32 *pBlockNum;
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));
    
    Limit = BlockSize >> 2;
        
    pCtx->DIndBuf = (NvU8*)GetBlock(hDev, fs, 0, 
                        (fs->pSb->FirstDataBlock + gs_NextFreeDataBlock),
                        SectorsPerBlock, StartLogicalSectorAddress);
    if (!pCtx->DIndBuf)
        return NV_FALSE;
            
    *pDindBlock = gs_NextFreeDataBlock;
    UpdateBlockStats(hDev, fs, gs_NextFreeDataBlock, SectorsPerBlock, 
        StartLogicalSectorAddress);
    UpdateNextFreeDataBlock(fs);
    
    pBlockNum = (NvU32 *)pCtx->DIndBuf;
    Offset = 0;     
        
    for (i = 0; i < Limit; i++, pBlockNum++) 
    {
        Ret = BlockIterateInd(hDev, fs, pBlockNum, *pDindBlock, Offset, pCtx, 
                SectorsPerBlock, StartLogicalSectorAddress);
        if (!Ret)
            break;
        Offset += sizeof(NvU32);            
    }
        
    return Ret;
}

/*
 * Writes triple indirect block of journal inode.
 */
static NvBool 
BlockIterateTind(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 *pTindBlock, 
    NvU32 RefBlock,
    NvU32 RefOffset, 
    BlockContext *pCtx,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvBool Ret = NV_TRUE;
    NvU32 i;
    NvU32 Limit;
    NvU32 Offset;
    NvU32 *pBlockNum;
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));
    
    Limit = BlockSize >> 2;
        
    pCtx->TIndBuf = (NvU8*)GetBlock(hDev, fs, 0, 
                        (fs->pSb->FirstDataBlock + gs_NextFreeDataBlock),
                        SectorsPerBlock, StartLogicalSectorAddress);
    if (!pCtx->TIndBuf)
        return NV_FALSE;
        
    UpdateBlockStats(hDev, fs, gs_NextFreeDataBlock, SectorsPerBlock, 
        StartLogicalSectorAddress);
    *pTindBlock = gs_NextFreeDataBlock;
    UpdateNextFreeDataBlock(fs);
    
    pBlockNum = (NvU32 *) pCtx->TIndBuf;
    Offset = 0;     
        
    for (i = 0; i < Limit; i++, pBlockNum++) 
    {
        Ret = BlockIterateDind(hDev, fs, pBlockNum, *pTindBlock, Offset, pCtx, 
                SectorsPerBlock, StartLogicalSectorAddress);
        if (!Ret)
            break;
        Offset += sizeof(NvU32);
    }
        
    return Ret;
}

/*
 * Writes all the blocks of journal inode.
 */
static void 
BlockIterate(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs, 
    void *pPrivData, 
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvU32 i;
    NvU32 Blocks[NV_EXT2_N_BLOCKS] = {0};   /* directory data blocks */
    NvExt2Inode *pInode;
    BlockContext Ctx;
    NvU32 Limit;
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));
    NvBool Success = NV_TRUE;
        
    Limit = BlockSize >> 2;

    Ctx.PrivData = pPrivData;
    Ctx.BlockCount = 0;
        
    //Iterate over normal data blocks
    for (i = 0; i < NV_EXT2_NDIR_BLOCKS ; i++, Ctx.BlockCount++) 
    {               
        Success = MakeJournal(hDev, fs, &Blocks[i], Ctx.BlockCount, 0, i, 
                    pPrivData, SectorsPerBlock, StartLogicalSectorAddress);
        if (!Success)
        {
            goto AbortExit;         
        }
    }
        
    Success = BlockIterateInd(hDev, fs, (Blocks + NV_EXT2_IND_BLOCK), 0, 
                NV_EXT2_IND_BLOCK, &Ctx, SectorsPerBlock, 
                StartLogicalSectorAddress);   
    if (!Success)
    {
        goto AbortExit;
    }
        
    Success = BlockIterateDind(hDev, fs, (Blocks + NV_EXT2_DIND_BLOCK), 0, 
                NV_EXT2_DIND_BLOCK, &Ctx, SectorsPerBlock, 
                StartLogicalSectorAddress);
    if (!Success)
    {
        goto AbortExit;
    }
        
    Success = BlockIterateTind(hDev, fs, (Blocks + NV_EXT2_TIND_BLOCK), 0, 
                  NV_EXT2_TIND_BLOCK, &Ctx, SectorsPerBlock, 
                  StartLogicalSectorAddress);
    if (!Success)
    {
        goto AbortExit;
    }
        
AbortExit:      
    pInode = GetInode(hDev, fs, 0, (NV_EXT2_JOURNAL_INODE_INDEX - 1), 
                SectorsPerBlock, StartLogicalSectorAddress);
        
    for (i=0; i < NV_EXT2_N_BLOCKS; i++)
    {
        pInode->BlockTable[i] = Blocks[i];
    }
}

/*
 * Writes journal inode.
 */
static NvError 
WriteJournalInode(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs, 
    NvU32 JournalBlocks,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress, 
    NvU8* pBuf)
{
    NvU32 BlockSize = (NvU32)(1 << (10 + fs->pSb->BlockSizeLog2));
    NvExt2Inode *pInode;
    MkJournal Es;
    NvError e = NvSuccess;
    
    Es.NumBlocks = JournalBlocks;
    Es.NewBlocks = 0;
    Es.Buf = pBuf;
        
    BlockIterate(hDev, fs, &Es, SectorsPerBlock, StartLogicalSectorAddress);
            
    // Update journal inode
    pInode = GetInode(hDev, fs, 0, (NV_EXT2_JOURNAL_INODE_INDEX - 1), 
                SectorsPerBlock, StartLogicalSectorAddress);
    if (!pInode)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    pInode->Size = BlockSize * JournalBlocks;
    pInode->NumBlocks = (BlockSize / NV_EXT2_INODE_BLOCKSIZE) * Es.NewBlocks;
    pInode->AccessTime = pInode->ModifyTime = pInode->CreateTime = 0;
    pInode->DeleteTime = 0;
    pInode->LinkCount = 1;
    pInode->Mode = 0100000 | 0600;

    /* No need to update inode bitmap for journal inode
     * (NV_EXT2_JOURNAL_INODE_INDEX=8) and FreeInodeCount in super block
     * and group descriptor since this is already done in 
     * SetupGroupInodeBitmap() for all inodes less than the first inode
     * (NV_EXT2_FIRST_INODE_INDEX=11) */

fail:
    return e;    
}

/*
 * Gets the number of journal blocks required based on the total block count.
 */
static NvU32 GetJournalSize(NvExt2SuperBlock *pSb)
{
    NvU32 JBlocks = pSb->BlockCount/64;
    
    if (JBlocks < 1024)
        JBlocks = 1024;
    if (JBlocks > 32768)
        JBlocks = 32768;
            
    return JBlocks;
}

/*
 * Creates journalling info.
 */
static NvError 
CreateJournalInfo(
    NvDdkBlockDevHandle hDev, 
    FileSystem *pFs, 
    NvU32 SectorsPerBlock, 
    NvU64 StartLogicalSectorAddress)
{
    NvError e = NvSuccess;
    NvU32 JournalBlocks;
    NvExt2SuperBlock *pSb = pFs->pSb;
    NvExt2JournalSuperBlock *pJSb;
    NvU32 BlockSize = (NvU32)(1 << (10 + pSb->BlockSizeLog2));
    
    JournalBlocks = GetJournalSize(pSb);
    if (JournalBlocks < 1024)
    {
        e = NvError_BadValue;
        goto fail;
    }
    
    pJSb = NvOsAlloc(BlockSize);
    if (!pJSb)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(pJSb, 0, BlockSize);
    
    pJSb->Header.Magic = NV_EXT2_HOST_TO_NET_L(NV_EXT2_JSB_HEADER_MAGIC_NUMBER);
    pJSb->Header.BlockType = NV_EXT2_HOST_TO_NET_L(NV_EXT2_JSB_SUPERBLOCK_V2);
    pJSb->BlockSize = NV_EXT2_HOST_TO_NET_L(BlockSize);
    pJSb->TotalBlocksInJournFile = NV_EXT2_HOST_TO_NET_L(JournalBlocks);
    pJSb->NumOfUsers = NV_EXT2_HOST_TO_NET_L(1);
    pJSb->FirstBlock = NV_EXT2_HOST_TO_NET_L(1);
    pJSb->FirstCommitId = NV_EXT2_HOST_TO_NET_L(1);
    NvOsMemcpy(&(pJSb->Uuid[0]), 
        ((NvU8 *)pSb + NV_EXT2_SUPERBLOCK_UUID_OFFSET),
        NV_EXT2_SUPERBLOCK_UUID_LENGTH);
        
    NV_CHECK_ERROR_CLEANUP(WriteJournalInode(hDev, pFs, JournalBlocks, 
        SectorsPerBlock, StartLogicalSectorAddress, (NvU8*)pJSb));
    
    pSb->JournalInodeNum = NV_EXT2_JOURNAL_INODE_INDEX;
    pSb->CompatibilityFeatureSet |= NV_EXT2_FEATURE_COMPAT_HAS_JOURNAL;
    NvOsFree(pJSb);
    
fail:
    return e;
}

static NvError 
InitializeSuperBlock(
    NvDdkBlockDevHandle hDev,
    FileSystem *fs,
    NvU32 BlockSize,
    NvU32 SectorsPerBlock,
    NvU64 StartLogicalSectorAddress)
{
    NvExt2SuperBlock *pSb = fs->pSb;
    NvExt2Inode *pInode;
    NvExt2Directory *pDirectory;
    NvExt2GroupDesc *pGd;
    NvU32 *pTemp;
    NvU32 NumGroups;
    NvU32 NumBlocks;
    NvU32 Index;
    NvU32 i, j;
    NvU32 rem = 0;
    NvU32 overhead = 0;
    NvError e;
    
    NvOsMemset(pSb, 0, sizeof(NvExt2SuperBlock));

    /* Since only 1024, 2048 and 4096 byte block sizes are allowed
     * to be allocated, shifting right by 11 = (0, 1, 2) */
    pSb->BlockSizeLog2 = (BlockSize >> 11);
    pSb->FragSizeLog2 = pSb->BlockSizeLog2;
    pSb->BlockCount = fs->NumBlocks;
    pSb->BlocksPerGroup = BlockSize * 8;
    pSb->FirstDataBlock = ((NvU8*)pSb == fs->BlockPtrs[1]) ? 1 : 0;
    pSb->FragsPerGroup = pSb->BlocksPerGroup;
    pSb->MaxMountCount = 0xFFFF;
    pSb->ReservedBlockCount = 0;
    pSb->MountTime = 0;
    pSb->RevLevel = 0;
    pSb->CreatorOs = 0;
    pSb->WriteTime = 0;
    pSb->Magic = NV_EXT2_MAGIC;
    pSb->State = 1;
    pSb->InCompatibilityFeatureSet |= NV_EXT2_FEATURE_INCOMPAT_FILETYPE;
    pSb->RoCompatibilityFeatureSet |= NV_EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER |
                                NV_EXT2_FEATURE_RO_COMPAT_LARGE_FILE;
retry:
    //group descriptor count
    NumGroups = DIV_CEIL(pSb->BlockCount - pSb->FirstDataBlock, pSb->BlocksPerGroup);

    fs->GroupDescBlocks = DIV_CEIL(NumGroups * sizeof(NvExt2GroupDesc), BlockSize);
    pSb->InodeCount = pSb->BlockCount / (8192 / BlockSize);

    pSb->InodesPerGroup = DIV_CEIL(pSb->InodeCount, NumGroups);
    if (pSb->InodesPerGroup > BlockSize * 8)
    {
        pSb->InodesPerGroup = BlockSize * 8;
    }
    fs->InodeBlocksPerGroup = DIV_CEIL(pSb->InodesPerGroup * sizeof(NvExt2Inode),
                                    BlockSize);
    pSb->InodesPerGroup = (fs->InodeBlocksPerGroup * BlockSize) /
                            sizeof(NvExt2Inode);
    // number of inodes per group is a * multiple of 8.
    pSb->InodesPerGroup &= ~7;
    fs->InodeBlocksPerGroup =
        DIV_CEIL(pSb->InodesPerGroup * sizeof(NvExt2Inode), BlockSize);
    pSb->InodeCount = pSb->InodesPerGroup * NumGroups;
    pSb->FreeInodeCount = pSb->InodeCount;
    overhead = 1 + fs->GroupDescBlocks + 1 + 1 + fs->InodeBlocksPerGroup;
    rem = (pSb->BlockCount - pSb->FirstDataBlock) % pSb->BlocksPerGroup;

    /* If the last group does not contain enough blocks to store all of the
     * bookkeeping data, get rid of it */
    if ((NumGroups == 1) && rem && (rem < overhead))
    {
         return NvError_InsufficientMemory;
    }
    if (rem && (rem < (overhead + 50)))
    {
         pSb->BlockCount -= rem;
         goto retry;
    }
    // done with super block

    pSb->FreeBlockCount = 0;
    NumBlocks = pSb->BlockCount - pSb->FirstDataBlock;

    // make group descriptor table
    for (i=0, Index=(1 + fs->GroupDescBlocks);
         i<NumGroups; i++, Index += pSb->BlocksPerGroup)
    {
        pGd = GetGroupDescriptor(hDev, fs, i, SectorsPerBlock,
                                    StartLogicalSectorAddress);
        if (!pGd)
        {
            return NvError_InsufficientMemory;
        }
        if (NumBlocks >= pSb->BlocksPerGroup)
        {
            pGd->FreeBlockCount = (NvU16)pSb->BlocksPerGroup;
            NumBlocks -= pSb->BlocksPerGroup;
        }
        else
        {
            pGd->FreeBlockCount = (NvU16) NumBlocks;
            NumBlocks = 0;
            // this should happen in the last block
        }
        pSb->FreeBlockCount += pGd->FreeBlockCount;
        pGd->FreeInodeCount = (NvU16)pSb->InodesPerGroup;

        /* The first block of each group stores the superblock; the next
         * N blocks store the group descriptors.  The next 2 blocks store
         * the group's block-allocation bitmap and Inode allocation bitmap,
         * followed by M blocks of Inode records.
         */
        pGd->BlockBitmapBlock = NV_EXT2_INDEX(Index);
        pGd->InodeBitmapBlock = NV_EXT2_INDEX(Index+1);
        pGd->InodeTableBlock  = NV_EXT2_INDEX(Index+2);
        pGd->UsedDirectoryCount = 0;

        NV_CHECK_ERROR(
            SetupGroupBlockBitmap(hDev, fs, pGd, i, BlockSize,
                                    fs->GroupDescBlocks + fs->InodeBlocksPerGroup,
                                    SectorsPerBlock, StartLogicalSectorAddress)
        );
    }
    pGd = GetGroupDescriptor(hDev, fs, 0, SectorsPerBlock,
                                StartLogicalSectorAddress);
    if (!pGd)
    {
        return NvError_InsufficientMemory;
    }
    /* The first two block which are free will store the root directory and
     * lost+found directory info.  These block will always be the first two
     * blocks after the Inode table */
    // this is actaul data block offset
    i = NV_EXT2_OFFSET(pGd->InodeTableBlock) + fs->InodeBlocksPerGroup;
    gs_NextFreeDataBlock = i;
    
    // this is inode bitmap block
    pTemp = (NvU32 *)GetBlock(hDev, fs, 0,
                                fs->pSb->FirstDataBlock +
                                NV_EXT2_OFFSET(pGd->InodeBitmapBlock),
                                SectorsPerBlock, StartLogicalSectorAddress);
    if (!pTemp)
    {
        return NvError_InsufficientMemory;
    }
    pTemp[(NV_EXT2_FIRST_INODE_INDEX-1)/32] |=
        (1<<((NV_EXT2_FIRST_INODE_INDEX-1)&31));
    pGd->FreeInodeCount--;
    pSb->FreeInodeCount--;
    // this is block bitmap block
    pTemp = (NvU32 *)GetBlock(hDev, fs, 0,
                                fs->pSb->FirstDataBlock +
                                NV_EXT2_OFFSET(pGd->BlockBitmapBlock),
                                SectorsPerBlock, StartLogicalSectorAddress);
    if (!pTemp)
    {
        return NvError_InsufficientMemory;
    }
    // mark two blocks as used
    pTemp[i/32] |= (1<<(i&31));
    pTemp[(i+1)/32] |= (1<<((i+1)&31));

    //  Fill in the Inode for the root directory
    pGd->UsedDirectoryCount+=2;
    pGd->FreeBlockCount-=2;
    pSb->FreeBlockCount-=2;

    pInode = GetInode(hDev, fs, 0, NV_EXT2_ROOT_INODE_INDEX-1, SectorsPerBlock,
                        StartLogicalSectorAddress);
    if (!pInode)
    {
        return NvError_InsufficientMemory;
    }
    //  Note: All of these are Octal values, for added illegibility.
    pInode->Mode = 040000 | 0700 | 050 | 05; //  Directory, U=RWX, G=RX, O=RX
    pInode->Size = BlockSize;
    pInode->AccessTime = pInode->ModifyTime = pInode->CreateTime = 0;
    pInode->DeleteTime = 0;
    pInode->LinkCount = 3;
    pInode->NumBlocks = BlockSize / NV_EXT2_INODE_BLOCKSIZE;
    pInode->BlockTable[0] = NV_EXT2_INDEX(i);

    //  Fill in the Inode for the lost+found directory
    pInode = GetInode(hDev, fs, 0, NV_EXT2_FIRST_INODE_INDEX-1,
                        SectorsPerBlock, StartLogicalSectorAddress);
    if (!pInode)
    {
        return NvError_InsufficientMemory;
    }
    pInode->Mode = 040000 | 0700;  // Directory, User = RWX, others = none
    pInode->Size = BlockSize;
    pInode->AccessTime = pInode->ModifyTime = pInode->CreateTime = 0;
    pInode->DeleteTime = 0;
    pInode->LinkCount = 2;
    pInode->NumBlocks = BlockSize / NV_EXT2_INODE_BLOCKSIZE;
    pInode->BlockTable[0] = NV_EXT2_INDEX((i+1));

    /*  Fill in the data block for the root directory.  Include 3 directory
     * entries ".", "..", and "lost+found" */
    pDirectory = (NvExt2Directory*)GetBlock(hDev, fs, 0,
                                     fs->pSb->FirstDataBlock + gs_NextFreeDataBlock,
                                     SectorsPerBlock,
                                     StartLogicalSectorAddress);
    gs_NextFreeDataBlock++;
    if (!pDirectory)
    {
        return NvError_InsufficientMemory;
    }
    pDirectory->Inode = NV_EXT2_ROOT_INODE_INDEX;
    pDirectory->NameLength = 1;
    pDirectory->Size =
        (sizeof(NvExt2Directory)-1 + pDirectory->NameLength + 3) & ~3;
    j = pDirectory->Size;
    pDirectory->Name[0] = '.';
    pDirectory->Name[1] = pDirectory->Name[2] = pDirectory->Name[3] = '\0';
    pDirectory = (NvExt2Directory*) (((NvU8*)pDirectory) + pDirectory->Size);
    pDirectory->Inode = NV_EXT2_ROOT_INODE_INDEX;
    pDirectory->NameLength = 2;
    pDirectory->Size =
        (sizeof(NvExt2Directory)-1 + pDirectory->NameLength + 3) & ~3;
    j += pDirectory->Size;
    pDirectory->Name[0] = pDirectory->Name[1] = '.';
    pDirectory->Name[2] = pDirectory->Name[3] = '\0';
    pDirectory = (NvExt2Directory *)(((NvU8*)pDirectory) + pDirectory->Size);
    pDirectory->Inode = NV_EXT2_FIRST_INODE_INDEX;
    pDirectory->NameLength = 10;
    pDirectory->Size = BlockSize - j;
    NvOsMemcpy(&(pDirectory->Name[0]), "lost+found", 10);
    pDirectory->Name[10] = pDirectory->Name[11] = '\0';
    NV_CHECK_ERROR(
        CommitBlock(hDev, fs, 0, pGd->BlockBitmapBlock, SectorsPerBlock,
                    StartLogicalSectorAddress, SectorsPerBlock)
    );
    NV_CHECK_ERROR(
        CommitBlock(hDev, fs, 0, pGd->InodeTableBlock, SectorsPerBlock,
                    StartLogicalSectorAddress, SectorsPerBlock)
    );
    NV_CHECK_ERROR(
        CommitBlock(hDev, fs, 0, i, SectorsPerBlock, StartLogicalSectorAddress, SectorsPerBlock)
    );

    //  Fill in the data block for the lost+found directory.  Just "." and ".."
    pDirectory = (NvExt2Directory*)GetBlock(hDev, fs, 0,
                                            fs->pSb->FirstDataBlock+gs_NextFreeDataBlock,
                                            SectorsPerBlock,
                                            StartLogicalSectorAddress);
    gs_NextFreeDataBlock++;
    if (!pDirectory)
    {
        return NvError_InsufficientMemory;
    }
    pDirectory->Inode = NV_EXT2_FIRST_INODE_INDEX;
    pDirectory->NameLength = 1;
    pDirectory->Size =
        (sizeof(NvExt2Directory)-1 + pDirectory->NameLength + 3) & ~3;
    j = pDirectory->Size;
    pDirectory->Name[0] = '.';
    pDirectory->Name[1] = pDirectory->Name[2] = pDirectory->Name[3] = '\0';
    pDirectory = (NvExt2Directory *)(((NvU8*)pDirectory) + pDirectory->Size);
    pDirectory->Inode = NV_EXT2_ROOT_INODE_INDEX;
    pDirectory->NameLength = 2;
    pDirectory->Size = BlockSize - j;
    pDirectory->Name[0] = pDirectory->Name[1] = '.';
    pDirectory->Name[2] = pDirectory->Name[3] = '\0';


    return CommitBlock(hDev, fs, 0, i+1, SectorsPerBlock,
                        StartLogicalSectorAddress, SectorsPerBlock);
}

NvError 
NvExt2PrivFormatPartition(
    NvDdkBlockDevHandle hDev,
    const char    *NvPartitionName)
{
    NvPartInfo Partition;
    NvU32      PartitionId;
    NvError    e;
    NvDdkBlockDevInfo BdInfo;
    FileSystem *fs = NULL;
    NvU8 *pUuid;
    NvU32 i, Num, j, SectorsPerBlock, BlockSize;
    NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs EraseArg;
    NvRmDeviceHandle    hRm = NULL;
    NvU32 NumGroups;
    NvU32 Block;
    NvU8 *SuperBlockData;
    NvFsMountInfo FsMountInfo;
    NvU8 *buff = NULL;
    NvU8 *tempbuff = NULL;
    NvU32 Addr =0;
    NvU32 multtab[]= {3,5,7};
    NvU32 n=0,s=0, mult;

#if WAR_SD_FORMAT_PART
    NvPartInfo PartPTInf;
    const char PartitionPTName[] = "PT";
    NvU32 PartitionPTId;

    // FIXME: SD partial erase does not work so using this hack
    // Workaround to tell if previous partition is PT
    // Assumption that partition table is named PT
    // Get Partition id for given partition name
    NV_CHECK_ERROR_CLEANUP(
        NvPartMgrGetIdByName(PartitionPTName, &PartitionPTId)
    );
    // get partition size and location
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionPTId, &PartPTInf));
#endif

    if (!hDev || !NvPartitionName)
    {
        return NvError_BadParameter;
    }

    NV_CHECK_ERROR_CLEANUP(NvRmOpen(&hRm, 0));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetIdByName(NvPartitionName, &PartitionId));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetPartInfo(PartitionId, &Partition));
    NV_CHECK_ERROR_CLEANUP(NvPartMgrGetFsInfo(PartitionId, &FsMountInfo));
    
    hDev->NvDdkBlockDevGetDeviceInfo(hDev, &BdInfo);

    // Erase entire partition before writing anything
    EraseArg.StartLogicalSector = (NvU32)Partition.StartLogicalSectorAddress;
    EraseArg.NumberOfLogicalSectors = (NvU32)Partition.NumLogicalSectors;
    EraseArg.IsPTpartition = (PartitionId == PartitionPTId)? NV_TRUE : NV_FALSE;
    EraseArg.IsTrimErase = NV_FALSE;
    EraseArg.IsSecureErase = NV_FALSE;
#if WAR_SD_FORMAT_PART
    // FIXME:
    // We check that previous partition is not PT. If previous
    // partition is other than PT we allow erase of erasable group for SD
    EraseArg.PTendSector = (NvU32)(PartPTInf.StartLogicalSectorAddress +
        PartPTInf.NumLogicalSectors);
#else
    EraseArg.PTendSector = 0;
#endif

    NV_CHECK_ERROR_CLEANUP(
        hDev->NvDdkBlockDevIoctl(
                            hDev, NvDdkBlockDevIoctlType_ErasePartition,
                            sizeof(NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs),
                            0, (const void *)&EraseArg, NULL)
    );

    if (BdInfo.BytesPerSector > 4096)
    {
        e = NvError_NotSupported;
        goto fail;
    }
    if (BdInfo.BytesPerSector < 1024)
    {
        SectorsPerBlock = 1024 / BdInfo.BytesPerSector;
        Num = (NvU32)Partition.NumLogicalSectors / SectorsPerBlock;
        BlockSize = 1024;
    }
    else
    {
        BlockSize = BdInfo.BytesPerSector;
        SectorsPerBlock = 1;
        Num = (NvU32)Partition.NumLogicalSectors;
    }

#if NVOS_IS_LINUX && !defined(NV_EMBEDDED_BUILD)
    {
	/*
	 * android expects that the actual user data file system size should be <=
	 * the block device size, which is a valid requirement. to add to it
	 * it maintains a footer containing the partition's encryption info
	 * at the end of the fs. so the actual user data fs size should not be
	 * greater than (partition size - footer size).
	 */
	NvU32 FooterSize = NvOdmQueryDataPartnEncryptFooterSize(NvPartitionName);
	Num -= (FooterSize / BlockSize);
    }
#endif

    NV_CHECK_ERROR_CLEANUP(
        AllocFileSystem(Num, BlockSize, &fs)
    );

    NV_CHECK_ERROR_CLEANUP(
        InitializeSuperBlock(hDev, fs, BlockSize, SectorsPerBlock,
                                Partition.StartLogicalSectorAddress)
    );

    /* This implements the version 4 (random) UUID generation scheme as
     * described at http://en.wikipedia.org/wiki/UUID */
    pUuid = ((NvU8 *)fs->pSb) + NV_EXT2_SUPERBLOCK_UUID_OFFSET;
    NV_CHECK_ERROR_CLEANUP(
        NvRmGetRandomBytes(hRm, NV_EXT2_SUPERBLOCK_UUID_LENGTH, pUuid)
    );
    pUuid[6] = (pUuid[6] & 0x0f) | 0x40;
    pUuid[8] = (pUuid[8] & 0x3f) | 0x80;

    if (((NvFsMgrFileSystemType)FsMountInfo.FileSystemType == NvFsMgrFileSystemType_Ext3) ||
         ((NvFsMgrFileSystemType)FsMountInfo.FileSystemType == NvFsMgrFileSystemType_Ext4))
    {
        // Create Journaling info for ext3 format
        NV_CHECK_ERROR_CLEANUP(CreateJournalInfo(hDev, fs, SectorsPerBlock,
                                    Partition.StartLogicalSectorAddress));

        // update required superblock fields to enable rev 1 fs
        fs->pSb->RevLevel = 1;
        fs->pSb->FirstInodeNumber = NV_EXT2_FIRST_INODE_INDEX;
        fs->pSb->InodeSize = sizeof(NvExt2Inode);   //128
    }

    if ((NvFsMgrFileSystemType)FsMountInfo.FileSystemType == NvFsMgrFileSystemType_Ext4)
    {
        fs->pSb->InCompatibilityFeatureSet |=  NV_EXT2_FEATURE_INCOMPAT_EXTENTS;
    }

    // need to burn super block and group desciptors for each group
    NumGroups = DIV_CEIL(fs->pSb->BlockCount, fs->pSb->BlocksPerGroup);
    // first data block contains the super block
    SuperBlockData = GetBlock(hDev, fs, 0, fs->pSb->FirstDataBlock,
                                SectorsPerBlock,
                                Partition.StartLogicalSectorAddress);

    if (SuperBlockData)
    {
        for(n = 0; n < 3; n++)
        {
            mult = multtab[n];
            s = n ? mult : 1;
            for (i = s; i < NumGroups; i *= mult)
            {
                Addr = ((i * fs->pSb->BlocksPerGroup) +
                                fs->pSb->FirstDataBlock) * SectorsPerBlock +
                                (NvU32)Partition.StartLogicalSectorAddress;

                NV_CHECK_ERROR_CLEANUP(
                    hDev->NvDdkBlockDevWriteSector(hDev, Addr, SuperBlockData,
                                                    SectorsPerBlock)
                );
            }
        }
    }

    buff = NvOsAlloc(fs->GroupDescBlocks*BlockSize);
    if(buff == NULL)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    tempbuff = buff;

    for (j = 0; j < fs->GroupDescBlocks; j++)
    {
        NvU8 *GroupDescBlockData = 0;
        Block = fs->pSb->FirstDataBlock + 1 + j;
        GroupDescBlockData = GetBlock(hDev, fs, 0, Block,
                                        SectorsPerBlock,
                                        Partition.StartLogicalSectorAddress);
        if (!GroupDescBlockData)
        {
            break;
        }

        NvOsMemcpy(buff,GroupDescBlockData, sizeof(GroupDescBlockData));
        buff+=sizeof(GroupDescBlockData);
    }

    for(n = 0; n < 3; n++)
    {
        mult = multtab[n];
        s = n ? mult : 1;
        for (i = s; i < NumGroups; i *= mult)
        {
            Block = fs->pSb->FirstDataBlock + 1;
            Block += i * fs->pSb->BlocksPerGroup;
            Addr = Block * SectorsPerBlock +
                    (NvU32)Partition.StartLogicalSectorAddress;
            NV_CHECK_ERROR_CLEANUP(
                hDev->NvDdkBlockDevWriteSector(hDev, Addr,tempbuff,
                                                SectorsPerBlock * fs->GroupDescBlocks)
            );
        }
    }

    for (i=0; i < fs->NumBlocks; i++)
    {
        if ((fs->BlockPtrs[i] == (NvU8 *)-1) || (fs->BlockPtrs[i] == 0))
            continue;
        {
            NvU32 Addr = i * SectorsPerBlock +
                            (NvU32)Partition.StartLogicalSectorAddress;

            NV_CHECK_ERROR_CLEANUP(
                hDev->NvDdkBlockDevWriteSector(hDev, Addr, fs->BlockPtrs[i],
                                                SectorsPerBlock)
            );
        }
    }

 fail:

    if (fs)
    {
        for (i=0; i<fs->NumBlocks; i++)
        {
             if ((fs->BlockPtrs[i] != (NvU8 *)-1) && (fs->BlockPtrs[i] != 0))
             {
                NvOsFree(fs->BlockPtrs[i]);
            }
        }
        NvOsFree(fs);
    }
    NvRmClose(hRm);
    if(tempbuff)
    {
        NvOsFree(tempbuff);
        tempbuff = NULL;
    }
    return e;
}
