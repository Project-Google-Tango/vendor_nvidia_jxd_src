#include "nvddk_nand.h"
#include "nvassert.h"
#include "nvnandextftl.h"
#include "nvddk_nand_blockdev_defs.h"


typedef struct NandExtFtlRec
{
    NvDdkNandDeviceInfo NandDevInfo;
    NvU32 InterleaveBankCount;
    NvU32 NumberOfLogicalBlocks;

    NvDdkNandFunctionsPtrs DdkFunctions;
}NvNandFtlPriv;


/*
  * This API will do the following
  * Creates handle for the requested external FTL- region.
  * Creates physical block to logical block mapping.
  * Creates spare block array for bad block management
*/
NvError 
NvNandExtFtlOpen(
    ExtFtlPrivHandle*  phNandExtFtl,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion)
{
    NvError e = NvError_Success;
    ExtFtlPrivHandle  hPrivFtl = NULL;

    *phNandExtFtl = NULL;

    NV_ASSERT(hNvDdkNand && nandregion);// return NvError_BadParameter;
    NV_ASSERT(nandregion->TotalPhysicalBlocks >= nandregion->TotalLogicalBlocks);
    NV_ASSERT(nandregion->InterleaveBankCount > 0);

    //allocate memory for the external ftl  hand;e
    hPrivFtl = NvOsAlloc(sizeof(NvNandFtlPriv));
    NV_ASSERT(hPrivFtl != NULL);// return NvError_InsufficientMemory;

    //get the device info for nand.
    NV_CHECK_ERROR_CLEANUP(pFuncPtrs->NvDdkGetDeviceInfo(hNvDdkNand,
        0,&hPrivFtl->NandDevInfo));
    
    //initilalize handle variables
    hPrivFtl->InterleaveBankCount = nandregion->InterleaveBankCount;
    hPrivFtl->NumberOfLogicalBlocks = nandregion->TotalLogicalBlocks;
    hPrivFtl->DdkFunctions.NvDdkDeviceErase = pFuncPtrs->NvDdkDeviceErase;
    hPrivFtl->DdkFunctions.NvDdkDeviceRead = pFuncPtrs->NvDdkDeviceRead;
    hPrivFtl->DdkFunctions.NvDdkDeviceWrite = pFuncPtrs->NvDdkDeviceWrite;
    hPrivFtl->DdkFunctions.NvDdkGetDeviceInfo = pFuncPtrs->NvDdkGetDeviceInfo;
    hPrivFtl->DdkFunctions.NvDdkGetLockedRegions = pFuncPtrs->NvDdkGetLockedRegions;
    hPrivFtl->DdkFunctions.NvDdkGetBlockInfo = pFuncPtrs->NvDdkGetBlockInfo;
    hPrivFtl->DdkFunctions.NvDdkReadSpare = pFuncPtrs->NvDdkReadSpare;
    hPrivFtl->DdkFunctions.NvDdkWriteSpare = pFuncPtrs->NvDdkWriteSpare;

    *phNandExtFtl = hPrivFtl;
    return e;

fail:
    NvOsFree(hPrivFtl);
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
NvNandExtFtlReadSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    // Read fails as LBA to PBA information not available for external FTL
    NvError e = NvError_NandReadFailed;
    return e;
}

/*
  * This API will allow the client to write data in sequential as well as interleave mode.
  * On the first write operation, all the physical blocks for this region will be erased. In interleave
  * mode write should always start from offset zero and then all subsequent write requests
  * should be consecutive.
*/
NvError 
NvNandExtFtlWriteSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvError e = NvError_NandWriteFailed;
    return e;
}


void NvNandExtFtlClose(NvNandRegionHandle hRegion)
{
    NvNandExtFtlHandle TempExtFtlHandle = (NvNandExtFtlHandle)hRegion;
    ExtFtlPrivHandle  hPrivFtl = TempExtFtlHandle->hPrivFtl;

    // Free allocated buffers
    NvOsFree(hPrivFtl);
    NvOsFree(hRegion);
}

void NvNandExtFtlGetInfo(
    NvNandRegionHandle hRegion,
    NvNandRegionInfo* pNandRegionInfo)
{
    NvNandExtFtlHandle TempExtFtlHandle = (NvNandExtFtlHandle)hRegion;
    ExtFtlPrivHandle  hPrivFtl = TempExtFtlHandle->hPrivFtl;

    pNandRegionInfo->BytesPerSector = hPrivFtl->NandDevInfo.PageSize;
    pNandRegionInfo->SectorsPerBlock = hPrivFtl->NandDevInfo.PagesPerBlock * 
        hPrivFtl->InterleaveBankCount;
    pNandRegionInfo->TotalBlocks = hPrivFtl->NumberOfLogicalBlocks;
}

NvError 
NvNandExtFtlIoctl(
    NvNandRegionHandle hRegion, 
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            // Empty implementation as yaffs2 write calls Ddk APIs directly
            break;
        case NvDdkBlockDevIoctlType_IsGoodBlock:
            // Fallthrough code
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            // Fallthrough code
        case NvDdkBlockDevIoctlType_EraseLogicalSectors:
            // Fallthrough code
        case NvDdkBlockDevIoctlType_ForceBlkRemap:
            // Fallthrough code
        default:
            return NvError_NotSupported;
    }
    return NvError_Success;
}

void NvNandExtFtlFlush(NvNandRegionHandle hRegion)
{
    //nothing to flush in external ftl 
    if (hRegion) {}; 
}


