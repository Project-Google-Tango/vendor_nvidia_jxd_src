/*
 * Copyright (c) 2007 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvapputil.h"

#if NVOS_IS_WINDOWS || NVOS_IS_LINUX
// On regular Windows/Linux, need stdio.h for vprintf() definition.
#include <stdio.h>
#else
// On WinCE/AOS, need nvos.h for NvOsDebugVprintf() definition.
#include "nvos.h"
#endif

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvAuPrintf() - Print a string to the application's stdout (whatever that
// may be).
//===========================================================================
void
NvAuPrintf(const char *format, ...)
{
    va_list ap;

    va_start(ap, format);

#if NVOS_IS_WINDOWS || NVOS_IS_LINUX
    // On regular Windows/Linux, printing go to stdout.
    vprintf(format, ap);
#else
    // On WinCE/AOS, printing goes to the OS debug console (there is no stdout).
    NvOsDebugVprintf(format, ap);
#endif

    va_end(ap);
}
