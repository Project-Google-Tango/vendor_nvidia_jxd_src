/*
 * Copyright 2008 - 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"

#if NVOS_TRACE

#define MAX_LINE_SIZE 256
static NvOsFileHandle s_TraceFileHandle = 0;
static NvU32 s_TraceFileRefCount = 0;

#if defined(NV_IS_AVP)
static char s_FileName[] = "nvos_trace_avp.txt";
#else
static char s_FileName[] = "nvos_trace.txt";
#endif

void
NvOsTraceLogPrintf( const char *format, ... )
{
    char str[MAX_LINE_SIZE];
    NvU32 len;
    va_list args;

    if (!s_TraceFileHandle)
        return;

    va_start( args, format );
    NvOsMemset( str, 0, sizeof(str) );
    NvOsVsnprintf( str, sizeof(str), format, args );
    va_end( args );

    len = NvOsStrlen( str );
    (void)NvOsFwrite( s_TraceFileHandle, str, len );
    (void)NvOsFflush( s_TraceFileHandle );
}

void
NvOsTraceLogStart( void )
{
    if (s_TraceFileHandle)
    {
        s_TraceFileRefCount++;
        return;
    }

    NV_ASSERT_SUCCESS( NvOsFopen(s_FileName,
        (NVOS_OPEN_CREATE|NVOS_OPEN_WRITE|NVOS_OPEN_APPEND),
        &s_TraceFileHandle) );

    NV_ASSERT(s_TraceFileHandle != 0);
    s_TraceFileRefCount++;
}

void
NvOsTraceLogEnd( void )
{
    NV_ASSERT(s_TraceFileRefCount);
    s_TraceFileRefCount--;
    if (s_TraceFileRefCount == 0)
    {
        NvOsFclose(s_TraceFileHandle);
        s_TraceFileHandle = 0;
    }
}

#else

// FIXME: avp builds end up referencing tracing regardless of its enable bit

void NvOsTraceLogPrintf( const char *format, ... ) {}
void NvOsTraceLogStart( void ) {}
void NvOsTraceLogEnd( void ) {}

#endif
