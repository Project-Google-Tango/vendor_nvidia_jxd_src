/**
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */


#include "t30_data_layout.h"
/* For ComputeRandomAesBlock, ReadBlImage */
#include "t30_set.h"
#include "nvassert.h"
#include "nvapputil.h"
#include "t30/nvboot_version.h"
#include "t30/nvboot_bct.h"
#include "../nvbuildbct_util.h"

#define ENABLE_DEBUG 0

/* Function prototypes */
static void  UpdateRandomAesBlock(BuildBctContext *Context);
static void InsertPadding(NvU8 *Data, NvU32 Length);
static void WritePadding(NvU8 *Data, NvU32 Length);
static void WriteBct(BuildBctContext *Context, NvU32 Block, NvU32 BctSlot);

static void
SetBlData(BuildBctContext *Context,
          NvU32              Instance,
          NvU32              StartBlock,
          NvU32              StartPage,
          NvU32              Length);

static void FillBootloaderData(BuildBctContext *Context, UpdateEndState EndState);
static void BeginUpdate (BuildBctContext *Context, UpdateEndState EndState);
static void FinishUpdate(BuildBctContext *Context, UpdateEndState EndState);

static void UpdateRandomAesBlock(BuildBctContext *Context)
{
    switch (Context->RandomBlockType)
    {
    case RandomAesBlockType_Zeroes:
    case RandomAesBlockType_Literal:
    case RandomAesBlockType_RandomFixed:
        /* The random block was updated when parsed. */
        break;
    case RandomAesBlockType_Random:
        t30ComputeRandomAesBlock(Context);
        break;
    default:
        NV_ASSERT(!"Unknown random block type.");
        return;
    }
}

static void InsertPadding(NvU8 *Data, NvU32 Length)
{
    NvU32        AesBlocks;
    NvU32        Remaining;

    AesBlocks = ICeilLog2(Length, NVBOOT_AES_BLOCK_SIZE_LOG2);
    Remaining = (AesBlocks << NVBOOT_AES_BLOCK_SIZE_LOG2) - Length;
    WritePadding(Data + Length, Remaining);
}

static void WritePadding(NvU8 *p, NvU32 Remaining)
{
    NvU8 RefVal = 0x80;

    while (Remaining)
    {
        *p++ = RefVal;
        Remaining--;
        RefVal = 0x00;
    }
}

static void WriteBct(BuildBctContext *pContext, NvU32 Block, NvU32 BctSlot)
{
    NvU32 BctLength;
    NvU8 *pBuffer = NULL;

    NV_ASSERT(pContext);

    BctLength = pContext->NvBCTLength;

    /* Create a local copy of the BCT data */
    pBuffer = NvOsAlloc(BctLength);
    if (pBuffer == NULL)
        goto fail;
    NvOsMemcpy(pBuffer, pContext->NvBCT, BctLength);

    InsertPadding(pBuffer, BctLength);
    /* If output file is specified then Write the data to file
     * else retain the buffer
     */
    if (pContext->RawFile)
        NvOsFwrite(pContext->RawFile, pBuffer, BctLength);
    else
        pContext->pBuffer = pBuffer;

fail:
    if (pContext->RawFile && pBuffer)
        NvOsFree(pBuffer);
}

/**** TODO: Add hash ****/
static void
SetBlData(BuildBctContext *Context,
          NvU32              Instance,
          NvU32              StartBlock,
          NvU32              StartPage,
          NvU32              Length)
{
    NvBootConfigTable *Bct = NULL;

    NV_ASSERT(Context);

    Bct = (NvBootConfigTable*)(Context->NvBCT);

    Bct->BootLoader[Instance].Version     = Context->Version;
    Bct->BootLoader[Instance].StartBlock  = Context->NewBlStartBlk;
    Bct->BootLoader[Instance].StartPage   = Context->NewBlStartPg;
    Bct->BootLoader[Instance].Length      = Length;
    Bct->BootLoader[Instance].LoadAddress = Context->NewBlLoadAddress;
    Bct->BootLoader[Instance].EntryPoint  = Context->NewBlEntryPoint;
}

static void FillBootloaderData(BuildBctContext *Context, UpdateEndState EndState)
{
    NvU32  BlActualSize = 0; /* In bytes */
    NvU32 CurrentBlock = 0;
    NvU32 CurrentPage = 0;

    SetBlData(Context,
                  0,// assuming only one bootloader is active at the moment
                  CurrentBlock,
                  CurrentPage,
                  BlActualSize);
}

static void InitBadBlockTable(BuildBctContext *Context)
{
    NvU32                BytesPerEntry;
    NvBootBadBlockTable *Table;
    NvBootConfigTable*Bct = NULL;

    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context      != NULL);
    NV_ASSERT(Bct != NULL);

    Table = &(Bct->BadBlockTable);

    /* Validate context values. */
    if ((Context->PartitionSize % Context->BlockSize) != 0)
    {
        NvAuPrintf("Partition size is not an integral number of blocks\n");
        return;
    }

    /* Initialize the bad block table. */
    Table->BlockSizeLog2        = (NvU8)(Context->BlockSizeLog2);
    BytesPerEntry               = ICeil(Context->PartitionSize,
                                           NVBOOT_BAD_BLOCK_TABLE_SIZE);
    Table->VirtualBlockSizeLog2 = (NvU8) NV_MAX(CeilLog2(BytesPerEntry),
                                                Table->BlockSizeLog2);
    Table->EntriesUsed          = ICeilLog2(Context->PartitionSize,
                                            Table->VirtualBlockSizeLog2);

#if ENABLE_DEBUG
    {
        NvAuPrintf("InitBadBlockTable():\n");
        NvAuPrintf("  BCT->BlockSizeLog2 = %d (%d bytes)\n",
                   Bct->BlockSizeLog2,
                   1 << Bct->BlockSizeLog2);

        NvAuPrintf("  BCT->PartitionSize = %d\n",
                    Bct->PartitionSize);

        NvAuPrintf("  BBT->BlockSizeLog2 = %d (%d bytes)\n",
                    Bct->BadBlockTable.BlockSizeLog2,
                   1 <<  Bct->BadBlockTable.BlockSizeLog2);

        NvAuPrintf("  BBT->VirtualBlockSizeLog2 = %d (%d bytes)\n",
                    Bct->BadBlockTable.VirtualBlockSizeLog2,
                   1 <<  Bct->BadBlockTable.VirtualBlockSizeLog2);

        NvAuPrintf("  BBT->EntriesUsed = %d\n",
                    Bct->BadBlockTable.EntriesUsed);
    }
#endif
}

static void BeginUpdate(BuildBctContext *Context, UpdateEndState EndState)
{
    NvBootConfigTable *Bct = NULL;
    Bct = (NvBootConfigTable*)(Context->NvBCT);

    NV_ASSERT(Context);

    /* Ensure that the BCT block & page data is current. */
#if ENABLE_DEBUG
        NvAuPrintf("BeginUpdate(): BCT data: b=%d p=%d\n",
                   Bct->BlockSizeLog2,
                   Bct->PageSizeLog2);
#endif

    Bct->BootDataVersion = NVBOOT_BOOTDATA_VERSION;
    Bct->BlockSizeLog2   = Context->BlockSizeLog2;
    Bct->PageSizeLog2    = Context->PageSizeLog2;
    Bct->PartitionSize   = Context->PartitionSize;
    FillBootloaderData(Context, EndState);
    /* Update the random AES block and store in the BCT. */
    UpdateRandomAesBlock(Context);
    NvOsMemcpy(&(Bct->RandomAesBlock),
               &(Context->RandomBlock),
               sizeof(NvBootHash));

    /* Ensure the bad block table data is up to date. */
    InitBadBlockTable(Context);

     /* Fill the reserved data w/the padding pattern. */
    WritePadding(Bct->Reserved, NVBOOT_BCT_RESERVED_SIZE);

    WriteBct(Context, 0, 0);
}

static void FinishUpdate(BuildBctContext *Context, UpdateEndState EndState)
{
    Context->BctWritten = NV_TRUE;
}

void t30UpdateBct(BuildBctContext *Context, UpdateEndState EndState)
{
    BeginUpdate(Context, EndState);
    FinishUpdate(Context, EndState);
}

void t30UpdateBl(BuildBctContext *Context, UpdateEndState EndState)
{
    FillBootloaderData(Context, EndState);
}

