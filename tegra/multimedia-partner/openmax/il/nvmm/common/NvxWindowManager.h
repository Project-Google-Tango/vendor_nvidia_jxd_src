/* Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an
 * express license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef __NVX_WINDOWMANAGER_H__
#define __NVX_WINDOWMANAGER_H__


/**
 * @file
 * @brief <b>nVIDIA OpenMax window manager</b>
 *
 * @b Description: Helpers to coordinate with various windowing systems
 */
 
/**
 *   Note: These helper functions are intended to provide an abstraction
 *   layer for supporting various windowing systems.  
 */ 

#include "nvrm_surface.h"
#include "common/NvxHelpers.h"

NvBool NvxWindowGetDisplayRect(void *hWnd, NvRect *pRect);

#endif /* __NVX_WINDOWMANAGER_H__ */
