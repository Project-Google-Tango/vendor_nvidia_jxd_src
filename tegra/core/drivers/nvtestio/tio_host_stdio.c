/*
 * Copyright 2008 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_local.h"
#include <stdio.h>

//   TODO: Fix this.  For now, temporarily disable a warning 
// that makes the Visual Studio 2005 and/or VS2008 compiler unhappy.  
#ifdef _WIN32
//#pragma warning( disable : 4996 )
#endif

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioDebugDumpf() - write to dump file (fflush if fmt==0)
//===========================================================================
void NvTioDebugDumpf(const char *fmt, ...)
{
#if NV_TIO_DUMP_CT_ENABLE
    static FILE *dumpFileFp = 0;
    va_list ap;

    if (!NvTioGlob.enableDebugDump)
        return;

    if (!dumpFileFp) {
        dumpFileFp = fopen(NvTioGlob.dumpFileName, "a");
        if (!dumpFileFp)
            return;
        fprintf(dumpFileFp, "======================= Start program\n");
    }

    if (!fmt) {
        fflush(dumpFileFp);
        return;
    }

    va_start(ap, fmt);
    vfprintf(dumpFileFp, fmt, ap);
    va_end(ap);

    fflush(dumpFileFp);
#endif
}

