/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 * 
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVSTOREBINCONVERTER_H
#define INCLUDED_NVSTOREBINCONVERTER_H

#include "nvos.h"

#if defined(__cplusplus)
extern "C"
{
#endif

//------------------------------ubl store bin defines------------------------------------------------
/* Signature for the RAM*/
#define BOOT_BIN_SIGNATURE_RAM              "B000FF"

/* Signature for the STORE*/
#define BOOT_BIN_SIGNATURE_STORE            "D000FF\x0A"

/* Segment name*/
#define BOOT_BIN_SEGMENT_NAME               8

/* Boot BIN signature size*/
#define BOOT_BIN_SIGNATURE_SIZE             7

//---------------------------private defines-----------------------------------------------------------
/* MBR sector + Flash Layout sector*/
#define MBR_FL_SECTORS             2
//------------------------------Private store bin format enums-----------------------------------------
/*
 * BootBinFormatFlags: Enumerated list of UBL store bin format flags.
 */
typedef enum 
{
    BootBinFormatFlag_None = 0,     /* Illegal value  */
    BootBinFormatFlag_Clean = (1 << 0),    /* clean  */
    
    /* The following two definitions must be last. */
    BootBinFormatFlag_Max, /* Must appear after the last legal item */
    BootBinFormatFlag_Force32 = 0x7fffffff
} BootBinFormatFlags;

/*
 * BootBinFormatHashType: Enumerated list of hash algorithms used.
 */
typedef enum 
{
    BOOTBinFormatHash_Sum = 0, /* sum    */
    BOOTBinFormatHash_SHA1,    /* SHA1   */

    /* The following two definitions must be last. */
    BootBinFormatHash_Max, /* Must appear after the last legal item */
    BootBinFormatHash_Force32 = 0x7fffffff
} BootBinFormatHashType;

/*
 * BootBinFormatStoreSegmentType: Enumerated list of store segment types.
 */
typedef enum 
{
    BootBinFormatStoreSegment_None = 0,     /* Illegal value  */
    BootBinFormatStoreSegment_Binary,       /* Binary type    */
    BootBinFormatStoreSegment_Reserved,     /* Reserved type  */
    BootBinFormatStoreSegment_Partition,    /* Partition type */

    /* The following two definitions must be last. */
    BootBinFormatStoreSegment_Max, /* Must appear after the last legal item */
    BootBinFormatStoreSegment_Force32 = 0x7fffffff
} BootBinFormatStoreSegmentType;

//------------------------------Private store bin format structures-------------------------------------------
/*
 * BootBinFormatSignature: This is the structure used to check BIN format signature.
 */
typedef struct BootBinFormatSignatureRec 
{
    NvU8 Signature[BOOT_BIN_SIGNATURE_SIZE];  /* Signature is 7 bytes */
} BootBinFormatSignature;

/*
 * NvBootBinFormatStoreHeader: This is the store format header structure.
 */
typedef struct BootBinFormatStoreHeaderRec 
{
    NvU32 Flags;                   /* flags */
    NvU32 SectorSize;              /* Sector size in bytes */
    NvU32 Sectors;                 /* Total sectors */
    NvU32 Segments;                /* Total Segments */ 
    NvU32 HashType;                /* Type of Hash used */ 
    NvU32 HashSize;                /* Size of Hash */
    NvU32 SeedSize;                /* Size of seed */ 
} BootBinFormatStoreHeader;    

/*
 * BootBinFormatStoreSegmentHeader: This is the store format segment
 * header structure.
 */
typedef struct BootBinFormatStoreSegmentHeaderRec 
{
    NvU32 Type;       /* Segemnt type */
    NvU32 Sectors;    /* Total sectors in segment */
    NvU32 InfoSize;   /* Size of the info */

} BootBinFormatStoreSegmentHeader;

/*
 * BootBinFormatStoreSegmentBinaryInfo: This is the store format segment
 * Binary Info structure.
 */
typedef struct BootBinFormatStoreSegmentBinaryInfoRec 
{
    NvU8 Index;       /* Segment Index */
} BootBinFormatStoreSegmentBinaryInfo;

/*
 * BootBinFormatStoreSegmentReservedInfo: This is the store format segment
 * Reserved Info structure.
 */
typedef struct BootBinFormatStoreSegmentReservedInfoRec 
{
    NvU8 Name[BOOT_BIN_SEGMENT_NAME];    /* Segment name */
} BootBinFormatStoreSegmentReservedInfo;

/*
 * BootBinFormatStoreSegmentPartitionInfo: This is the store format segment
 * Partition Info structure.
 */
typedef struct BootBinFormatStoreSegmentPartitionInfoRec
{
    NvU8 FileSystem;   /* File System type */
    NvU8 Index;        /* Segment index */
} BootBinFormatStoreSegmentPartitionInfo;

/*
 * BootBinFormatStoreRecordHeader: This is the store format Record
 * header Info structure.
 */
typedef struct BootBinFormatStoreRecordHeaderRec 
{
    NvU32 Segment;   /* Segment index of the record */
    NvU32 Sector;    /* Sector */
    NvU32 Sectors;   /* Total sectors */
} BootBinFormatStoreRecordHeader;

typedef struct SegmentContextRec
{
    NvU32 SegmentIdx;
    NvU32 Sectors;
    NvU32 FileSystem;
}SegmentContext;

typedef struct StoreContextRec
{
    NvU32 StoreSectorSize;
    NvU32 StoreTotalSectors;
    NvU32 StoreTotalSegments;
    NvU32 StoreHashSize;
    NvU32 BlockSize;
}StoreContext;

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVSTOREBINCONVERTER_H

