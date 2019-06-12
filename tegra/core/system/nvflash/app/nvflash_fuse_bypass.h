/*
 * Copyright (c) 2013, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVFLASH_FUSE_BYPASS_H
#define INCLUDED_NVFLASH_FUSE_BYPASS_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * Stores the fuse information on specified partition
 *
 * @param ParitionId    Id of the partition where fuse info is to be stored
 * @parma PartitionSize Size of the partition
 *
 * @returns             NV_TRUE if success else NV_FALSE
 */
NvBool NvFlashSendFuseBypassInfo(NvU32 ParitionId, NvU64 PartitionSize);

/* Parses the fuse bypass config file and select the appropriate sku
 * based on speedo/iddq values.
 *
 * @return NvSuccess if successful else NvError
 */
NvError NvFlashParseFusebypass(void);

/**
 * Returns the sku name of specified sku id as per fuse bypass config file
 *
 * @param SkuId Id of the sku
 *
 * @return      Sku name
 */
char* NvFlashGetSkuName(NvU32 SkuId);

/**
 * Returns the sku id of specified sku name as per fuse bypass config file
 *
 * @param pSkuName Name of the sku
 *
 * @return         Sku Id
 */
NvU32 NvFlashGetSkuId(const char *pSkuName);

/**
 * Writes the fuse offsets and values at the start of warmboot
 *
 * @param pFuseWriteStart Start address
 */
void NvFlashWriteFusesToWarmBoot(NvU32 *pFuseWriteStart);

#if defined(__cplusplus)
}
#endif

#endif //INCLUDED_NVFLASH_FUSE_BYPASS_H

