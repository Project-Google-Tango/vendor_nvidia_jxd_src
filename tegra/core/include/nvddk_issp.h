/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#ifndef INCLUDED_NVDDK_ISSP_HEADER_H
#define INCLUDED_NVDDK_ISSP_HEADER_H

#include "nvcommon.h"
#include "nverror.h"

#if defined(__cplusplus)
extern "C"
{

#endif

NvBool IsspProgramFirmware(void);
NvBool IsspParseHexFile(NvU8 *hexBuffer, NvU64 pSize);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_NVDDK_ISSP_H
