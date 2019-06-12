/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMS_LUT_H_
#define INCLUDED_NVCMS_LUT_H_

#include "nvcms.h"
#include "nvrm_init.h"
#include "nvrm_memmgr.h"
#include "nvrm_surface.h"
#include "nverror.h"

NvError NvCmsGenerateLuts(NvCmsDeviceLinkProfile *profile,
        NvRmDeviceHandle RmDevice);

#endif /* INCLUDED_NVCMS_LUT_H_ */
