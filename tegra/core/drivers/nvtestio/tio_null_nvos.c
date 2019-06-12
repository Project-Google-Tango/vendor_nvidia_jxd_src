/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
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

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define NV_TIO_MAX_FILES    10

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

NvTioStream gs_Streams[NV_TIO_MAX_FILES] = {{0}};

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioDebugf() - debug printf
//===========================================================================
void NvTioDebugf(const char *fmt, ... )
{ }

//===========================================================================
// NvTioOsAllocStream() - allocate a stream structure
//===========================================================================
NvTioStream *NvTioOsAllocStream(void)
{
    int i, j;
    for (i=0; i<NV_TIO_MAX_FILES; i++) {
        if (gs_Streams[i]->magic == NV_TIO_FREE_MAGIC) {
            gs_Streams[i]->magic =  NV_TIO_ALLOC_MAGIC;
            gs_Streams[i]->f.fp  =  0;
            gs_Streams[i]->ops   =  0;
            return &gs_Streams[i];
        }
    }
    return NULL;
}

//===========================================================================
// NvTioOsFreeStream() - free a stream structure
//===========================================================================
void NvTioOsFreeStream(NvTioStream *stream)
{
    if (stream)
        stream->magic = NV_TIO_FREE_MAGIC;
}

//===========================================================================
// NvTioOsAllocVsprintf() vsprintf to an allocated buffer
//===========================================================================
NvError NvTioOsAllocVsprintf(
                    char  **buf,
                    NvU32  *len,
                    const char *format,
                    va_list ap)
{
    return NvError_NotImplemented;
}

//===========================================================================
// NvTioOsFreeVsprintf() - free buffer allocated by NvTioVsprintfAlloc()
//===========================================================================
void NvTioOsFreeVsprintf(
                    char  *buf,
                    NvU32  len)
{ }

//===========================================================================
// NvTioRegisterNvos() - register uart transport
//===========================================================================
void NvTioRegisterNvos(void)
{ }

//===========================================================================
// NvTioOsStrcmp() - strncmp
//===========================================================================
int NvTioOsStrcmp(const char *s1, const char *s2)
{
    while(1) {
        int val = *s1 - *s2;
        if (val)
            return val;
        if (!*s1)
            break;
        s1++;
        s2++;
    }
    return 0;
}

//===========================================================================
// NvTioOsStrncmp() - strncmp
//===========================================================================
int NvTioOsStrncmp(const char *s1, const char *s2, size_t size)
{
    while(size--) {
        int val = *s1 - *s2;
        if (val)
            return val;
        if (!*s1)
            break;
        s1++;
        s2++;
    }
    return 0;
}

//===========================================================================
// NvTioNvosGetConfigString() - get string env var or registry value
//===========================================================================
NvError NvTioNvosGetConfigString(const char *name, char *value, NvU32 size)
{
    return NvError_NotSupported;
}

//===========================================================================
// NvTioNvosGetConfigU32() - get integer env var or registry value
//===========================================================================
NvError NvTioNvosGetConfigU32(const char *name, NvU32 *value)
{
    return NvError_NotSupported;
}

