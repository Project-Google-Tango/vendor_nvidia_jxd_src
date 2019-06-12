/**
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvos.h"
#include "nvassert.h"
#include "nvmbrdefs.h"
#include "nvstorebinconverter.h"

// print valid errors 
#define NV_PRINT_ERROR (void) //NvOsDebugPrintf

static SegmentContext **s_SegContextList = NULL;
static NvBool s_bExtendedPartitions = NV_FALSE; 

static NvBool NvSystemPriv_IsStoreBinValid(NvOsFileHandle InputFileHandle)
{
    NvError e = NvSuccess;
    BootBinFormatSignature SignFmt;
    NvBool ValidBootBin = NV_FALSE;
    size_t BytesRead;

    NV_CHECK_ERROR_CLEANUP(NvOsFread(InputFileHandle,
                                     &SignFmt,
                                     sizeof(SignFmt),
                                     &BytesRead));

    ValidBootBin = ((NvOsStrncmp((const char*)SignFmt.Signature,
                                 BOOT_BIN_SIGNATURE_STORE,
                                 BOOT_BIN_SIGNATURE_SIZE ) == 0 )
                                 ? (NV_TRUE):(NV_FALSE));
         
fail:
    return(ValidBootBin);
}
// This routine creates MBR record by filling the partitions
static void NvSystemPriv_CreateMBR(NvU8 *pMBRSector, StoreContext *pStore)
{
    NvU32 counter = 0;
    NvU32 extendedSectors = 0;
    MbrPartition Partitions[DIO_MBR_MAX_PARTITIONS];
    NvU16 MBRTrailSignature = DIO_MBR_TRAILSIGH;
    if(pMBRSector != NULL)
    {
        NvOsMemset(pMBRSector, FILL_DATA, pStore->StoreSectorSize);
        NvOsMemset(Partitions, 0, (DIO_MBR_MAX_PARTITIONS * sizeof(MbrPartition)));
        for( counter=0; counter < DIO_MBR_MAX_PARTITIONS; counter++ )
        {
            if( (s_bExtendedPartitions) && (counter == DIO_MBR_MAX_PARTITIONS - 1))
            {
                Partitions[counter].Part_FileSystem = PART_EXTENDED;
                extendedSectors = s_SegContextList[counter]->Sectors +
                                  s_SegContextList[counter + 1]->Sectors +
                                  ((pStore->StoreTotalSegments - 
                                   DIO_MBR_MAX_PARTITIONS) * DIO_EBR_MAX_PARTITIONS);
                Partitions[counter].Part_TotalSectors = extendedSectors;
                Partitions[counter].Part_StartSector += 
                    ( Partitions[counter - 1].Part_StartSector +
                      Partitions[counter - 1].Part_TotalSectors );
                break;
            }
            Partitions[counter].Part_FileSystem =
                s_SegContextList[counter]->FileSystem;
            Partitions[counter].Part_TotalSectors =
                s_SegContextList[counter]->Sectors;
            if(counter == 0)
            {
                if(pStore->StoreTotalSegments == 1)
                {
                    Partitions[counter].Part_StartSector = MBR_FL_SECTORS;
                    break;
                }
                else
                {
                    Partitions[counter].Part_StartSector =
                        (pStore->BlockSize / pStore->StoreSectorSize);
                }
            }
            else
            {
                Partitions[counter].Part_StartSector +=
                    ( Partitions[counter - 1].Part_StartSector +
                      Partitions[counter - 1].Part_TotalSectors );
            }
        }
        NvOsMemcpy(&pMBRSector[DIO_MBR_SECTOR_SIZE -2 - sizeof(Partitions)],
                   Partitions,
                   sizeof(Partitions));
        NvOsMemcpy(&pMBRSector[DIO_MBR_SECTOR_SIZE -2],
                   &MBRTrailSignature,
                   sizeof(MBRTrailSignature));
        pMBRSector[0] = DIO_MBR_3BYTEJMP;
    }
}
// This routine creates EBR record by filling the partitions. Pass 1 for creating 1st EBR
static void NvSystemPriv_CreateEBR(NvU8 *pMBRSector, StoreContext *pStore, NvU32 EBRIdx)
{
    MbrPartition Partitions[DIO_MBR_MAX_PARTITIONS];
    NvU16 MBRTrailSignature = DIO_MBR_TRAILSIGH;
    if(pMBRSector != NULL)
    {
        NvOsMemset(pMBRSector, 0, pStore->StoreSectorSize);
        NvOsMemset(Partitions, 0, (DIO_MBR_MAX_PARTITIONS * sizeof(MbrPartition)));
        
        if (EBRIdx == 1)
        {
            // Filling first EBR partitions for 1st EBR
            Partitions[0].Part_FileSystem = 
                s_SegContextList[(DIO_MBR_MAX_PARTITIONS - 1)]->FileSystem;
            Partitions[0].Part_TotalSectors = 
                s_SegContextList[(DIO_MBR_MAX_PARTITIONS - 1)]->Sectors;
            Partitions[0].Part_StartSector = 1;
            // Filling second EBR partition
            Partitions[1].Part_FileSystem = PART_EXTENDED;
            Partitions[1].Part_TotalSectors = 
                s_SegContextList[(DIO_MBR_MAX_PARTITIONS)]->Sectors + 1;
            Partitions[1].Part_StartSector = 
                Partitions[0].Part_TotalSectors + 1;
        }
        else
        {
            // Filling first EBR partition for 2nd EBR
            Partitions[0].Part_FileSystem = 
                s_SegContextList[DIO_MBR_MAX_PARTITIONS]->FileSystem;
            Partitions[0].Part_TotalSectors = 
                s_SegContextList[DIO_MBR_MAX_PARTITIONS]->Sectors;
            Partitions[0].Part_StartSector = 1;
        }
        NvOsMemcpy(&pMBRSector[DIO_MBR_SECTOR_SIZE -2 - sizeof(Partitions)],
                   Partitions,
                   sizeof(Partitions));
        NvOsMemcpy(&pMBRSector[DIO_MBR_SECTOR_SIZE -2],
                   &MBRTrailSignature,
                   sizeof(MBRTrailSignature));
     }
}

// This routine creates FL table 
static void NvSystemPriv_CreateFL(
            NvU8 *pFLSector,
            StoreContext *pStore)
{
    pDioFlashLayoutSector pFLS = NULL;
    pDioFlashRegion FlashRegion; 
    NvU8 FLSignature[] = DIO_FLASH_LAYOUT_SIG;
    NV_ASSERT(pFLSector);
    NV_ASSERT(pStore);

    if(pFLSector != NULL)
    {
        NvOsMemset(pFLSector, FILL_DATA, pStore->StoreSectorSize);
        pFLS = (pDioFlashLayoutSector)pFLSector;
        NvOsMemcpy(
                   pFLS,
                   (const char*)FLSignature,
                   NvOsStrlen((const char*)FLSignature));
        pFLS->ReservedEntriesSize = 0;
        
        FlashRegion = (pDioFlashRegion)(pFLS + 1);
    
        if(pStore->StoreTotalSegments == 1)
        {
            //CEBASE
            pFLS->RegionEntriesSize = (DIO_FLASH_REGION_TABLE_SIZE);
            NvOsMemset((pFLS+1), 0, DIO_FLASH_REGION_TABLE_SIZE);
            FlashRegion->regionType = DioRegionType_Xip;
            FlashRegion->dwBytesPerBlock = pStore->BlockSize;
            FlashRegion->dwSectorsPerBlock = 
                (pStore->BlockSize / pStore->StoreSectorSize);
            if(((s_SegContextList[0]->Sectors + MBR_FL_SECTORS)  % 
                FlashRegion->dwSectorsPerBlock) > 0)
            {
                FlashRegion->dwNumLogicalBlocks = 
                    (((s_SegContextList[0]->Sectors + MBR_FL_SECTORS)/ 
                      FlashRegion->dwSectorsPerBlock) + 1);
            }
            else
            {
                FlashRegion->dwNumLogicalBlocks = 
                    ((s_SegContextList[0]->Sectors + MBR_FL_SECTORS)/ 
                     FlashRegion->dwSectorsPerBlock);
            }
        }
        else
        {
            //SMARTFON
            pFLS->RegionEntriesSize = (DIO_FLASH_REGION_TABLE_SIZE*3);
            NvOsMemset((pFLS+1), 0, DIO_FLASH_REGION_TABLE_SIZE*3);
    
            //XIP region
            FlashRegion->regionType = DioRegionType_Xip;
            FlashRegion->dwBytesPerBlock = pStore->BlockSize;
            FlashRegion->dwSectorsPerBlock = (pStore->BlockSize / 
                                              pStore->StoreSectorSize);
            if( ((s_SegContextList[0]->Sectors + 
                s_SegContextList[1]->Sectors) % 
                FlashRegion->dwSectorsPerBlock) > 0 )
            {
                FlashRegion->dwNumLogicalBlocks = 
                    (((s_SegContextList[0]->Sectors + 
                      s_SegContextList[1]->Sectors) / 
                      FlashRegion->dwSectorsPerBlock) + 1);
            }
            else
            {
                FlashRegion->dwNumLogicalBlocks = 
                    ((s_SegContextList[0]->Sectors + 
                      s_SegContextList[1]->Sectors) / 
                      FlashRegion->dwSectorsPerBlock);
            }
    
            FlashRegion->dwNumLogicalBlocks += 1; // For the MBR block
    
            // RO region
            FlashRegion = FlashRegion + 1;
            FlashRegion->regionType = DioRegionType_ReadOnly_FileSys;
            FlashRegion->dwBytesPerBlock = pStore->BlockSize;
            FlashRegion->dwSectorsPerBlock = (pStore->BlockSize / 
                                              pStore->StoreSectorSize);
            FlashRegion->dwCompactBlocks = COMPACT_BLOCKS;
            if( (s_SegContextList[2]->Sectors % 
                 FlashRegion->dwSectorsPerBlock) > 0 )
            {
                FlashRegion->dwNumLogicalBlocks = 
                    ((s_SegContextList[2]->Sectors / 
                      FlashRegion->dwSectorsPerBlock) + 1);
            }
            else
            {
                FlashRegion->dwNumLogicalBlocks = 
                    (s_SegContextList[2]->Sectors / 
                     FlashRegion->dwSectorsPerBlock);
            }
    
            // RW region
            FlashRegion = FlashRegion + 1;
            FlashRegion->regionType = DioRegionType_FileSys;
            FlashRegion->dwBytesPerBlock = pStore->BlockSize;
            FlashRegion->dwSectorsPerBlock = 
                ( pStore->BlockSize /  pStore->StoreSectorSize);
            if( (s_SegContextList[3]->Sectors % 
                 FlashRegion->dwSectorsPerBlock) > 0 )
            {
                FlashRegion->dwNumLogicalBlocks = 
                    ((s_SegContextList[3]->Sectors / 
                      FlashRegion->dwSectorsPerBlock) + 1);
            }
            else
            {
                FlashRegion->dwNumLogicalBlocks = 
                    (s_SegContextList[3]->Sectors / 
                     FlashRegion->dwSectorsPerBlock);
            }
        }
    }
}
// This routine creates and writes MBR     
static NvError NvSystemPriv_WriteMBR(
               NvOsFileHandle DiskWriteFileHandle, 
               StoreContext *pStore)
{
    NvError e = NvSuccess;
    NvU8 *pMBRSector = NULL;
    NV_ASSERT(pStore);
    if( pStore )
    {
        pMBRSector = NvOsAlloc(pStore->StoreSectorSize);
    }
    if (pMBRSector == NULL)
    {
        NV_PRINT_ERROR("NvOsAlloc failed in WriteMBR \n");
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvSystemPriv_CreateMBR(pMBRSector, pStore);
    /* Write generated MBR to file */
    NV_CHECK_ERROR_CLEANUP(NvOsFwrite(
        DiskWriteFileHandle,
        pMBRSector,
        pStore->StoreSectorSize));
    fail:
    if (pMBRSector)
    {
        NvOsFree(pMBRSector);
        pMBRSector = NULL;
    }
    return e;
}
// This routine creates and writes EBR  
static NvError NvSystemPriv_WriteEBR(
               NvOsFileHandle DiskWriteFileHandle, 
               StoreContext *pStore, NvU32 EBRIdx)
{
    NvError e = NvSuccess;
    NvU8 *pMBRSector = NULL;
    NV_ASSERT(pStore);
    if( pStore )
    {
        pMBRSector = NvOsAlloc(pStore->StoreSectorSize);
    }
    if (pMBRSector == NULL)
    {
        NV_PRINT_ERROR("NvOsAlloc failed in WriteEBR \n");
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvSystemPriv_CreateEBR(pMBRSector, pStore, EBRIdx);
    /* Write generated MBR to file */
    NV_CHECK_ERROR_CLEANUP(NvOsFwrite(
        DiskWriteFileHandle,
        pMBRSector,
        pStore->StoreSectorSize));
    fail:
    if (pMBRSector)
    {
        NvOsFree(pMBRSector);
        pMBRSector = NULL;
    }
    return e;
}
// This routine creates and writes Flash Layout     
static NvError NvSystemPriv_WriteFL( 
               NvOsFileHandle DiskWriteFileHandle, 
               StoreContext *pStore)
{
    NvError e = NvSuccess;
    NvU8 *pFLSector = NULL;
    NV_ASSERT(pStore);
    if(pFLSector == NULL)
    {
        pFLSector = NvOsAlloc(pStore->StoreSectorSize);
    }
    if (pFLSector == NULL)
    {
        NV_PRINT_ERROR("NvOsAlloc failed in WriteFL \n");
        e = NvError_InsufficientMemory;
        goto fail;
    }
    NvSystemPriv_CreateFL(pFLSector, pStore);
    /* Write generated FL to file */
    NV_CHECK_ERROR_CLEANUP(NvOsFwrite(
        DiskWriteFileHandle,
        pFLSector,
        pStore->StoreSectorSize));
    fail:
    if (pFLSector)
    {
        NvOsFree(pFLSector);
        pFLSector = NULL;
    }
    return e;
}
// This routine pads empty sectors with zeroes     
static NvError NvSystemPriv_FillEmptySectors(
               NvOsFileHandle DiskWriteFileHandle,
               NvU32 SkippedSectors,
               StoreContext *pStore)
{
    NvError e = NvSuccess;
    NvU8 *pFillBuffer = NULL;
    NvU32 count = 0;
    NV_ASSERT(pStore);
    if((pFillBuffer == NULL) && (pStore != NULL))
    {
        pFillBuffer = NvOsAlloc(pStore->StoreSectorSize);
        if (pFillBuffer == NULL)
        {
            NV_PRINT_ERROR("NvOsAlloc failed in FillEmptySectors \n");
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NvOsMemset(pFillBuffer, FILL_DATA, pStore->StoreSectorSize);
        for (count = 0; count < SkippedSectors; count++)
        {
            NV_CHECK_ERROR_CLEANUP(NvOsFwrite(
                                   DiskWriteFileHandle,
                                   pFillBuffer,
                                   pStore->StoreSectorSize));
        }
    }
    
    fail:
    if (pFillBuffer)
    {
        NvOsFree(pFillBuffer);
        pFillBuffer = NULL;
    }
    return e;
}
// This routine caches the segment context info and writes MBR and FL
static NvError NvSystemPriv_FillSegmentList(
               NvOsFileHandle StoreBinSrcHandle,
               NvOsFileHandle DiskWriteFileHandle,
               StoreContext *pStore)
{
    NvError e = NvSuccess;
    size_t BytesRead = 0;
    NvU32 loop = 0;
    BootBinFormatStoreSegmentHeader SegmentHdr;
    BootBinFormatStoreSegmentPartitionInfo SegmentPartInfo;
    NvU32 FixAlignment = 0;
    NV_ASSERT(pStore);
    if( pStore )
    {
        for(loop = 0; loop < pStore->StoreTotalSegments; loop++)
        {
            NV_CHECK_ERROR_CLEANUP(NvOsFread(
                                   StoreBinSrcHandle,
                                   &SegmentHdr,
                                   sizeof(SegmentHdr),
                                   &BytesRead));
            NvOsDebugPrintf("BootBinFormatStoreSegmentHeader:\n\
                Segment type is %x\r\n\
                Sectors is %x\r\n\
                infoSize is %x\r\n",\
                SegmentHdr.Type,
                SegmentHdr.Sectors,
                SegmentHdr.InfoSize);
            
            if(SegmentHdr.Type == BootBinFormatStoreSegment_Partition)
            {
                NV_CHECK_ERROR_CLEANUP(NvOsFread(
                    StoreBinSrcHandle,
                    &SegmentPartInfo,
                    sizeof(SegmentPartInfo),
                    &BytesRead));
                NvOsDebugPrintf("BootBinFormatStoreSegmentPartitionInfo:\n\
                FileSystem is %x\r\n\
                Index is %x\r\n",\
                SegmentPartInfo.FileSystem,
                SegmentPartInfo.Index);
            }
            else
            {
                NV_PRINT_ERROR("Not supported filesystem type \n");
                e = NvError_BadValue;
                goto fail;
            }
            s_SegContextList = (SegmentContext **)NvOsRealloc(
                s_SegContextList,
                ((loop + 1) * sizeof(SegmentContext *)));
            s_SegContextList[loop] = 
                (SegmentContext *)NvOsAlloc(sizeof(SegmentContext));
            if (s_SegContextList[loop] == NULL)
            {
                NV_PRINT_ERROR("NvOsAlloc failed in SegList creation \n");
                e = NvError_InsufficientMemory;
                goto fail;
            }
        
            s_SegContextList[loop]->SegmentIdx = loop;
            s_SegContextList[loop]->Sectors = SegmentHdr.Sectors;
            s_SegContextList[loop]->FileSystem = SegmentPartInfo.FileSystem;
        }

        NV_CHECK_ERROR_CLEANUP(NvSystemPriv_WriteMBR(
                               DiskWriteFileHandle, 
                               pStore));
        NV_CHECK_ERROR_CLEANUP(NvSystemPriv_WriteFL(
                               DiskWriteFileHandle, 
                               pStore));

        if(pStore->StoreTotalSegments > 1)
        {
            FixAlignment = (((pStore->BlockSize / 
                              pStore->StoreSectorSize ) - 2));
            NvSystemPriv_FillEmptySectors(DiskWriteFileHandle, 
                                          FixAlignment, 
                                          pStore);
        }
    }
fail:
    return e;
}
// This routine writes segment data     
static NvError NvSystemPriv_WriteSegmentRecords(
               NvOsFileHandle StoreBinSrcHandle,
               NvOsFileHandle DiskWriteFileHandle,
               StoreContext *pStore)
{
    NvError e = NvSuccess;
    BootBinFormatStoreRecordHeader RecordHdr;
    size_t BytesRead = 0;
    NvU32 SkippedSectors = 0;
    NvU8 *pDataBuffer = NULL;
    NvU32 SectorCounter = 0, RecordSize = 0;
    NvBool IsCompactDone = NV_FALSE, IsEBR1WriteDone = NV_FALSE, IsEBR2WriteDone = NV_FALSE;
    NvU32 PrevSegmentIdx = 0, PrevSegmentSector = 0, PrevSegmentSectors = 0;
 
    NV_ASSERT(pStore);
    while (NvOsFread(StoreBinSrcHandle, 
                     &RecordHdr,
                     sizeof(RecordHdr),
                     &BytesRead) == NvSuccess)
    { 
        NvOsDebugPrintf("BootBinFormatStoreRecordHeader:\n\
            Sector is %d\r\n\
            Sectors is %d\r\n\
            Segment is %d\r\n",\
            RecordHdr.Sector,
            RecordHdr.Sectors,
            RecordHdr.Segment);
       
        if( ( s_SegContextList[RecordHdr.Segment]->FileSystem == PART_IMGFS) && (!IsCompactDone) )
        {
            SkippedSectors = (( pStore->BlockSize / 
                                pStore->StoreSectorSize) * COMPACT_BLOCKS);
            NvSystemPriv_FillEmptySectors(DiskWriteFileHandle, 
                                          SkippedSectors, 
                                          pStore);
            IsCompactDone = NV_TRUE;
        }
        
        if(PrevSegmentIdx != RecordHdr.Segment)
        {
            SkippedSectors = ((s_SegContextList[PrevSegmentIdx]->Sectors) - 
                                (PrevSegmentSector + PrevSegmentSectors));
            NvSystemPriv_FillEmptySectors(DiskWriteFileHandle, 
                                          SkippedSectors, 
                                          pStore);
            SectorCounter = 0;
        }

        if( ( s_SegContextList[RecordHdr.Segment]->FileSystem == PART_IMGFS)
             && (s_bExtendedPartitions) && (!IsEBR1WriteDone) )
        {
            //write First EBR here at beginning of OS partition
            NV_CHECK_ERROR_CLEANUP(NvSystemPriv_WriteEBR(
                               DiskWriteFileHandle, 
                               pStore, 1));
            IsEBR1WriteDone = NV_TRUE;
         
        }
        if( ( s_SegContextList[RecordHdr.Segment]->FileSystem == PART_DOS3_FAT)
             && (s_bExtendedPartitions) && (!IsEBR2WriteDone) )
        {
            //write Second EBR here at beginning of USER partition
            NV_CHECK_ERROR_CLEANUP(NvSystemPriv_WriteEBR(
                               DiskWriteFileHandle, 
                               pStore, 2));
            IsEBR2WriteDone = NV_TRUE;
        }
       
        PrevSegmentIdx = RecordHdr.Segment;
        PrevSegmentSector = RecordHdr.Sector;
        PrevSegmentSectors = RecordHdr.Sectors;
        RecordSize = RecordHdr.Sectors * pStore->StoreSectorSize;
        pDataBuffer = NvOsAlloc(RecordSize);
        if (pDataBuffer == NULL)
        {
            NV_PRINT_ERROR("NvOsAlloc failed in DiskCreateImage \n");
            e = NvError_InsufficientMemory;
            goto fail;
        }
        NV_CHECK_ERROR_CLEANUP(NvOsFread(
                StoreBinSrcHandle,
                pDataBuffer,
                RecordSize,
                &BytesRead));
        if(SectorCounter != RecordHdr.Sector)
        {
            SkippedSectors = (RecordHdr.Sector - SectorCounter);
            NvSystemPriv_FillEmptySectors(DiskWriteFileHandle, 
                                          SkippedSectors, 
                                          pStore);
            SectorCounter += (RecordHdr.Sector - SectorCounter);
        }
        NV_CHECK_ERROR_CLEANUP(NvOsFwrite(
            DiskWriteFileHandle,
            pDataBuffer,
            RecordSize));
        SectorCounter += RecordHdr.Sectors;
               
        if (pDataBuffer)
        {
            NvOsFree(pDataBuffer);
            pDataBuffer = NULL;
        }
        NV_CHECK_ERROR_CLEANUP(NvOsFseek(
        StoreBinSrcHandle,
        pStore->StoreHashSize,
        NvOsSeek_Cur));
      
    }
fail:
    if (pDataBuffer)
    {
        NvOsFree(pDataBuffer);
        pDataBuffer = NULL;
    }
    return e;
}
    
    
NvError NvConvertStoreBin(NvU8 *InputFile, NvU32 BlockSize, NvU8 *OutputFile);
NvError NvConvertStoreBin(NvU8 *InputFile, NvU32 BlockSize, NvU8 *OutputFile)
{
    NvError e = NvSuccess;
    NvOsFileHandle StoreBinSrcHandle = NULL, DiskWriteFileHandle= NULL;
    StoreContext *pStore = NULL;
    NvU32 i = 0;
    size_t BytesRead = 0;
    BootBinFormatStoreHeader StoreHdr;

    // Open the input file and check for store BIN siganture
    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
        (const char *)InputFile,
        NVOS_OPEN_READ,
        &StoreBinSrcHandle));
    
    if (!NvSystemPriv_IsStoreBinValid(StoreBinSrcHandle))
    {
        NV_PRINT_ERROR("InValid Image \n");
        e = NvError_BadParameter;
        goto fail;
    }
    /* Block size cannot be 0 */
    if (BlockSize <= 0)
    {
        NV_PRINT_ERROR("InValid Block size \n");
        e = NvError_BadParameter;
        goto fail;
    }
    
    /* Get store header info */
    NV_CHECK_ERROR_CLEANUP(NvOsFread(
        StoreBinSrcHandle,
        &StoreHdr,
        sizeof(StoreHdr),
        &BytesRead));
    if(pStore == NULL)
    {
        pStore = NvOsAlloc(sizeof(StoreContext));
    }
    if (pStore == NULL)
    {
        NV_PRINT_ERROR("NvOsAlloc failed for StoreContext \n");
        e = NvError_InsufficientMemory;
        goto fail;
    }
    else
    {
        pStore->StoreSectorSize = StoreHdr.SectorSize;
        pStore->StoreTotalSectors = StoreHdr.Sectors;
        pStore->StoreTotalSegments = StoreHdr.Segments;
        pStore->StoreHashSize = StoreHdr.HashSize;
        pStore->BlockSize = BlockSize;
    }
    
    NV_CHECK_ERROR_CLEANUP(NvOsFopen(
        (const char *)OutputFile,
        NVOS_OPEN_WRITE | NVOS_OPEN_CREATE,
        &DiskWriteFileHandle));
    // check for extended partition
    if( pStore->StoreTotalSegments > DIO_MBR_MAX_PARTITIONS)
    {
        s_bExtendedPartitions = NV_TRUE;
    }
    // Fetch and Fill the segment info
    NV_CHECK_ERROR_CLEANUP(NvSystemPriv_FillSegmentList(StoreBinSrcHandle,
                                                        DiskWriteFileHandle,
                                                        pStore));
    
    // seek to hash size to get records 
    NV_CHECK_ERROR_CLEANUP(NvOsFseek(
        StoreBinSrcHandle,
        pStore->StoreHashSize,
        NvOsSeek_Cur));
    
    NV_CHECK_ERROR_CLEANUP(NvSystemPriv_WriteSegmentRecords(StoreBinSrcHandle, 
                                                            DiskWriteFileHandle, 
                                                            pStore));
    NvOsDebugPrintf("Disk Image Successfully Created! \n");
fail:   
    if (StoreBinSrcHandle)
    {
        NvOsFclose(StoreBinSrcHandle);
    }
    if (DiskWriteFileHandle)
    {
        NvOsFclose(DiskWriteFileHandle);
    }
    
    if ((s_SegContextList) && (pStore))
    {
        for(i = 0; i < pStore->StoreTotalSegments; i++)
        {
            NvOsFree(s_SegContextList[i]);
            i++;
        }
        NvOsFree(s_SegContextList);
    }
   
    if (pStore)
    {
        NvOsFree(pStore);
    }
    return e;

}
