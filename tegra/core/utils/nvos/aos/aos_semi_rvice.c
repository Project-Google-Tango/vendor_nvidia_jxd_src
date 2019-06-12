/*
 * Copyright (c) 2007-2013, NVIDIA CORPORATION.  All rights reserved.
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

static void rvice_Setup( void )
{
}

static void rvice_BreakPoint( void )
{
    swiSemi(0x18, 0x20025);
}

static void rvice_Vprintf( const char *format, va_list ap )
{
    vprintf( format, ap );
}

static NvS32 rvice_Vsnprintf( char *str, size_t size, const char *format, va_list ap )
{
    int n;

    n = vsnprintf( str, size, format, ap );
    return n;
}

static NvError rvice_Vfprintf( NvOsFileHandle stream, const char *format, va_list ap )
{
    int n;
    int length = 0;
    int ExpandedLength;
    char *BigBuf=NULL;
    char SmallBuf[256];

    ExpandedLength = vsnprintf(SmallBuf, sizeof(SmallBuf)-1, format, ap);
    if (ExpandedLength < (int)(sizeof(SmallBuf) - 1))
    {
        length = strlen(SmallBuf);
        n = fwrite(SmallBuf, 1, length, (FILE*)stream);

        if (n == length)
            return NvSuccess;

        return NvError_FileWriteFailed;
    }

    BigBuf = NvOsAlloc(length + 1);
    if (!BigBuf)
        return NvError_FileWriteFailed;

    ExpandedLength = vsnprintf(BigBuf, length, format, ap);
    if (ExpandedLength >= length)
    {
        length = strlen(BigBuf);
        n = fwrite(BigBuf, 1, length, (FILE*)stream);
        NvOsFree(BigBuf);
        if (n == length)
            return NvSuccess;

        return NvError_FileWriteFailed;
    }

    NvOsFree(BigBuf);
    return NvError_FileWriteFailed;
}

static void rvice_DebugString( const char *msg )
{
    swiSemi(0x04, (int) msg);
}

static NvError rvice_Fopen( const char *path, NvU32 flags, NvOsFileHandle *file )
{
    FILE *f;
    char *mode;

    NV_ASSERT( path );
    NV_ASSERT( file );

    switch( flags ) {
    case NVOS_OPEN_READ:
        mode = "rb";
        break;
    case NVOS_OPEN_WRITE:
    case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE:
        mode = "wb";
        break;
    case NVOS_OPEN_READ | NVOS_OPEN_WRITE:
        mode = "rb+";
        break;
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE:
        mode = "wb+";
        break;
    case NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_APPEND:
    case NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        mode = "ab";
        break;
    case NVOS_OPEN_READ | NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_APPEND:
    case NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
    case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        mode = "ab+";
        break;
    default:
        return NvError_BadParameter;
    }

    f = fopen( path, mode );
    if( f == NULL )
    {
        return NvError_BadParameter;
    }

    *file = (NvOsFileHandle)f;
    return NvSuccess;
}

static void rvice_Fclose( NvOsFileHandle stream )
{
    (void)fclose((FILE *)stream);
}

static NvError rvice_Fwrite( NvOsFileHandle stream, const void *ptr,
    size_t size )
{
    size_t bytes;

    NV_ASSERT( stream );
    NV_ASSERT( ptr );

    bytes = fwrite( ptr, 1, size, (FILE *)stream );
    if (bytes != size)
    {
        return NvError_FileWriteFailed;
    }

    return NvSuccess;
}

static NvError rvice_Fread( NvOsFileHandle stream, void *ptr, size_t size,
    size_t *bytes )
{
    size_t b;
    long pos;

    pos = ftell((FILE*)stream);
    if (pos != pos)
    {
        NV_ASSERT(0);
    }

    NV_ASSERT( (FILE *)stream );
    NV_ASSERT( ptr );

    b = fread( ptr, 1, size, (FILE *)stream );
    if( b != size )
    {
        if( ferror( (FILE *)stream ) )
        {
            clearerr( (FILE *)stream );
            return NvError_FileReadFailed;
        }
        else if( feof( (FILE *)stream ) )
        {
            if( bytes )
            {
                *bytes = b;
            }

            clearerr( (FILE *)stream );
            return NvError_EndOfFile;
        }
    }

    if( bytes ) *bytes = b;
    return NvSuccess;
}

static NvError rvice_Fseek( NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence )
{
    int w;
    int err;

    NV_ASSERT( (FILE *)file );

    switch( whence ) {
    case NvOsSeek_Set: w = SEEK_SET; break;
    case NvOsSeek_Cur: w = SEEK_CUR; break;
    case NvOsSeek_End: w = SEEK_END; break;
    default:
        return NvError_BadParameter;
    }

    err = fseek( (FILE *)file, (long)offset, w );
    if( err != 0 )
    {
        return NvError_FileOperationFailed;
    }

    return NvSuccess;
}

static NvError rvice_Ftell( NvOsFileHandle file, NvU64 *position )
{
    long offset;

    NV_ASSERT( file );
    NV_ASSERT( position );

    offset = ftell( (FILE *)file );
    if( offset == -1 )
    {
        return NvError_FileOperationFailed;
    }

    *position = (NvU64)offset;
    return NvSuccess;
}

static NvError rvice_Fflush( NvOsFileHandle stream )
{
    int err;

    NV_ASSERT( (FILE *)stream );

    err = fflush( (FILE *)stream );
    if( err != 0 )
    {
        return NvError_FileOperationFailed;
    }
    return NvSuccess;
}

static NvError rvice_Fsync( NvOsFileHandle stream )
{
    return NvSuccess;
}

static NvError rvice_Fremove( const char *filename )
{
    if (0 == remove(filename))
    {
        return NvSuccess;
    }
    else
    {
        return NvError_FileOperationFailed;
    }
}

static NvError rvice_Flock( NvOsFileHandle stream, NvOsFlockType type )
{
    return NvError_NotImplemented;
}

static NvError rvice_Ftruncate( NvOsFileHandle stream, NvU64 length )
{
    return NvError_NotImplemented;
}

static NvError rvice_Opendir( const char *path, NvOsDirHandle *dir )
{
    return NvError_NotImplemented;
}

static NvError rvice_Readdir( NvOsDirHandle dir, char *name, size_t size )
{
    return NvError_NotImplemented;
}

static void rvice_Closedir(NvOsDirHandle dir)
{
}

static NvError rvice_Mkdir( char *dirname )
{
    return NvError_NotImplemented;
}

static NvError rvice_GetConfigU32( const char *name, NvU32 *value )
{
    char *EnvVar;

    NV_ASSERT( name );
    NV_ASSERT( value );

    EnvVar = getenv(name);
    if (EnvVar == NULL)
    {
        return NvError_BadParameter;
    }

    *value = (NvU32)atol(EnvVar);

    return NvSuccess;
}

static NvError rvice_GetConfigString( const char *name, char *value, NvU32 size )
{
    char *EnvVar;

    NV_ASSERT( name );
    NV_ASSERT( value );
    NV_ASSERT( size > 0 );

    EnvVar = getenv(name);
    if ( EnvVar == NULL)
    {
        return NvError_BadParameter;
    }

    if ( (strlen(EnvVar)+1) > size)
    {
        return NvError_InsufficientMemory;
    }

    strcpy(value, EnvVar);
    return NvError_Success;
}

NV_WEAK void
nvaosRegisterSemiRvice(NvAosSemihost *pSemi)
{
    const NvAosSemihost semi_rvice = {
        rvice_Setup,
        rvice_BreakPoint,
        rvice_Vprintf,
        rvice_Vsnprintf,
        rvice_Vfprintf,
        rvice_DebugString,
        rvice_Fopen,
        rvice_Fclose,
        rvice_Fwrite,
        rvice_Fread,
        rvice_Fseek,
        rvice_Ftell,
        rvice_Fflush,
        rvice_Fsync,
        rvice_Fremove,
        rvice_Flock,
        rvice_Ftruncate,
        rvice_Opendir,
        rvice_Readdir,
        rvice_Closedir,
        rvice_Mkdir,
        rvice_GetConfigU32,
        rvice_GetConfigString,
        0,
        0,
        0,
        0,
        0,
        0,
        0
    };
    if (nvaosGetSemihostStyle() == NvAosSemihostStyle_rvice)
        NvOsMemcpy(pSemi, &semi_rvice, sizeof(semi_rvice));
}
