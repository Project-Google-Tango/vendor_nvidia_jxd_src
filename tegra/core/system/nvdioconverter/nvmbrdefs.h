/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVMBRDEFS_H
#define INCLUDED_NVMBRDEFS_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif
//------------------------------dio mbr defines------------------------------------------------
#define BYTES_PER_BLOCK 131072

/* Flash layout signature  */
#define DIO_FLASH_LAYOUT_SIG                "MSFLSH50"

/* Master Boot Record(MBR) Sector size   */
#define DIO_MBR_SECTOR_SIZE 512
#define DIO_MBR_2BYTEJMP 0xEB
#define DIO_MBR_3BYTEJMP 0xE9

/* Signature for the MBR*/
#define DIO_MBR_TRAILSIGH 0xAA55

/* Flash layout sector signature size */
#define  DIO_FLASH_LAYOUT_SIG_SIZE 8

/* Reserved region name length   */
#define DIO_RESERVED_NAME_LEN 8

/* Each region table size in FLS   */
#define DIO_FLASH_REGION_TABLE_SIZE 28

/* Maximun number of partitions in MBR*/
#define DIO_MBR_MAX_PARTITIONS 4

/* Number of EBR partitions */
#define DIO_EBR_MAX_PARTITIONS 2
//----------------------------filesystem defines----------------------------------------------
/* Unknown filesystem */
#define PART_UNKNOWN            0

/* legit DOS partition */
#define PART_DOS2_FAT           0x01    

/* legit DOS partition */
#define PART_DOS3_FAT           0x04    

/* legit DOS partition */
#define PART_EXTENDED           0x05    

/* legit DOS partition */
#define PART_DOS4_FAT           0x06    

/* legit DOS partition (FAT32) */
#define PART_DOS32              0x0B    

/* CE only partition types for Part_FileSystem */
#define PART_CE_HIDDEN          0x18
#define PART_BOOTSECTION        0x20

/* BINFS file system */
#define PART_BINFS              0x21    

/* XIP ROM Image */
#define PART_XIP                0x22    

/* XIP ROM Image (same as PART_XIP) */
#define PART_ROMIMAGE           0x22    

/* XIP RAM Image */
#define PART_RAMIMAGE           0x23    

/* IMGFS file system */
#define PART_IMGFS              0x25    

/* Raw Binary Data */
#define PART_BINARY             0x26    

/* Fill data */
#define FILL_DATA               0xFF    

/* No. of blocks reserved for compaction by the FAL. */
#define COMPACT_BLOCKS          5

//------------------------------Private DIO format enums-------------------------------------------
/** @brief Defines  type of regions supported by dio format.*/
typedef enum
{
    // This represents XIP region
    DioRegionType_Xip,

    // This represents READONLY_FILESYS region
    DioRegionType_ReadOnly_FileSys,

    // This represent FILESYS region
    DioRegionType_FileSys,

    // maximum regions supported by dio format
    DioRegionType_MaxRegions,
    
    DioRegionType_Force32 = 0x7FFFFFFF
} DioRegionType;

typedef struct  DioFlashLayoutSectorRec
{
    NvU8  FLSSignature[DIO_FLASH_LAYOUT_SIG_SIZE]; /* Signiture identifying sector */         
    NvU32 ReservedEntriesSize;        /* Size in bytes of reserved entries array */
    NvU32 RegionEntriesSize;          /* Size in bytes of region array */
} DioFlashLayoutSector, *pDioFlashLayoutSector;

typedef struct DioReservedEntryRec {
    NvU8  szName[DIO_RESERVED_NAME_LEN]; /* Name of the reserved region. */
    NvU32 dwStartBlock;                  /* Starting physical block of the region. */
    NvU32 dwNumBlocks;                   /* Number of blocks in the region. */
    
} DioReservedEntry, *pDioReservedEntry;

typedef struct  DioFlashRegionRec
{
    DioRegionType   regionType;   /* Type of region (XIP or READONLY_FILESYS or FILESYS). */
    NvU32       dwStartPhysBlock; /* Start phy block of the rgn. Set to zero (0) if not used. */
    NvU32       dwNumPhysBlocks;  /* No. of phy blocks in the rgn. Set to zero (0) if not used. */
    NvU32       dwNumLogicalBlocks; /* No. of logical blocks in the rgn.
                                       Set to zero (0) if not used. 
                                       Set to -1 to indicate that the
                                       region extends to the end of flash memory. */
    NvU32       dwSectorsPerBlock; /* No. of sectors in a flash block. */    
    NvU32       dwBytesPerBlock; /* No. of bytes in a flash block. */
    NvU32       dwCompactBlocks; /* No of blocks used for bad block management and wear leveling */
}DioFlashRegion,*pDioFlashRegion;

typedef struct MbrPartitionRec {
    NvU8        Part_BootInd;     /* If 80h means this is boot partition */         
    NvU8        Part_FirstHead;   /* Partition starting head based 0 */      
    NvU8        Part_FirstSector; /* Partition starting sector based 1 */      
    NvU8        Part_FirstTrack;  /* Partition starting track based 0 */      
    NvU8        Part_FileSystem;  /* Partition type signature field */          
    NvU8        Part_LastHead;    /* Partition ending head based 0 */      
    NvU8        Part_LastSector;  /* Partition ending sector based 1 */      
    NvU8        Part_LastTrack;   /* Partition ending track based 0 */      
    NvU32       Part_StartSector; /* Logical starting sector based 0 */      
    NvU32       Part_TotalSectors;/* Total logical sectors in partition */      
} MbrPartition, *pMbrPartition;

typedef struct  Dio2ConInfoRec
{
    DioFlashRegion    RegionInfo[DioRegionType_MaxRegions]; /* Array of the Rgns in DIO */
    NvU32             FLSRegionTableStart; /* /// Start for each region */
}Dio2ConInfo,*pDio2ConInfo;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVMBRDEFS_H

