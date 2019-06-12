/*
 * Copyright (c) 2009-2014, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_AP_H
#define INCLUDED_AOS_AP_H

#include "nvcommon.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef ENABLE_NVTBOOT
void nvaosInitPmcScratch( NvAosChip Chip );
#endif

NvU32 nvaosGetOdmData(void);

#ifdef __cplusplus
}
#endif

#endif

