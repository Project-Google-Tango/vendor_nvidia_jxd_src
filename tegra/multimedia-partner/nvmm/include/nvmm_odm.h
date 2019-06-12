/*
 * Copyright (c) 2007 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVMM_ODMBLOCK_H__H
#define __NVMM_ODMBLOCK_H__H

#include "nvcommon.h"
#include "nvmm.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#define ODM_MAX_INPUT_BUFFERS          10
#define ODM_MAX_OUTPUT_BUFFERS         10
#define ODM_MAX_IN_BUFFSIZE            1024*10
#define ODM_MAX_OUT_BUFFSIZE           65536
#define ODM_MIN_BUFFSIZE               1024

enum
{
    NvMMAttribute_OdmBase       = (NvMMAttributeReference_Unknown + 21),
    NvMMAttributeOdm_Force32 = 0x7FFFFFFF
};

#if defined(__cplusplus)
}
#endif

#endif /* __NVMM_ODMBLOCK_H__H*/
