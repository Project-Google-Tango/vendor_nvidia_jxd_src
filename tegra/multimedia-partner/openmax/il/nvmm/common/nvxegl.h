#ifndef __OMX_EGL_H__
#define __OMX_EGL_H__

/*
 * Copyright (c) 2008 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software and related documentation.  
 * Any use, reproduction, disclosure or distribution of this software and
 * related documentation without an express license agreement from NVIDIA
 * Corporation is strictly prohibited.
 */

#include "eglapiinterface.h"
#include "nvassert.h"

NvError NvxConnectToEgl (void);

extern NvEglApiExports g_EglExports;

#endif // __OMX_EGL_H__
