/*
 * Copyright (c) 2006-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA CORPORATION
 * is strictly prohibited.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"
#include "nvutil.h"
#include "aos.h"
#include "aos_semihost.h"
#include "nvuart.h"
#include "nvddk_avp.h"

#if __GNUC__
unsigned int swiSemi(int swi, int msg)
{
    asm volatile(
#ifndef __thumb    
    "svc 0x123456\n"
#else
    "svc 0xab\n"
#endif
    );
    return 0;
}
#endif //__GNUC__

static void stub_BreakPoint(void) { swiSemi(0x18, 0x20025); }
static void stub_DebugString(const char *msg) { swiSemi(0x04, (int) msg); }

static void stub_Setup( void ) {}
static void stub_Vprintf( const char *format, va_list ap ) {}

static NvError stub_Vfprintf( NvOsFileHandle stream, const char *format, va_list ap )
{
    return NvError_NotImplemented;
}
static NvError stub_Fopen( const char *path, NvU32 flags, NvOsFileHandle *file )
{
    return NvError_NotImplemented;
}
static void stub_Fclose(NvOsFileHandle stream) {}
static NvError stub_Fwrite( NvOsFileHandle stream, const void *ptr, size_t size )
{
    return NvError_NotImplemented;
}
static NvError stub_Fread(NvOsFileHandle stream, void *ptr, size_t size,
    size_t *bytes)
{
    return NvError_NotImplemented;
}
static NvError stub_Fseek( NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence )
{
    return NvError_NotImplemented;
}
static NvError stub_Ftell( NvOsFileHandle file, NvU64 *position )
{
    return NvError_NotImplemented;
}
static NvError stub_Fflush( NvOsFileHandle stream )
{
    return NvError_NotImplemented;
}
static NvError stub_Fsync( NvOsFileHandle stream )
{
    return NvError_NotImplemented;
}
static NvError stub_Fremove( const char *filename )
{
    return NvError_NotImplemented;
}
static NvError stub_Flock( NvOsFileHandle stream, NvOsFlockType type )
{
    return NvError_NotImplemented;
}
static NvError stub_Ftruncate( NvOsFileHandle stream, NvU64 length )
{
    return NvError_NotImplemented;
}

static NvError stub_Opendir( const char *path, NvOsDirHandle *dir )
{
    return NvError_NotImplemented;
}
static NvError stub_Readdir( NvOsDirHandle dir, char *name, size_t size )
{
    return NvError_NotImplemented;
}
static NvError stub_Mkdir( char *name )
{
    return NvError_NotImplemented;
}
static void stub_Closedir(NvOsDirHandle dir) {}
static NvError stub_GetConfigU32( const char *name, NvU32 *value )
{
    return NvError_NotImplemented;
}
static NvError stub_GetConfigString( const char *name, char *value, NvU32 size )
{
    return NvError_NotImplemented;
}

/* semihost configuration */

static NvAosSemihost s_Semi =
{
    stub_Setup,
    stub_BreakPoint,
    stub_Vprintf,
    nvaosSimpleVsnprintf,
    stub_Vfprintf,
    stub_DebugString,
    stub_Fopen,
    stub_Fclose,
    stub_Fwrite,
    stub_Fread,
    stub_Fseek,
    stub_Ftell,
    stub_Fflush,
    stub_Fsync,
    stub_Fremove,
    stub_Flock,
    stub_Ftruncate,
    stub_Opendir,
    stub_Readdir,
    stub_Closedir,
    stub_Mkdir,
    stub_GetConfigU32,
    stub_GetConfigString,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

void nvaosSemiSetDebugString(void (*DebugString)( const char *msg ))
{
    s_Semi.SemiDebugString = DebugString;
}

void nvaosSemiSetup( void )
{
    NvOdmDebugConsole DebugConsoleDevice = NvOdmDebugConsole_None;
#if !NV_IS_AVP
    DebugConsoleDevice = NvAvpGetDebugPort();
#endif
    switch(DebugConsoleDevice)  {
    case NvOdmDebugConsole_UartA:
    case NvOdmDebugConsole_UartB:
    case NvOdmDebugConsole_UartC:
    case NvOdmDebugConsole_UartD:
    case NvOdmDebugConsole_UartE:
        nvaosRegisterSemiUart(&s_Semi);
        break;
    case NvOdmDebugConsole_None:
        break;
     default:
        nvaosRegisterSemiRvice(&s_Semi);
        break;
    }
    s_Semi.SemiSetup();
}

void
NvOsBreakPoint(const char* file, NvU32 line, const char* condition)
{
    if( file )
    {
        if( condition )
        {
            NvOsDebugPrintf("\n\nAssert on %s:%d: %s\n", file, line,
                condition);
        }
        else
        {
            NvOsDebugPrintf("\n\nAssert on %s:%d\n", file, line);
        }
    }
    s_Semi.SemiBreakPoint();
}

NvS32
NvOsSnprintf( char *str, size_t size, const char *format, ... )
{
    int n;
    va_list ap;

    va_start( ap, format );
    n = NvOsVsnprintf( str, size, format, ap );
    va_end( ap );

    return n;
}

NvS32
NvOsVsnprintf( char *str, size_t size, const char *format, va_list ap )
{
    return s_Semi.SemiVsnprintf( str, size, format, ap );
}

NvError
NvOsFopenInternal(
    const char *path,
    NvU32 flags,
    NvOsFileHandle *file )
{
    return s_Semi.SemiFopen( path, flags, file );
}

void NvOsFcloseInternal(NvOsFileHandle stream)
{
    if (!stream)
        return;

    s_Semi.SemiFclose( stream );
}

NvError
NvOsFprintf( NvOsFileHandle stream, const char *format, ... )
{
    NvError err;
    va_list ap;

    va_start( ap, format );
    err = s_Semi.SemiVfprintf( stream, format, ap );
    va_end( ap );

    return err;
}

NvError
NvOsVfprintf( NvOsFileHandle stream, const char *format, va_list ap )
{
    return s_Semi.SemiVfprintf( stream, format, ap );
}

void
NvOsDebugPrintf( const char *format, ... )
{
    va_list ap;

    va_start( ap, format );
    NvOsDebugVprintf( format, ap );
    va_end( ap );
}

void
NvOsDebugVprintf( const char *format, va_list ap )
{
    char msg[256];
    NvOsVsnprintf( msg, sizeof(msg), format, ap );
    s_Semi.SemiDebugString( msg );
}

// DebugPrintf version returning number of characters printed
NvS32
NvOsDebugNprintf(const char *format, ...)
{
    va_list ap;
    char msg[256];
    int msgSize = 256;
    int n;

    va_start(ap, format);
    n = NvOsVsnprintf(msg, msgSize, format, ap);
    // output longer than msgSize + null termination, crop it
    if ((n < 0) || (n == msgSize))
    {
        n = msgSize - 1;
        msg[n] = 0;
    }
    s_Semi.SemiDebugString( msg );
    va_end(ap);

    return n;
}

void
NvOsDebugString(const char *str)
{
    s_Semi.SemiDebugString(str);
}

NvError
NvOsFwriteInternal( NvOsFileHandle stream, const void *ptr, size_t size )
{
    return s_Semi.SemiFwrite( stream, ptr, size );
}

NvError
NvOsFreadInternal(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec )
{
    return s_Semi.SemiFread( stream, ptr, size, bytes );
}

NvError
NvOsFseekInternal( NvOsFileHandle file, NvS64 offset, NvOsSeekEnum whence )
{
    return s_Semi.SemiFseek( file, offset, whence );
}

NvError
NvOsFtellInternal( NvOsFileHandle file, NvU64 *position )
{
    return s_Semi.SemiFtell( file, position );
}

NvError
NvOsFstatInternal( NvOsFileHandle file, NvOsStatType *s )
{
    NvError err;
    NvU64 CurrentPos;
    NvU64 FileSize;

    if( !file )
    {
        return NvError_BadParameter;
    }

    err = NvOsFtellInternal( file, &CurrentPos );
    if( err != NvSuccess || (NvS64)CurrentPos == -1 )
    {
        return NvError_FileOperationFailed;
    }

    err = NvOsFseekInternal( file, 0L, NvOsSeek_End );
    if( err != NvSuccess )
    {
        return NvError_FileOperationFailed;
    }

    (void)NvOsFtellInternal( file, &FileSize );
    // if this fails things are really screwed up because we can't get back to
    // to the original position.
    NvOsFseekInternal( file, CurrentPos, NvOsSeek_Set );

    s->size = (NvU64)FileSize;
    s->type = NvOsFileType_File;
    s->mtime = 0;

    return NvError_Success;
}

NvError
NvOsFflushInternal( NvOsFileHandle stream )
{
    return s_Semi.SemiFflush( stream );
}

NvError
NvOsOpendirInternal( const char *path, NvOsDirHandle *dir )
{
    NV_ASSERT( path );
    NV_ASSERT( dir );

    return s_Semi.SemiOpendir( path, dir );
}

NvError
NvOsReaddirInternal( NvOsDirHandle dir, char *name, size_t size )
{
    return s_Semi.SemiReaddir( dir, name, size );
}

void NvOsClosedirInternal(NvOsDirHandle dir)
{
    s_Semi.SemiClosedir( dir );
}

NvError
NvOsMkdirInternal( char *dirname )
{
    return s_Semi.SemiMkdir( dirname );
}

NvError
NvOsGetConfigU32( const char *name, NvU32 *value )
{
#if NVAOS_USE_SEMI_CONFIG
    return s_Semi.SemiGetConfigU32( name, value );
#else
    char *env;

    NV_ASSERT( name );
    NV_ASSERT( value );

    env = nvaosGetenv( name );
    if( !env )
    {
        return NvError_BadParameter;
    }

    *value = (NvU32)atol( env );
    return NvSuccess;
#endif
}

NvError
NvOsGetConfigString( const char *name, char *value, NvU32 size )
{
#if NVAOS_USE_SEMI_CONFIG
    return s_Semi.SemiGetConfigString( name, value, size );
#else

    char *env;
    NvU32 len;

    NV_ASSERT( name );
    NV_ASSERT( value );
    NV_ASSERT( size != (NvU32)-1 );

    env = nvaosGetenv( name );
    if( !env )
    {
        return NvError_BadParameter;
    }

    len = NvOsStrlen( env ) + 1;
    if( len > size )
    {
        return NvError_InsufficientMemory;
    }

    NvOsStrncpy( value, env, len );
    return NvSuccess;
#endif
}

NvError
NvOsConfigGetState(int stateId, const char *name, char *value,
        int valueSize, int flags)
{
    return NvError_NotSupported;
}

#if !__GNUC__
/** Semihosting redirection */

extern FILEHANDLE $Super$$_sys_open(  const char *name, int openmode );
extern int $Super$$_sys_close( FILEHANDLE f );
extern int $Super$$_sys_write( FILEHANDLE f, const unsigned char *buf,
    unsigned len, int mode );
extern int $Super$$_sys_read( FILEHANDLE f, unsigned char *buf, unsigned len,
    int mode );
extern int $Super$$_sys_istty( FILEHANDLE f );
extern int $Super$$_sys_seek( FILEHANDLE f, long pos );
extern int $Super$$_sys_ensure( FILEHANDLE f );
extern long $Super$$_sys_flen( FILEHANDLE f );
extern char *$Super$$_sys_command_string( char *cmd, int len);

extern void $Super$$_ttrywrch( char ch );;

void
$Sub$$_trywrch(char ch)
{
    // just drop it, no one uses this.
}

FILEHANDLE
$Sub$$_sys_open( const char *name, int openmode )
{
    if( !s_Semi.Semi_sys_open )
    {
        return $Super$$_sys_open( name, openmode );
    }
    else
    {
        return s_Semi.Semi_sys_open( name, openmode );
    }
}

int
$Sub$$_sys_close( FILEHANDLE f )
{
    if( !f )
    {
        return 0;
    }

    if( !s_Semi.Semi_sys_close )
    {
        return $Super$$_sys_close( f );
    }
    else
    {
        return s_Semi.Semi_sys_close( f );
    }
}

int
$Sub$$_sys_write( FILEHANDLE f, const unsigned char *buf, unsigned len,
    int mode )
{
    if (f == 0)
    {
        return 0;
    }

    if( !s_Semi.Semi_sys_write )
    {
        return $Super$$_sys_write( f, buf, len, mode );
    }
    else
    {
        return s_Semi.Semi_sys_write( f, buf, len, mode );
    }
}

int
$Sub$$_sys_read( FILEHANDLE f, unsigned char *buf, unsigned len, int mode )
{
    if( !f )
    {
        return 0;
    }

    if( !s_Semi.Semi_sys_read )
    {
        return $Super$$_sys_read( f, buf, len, mode );
    }
    else
    {
        return s_Semi.Semi_sys_read( f, buf, len, mode );
    }
}

int
$Sub$$_sys_istty( FILEHANDLE f )
{
    // filehandle numbers 1 and 2 are for stdin/stdout/stderr
    if ( (NvU32)f == 1 ||
         (NvU32)f == 2 )
    {
        return 1;
    }
    return 0;
}

int
$Sub$$_sys_seek( FILEHANDLE f, long pos )
{
    if( !s_Semi.Semi_sys_seek )
    {
        return $Super$$_sys_seek( f, pos );
    }
    else
    {
        return s_Semi.Semi_sys_seek( f, pos );
    }
}

int
$Sub$$_sys_ensure( FILEHANDLE f )
{
    return 0;
}

long
$Sub$$_sys_flen( FILEHANDLE f )
{
    if( !s_Semi.Semi_sys_flen )
    {
        return $Super$$_sys_flen( f );
    }
    else
    {
        return s_Semi.Semi_sys_flen( f );
    }
}

char *
$Sub$$_sys_command_string(char *cmd, int len)
{
    if( !s_Semi.Semi_sys_command_string )
    {
        return $Super$$_sys_command_string(cmd, len);
    }
    else
    {
        return s_Semi.Semi_sys_command_string( cmd, len );
    }
}
#endif //__GNUC__

