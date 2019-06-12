/*
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#ifndef INCLUDED_NVDDK_XUSB_ARC_H
#define INCLUDED_NVDDK_XUSB_ARC_H


#if defined(__cplusplus)
     extern "C"
     {
#endif

void NvBootArcEnable(void);
void NvBootArcDisable(NvU32 TargetAddr);

#if defined(__cplusplus)
     }
#endif

#endif //INCLUDED_NVDDK_XUSB_ARC_H
