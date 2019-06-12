/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvrm_rmctrace.h"
#include "nvos.h"
#include "nvassert.h"

#define MAX_LINE_SIZE 128

NvError
NvRmRmcOpen( const char *name, NvRmRmcFile *rmc )
{
    NvError err = NvSuccess;
    NvOsFileHandle f;

    NV_ASSERT( rmc );

    rmc->enable = NV_FALSE;

    /* open the rmc file */
    if( name && name[0] )
    {
        err = NvOsFopen( name, NVOS_OPEN_CREATE | NVOS_OPEN_WRITE, &f );
        if( err == NvSuccess )
        {
            rmc->enable = NV_TRUE;
            rmc->file = f;
        }
    }

    return err;
}

void
NvRmRmcClose( NvRmRmcFile *rmc )
{
    if( rmc && rmc->file )
    {
        NvOsFclose( rmc->file );
    }
}

void
NvRmRmcTrace( NvRmRmcFile *rmc, const char *format, ... )
{
    char str[MAX_LINE_SIZE];
    NvU32 len;
    va_list args;

    if( !rmc || rmc->enable == NV_FALSE )
    {
        return;
    }

    va_start( args, format );
    NvOsMemset( str, 0, sizeof(str) );
    NvOsVsnprintf( str, sizeof(str), format, args );
    va_end( args );

    len = NvOsStrlen( str );
    (void)NvOsFwrite( rmc->file, str, len );
    (void)NvOsFflush( rmc->file );
}
