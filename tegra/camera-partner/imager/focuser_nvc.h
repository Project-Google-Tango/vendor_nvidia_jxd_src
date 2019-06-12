/*
 * Copyright (c) 2011 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef FOCUSER_NVC_H
#define FOCUSER_NVC_H

#include "nvodm_imager.h"

#if defined(__cplusplus)
extern "C"
{
#endif

NvBool FocuserNvc_GetHal(NvOdmImagerHandle hImager);

#if defined(__cplusplus)
}
#endif

#endif  // FOCUSER_NVC_H
