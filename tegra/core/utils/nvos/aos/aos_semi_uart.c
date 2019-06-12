/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "nvos.h"
#include "aos_semihost.h"
#include "nvassert.h"
#include "nvuart.h"

static void uart_Setup( void )
{
#if !NV_IS_AVP
    NvAvpUartInit(PLLP_FIXED_FREQ_KHZ_408000);
#endif
}
static void uart_BreakPoint( void ) {}

static void uart_Vprintf( const char *format, va_list ap )
{

}

static NvS32 uart_Vsnprintf( char *str, size_t size, const char *format, va_list ap )
{
    int n;
    n = nvaosSimpleVsnprintf( str, size, format, ap );
    return n;
}

static NvError uart_Vfprintf( NvOsFileHandle stream, const char *format, va_list ap )
{
    return NvError_NotImplemented;
}

static void uart_DebugString( const char *msg )
{
#if !NV_IS_AVP
     NvAvpUartWrite(msg);
#endif
}

static NvError uart_Fopen( const char *path, NvU32 flags, NvOsFileHandle *file )
{
    return NvError_NotImplemented;
}

static void uart_Fclose( NvOsFileHandle stream )
{
}

static NvError uart_Fwrite( NvOsFileHandle stream, const void *ptr,
    size_t size )
{
    return NvError_NotImplemented;
}

static NvError uart_Fread( NvOsFileHandle stream, void *ptr, size_t size,
    size_t *bytes )
{
    return NvError_NotImplemented;
}

static NvError uart_Fseek( NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence )
{
    return NvError_NotImplemented;
}

static NvError uart_Ftell( NvOsFileHandle file, NvU64 *position )
{
    return NvError_NotImplemented;
}

static NvError uart_Fflush( NvOsFileHandle stream )
{
    return NvError_NotImplemented;
}

static NvError uart_Fsync( NvOsFileHandle stream )
{
    return NvError_NotImplemented;
}

static NvError uart_Fremove( const char *filename )
{
    return NvError_NotImplemented;
}

static NvError uart_Flock( NvOsFileHandle stream, NvOsFlockType type )
{
    return NvError_NotImplemented;
}

static NvError uart_Ftruncate( NvOsFileHandle stream, NvU64 length )
{
    return NvError_NotImplemented;
}

static NvError uart_Opendir( const char *path, NvOsDirHandle *dir )
{
    return NvError_NotImplemented;
}

static NvError uart_Readdir( NvOsDirHandle dir, char *name, size_t size )
{
    return NvError_NotImplemented;
}

static void uart_Closedir(NvOsDirHandle dir)
{
}

static NvError uart_Mkdir( char *dirname )
{
    return NvError_NotImplemented;
}

static NvError uart_GetConfigU32( const char *name, NvU32 *value )
{
    return NvError_NotImplemented;
}

static NvError uart_GetConfigString( const char *name, char *value, NvU32 size )
{
    return NvError_NotImplemented;
}

void
nvaosRegisterSemiUart(NvAosSemihost *pSemi)
{
    const NvAosSemihost semi_uart = {
        uart_Setup,
        uart_BreakPoint,
        uart_Vprintf,
        uart_Vsnprintf,
        uart_Vfprintf,
        uart_DebugString,
        uart_Fopen,
        uart_Fclose,
        uart_Fwrite,
        uart_Fread,
        uart_Fseek,
        uart_Ftell,
        uart_Fflush,
        uart_Fsync,
        uart_Fremove,
        uart_Flock,
        uart_Ftruncate,
        uart_Opendir,
        uart_Readdir,
        uart_Closedir,
        uart_Mkdir,
        uart_GetConfigU32,
        uart_GetConfigString,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    if (nvaosGetSemihostStyle() == NvAosSemihostStyle_uart)
        NvOsMemcpy(pSemi, &semi_uart, sizeof(semi_uart));
}

