/*
 * Copyright (c) 2010-2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __SKUINFO__
#define __SKUINFO__

#include "nvbct.h"
#include "nvbit.h"

/**
 * Protect and update if required field in bct which are one time flashable
 *
 * This fields includes: serial-id,sku-id, mac-id, prod-sku
 *
 * @param hBct The bct handle
 * @param hBit The bit handle
 * @param BctPartId The bct partition id
 * @param NvU8* NvinternalBlob buffer
 * @param NvU32 NvInternalBlob buffer size
 *
 * @retval NvSuccess on success
 *         NvError_BadParameter: When incorrect data
 *         NvError_InsufficientMemory
 */

NvError NvProtectBctInvariant (NvBctHandle hBct, NvBitHandle hBit, NvU32 BctPartId,
        NvU8 *NvInternalBlobBuf, NvU32 NvInternalBlobBufferSize);

/**
 * Get the Sku variant of the module
 *
 * @param *SkuId : Contains the sku-id
 *
 * @retval NvSuccess on success
 *         NvError_BadValue when failure
 */
NvError NvBootGetSkuNumber(NvU32 *SkuId);

/**
 * Get the skuInfo in either raw or string format
 *
 * @param pInfo Pointer to skuinfo structure to be populated
 * @param Stringify A flag that tells NvBootGetSkuInfo to stringify data
 *        before copying.
 * @param strlength to create a buffer for cmdline
 *
 * @retval NvSuccess on success
 *         NvError_BadValue when failure
 */

NvError NvBootGetSkuInfo(NvU8 *pInfo, NvU32 Stringify, NvU32 strlength);
#endif
