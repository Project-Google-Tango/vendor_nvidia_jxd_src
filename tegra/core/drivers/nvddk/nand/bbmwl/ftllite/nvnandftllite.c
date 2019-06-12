#include "nvddk_nand.h"
#include "nvassert.h"
#include "nvnandftllite.h"
#include "nvddk_nand_blockdev_defs.h"

enum {
    // This must match with nvddk_nand.c definition which currently
    // uses 8 chips as maximum. So array with 8 elements is checked
    // for calls to NvDdk APIs.
    // Constant representing maximum number of Nand interleaved chips
    MAX_INTERLEAVED_NAND_CHIPS = 8,
    // byte offset of runtime bad mark in spare area
    RUNTIME_BAD_BYTE_OFFSET = 1,
    // const for log2 MAX_INTERLEAVED_NAND_CHIPS
    LOG2MAX_INTERLEAVED_NAND_CHIPS = 3 // log2(8) == 3
};

#if NV_DEBUG
#define FTLLITE_ERROR_LOG(X) NvOsDebugPrintf X
#else
#define FTLLITE_ERROR_LOG(X)
#endif

//Macros for error checkings
#define CHECK_PARAM(x) \
        if (!(x)) \
        { \
           return NvError_BadParameter; \
        }
#define CHECK_VALUE(x) \
        if (!(x)) \
        { \
           return NvError_BadValue; \
        }
#define CHECK_MEM(x) \
        if (!(x)) \
        { \
           e = NvError_InsufficientMemory; \
           goto fail; \
        }

///enable this macro to check whether we are writing into a free page or not.
#define VALIDATE_WRITE_OPERATION 0

/// This macro if 0 disables tag data verification in ftl lite
#define ENABLE_TAG_VERIFICATION 0

// Constant indicating the end marker for spare blocks
#define SENTINEL_SPARE 0xFFFFFFFF


typedef struct NandFtlLiteRec
{
    NvDdkNandDeviceInfo NandDevInfo;
    NvU32 RegionValid;
    NvDdkNandHandle hNvDdkNand;
    NvU32 InterleaveBankCount;
    NvU32 NumberOfLogicalBlocks;
    NvU32 StartLogicalBlock;
    NvU32 TotalSectors;
    NvU32 StartPhysicalBlock;
    NvU32 TotalPhysicalBlocks;


    NvU32 PagesPerBlockLog2;
    NvU32 IlCountLog2;
    NvU32 WritePointer;
    NvU32 ReadPointer;
    NvU32 WritePbaIndex;
    NvU32 WriteDeviceNum;
    NvU32 WritePageOffset;
    NvU32 ReadPbaIndex;
    NvU32 ReadDeviceNum;
    NvU32 ReadPageOffset;

    NvU32 NumberOfSpareBlocks[MAX_NAND_SUPPORTED];
    NvU32 *PhysicalBlocks[MAX_NAND_SUPPORTED];
    NvU32 *SpareBlocks[MAX_NAND_SUPPORTED];
    //  Pointer to Tag Buffer
    NvU8 * pTagBuffer;
    NvDdkNandFunctionsPtrs DdkFunctions;
    // This flag is used to enforce extra check on good block count
    // versus actual physical blocks found
    NvBool IsUnbounded;
    // Sequenced read requirement - for true
    NvBool IsSequencedReadNeeded;
    // TRUE means FTL table partially created.
    // FALSE means FTL table fully created
    NvBool IsPartiallyCreated;
    // Read verify enable
    NvBool IsReadVerifyWriteEnabled;
    // buffer to hold the spare area
    NvU8* pSpareBuf;
}NvNandFtlPriv;

enum {
    // run-time good FTL byte pattern
    NAND_RUN_TIME_GOOD = 0xFF,
};

// Macro to get expression for multiply by number which is power of 2
// Expression: VAL * (1 << Log2Num)
#define MACRO_MULT_POW2_LOG2NUM(VAL, Log2Num) \
    ((VAL) << (Log2Num))

// static data
// illegal block address used to initialize table of block address
static const NvU32 s_NandIllegalAddress = 0xFFFFFFFF;

//private functions forward declarations

//Erases all good blocks of a region including bad block replacement blocks
static void FtlLitePrivEraseAllBlocks(FtlLitePrivHandle  hPrivFtl);

//creates mapping table for logcal blocks to physical blocks for a region
static NvError
FtlLitePrivCreatePba2LbaMapping(
    FtlLitePrivHandle hPrivFtl,
    NvBool CreatePartialTable);

//Erases all good blocks of a region excluding bad block replacement blocks, fom the given LBA
static NvBool
FtlLitePrivEraseLogicalBlocks(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 StarBlock,
    NvU32 EndBlock);
//removes  used spare blocks and repopulates spare blocks array
static NvBool
FtlLitePrivRearrangeSpareBlocks(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum);

//replaces bad block from physical block array with a good from spare bock list
static NvError
FtlLitePrivReplaceBlock(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 PbaIndex,
    NvU32 StartPgCurrOp);
// replaces block after write failure
static NvError
FtlLitePrivWrReplaceBlock(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 PbaIndex,
    NvU32 StartPgCurrOp);
/*Copies page by page data from one block to another block, provided data in the page is other
  *  than default
  */
static NvError
FtlLitePrivCopybackData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 SrcBlock,
    NvU32 DestBlock,
    NvU32 StartPgCurrOp);
//writes data into blocks in Physical block list as per the request
static NvError
FtlLitePrivWriteData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32* Sectors2Write,
    NvU8* DataBuffer);
//reads data from the blocks in Physical block list as per the request
static NvError
FtlLitePrivReadData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32* Sectors2Read,
    NvU8* DataBuffer);

static NvError
FtlLiteGetBlockInfo(FtlLitePrivHandle hFtl,
                    NvU32 ChipNumber,
                    NvU32 PBA,
                    NvDdkBlockDevIoctl_IsGoodBlockOutputArgs* pSpareInfo);

static NvU32
FtlUtilGetWord(NvU8 *pByte)
{
    NvU8 a, b, c, d;
    a = *(pByte + 0);
    b = *(pByte + 1);
    c = *(pByte + 2);
    d = *(pByte + 3);
    return ((NvU32)((d << 24) | (c << 16) | (b << 8) | (a)));
}

// local function to mark blocks as bad in FTL Lite
static NvError
FtlLiteMarkBlockAsBad(FtlLitePrivHandle hPrivFtl,
    NvU32 ChipNum, NvU32 BlockNum)
{

    NvError e = NvError_Success;
    NvU32 PageNum[MAX_NAND_SUPPORTED];
    NvU32 SpareAreaSize;
    NvU32 NumBlocks;

    SpareAreaSize = hPrivFtl->NandDevInfo.NumSpareAreaBytes;

    NV_ASSERT(hPrivFtl->pSpareBuf);
    NV_ASSERT(ChipNum < MAX_NAND_SUPPORTED);
    NvOsDebugPrintf("\r\nftllite mark bad: chip=%d blk=%d ",
        ChipNum, BlockNum);
    NvOsMemset(PageNum, 0xFF, (sizeof(PageNum)));
    PageNum[ChipNum] =
        BlockNum *hPrivFtl->NandDevInfo.PagesPerBlock;
    // Erase before mark bad
    NumBlocks = 1;
    e = hPrivFtl->DdkFunctions.NvDdkDeviceErase(hPrivFtl->hNvDdkNand,
                                                    ChipNum,
                                                    PageNum,
                                                    &NumBlocks);
    if (e != NvError_Success)
    {
        NvOsDebugPrintf("\r\nftllite mark bad erase fail error=0x%x : chip=%d blk=%d ",
            e, ChipNum, BlockNum);
    }
    NvOsMemset(hPrivFtl->pSpareBuf, 0x0, SpareAreaSize);
    //mark the block as runtime bad
    NV_CHECK_ERROR_CLEANUP(hPrivFtl->DdkFunctions.NvDdkWriteSpare(
                    hPrivFtl->hNvDdkNand,
                    ChipNum, PageNum,
                    hPrivFtl->pSpareBuf, 1,
                    (SpareAreaSize-1)));
fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nFtl Lite bad block mark failed at "
            "Chip=%d, Block=%d ",ChipNum, BlockNum);
    }
    return e;
}

// Function to update the tag area of blocks skipped during FTL Lite write
// Blocks written starting at page with non-zero offset also updated
// Page number arguments are logical page numbers relative to partition
// FromPage: start page that that is skipped
// ToPage: end page that is skipped.
static NvError
FtlLitePrivTagUpdate(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 FromPage,
    NvU32 ToPage)
{
    NvError e = NvError_Success;
    NvU32 CurrPage;
    NvU32 Sectors2Write;
    NvU32 PageOffset;
    CurrPage = FromPage;
    if ((FromPage == ToPage) && (!FromPage))
        goto fail;
    while (CurrPage <= ToPage)
    {
        // Get page offset within block for partition relative page
        PageOffset = ((CurrPage >> hPrivFtl->IlCountLog2) &
            (hPrivFtl->NandDevInfo.PagesPerBlock - 1));
        // Check one logical page at time.
        // Only pages that are page0 of blocks updated with tag information
        if (!PageOffset)
        {
            // update fields needed by API FtlLitePrivWriteData
            // Update Page0 tag area only
            hPrivFtl->WritePageOffset = 0;
            // Get Super Block number
            hPrivFtl->WritePbaIndex = (CurrPage >> (hPrivFtl->IlCountLog2 +
                hPrivFtl->PagesPerBlockLog2));
            // get interleave bank number
            hPrivFtl->WriteDeviceNum = CurrPage &
                (hPrivFtl->InterleaveBankCount - 1);
            // Update one sector i.e. Page0 tag area
            Sectors2Write = 1;
            // write tag area of block only, no data written
            NV_CHECK_ERROR_CLEANUP(FtlLitePrivWriteData(hPrivFtl,
                &Sectors2Write, NULL));
            // after tag update of last bank in super block try to
            // jump forward to page0 of next block
            if (hPrivFtl->WriteDeviceNum ==
                (hPrivFtl->InterleaveBankCount - 1))
            {
                // Skip the remaining pages of the superblock
                NvU32 SkipPageCount = ((NvU32)((NvU8)1 <<
                    (NvU8)(hPrivFtl->PagesPerBlockLog2 + hPrivFtl->IlCountLog2)
                    ) - (hPrivFtl->InterleaveBankCount - 1));
                CurrPage += SkipPageCount;
                // Check if any more pages remain to be processed
                continue;
            }
        }
        // Check next page
        CurrPage++;
    }
fail:
    return e;
}


/*
  * This API will do the following
  * Creates handle for the requested FTL-lite region.
  * Creates physical block to logical block mapping.
  * Creates spare block array for bad block management
*/
NvError
NvNandFtlLiteOpen(
    FtlLitePrivHandle*  phNandFtlLite,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion)
{
    NvError e = NvError_Success;
    FtlLitePrivHandle  hPrivFtl = NULL;
    NvU32 DevNum;
    NvU32 Size = 0;

    *phNandFtlLite = NULL;

    CHECK_PARAM(hNvDdkNand && nandregion);
    CHECK_PARAM(nandregion->TotalPhysicalBlocks >= nandregion->TotalLogicalBlocks);
    CHECK_PARAM(nandregion->InterleaveBankCount > 0);

    // allocate memory for the ftl lite handle
    hPrivFtl = NvOsAlloc(sizeof(NvNandFtlPriv));
    CHECK_MEM(hPrivFtl);// return NvError_InsufficientMemory;
    //get the device info for nand.
    NV_CHECK_ERROR_CLEANUP(pFuncPtrs->NvDdkGetDeviceInfo(hNvDdkNand,
                0,&hPrivFtl->NandDevInfo));


    Size = (hPrivFtl->NandDevInfo.TagSize > sizeof(TagInformation)) ?
        hPrivFtl->NandDevInfo.TagSize : sizeof(TagInformation);
    hPrivFtl->pTagBuffer = NULL;
    hPrivFtl->pTagBuffer = NvOsAlloc(Size);
    CHECK_MEM(hPrivFtl->pTagBuffer);

    // initialize handle variables
    hPrivFtl->RegionValid = 1;
    hPrivFtl->hNvDdkNand = hNvDdkNand;
    hPrivFtl->InterleaveBankCount = nandregion->InterleaveBankCount;
    hPrivFtl->NumberOfLogicalBlocks = nandregion->TotalLogicalBlocks;
    hPrivFtl->StartLogicalBlock = nandregion->StartLogicalBlock;
    // FIXME: 1 indicates FTL lite management policy
    hPrivFtl->IsUnbounded = nandregion->IsUnbounded;
    hPrivFtl->IsSequencedReadNeeded = nandregion->IsSequencedReadNeeded;
    hPrivFtl->TotalSectors =
        nandregion->TotalLogicalBlocks *
        hPrivFtl->NandDevInfo.PagesPerBlock *
        nandregion->InterleaveBankCount;
    hPrivFtl->StartPhysicalBlock = nandregion->StartPhysicalBlock;
    hPrivFtl->TotalPhysicalBlocks = nandregion->TotalPhysicalBlocks;
    hPrivFtl->PagesPerBlockLog2 = NandUtilGetLog2(hPrivFtl->NandDevInfo.PagesPerBlock);
    hPrivFtl->IlCountLog2 = NandUtilGetLog2(nandregion->InterleaveBankCount);
    hPrivFtl->WritePointer = 0;
    hPrivFtl->ReadPointer = 0;
    hPrivFtl->WritePbaIndex = 0;
    hPrivFtl->WriteDeviceNum = 0;
    hPrivFtl->WritePageOffset = 0;
    hPrivFtl->ReadPbaIndex = 0;
    hPrivFtl->ReadDeviceNum = 0;
    hPrivFtl->ReadPageOffset = 0;
    hPrivFtl->DdkFunctions.NvDdkDeviceErase = pFuncPtrs->NvDdkDeviceErase;
    hPrivFtl->DdkFunctions.NvDdkDeviceRead = pFuncPtrs->NvDdkDeviceRead;
    hPrivFtl->DdkFunctions.NvDdkDeviceWrite = pFuncPtrs->NvDdkDeviceWrite;
    hPrivFtl->DdkFunctions.NvDdkGetDeviceInfo = pFuncPtrs->NvDdkGetDeviceInfo;
    hPrivFtl->DdkFunctions.NvDdkGetLockedRegions = pFuncPtrs->NvDdkGetLockedRegions;
    hPrivFtl->DdkFunctions.NvDdkGetBlockInfo = pFuncPtrs->NvDdkGetBlockInfo;
    hPrivFtl->DdkFunctions.NvDdkReadSpare = pFuncPtrs->NvDdkReadSpare;
    hPrivFtl->DdkFunctions.NvDdkWriteSpare = pFuncPtrs->NvDdkWriteSpare;

    hPrivFtl->IsPartiallyCreated = NV_TRUE;
    NvOsMemset(hPrivFtl->NumberOfSpareBlocks,
        0x0, (sizeof(NvU32) *
        MAX_NAND_SUPPORTED));

    NV_ASSERT(hPrivFtl->NandDevInfo.NumSpareAreaBytes);
    hPrivFtl->pSpareBuf = NvOsAlloc(hPrivFtl->NandDevInfo.NumSpareAreaBytes);
    if(!hPrivFtl->pSpareBuf)
    {
        e = NvError_InsufficientMemory;
        goto fail;
    }
    for (DevNum = 0;DevNum < nandregion->InterleaveBankCount;DevNum++)
    {
        hPrivFtl->PhysicalBlocks[DevNum] = 0;
        hPrivFtl->SpareBlocks[DevNum] = 0;
    }

    //allocate memory for storing physical blocks and spare blocks
    for (DevNum = 0;DevNum < nandregion->InterleaveBankCount;DevNum++)
    {
        hPrivFtl->PhysicalBlocks[DevNum] =
            NvOsAlloc(sizeof(NvU32) * nandregion->TotalLogicalBlocks);
        CHECK_MEM(hPrivFtl->PhysicalBlocks[DevNum]);
        if(nandregion->TotalPhysicalBlocks > nandregion->TotalLogicalBlocks)
        {
            hPrivFtl->SpareBlocks[DevNum] =
                NvOsAlloc(sizeof(NvU32) *
                (nandregion->TotalPhysicalBlocks - nandregion->TotalLogicalBlocks));
            CHECK_MEM(hPrivFtl->SpareBlocks[DevNum]);
        }
    }

    //map LBA's to PBA's
    NV_CHECK_ERROR_CLEANUP(FtlLitePrivCreatePba2LbaMapping(hPrivFtl, NV_TRUE));

    //we will come here only if mapping is done successfully, return back the handle pointer
    *phNandFtlLite = hPrivFtl;
    return e;

fail:
    NvOsDebugPrintf("\r\n Error in FtlLitePrivCreatePba2LbaMapping: e=0x%x ", e);
    if (hPrivFtl)
    {
        if (hPrivFtl->pTagBuffer)
        {
            NvOsFree(hPrivFtl->pTagBuffer);
            hPrivFtl->pTagBuffer = NULL;
        }
        for (DevNum = 0;DevNum < nandregion->InterleaveBankCount;DevNum++)
        {
            if (hPrivFtl->PhysicalBlocks[DevNum])
                NvOsFree(hPrivFtl->PhysicalBlocks[DevNum]);
            if (hPrivFtl->SpareBlocks[DevNum])
                NvOsFree(hPrivFtl->SpareBlocks[DevNum]);
        }

        NvOsFree(hPrivFtl->pSpareBuf);
        NvOsFree(hPrivFtl);
    }

    return e;
}

/*
  * This API will allow the client to read data in sequential as well as
  * interleave mode. Once write operation is issued, read pointer should
  * never cross write pointer. In interleave mode read should always
  * start from offset zero and then all subsequent read requests should
  * be consecutive.
  */
NvError
NvNandFtlLiteReadSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvError_Success;
    NvNandFtlLiteHandle TempFtlLiteHandle = (NvNandFtlLiteHandle)hRegion;
    FtlLitePrivHandle  hPrivFtl = TempFtlLiteHandle->hPrivFtl;
    NvU8* DataBuffer = (NvU8*) pBuffer;
    CHECK_PARAM(hPrivFtl->RegionValid);
    CHECK_PARAM((SectorNumber + NumberOfSectors - 1) < hPrivFtl->TotalSectors);
    if ((hPrivFtl->WritePointer) && (SectorNumber > hPrivFtl->WritePointer))
    {
        e = NvError_NandReadFailed;
        goto fail;
    }
    hPrivFtl->ReadPointer = SectorNumber + NumberOfSectors;

    while (NumberOfSectors)
    {
        NvU32 Sectors2Read = 0;
        // update fields needed by API FtlLitePrivReadData
        hPrivFtl->ReadPageOffset = (SectorNumber /
            hPrivFtl->InterleaveBankCount) &
            (NvU32)((1 << hPrivFtl->PagesPerBlockLog2) - 1);
        // convert into blocks from sectors
        // also consider the interleave count
        hPrivFtl->ReadPbaIndex = (SectorNumber /
            hPrivFtl->InterleaveBankCount) >>
            hPrivFtl->PagesPerBlockLog2;
        // device number calculated and limited to number of devices
        hPrivFtl->ReadDeviceNum = (SectorNumber %
            hPrivFtl->InterleaveBankCount) &
            (NvU32)(hPrivFtl->NandDevInfo.NumberOfDevices - 1);
        if (hPrivFtl->ReadPbaIndex >= hPrivFtl->NumberOfLogicalBlocks)
        {
            e= NvError_NandProgramFailed;
            goto fail;
        }
        if (hPrivFtl->InterleaveBankCount  == 1)
        {
            if ((hPrivFtl->ReadPageOffset + NumberOfSectors) >
                hPrivFtl->NandDevInfo.PagesPerBlock)
            {
                Sectors2Read = hPrivFtl->NandDevInfo.PagesPerBlock -
                    hPrivFtl->ReadPageOffset;
            }
            else
            {
                Sectors2Read = NumberOfSectors;
            }
            NV_CHECK_ERROR_CLEANUP(FtlLitePrivReadData(hPrivFtl,
                &Sectors2Read, DataBuffer));
        }
        else
        {
            //only single sector reads in interleave mode
            Sectors2Read = 1;
            NV_CHECK_ERROR_CLEANUP(FtlLitePrivReadData(hPrivFtl,
                &Sectors2Read, DataBuffer));
        }

        NumberOfSectors -= Sectors2Read;
        SectorNumber += Sectors2Read;
        DataBuffer += (Sectors2Read * hPrivFtl->NandDevInfo.PageSize);
    }
fail:
    return e;
}

/*
  * This API will allow the client to write data in sequential as well as interleave mode.
  * On the first write operation, all the physical blocks for this region will be erased. In interleave
  * mode write should always start from offset zero and then all subsequent write requests
  * should be consecutive.
*/
NvError
NvNandFtlLiteWriteSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvError_Success;
    NvNandFtlLiteHandle TempFtlLiteHandle = (NvNandFtlLiteHandle)hRegion;
    FtlLitePrivHandle  hPrivFtl = TempFtlLiteHandle->hPrivFtl;
    NvU32 PrevWritePtr = 0;
    NvU8* DataBuffer = (NvU8*) pBuffer;
    CHECK_PARAM(hPrivFtl->RegionValid);


    if ((hPrivFtl->WritePointer) && (SectorNumber < hPrivFtl->WritePointer))
    {
        e = NvError_NandProgramFailed;
        goto fail;
    }

    if ((hPrivFtl->WritePointer == 0) && !FtlLitePrivEraseLogicalBlocks(
        hPrivFtl, 0, (NvU32)(-1)))
    {
        return NvError_ResourceError;
    }
    PrevWritePtr = hPrivFtl->WritePointer;
    hPrivFtl->WritePointer = SectorNumber + NumberOfSectors;

    CHECK_PARAM((hPrivFtl->WritePointer - 1) < hPrivFtl->TotalSectors);

    // function Skip Block updates tag area of blocks skipped during
    // FTL Lite write with seek
    NV_CHECK_ERROR_CLEANUP(FtlLitePrivTagUpdate(hPrivFtl,
        PrevWritePtr,
        ((SectorNumber > 0)? (SectorNumber - 1) : SectorNumber)));

    while (NumberOfSectors)
    {
        NvU32 Sectors2Write = 0;
        // update fields needed by API FtlLitePrivWriteData
        hPrivFtl->WritePageOffset = (SectorNumber /
            hPrivFtl->InterleaveBankCount) &
            (NvU32)((1 << hPrivFtl->PagesPerBlockLog2) - 1);
        // convert into blocks from sectors
        // also consider the interleave count
        hPrivFtl->WritePbaIndex = (SectorNumber /
            hPrivFtl->InterleaveBankCount) >>
            hPrivFtl->PagesPerBlockLog2;
        // device number calculated and limited to number of devices
        hPrivFtl->WriteDeviceNum = (SectorNumber %
            hPrivFtl->InterleaveBankCount) &
            (NvU32)(hPrivFtl->NandDevInfo.NumberOfDevices -1);
        if (hPrivFtl->WritePbaIndex >= hPrivFtl->NumberOfLogicalBlocks)
        {
            e= NvError_NandProgramFailed;
            goto fail;
        }
        if (hPrivFtl->InterleaveBankCount  == 1)
        {
            if ((hPrivFtl->WritePageOffset + NumberOfSectors) >
                hPrivFtl->NandDevInfo.PagesPerBlock)
            {
                // Sectors to write to limited to sectors in a block
                Sectors2Write = hPrivFtl->NandDevInfo.PagesPerBlock -
                    hPrivFtl->WritePageOffset;
            }
            else if ((hPrivFtl->WritePageOffset == 0) && (NumberOfSectors > 1))
            {
                Sectors2Write = 1;
            }
            else
            {
                Sectors2Write = NumberOfSectors;
            }
            // call API to write data and tag area of block
            NV_CHECK_ERROR_CLEANUP(FtlLitePrivWriteData(hPrivFtl,
                &Sectors2Write,DataBuffer));
        }
        else
        {
            //only single sector writes in interleave mode
            Sectors2Write = 1;
            NV_CHECK_ERROR_CLEANUP(FtlLitePrivWriteData(hPrivFtl,&Sectors2Write,DataBuffer));
        }

        NumberOfSectors -= Sectors2Write;
        SectorNumber += Sectors2Write;
        DataBuffer += (Sectors2Write * hPrivFtl->NandDevInfo.PageSize);
    }
fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nError in FTL Lite write ");
    }
    return e;
}


void NvNandFtlLiteClose(NvNandRegionHandle hRegion)
{
    NvNandFtlLiteHandle TempFtlLiteHandle = (NvNandFtlLiteHandle)hRegion;
    FtlLitePrivHandle  hPrivFtl = TempFtlLiteHandle->hPrivFtl;
    NvU32 ChipNum = 0;
    // Free other allocated buffers
    for (ChipNum = 0;ChipNum < hPrivFtl->InterleaveBankCount;ChipNum++)
    {
        if (hPrivFtl->PhysicalBlocks[ChipNum])
            NvOsFree(hPrivFtl->PhysicalBlocks[ChipNum]);
        if (hPrivFtl->SpareBlocks[ChipNum])
            NvOsFree(hPrivFtl->SpareBlocks[ChipNum]);
    }
    if (hPrivFtl->pTagBuffer)
    {
        NvOsFree(hPrivFtl->pTagBuffer);
        hPrivFtl->pTagBuffer = NULL;
    }

    NvOsFree(hPrivFtl->pSpareBuf);
    NvOsFree(hPrivFtl);
    NvOsFree(hRegion);
}

void NvNandFtlLiteGetInfo(
    NvNandRegionHandle hRegion,
    NvNandRegionInfo* pNandRegionInfo)
{
    NvNandFtlLiteHandle TempFtlLiteHandle = (NvNandFtlLiteHandle)hRegion;
    FtlLitePrivHandle  hPrivFtl = TempFtlLiteHandle->hPrivFtl;

    pNandRegionInfo->BytesPerSector = hPrivFtl->NandDevInfo.PageSize;
    pNandRegionInfo->SectorsPerBlock = hPrivFtl->NandDevInfo.PagesPerBlock *
        hPrivFtl->InterleaveBankCount;
    pNandRegionInfo->TotalBlocks = hPrivFtl->NumberOfLogicalBlocks;
}

NvError
NvNandFtlLiteIoctl(
    NvNandRegionHandle hRegion,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvNandFtlLiteHandle TempFtlLiteHandle = (NvNandFtlLiteHandle)hRegion;
    FtlLitePrivHandle  hPrivFtl = TempFtlLiteHandle->hPrivFtl;

    CHECK_PARAM(hPrivFtl->RegionValid);

    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            break;
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            {
                NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs* pEraseLbaInfo =
                    (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs*)InputArgs;
                NvU32 StartBlock = 0;
                NvU32 EndtBlock = 0;
                if(!pEraseLbaInfo)
                    return NvError_BadParameter;
                StartBlock = (pEraseLbaInfo->StartLogicalSector) >>
                    hPrivFtl->PagesPerBlockLog2;

                if(pEraseLbaInfo->NumberOfLogicalSectors != 0xFFFFFFFF)
                    EndtBlock = StartBlock +
                        (pEraseLbaInfo->NumberOfLogicalSectors >>hPrivFtl->PagesPerBlockLog2);

                if(!FtlLitePrivEraseLogicalBlocks(hPrivFtl,StartBlock,EndtBlock))
                    return NvError_NandEraseFailed;
            }
            break;
        case NvDdkBlockDevIoctlType_ForceBlkRemap:
            FtlLitePrivEraseAllBlocks(hPrivFtl);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            {
                NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *
                    pWriteVerifyModeIn =
                    (NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;
                NV_ASSERT(InputArgs);
                hPrivFtl->IsReadVerifyWriteEnabled =
                    pWriteVerifyModeIn->IsReadVerifyWriteEnabled;
            }
            break;
        case NvDdkBlockDevIoctlType_IsGoodBlock:
            {
                NvDdkBlockDevIoctl_IsGoodBlockInputArgs *pBlockInfoInputArgs =
                            (NvDdkBlockDevIoctl_IsGoodBlockInputArgs*) InputArgs;
                NvDdkBlockDevIoctl_IsGoodBlockOutputArgs *pBlockInfoOutputArgs =
                            (NvDdkBlockDevIoctl_IsGoodBlockOutputArgs*) OutputArgs;

                NV_ASSERT(hPrivFtl);
                return (FtlLiteGetBlockInfo(hPrivFtl,
                                pBlockInfoInputArgs->ChipNumber,
                                pBlockInfoInputArgs->PhysicalBlockAddress,
                                pBlockInfoOutputArgs));
            }
        case NvDdkBlockDevIoctlType_ErasePartition:
            {
                NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs *EraseArgs
                    = (NvDdkBlockDevIoctl_EraseLogicalSectorsInputArgs*)
                    InputArgs;
                NvU32 StartBlock, NumBlock;
                NvBool RetFlag;

                StartBlock = (EraseArgs->StartLogicalSector >> hPrivFtl->PagesPerBlockLog2);
                if (hPrivFtl->StartLogicalBlock != StartBlock) {
                    NvOsDebugPrintf("\r\n Erase partition error: start arg=%d, start log blk=%d ",
                        (EraseArgs->StartLogicalSector >> hPrivFtl->PagesPerBlockLog2),
                        hPrivFtl->StartLogicalBlock);
                    return NvError_NandEraseFailed;
                }
                NumBlock = (EraseArgs->NumberOfLogicalSectors >> hPrivFtl->PagesPerBlockLog2);
                if (hPrivFtl->NumberOfLogicalBlocks != NumBlock) {
                    NvOsDebugPrintf("\r\n Erase partition error: count arg=%d, erase size=%d ",
                        (EraseArgs->NumberOfLogicalSectors >> hPrivFtl->PagesPerBlockLog2),
                        hPrivFtl->NumberOfLogicalBlocks);
                    return NvError_NandEraseFailed;
                }
                RetFlag = FtlLitePrivEraseLogicalBlocks(hPrivFtl, 0, NumBlock);
                if (!RetFlag) {
                    NvOsDebugPrintf("\r\n Ftllite erase logical failed: blk start=%d,end=%d ",
                        StartBlock, NumBlock);
                    return NvError_NandEraseFailed;
                }

                break;
            }
        default:
            return NvError_NotSupported;
    }
    return NvError_Success;
}

void NvNandFtlLiteFlush(NvNandRegionHandle hRegion)
{
    //nothing to flush in ftl lite
    if (hRegion) {};
}

static NvError
FtlLiteGetBlockInfo(FtlLitePrivHandle hFtl,
                    NvU32 ChipNumber,
                    NvU32 PBA,
                    NvDdkBlockDevIoctl_IsGoodBlockOutputArgs* pSpareInfo)
{
    NvError e = NvSuccess;
    NandBlockInfo NandBlockInfo;

    NV_ASSERT(hFtl->pSpareBuf);
    NV_ASSERT(hFtl->NandDevInfo.NumSpareAreaBytes);
    NV_ASSERT(pSpareInfo);

    NandBlockInfo.pTagBuffer = hFtl->pSpareBuf;
    // Initialize the spare buffer before calling GetBlockInfo API
    NvOsMemset(NandBlockInfo.pTagBuffer, 0,
        hFtl->NandDevInfo.NumSpareAreaBytes);
    // Need skipped bytes along with tag information
    NV_CHECK_ERROR(NvDdkNandGetBlockInfo(hFtl->hNvDdkNand, ChipNumber,
        PBA, &NandBlockInfo, NV_TRUE));
    pSpareInfo->IsFactoryGoodBlock = NV_TRUE;
    pSpareInfo->IsRuntimeGoodBlock = NV_TRUE;

    // check factory bad block info
    if (NandBlockInfo.IsFactoryGoodBlock == NV_FALSE)
    {
        NvOsDebugPrintf("\nFactory Bad block: Chip%u Block=%u ",
                                                ChipNumber, PBA);
        pSpareInfo->IsFactoryGoodBlock = NV_FALSE;
    }
    // run-time good check
    if (hFtl->pSpareBuf[RUNTIME_BAD_BYTE_OFFSET] != 0xFF)
    {
        NvOsDebugPrintf("\nRuntime Bad block: Chip%u Block=%u,"
            "RTB=0x%x ", ChipNumber, PBA, hFtl->pSpareBuf[RUNTIME_BAD_BYTE_OFFSET]);
        pSpareInfo->IsRuntimeGoodBlock = NV_FALSE;
    }
    pSpareInfo->IsGoodBlock = pSpareInfo->IsFactoryGoodBlock &&
                                    pSpareInfo->IsRuntimeGoodBlock;
    return e;

}
void FtlLitePrivEraseAllBlocks(FtlLitePrivHandle  hPrivFtl)
{
    NvU32 BlockNum = 0;
    NvU32 DevCount = 0;
    NvU32 PageNumber[MAX_NAND_SUPPORTED];
    NvError e = NvSuccess;
    NandBlockInfo BlockInfo;
    BlockInfo.pTagBuffer = (NvU8*)(hPrivFtl->pSpareBuf);
    NvOsMemset(PageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    for (DevCount = 0;DevCount < hPrivFtl->InterleaveBankCount; DevCount++)
    {
        for (BlockNum = 0;BlockNum < hPrivFtl->TotalPhysicalBlocks; BlockNum++)
        {
            NvU32 DeviceNum = DevCount;
            NvU32 PhysicalBlockNumber = hPrivFtl->StartPhysicalBlock + BlockNum;
            NvBtlGetPba(&DeviceNum,&PhysicalBlockNumber);
            NvOsMemset(BlockInfo.pTagBuffer, 0x0,
                hPrivFtl->NandDevInfo.NumSpareAreaBytes);
            //get block info
            e = hPrivFtl->DdkFunctions.NvDdkGetBlockInfo(
                                                    hPrivFtl->hNvDdkNand,
                                                    DeviceNum,
                                                    PhysicalBlockNumber,
                                                    &BlockInfo, NV_FALSE);
            if (e != NvSuccess) {
                NvOsDebugPrintf("\r\n EraseAllBlocks: GetBlockInfo error=0x%x @ chip=%d,blk=%d ",
                    e, DeviceNum, PhysicalBlockNumber);
                continue;
            }
            //erase the block if its good
            if((!BlockInfo.IsFactoryGoodBlock) || // factory bad
                (hPrivFtl->pSpareBuf[RUNTIME_BAD_BYTE_OFFSET] != NAND_RUN_TIME_GOOD)) // run-time bad
            {
                if (!BlockInfo.IsFactoryGoodBlock)
                    NvOsDebugPrintf("\r\n EraseAllBlocks: factory bad block @ chip=%d,blk=%d ",
                DeviceNum, PhysicalBlockNumber);
                if (hPrivFtl->pSpareBuf[RUNTIME_BAD_BYTE_OFFSET] != NAND_RUN_TIME_GOOD)
                    NvOsDebugPrintf("\r\n EraseAllBlocks: runtime bad block @ chip=%d,blk=%d ",
                DeviceNum, PhysicalBlockNumber);
                continue;
            }
            else
            {
                NvU32 NumberOfBlocks = 1;
                PageNumber[DeviceNum] = PhysicalBlockNumber *
                    hPrivFtl->NandDevInfo.PagesPerBlock;

                if (hPrivFtl->DdkFunctions.NvDdkDeviceErase(
                                    hPrivFtl->hNvDdkNand,
                                    DeviceNum,
                                    PageNumber,
                                    &NumberOfBlocks) != NvError_Success)
                {
                    //mark the block as bad, if erase fails
                    (void)FtlLiteMarkBlockAsBad(hPrivFtl,
                        DeviceNum, PhysicalBlockNumber);
                }
            }
        }
    }
}

NvBool
FtlLitePrivEraseLogicalBlocks(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 StarBlock,
    NvU32 EndBlock)
{
    NvU32 BlockNum;
    NvU32 DevNum;
    NvU32 PageNum[MAX_NAND_SUPPORTED];
    NvU32 NumBlocks =1;
    NvBool EraseFailed = NV_FALSE;
    NvError e = NvError_Success;

    NvOsMemset(PageNum, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));

    if(EndBlock >= 0xFFFFFFFF)
        EndBlock = hPrivFtl->NumberOfLogicalBlocks;

    if((StarBlock > hPrivFtl->NumberOfLogicalBlocks) ||
        (EndBlock> hPrivFtl->NumberOfLogicalBlocks))
        return NV_FALSE;

    for (DevNum = 0;DevNum < hPrivFtl->InterleaveBankCount;DevNum++)
    {
        for (BlockNum = StarBlock;BlockNum < hPrivFtl->NumberOfLogicalBlocks;BlockNum++)
        {
            NvU32 AbsBlockNum =
                hPrivFtl->PhysicalBlocks[DevNum][BlockNum];
            NvU32 ChipNum = DevNum;
            if(AbsBlockNum == 0xFFFFFFFF)
            {
                EraseFailed = NV_TRUE;
                break;
            }
            NvBtlGetPba(&ChipNum,&AbsBlockNum);
            PageNum[ChipNum] =
                AbsBlockNum *hPrivFtl->NandDevInfo.PagesPerBlock;
            if (hPrivFtl->DdkFunctions.NvDdkDeviceErase(hPrivFtl->hNvDdkNand,
                                                                    ChipNum,
                                                                    PageNum,
                                                                    &NumBlocks) != NvError_Success)
            {
                //mark the block as bad, if erase fails
                e = FtlLiteMarkBlockAsBad(hPrivFtl, ChipNum, AbsBlockNum);
                if (e != NvSuccess)
                    EraseFailed = NV_TRUE;
                // do not abort format for failed erase
                continue;
            }
            PageNum[ChipNum] = 0xFFFFFFFF;
        }
        if (EraseFailed)
            break;
    }

    if (EraseFailed)
    {
        FtlLitePrivEraseAllBlocks(hPrivFtl);
        if (FtlLitePrivCreatePba2LbaMapping(hPrivFtl, NV_FALSE) !=
                NvError_Success)
            return NV_FALSE;
    }
    return NV_TRUE;
}

// Function to do read verify of data written earlier by FTL Lite
static NvError
ReadVerifyFtlLite(FtlLitePrivHandle hPrivFtl,
    NvU32 DevNum, NvU32 PhysicalBlockNumber,
    NvU8* DataBuffer,
    NvU8* pTagBuffer,
    NvU32 Sectors2Write)
{
    NvError e = NvSuccess;
    NvU8 *pBuf;
    NvU8* pTBuf;
    NvU32 PageNumber[MAX_NAND_SUPPORTED];
    NvU32 i;
    NvU32 Log2PageSize;
    NvU32 NumSectors;
    NvU32 Log2PagesPerBlock;
    // FIXME: dynamic allocation avoided for limitation in heap mgr in nvbl
    NvU8 s_MyDataBuf[4096]; // maximum Nand page size is 4K now
    NvU8 s_MyTagBuf[256]; // maximum tag size now is 218

    pTBuf = s_MyTagBuf;
    pBuf = s_MyDataBuf;
    NvOsMemset(PageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    Log2PagesPerBlock = NandUtilGetLog2(hPrivFtl->NandDevInfo.PagesPerBlock);
    PageNumber[DevNum] = MACRO_MULT_POW2_LOG2NUM(PhysicalBlockNumber,
        Log2PagesPerBlock) +
        hPrivFtl->WritePageOffset;
    Log2PageSize = NandUtilGetLog2(hPrivFtl->NandDevInfo.PageSize);
    // for each page upto count Sectors2Write compare data
    for (i = 0; i < Sectors2Write; i++)
    {
        NumSectors = 1;
        NV_CHECK_ERROR_CLEANUP(hPrivFtl->DdkFunctions.NvDdkDeviceRead(hPrivFtl->hNvDdkNand,
            DevNum, PageNumber, pBuf, pTBuf,
            &NumSectors, NV_FALSE));
        if (!NumSectors)
        {
            e = NvError_BlockDriverWriteVerifyFailed;
            break;
        }
        {
            NvU32 Offset;
            NvU8 *pSrc;
            Offset = MACRO_MULT_POW2_LOG2NUM(i, Log2PageSize);
            pSrc = ((NvU8 *)DataBuffer + Offset);
            // Check the data against written data
            if (NandUtilMemcmp((const void *)pSrc, (const void *)pBuf,
                hPrivFtl->NandDevInfo.PageSize))
            {
                NvOsDebugPrintf("\nData area read verification failed in FTL "
                    "Lite at Chip=%d,Blk=%d,Pg=%d ", DevNum, PhysicalBlockNumber,
                    (hPrivFtl->WritePageOffset + i));
                e = NvError_BlockDriverWriteVerifyFailed;
                break;
            }
#if ENABLE_TAG_VERIFICATION
            // Sometime tag area comparison showed 0xFE and not 0xFF
            // at byte offset 1. Since tag data is not user data - skipping the
            // tag area verification with the write data
            if ((i == 0) && (pTagBuffer != NULL))
            {
                NvU32 CmpSize;
                TagInformation *pTagInf;
                // FIXME: Assumed that TagInformation structure has member Padding
                // at end of structure
                pTagInf = (TagInformation *)pTBuf;
                CmpSize = ((NvU8 *)&pTagInf->Padding[0] - pTBuf);
                // Verify Tag data also
                if (NvOsMemcmp((const void *)pTagBuffer, (const void *)pTBuf,
                    CmpSize))
                {
                    NvOsDebugPrintf("\nTag area read verification failed in FTL "
                        "Lite at chip=%d,blk=%d ", DevNum, PhysicalBlockNumber);
                    e = NvError_BlockDriverWriteVerifyFailed;
                    break;
                }
            }
#endif
        }
        // move to next page
        PageNumber[DevNum]++;
    }

fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nFTL Lite Read Verify error code=0x%x ", e);
    }
    return e;
}

// This write function should never be called if starting at first
// page of blockmultiple pages are written.
NvError
FtlLitePrivWriteData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32* Sectors2Write,
    NvU8* DataBuffer)
{
    NvError e = NvError_Success;
    NvU32 PhysicalBlockNumber = 0;
    NvU32 LogicalBlockNumber = 0;
    NvU32 PageNumber[MAX_NAND_SUPPORTED];
    NvU32 DevNum = 0;
    TagInformation tagInfo;
    TagInformation *pTaginfo = NULL;
    NvU8 * pTagBuffer = NULL;

    NvError Err2 = NvError_Success;

    while (1)
    {
        PhysicalBlockNumber =
            hPrivFtl->PhysicalBlocks[hPrivFtl->WriteDeviceNum][hPrivFtl->WritePbaIndex];
        LogicalBlockNumber = hPrivFtl->StartLogicalBlock + hPrivFtl->WritePbaIndex;
        if (-1 == (NvS32)PhysicalBlockNumber) {
            NvOsDebugPrintf("\r\n Write called without PBA mapping info: chip=%d,lba=%d ",
                hPrivFtl->WriteDeviceNum, LogicalBlockNumber);
            return NvError_NandWriteFailed;
        }
        DevNum = hPrivFtl->WriteDeviceNum;
        NvBtlGetPba(&DevNum,&PhysicalBlockNumber);

        NvOsMemset(PageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
        PageNumber[DevNum] = (PhysicalBlockNumber *
            hPrivFtl->NandDevInfo.PagesPerBlock) +
            hPrivFtl->WritePageOffset;

        if (hPrivFtl->WritePageOffset == 0)
        {
            NvOsMemset(&tagInfo, 0xFF, sizeof(TagInformation));
            pTaginfo = &tagInfo;
            // AOS gets compilation error due to packing for TagInformation structure
            //tagInfo.LogicalBlkNum = LogicalBlockNumber;
            NvOsMemcpy((void *)&(tagInfo.LogicalBlkNum), (const void *)&LogicalBlockNumber, sizeof(NvU32));
        }
        else
        {
            pTaginfo = 0;
        }
#if VALIDATE_WRITE_OPERATION
        {
            NvU32 TempSectors2Write = *Sectors2Write;
            NvU32 FullPageSize = hPrivFtl->NandDevInfo.PageSize +
                hPrivFtl->NandDevInfo.TagSize;

            NvU8* TempBuffer = NvOsAlloc(TempSectors2Write * FullPageSize);
            NvU32 i;
            NvOsMemset(TempBuffer,0xFF,TempSectors2Write * FullPageSize);
            e = hPrivFtl->DdkFunctions.NvDdkDeviceRead(hPrivFtl->hNvDdkNand,
                    DevNum,
                    PageNumber,
                    TempBuffer,
                    TempBuffer + hPrivFtl->NandDevInfo.PageSize,
                    &TempSectors2Write,
                    NV_FALSE);
            if (e != NvError_Success)
            {
                NvOsDebugPrintf("\r\n Write validation failed for pre write %d",e);
            }
            for(i = 0; i < TempSectors2Write * FullPageSize; i++)
            {
                if(TempBuffer[i] != 0xFF)
                {
                    NvOsDebugPrintf("\r\n Pre- write validatio: data is not clean at%d",i);
                    break;
                }
            }
            NvOsFree(TempBuffer);
        }
#endif
        if (pTaginfo)
        {
            pTagBuffer = (NvU8 *)pTaginfo;
        }
        e = hPrivFtl->DdkFunctions.NvDdkDeviceWrite(hPrivFtl->hNvDdkNand,
            DevNum,
            PageNumber,
            DataBuffer,
            (NvU8*)pTaginfo,
            Sectors2Write);

        if ((DataBuffer) && (e == NvSuccess) && (hPrivFtl->IsReadVerifyWriteEnabled))
        {
            // verification only for data now
            Err2 = ReadVerifyFtlLite(hPrivFtl, DevNum, PhysicalBlockNumber,
                            DataBuffer, pTagBuffer, *Sectors2Write);
        }

        if ((e != NvError_Success) || (Err2 != NvError_Success))
        {
            NvError LastErr = e;
            if (e != NvSuccess)
            {
                NvOsDebugPrintf("\nWr Error: 0x%x, Replace ftl lite bad block, PbaIndex=%d,"
                    "Chip=%d,Block=%d,StartPg=%d,PgCount=%d ", e,
                hPrivFtl->WritePbaIndex,
                hPrivFtl->WriteDeviceNum, PhysicalBlockNumber,
                hPrivFtl->WritePageOffset, *Sectors2Write);
            }
            else
            {
                NvOsDebugPrintf("\nRd verify error: 0x%x, Replace ftl lite "
                    "bad block, PbaIndex=%d,"
                    "Chip=%d,Block=%d,StartPg=%d,PgCount=%d ", Err2,
                    hPrivFtl->WritePbaIndex,
                    hPrivFtl->WriteDeviceNum, PhysicalBlockNumber,
                    hPrivFtl->WritePageOffset, *Sectors2Write);
            }
            e = FtlLitePrivWrReplaceBlock(
                hPrivFtl,
                hPrivFtl->WriteDeviceNum,
                hPrivFtl->WritePbaIndex,
                hPrivFtl->WritePageOffset);
            // Mark original block as bad
            (void)FtlLiteMarkBlockAsBad(hPrivFtl, hPrivFtl->WriteDeviceNum,
                PhysicalBlockNumber);
            // when no spare blocks left return the Ddk write error
            if (e != NvSuccess)
                return LastErr;
        }
        else
        {
            // write success
            break;
        }
    }

    return e;
}

NvError
FtlLitePrivReadData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32* Sectors2Read,
    NvU8* DataBuffer)
{
    NvError e = NvError_Success;
    NvU32 PhysicalBlockNumber = 0;
    NvU32 PageNumber[MAX_NAND_SUPPORTED];
    NvU32 DevNum = 0;
    PhysicalBlockNumber =
        hPrivFtl->PhysicalBlocks[hPrivFtl->ReadDeviceNum][hPrivFtl->ReadPbaIndex];
    DevNum = hPrivFtl->ReadDeviceNum;
    //check if pba is valid
    if(PhysicalBlockNumber == 0xFFFFFFFF)
    {
        // If table is created partially create full table
        if (hPrivFtl->IsPartiallyCreated)
        {
            // Table is partially created. Create full table

            // TODO: Here complete table is created. Instead of this use
            // existing partial table as is and scan the partition for only the
            // required block. FtlLitePrivCreatePba2LbaMapping should be
            //  modified accordingly
            NV_CHECK_ERROR(
                FtlLitePrivCreatePba2LbaMapping(hPrivFtl, NV_FALSE)
            );

            PhysicalBlockNumber =
                hPrivFtl->PhysicalBlocks[hPrivFtl->ReadDeviceNum][hPrivFtl->ReadPbaIndex];
        }

        if(PhysicalBlockNumber == 0xFFFFFFFF)
        {
            //PBA is not assigned to this LBA, return defalult data
            NvOsMemset(
                DataBuffer,0xFF,
                (*Sectors2Read) * hPrivFtl->NandDevInfo.PageSize);
            return NvError_Success;
        }
    }

    NvBtlGetPba(&DevNum,&PhysicalBlockNumber);

    NvOsMemset(PageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    PageNumber[DevNum] =
        (PhysicalBlockNumber * hPrivFtl->NandDevInfo.PagesPerBlock) +
        hPrivFtl->ReadPageOffset;

    e = hPrivFtl->DdkFunctions.NvDdkDeviceRead(
                                                                    hPrivFtl->hNvDdkNand,
                                                                    DevNum,
                                                                    PageNumber,
                                                                    DataBuffer,
                                                                    NULL,
                                                                    Sectors2Read,
                                                                    NV_FALSE);

    if (e != NvError_Success)
    {
        NvError LastErr = e;
        if (!hPrivFtl->IsSequencedReadNeeded) {
        NvOsDebugPrintf("\nReplace block=%d in chip=%d for read failure "
            , PhysicalBlockNumber, hPrivFtl->ReadDeviceNum);
        e = FtlLitePrivReplaceBlock(
            hPrivFtl,
            hPrivFtl->ReadDeviceNum,
            hPrivFtl->ReadPbaIndex,
            hPrivFtl->NandDevInfo.PagesPerBlock);
            // Mark block with read failure as bad
        (void)FtlLiteMarkBlockAsBad(hPrivFtl, hPrivFtl->ReadDeviceNum,
            PhysicalBlockNumber);
        } else {
            NvOsDebugPrintf("\r\n Partition sequential read type: read failure at chip=%d, blk=%d ",
                DevNum, PhysicalBlockNumber);
        }
        // If no replacement blocks found return the Ddk error
        if (e != NvSuccess)
        return LastErr;
    }

    return e;
}

// local function to mark blocks as bad
static NvError
FtlUtilMarkBad(FtlLitePrivHandle  hPrivFtl,
    NvU32 ChipNum, NvU32 BlockNum)
{
    NvError e = NvError_Success;
    NvU32 SpareAreaSize;
    NvU32 dev;
    NvU32 Log2PgsPerBlk;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];

    NV_ASSERT(ChipNum < MAX_NAND_SUPPORTED);

    FTLLITE_ERROR_LOG(("\nMarking Runtime Bad block: Chip%u Block=%u ",
                                            ChipNum, BlockNum));

    for(dev = 0; dev < MAX_INTERLEAVED_NAND_CHIPS;dev++)
        PageNumber[dev] = s_NandIllegalAddress;

    SpareAreaSize = hPrivFtl->NandDevInfo.NumSpareAreaBytes;
    NV_ASSERT(SpareAreaSize);
    NV_ASSERT(hPrivFtl->pSpareBuf);

    Log2PgsPerBlk = NandUtilGetLog2(hPrivFtl->NandDevInfo.PagesPerBlock);
    PageNumber[ChipNum] = (BlockNum << Log2PgsPerBlk);
    NvOsMemset(hPrivFtl->pSpareBuf, 0x0, SpareAreaSize);
    //mark the block as runtime bad
    NV_CHECK_ERROR_CLEANUP(NvDdkNandWriteSpare(hPrivFtl->hNvDdkNand,
        ChipNum, PageNumber,hPrivFtl->pSpareBuf,
        1, (SpareAreaSize -1)));
fail:
    if (e != NvSuccess)
    {
        NvOsDebugPrintf("\nBlock driver mark bad failed at "
            "Chip=%d, Block=%d ",ChipNum, BlockNum);
    }

    return e;
}

// Given chip number and block number on chip, erases the block
// Function assumes block to be erased is good block
static NvError FtlNandUtilEraseBlock(FtlLitePrivHandle  hPrivFtl,
    NvU32 ChipNum, NvU32 BlockAddr)
{
    NvU32 TmpBlkCount;
    NvU32 PageNumber[MAX_INTERLEAVED_NAND_CHIPS];
    NvError e = NvSuccess;
    NvOsMemset(PageNumber, 0xFF,
        MACRO_MULT_POW2_LOG2NUM(sizeof(NvU32),
        LOG2MAX_INTERLEAVED_NAND_CHIPS));
    PageNumber[ChipNum] = MACRO_MULT_POW2_LOG2NUM(BlockAddr,
        hPrivFtl->PagesPerBlockLog2);
    // Erase a block at a time
    TmpBlkCount = 1;
    if ((e = NvDdkNandErase(hPrivFtl->hNvDdkNand,
        ChipNum, PageNumber, &TmpBlkCount)) != NvError_Success)
    {
        //mark the block as runtime bad, if erase fails
        TmpBlkCount = 1;
        FTLLITE_ERROR_LOG(("\r\nMarking Runtime Bad block: Chip%u Block=%u ",
                                                ChipNum, BlockAddr));
        NV_CHECK_ERROR(FtlUtilMarkBad(hPrivFtl, ChipNum, BlockAddr));
        if ((e != NvSuccess) || ((!TmpBlkCount) && (e == NvSuccess)))
        {
            NvOsDebugPrintf("\r\nErase Partition Error: failed to erase "
                "block chip=%d,blk=%d ", ChipNum, BlockAddr);
            e = NvError_NandBlockDriverEraseFailure;
        }
    }
    return e;
}

/*
  * This method generated physical blocks for LBA's with in this region. There are two ways
  * involved in this process.
  * 1) When the region is in completely erased state, mostly first boot. In this cases all good
  * PBA's are assigned to LBA's in sequential order.
  * 2) Device/region is already used. in this case LBA number will be part of taginfo for all
  * physical blocks. remember that partial write to a region are not allowed. Then this API will
  * do the mapping using taginfo
*/
NvError
FtlLitePrivCreatePba2LbaMapping(
    FtlLitePrivHandle  hPrivFtl,
    NvBool CreatePartialTable)
{
    NvBool UseTagInfo;
    NandBlockInfo PbaInfo;
    TagInformation *pTagInfo;
    NvU32 BlockNum;
    NvU32 BlockIndex[MAX_NAND_SUPPORTED];
    NvU32 DevNum;
    NvU32 NumPhysicalBlocksRequired = hPrivFtl->NumberOfLogicalBlocks;
    NvU32 LogicalBlkNum;
    NvU8 * pByte;
    NvError e = NvSuccess;
    NvU8 *Buffer = NULL;
    NvU8 *pPageBuffer = NULL;
    NvU8 *pTagBuffer = NULL;

    PbaInfo.pTagBuffer = (NvU8*)hPrivFtl->pSpareBuf;

    UseTagInfo = NV_FALSE;
    NvOsMemset(BlockIndex, 0x0, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    for (DevNum = 0;DevNum < hPrivFtl->InterleaveBankCount;DevNum++)
    {
        if (hPrivFtl->PhysicalBlocks[DevNum])
        {
            NvOsMemset(hPrivFtl->PhysicalBlocks[DevNum],
                0xFF, (sizeof(NvU32) * hPrivFtl->NumberOfLogicalBlocks));
        }
        if (hPrivFtl->SpareBlocks[DevNum])
        {
            NvOsMemset(hPrivFtl->SpareBlocks[DevNum], 0xFF,
                (sizeof(NvU32) *
                (hPrivFtl->TotalPhysicalBlocks - hPrivFtl->NumberOfLogicalBlocks)));
        }
        BlockIndex[DevNum] = 0x00;
        hPrivFtl->NumberOfSpareBlocks[DevNum] = 0x00;

        for (BlockNum = 0;BlockNum < hPrivFtl->TotalPhysicalBlocks;BlockNum++)
        {
            NvU32 AbsBlockNum = BlockNum + hPrivFtl->StartPhysicalBlock;
            NvU32 ChipNum = DevNum;
            if (NvBtlGetPba(&ChipNum,&AbsBlockNum) != NV_TRUE)
            {
                return NvError_NotInitialized;
            }
LblRedoMap:
            NvOsMemset(PbaInfo.pTagBuffer, 0x0,
                hPrivFtl->NandDevInfo.NumSpareAreaBytes);
            NV_CHECK_ERROR(hPrivFtl->DdkFunctions.NvDdkGetBlockInfo(
                                                                hPrivFtl->hNvDdkNand,
                                                                ChipNum,
                                                                AbsBlockNum,
                                                                &PbaInfo, NV_TRUE));

            pTagInfo = (TagInformation *)((NvU32)PbaInfo.pTagBuffer +
                hPrivFtl->NandDevInfo.TagOffset);
            if ((PbaInfo.IsFactoryGoodBlock == NV_FALSE) || (
                (hPrivFtl->pSpareBuf[RUNTIME_BAD_BYTE_OFFSET] != NAND_RUN_TIME_GOOD))) {
                NvOsDebugPrintf("\r\n Bad block in pba2lba ftlite map: chip=%d, blk=%d ",
                    ChipNum, AbsBlockNum);
                continue;
            }
            // AOS gets compilation error due to packing for TagInformation structure
            pByte = (NvU8 *)&(pTagInfo->LogicalBlkNum);
            LogicalBlkNum = FtlUtilGetWord(pByte);// tagInfo.LogicalBlkNum;
            if ((LogicalBlkNum == 0xFFFFFFFF) && CreatePartialTable)
            {
                hPrivFtl->IsPartiallyCreated = CreatePartialTable;
                // Scanning the current chip is done.
                break;
            }

            if (BlockIndex[DevNum] < NumPhysicalBlocksRequired)
            {
                if (LogicalBlkNum != 0xFFFFFFFF)
                    UseTagInfo = NV_TRUE;

                if (UseTagInfo)
                {
                    NvU32 RelativeLba = LogicalBlkNum - hPrivFtl->StartLogicalBlock;
                    if (LogicalBlkNum < hPrivFtl->StartLogicalBlock) {
                        NvOsDebugPrintf("\r\n Fatal error in pba2lba ftllite: line%d,"
                            "lba=%d, startlba=%d chip=%d blk=%d ", __LINE__,
                            LogicalBlkNum, hPrivFtl->StartLogicalBlock, ChipNum,
                            AbsBlockNum);
                        NvOsDebugPrintf("\r\n sparebuf[0]=0x%x, factory good=%d ",
                            *((NvU32 *)PbaInfo.pTagBuffer), PbaInfo.IsFactoryGoodBlock);
                        // erase the block and fail
                        {
                            NvU32 PageNum[MAX_NAND_SUPPORTED];
                            NvU32 NumberOfBlocks = 1;
                            NvOsDebugPrintf("\r\n Erasing block at chip=%d, blk=%d ",
                                ChipNum, AbsBlockNum);
                            NvOsMemset(PageNum, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
                            PageNum[ChipNum] =
                                AbsBlockNum * hPrivFtl->NandDevInfo.PagesPerBlock;
                            //erase the free block from the spare blocks
                            hPrivFtl->DdkFunctions.NvDdkDeviceErase(
                                                                hPrivFtl->hNvDdkNand,
                                                                ChipNum,
                                                                PageNum,
                                                                &NumberOfBlocks);
                        }
                        NvOsDebugPrintf("\r\n continuing mapping erased blk ");
                        goto LblRedoMap;
                    }
                    if (LogicalBlkNum == 0xFFFFFFFF)
                    {
                        BlockIndex[DevNum]++;
                    }
                    else if(RelativeLba > hPrivFtl->NumberOfLogicalBlocks)
                    {
                        if (!Buffer)
                        {
                            Buffer = (NvU8 *)(NvOsAlloc(((NvU32)(hPrivFtl->NandDevInfo.TagSize
                                + 63) & (NvU32)~63) + ((hPrivFtl->NandDevInfo.PageSize + 63) &
                                (NvU32)~63) + 64));
                        }

                        if (Buffer)
                        {
                            pPageBuffer = Buffer;
                            pTagBuffer = Buffer +
                                ((hPrivFtl->NandDevInfo.PageSize+63) & (NvU32)~63) + 64;
                        }
                        else
                        {
                            return NvError_InsufficientMemory;
                        }
                        // Test bad block using erase, write/read data pattern
                        if (NvDdkNandUtilEraseAndTestBlock(hPrivFtl->hNvDdkNand,
                            &hPrivFtl->NandDevInfo, pPageBuffer, pTagBuffer,
                            ChipNum, AbsBlockNum)==NandBlock_RuntimeBad)
                        {
                            // Found bad block
                            NvU32 PageNumbers[8] = {(NvU32)~0, (NvU32)~0, (NvU32)~0, (NvU32)~0, (NvU32)~0, (NvU32)~0, (NvU32)~0, (NvU32)~0};
                            NvU32 NumPages = 1;
                            NvU8  BadBlockSentinel[4] = { ~NAND_SPARE_SENTINEL_GOOD,
                                                          ~NAND_SPARE_SENTINEL_GOOD,
                                                          ~NAND_SPARE_SENTINEL_GOOD,
                                                          ~NAND_SPARE_SENTINEL_GOOD };

                            PageNumbers[ChipNum] = AbsBlockNum *
                                hPrivFtl->NandDevInfo.PagesPerBlock;
                            /*  FIXME: To be compatible with FTL and non-FTL, both
                             *  the 1st byte of the spare area and the FTL tag area
                             *  are written for run-time bad blocks. FTL should migrate
                             *  over to use the spare area, so that only 1 byte needs
                             *  to be written.  For simplicity, the entire tag area is
                             *  cleared to 0 for run-time bad blocks.  The page data is
                             *  filled with whatever happens to be left-over in the
                             *  page-buffer following the NandUtilEraseAndTestBlock
                             *  call.
                             */
                            NvOsMemset(pTagBuffer, 0,
                                hPrivFtl->NandDevInfo.TagSize);
                            NvDdkNandWriteSpare(hPrivFtl->hNvDdkNand,
                                ChipNum, PageNumbers, BadBlockSentinel,
                                NAND_SPARE_SENTINEL_LOC, NAND_SPARE_SENTINEL_LEN);
                            NvDdkNandWrite(hPrivFtl->hNvDdkNand, ChipNum,
                                PageNumbers, pPageBuffer, pTagBuffer, &NumPages);
                        }
                        else
                        {
                            // if good block, erase the block
                            NV_CHECK_ERROR_CLEANUP(FtlNandUtilEraseBlock(
                                hPrivFtl, ChipNum, AbsBlockNum));
                            // and handle same as case lba = 0xFFFFFFFF
                            BlockIndex[DevNum]++;
                        }
                    }
                    else
                    {
                        hPrivFtl->PhysicalBlocks[DevNum][RelativeLba] =
                            BlockNum + hPrivFtl->StartPhysicalBlock;
                        BlockIndex[DevNum]++;
                    }
                }
                else
                {
                    hPrivFtl->PhysicalBlocks[DevNum][BlockIndex[DevNum]++] =
                        BlockNum + hPrivFtl->StartPhysicalBlock;
                }
            }
            else
            {
                if (hPrivFtl->SpareBlocks[ChipNum])
                {
                    hPrivFtl->SpareBlocks[DevNum][hPrivFtl->NumberOfSpareBlocks[DevNum]++] =
                    BlockNum + hPrivFtl->StartPhysicalBlock;
                }
            }
        }
    }

    //check if we got sufficient physical blocks or not
    if(!CreatePartialTable)
    {
        NvBool UnAssignedPhysicalBlocks = NV_FALSE;
        NvBool NotEnoughPhysicalBlocks = NV_FALSE;
        for (DevNum = 0;DevNum < hPrivFtl->InterleaveBankCount;DevNum++)
        {
            for (BlockNum = 0;BlockNum < hPrivFtl->NumberOfLogicalBlocks;BlockNum++)
                if (hPrivFtl->PhysicalBlocks[DevNum][BlockNum] == 0xFFFFFFFF)
                    UnAssignedPhysicalBlocks = NV_TRUE;
            // Check to see good blocks requested is actually found
            if (hPrivFtl->IsUnbounded == NV_FALSE)
            {
                if (BlockIndex[DevNum] < NumPhysicalBlocksRequired)
                {
                    // Physical and Spare blocks are freed by caller once
                    // error is returned
                    NotEnoughPhysicalBlocks = NV_TRUE;
                }
            }
        }

        if (NotEnoughPhysicalBlocks)
        {
        // Don't return the Error, we will try to map all the good block
        // if read/write is for less than (good-block + spare-area) than we will
        // succeed. Otherwise at that point we will return error.
        //    return NvError_InvalidState;
        }
        if (UnAssignedPhysicalBlocks)
        {
            // FIXME: Is this Success?
            hPrivFtl->IsPartiallyCreated = CreatePartialTable;
            return NvError_Success;
        }
    }
    hPrivFtl->IsPartiallyCreated = CreatePartialTable;
    return NvError_Success;
fail:
    NvOsFree(Buffer);
    return e;
}

NvError
FtlLitePrivWrReplaceBlock(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 PbaIndex,
    NvU32 StartPgCurrOp)
{
    NvU32 BlockToReplace, OldPba;
    NvU32 LogicalBlockNumber;
    NvDdkBlockDevIoctl_IsGoodBlockOutputArgs BlockInfoOutputArgs;
    NvU32 i, j;
    NvError e;
    NvBool RetFlag;
    BlockToReplace = hPrivFtl->PhysicalBlocks[DevNum][PbaIndex];
    OldPba = BlockToReplace++;
    LogicalBlockNumber = hPrivFtl->StartLogicalBlock + hPrivFtl->WritePbaIndex;

    // replace with next good block
    while (1) {
        e =  FtlLiteGetBlockInfo(hPrivFtl,
            DevNum, BlockToReplace, &BlockInfoOutputArgs);
        if (e) {
            // Mark block as bad
            (void)(FtlLiteMarkBlockAsBad(hPrivFtl, DevNum,
                BlockToReplace));
        } else {
            if (BlockInfoOutputArgs.IsFactoryGoodBlock &&
                BlockInfoOutputArgs.IsRuntimeGoodBlock) {
                NvOsDebugPrintf("\r\n Replaced mapped block for lba=%d: "
                    "old=%d new pba=%d ", LogicalBlockNumber,
                    OldPba, BlockToReplace);

                e = FtlLitePrivCopybackData(hPrivFtl, DevNum, OldPba,BlockToReplace,
                        StartPgCurrOp);
                if(e)
                    continue;
                break;
            } else {
                if (!BlockInfoOutputArgs.IsFactoryGoodBlock)
                    NvOsDebugPrintf("\r\n Factory bad block at chip=%d blk=%d: ",
                        DevNum, BlockToReplace);
                if (!BlockInfoOutputArgs.IsRuntimeGoodBlock)
                    NvOsDebugPrintf("\r\n Runtime bad block at chip=%d blk=%d: ",
                        DevNum, BlockToReplace);
            }
        }
        if (BlockToReplace <
            (hPrivFtl->StartPhysicalBlock + hPrivFtl->TotalPhysicalBlocks))
            BlockToReplace++;
        else {
            NvOsDebugPrintf("\r\n Error: exhausted spare blocks to"
                "replace lba=%d ", LogicalBlockNumber);
            return NvError_NandNoFreeBlock;
        }
    }
    for (j = PbaIndex + 1; j < hPrivFtl->NumberOfLogicalBlocks; j++) {
        if (hPrivFtl->PhysicalBlocks[DevNum][j] == BlockToReplace) break;
    }
    // reuse remaining mapped blocks
    for (i = PbaIndex; j < hPrivFtl->NumberOfLogicalBlocks; i++, j++) {
        hPrivFtl->PhysicalBlocks[DevNum][i] = hPrivFtl->PhysicalBlocks[DevNum][j];
    }
    NvOsDebugPrintf("\r\n finished remapping till index=%d out of total blocks=%d ", i,
        hPrivFtl->NumberOfLogicalBlocks);
    // map to available spare blocks
    for (j = 0; ((i < hPrivFtl->NumberOfLogicalBlocks) &&
            (j < hPrivFtl->NumberOfSpareBlocks[DevNum])); i++, j++) {
        hPrivFtl->PhysicalBlocks[DevNum][i] = hPrivFtl->SpareBlocks[DevNum][j];
        hPrivFtl->SpareBlocks[DevNum][j] = SENTINEL_SPARE;
    }
    NvOsDebugPrintf("\r\n used spare blocks=%d ", j);
    // check for insufficient spare blocks
    if (i < hPrivFtl->NumberOfLogicalBlocks) {
        NvOsDebugPrintf("\r\n Error: Unable to replace blocks with spare blocks for %d blocks ",
            (hPrivFtl->NumberOfLogicalBlocks - i));
        return NvError_NandNoFreeBlock;
    }
    hPrivFtl->NumberOfSpareBlocks[DevNum] -= j;
    //rearrange the spare blocks
    RetFlag = FtlLitePrivRearrangeSpareBlocks(hPrivFtl,DevNum);
    if (!RetFlag)
        return NvError_CountMismatch;
    return NvSuccess;
}

NvError
FtlLitePrivReplaceBlock(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 PbaIndex,
    NvU32 StartPgCurrOp)
{
    NvU32 BlockToReplace;
    NvU32 FreeBlock;
    NvU32 SpareBlkIndex = 0;
    NvU32 PageNum[MAX_NAND_SUPPORTED];
    TagInformation tagInfo;
    NvU32 NumberOfBlocks = 1;
    NvError e = NvError_Success;
    CHECK_PARAM(DevNum<MAX_NAND_SUPPORTED);
    BlockToReplace = hPrivFtl->PhysicalBlocks[DevNum][PbaIndex];
    // If table is created partially create full table
    if (hPrivFtl->IsPartiallyCreated)
    {
        // Table is partially created. Create full table

        NV_CHECK_ERROR(
            FtlLitePrivCreatePba2LbaMapping(hPrivFtl, NV_FALSE)
        );
    }
    NvOsMemset(&tagInfo, 0x0, sizeof(TagInformation));

    NextBlock:
    CHECK_VALUE(SpareBlkIndex < hPrivFtl->NumberOfSpareBlocks[DevNum]);
    CHECK_VALUE(hPrivFtl->SpareBlocks[DevNum]);
    FreeBlock = hPrivFtl->SpareBlocks[DevNum][SpareBlkIndex];
    {
        NvU32 AbsBlockNum = FreeBlock;
        NvU32 ChipNum = DevNum;
        //get actual pba
        NvBtlGetPba(&ChipNum,&AbsBlockNum);
        NvOsDebugPrintf("\nNew Block at: chip=%d,block=%d ", ChipNum,
            AbsBlockNum);
        NvOsMemset(PageNum, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
        PageNum[ChipNum] =
            AbsBlockNum *hPrivFtl->NandDevInfo.PagesPerBlock;
        //erase the free block from the spare blocks
        if (hPrivFtl->DdkFunctions.NvDdkDeviceErase(
                                            hPrivFtl->hNvDdkNand,
                                            ChipNum,
                                            PageNum,
                                            &NumberOfBlocks) != NvError_Success)
        {
            NV_CHECK_ERROR_CLEANUP(FtlLiteMarkBlockAsBad(hPrivFtl, ChipNum,
                AbsBlockNum));
            hPrivFtl->SpareBlocks[DevNum][SpareBlkIndex] = SENTINEL_SPARE;
            SpareBlkIndex++;
            goto NextBlock;
        }
        //copyback data from error block to the new block
        if (FtlLitePrivCopybackData(hPrivFtl,DevNum, BlockToReplace, FreeBlock,
            StartPgCurrOp) != NvError_Success)
        {
            hPrivFtl->SpareBlocks[DevNum][SpareBlkIndex] = SENTINEL_SPARE;
            SpareBlkIndex++;
            goto NextBlock;
        }
    }
    hPrivFtl->SpareBlocks[DevNum][SpareBlkIndex] = SENTINEL_SPARE;
    SpareBlkIndex++;
    // update lba to pba mapping with replaced block number
    hPrivFtl->PhysicalBlocks[DevNum][PbaIndex] = FreeBlock;
    hPrivFtl->NumberOfSpareBlocks[DevNum] -= SpareBlkIndex;
    //rearrange the spare blocks
    if (!FtlLitePrivRearrangeSpareBlocks(hPrivFtl,DevNum))
        return NvError_CountMismatch;

fail:
    return NvError_Success;
}

NvBool
FtlLitePrivRearrangeSpareBlocks(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum)
{
    NvU32 BlkCnt;
    NvU32 NumSpareBlocks = 0;
    NvU32 MaxSpareBlocks =
        hPrivFtl ->TotalPhysicalBlocks - hPrivFtl->NumberOfLogicalBlocks;
    for (BlkCnt = 0;BlkCnt < MaxSpareBlocks;BlkCnt++)
    {
        CHECK_VALUE(hPrivFtl->SpareBlocks[DevNum]);
        if (hPrivFtl->SpareBlocks[DevNum][BlkCnt] == SENTINEL_SPARE)
            continue;
        hPrivFtl->SpareBlocks[DevNum][NumSpareBlocks] =
            hPrivFtl->SpareBlocks[DevNum][BlkCnt];
        NumSpareBlocks++;
    }
    for (BlkCnt = NumSpareBlocks;BlkCnt < MaxSpareBlocks;BlkCnt++)
    {
        if (hPrivFtl->SpareBlocks[DevNum][BlkCnt] != SENTINEL_SPARE)
            hPrivFtl->SpareBlocks[DevNum][BlkCnt] = SENTINEL_SPARE;
        else
            break;
    }
    if (NumSpareBlocks != hPrivFtl->NumberOfSpareBlocks[DevNum])
        return NV_FALSE;
    else
        return NV_TRUE;
}

NvError
FtlLitePrivCopybackData(
    FtlLitePrivHandle  hPrivFtl,
    NvU32 DevNum,
    NvU32 SrcBlock,
    NvU32 DestBlock,
    NvU32 StartPgCurrOp)
{
    NvError e = NvError_Success;
    NvU32 ByteOffset;
    NvU32 SectorNum;
    NvU8* SectorData;
    TagInformation tagInfo;
    NvU32 NumSectors;
    NvU32 SrcPageNumber[MAX_NAND_SUPPORTED];
    NvU32 DstPageNumber[MAX_NAND_SUPPORTED];
    SectorData = NvOsAlloc(hPrivFtl->NandDevInfo.PageSize);
    CHECK_MEM(SectorData);
    NvOsMemset(SrcPageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    NvBtlGetPba(&DevNum,&SrcBlock);
    SrcPageNumber[DevNum] =
        (SrcBlock * hPrivFtl->NandDevInfo.PagesPerBlock);
    NvOsMemset(DstPageNumber, 0xFF, (sizeof(NvU32) * MAX_NAND_SUPPORTED));
    NvBtlGetPba(&DevNum,&DestBlock);
    DstPageNumber[DevNum] =
        (DestBlock * hPrivFtl->NandDevInfo.PagesPerBlock);
    // FIXME: Assuming WritePointer is not updated in between.
    // Copyback should be done for old pages only - as write to a page
    // twice without erase causes Ecc Read failure later on.
    for (SectorNum = 0; SectorNum < StartPgCurrOp; SectorNum++)
    {
        NumSectors = 1;
        NvOsMemset(&tagInfo, 0x0, sizeof(TagInformation));
        //ignore read errors
        e = hPrivFtl->DdkFunctions.NvDdkDeviceRead(
                                            hPrivFtl->hNvDdkNand,
                                            DevNum,
                                            SrcPageNumber,
                                            SectorData,
                                            (NvU8*)&tagInfo,
                                            &NumSectors,
                                            NV_FALSE);
        // Read errors from old block ignored. Bad marking done by caller
        for (ByteOffset = 0; ByteOffset < hPrivFtl->NandDevInfo.PageSize; ByteOffset++)
        {
            if (SectorData[ByteOffset] != 0xFF)
                break;
        }
        // Page if not written will have all 0xFF, skip write in such case
        if (ByteOffset == hPrivFtl->NandDevInfo.PageSize)
        {
            goto LblNextSector;
        }
        NumSectors = 1;
        e = hPrivFtl->DdkFunctions.NvDdkDeviceWrite(
                                            hPrivFtl->hNvDdkNand,
                                            DevNum,
                                            DstPageNumber,
                                            SectorData,
                                            (NvU8*)&tagInfo,
                                            &NumSectors);
        if (e != NvError_Success)
        {
            // upon Write failure this new block marked bad
            NV_CHECK_ERROR_CLEANUP(FtlLiteMarkBlockAsBad(hPrivFtl, DevNum,
                DestBlock));
            goto fail;
        }
LblNextSector:
        // increment page number
        SrcPageNumber[DevNum] += NumSectors;
        DstPageNumber[DevNum] += NumSectors;
    }

    fail:
    NvOsFree(SectorData);
    return e;
}

