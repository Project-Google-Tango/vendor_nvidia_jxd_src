/*
 * Copyright (c) 2009 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVODM_NLED_INT_H
#define INCLUDED_NVODM_NLED_INT_H

#include "nvodm_nled.h"

/**
 * @brief Used to enable/disable debug print messages.
 */
#define NV_ODM_DEBUG 0

#if NV_ODM_DEBUG
    #define NV_ODM_TRACE NvOdmOsDebugPrintf
#else
    #define NV_ODM_TRACE (void)
#endif

typedef struct NvOdmNledDeviceRec
{
    /** Only a placeholder. Does nothing */
    NvU32 Dumb;
} NvOdmNledContext;

#endif // INCLUDED_NVODM_NLED_INT_H
