#include "nvnandftlfull.h"
#include "nvnandtlfull.h"
#include "nand_sector_cache.h"
#include "nand_debug_utility.h"
#include "nvddk_nand_blockdev_defs.h"
#include "nand_ttable.h"

static NvBool NandConfigInit(NvNandHandle hNand)
{
    NvS8 tBuffer[50];
    NvU32 bank;
    NvS32 ReqBlkCount;
    NvS32 ReqRowCount;
    NvBool status = NV_FALSE;
    NvS32 TotPhysBlksPerDevice;
#ifdef NVBL_FLASH
    NvDdkNandClientBuffer *pNandTagBuffer;
    TagInformation *pTagInfo;
    NvS32 reservedBlocks;
    NvError err;
#endif
    static NvBool s_initialized = NV_FALSE;
    NvU8 NumberOfDevices = MAX_NAND_SUPPORTED;

    if (s_initialized)
    {
        return NV_TRUE;
    }
    // TODO tBuffer = new char[50];
    hNand->NandCfg.NoOfPhysDevs = 0;
    hNand->NandCfg.AllowTranslationForSinglePlane = NV_FALSE;

    for (bank = 0; bank < NumberOfDevices; bank++)
    {
        hNand->NandCfg.DeviceLocations[bank] = -1;
        NvOsMemset(&hNand->NandCfg.NandDevInfo, 0, sizeof(NandDeviceInfo));

        // Do get device information to determine if the device/chip exist
        if (NandTLGetDeviceInfo(hNand, bank, &hNand->NandCfg.NandDevInfo) != NvSuccess)
            continue;

        hNand->NandCfg.DeviceLocations[hNand->NandCfg.NoOfPhysDevs] = bank;
        hNand->NandCfg.NoOfPhysDevs++;
        NumberOfDevices = hNand->NandCfg.NandDevInfo.NumberOfDevices;
    }

    // NV_NAND_CONFIG_PRINT((tBuffer, "\n NoOfPhysDevs %d \n", hNand->NandCfg.NoOfPhysDevs));
    NandDbgOutput(tBuffer);

    // return error if no devices are found
    if (!hNand->NandCfg.NoOfPhysDevs)
        goto errorHandle;

    NandTLGetDeviceInfo(hNand, 0, &hNand->NandCfg.NandDevInfo);
    TotPhysBlksPerDevice = hNand->NandCfg.NandDevInfo.NoOfPhysBlks;
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n Physical blocks per device %d \n", TotPhysBlksPerDevice));
    NandDbgOutput(tBuffer);
    hNand->NandCfg.BlkCfg.Capability = hNand->NandCfg.NandDevInfo.InterleaveCapabililty;
    // return error if we have odd number of devices(just one device is fine)
    if ((hNand->NandCfg.NoOfPhysDevs > 1) && (hNand->NandCfg.NoOfPhysDevs & 0x01))
        goto errorHandle;

    // if we have enough physical chips to do interleave operation,
    // no need to check for additional capabilties
    if ((hNand->NandCfg.NoOfPhysDevs >= MAX_ALLOWED_INTERLEAVE_BANKS) ||
    (hNand->NandCfg.BlkCfg.Capability == SINGLE_PLANE))
    {
        hNand->NandCfg.BlkCfg.Capability = SINGLE_PLANE;
        hNand->NandCfg.BlkCfg.VirtBanksPerDev = 1;
        // NV_NAND_CONFIG_PRINT((tBuffer, "\n capability SINGLE_PLANE \n"));
        NandDbgOutput(tBuffer);
    }
    else
    {
        /* current flash device can support multi plane functionalities;
        based on number of flash devices available, decide
        whether or not use the multi plane capability*/
        switch (hNand->NandCfg.BlkCfg.Capability)
        {
        case MULTIPLANE_ALT_BLOCK_INTERLEAVE:
            // device supports multiplane(2 devices)
            // and interleave(2 devices) = 4 virtual devices
            hNand->NandCfg.BlkCfg.VirtBanksPerDev = 4;
            if ((hNand->NandCfg.NoOfPhysDevs * hNand->NandCfg.BlkCfg.VirtBanksPerDev) >
            MAX_ALLOWED_INTERLEAVE_BANKS)
            {
                // device supports multiplane(2 devices)
                // and interleave(2 devices) = 4 virtual devices
                hNand->NandCfg.AllowTranslationForSinglePlane = NV_TRUE;
                hNand->NandCfg.BlkCfg.VirtBanksPerDev = 2;
                hNand->NandCfg.BlkCfg.Capability = SINGLE_PLANE_INTERLEAVE;
                // NV_NAND_CONFIG_PRINT((tBuffer, "\n capability SINGLE_PLANE_INTERLEAVE \n"));
                NandDbgOutput(tBuffer);
            }
            else break;
        case SINGLE_PLANE_INTERLEAVE:
            hNand->NandCfg.AllowTranslationForSinglePlane = NV_TRUE;
        case MULTIPLANE_ALT_BLOCK:
        case MULTIPLANE_ALT_PLANE:
            hNand->NandCfg.BlkCfg.VirtBanksPerDev = 2;
            if ((hNand->NandCfg.NoOfPhysDevs * hNand->NandCfg.BlkCfg.VirtBanksPerDev) >
            MAX_ALLOWED_INTERLEAVE_BANKS)
            {
                hNand->NandCfg.BlkCfg.VirtBanksPerDev = 1;
                hNand->NandCfg.BlkCfg.Capability = SINGLE_PLANE;
                // NV_NAND_CONFIG_PRINT((tBuffer, "\n capability SINGLE_PLANE \n"));
                NandDbgOutput(tBuffer);
            }
            break;
        default:
            goto errorHandle;
        }
    }
    hNand->NandCfg.BlkCfg.PhysBlksPerZone =
        hNand->NandCfg.NandDevInfo.NoOfPhysBlks /
        hNand->NandCfg.NandDevInfo.NoOfZones;

    hNand->NandCfg.BlkCfg.LogBlksPerZone =
        (hNand->NandCfg.BlkCfg.PhysBlksPerZone/1024) * 1000;
    hNand->NandCfg.BlkCfg.NoOfBanks =
        hNand->NandCfg.NoOfPhysDevs * hNand->NandCfg.BlkCfg.VirtBanksPerDev;
    hNand->NandCfg.BlkCfg.PhysBlksPerBank =
        TotPhysBlksPerDevice/hNand->NandCfg.BlkCfg.VirtBanksPerDev;

    hNand->NandCfg.BlkCfg.ILBankCount = hNand->InterleaveCount;
    // For optimum throughput performance it's not good to have interleave
    // banks count more than MAX_ALLOWED_INTERLEAVE_BANKS.
    // So if the banks count is more than MAX_ALLOWED_INTERLEAVE_BANKS divide
    // by 2. Here the assumption or the requirement is the number of banks
    // should always be even count.
    if (hNand->NandCfg.BlkCfg.ILBankCount == 0)
    {
        hNand->NandCfg.BlkCfg.ILBankCount = 1;
    }
    if (hNand->NandCfg.BlkCfg.ILBankCount > MAX_ALLOWED_INTERLEAVE_BANKS)
    {
        hNand->NandCfg.BlkCfg.ILBankCount = MAX_ALLOWED_INTERLEAVE_BANKS;
    }
    // total number of banks should always be in multiples of interleave banks
    if (hNand->NandCfg.BlkCfg.NoOfBanks % hNand->NandCfg.BlkCfg.ILBankCount)
        hNand->NandCfg.BlkCfg.ILBankCount /= 2;

    // calculate appropriate ecc algorith based on the type of the nand part
    if ((hNand->NandCfg.NandDevInfo.DeviceType == SLC))
    {
        hNand->NandCfg.BlkCfg.DataAreaEccAlgorithm = ECCAlgorithm_HAMMING;
        hNand->NandCfg.BlkCfg.DataAreaEccThreshold = 1;
    }
    else
    {
        hNand->NandCfg.BlkCfg.DataAreaEccAlgorithm = ECCAlgorithm_REED_SOLOMON;
        hNand->NandCfg.BlkCfg.DataAreaEccThreshold = 3;
    }

    hNand->NandCfg.BlkCfg.DataAreaEccAlgorithm = ECCAlgorithm_REED_SOLOMON;
    hNand->NandCfg.BlkCfg.DataAreaEccThreshold = 3;

    hNand->NandCfg.PhysicalPagesPerZone =
        hNand->NandCfg.BlkCfg.PhysBlksPerZone *
        hNand->NandCfg.NandDevInfo.PgsPerBlk;

    {
        ReqBlkCount = GeneralConfiguration_BLOCKS_FOR_TAT_STORAGE;
        ReqRowCount = (ReqBlkCount / hNand->NandCfg.BlkCfg.ILBankCount);
        ReqRowCount += (ReqBlkCount % hNand->NandCfg.BlkCfg.ILBankCount) ? 1 : 0;
        ReqRowCount = ReqRowCount * hNand->NandCfg.NoOfPhysDevs;
        hNand->NandCfg.BlkCfg.NoOfTatBlks =
            ReqRowCount * hNand->NandCfg.BlkCfg.ILBankCount;
    }
    {
        ReqBlkCount = GeneralConfiguration_BLOCKS_FOR_TT_STORAGE;
        ReqRowCount = (ReqBlkCount / hNand->NandCfg.BlkCfg.ILBankCount);
        ReqRowCount += (ReqBlkCount % hNand->NandCfg.BlkCfg.ILBankCount) ? 1 : 0;
        ReqRowCount = ReqRowCount * hNand->NandCfg.NoOfPhysDevs;
        hNand->NandCfg.BlkCfg.NoOfTtBlks =
            ReqRowCount * hNand->NandCfg.BlkCfg.ILBankCount;
    }
    {
        ReqBlkCount = GeneralConfiguration_BLOCKS_FOR_BADBLOCK_STORAGE;
        ReqRowCount = (ReqBlkCount / hNand->NandCfg.BlkCfg.ILBankCount);
        ReqRowCount += (ReqBlkCount % hNand->NandCfg.BlkCfg.ILBankCount) ? 1 : 0;
        hNand->NandCfg.BlkCfg.NoOfFbbMngtBlks =
            ReqRowCount * hNand->NandCfg.BlkCfg.ILBankCount;
    }

    // NV_NAND_CONFIG_PRINT((tBuffer, "\n capability %d \n", hNand->NandCfg.BlkCfg.Capability));
    NandDbgOutput(tBuffer);
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n Physical blocks per Zone %d \n", hNand->nandCfg.blockConfiguration.NoOfPhysBlksPerZone));
    NandDbgOutput(tBuffer);
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n virtualBanksPerDevice %d \n", hNand->nandCfg.blockConfiguration.virtualBanksPerDevice));
    NandDbgOutput(tBuffer);
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n numberOfBanks %d \n", hNand->nandCfg.blockConfiguration.numberOfBanks));
    NandDbgOutput(tBuffer);
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n Physical blocks per bank %d \n", hNand->nandCfg.blockConfiguration.numberOfPhysicalBlocksPerBank));
    NandDbgOutput(tBuffer);
    // NV_NAND_CONFIG_PRINT((tBuffer, "\n interleaveBankCount %d \n", hNand->nandCfg.blockConfiguration.interleaveBankCount));
    NandDbgOutput(tBuffer);

    s_initialized = NV_TRUE;
    status = NV_TRUE;
    hNand->NandCfg.IsInitialized = NV_TRUE;
errorHandle:
    // TODO delete tBuffer;
    return status;
}

static NvError 
FtlFullGetBlockInfo(FtlFullPrivHandle hFtl, 
                NvU32 ChipNumber,
                NvU32 PBA, 
                NvDdkBlockDevIoctl_IsGoodBlockOutputArgs *pSpareInfo);

NvError 
NvNandFtlFullOpen(
    FtlFullPrivHandle*  phNandFtlFull,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion)
{
    FtlFullPrivHandle  hPrivFtl;
    NvError ErrorStatus = NvError_NandOperationFailed;
    NvU32 MinPhysicalBlocksReq = 0;
    hPrivFtl = NvOsAlloc(sizeof(NvNand));

    if (hPrivFtl == NULL)
    {
        ErrorStatus = NvError_InsufficientMemory;
        goto exitError;
    }
    NvOsMemset(hPrivFtl, 0, sizeof(NvNand));
    hPrivFtl->hNvDdkNand = hNvDdkNand;
    hPrivFtl->IsFirstBoot = NV_FALSE;
    hPrivFtl->RegionCount = 0;
    hPrivFtl->NandFallBackMode = NV_FALSE;
    hPrivFtl->FtlStartPba = nandregion->StartPhysicalBlock;
    hPrivFtl->IsCacheEnabled = NV_TRUE;
    // The nandregion->TotalLogicalBlocks will beinterpreted as 
    // percent reserve data for the ftl full managed partition
    // This needs a better fix
    hPrivFtl->PercentReserved = nandregion->TotalLogicalBlocks;

    if(nandregion->TotalPhysicalBlocks == 0xFFFFFFFF)
        hPrivFtl->FtlEndPba = nandregion->TotalPhysicalBlocks;
    else
        hPrivFtl->FtlEndPba = nandregion->StartPhysicalBlock + nandregion->TotalPhysicalBlocks;
    hPrivFtl->FtlStartLba = nandregion->StartLogicalBlock;
    
    hPrivFtl->InterleaveCount = nandregion->InterleaveBankCount;
    
    hPrivFtl->DdkFuncs.NvDdkDeviceErase = pFuncPtrs->NvDdkDeviceErase;
    hPrivFtl->DdkFuncs.NvDdkDeviceRead= pFuncPtrs->NvDdkDeviceRead;
    hPrivFtl->DdkFuncs.NvDdkDeviceWrite= pFuncPtrs->NvDdkDeviceWrite;
    hPrivFtl->DdkFuncs.NvDdkGetDeviceInfo = pFuncPtrs->NvDdkGetDeviceInfo;
    hPrivFtl->DdkFuncs.NvDdkGetLockedRegions = pFuncPtrs->NvDdkGetLockedRegions;
    hPrivFtl->DdkFuncs.NvDdkReadSpare = pFuncPtrs->NvDdkReadSpare;
    hPrivFtl->DdkFuncs.NvDdkWriteSpare = pFuncPtrs->NvDdkWriteSpare;

    if (NandConfigInit(hPrivFtl) != NV_TRUE)
        goto exitError;
    
    MinPhysicalBlocksReq = hPrivFtl->NandCfg.BlkCfg.NoOfTtBlks + 
        hPrivFtl->NandCfg.BlkCfg.NoOfTatBlks;
    if((nandregion->TotalPhysicalBlocks != 0xFFFFFFFF) && 
        (nandregion->TotalPhysicalBlocks < (MinPhysicalBlocksReq +
        (2 + NUMBER_OF_LBAS_TO_TRACK))))
    {
        ErrorStatus = NvError_BadParameter;
        goto exitError;
    }

    if (NandTLInit(hPrivFtl) != NV_TRUE)
        goto exitError;

    ErrorStatus = NvError_Success;
    exitError:
    if (ErrorStatus != NvError_Success)
    {
        if (hPrivFtl)
        {
            NvOsFree(hPrivFtl);
        }
        NvOsDebugPrintf("\r\n RETURNING ERROR FROM NvNandOpen");
        RETURN(ErrorStatus);
    }
    else
    {
        hPrivFtl->FtlEndLba = hPrivFtl->FtlStartLba + hPrivFtl->ITat.Misc.TotalLogicalBlocks;
        *phNandFtlFull = hPrivFtl;
        return ErrorStatus;
    }
}


NvError 
NvNandFtlFullReadSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    void* const pBuffer,
    NvU32 NumberOfSectors)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;

    NvError Error;
    NvS32 lun = 0;
    NvS32 retVal = 0;
    NvU32 sector = 0;

    RW_TRACE(("\r\n\t**** [READ] StartBlock(%d)Page(%d)NUmPages(%d)**********",
                    SectorNumber >> hNand->NandStrat.bsc4PgsPerLBlk,
                    SectorNumber % hNand->NandStrat.PgsPerLBlk,
                    NumberOfSectors));
    if (hNand->NandTl.SectorCache)
    {
        if (NumberOfSectors <= MAX_SECTORS_TO_CACHE)
        {
            for (sector = 0; sector < NumberOfSectors; sector++)
            {
                retVal = NandSectorCacheRead(
                    hNand,
                    lun,
                    SectorNumber + sector,
                    (NvU8*)pBuffer + (sector * hNand->NandDevInfo.PageSize),
                    1);
            }
        }
        else
        {
            retVal = NandSectorCacheRead(
                hNand,
                lun,
                SectorNumber,
                (NvU8*)pBuffer,
                NumberOfSectors);
        }
    }
    else
    {
        retVal = NandTLRead(hNand, lun, SectorNumber, (NvU8*)pBuffer, NumberOfSectors);
    }

    if (retVal != NvSuccess)
    {
        Error = NvError_NandReadFailed;
        NvOsDebugPrintf("\r\n RETURNING ERROR FROM NvNandReadSector"
            " TL error=%u,Sector Start=0x%x,Count=0x%x ", retVal,
            SectorNumber, NumberOfSectors);
        RETURN(Error);
    }
    else
    {
        Error = NvSuccess;
        return Error;
    }
}

#ifndef DEBUG_WRITE_LOCK_AREA_ISSUE
#define DEBUG_WRITE_LOCK_AREA_ISSUE 0
#endif
#if DEBUG_WRITE_LOCK_AREA_ISSUE
// Local function used to print TT and locked physical block details.
static void
UtilBootupPrintInfo(NvNandHandle hNand)
{
    NvU32 nandDevice = 0;
    // Print TT
    DumpTT(hNand, -1);
    // Print locked physical blocks
    for (nandDevice = 0;nandDevice < MAX_NAND_SUPPORTED;nandDevice++)
    {
        NvOsDebugPrintf("\nLocked region[%d]: Dev%d, StartPg=0x%x,EndPg=0x%x ",
            nandDevice, hNand->NandLockParams[nandDevice].DeviceNumber,
            hNand->NandLockParams[nandDevice].StartPageNumber,
            hNand->NandLockParams[nandDevice].EndPageNumber);
    }
}
#endif

NvError 
NvNandFtlFullWriteSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    const void* pBuffer,
    NvU32 NumberOfSectors)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;
    NvError Error;
    NvS32 lun = 0;
    NvS32 retVal = 0;
    NvU32 sector = 0;
    RW_TRACE(("\r\n\t**** [WRITE] StartBlock(%d)Page(%d)NUmPages(%d)**********",
                    SectorNumber>> hNand->NandStrat.bsc4PgsPerLBlk,
                    SectorNumber % hNand->NandStrat.PgsPerLBlk,
                    NumberOfSectors));

    if (NandTLIsLbaLocked(hNand, SectorNumber) == NV_TRUE)
    {
#if 0 /*DEBUG_WRITE_LOCK_AREA_ISSUE*/
        // Print the TT and locked physical blocks
        UtilBootupPrintInfo(hNand);
#endif
        return NvError_NandBlockIsLocked;
    }
    if (hNand->NandTl.SectorCache)
    {
        if (NumberOfSectors <= MAX_SECTORS_TO_CACHE)
        {
            for (sector = 0; sector < NumberOfSectors; sector++)
            {
                retVal = NandSectorCacheWrite(
                    hNand,
                    lun,
                    SectorNumber + sector,
                    (NvU8*)pBuffer + (sector*hNand->NandDevInfo.PageSize),
                    1);
            }
        }
        else
        {
            retVal = NandSectorCacheWrite(
                hNand,
                lun,
                SectorNumber,
                (NvU8*)pBuffer,
                NumberOfSectors);
        }
    }
    else
    {
        retVal = NandTLWrite(hNand, SectorNumber, (NvU8*)pBuffer, NumberOfSectors);
    }

    if(!hNand->IsCacheEnabled)
    {
        NvNandFtlFullFlush(hRegion);
    }
    
    if (retVal != NvSuccess)
    {
        NvOsDebugPrintf("\r\n RETURNING ERROR FROM NvNandWriteSector "
            " TL error=%u,Sector Start=0x%x,Count=0x%x ", retVal,
            SectorNumber, NumberOfSectors);
        Error = NvError_NandProgramFailed;
        RETURN(Error);
    }
    else
    {
        Error = NvSuccess;
        return Error;
    }
}


void NvNandFtlFullClose(NvNandRegionHandle hRegion)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;

    NvOsFree(hNand->pSpareBuf);
    NvOsFree(hNand);
    NvOsFree(hRegion);
}

void
NvNandFtlFullGetInfo(
    NvNandRegionHandle hRegion,
    NvNandRegionInfo* pNandRegionInfo)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;

    pNandRegionInfo->BytesPerSector = hNand->NandDevInfo.PageSize;
    pNandRegionInfo->SectorsPerBlock = hNand->NandDevInfo.PgsPerBlk * 
        hNand->InterleaveCount;
    pNandRegionInfo->TotalBlocks = hNand->NandTl.TotalLogicalBlocks;
}

NvError
NvNandFtlFullIoctl(
    NvNandRegionHandle hRegion,
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;
    NvError e = NvError_Success;
    switch (Opcode)
    {
        case NvDdkBlockDevIoctlType_MapLogicalToPhysicalSector:
            break;
        case NvDdkBlockDevIoctlType_QueryFirstBoot:
            {
                NvDdkBlockDevIoctl_QueryFirstBootOutputArgs* OutArgs = 
                    (NvDdkBlockDevIoctl_QueryFirstBootOutputArgs*)OutputArgs;
                OutArgs->IsFirstBoot = hNand->IsFirstBoot;
            }
            break;
        case NvDdkBlockDevIoctlType_DisableCache:
            {
                hNand->IsCacheEnabled = NV_FALSE;
                NvNandFtlFullFlush(hRegion);
                if(hNand->NandTl.SectorCache)
                {
                    NvOsFree(hNand->NandTl.SectorCache);
                    hNand->NandTl.SectorCache = NULL;
                }
            }
            break;
        case NvDdkBlockDevIoctlType_DefineSubRegion:
            if(hNand->IsFirstBoot)
            {
                NvDdkBlockDevIoctl_DefineSubRegionInputArgs* pSubRegionInfo = 
                    (NvDdkBlockDevIoctl_DefineSubRegionInputArgs*)InputArgs;
                
                NV_CHECK_ERROR_CLEANUP(NandTLSetRegionInfo(hNand,pSubRegionInfo));
            }
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
                    hNand->ITat.Misc.bsc4PgsPerBlk;

                if(pEraseLbaInfo->NumberOfLogicalSectors != 0xFFFFFFFF)
                    EndtBlock = StartBlock + 
                        (pEraseLbaInfo->NumberOfLogicalSectors >>hNand->ITat.Misc.bsc4PgsPerBlk);

                if(!NandTLEraseLogicalBlocks(hNand,StartBlock,EndtBlock))
                    return NvError_NandEraseFailed;
            }
            break;
        case NvDdkBlockDevIoctlType_ForceBlkRemap:
            e = NandTLEraseAllBlocks(hNand);
            break;
        case NvDdkBlockDevIoctlType_WriteVerifyModeSelect:
            {
                NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *
                    pWriteVerifyModeIn =
                    (NvDdkBlockDevIoctl_WriteVerifyModeSelectInputArgs *)InputArgs;
                NV_ASSERT(InputArgs);
                hNand->IsReadVerifyWriteEnabled = 
                    pWriteVerifyModeIn->IsReadVerifyWriteEnabled;
            }
            break;
        case NvDdkBlockDevIoctlType_IsGoodBlock:
            {
                NvDdkBlockDevIoctl_IsGoodBlockInputArgs *pBlockInfoInputArgs =
                            (NvDdkBlockDevIoctl_IsGoodBlockInputArgs*) InputArgs;
                NvDdkBlockDevIoctl_IsGoodBlockOutputArgs *pBlockInfoOutputArgs =
                            (NvDdkBlockDevIoctl_IsGoodBlockOutputArgs*) OutputArgs;
                NV_ASSERT(hNand);
                return (FtlFullGetBlockInfo(hNand, 
                                pBlockInfoInputArgs->ChipNumber, 
                                pBlockInfoInputArgs->PhysicalBlockAddress, 
                                pBlockInfoOutputArgs));
            }
        default:
            e = NvError_NotSupported;
            break;
    }
    fail:
    return e;
}


static NvError 
FtlFullGetBlockInfo(FtlFullPrivHandle hFtl, 
                    NvU32 ChipNumber,
                    NvU32 PBA, 
                    NvDdkBlockDevIoctl_IsGoodBlockOutputArgs* pSpareInfo)
{
    NvError e = NvSuccess;
    NandBlockInfo NandBlockInfo;

    NV_ASSERT(hFtl->pSpareBuf);
    NV_ASSERT(hFtl->gs_NandDeviceInfo.NumSpareAreaBytes);
    NV_ASSERT(pSpareInfo);

    NandBlockInfo.pTagBuffer = hFtl->pSpareBuf;
    // Initialize the spare buffer before calling GetBlockInfo API
    NvOsMemset(NandBlockInfo.pTagBuffer, 0,
        hFtl->gs_NandDeviceInfo.NumSpareAreaBytes);
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
    // Run-time bad block check
    if (hFtl->pSpareBuf[1] != 0xFF)
    {
        NvOsDebugPrintf("\nRuntime Bad block: Chip%u Block=%u,"
            "RTB=0x%x ", ChipNumber, PBA, hFtl->pSpareBuf[1]);
        pSpareInfo->IsRuntimeGoodBlock = NV_FALSE;
    }
    pSpareInfo->IsGoodBlock = pSpareInfo->IsFactoryGoodBlock && 
                                    pSpareInfo->IsRuntimeGoodBlock;
    return e;

}

void NvNandFtlFullFlush(NvNandRegionHandle hRegion)
{
    NvNandFtlFullHandle TempFtlFullHandle = (NvNandFtlFullHandle )hRegion;
    FtlFullPrivHandle  hNand = TempFtlFullHandle->hPrivFtl;

    if (hNand->NandTl.SectorCache)
        NandSectorCacheFlush(hNand, NV_FALSE);
    NandTLFlushTranslationTable(hNand);
}
