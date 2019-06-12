/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "NvxHelpers.h"

NvU32 NvxCrcGetDigestLength(void);

void NvxCrcCalcBuffer( NvU8 *buffer, int len, char *out);

NvBool NvxCrcCalcNvMMSurface( NvxSurface *pSurface, char *out);

NvBool NvxCrcCalcRgbSurface( NvxSurface *pSurface, char *out);

NvBool NvxCrcCalcYUV420Planar( NvxSurface *pSurface, char *out);





