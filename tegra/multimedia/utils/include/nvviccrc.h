/*
 * Copyright (c) 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_VIC_CRC
#define INCLUDED_VIC_CRC

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool
NvGetVicCrc(
    NvMMSurfaceDescriptor *pSrcSurface,
    NvMMSurfaceDescriptor *pDstSurface,
    NvU32 *Vic_crc);

#if defined(__cplusplus)
}
#endif

#endif // INCLUDED_VIC_CRC

