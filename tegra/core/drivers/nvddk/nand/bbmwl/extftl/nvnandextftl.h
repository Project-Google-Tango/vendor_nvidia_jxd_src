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
 * @file nvnandextftl.h
 *
 * Public definitions for external FTL driver interface.
 *
 */

 #ifndef NV__NAND_EXT_FTL_H_
#define NV__NAND_EXT_FTL_H_
#include "../nvnandregion.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/*
Assumptions made for external FTL:
1.  When a partition is created FTL-lite will get open call along with details of the partition,
such as start/end Logical blocks, start/end physical blocks, number of spare blocks,
partition type, and management policy.
2.  Partition information has to be passed along with the read, write and close operations.
3.  After a partition is opened, external FTL - will get read/write operations for data in 
sequential order starting from offset zero.
4. Only operations defined are  - erase, read/write. The read and write operations are 
    performed if the block is not a factory bad block.  
*/

typedef struct NandExtFtlRec* ExtFtlPrivHandle;

/**
 * FTL handle that holds region interface and its private data
 */
typedef struct NvNandExtFtlRec
{
    ///interface pointers to nand region
    // This must be the first member as the free call
    // uses this member to free complete data type instance
    NvnandRegion NandRegion;
    ///ftl private data
    ExtFtlPrivHandle hPrivFtl;
}NvNandExtFtl,*NvNandExtFtlHandle;


//Look "nvnandregion.h" for comments 

NvError
    NvNandExtFtlOpen(
    ExtFtlPrivHandle*  phNandExtFtl,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion);

void NvNandExtFtlClose(NvNandRegionHandle hNand);

NvError
NvNandExtFtlReadSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    void* const pBuffer,
    NvU32 NumberOfSectors);

NvError
NvNandExtFtlWriteSector(
    NvNandRegionHandle hRegion,
    NvU32 SectorNumber,
    const void* pBuffer,
    NvU32 NumberOfSectors);

void 
NvNandExtFtlGetInfo(
    NvNandRegionHandle hRegion,
    NvNandRegionInfo* pNandRegionInfo);

void NvNandExtFtlFlush(NvNandRegionHandle hRegion);

NvError 
NvNandExtFtlIoctl(
    NvNandRegionHandle hRegion, 
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif//NV__NAND_EXT_FTL_H_

