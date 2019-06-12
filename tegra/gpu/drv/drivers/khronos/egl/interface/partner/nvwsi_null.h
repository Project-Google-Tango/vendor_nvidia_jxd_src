/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#ifndef NVWSI_NULL_H
#define NVWSI_NULL_H

/**
 * @file
 * <b>NVIDIA null window system</b>
 *
 * @b Description: Nvwsi supports a null window system for testing purposes.
 *     Display and window/pixmap creation for this window system is the
 *     reponsibility of the user (typically nvwinsys).
 */

#include "nvcolor.h"

#ifdef __cplusplus
extern "C" {
#endif

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

/**
 * Native display recognized by Nvwsi's null backend (for use with
 * eglgetDisplay).
 * In addition to this value, EGL_DEFAULT_DISPLAY is recognized if the null
 * backend is built as Nvwsi's default backend.
 *
 * This is an odd value, to avoid collision with pointers.
 */
#define NVWSI_NULL_NATIVEDISPLAY    (void*)0xC0000001


/**
 * Magic value to detect valid null window system structures.
 */
#define NVWSI_NULL_WINDOW_MAGIC     0x84539653
#define NVWSI_NULL_PIXMAP_MAGIC     0x98563965


/**
 * Dimensions of the null desktop.
 */
#define NVWSI_NULL_DESKTOP_WIDTH    1920
#define NVWSI_NULL_DESKTOP_HEIGHT   1080


//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

/**
 * Structure defining a null window.
 */
typedef struct
{
    NvU32           magic;      // Always set to NVWSI_NULL_WINDOW_MAGIC
    NvU32           width;
    NvU32           height;
} NvWsiNullWindow;

/**
 * Structure defining a null pixmap.
 */
typedef struct
{
    NvU32           magic;      // Always set to NVWSI_NULL_PIXMAP_MAGIC
    NvU32           width;
    NvU32           height;
    NvColorFormat   format;
} NvWsiNullPixmap;

#ifdef __cplusplus
}
#endif

/** @} */
#endif // NVWSI_NULL_H
