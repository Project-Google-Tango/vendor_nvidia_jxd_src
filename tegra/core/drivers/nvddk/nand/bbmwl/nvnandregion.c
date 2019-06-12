/*
 * Copyright (c) 2009 - 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvnandregion.h"
#include "ftlfull/nvnandftlfull.h"
#include "ftllite/nvnandftllite.h"
#include "extftl/nvnandextftl.h"

typedef struct NvBtlRec
{
    NvU32 Initlialized;
    NvU32 NoOfBlocksPerDevice;
    NvU32 NoOfDevices;
    NvU32 InterleaveCount;
}NvBtl;

// Static variables
static NvBtl g_NvBtl;

NvError
NvNandOpenRegion(
    NvNandRegionHandle* phNandRegion,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandRegion)
{
    NvError e = NvError_Success;
    *phNandRegion = NULL;
    switch (nandRegion->MgmtPolicy)
    {
        case 2:
            {
                NvNandFtlFullHandle TempFtlHandle = NULL;
                TempFtlHandle = NvOsAlloc(sizeof(NvNandFtlFull));
                if(!TempFtlHandle)
                    return NvError_InsufficientMemory;
                TempFtlHandle->NandRegion.NvNandCloseRegion = NvNandFtlFullClose;
                TempFtlHandle->NandRegion.NvNandRegionGetInfo= NvNandFtlFullGetInfo;
                TempFtlHandle->NandRegion.NvNandRegionReadSector = NvNandFtlFullReadSector;
                TempFtlHandle->NandRegion.NvNandRegionWriteSector = NvNandFtlFullWriteSector;
                TempFtlHandle->NandRegion.NvNandRegionFlushCache = NvNandFtlFullFlush;
                TempFtlHandle->NandRegion.NvNandRegionIoctl = NvNandFtlFullIoctl;
                e = NvNandFtlFullOpen(&TempFtlHandle->hPrivFtl, hNvDdkNand, pFuncPtrs, nandRegion);
                if (e != NvError_Success)
                    NvOsFree(TempFtlHandle);
                else
                *phNandRegion = &TempFtlHandle->NandRegion;
            }
            break;
        //fall through if we are in bootloader
        case 1:
            {
                NvNandFtlLiteHandle TempFtlHandle = NULL;
                TempFtlHandle = NvOsAlloc(sizeof(NvNandFtlLite));
                if(!TempFtlHandle)
                    return NvError_InsufficientMemory;
                TempFtlHandle->NandRegion.NvNandCloseRegion = NvNandFtlLiteClose;
                TempFtlHandle->NandRegion.NvNandRegionGetInfo= NvNandFtlLiteGetInfo;
                TempFtlHandle->NandRegion.NvNandRegionReadSector = NvNandFtlLiteReadSector;
                TempFtlHandle->NandRegion.NvNandRegionWriteSector = NvNandFtlLiteWriteSector;
                TempFtlHandle->NandRegion.NvNandRegionFlushCache = NvNandFtlLiteFlush;
                TempFtlHandle->NandRegion.NvNandRegionIoctl = NvNandFtlLiteIoctl;
                e = NvNandFtlLiteOpen(&TempFtlHandle->hPrivFtl, hNvDdkNand, pFuncPtrs, nandRegion);
                if (e != NvError_Success)
                    NvOsFree(TempFtlHandle);
                else
                *phNandRegion = &TempFtlHandle->NandRegion;
            }
            break;
        case 3:
            {
                NvNandExtFtlHandle TempFtlHandle = NULL;
                TempFtlHandle = NvOsAlloc(sizeof(NvNandExtFtl));
                if(!TempFtlHandle)
                    return NvError_InsufficientMemory;
                TempFtlHandle->NandRegion.NvNandCloseRegion = NvNandExtFtlClose;
                TempFtlHandle->NandRegion.NvNandRegionGetInfo= NvNandExtFtlGetInfo;
                TempFtlHandle->NandRegion.NvNandRegionReadSector = NvNandExtFtlReadSector;
                TempFtlHandle->NandRegion.NvNandRegionWriteSector = NvNandExtFtlWriteSector;
                TempFtlHandle->NandRegion.NvNandRegionFlushCache = NvNandExtFtlFlush;
                TempFtlHandle->NandRegion.NvNandRegionIoctl = NvNandExtFtlIoctl;
                e = NvNandExtFtlOpen(&TempFtlHandle->hPrivFtl, hNvDdkNand,
                    pFuncPtrs, nandRegion);
                if (e != NvError_Success)
                    NvOsFree(TempFtlHandle);
                else
                *phNandRegion = &TempFtlHandle->NandRegion;
            }
            break;
        default:
            e = NvError_BadValue;
            break;
    }
    return e;
}

void NvBtlInit(NvDdkNandHandle hNvDdkNand, NvDdkNandFunctionsPtrs *pFuncPtrs, NvU32 GlobalInterleaveCount)
{
    NvDdkNandDeviceInfo NandDevInfo;
    pFuncPtrs->NvDdkGetDeviceInfo(hNvDdkNand,0,&NandDevInfo);
    g_NvBtl.NoOfBlocksPerDevice = NandDevInfo.NoOfBlocks;
    g_NvBtl.NoOfDevices = NandDevInfo.NumberOfDevices;
    g_NvBtl.InterleaveCount = GlobalInterleaveCount;
    g_NvBtl.Initlialized = 1;

}

NvBool NvBtlGetPba(NvU32 *DevNum, NvU32 *BlockNum)
{
    NvU32 ReturnBlockNum;
    if ((!g_NvBtl.Initlialized) ||
        (!g_NvBtl.InterleaveCount) ||
        (g_NvBtl.InterleaveCount  > g_NvBtl.NoOfDevices))
        return NV_FALSE;
    if (g_NvBtl.InterleaveCount  == 1)
    {
        ReturnBlockNum = *BlockNum % g_NvBtl.NoOfBlocksPerDevice;
        *DevNum = *BlockNum / g_NvBtl.NoOfBlocksPerDevice;
    }
    else
    {
        NvU32 RowNumber = *BlockNum / g_NvBtl.NoOfBlocksPerDevice;
        NvU32 DevicesPerCol = (g_NvBtl.NoOfDevices / g_NvBtl.InterleaveCount);
        ReturnBlockNum = *BlockNum % g_NvBtl.NoOfBlocksPerDevice;
        *DevNum = (*DevNum * DevicesPerCol) + RowNumber;
    }
    *BlockNum = ReturnBlockNum;
    return NV_TRUE;
}


/*blank definitions for NvNandFtl APIs to satisfy compiler
 * This code should be removed once multi partiton is enabled by default
 */
typedef struct NandRegionsRec
{
    NvS32 numRegions;
}NandRegions;

// Function prototypes needed for definitions used by Shim
NvError NvNandOpen(
    NvNandHandle* hNand,
    NandArgs* nandArgs,
    void (*getRegionInfo)(NandRegions *nandRegions));

void NvNandClose(NvNandHandle hNand);
NvError NvNandReadSector(
    NvNandHandle hNand,
    NvU32 lba,
    NvU8* pBuffer,
    NvU32 NumberOfSectors);
NvError NvNandWriteSector(
    NvNandHandle hNand,
    NvU32 lba,
    NvU8* pBuffer,
    NvU32 NumberOfSectors);
NvU32 NvNandGetNoOfSectors(NvNandHandle hNand);
NvU32  NvNandGetBlockSize(NvNandHandle hNand);
void NvNandFlushCache(NvNandHandle hNand);
NvU32 NvNandGetSectorSize(NvNandHandle hNand);
NvError NvNandIoctl(NvNandHandle hNand, NvS32 command, void* data, NvS32 arg1, NvS32 arg2);
NvError NvNandSuspend(NvNandHandle hNand);
NvError NvNandResume(NvNandHandle hNand);


//Function definitions
NvError NvNandOpen(
    NvNandHandle* hNand,
    NandArgs* nandArgs,
    void (*getRegionInfo)(NandRegions *nandRegions))
{
    return NvError_Success;
}
void NvNandClose(NvNandHandle hNand)
{
}

NvU32 NvNandGetSectorSize(NvNandHandle hNand)
{
    return 0;
}
NvError
NvNandReadSector(
                NvNandHandle hNand,
                NvU32 lba,
                NvU8*pBuffer,
                NvU32 NumberOfSectors)
{
    return NvError_Success;
}
NvError
NvNandWriteSector(
                 NvNandHandle hNand,
                 NvU32 lba,
                 NvU8* pBuffer,
                 NvU32 NumberOfSectors)
{
    return NvError_Success;
}
NvU32 NvNandGetNoOfSectors(NvNandHandle hNand)
{
    return 0;;
}
NvU32 NvNandGetBlockSize(NvNandHandle hNand)
{
    return 0;
}
void NvNandFlushCache(NvNandHandle hNand)
 {
 }
NvError NvNandIoctl(NvNandHandle hNand,
                                        NvS32 command,
                                        void* data,
                                        NvS32 arg1,
                                        NvS32 arg2)
{
    return NvError_Success;
}


NvError NvNandSuspend(NvNandHandle hNand)
{
    return NvError_Success;
}

NvError NvNandResume(NvNandHandle hNand)
{
    return NvError_Success;
}

