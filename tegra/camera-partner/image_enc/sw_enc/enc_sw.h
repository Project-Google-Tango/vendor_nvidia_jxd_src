/*
 * Copyright (c) 2012 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NV_ENC_SW_H
#define INCLUDED_NV_ENC_SW_H

#include "nvassert.h"
#include "nvimage_enc.h"
#include "nvimage_enc_pvt.h"
#include "nvcommon.h"
#include "nvrm_memmgr.h"
#include "nvrm_surface.h"
#include <stdio.h>
#include <stdlib.h>
#include "jpeglib.h"

void ImgEnc_swEnc(NvImageEncHandle pContext);

#endif // INCLUDED_NV_ENC_SW_H

