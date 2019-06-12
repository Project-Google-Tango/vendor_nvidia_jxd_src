/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_AOS_SEMIHOST_H
#define INCLUDED_AOS_SEMIHOST_H

#include <stdarg.h>
#include "nvcommon.h"
#include "nvos.h"
#include "aos.h"
#if !__GNUC__
#include "rt_sys.h"
#else
typedef void* FILEHANDLE;
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * AOS supports several file i/o (semihosting) implementations.
 */
typedef struct NvAosSemihostRec
{
    /* called during init */
    void (*SemiSetup)( void );

    void (*SemiBreakPoint)( void );

    void (*SemiVprintf)( const char *format, va_list ap );
    NvS32 (*SemiVsnprintf)( char *str, size_t size, const char *format,
        va_list ap );
    NvError (*SemiVfprintf)( NvOsFileHandle stream, const char *format,
        va_list ap );
    void (*SemiDebugString)( const char *msg );

    NvError (*SemiFopen)( const char *path, NvU32 flags,
        NvOsFileHandle *file );
    void (*SemiFclose)( NvOsFileHandle stream );
    NvError (*SemiFwrite)( NvOsFileHandle stream, const void *ptr,
        size_t size );
    NvError (*SemiFread)( NvOsFileHandle stream, void *ptr, size_t size,
        size_t *bytes );
    NvError (*SemiFseek)( NvOsFileHandle file, NvS64 offset,
        NvOsSeekEnum whence );
    NvError (*SemiFtell)( NvOsFileHandle file, NvU64 *position );
    NvError (*SemiFflush)( NvOsFileHandle stream );
    NvError (*SemiFsync)( NvOsFileHandle stream );
    NvError (*SemiFremove)( const char *filename );
    NvError (*SemiFlock)( NvOsFileHandle stream, NvOsFlockType type );
    NvError (*SemiFtruncate)( NvOsFileHandle stream, NvU64 length );

    NvError (*SemiOpendir)( const char *path, NvOsDirHandle *dir );
    NvError (*SemiReaddir)( NvOsDirHandle dir, char *name, size_t size );
    void (*SemiClosedir)(NvOsDirHandle dir);
    NvError (*SemiMkdir)(char *dirname);

    NvError (*SemiGetConfigU32)( const char *name, NvU32 *value );
    NvError (*SemiGetConfigString)( const char *name, char *value,
        NvU32 size );

    /* sys interface from rt_sys.h, leave null for defaults ($Super calls) */
    FILEHANDLE (*Semi_sys_open)(  const char *name, int openmode );
    int (*Semi_sys_close)( FILEHANDLE f );
    int (*Semi_sys_write)( FILEHANDLE f, const unsigned char *buf, 
        unsigned len, int mode );
    int (*Semi_sys_read)( FILEHANDLE f, unsigned char *buf, unsigned len,
        int mode );
    int (*Semi_sys_seek)( FILEHANDLE f, long pos );
    long (*Semi_sys_flen)( FILEHANDLE f );
    char *(*Semi_sys_command_string)( char *cmd, int len);
} NvAosSemihost;

typedef enum
{
    O_RDONLY        = 0,
    O_WRONLY        = 1,
    O_RDWR          = 2,

    O_APPEND        = 0x0008,
    O_CREAT         = 0x0200,
    O_TRUNC         = 0x0400,
    O_EXCL          = 0x0800,
    O_SYNC          = 0x2000,
    O_NONBLOCK      = 0x4000,
    O_NOCTTY        = 0x8000,

    O_ASYNC         = 0x0040,

    O_BINARY        = 0x10000,
    O_TEXT          = 0x20000,
    O_NOINHERIT     = 0x40000,

    O_DIRECT,
    O_DIRECTORY,
    O_LARGEFILE,
    O_NOATIME,
    O_NOFOLLOW,
    O_NONDELAY,
    
    FS_FLAG_APPEND      = O_APPEND,
    FS_FALG_CREATE      = O_CREAT,
    FS_FLAG_TRUNCATE    = O_TRUNC,
    FS_FLAG_EXCLUDE     = O_EXCL,
    FS_FLAG_SYNC        = O_SYNC,
    FS_FLAG_NONBLOCK    = O_NONBLOCK,
    FS_FLAG_NOCTTY      = O_NOCTTY,
    FS_FLAG_ASYNC       = O_ASYNC,

    FS_FLAG_BINARY      = O_BINARY,
    FS_FLAG_TEXT        = O_TEXT,
    FS_FLAG_NOINHERIT   = O_NOINHERIT,
} FsFlag;

typedef struct FsStatRec
{
    int st_dev;
    int st_ino;
    int st_mode;
    int st_nlink;
    int st_rdev;
    int st_size;

    int st_blksize;     
    int st_blocks;
    
    //time_t st_atime;
    //time_t st_mtime;  
    //time_t st_ctime;  
    
    // something new stat

    char fattribute;
} FsStat;

void nvaosRegisterSemiUart(NvAosSemihost *pSemi);
NV_WEAK void nvaosRegisterSemiRvice(NvAosSemihost *pSemi);

#if __GNUC__
unsigned int swiSemi(int swi, int msg);
#else
/* default configuration for semihosting */
#ifndef __thumb
    unsigned __swi(0x123456) int swiSemi(int, int);
#else
    unsigned __swi(0xab) int swiSemi(int, int);
#endif
#endif

#if defined(__cplusplus)
}
#endif

#endif
