/*
 * Copyright (c) 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef INCLUDED_NVCMS_DEVICE_H
#define INCLUDED_NVCMS_DEVICE_H

#include "nvcms.h"
#include "nverror.h"

/*
 * TODO [TaCo]: Support other profile types, e.g. LUT.
 */
NvError NvCmsDeviceGetProfile(NvCmsDisplayProfile* res, int dev);

#endif // INCLUDED_NVCMS_DEVICE_H
