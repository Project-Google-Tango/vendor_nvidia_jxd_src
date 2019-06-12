/*
 * Copyright (c) 2008 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvml.h"
#include "nvml_device.h"
#include "nvml_utils.h"
#include "nvml_aes.h"
#include "nvml_hash.h"

/*
 * NvMlReaderObjDesc: Description of an object to load.
 */
typedef struct NvMlReaderObjDescRec
{
    NvS32       BlIndex; /* -1 for BCTs, 0 to 3 for BLs */
    NvU32       StartBlock;
    NvU32       StartPage;
    NvU32       Length; /* in bytes */
    NvU32       CryptoOffset;
    NvBootHash *HashRef;
} NvMlReaderObjDesc;

/*
 * NvMlReaderState: Internal state of the the object reader.
 */
typedef struct NvMlReaderStateRec
{
    NvMlDevMgr        *DevMgr;        /* Pointer to the device manager  */
    NvBootBadBlockTable *BadBlockTable; /* Pointer to the bad block table */

    NvU32  ChunkSize;
    NvU32  ChunkSizeLog2;

    NvU32  ChunksRemainingForDev;
    NvU32  ChunksRemainingForAes;

    /* Source information */
    NvMlReaderObjDesc *ObjDesc;
    NvU32             NumCopies;

    /* Current read information */
    NvU32  ActiveCopy; /* Tracks the copy being read from */
    NvU32  DevReadBlock;
    NvU32  DevReadPage;
    NvU32  DevLogicalPage;
    NvU32  RedundantRdBlock;
    NvU32  RedundantRdPage;

    /* Unit idle status */
    NvBool AesIsIdle;
    NvBool DevIsIdle;

    /* Buffer information */
    NvU8   BuffersForDev;
    NvU8   BuffersForAes;
    NvU8   DevDstBuf;
    NvU8   AesSrcBuf;
    NvBootAesIv HashK1; /* IV buffer used by the hash code. */

    /* Destinations */
    NvU8  *ObjDst;
    NvBootHash HashDst;

    /* Flags to trigger special work w/first chunk. */
    NvBool IsFirstChunkForDevice;
    NvBool IsFirstChunkForDecrypt;
    NvBool IsFirstChunkForHash;

    /* Crypto flags */
    NvBool DecryptObject;
    NvBool HashObject;

} NvMlReaderState;

NvMlReaderState ReaderState;
NvBootAesEngine g_HashEngine;
NvBootAesKeySlot g_HashSlot;
extern NvBootInfoTable *pBitTable;
NvMlDevContext s_DeviceContext;
NvBctHandle BctDeviceHandle = NULL;
NvU8 *BctDeviceHandlePtr;

//user defined boot device config fuse value
static NvU32 s_BootDeviceConfig = 0;
//user defined boot device type fuse value
static NvBootFuseBootDevice s_BootDeviceType = NvBootFuseBootDevice_Max;
extern NvMlDevMgrCallbacks s_DeviceCallbacks[];

/*
 * Increment a buffer index into a circular buffer.
 */
#define BUFFER_INCR(idx, count) (((idx) == ((count) - 1)) ? 0 : ((idx) + 1))

#define BUFFER_ADDR(index) (BufferMemory + ((index) << State->ChunkSizeLog2))

#define NVBOOT_READER_NUM_BUFFERS 2
#define MAX_BCT_COPY 63

NvU8 NV_ALIGN(4096) BufferMemory[8192]; //should be 8k

#ifndef NVBOOT_SSK_ENGINE
#define NVBOOT_SSK_ENGINE NvBootAesEngine_B
#endif

#ifndef NVBOOT_SBK_ENGINE
#define NVBOOT_SBK_ENGINE NvBootAesEngine_A
#endif

static NvBootError
InitDevice(NvMlDevMgr *DevMgr, NvU32 ParamIndex)
{
    NvBool            ValidParams;
    void                *Params;
    NvBootError e = NvBootError_Success;

    /* Query the boot parameters from the device. */
    DevMgr->Callbacks->GetParams(ParamIndex, &Params);

    /* Validate the parameters */
    ValidParams = DevMgr->Callbacks->ValidateParams(Params);
    if (ValidParams == NV_FALSE) return NvBootError_InvalidDevParams;

    /* Initialize the device. This must occur before querying the block sizes*/
    e = DevMgr->Callbacks->Init(Params, &s_DeviceContext);

    if (e != NvBootError_Success)
        return e;

    /* Query the block sizes. */
    DevMgr->Callbacks->GetBlockSizes(Params,
                                     &DevMgr->BlockSizeLog2,
                                     &DevMgr->PageSizeLog2);

    return e;
}

NvBootError NvMlDevMgrInit(NvMlDevMgr     *DevMgr,
                             NvBootDevType  DevType,
                             NvU32             ConfigIndex)
{
    NvBootError e = NvBootError_Success;

    if (DevMgr == NULL) return NvBootError_InvalidParameter;

    /*
     * Assign the function pointers and device class.
     * The subtraction by NvBootDeviceType_Nand maps the range
     * of legal values to a 0-based range suitable for indexing into the
     * s_DeviceCallbacks[] table.
     */
    DevMgr->Callbacks = &(s_DeviceCallbacks[DevType]);

    /* Initialize the device. */
    NV_BOOT_CHECK_ERROR_CLEANUP(InitDevice(DevMgr, ConfigIndex));

    /* Initialization is complete! */
    return e;

fail:
    /**** TODO: Determine if this is really necessary ****/

    DevMgr->Callbacks   = NULL;

    return e;
}

NvBootError NvMlBctDeviceHandleInit(void)
{
    NvError e = NvSuccess;
    NvU32 BctLength = 0;

    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, NULL, NULL));
    BctDeviceHandlePtr = NvOsAlloc(BctLength);
    if (!BctDeviceHandlePtr)
    {
        e = NvError_InvalidState;
        goto fail;
    }
    NV_CHECK_ERROR_CLEANUP(NvBctInit(&BctLength, BctDeviceHandlePtr, &BctDeviceHandle));
    return NvBootError_Success;

fail:
    return NvBootError_NotInitialized;
}

static void NvMlBctDeviceHandleDeInit(void)
{
    if (BctDeviceHandlePtr)
        NvOsFree(BctDeviceHandlePtr);
    if (BctDeviceHandle)
        NvBctDeinit(BctDeviceHandle);
}

static void UpdateCryptoStatus(NvMlReaderState *State)
{
    /*
     * When not decrypting, consider the status to be Idle to ensure
     * the correct path through the logic that follows.
     */
    NvBool DecryptIsBusy = NV_FALSE;
    NvBool HashIsBusy    = NV_FALSE;

    /* TODO: Revisit this code when using the hash implementation. */
    /* TODO: Revisit this code after the aes update has happened.
     *       It may be safe to unconditionally perform the two busy queries
     *       below.
     */

    if (State->DecryptObject == NV_TRUE)
    {
        DecryptIsBusy = NvMlAesIsEngineBusy(NVBOOT_SBK_ENGINE);
    }

    if (State->HashObject == NV_TRUE)
    {
        HashIsBusy = NvMlAesIsEngineBusy(g_HashEngine);
    }

    /*
     * Update buffer state if newly idle.
     */
    if (State->AesIsIdle == NV_FALSE &&
        !DecryptIsBusy &&
        !HashIsBusy)
    {
        State->AesIsIdle = NV_TRUE;
        State->BuffersForDev++;
        State->ObjDst += State->ChunkSize;
    }
}

static void
StartCryptoEngine(
    NvBootAesEngine   Engine,
    NvBootAesKeySlot  Slot,
    NvBool           *First,
    NvU32             BlockCnt,
    NvU8             *Src,
    NvU8             *Dst)
{
    NV_ASSERT(First != NULL);
    NV_ASSERT(Src   != NULL);
    NV_ASSERT(Dst   != NULL);

    if (*First)
    {
        /* Select the correct engine & slot */
        NvMlAesSelectKeyIvSlot(Engine, Slot);
        /* Clear the First flag. */
        *First = NV_FALSE;
    }

    /* Launch the decryption of the chunk. */
    NvMlAesStartEngine(Engine, BlockCnt, Src, Dst, NV_FALSE);
}

static NvBootError LaunchDevRead(NvMlReaderState *State)
{
    NvBootError  e;
    NvU8        *Dst;
    NvU32        PagesPerBlock;
    State->ActiveCopy = 0; /* Always start from the primary copy */
    State->DevIsIdle = NV_FALSE;
    State->ChunksRemainingForDev--;
    State->BuffersForDev--;
    State->DevDstBuf = BUFFER_INCR(State->DevDstBuf,
                                   NVBOOT_READER_NUM_BUFFERS);

    Dst = BUFFER_ADDR(State->DevDstBuf);

    PagesPerBlock = 1 << (State->DevMgr->BlockSizeLog2 -
                          State->DevMgr->PageSizeLog2);

    if (State->IsFirstChunkForDevice == NV_TRUE)
    {
        /*
         * Skip the incrementing of the pages & blocks.
         * Also, the first page should land on a known good
         * block.
         */
        State->IsFirstChunkForDevice = NV_FALSE;
    }
    else
    {
    State->DevLogicalPage++;
    State->DevReadPage++;

    if (State->DevReadPage >= PagesPerBlock)
    {
        /* Advance to the next known good block. */
        State->DevReadPage = 0;
            State->DevReadBlock++;
        }
    }

    /* Skip past known bad blocks. */
    //FIXME?
    /* Initiate the page read. */

    e = State->DevMgr->Callbacks->ReadPage(State->DevReadBlock,
                                           State->DevReadPage,
                                           Dst);

    return e;
}

static NvBootError UpdateDevStatus(NvMlReaderState *State)
{
    NvBootDeviceStatus  Status;
    NvS32               BlIndex;

    NV_ASSERT(State != NULL);

    if (State->IsFirstChunkForDevice)
    {
        /* Skip querying the device and simply return success. */
        return NvBootError_Success;
    }

    Status  = State->DevMgr->Callbacks->QueryStatus();
    BlIndex = State->ObjDesc[State->ActiveCopy].BlIndex;
    BlIndex = BlIndex;

    /* Error status at this point indicates that no recovery is possible. */
    if ((Status == NvBootDeviceStatus_ReadFailure) ||
        (Status == NvBootDeviceStatus_EccFailure))
    {
        return NvBootError_DeviceReadError;
    }

    /* Check if the device finished reading a chunk. */
    if (Status == NvBootDeviceStatus_Idle &&
        State->DevIsIdle == NV_FALSE)
    {
        /* The device finished work since last we checked. */
        State->DevIsIdle = NV_TRUE;
        State->BuffersForAes++;
    }

    return NvBootError_Success;
}

static void BlockForCryptoFinish(NvMlReaderState *State)
{
    while (NvMlAesIsEngineBusy(NVBOOT_SBK_ENGINE));
    while (NvMlAesIsEngineBusy(g_HashEngine));
}

static void BlockForDevFinish(NvMlReaderState *State)
{
    NvBootDeviceStatus Status;

    NV_ASSERT(State != NULL);

    do
    {
        Status = State->DevMgr->Callbacks->QueryStatus();
    } while (Status == NvBootDeviceStatus_ReadInProgress);
}

static NvBool
IsValidPadding(NvU8 *Data, NvU32 Length)
{
    NvU32        AesBlocks;
    NvU32        Remaining;
    NvU8        *p;
    NvU8         RefVal;

    AesBlocks = NV_ICEIL_LOG2(Length, NVBOOT_AES_BLOCK_LENGTH_LOG2);
    Remaining = (AesBlocks << NVBOOT_AES_BLOCK_LENGTH_LOG2) - Length;

    p = Data + Length;
    RefVal = 0x80;

    while (Remaining)
    {
        if (*p != RefVal) return NV_FALSE;
        p++;
        Remaining--;
        RefVal = 0x00;
    }

    return NV_TRUE;
}

static NvBool CheckCryptoHash(NvBootHash *Hash1, NvBootHash *Hash2)
{
    int i;

    NV_ASSERT(Hash1 != NULL);
    NV_ASSERT(Hash2 != NULL);

    for (i = 0; i < NVBOOT_AES_BLOCK_LENGTH; i++)
    {
        if (Hash1->hash[i] != Hash2->hash[i]) return NV_FALSE;
    }

    return NV_TRUE;
}

static void
LaunchCryptoOps(NvMlReaderState *State)
{
    NvU32  NumBlocks;
    NvU8  *Src;
    NvBool First;
    NvBool Last;
    NvU32  Offset; /* Byte offset to usable data. */

    NV_ASSERT(State != NULL);

    /* Update Aes state & buffers */
    State->AesIsIdle = NV_FALSE;
    State->BuffersForAes--;
    State->ChunksRemainingForAes--;
    State->AesSrcBuf = BUFFER_INCR(State->AesSrcBuf,
                                   NVBOOT_READER_NUM_BUFFERS);

    /* TODO: Replace with a single first flag.*/
    First = State->IsFirstChunkForDecrypt || State->IsFirstChunkForHash;

    /* Adjust for first block, which has a crypto offset for BCT. */
    if (First)
    {
        Offset = State->ObjDesc[0].CryptoOffset;
    }
    else
    {
        Offset = 0;
    }

    if (State->ChunksRemainingForAes == 0)
    {
        NvU32 Remaining;

        /* This is the final chunk, so only read the number of blocks
         * necessary.
         */
        /* Remaining = Length % ChunkSize; */
        Remaining = State->ObjDesc[0].Length -
            ((State->ObjDesc[0].Length >> State->ChunkSizeLog2) <<
             State->ChunkSizeLog2);
        NumBlocks = NV_ICEIL_LOG2(Remaining, NVBOOT_AES_BLOCK_LENGTH_LOG2);
    }
    else
    {
        NumBlocks = 1 << (State->ChunkSizeLog2 - NVBOOT_AES_BLOCK_LENGTH_LOG2);
    }

    Src = BUFFER_ADDR(State->AesSrcBuf);

    NumBlocks -= Offset >> NVBOOT_AES_BLOCK_LENGTH_LOG2;

    /*
     * Copy the skipped section to the destination.
     * This happens for BCTs, which is when CryptoOffset > 0.
     */
    memcpy(State->ObjDst, Src, State->ObjDesc[0].CryptoOffset);

    if (State->DecryptObject == NV_TRUE)
    {
        StartCryptoEngine(NVBOOT_SBK_ENGINE,
                          NVBOOT_SBK_DECRYPT_SLOT,
                          &(State->IsFirstChunkForDecrypt),
                          NumBlocks,
                          Src + Offset,
                          State->ObjDst + Offset);
    }
    else
    {
        /* Copy the bootloader data to memory. */
        memcpy(State->ObjDst + Offset,
               Src + Offset,
               NumBlocks << NVBOOT_AES_BLOCK_LENGTH_LOG2);
        /* TODO: Set up DMA to perform the transfer. */

        State->IsFirstChunkForDecrypt = NV_FALSE;
    }

    if (State->HashObject == NV_TRUE)
    {
        if (First)
        {
            NvMlAesSelectKeyIvSlot(g_HashEngine, g_HashSlot);

            NvMlHashGenerateSubkeys(g_HashEngine, &(State->HashK1));

            State->IsFirstChunkForHash = NV_FALSE;
        }

        /* Loop over the AES blocks in this buffer */
        Src += Offset;
        Last = (State->ChunksRemainingForAes == 0);

        NvMlDoHashBlocks(g_HashEngine,
                         &(State->HashK1),
                         Src,
                         &(State->HashDst),
                         NumBlocks,
                         First,
                         Last);

    }
}

static void
InitReaderState(NvMlReaderState   *State,
                NvMlReaderObjDesc *ObjDesc,
                NvU32                NumCopies,
                NvU8                *ReadDst,
                NvMlContext       *Context)
{
    State->DevMgr = &(Context->DevMgr);

    /*
     * Initialize the bad block table pointer to NULL.  It will be overridden
     * by NvBootReadOneObject.
     */
    State->BadBlockTable = NULL;

    State->ChunkSizeLog2 = State->DevMgr->PageSizeLog2;
    State->ChunkSize     = (1 << State->ChunkSizeLog2);

    State->ChunksRemainingForDev = NV_ICEIL_LOG2(ObjDesc->Length,
                                                 State->ChunkSizeLog2);
    State->ChunksRemainingForAes = State->ChunksRemainingForDev;

    State->ObjDesc   = ObjDesc;
    State->NumCopies = NumCopies;

    /* Setup the reading of the first chunk. */
    State->ActiveCopy      = 0;
    State->DevReadBlock    = ObjDesc->StartBlock;
    State->DevReadPage     = ObjDesc->StartPage;
    State->DevLogicalPage  = 0;

    /* Mark the units idle at the start. */
    State->DevIsIdle = NV_TRUE;
    State->AesIsIdle = NV_TRUE;

    /*
     * Initially assign all but 1 of the buffers to the reading device.
     * The extra buffer is used as the hash destination.
     */
    State->BuffersForDev = NVBOOT_READER_NUM_BUFFERS - 1;
    State->BuffersForAes = 0;

    /*
     * Initialize each component so that when they increment their
     * buffer index w/their first work, they'll use the correct buffer.
     */
    State->DevDstBuf     = 0; /* Will be 1 when incremented. */
    State->AesSrcBuf     = 0; /* Will be 1 when incremented. */

    /* Set the destination pointers. */
    State->ObjDst         = ReadDst;

    /*
     * Set the flags indicating first chunck for all units.
     */
    State->IsFirstChunkForDevice  = NV_TRUE;
    State->IsFirstChunkForDecrypt = NV_TRUE;
    State->IsFirstChunkForHash    = NV_TRUE;

    /* Crypto flags */
    /**** TODO: Update the following calculations. ****/
    State->DecryptObject = Context->DecryptBootloader;
    State->HashObject    = Context->CheckBootloaderHash;
}

static NvBootError
NvBootReadOneObject(
    NvMlContext       *Context,
    NvU8                *ReadDst,
    NvMlReaderObjDesc *ObjDesc,
    NvU32                NumCopies,
    NvBootBadBlockTable *BadBlockTable)
{
    NvBootError        e;
    NvMlReaderState *State = &ReaderState;

    NV_ASSERT(Context != NULL);
    NV_ASSERT(ReadDst != NULL);
    NV_ASSERT(ObjDesc != NULL);

    InitReaderState(State, ObjDesc, NumCopies, ReadDst, Context);

    if (State->HashObject == NV_TRUE)
    {
        NvMlUtilsInitHash(g_HashEngine);
    }

    State->BadBlockTable = BadBlockTable;

    /* Continue processing work while there is work to do. */
    while ((State->ChunksRemainingForAes > 0) ||
       (State->DevIsIdle == NV_FALSE)     ||
       (State->AesIsIdle == NV_FALSE))
    {
        /*
         * Handle the device reading in the boot loader.
         */
        /* Check on the device reader progress. */
        NV_BOOT_CHECK_ERROR_CLEANUP(UpdateDevStatus(State));

        /* Start the device reading another chunk if possible. */
        if ((State->DevIsIdle == NV_TRUE) &&
            (State->BuffersForDev > 0)    &&
            (State->ChunksRemainingForDev > 0))
        {
            NV_BOOT_CHECK_ERROR_CLEANUP(LaunchDevRead(State));
        }

        /*
         * Handle the decryption and signing in the AES engine.
         */
        /* Check on the crypto progress. */
        UpdateCryptoStatus(State);

        if ((State->AesIsIdle == NV_TRUE) &&
            (State->BuffersForAes > 0))
        {
            /* Start the decryption & validation of the buffer */
            LaunchCryptoOps(State);
        }
    }

    /*
     * Check the final checksum/crypto hash data, which resides at the
     * end of the hash buffer, with the supplied reference hash.
     */
    if (State->HashObject == NV_TRUE)
    {

        NvMlUtilsReadHashOutput(g_HashEngine, &(State->HashDst));
        NvMlUtilsDeInitHash(g_HashEngine);
        if (!CheckCryptoHash(ObjDesc[0].HashRef, &(State->HashDst)))
            return NvBootError_HashMismatch;

    }

    /* Check the padding */
    if (!IsValidPadding(ReadDst, ObjDesc[0].Length))
    {
        return NvBootError_ValidationFailure;
    }

    return NvBootError_Success;

fail:
    /* Clean up after an unrecoverable failure. */

    /* Spin wait for the pending work to finish or abort. */
    BlockForDevFinish   (State);
    BlockForCryptoFinish(State);

    return e;
}

static NvBootError
ValidateBct(NvMlContext *Context)
{
    NvU32 i;
    NvU32 BlockSize;
    NvU32 PageSize;
    NvU32 PagesPerBlock;
    NvU32 BlStart;
    NvU32 BlEnd;
    NvU32 BootDataVersion;
    NvU32 BlockSizeLog2;
    NvU32 PageSizeLog2;
    NvU32 PartitionSize;
    NvU32 NumParamSets;
    NvU32 NumSdramSets;
    NvU32 BootLoadersUsed;
    NvU8  Reserved;
    NvU32 BlStartPage;
    NvU32 BlStartBlock;
    NvU32 BlLength;
    NvU32 BlLoadAddress;
    NvU32 BlEntryPoint;
    NvU8  PadVal;

    NvError e  =  NvSuccess;
    NvU32 Size     = sizeof(NvU32);
    NvU32 Instance = 0;

    NV_ASSERT(Context != NULL);

    /* Check for matching data structure verisons. */
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_Version,
                     &Size, &Instance, &BootDataVersion)
    );

    if (BootDataVersion != NVBOOT_BOOTDATA_VERSION)
        return NvBootError_ValidationFailure;

    /* Check for legal block and page sizes. */
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_BootDeviceBlockSizeLog2,
                     &Size, &Instance, &BlockSizeLog2)
    );

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_BootDevicePageSizeLog2,
                     &Size, &Instance, &PageSizeLog2)
    );
    if ((BlockSizeLog2 < NVBOOT_MIN_BLOCK_SIZE_LOG2) ||
        (BlockSizeLog2 > NVBOOT_MAX_BLOCK_SIZE_LOG2) ||
        (PageSizeLog2  < NVBOOT_MIN_PAGE_SIZE_LOG2 ) ||
        (PageSizeLog2  > NVBOOT_MAX_PAGE_SIZE_LOG2 ))
    {
        return NvBootError_ValidationFailure;
    }

    /*
     * Verify that the BCT's block & page sizes match those used by the
     * device manager.
     */
    if ((BlockSizeLog2 != Context->DevMgr.BlockSizeLog2) ||
        (PageSizeLog2  != Context->DevMgr.PageSizeLog2))
    {
        return NvBootError_BctBlockInfoMismatch;
    }

    BlockSize     = 1 << BlockSizeLog2;
    PageSize      = 1 << PageSizeLog2;
    PagesPerBlock = 1 << (BlockSizeLog2 - PageSizeLog2);

    /*
     * Check the legality of the PartitionSize.
     * It should be a multiple of the block size.
     */
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_PartitionSize,
                     &Size, &Instance, &PartitionSize)
    );
    if (PartitionSize & (BlockSize - 1))
        return NvBootError_ValidationFailure;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_NumValidBootDeviceConfigs,
                     &Size, &Instance, &NumParamSets)
    );

    /* Check the range of NumParamSets */
    if (NumParamSets > NVBOOT_BCT_MAX_PARAM_SETS)
        return NvBootError_ValidationFailure;

    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_NumValidSdramConfigs,
                     &Size, &Instance, &NumSdramSets)
    );

    /* Check the range of NumSdramSets */
    if (NumSdramSets > NVBOOT_BCT_MAX_SDRAM_SETS)
        return NvBootError_ValidationFailure;

#if USE_BADBLOCKS

    /*
     * Check the BadBlockTable.
     */
    NvBootBadBlockTable *Table;
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_BadBlockTable,
                     &Size, &Instance, Table)
    );

    /* Check for matching block sizes.  */
    if (Table->BlockSizeLog2 != BlockSizeLog2)
        return NvBootError_ValidationFailure;

    /* Check for compatible block sizes.  */
    if (Table->BlockSizeLog2 > Table->VirtualBlockSizeLog2)
        return NvBootError_ValidationFailure;

    /* Check for the portion of the table used. */
    if (Table->EntriesUsed > NVBOOT_BAD_BLOCK_TABLE_SIZE)
        return NvBootError_ValidationFailure;

    if (Table->EntriesUsed !=
        NV_ICEIL_LOG2(PartitionSize, Table->VirtualBlockSizeLog2))
        return NvBootError_ValidationFailure;
#endif

    /* Check the range of NumSdramSets */
    NV_CHECK_ERROR_CLEANUP(
        NvBctGetData(BctDeviceHandle, NvBctDataType_NumEnabledBootLoaders,
                     &Size, &Instance, &BootLoadersUsed)
    );
    if (BootLoadersUsed > NVBOOT_MAX_BOOTLOADERS)
        return NvBootError_ValidationFailure;

    /*
     * Check the bootloader data.
     */
    for (i = 0; i < BootLoadersUsed; i++)
    {
        Instance = i;
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_BootLoaderStartSector,
                         &Size, &Instance, &BlStartPage)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_BootLoaderStartBlock,
                         &Size, &Instance, &BlStartBlock)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_BootLoaderLength,
                         &Size, &Instance, &BlLength)
        );
        if (BlStartPage >= PagesPerBlock)
            return NvBootError_ValidationFailure;

        /*
         * Check that the bootloader fits within the partition.
         * BlEnd stores the ending offset of the BL in the device.
         */
        BlEnd = BlStartBlock * BlockSize +
                BlStartPage  * PageSize  +
                BlLength;

        if (BlEnd > PartitionSize)
            return NvBootError_ValidationFailure;

        /*
         * Check the entry point.
         * BlStart and BlEnd are address in the destination memory.
         */
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_BootLoaderLoadAddress,
                         &Size, &Instance, &BlLoadAddress)
        );
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_BootLoaderEntryPoint,
                         &Size, &Instance, &BlEntryPoint)
        );
        BlStart = BlLoadAddress;
        BlEnd   = BlStart + BlLength;

        if ((BlEntryPoint < BlStart) ||
            (BlEntryPoint >= BlEnd))
            return NvBootError_ValidationFailure;
    }

    /* Check the Reserved field for the correct padding pattern. */
    PadVal = 0x80;

    for (i = 0; i < NVBOOT_BCT_RESERVED_SIZE; i++)
    {
        Instance = i;
        NV_CHECK_ERROR_CLEANUP(
            NvBctGetData(BctDeviceHandle, NvBctDataType_Reserved,
                         &Size, &Instance, &Reserved)
        );
        if (Reserved != PadVal)
            return NvBootError_ValidationFailure;
        PadVal = 0x00;
    }
    return NvBootError_Success;

fail:
    return NvBootError_InvalidParameter;
}

static NvBootError ReadOneBct(NvMlContext *Context, const NvU32 Block, const NvU32 Page)
{
    NvBootError  e;
    NvU8 *BctDst = (NvU8*)BctDeviceHandle;
    NvMlReaderObjDesc BctObjDesc;

    NV_ASSERT(Context != NULL);

    /* Setup an object description to read a single BCT w/no redundancy. */
    BctObjDesc.BlIndex      = -1; /* Marks the ObjDesc as a BCT. */
    BctObjDesc.StartBlock   = Block;
    BctObjDesc.StartPage    = Page;
    BctObjDesc.Length       = NvMlUtilsGetBctSize();
    BctObjDesc.CryptoOffset = 4 * NVBOOT_AES_KEY_LENGTH; /* in bytes */
    BctObjDesc.HashRef      = (NvBootHash *)BctDst; /* Start of result */

    /* Note: There is no bad block table available when reading the BCT. */
    NV_BOOT_CHECK_ERROR_CLEANUP(NvBootReadOneObject(Context,
                                            BctDst,
                                            &BctObjDesc,
                                            1,      /* Single obj desc */
                                            NULL)); /* No bad block table */

    /* Validate the BCT */
    NV_BOOT_CHECK_ERROR_CLEANUP(ValidateBct(Context));

    /* Update the BCT info in the BIT */
    pBitTable->BctValid = NV_TRUE;
    pBitTable->BctBlock = Block;
    pBitTable->BctPage  = Page;

fail:
    return e;
}

static NvBootError NvMlReadBct(NvMlContext *Context)
{
    NvU32         PagesPerBct;
    NvBootError   e = NvBootError_Success;
    NvMlDevMgr *DevMgr;
    NvU32 BctBlock = 0;
    // upto 63 copies of bct (except bct in 2nd slot of 1st block) be read from device
    NvU32 NumBctBlocks = MAX_BCT_COPY;

    /*
     * Update the BIT to reflect the fact that BCT reading has started:
     * - Set the size and BCT pointers.
     * - Update SafeStartAddr to the memory following the BCT
     */
    pBitTable->BctSize = NvMlUtilsGetBctSize();

    DevMgr = &(Context->DevMgr);

    PagesPerBct = NV_ICEIL_LOG2(pBitTable->BctSize,
                                DevMgr->PageSizeLog2);

    PagesPerBct = PagesPerBct;

    /*
     * Attempt to read the BCT from block 0, slot 0.
     * If this failed, check slot 1.
     * If this failed, read all the BCTs from the journal block and
     * keep the latest one.
     * Keep a record of useful failure information.
     */

    /* Read the BCT in block 0, slot 0. */
    for ( ;BctBlock < NumBctBlocks; BctBlock++)
    {
        e = ReadOneBct(Context, BctBlock, 0);
        if (e == NvBootError_Success)
            break;
    }

    return e;
}

NvBootError NvMlBootGetBct(NvU8 **bct, NvDdkFuseOperatingMode Mode)
{
    NvMlContext     Context;
    NvBootError e = NvBootError_Success;
    NvBootAesKey key;
    NvBootAesIv zeroIv;

    /* Initialize BctDeviceHandle */
    NV_BOOT_CHECK_ERROR_CLEANUP(NvMlBctDeviceHandleInit());
    NvMlAesInitializeEngine();
    NV_BOOT_CHECK_ERROR_CLEANUP(NvMlUtilsSetupBootDevice(&Context));

    Context.CheckBootloaderHash = NV_TRUE;

    if (Mode == NvDdkFuseOperatingMode_OdmProductionSecure)
    {
        Context.DecryptBootloader = NV_TRUE;
        g_HashEngine = NvBootAesEngine_B;
        g_HashSlot = NVBOOT_SBK_ENCRYPT_SLOT;
    }
    else
    {
        NvOsMemset(&key, 0, sizeof(key));
        NvOsMemset(&zeroIv, 0, sizeof(zeroIv));

        Context.DecryptBootloader = NV_FALSE;
        NvMlAesSetKeyAndIv(NvBootAesEngine_B, NvBootAesKeySlot_1,
                                                            &key, &zeroIv, NV_TRUE);

        g_HashEngine = NvBootAesEngine_B;
        g_HashSlot = NvBootAesKeySlot_1;
    }

    NV_BOOT_CHECK_ERROR_CLEANUP(NvMlReadBct(&Context));
    *bct = (NvU8*)BctDeviceHandle;

fail:
    NvMlBctDeviceHandleDeInit();
    return e;

}

NvBool NvMlDecryptValidateImage(NvU8 *pBootloader, NvU32 BlLength,
    NvBool IsFirstChunk, NvBool IsLastChunk, NvBool IsSign, NvBool IsBct, NvU8* BlHash)
{
    NvU32 NumBlocks = 0;
    NvU8 *BlDstBuf = NULL;
    static NvBootAesIv ZeroIv = {{0,0,0,0}};
    static NvBootHash HashDst;
    static NvBootHash *pHashSrc = NULL;
    // Decryption uses engineA while Hashing uses engineB, both uses slot0
    static NvBootAesKeySlot Slot = NVBOOT_SBK_ENCRYPT_SLOT;
    NvBool TmpFistChunk = IsFirstChunk;
    NvBool RetVal = NV_TRUE;

    // BL length must be multiple of 16 in order to use AES engine.
    if (BlLength % (1 << NVBOOT_AES_BLOCK_LENGTH_LOG2)){
        RetVal = NV_FALSE;
        goto fail;
    }
    NumBlocks = BlLength / (1 << NVBOOT_AES_BLOCK_LENGTH_LOG2);

    BlDstBuf = NvOsAlloc(BlLength);
    if (!BlDstBuf){
        RetVal = NV_FALSE;
        goto fail;
    }
    NvOsMemset(BlDstBuf, 0x0, BlLength);

    // Flush the sdram cache buffer so that all bootloader data are present
    // physically in SDRAM since AES does not work on cached buffer.
    if (!IsBct)
        NvOsDataCacheWriteback();

    if (IsFirstChunk)
    {
        NvMlAesInitializeEngine();
        NvMlAesSelectKeyIvSlot(NVBOOT_SBK_ENGINE, Slot);
        NvMlAesSelectKeyIvSlot(NVBOOT_SSK_ENGINE, Slot);
        if (IsSign)
            NvMlUtilsInitHash(NVBOOT_SSK_ENGINE);
    }
    while (NvMlAesIsEngineBusy(NVBOOT_SBK_ENGINE));
    while (NvMlAesIsEngineBusy(NVBOOT_SSK_ENGINE));
    StartCryptoEngine(NVBOOT_SBK_ENGINE,
                      Slot,
                      &TmpFistChunk,
                      NumBlocks,
                      pBootloader,
                      BlDstBuf);
    if (IsSign)
    {
        if (IsFirstChunk)
        {
            NvMlAesSelectKeyIvSlot(NVBOOT_SSK_ENGINE, Slot);
            NvMlHashGenerateSubkeys(NVBOOT_SSK_ENGINE, &ZeroIv);
            // determine whether validating bct or bootloader
            if (IsBct)
                pHashSrc = (NvBootHash*)(pBootloader - sizeof(NvBootHash));
            else
                pHashSrc = (NvBootHash*)BlHash;
        }
        NvMlDoHashBlocks(NVBOOT_SSK_ENGINE,
                                   &ZeroIv,
                                   pBootloader,
                                   &HashDst,
                                   NumBlocks,
                                   IsFirstChunk,
                                   IsLastChunk);
        if (IsLastChunk)
        {
            // read hash computed from engine and compare with that stored in bct
            NvMlUtilsReadHashOutput(NVBOOT_SSK_ENGINE, &HashDst);
            NvMlUtilsDeInitHash(NVBOOT_SSK_ENGINE);
            if (!CheckCryptoHash(pHashSrc, &HashDst)){
                RetVal = NV_FALSE;
                goto fail;
            }
        }
    }

    // copy decrypted data to bootloader in SDRAM.
    NvOsMemcpy(pBootloader, BlDstBuf, BlLength);

fail:
    if (BlDstBuf)
        NvOsFree(BlDstBuf);
    return RetVal;
}

void NvMlSetBootDeviceConfig(NvU32 ConfigIndex)
{
    s_BootDeviceConfig = ConfigIndex;
}
// Get boot device config value from fuses, if it is undefined
// substitute it with the user defined boot device config value.
void NvMlGetBootDeviceConfig(NvU32 *pConfigVal)
{
    NvMlUtilsGetBootDeviceConfig(pConfigVal);
    if (!*pConfigVal)
        *pConfigVal = s_BootDeviceConfig;
}

void NvMlSetBootDeviceType(NvBootFuseBootDevice DevType)
{
    s_BootDeviceType = DevType;
}
// Get boot device ID from fuses, if it is undefined
// substitute it with the user defined boot device type value.
void NvMlGetBootDeviceType(NvBootFuseBootDevice *pDevType)
{
    NvMlUtilsGetBootDeviceType(pDevType);
    if (*pDevType >= NvBootFuseBootDevice_Max)
        *pDevType = s_BootDeviceType;
}

//do nothing..dummy function
void NvMlSetMemioVoltage(NvU32 PllmFeedBackDivider)
{
}

#ifndef NVBOOT_SSK_ENGINE
#define NVBOOT_SSK_ENGINE NvBootAesEngine_B
#endif

#ifndef NVBOOT_SBK_ENGINE
#define NVBOOT_SBK_ENGINE NvBootAesEngine_A
#endif

NvBool NvMlCheckIfSbkBurned(void)
{
    NvU8 *plainText = "*this is a test*";
    NvU8 cipherTextA[NVBOOT_AES_BLOCK_LENGTH_BYTES], cipherTextB[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvBootAesKey key;
    NvBootAesIv zeroIv;
    NvU32 i;
    NvBool sbkBurned = NV_FALSE;

    //Now we need to figure out of the DK and SBK are burned
    NvOsMemset(&key, 0, sizeof(key));
    NvOsMemset(&zeroIv, 0, sizeof(zeroIv));

    //Step I: We take a random plaintext string, and encrypt it with the SBK.
    //We store this in cipherTextA
    NvMlAesInitializeEngine();
    NvMlAesSelectKeyIvSlot(NVBOOT_SBK_ENGINE, NVBOOT_SBK_ENCRYPT_SLOT);

    NvMlAesStartEngine(NVBOOT_SBK_ENGINE, 1, plainText, cipherTextA, NV_TRUE);

    while (NvMlAesIsEngineBusy(NVBOOT_SBK_ENGINE));

    //Step II: We take the same plaintext string and encrypt it
    //with a key and IV of zero. We store this in cipherTextB
    NvMlAesSetKeyAndIv(NvBootAesEngine_A, NvBootAesKeySlot_2, &key, &zeroIv, NV_TRUE);

    NvMlAesSelectKeyIvSlot(NvBootAesEngine_A, NvBootAesKeySlot_2);
    NvMlAesStartEngine(NvBootAesEngine_A, 1, plainText, cipherTextB, NV_TRUE);

    while (NvMlAesIsEngineBusy(NvBootAesEngine_A));

    //Step III: If cipherTextA == cipherTextB, it implies
    //that the SBK is zero (ie, not burned).
    sbkBurned = NV_FALSE;

    for (i=0;i<NVBOOT_AES_BLOCK_LENGTH_BYTES;i++)
    {
        if (cipherTextA[i] != cipherTextB[i])
        {
            sbkBurned = NV_TRUE;
            break;
        }
    }

    return sbkBurned;
}


Nv3pDkStatus NvMlCheckIfDkBurned(void)
{
    NvU8 *plainText = "*this is a test*";
    NvU8 cipherTextA[NVBOOT_AES_BLOCK_LENGTH_BYTES], cipherTextB[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvBootAesKey key, sskKey;
    NvBootAesIv zeroIv;
    NvU8 ssk[NVBOOT_AES_BLOCK_LENGTH_BYTES];
    NvU32 i, j;
    Nv3pDkStatus dkBurned;


    //Step I: We take a random plaintext string and encrypt it with the SSK.
    //We store this in cipherTextA.
    NvMlAesInitializeEngine();
    NvMlAesSelectKeyIvSlot(NVBOOT_SSK_ENGINE, NVBOOT_SSK_ENCRYPT_SLOT);
    NvMlAesStartEngine(NVBOOT_SSK_ENGINE, 1, plainText, cipherTextA, NV_TRUE);

    while (NvMlAesIsEngineBusy(NVBOOT_SSK_ENGINE));

    NvOsMemset(&key, 0, sizeof(key));
    NvOsMemset(&zeroIv, 0, sizeof(zeroIv));

    //Step II: We assume that SBK=DK=0. Then we generate
    //a new SSK value. SSK is a function of UID, SBK, DK:
    //SSK = f(UID, SBK, DK).
    NvMlAesSetKeyAndIv(NvBootAesEngine_A, NvBootAesKeySlot_2, &key, &zeroIv, NV_TRUE);
    NvMlUtilsSskGenerate(NvBootAesEngine_A, NvBootAesKeySlot_2, ssk);

    j = 0;
    //Convert the ssk key from a byte array to an NvU32 array
    for (i=0;i<NVBOOT_AES_BLOCK_LENGTH_BYTES;i+=4)
    {
        sskKey.key[j] = ssk[i+3] << 24 | ssk[i+2] << 16 | ssk[i+1] << 8 | ssk[i];
        j++;
    }

    //Step III: We use the new SSK to encrypt the same plaintext
    //string and store the result in cipherTextB.
    NvMlAesSetKeyAndIv(NvBootAesEngine_A, NvBootAesKeySlot_2, &sskKey, &zeroIv, NV_TRUE);
    NvMlAesSelectKeyIvSlot(NvBootAesEngine_A, NvBootAesKeySlot_2);
    NvMlAesStartEngine(NvBootAesEngine_A, 1, plainText, cipherTextB, NV_TRUE);

    while (NvMlAesIsEngineBusy(NvBootAesEngine_A));

    //Step IV: If cipherTextA == cipherTextB, then that
    //implies that the original SSK in step I and the
    //the new generated SSK in step II are the same.
    //This further implies that the DK is zero
    //since we know that the SBK is zero (We execute
    //this function only when SBK is zero) and the
    //UID is a constant. So the value of DK=0
    //that we plugged into SSK = f(UID, SBK, DK) is
    //correct.
    dkBurned = Nv3pDkStatus_NotBurned;
    for (i=0;i<NVBOOT_AES_BLOCK_LENGTH_BYTES;i++)
    {
        if (cipherTextA[i] != cipherTextB[i])
        {
            dkBurned = Nv3pDkStatus_Burned;
            break;
        }
    }

    return dkBurned;
}
