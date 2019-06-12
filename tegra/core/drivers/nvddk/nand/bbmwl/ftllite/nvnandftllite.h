/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */
/**
 * @file nvnandftlfull_defs.h
 *
 * Public definitions for FTL lite driver interface.
 *
 */

 #ifndef NV__NAND_FTL_LITE_H_
#define NV__NAND_FTL_LITE_H_
#include "../nvnandregion.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
Assumptions made for FTL-lite:
1.  When a partition is created FTL-lite will get open call along with details of the partition,
such as start/end Logical blocks, start/end physical blocks, number of spare blocks,
partition type, and management policy.
2.  Partition information has to be passed along with the read, write and close operations.
3.  After a partition is opened, FTL-lite will get read/write operations for data in 
sequential order starting from offset zero.
4.  Once the partition is closed, FTL-lite will get a call and after that,
all calls to those LBA's in that partition will be invalid.
5.  FTL-lite doesn't support multi files in a partition.
If multiple files should be stored in a single partition then the partition 
should be managed by full FTL.
6.  If power goes off before closing a partition, then data inside that partition is not valid.
*/

typedef struct NandFtlLiteRec* FtlLitePrivHandle;

/**
 * FTL handle that holds region interface and its private data
 */
typedef struct NvNandFtlLiteRec
{
    ///interface pointers to nand region
    // This must be the first member as the free call
    // uses this member to free complete data type instance
    NvnandRegion NandRegion;
    ///ftl private data
    FtlLitePrivHandle hPrivFtl;
}NvNandFtlLite,*NvNandFtlLiteHandle;


//Look "nvnandregion.h" for comments 

NvError
    NvNandFtlLiteOpen(
    FtlLitePrivHandle*  phNandFtlLite,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion);

void NvNandFtlLiteClose(NvNandRegionHandle hNand);

NvError
NvNandFtlLiteReadSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    void* const pBuffer,
    NvU32 NumberOfSectors);

NvError
NvNandFtlLiteWriteSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    const void* pBuffer,
    NvU32 NumberOfSectors);

void 
NvNandFtlLiteGetInfo(
    NvNandRegionHandle hRegion,
    NvNandRegionInfo* pNandRegionInfo);

void NvNandFtlLiteFlush(NvNandRegionHandle hRegion);

NvError 
NvNandFtlLiteIoctl(
    NvNandRegionHandle hRegion, 
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif//NV__NAND_FTL_LITE_H_
