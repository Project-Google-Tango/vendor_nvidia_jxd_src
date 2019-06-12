
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
 * @file
 * @brief   Hardware NAND Flash Driver Interface.
 *
 * @b Description:This provides interface for block device drivers which 
 *                      requires complete wear levling and bad block management.
 */

#ifndef INCLUDED_NV__NAND_FTL_FULL_H_
#define INCLUDED_NV__NAND_FTL_FULL_H_

#include "nandconfiguration.h"
#include "nvarguments.h"
#include "nvnandftlfull_defs.h"
#include "../nvnandregion.h"

#if defined(__cplusplus)
extern "C"
{
#endif

typedef struct NvNandRec* FtlFullPrivHandle;

/**
 * FTL handle that holds region interface and its private data
 */
typedef struct NvNandFtlFullRec
{
    ///interface pointers to nand region
    // This must be the first member as the free call
    // uses this member to free complete data type instance
    NvnandRegion NandRegion;
    ///ftl private data
    FtlFullPrivHandle hPrivFtl;
}NvNandFtlFull,*NvNandFtlFullHandle;


NvError
NvNandFtlFullOpen(
    FtlFullPrivHandle*  hNandFtlFull,
    NvDdkNandHandle hNvDdkNand,
    NvDdkNandFunctionsPtrs *pFuncPtrs,
    NandRegionProperties* nandregion);

void NvNandFtlFullClose(NvNandRegionHandle hNand);

NvError 
NvNandFtlFullReadSector(
    NvNandRegionHandle hNand,
    NvU32 lba,
    void* const pBuffer,
    NvU32 NumberOfSectors);

NvError
NvNandFtlFullWriteSector(
    NvNandRegionHandle hNand,
    NvU32 lba,
    const void* pBuffer,
    NvU32 NumberOfSectors);

void 
NvNandFtlFullGetInfo(
    NvNandRegionHandle hNand,
    NvNandRegionInfo* pNandRegionInfo);

void NvNandFtlFullFlush(NvNandRegionHandle hNand);

NvError 
NvNandFtlFullIoctl(
    NvNandRegionHandle hNand, 
    NvU32 Opcode,
    NvU32 InputSize,
    NvU32 OutputSize,
    const void *InputArgs,
    void *OutputArgs);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */

#endif /* INCLUDED_NV__NAND_FTL_FULL_H_ */
