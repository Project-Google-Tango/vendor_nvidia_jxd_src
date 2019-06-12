/**
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include "nvos.h"
#include "nvapputil.h"
#include "nvflash_util.h"
#include "nvmbrdefs.h"

static NvBool
DioValidateMBR(NvU8 *InputFile)
{
    NvError e = NvSuccess;
    NvOsFileHandle ReadFileHandle = NULL;
    NvU8 *pDataBuffer=NULL;
    size_t BytesRead;
    NvU32 SectorSize = 0;
    NvBool ValidMBR = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
        (const char *)InputFile,
        NVOS_OPEN_READ,
        &ReadFileHandle));

    SectorSize = DIO_MBR_SECTOR_SIZE;
    pDataBuffer = NvOsAlloc(SectorSize);
    if (pDataBuffer == NULL)
    {
        NV_DEBUG_PRINTF(("NvOsAlloc failed in DioValidateMBR\n"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
    /* Skip MBS record and then scan for FLS */
    NV_CHECK_ERROR_CLEANUP(
        NvOsFread(ReadFileHandle,
        pDataBuffer,
        SectorSize,
        &BytesRead));
     ValidMBR = (((*(NvU16 *)((pDataBuffer) + DIO_MBR_SECTOR_SIZE - 2) == 
        DIO_MBR_TRAILSIGH) && 
                  ((*(pDataBuffer) == DIO_MBR_2BYTEJMP) ||
                  (*(pDataBuffer) == DIO_MBR_3BYTEJMP))) ? (NV_TRUE):(NV_FALSE));

fail:
    NvOsFree(pDataBuffer);
    NvOsFclose(ReadFileHandle);

    return(ValidMBR);
}

static void
DioParseFlashLayoutSector(void *FLSector, Dio2ConInfo *DioInfo)
{

    pDioFlashLayoutSector pFLS;
    pDioReservedEntry pEntry;
    pDioFlashRegion pRegion = NULL;
    int NumRegions;

    pFLS = (pDioFlashLayoutSector)(FLSector);
    NumRegions =(pFLS->RegionEntriesSize)/DIO_FLASH_REGION_TABLE_SIZE;

    // ReservedEntry table starts at the end of the PFlashLayoutSector struct.
    pEntry = (pDioReservedEntry)(pFLS+1);
    DioInfo->FLSRegionTableStart += sizeof(DioFlashLayoutSector);

    // FlashRegion table starts after the ReservedEntry table.
    if (NumRegions)
    {
        pRegion = (pDioFlashRegion)((NvU8*)pEntry + pFLS->ReservedEntriesSize);
        DioInfo->FLSRegionTableStart += sizeof(DioFlashRegion) * pFLS->ReservedEntriesSize;
    }
    NvOsMemcpy(DioInfo->RegionInfo, pRegion, (NumRegions * sizeof(DioFlashRegion)));

}

static NvError
DioScanForFlashLayoutSector(NvU8 *InputFile,Dio2ConInfo *pInfo)
{
    NvError e = NvSuccess;
    NvOsFileHandle ReadFileHandle = NULL;
    NvU8 *pDataBuffer=NULL;
    size_t BytesRead;
    NvU32 SectorSize = 0;
    NvBool FLSSigFound = NV_FALSE;

    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
        (const char *)InputFile,
        NVOS_OPEN_READ,
        &ReadFileHandle));
    SectorSize = DIO_MBR_SECTOR_SIZE;
    pDataBuffer = NvOsAlloc(SectorSize);
    if (pDataBuffer == NULL)
    {
        NV_DEBUG_PRINTF(("NvOsAlloc failed in DioScanForFlashLayoutSector\n"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
    /* Skip MBS record and then scan for FLS */
    NV_CHECK_ERROR_CLEANUP(NvOsFread(ReadFileHandle,
        pDataBuffer,
        SectorSize,
        &BytesRead));
    while (!FLSSigFound)
    {
        pInfo->FLSRegionTableStart += BytesRead;
        BytesRead = 0;
        NvOsMemset(pDataBuffer,0, SectorSize);
        /// Seraches till end of file and will exit when reaches the end of file
        e = NvOsFread(ReadFileHandle,
            pDataBuffer,
            SectorSize,
            &BytesRead);
        /// Check if the read is successful or the end of file 
        /// Make sure that the bytes read is greater than the DIO_FLASH_LAYOUT_SIG_SIZE
        if ( ((e == NvSuccess) || (e == NvError_EndOfFile)) &&
            ( BytesRead >= DIO_FLASH_LAYOUT_SIG_SIZE ))
        {
            if (!NvOsMemcmp(pDataBuffer,
                DIO_FLASH_LAYOUT_SIG,
                DIO_FLASH_LAYOUT_SIG_SIZE))
            {
                   /* Falsh layout sector block was found */
                   // display flash layout sector infor */
                   DioParseFlashLayoutSector((void *)pDataBuffer,pInfo);
                   FLSSigFound = NV_TRUE;
            }
        }
        else
        {
            /// Goto fail as the signature is not found or there is a file operation error
            goto fail;
        }
    }
fail:
    NvOsFree(pDataBuffer);
    NvOsFclose(ReadFileHandle);
    return e;
}

NvError PostProcDioOSImage(
    NvU8 *InputFile, NvU8 *OutputFile);
NvError PostProcDioOSImage(
    NvU8 *InputFile, NvU8 *OutputFile)
{
    NvError e = NvSuccess;
    NvOsFileHandle ReadFileHandle = NULL, WriteFileHandle= NULL;
    NvU8 *pDataBuffer = NULL;
    size_t BytesRead = 0;
    pDio2ConInfo pDioInfo = NULL;
    NvU32  BlockSize = 0;
    NvU32  BlocksToCopy  = 0;
    NvBool FirstBlockRead = NV_FALSE;
    pDioFlashRegion OsRegionInfo;
    NvU32 Count = 0;
    NvU32 i = 0;
    NvOsFileHandle hFile = 0;

    NvAuPrintf( "converting dio image: %s\n", InputFile );

    NV_CHECK_ERROR_CLEANUP(
        NvOsFopen( (const char *)InputFile, NVOS_OPEN_READ, &hFile )
    );

    /* Step1: check proper DIO file is inputted or not .*
    *  Open the input file and check  DIO siganture in MSB  */
    if( !DioValidateMBR(InputFile))
    {
        NvAuPrintf("invalid dio image: %s\n", InputFile );
        e = NvError_BadParameter;
        goto fail;
    }

     /* Allocates the memory for Dio2ConInfo */
     pDioInfo = NvOsAlloc(sizeof(Dio2ConInfo));
     if (pDioInfo == NULL)
    {
        NV_DEBUG_PRINTF(("NvOsAlloc failed in NvFlashPostProcDioOSImage\n"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
     NvOsMemset(pDioInfo,0,sizeof(Dio2ConInfo));

    /* Step2 : findout flash Layout sector for getting the block size, sectors per *
    **  block Look for "MSFLSH50" signature after first 512 bytes(MBR)          */
    NV_CHECK_ERROR_CLEANUP(
        DioScanForFlashLayoutSector(InputFile,pDioInfo)
    );

    /* Step3: Copy till emd of Kernel/XIP region */
    /*FLS is found and the required info related to block size, number of sector for block are found *
    ** So first do a block by block copy until end of kernel/XIP  region */
    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
        (const char *)InputFile,
        NVOS_OPEN_READ, 
        &ReadFileHandle));
    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
            (const char *)OutputFile,
            NVOS_OPEN_WRITE | NVOS_OPEN_CREATE,
            &WriteFileHandle));
    BlockSize = pDioInfo->RegionInfo[DioRegionType_Xip].dwBytesPerBlock;
    pDataBuffer = NvOsAlloc(BlockSize);
    if (pDataBuffer == NULL)
    {
        NV_DEBUG_PRINTF(("NvOsAlloc failed while creating data buffer in "
            "NvFlashPostProcDioOSImage\n"));
        e = NvError_InsufficientMemory;
        goto fail;
    }
    BlocksToCopy  = pDioInfo->RegionInfo[DioRegionType_Xip].dwNumLogicalBlocks;
    FirstBlockRead = NV_TRUE;
    while (BlocksToCopy > 0)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsFread(ReadFileHandle,
            pDataBuffer,
            BlockSize,
            &BytesRead));
        if (FirstBlockRead)
        {
            OsRegionInfo = (pDioFlashRegion)(pDataBuffer +
                     (pDioInfo->FLSRegionTableStart + 
                     DIO_FLASH_REGION_TABLE_SIZE *
                     DioRegionType_ReadOnly_FileSys));
            /* Check if the Flash Layout sector has the compaction blocks to be 0(zero)*/
            /* Only then continue to add the required compaction blocks*/
            if (OsRegionInfo->dwCompactBlocks != 0)
            {
                e = NvError_BadValue;
                goto fail;
            }
            else
            {
                OsRegionInfo->dwCompactBlocks = COMPACT_BLOCKS;
                FirstBlockRead = NV_FALSE;
            }
        }
        NV_CHECK_ERROR_CLEANUP(NvOsFwrite(WriteFileHandle,pDataBuffer, BlockSize));
        Count += BlockSize;
        BlocksToCopy --;
    }

    /* Step4: Copy the compaction blocks at the start of OS region */
    /* Now add required compaction blocks with all 1s in destination file */
    NvOsMemset(pDataBuffer,0xFF, BlockSize);
    for (i = 0;i < COMPACT_BLOCKS; i++)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsFwrite(WriteFileHandle,pDataBuffer, BlockSize));
    }

     /* Step5: Copy remaining Regions */
     
    /* Now continue copy with remaing blocks of data */
    BlocksToCopy = pDioInfo->RegionInfo[DioRegionType_ReadOnly_FileSys].dwNumLogicalBlocks +
                            pDioInfo->RegionInfo[DioRegionType_FileSys].dwNumLogicalBlocks;
    while (BlocksToCopy > 0)
    {
        NV_CHECK_ERROR_CLEANUP(NvOsFread(ReadFileHandle,
            pDataBuffer,
            BlockSize,
            &BytesRead));
        NV_CHECK_ERROR_CLEANUP(NvOsFwrite(WriteFileHandle,pDataBuffer, BlockSize));
        BlocksToCopy --;
    }

fail:
    NvOsFree(pDataBuffer);
    NvOsFclose(ReadFileHandle);
    NvOsFclose(WriteFileHandle);
    NvOsFree(pDioInfo);
    return e;
}
