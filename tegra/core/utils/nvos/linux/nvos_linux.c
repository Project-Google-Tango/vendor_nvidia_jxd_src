/*
 * Copyright (c) 2006-2013,  NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#if defined(__linux__)
/* Large File Support */
#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64
#define _GNU_SOURCE
#endif

#if defined(__APPLE__)
// open() and lseek() on OS X are 64-bit safe
#define O_LARGEFILE 0
#define loff_t off_t
#define lseek64 lseek
#endif

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sched.h>
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvos_debug.h"
#include "nvassert.h"
#include "nvos_linux.h"
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/file.h>

#define USE_SELECT_FOR_FIFOS 0

#ifndef UNIFIED_SCALING
#define UNIFIED_SCALING 0
#endif

#ifdef ANDROID
#include <cutils/properties.h>
#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>
#include <unwind.h>
#include <cutils/ashmem.h>
#include <cutils/log.h>
#include <linux/throughput_ioctl.h>
#define THROUGHPUT_DEVICE "/dev/tegra-throughput"
#else
#include <execinfo.h>
#endif

#ifdef ANDROID
static int tegra_throughput_fd = -1;
#endif

#define MAX_CALLSTACK_FRAMES 32

struct NvCallstackRec
{
    NvOsCallstackType           type;
    void*                       frames[MAX_CALLSTACK_FRAMES];
    NvU32                       numFrames;
    char**                      frameSymbols;
};

extern NvBool g_UseCoopThread;

#if NVOS_TRACE || NV_DEBUG
#undef NvOsAlloc
#undef NvOsFree
#undef NvOsRealloc
#undef NvOsSharedMemAlloc
#undef NvOsSharedMemHandleFromFd
#undef NvOsSharedMemGetFd
#undef NvOsSharedMemMap
#undef NvOsSharedMemUnmap
#undef NvOsSharedMemFree
#undef NvOsMutexCreate
#undef NvOsExecAlloc
#undef NvOsExecFree
#undef NvOsPageAlloc
#undef NvOsPageLock
#undef NvOsPageFree
#undef NvOsPageLock
#undef NvOsPageMap
#undef NvOsPageMapIntoPtr
#undef NvOsPageUnmap
#undef NvOsPageAddress
#undef NvOsIntrMutexCreate
#undef NvOsIntrMutexLock
#undef NvOsIntrMutexUnlock
#undef NvOsIntrMutexDestroy
#undef NvOsInterruptRegister
#undef NvOsInterruptUnregister
#undef NvOsInterruptEnable
#undef NvOsInterruptDone
#undef NvOsPhysicalMemMapIntoCaller
#undef NvOsMutexLock
#undef NvOsMutexUnlock
#undef NvOsMutexDestroy
#undef NvOsPhysicalMemMap
#undef NvOsPhysicalMemUnmap
#undef NvOsSemaphoreCreate
#undef NvOsSemaphoreWait
#undef NvOsSemaphoreWaitTimeout
#undef NvOsSemaphoreSignal
#undef NvOsSemaphoreDestroy
#undef NvOsSemaphoreClone
#undef NvOsSemaphoreUnmarshal
#undef NvOsThreadCreate
#undef NvOsInterruptPriorityThreadCreate
#undef NvOsThreadJoin
#undef NvOsThreadYield
#endif

/** implementation helper functions, etc. */
static pthread_mutex_t g_timemutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t g_callstackmutex = PTHREAD_MUTEX_INITIALIZER;

#if defined(ANDROID)
static int fpstarget_fd = -1;
static int lastFpsTarget = -1;
#endif

const NvOsLinuxKernelDriver *g_NvOsKernel = NULL;

#if !defined(ANDROID)
/* Hack -- static host builds will not include the so _init and _fini
 * functions, so a run-time initialization is required (although this is
 * not thread safe).  ANDROID always uses dynamic libraries, so rather than
 * paying the extra cost for a run-time test that always passes, the
 * preprocessor is used to select between a real function and a no-op name */
const NvOsLinuxKernelDriver *NvOsLinUserInit(void);

static void NvOs_StaticInit(void)
{
    if (!g_NvOsKernel)
        g_NvOsKernel = NvOsLinUserInit();
}
#else

#define NvOs_StaticInit()

#endif

#if defined(ANDROID)
/* This version can be used for LDK and Linux host utilities, but everything
 * using libnvos.a (static version) will have to be modified to build with
 * -lrt for librt. Because on glibc clock_gettime() is in librt.
 * On Android clock_gettime() is in Bionic/NDK's libc.
 */
/**
 * NvOs_gettimeofday - returns the current time in microseconds.
 * This API guarantees monotonicity of the time.
 */
static NvU64 NvOs_gettimeofday(void)
{
    struct timespec ts;
    NvU64 time;
    int res;

    res = clock_gettime( CLOCK_MONOTONIC, &ts );
    if ( res )
    {
        NvOsDebugPrintf("\n\nNvOs_gettimeofday() failure:CLOCK_MONOTONIC unsupported\n");
        return 0;
    }

    time = (NvU64)ts.tv_sec * 1000000 + (NvU64)ts.tv_nsec / 1000;

    return time;
}
#else
/**
 * NvOs_gettimeofday - returns the current time in microseconds.
 * This API gaurantees monotonicity of the time. If for some reason the time
 * shiftes backwards like in the sytems which relies on NTP for time, this API
 * compenstates for the time skew.
 */
static NvU64 NvOs_gettimeofday(void)
{
    static NvU64 s_oldtime = 0;
    static NvU64 s_timedelta = 0;
    struct timeval tv;
    NvU64 time;

    pthread_mutex_lock(&g_timemutex);

    (void)gettimeofday( &tv, 0 );
    time = (NvU64)( ( (NvU64)tv.tv_sec * 1000 * 1000 ) + (NvU64)tv.tv_usec );
    if (time < s_oldtime)
        s_timedelta += (s_oldtime - time);
    s_oldtime = time;
    time += s_timedelta;

    pthread_mutex_unlock(&g_timemutex);

    return time;
}
#endif

void NvOsBreakPoint(
    const char* file,
    NvU32 line,
    const char* condition)
{
    if (file != NULL)
    {
        NvOsDebugPrintf("\n\nAssert on %s:%d: %s\n", file, line, (condition) ?
            condition : " " );

        NvOsDebugCallstack(1);
    }

#if NVCPU_IS_X86
    __asm__("int $3");
#elif NVCPU_IS_ARM
#ifdef __ARM_EABI__
    /* Standard breakpoint idiom for arm-eabi-linux. These byte sequences are
     * _guaranteed_ to be an invalid instruction which gets correctly
     * recognized and trapped by Linux kernel. */
#ifdef __thumb__
    __asm__ __volatile(".word 0xde01");
#else
    __asm__ __volatile(".long 0xe7f001f0");
#endif
#else
    /* BKPT only works for arm-unknown-linux, the traditional ARM ABI. */
    __asm__ __volatile("bkpt #3"); // immediate is ignored
#endif
#else
#error "No asm implementation of NvOsBreakPoint yet for this CPU"
#endif
}

typedef struct NvOsFileRec
{
    int fd;
    NvOsFileType type;
} NvOsFile;

static NvError NvOsVdprintf(NvOsFileHandle stream, const char *format, va_list ap, int *n)
{
    char *string;
    int string_len;
    NvError e = NvSuccess;

    string_len = vasprintf(&string, format, ap);
    if (string_len < 0)
        return NvError_InsufficientMemory;
    e = NvOsFwriteInternal(stream, string, string_len);
    if (n)
        *n = string_len;
    free(string);
    return e;
}

NvError NvOsFprintf(NvOsFileHandle stream, const char *format, ...)
{
    int n = 0;
    va_list ap;
    NvError e = NvSuccess;

    va_start( ap, format );
    e = NvOsVdprintf(stream, format, ap, &n);
    va_end( ap );

    if (e != NvSuccess)
        return e;

    if( n > 0 )
    {
        return NvSuccess;
    }
    else
    {
        return NvError_FileWriteFailed;
    }
}

NvS32 NvOsSnprintf( char *str, size_t size, const char *format, ... )
{
    int n;
    va_list ap;

    va_start( ap, format );
    n = vsnprintf( str, size, format, ap );
    va_end( ap );

    return n;
}

NvError NvOsVfprintf( NvOsFileHandle stream, const char *format, va_list ap )
{
    int n;
    NvError e = NvSuccess;

    e = NvOsVdprintf(stream, format, ap, &n);
    if (e != NvSuccess)
        return e;

    if( n > 0 )
    {
        return NvSuccess;
    }
    else
    {
        return NvError_FileWriteFailed;
    }
}

NvS32 NvOsVsnprintf( char *str, size_t size, const char *format, va_list ap )
{
    int n;

    n = vsnprintf( str, size, format, ap );
    return n;
}

#define NVOS_LOG_DEBUG_STRING 0
static void NvOsDebugStringLog(const char *str)
{
    if (NVOS_LOG_DEBUG_STRING)
    {
        static int fd = -1;
        if (fd<0)
        {
            fd = open(
                    "/data/tegralog.txt",
                    O_WRONLY|O_APPEND|O_CREAT|O_SYNC,
                    S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
        }
        if (fd>=0)
        {
            const char *s = str;
            size_t len = NvOsStrlen(s);
            ssize_t l;
            while(len)
            {
                errno=0;
                l = write(fd,s,len);
                if (l<=0)
                {
                    int e = errno;
                    if (e==EAGAIN || e==EINTR)
                        continue;
                    break;
                }
                s += l;
                len = (size_t)(len - l);
            }
            fsync(fd);
        }
    }
}


#define NVOS_DEBUG_SYSFS_DCC  0
static void NvOsDebugSysfsDcc(const char *str)
{
    if (NVOS_DEBUG_SYSFS_DCC)
    {
        static int fd = -1;
        if (fd<0)
        {
            fd = open("/sys/kernel/dcc/dcc0", O_WRONLY, S_IWUSR|S_IWGRP);

            /* If DCC open fails, don't bother trying again */
            if (fd<0) {
                    fd = 0;
                    NvOsDebugPrintf("nvos: dcc0 open failed: %d\n", errno);
            }
        }
        if (fd>0)
        {
            const char *s = str;
            size_t len = NvOsStrlen(s);
            ssize_t l;
            while(len)
            {
                errno=0;
                l = write(fd,s,len);
                if (l<=0)
                {
                    int e = errno;
                    if (e==EAGAIN || e==EINTR)
                        continue;
                    break;
                }
                s += l;
                len = (size_t)(len - l);
            }
        }
    }
}

void NvOsDebugString(const char *str)
{
    NvOs_StaticInit();
    if (NVOS_LOG_DEBUG_STRING)
        NvOsDebugStringLog(str);

    if (NVOS_DEBUG_SYSFS_DCC)
        NvOsDebugSysfsDcc(str);

    if (g_NvOsKernel && g_NvOsKernel->nvOsDebugString)
        g_NvOsKernel->nvOsDebugString(str);

    fprintf(stderr,"%s", str);
    fflush(stderr);
}

static NvS32 NvOsDebugVprintfPvt (const char *format, va_list ap)
{
    NvS32 n;
    NvOs_StaticInit();
    if ((g_NvOsKernel && g_NvOsKernel->nvOsDebugString) ||
        NVOS_LOG_DEBUG_STRING || NVOS_DEBUG_SYSFS_DCC)
    {
        char buf[1024];
        n = vsnprintf(buf, sizeof(buf), format, ap);
        buf[sizeof(buf)-1] = 0;
        NvOsDebugString(buf);
    }
    else
    {
        n = vfprintf(stderr, format, ap);
        fflush(stderr);
    }
    return n;
}

void NvOsDebugPrintf( const char *format, ... )
{
    va_list ap;

    va_start( ap, format );
    NvOsDebugVprintfPvt( format, ap );
    va_end( ap );
}

void NvOsDebugVprintf( const char *format, va_list ap )
{
    NvOsDebugVprintfPvt( format, ap );
}

NvS32 NvOsDebugNprintf( const char *format, ...)
{
    va_list ap;
    int n;

    va_start( ap, format );
    n = NvOsDebugVprintfPvt( format, ap );
    va_end(ap);

    return n;
}

void NvOsStrcpy( char *dest, const char *src )
{
    NV_ASSERT( dest );
    NV_ASSERT( src );

    (void)strcpy( dest, src );
}

void NvOsStrncpy( char *dest, const char *src, size_t size )
{
    NV_ASSERT( dest );
    NV_ASSERT( src );

    (void)strncpy( dest, src, size );
}

size_t NvOsStrlen( const char *s )
{
    NV_ASSERT( s );
    return strlen( s );
}

NvOsCodePage NvOsStrGetSystemCodePage(void)
{
    return NvOsCodePage_Windows1252;
}

int NvOsStrcmp( const char *s1, const char *s2 )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return strcmp( s1, s2 );
}



int NvOsStrncmp( const char *s1, const char *s2, size_t size )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return strncmp( s1, s2, size );
}

void NvOsMemcpy(void *dest, const void *src, size_t size)
{
    (void)memcpy(dest, src, size);
}

int NvOsMemcmp( const void *s1, const void *s2, size_t size )
{
    NV_ASSERT( s1 );
    NV_ASSERT( s2 );

    return memcmp( s1, s2, size );
}

void NvOsMemset( void *s, NvU8 c, size_t size )
{
    NV_ASSERT( s );

    (void)memset( s, (int)c, size );
}

void NvOsMemmove( void *dest, const void *src, size_t size )
{
    NV_ASSERT( dest );
    NV_ASSERT( src );

    (void)memmove( dest, src, size );
}

NvError NvOsCopyIn( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError NvOsCopyOut( void *pDst, const void *pSrc, size_t Bytes )
{
    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError NvOsFopenInternal(
    const char *path,
    NvU32 flags,
    NvOsFileHandle *file )
{
    NvError e = NvError_BadParameter;
    NvOsFile *f;
    int mode;
    int cflags = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    NvOsStatType st;

    NV_ASSERT( path );
    NV_ASSERT( file );

    switch( flags )
    {
        case NVOS_OPEN_READ:
            mode = O_RDONLY | O_LARGEFILE;
            break;
        case NVOS_OPEN_WRITE:
        case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE:
            mode = O_CREAT | O_WRONLY | O_TRUNC | O_LARGEFILE;
            break;
        case NVOS_OPEN_READ | NVOS_OPEN_WRITE:
            mode = O_RDWR | O_LARGEFILE;
            break;
        case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE:
            mode = O_CREAT | O_RDWR | O_TRUNC | O_LARGEFILE;
            break;
        case NVOS_OPEN_APPEND:
        case NVOS_OPEN_CREATE | NVOS_OPEN_APPEND:
        case NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
            mode = O_CREAT | O_WRONLY | O_APPEND | O_LARGEFILE;
            break;
        case NVOS_OPEN_READ | NVOS_OPEN_APPEND:
        case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_APPEND:
        case NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
        case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE | NVOS_OPEN_APPEND:
            mode = O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE;
            break;
        default:
            return NvError_BadParameter;
    }

    f = NvOsAlloc( sizeof(NvOsFile) );
    if( !f )
    {
        return NvError_InsufficientMemory;
    }

    errno = 0;
    f->fd = open(path, mode, cflags);
    if (f->fd < 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
        goto fail;
    }

    e = NvOsFstatInternal( f, &st );
    if( e != NvSuccess )
    {
        goto fail;
    }

    f->type = st.type;

    *file = f;
    return NvSuccess;

fail:
    NvOsFree( f );
    return e;
}

void NvOsFcloseInternal(NvOsFileHandle stream)
{
    if (!stream)
        return;
    if (close(stream->fd))
        NV_ASSERT(!"NvOsFcloseInternal: close() failed");

    NvOsFree( stream );
}

NvError NvOsFwriteInternal(
    NvOsFileHandle stream,
    const void *ptr,
    size_t size)
{
    NvError e = NvSuccess;
    char *p;
    ssize_t len;
    size_t s;

    NV_ASSERT(stream && ptr);

    if (size == 0)
        return e;

#if USE_SELECT_FOR_FIFOS
    if (stream->type == NvOsFileType_Fifo)
    {
        fd_set block;
        int ready;
        FD_ZERO(&block);
        FD_SET(stream->fd, &block);
        ready = select(stream->fd+1, NULL, &block, NULL, NULL);
        if (ready <=0 )
            return NvError_FileWriteFailed;
    }
#endif

    s = size;
    p = (char *)ptr;
    do
    {
        len = write(stream->fd, p, s);
        if (len > 0)
        {
            p += len;
            s -= len;
        }
    } while ((len < 0 && (errno == EINTR)) || (s > 0 && len > 0));

    if (len < 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileWriteFailed;
        return e;
    }
    if (len && stream->type == NvOsFileType_Fifo)
    {
        NvError s = NvOsFflushInternal(stream);
        if (e==NvSuccess)
            e = s;
    }
    return e;
}

NvError NvOsFreadInternal(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec)
{
    NvError e = NvSuccess;
    char *p;
    ssize_t len;
    size_t s;

    NV_ASSERT(stream && ptr);

    if (size == 0)
    {
        if (bytes)
            *bytes = 0;
        return e;
    }
    if (size > SSIZE_MAX)
        return NvError_BadValue;

#if USE_SELECT_FOR_FIFOS
    if (stream->type == NvOsFileType_Fifo)
    {
        fd_set block;
        int ready;
        FD_ZERO(&block);
        FD_SET(stream->fd, &block);
        ready = select(stream->fd+1, &block, NULL, NULL, NULL);
        if (ready <=0 )
            return NvError_FileReadFailed;
    }
#endif

    s = size;
    p = (char *)ptr;

    do
    {
        len = read(stream->fd, p, s);
        if (len > 0)
        {
            p += len;
            s -= len;
        }
    } while ((len < 0 && (errno == EINTR)) || (s > 0 && len > 0));

    if (len < 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileReadFailed;
        return e;
    }

    if (bytes)
        *bytes = size-s;

    if (len == 0)
    {
        e = NvError_EndOfFile;
    }
    return e;
}

NvError NvOsFseekInternal(
    NvOsFileHandle hFile,
    NvS64 offset,
    NvOsSeekEnum whence)
{
    int w;
    loff_t off;
    NvError e = NvSuccess;

    NV_ASSERT( hFile );

    switch( whence ) {
    case NvOsSeek_Set: w = SEEK_SET; break;
    case NvOsSeek_Cur: w = SEEK_CUR; break;
    case NvOsSeek_End: w = SEEK_END; break;
    default:
        return NvError_BadParameter;
    }

    off = lseek64( hFile->fd, (loff_t)offset, w );
    if( off < 0 )
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }
    return e;
}

NvError NvOsFtellInternal(NvOsFileHandle hFile, NvU64 *position)
{
    loff_t offset;
    NvError e = NvSuccess;

    NV_ASSERT( hFile );
    NV_ASSERT( position );

    offset = lseek64( hFile->fd, (loff_t)0ULL, SEEK_CUR );
    if( offset < 0 )
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            return NvError_FileOperationFailed;
    }
    *position = (NvU64)offset;
    return e;
}

NvError NvOsFstatInternal(NvOsFileHandle hFile, NvOsStatType *s)
{
    struct stat64 fs;
    int fd;
    int err;

    NV_ASSERT( hFile );
    NV_ASSERT( s );

    fd = hFile->fd;

    err = fstat64( fd, &fs );
    if( err != 0 )
    {
        return NvError_FileOperationFailed;
    }

    s->size = (NvU64)fs.st_size;
    s->mtime = (NvU64)fs.st_mtime;

    if( S_ISREG( fs.st_mode ) )
    {
        s->type = NvOsFileType_File;
    }
    else if( S_ISDIR( fs.st_mode ) )
    {
        s->type = NvOsFileType_Directory;
    }
    else if( S_ISFIFO( fs.st_mode ) )
    {
        s->type = NvOsFileType_Fifo;
    }
    else if( S_ISCHR( fs.st_mode ) )
    {
        s->type = NvOsFileType_CharacterDevice;
    }
    else if( S_ISBLK( fs.st_mode ) )
    {
        s->type = NvOsFileType_BlockDevice;
    }
    else
    {
        s->type = NvOsFileType_Unknown;
    }

    return NvSuccess;
}

NvError NvOsFflushInternal(NvOsFileHandle stream)
{
    return NvOsFsyncInternal(stream);
}

NvError NvOsFsyncInternal(NvOsFileHandle stream)
{
    NvError e = NvSuccess;
    int err;

    NV_ASSERT(stream);

    err = fsync(stream->fd);
    if (err != 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    return e;
}

NvError NvOsFremoveInternal(const char *filename)
{

    NvError e = NvSuccess;
    int err;

    NV_ASSERT(filename);

    err = unlink(filename);
    if (err != 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }
    return e;


}

NvError NvOsFlockInternal(NvOsFileHandle stream, NvOsFlockType type)
{
    NvError e = NvSuccess;
    int err;
    struct flock lock;

    memset(&lock, 0, sizeof(lock));

    switch (type)
    {
        case NvOsFlockType_Shared:
            lock.l_type = F_RDLCK;
            break;
        case NvOsFlockType_Exclusive:
            lock.l_type = F_WRLCK;
            break;
        case NvOsFlockType_Unlock:
            lock.l_type = F_UNLCK;
            break;
        default:
            break;
    }

    NV_ASSERT(stream);

    err = fcntl(stream->fd, F_SETLK, &lock);
    if (err != 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    return e;
}

NvError NvOsFtruncateInternal(NvOsFileHandle stream, NvU64 length)
{
    NvError e = NvSuccess;
    int err;

    NV_ASSERT(stream);

    err = ftruncate(stream->fd, (off_t) length);
    if (err != 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    return e;
}

NvError NvOsIoctl(
    NvOsFileHandle hFile,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize )
{
    int fd;
    int e;
    NvOsIoctlParams p;

    if ( hFile == NULL ) {
        NV_DEBUG_PRINTF(( "NvOsIoctl: no kernel driver\n" ));
        return NvError_KernelDriverNotFound;
    }

    NV_ASSERT( hFile->type == NvOsFileType_CharacterDevice ||
        hFile->type == NvOsFileType_BlockDevice );

    fd = hFile->fd;
    if( fd <= 0 )
    {
        NV_DEBUG_PRINTF(( "NvOsIoctl: no kernel driver\n" ));
        return NvError_KernelDriverNotFound;
    }

    p.IoctlCode = IoctlCode;
    p.pBuffer = pBuffer;
    p.InBufferSize = InBufferSize;
    p.InOutBufferSize = InOutBufferSize;
    p.OutBufferSize = OutBufferSize;

    e = ioctl( fd, (int)IoctlCode, &p );
    if( e != 0 )
    {
        NV_DEBUG_PRINTF(( "NvOsIoctl: %s\n", strerror(errno) ));
        return NvError_IoctlFailed;
    }

    return NvSuccess;
}

NvError NvOsOpendirInternal(const char *path, NvOsDirHandle *dir)
{
    DIR *d;

    NV_ASSERT( path );
    NV_ASSERT( dir );

    d = opendir( path );
    if( d == NULL )
    {
        return NvError_DirOperationFailed;
    }

    *dir = (NvOsDirHandle)d;

    return NvSuccess;
}

NvError NvOsReaddirInternal(NvOsDirHandle dir, char *name, size_t size)
{
    struct dirent *d;

    NV_ASSERT( (DIR *)dir );
    NV_ASSERT( name );

    d = readdir( (DIR *)dir );
    if( d == NULL )
    {
        /* there doesn't seem to be a way to distinguish between end of file
           and a real error.
         */
        return NvError_EndOfDirList;
    }

    (void)strncpy( name, d->d_name, size );
    name[size-1] = '\0';

    return NvSuccess;
}

void NvOsClosedirInternal(NvOsDirHandle dir)
{
    if (!dir)
        return;
    (void)closedir((DIR *)dir);
}

NvError NvOsMkdirInternal(char *dirname)
{
    NvError e = NvSuccess;
    int err;

    err = mkdir(dirname, S_IRWXU);
    if (err != 0)
    {
        if (!NvOsLinuxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    return e;
}

NvError NvOsSetConfigU32Internal(const char *name, NvU32 value)
{
    // let string variant handle this
    return NvError_NotImplemented;
}

NvError NvOsGetConfigU32Internal(const char *name, NvU32* value)
{
    // let string variant handle this
    return NvError_NotImplemented;
}

#define TEGRA_PROP_PREFIX           "tegra."
#define SYS_PROP_PREFIX             "sys."
#define PERSIST_PREFIX              "persist."

/*
 *  Write a configuration string to storage.
 *  Only persistent Android properties are used.
 */
static NvError NvOsSetConfigStringCommon(const char* prefix,
                                         const char *name,
                                         const char *value)
{
#if !defined(ANDROID)
    return NvError_NotSupported;
#else
    char prefixedName[PROPERTY_KEY_MAX];
    int written;
    int ret;

    written = NvOsSnprintf(prefixedName,
                           PROPERTY_KEY_MAX,
                           "%s%s%s",
                           PERSIST_PREFIX,
                           prefix,
                           name);
    if (written == -1 || written >= PROPERTY_KEY_MAX)
        return NvError_InvalidSize;

    ret = property_set(prefixedName, value);

    // Sad but true: the Android property_set implementation isn't
    // synchronous. Alleviate the problem by waiting a bit here.
    // Remove this once Android is fixed.
    if (!ret)
        NvOsSleepMSInternal(5);

    /* TODO: [ahatala 2010-03-04] better error codes */
    return (ret) ? NvError_InvalidConfigVar : NvSuccess;
#endif
}

NvError NvOsSetConfigStringInternal(const char *name, const char *value)
{
    return NvOsSetConfigStringCommon(TEGRA_PROP_PREFIX, name, value);
}

NvError NvOsSetSysConfigStringInternal(const char *name, const char *value)
{
    return NvOsSetConfigStringCommon(SYS_PROP_PREFIX, name, value);
}

/*
 *  Query a configuration string from available storage locations.
 *  In decreasing order of precedence:
 *  - Environment variable
 *  - Android property (volatile)
 *  - Android property (persistent)
 */
static NvError NvOsGetConfigStringCommon(const char* prefix,
                                         const char *name,
                                         char *value,
                                         NvU32 size)
{
    const char* str = NULL;

    str = getenv(name);

#if defined(ANDROID)
    if (!str)
    {
        char prefixedName[PROPERTY_KEY_MAX];
        char buf[PROPERTY_VALUE_MAX];
        int written;

        written = NvOsSnprintf(prefixedName,
                               PROPERTY_KEY_MAX,
                               "%s%s",
                               prefix,
                               name);
        if (written != -1 && written < PROPERTY_KEY_MAX)
            if (property_get(prefixedName, buf, NULL) > 0)
                str = buf;

        if (!str)
        {
            written = NvOsSnprintf(prefixedName,
                                   PROPERTY_KEY_MAX,
                                   "%s%s%s",
                                   PERSIST_PREFIX,
                                   prefix,
                                   name);
            if (written != -1 && written < PROPERTY_KEY_MAX)
                if (property_get(prefixedName, buf, NULL) > 0)
                    str = buf;
        }
    }

#endif

    if (!str)
        return NvError_ConfigVarNotFound;

    (void)strncpy(value, str, size);
    value[size-1] = '\0';
    return NvSuccess;
}

NvError NvOsGetConfigStringInternal(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigStringCommon(TEGRA_PROP_PREFIX, name, value, size);
}

NvError NvOsGetSysConfigStringInternal(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigStringCommon(SYS_PROP_PREFIX, name, value, size);
}

void *NvOsAllocInternal(size_t size)
{
    return malloc(size);
}

void *NvOsReallocInternal(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void NvOsFreeInternal(void *ptr)
{
    if (!ptr)
        return;

    // ok to free null
    free(ptr);
}

void *NvOsExecAlloc(size_t size)
{
    static int s_zerofd = -1;
    void *ptr;

    if( s_zerofd == -1 )
    {
        s_zerofd = open( "/dev/zero", O_RDWR );
        if( s_zerofd == -1 )
        {
            return 0;
        }
    }

    ptr = mmap( NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE,
        s_zerofd, 0 );
    if( ptr == MAP_FAILED )
    {
        return 0;
    }

    return ptr;
}

void NvOsExecFree( void *ptr, size_t size )
{
    if( !ptr )
    {
        return;
    }

    munmap( ptr, size );
}

/* NvOsSharedMemAlloc() requires librt, and is in nvos_linux_librt.c */

NvError NvOsSharedMemHandleFromFd(
    int fd,
    NvOsSharedMemHandle *descriptor)
{
    NV_ASSERT(fd >= 0);
    NV_ASSERT(descriptor != NULL);

    if (descriptor == NULL)
        return NvError_BadParameter;

    *descriptor = (NvOsSharedMemHandle)(intptr_t)dup(fd);
    if (*descriptor == (NvOsSharedMemHandle)-1)
        return NvError_FileOperationFailed;

    return NvSuccess;
}

NvError NvOsSharedMemGetFd(
    NvOsSharedMemHandle descriptor,
    int *fd)
{
    NV_ASSERT(fd != NULL);

    if (fd == NULL)
        return NvError_BadParameter;

    *fd = dup((int)(intptr_t)descriptor);
    if (*fd == -1)
        return NvError_FileOperationFailed;

    return NvSuccess;
}

NvError NvOsSharedMemMap(
    NvOsSharedMemHandle descriptor,
    size_t offset,
    size_t size,
    void **ptr)
{
    void *region;
    NvError ret = NvError_MemoryMapFailed;

    int fd = (int)(intptr_t)descriptor;
    if(fd < 0 || ptr == NULL) {
        NV_ASSERT(0);
        return ret;
    }

#if !defined(ANDROID)
    /* TODO: why are these different? */
    region = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset);
    if (region == MAP_FAILED)
        return ret;

    *ptr = region;
#else // #if !defined(ANDROID)
    region = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(region == MAP_FAILED)
        return ret;

    *ptr = ((NvU8 *)region) + offset;
#endif
    return NvSuccess;
}

void NvOsSharedMemUnmap(void *ptr, size_t size)
{
    if (!ptr)
    {
        NV_ASSERT(ptr); // raise an assertion if someone tried to unmap a null pointer
        return;
    }
    if (munmap(ptr, size))
        NV_ASSERT(!"NvOsSharedMemUnmap: munmap() failed");
}

void NvOsSharedMemFree(NvOsSharedMemHandle descriptor)
{
    int fd = (int)(intptr_t)descriptor;
    if (fd < 0)
    {
        NV_ASSERT(0);
        return;
    }
    if (close(fd))
        NV_ASSERT(!"NvOsSharedMemFree: close() failed");
}

NvError NvOsLinuxPhysicalMemMapFd(
    int fd,
    NvOsPhysAddr     phys,
    size_t           size,
    NvOsMemAttribute attrib,
    NvU32            flags,
    void           **ptr)
{
    void *addr;
    int prot = PROT_NONE;
    int page_size;
    int page_offset;

    if( flags & NVOS_MEM_READ ) prot |= PROT_READ;
    if( flags & NVOS_MEM_WRITE ) prot |= PROT_WRITE;
    if( flags & NVOS_MEM_EXECUTE ) prot |= PROT_EXEC;

    page_size = getpagesize();
    page_offset = phys & ( page_size - 1 );
    phys = ( phys ) & ~( page_size - 1 );
    size += page_offset;
    size = (size + (page_size - 1)) & ~(page_size - 1);

    addr = mmap(0, size, prot, MAP_SHARED, fd, (off_t)phys);
    if( addr == MAP_FAILED )
        return NvError_MemoryMapFailed;

    // Need to verify unmap assumption -- virtual address is page aligned
    NV_ASSERT( !( ((NvUPtr)addr) & ( page_size - 1 ) ) );

    *ptr = (void *)( (NvU8 *)addr + page_offset );
    return NvSuccess;
}

NvError NvOsPhysicalMemMap(
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr)
{
    if (!ptr || !size)
        return NvError_BadParameter;

    if( flags == NVOS_MEM_NONE )
    {
        void *addr;
        int fd = 0;
        /* no access permissions - map /dev/zero */
        fd = open( "/dev/zero", O_RDWR );
        if (fd < 0)
            return NvError_MemoryMapFailed;

        /* map with no perms */
        addr = mmap( 0, size, PROT_NONE, MAP_PRIVATE, fd, 0 );
        (void)close(fd);

        if( addr == MAP_FAILED )
            return NvError_MemoryMapFailed;

        *ptr = addr;
        return NvSuccess;
    }

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsPhysicalMemMap)
        return g_NvOsKernel->nvOsPhysicalMemMap(phys, size, attrib, flags, ptr);

    return NvError_KernelDriverNotFound;
}

void NvOsPhysicalMemUnmap(void *ptr, size_t size)
{
    int page_size;
    int page_offset;

    if (!ptr)
        return;

    page_size = getpagesize();
    page_offset = (NvUPtr)ptr & (page_size - 1);

    // Round down
    ptr = (void *)((NvUPtr)ptr & ~(page_size - 1));
    size = size + page_offset;
    size = (size + page_size-1) & ~(page_size-1);

    (void)munmap(ptr, size);
}

NvError NvOsPageAlloc(
    size_t size,
    NvOsMemAttribute attrib,
    NvOsPageFlags flags,
    NvU32 protect,
    NvOsPageAllocHandle *descriptor )
{

    return NvError_NotImplemented;
}

void NvOsPageFree( NvOsPageAllocHandle descriptor )
{

}

NvError NvOsPageLock(
    void *ptr,
    size_t size,
    NvU32 protect,
    NvOsPageAllocHandle *descriptor)
{
    return NvError_NotImplemented;
}

NvError NvOsPageMap(
    NvOsPageAllocHandle descriptor,
    size_t offset,
    size_t size,
    void **ptr )
{

    return NvError_NotImplemented;
}

NvError NvOsPageMapIntoPtr(
    NvOsPageAllocHandle descriptor,
    void *pCallerPtr,
    size_t offset,
    size_t size)
{
    return NvError_NotImplemented;
}

void NvOsPageUnmap(
    NvOsPageAllocHandle descriptor,
    void *ptr,
    size_t size)
{

}

NvOsPhysAddr NvOsPageAddress(NvOsPageAllocHandle descriptor, size_t offset)
{
    return 0;
}

NvError NvOsLibraryLoad(const char *name, NvOsLibraryHandle *library)
{
#ifndef BUILD_NO_DL_PROFILE
    void *lib;
    size_t len;
    char buff[ NAME_MAX ];
    const char *n, *extpos;

    NV_ASSERT( name );
    NV_ASSERT( library );

    len = strlen( name );

    extpos = strstr( (const char *)name, ".so" );
    if( extpos )
    {
        /* library name contains ".so" */
        n = name;
    }
    else
    {
        /* library name does not contain ".so", try adding */
        if ( len + 4 > NAME_MAX )
        {
            /* don't have enough space to add ".so" */
            return NvError_BadParameter;
        }

        /* buff = name ; buff += ".so" ; */
        strcpy( buff, name );
        /* better than doing: strcat( buff, ".so" ); */
        strcpy( len + buff, ".so" );

        len += 3;
        n = buff;
    }

    lib = dlopen( n, RTLD_LAZY );

#ifdef BUILD_VERSION
    if( lib == 0 )
    {
        if ( n == name )
        {
            if ( 0 == *( extpos + 3 ) )
            {
                /* ".so" is located at the very end, try adding version # */
                if ( len + strlen( BUILD_VERSION ) + 2 > NAME_MAX )
                {
                    /* don't have enough space to add version # */
                    return NvError_BadParameter;
                }
                /* buff = name ; buff += "." BUILD_VERSION ; */
                strcpy( buff, name );
                /* better than doing: strcat( buf, "." BUILD_VERSION ); */
                strcpy( len + buff, "." BUILD_VERSION );
                n = buff;
            }
            /* else name already contains some version # */
        }
        else
        {
            if ( len + strlen( BUILD_VERSION ) + 2 > NAME_MAX )
            {
                /* don't have enough space to add version # */
                return NvError_BadParameter;
            }
            /* buff += "." BUILD_VERSION ; */
            /* better than doing: strcat(buf, "."BUILD_VERSION); */
            strcpy( len + buff, "." BUILD_VERSION );
        }

        if ( n != name )
        {
            lib = dlopen( n, RTLD_LAZY );
        }
    }
#endif

    if( lib == 0 )
    {
#if NV_DEBUG==1
        printf("NvOsLibraryLoad: %s: %s\n", n, dlerror());
#endif
        return NvError_LibraryNotFound;
    }

    *library = (NvOsLibraryHandle)lib;
    return NvSuccess;
#else
    return NvError_NotSupported;
#endif
}

void* NvOsLibraryGetSymbol(NvOsLibraryHandle library, const char *symbol)
{
#ifndef BUILD_NO_DL_PROFILE
    void *addr;

    NV_ASSERT( (void*)library );
    NV_ASSERT( symbol );

    addr = dlsym( (void*)library, symbol );

    return addr;

#else
    return NULL;
#endif
}

void NvOsLibraryUnload(NvOsLibraryHandle library)
{
    if (!library)
        return;
#ifndef BUILD_NO_DL_PROFILE
    (void)dlclose( (void*)library );
#endif
}

void NvOsSleepMSInternal(NvU32 msec)
{
    struct timespec ts;
    int res;

    if (g_UseCoopThread)
    {
        return;
    }

    /* break msec into seconds and nanoseconds */
    ts.tv_sec = msec / 1000;
    ts.tv_nsec = ( msec % 1000 ) * 1000000;
    /* loop until time is used up */
    while ( ( res = nanosleep(&ts, &ts) ) ) {
        if (ts.tv_sec == 0 && ts.tv_nsec == 0 )
            break;
        if (errno != EINTR)
        {
            NvOsDebugPrintf("\n\nNvOsSleepMSInternal() failure:%s\n", strerror(errno));
            return;
        }
    }
}

/* This API is used in cases where there sw is used to manually generate
 * clock a gpio line and wait time is small.
 */
void NvOsWaitUS(NvU32 usec)
{
    struct timespec ts;
    int res;

    if (g_UseCoopThread)
    {
        return;
    }

    /* break usec into seconds and nanoseconds */
    ts.tv_sec = usec / 1000000;
    ts.tv_nsec = ( usec % 1000000 ) * 1000;
    /* loop until time is used up */
    while ( ( res = nanosleep(&ts, &ts) ) ) {
        if (ts.tv_sec == 0 && ts.tv_nsec == 0 )
            break;
        if (errno != EINTR)
        {
            NvOsDebugPrintf("\n\nNvOsWaitUS() failure:%s\n", strerror(errno));
            return;
        }
    }
}

typedef struct NvOsMutexRec
{
    pthread_mutex_t    mutex;
    volatile pthread_t owner;
    NvU32              count;
} NvOsMutex;

NvError NvOsMutexCreateInternal(NvOsMutexHandle *mutex)
{
    NvOsMutex *m = 0;
    NvError ret = NvSuccess;

    NV_ASSERT( mutex );

    m = NvOsAlloc( sizeof(NvOsMutex) );
    if( !m )
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }

    /* create a process-local mutex (pthread) */

    (void)pthread_mutex_init( &m->mutex, 0 );
    m->count = 0;
    m->owner = (pthread_t)-1;

    *mutex = m;
    return NvSuccess;

fail:
    *mutex = 0;
    NvOsFree( m );
    return ret;
}

/** NvOs_LocalMutexLock - recursive lock using pthreads.
 */
static void NvOs_LocalMutexLock( NvOsMutex *m )
{
    NV_ASSERT( m );
    if ( !m ) return;

    if (m->owner == pthread_self())
    {
        m->count++;
    }
    else
    {
        pthread_mutex_lock(&m->mutex);
        NV_ASSERT((m->owner == (pthread_t)-1) && (m->count == 0));
        m->owner = pthread_self();
        m->count = 1;
    }
}

void NvOsMutexLockInternal( NvOsMutexHandle m )
{
    /* local recursive mutex lock */
    NvOs_LocalMutexLock( m );
}

/** NvOs_LocalMutexUnlock - recursive unlock using pthreads.
 */
static void NvOs_LocalMutexUnlock( NvOsMutex *m )
{
    NV_ASSERT( m );
    if ( !m ) return;

    if (m->owner == pthread_self())
    {
        m->count--;
        if (!m->count)
        {
            m->owner = (pthread_t)-1;
            pthread_mutex_unlock(&m->mutex);
        }
    }
    else
    {
        /* not the owner */
        NV_ASSERT( 0 && "illegal thread id in unlock" );
    }
}

void NvOsMutexUnlockInternal( NvOsMutexHandle m )
{
    /* local recursive unlock */
    NvOs_LocalMutexUnlock( m );
}

void NvOsMutexDestroyInternal( NvOsMutexHandle m )
{
    NV_ASSERT( m );
    if (!m)
        return;

    //Local mutex destroy
    (void)pthread_mutex_destroy( &m->mutex);
    NvOsFree(m);
}

typedef struct NvOsIntrMutexRec
{
    pthread_mutex_t mutex;
} NvOsIntrMutex;

NvError NvOsIntrMutexCreate( NvOsIntrMutexHandle *mutex )
{
    NvOsIntrMutex *m;

    /* use a standard non-recursive mutex */
    m = NvOsAlloc( sizeof(NvOsIntrMutex) );
    if( !m )
    {
        return NvError_InsufficientMemory;
    }

    (void)pthread_mutex_init( &m->mutex, 0 );

    *mutex = m;
    return NvSuccess;
}

void NvOsIntrMutexLock( NvOsIntrMutexHandle mutex )
{
    NV_ASSERT( mutex );
    pthread_mutex_lock( &mutex->mutex );
}

void NvOsIntrMutexUnlock( NvOsIntrMutexHandle mutex )
{
    NV_ASSERT( mutex );
    pthread_mutex_unlock( &mutex->mutex );
}

void NvOsIntrMutexDestroy(NvOsIntrMutexHandle mutex)
{
    if (!mutex)
        return;

    (void)pthread_mutex_destroy( &mutex->mutex );
    NvOsFree( mutex );
}

typedef struct NvOsConditionRec
{
    pthread_cond_t cond;
} NvOsCondition;

NvError NvOsConditionCreate(NvOsConditionHandle *cond)
{
    NvOsCondition *c;
    int ret;

    NV_ASSERT(cond);

    c = NvOsAlloc(sizeof(NvOsCondition));
    if (!c)
        return NvError_InsufficientMemory;

    ret = pthread_cond_init(&c->cond, 0);

    if (ret == 0)
    {
        *cond = c;
        return NvSuccess;
    }
    else
    {
        NvOsFree(c);
        return NvError_Busy;
    }
}

NvError NvOsConditionDestroy(NvOsConditionHandle cond)
{
    int ret;

    if (!cond)
        return NvError_BadParameter;

    ret = pthread_cond_destroy(&cond->cond);
    if (ret == 0)
    {
        NvOsFree(cond);
        return NvSuccess;
    }
    return NvError_Busy;
}

NvError NvOsConditionBroadcast(NvOsConditionHandle cond)
{
    if (!cond)
        return NvError_BadParameter;

    pthread_cond_broadcast(&cond->cond);
    return NvSuccess;
}

NvError NvOsConditionSignal(NvOsConditionHandle cond)
{
    if (!cond)
        return NvError_BadParameter;

    pthread_cond_signal(&cond->cond);
    return NvSuccess;
}

NvError NvOsConditionWait(NvOsConditionHandle cond, NvOsMutexHandle mutex)
{
    int ret;

    if (!cond || !mutex)
        return NvError_BadParameter;

    // Ensure that mutex is locked only once in the current thread
    if (mutex->count != 1 || mutex->owner != pthread_self())
        return NvError_AccessDenied;

    mutex->count = 0;
    mutex->owner = (pthread_t)-1;

    ret = pthread_cond_wait(&cond->cond, &mutex->mutex);
    switch (ret)
    {
        case 0:
            NV_ASSERT((mutex->owner == (pthread_t)-1) && (mutex->count == 0));
            mutex->owner = pthread_self();
            mutex->count = 1;
            return NvSuccess;
        case EPERM:
            return NvError_AccessDenied;
        default:
            return NvError_BadParameter;
    }
}

NvError NvOsConditionWaitTimeout(NvOsConditionHandle cond, NvOsMutexHandle mutex, NvU32 microsecs)
{
    NvU64 time;
    struct timespec ts;
    int ret;

    if (!cond || !mutex)
        return NvError_BadParameter;

    // Ensure that mutex is locked only once in the current thread
    if (mutex->count != 1 || mutex->owner != pthread_self())
        return NvError_AccessDenied;

    mutex->count = 0;
    mutex->owner = (pthread_t)-1;

    time = NvOsGetTimeUS();
    time += microsecs;
    ts.tv_sec = time/1000000ULL;
    ts.tv_nsec = (time - ts.tv_sec * 1000000ULL) * 1000ULL;
#if defined(ANDROID)
    ret = pthread_cond_timedwait_monotonic_np(&cond->cond, &mutex->mutex, &ts);
#else
    ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
#endif
    switch (ret)
    {
        case 0:
            NV_ASSERT((mutex->owner == (pthread_t)-1) && (mutex->count == 0));
            mutex->owner = pthread_self();
            mutex->count = 1;
            return NvSuccess;
        case ETIMEDOUT:
            NV_ASSERT((mutex->owner == (pthread_t)-1) && (mutex->count == 0));
            mutex->owner = pthread_self();
            mutex->count = 1;
            return NvError_Timeout;
        case EPERM:
            return NvError_AccessDenied;
        default:
            return NvError_BadParameter;
    }
}

NvError NvOsSemaphoreCreateInternal(
    NvOsSemaphoreHandle *semaphore,
    NvU32 value)
{
    NV_ASSERT(semaphore);
    if (!semaphore)
        return NvError_BadParameter;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreCreate)
        return g_NvOsKernel->nvOsSemaphoreCreate(semaphore, value);

    return NvError_NotSupported;
}

NvError NvOsSemaphoreCloneInternal(
    NvOsSemaphoreHandle s,
    NvOsSemaphoreHandle *semaphore)
{
    NV_ASSERT(s && semaphore);

    if (!s || !semaphore)
        return NvError_BadParameter;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreClone)
        return g_NvOsKernel->nvOsSemaphoreClone(s, semaphore);

    return NvError_NotSupported;
}

NvError NvOsSemaphoreUnmarshal(
    NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema)
{
    NV_ASSERT(hClientSema && phDriverSema);

    if (!hClientSema || !phDriverSema)
        return NvError_BadParameter;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreUnmarshal)
        return g_NvOsKernel->nvOsSemaphoreUnmarshal(hClientSema, phDriverSema);

    return NvError_NotSupported;
}

void NvOsSemaphoreWaitInternal(NvOsSemaphoreHandle s)
{
    NV_ASSERT(s);

    if (!s)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreWait)
        g_NvOsKernel->nvOsSemaphoreWait(s);
}

NvError NvOsSemaphoreWaitTimeoutInternal(NvOsSemaphoreHandle s, NvU32 msec)
{
    NV_ASSERT( s );

    if (!s)
        return NvError_BadParameter;

    if (msec==NV_WAIT_INFINITE)
    {
        NvOsSemaphoreWaitInternal(s);
        return NvSuccess;
    }

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreWaitTimeout)
        return g_NvOsKernel->nvOsSemaphoreWaitTimeout(s, msec);

    return NvError_NotSupported;
}

void NvOsSemaphoreSignalInternal( NvOsSemaphoreHandle s )
{
    NV_ASSERT(s);

    if (!s)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreSignal)
        g_NvOsKernel->nvOsSemaphoreSignal(s);
}

void NvOsSemaphoreDestroyInternal(NvOsSemaphoreHandle s)
{
    if (!s)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsSemaphoreDestroy)
        g_NvOsKernel->nvOsSemaphoreDestroy(s);
}

#define CONVERT_CPU_NUM_TO_CPU_MASK(cpu_num)  (1 << (cpu_num))

void NvOsThreadSetAffinity(NvU32 CpuHint)
{
#if NVCPU_IS_ARM
    unsigned long mask = 0;
    int tid, ret;
    mask = CONVERT_CPU_NUM_TO_CPU_MASK(CpuHint);
    tid = syscall(__NR_gettid);
    ret = syscall(__NR_sched_setaffinity, tid, sizeof(mask), &mask);
    if(ret <  0)
        return ;
    NV_DEBUG_PRINTF(("__NR_sched_setaffinity for tid = %d, on cpu = %d, ret = %d\n",tid, CpuHint, ret));
    return;
#else
    return;
#endif

}


NvU32 NvOsThreadGetAffinity(void)
{
#if NVCPU_IS_ARM
    unsigned long mask = 0;
    int tid, ret;
    tid = syscall(__NR_gettid);
    ret = syscall(__NR_sched_getaffinity, tid, sizeof(mask), &mask);
    if (ret < 0)
    {
        mask = NVOS_INVALID_CPU_AFFINITY;
        NV_DEBUG_PRINTF(("syscall __NR_sched_getaffinity FAILED \n"));
    }

    return mask;
#else
    return NVOS_INVALID_CPU_AFFINITY;
#endif
}


NvU32 NvOsTlsAllocInternal(void)
{
    pthread_key_t key = 0;
    NV_ASSERT(sizeof(key) <= sizeof(NvU32));
    if (pthread_key_create(&key, NULL))
        return NVOS_INVALID_TLS_INDEX;
    NV_ASSERT(key != (pthread_key_t)NVOS_INVALID_TLS_INDEX);
    return (NvU32)key;
}

void NvOsTlsFreeInternal(NvU32 TlsIndex)
{
    int fail;
    if (TlsIndex == NVOS_INVALID_TLS_INDEX)
        return;
    fail = pthread_key_delete((pthread_key_t)TlsIndex);
    NV_ASSERT(!fail);
    (void)fail;
}

void *NvOsTlsGetInternal(NvU32 TlsIndex)
{
    NV_ASSERT(TlsIndex != NVOS_INVALID_TLS_INDEX);
    return pthread_getspecific((pthread_key_t)TlsIndex);
}

void NvOsTlsSetInternal(NvU32 TlsIndex, void *Value)
{
    int fail;
    fail = pthread_setspecific((pthread_key_t)TlsIndex, Value);
    NV_ASSERT(!fail);
    (void)fail;
}

typedef struct NvOsThreadRec
{
    pthread_t thread;
} NvOsThread;


typedef struct
{
    NvOsThreadFunction function;
    NvOsThread        *thread;
    pthread_mutex_t    barrier;
    void              *thread_args;
    NvS32              init;
} NvOsThreadArgs;

static void *thread_wrapper(void *v)
{
    NvOsThreadArgs* args = (NvOsThreadArgs *)v;

    NV_ASSERT(v);

    /* indicate to the creator that this thread has started, use atomic
     * ops to ensure proper SMP memory barriers */
    (void)NvOsAtomicExchange32(&args->init, 1);
    pthread_mutex_lock(&args->barrier);
    pthread_mutex_unlock(&args->barrier);

    /* jump to user thread */
    args->function(args->thread_args);
    pthread_mutex_destroy(&args->barrier);
    NvOsFree(args);
    return 0;
}

/* To ensure that terminators are run regardless of whether the thread
 * was created using NvOs thread APIs or native thread APIs, the first
 * call to add a terminator callback will create an internal TLS key
 * which stores a linked-list of terminator functions, and an associated
 * destructor for this key which will walk the linked-list and call all
 * of the terminators
 */
static pthread_key_t g_terminator_key = (pthread_key_t)NVOS_INVALID_TLS_INDEX;

typedef struct NvOsTlsTerminatorRec
{
    void (*terminator)(void *);
    void *context;
    struct NvOsTlsTerminatorRec *next;
} NvOsTlsTerminator;

static void run_terminator_list(void *context)
{
    NvOsTlsTerminator *term_list = (NvOsTlsTerminator *)context;
    NvOsTlsTerminator *term;

    while ((term = term_list)!=NULL)
    {
        term_list = term->next;
        term->terminator(term->context);
        NvOsFree(term);
    }
}

NvBool NvOsTlsRemoveTerminatorInternal(void (*func)(void *), void *context)
{
    NvOsTlsTerminator *term;
    NvOsTlsTerminator *term_list;
    NvOsTlsTerminator *term_prev;

    if (g_terminator_key == (pthread_key_t)NVOS_INVALID_TLS_INDEX)
    {
        return NV_FALSE;
    }

    term_list = (NvOsTlsTerminator *)pthread_getspecific(g_terminator_key);
    term_prev = NULL;
    while ((term = term_list) != NULL)
    {
        term_list = term->next;
        if (term->terminator == func && term->context == context)
        {
            if (term_prev == NULL)
            {
                //first terminator
                //set the thread key to be the next guy
                //When term_list is NULL the thread destructor won't be called
                pthread_setspecific(g_terminator_key, term_list);
            }
            else
            {
                term_prev->next = term_list;
            }
            NvOsFree(term);
            return NV_TRUE;
        }
        term_prev = term;
    }
    return NV_FALSE;
}

NvError NvOsTlsAddTerminatorInternal(void (*func)(void*), void *context)
{
    NvOsTlsTerminator *term;
    NvOsTlsTerminator *term_list;

    if (g_terminator_key == (pthread_key_t)NVOS_INVALID_TLS_INDEX)
    {
        /* rather than creating yet another mutex for the rare case where
         * two threads each attempt to create a thread before the TLS key is
         * initialized, just reuse the time mutex
         */
        pthread_mutex_lock(&g_timemutex);
        if (g_terminator_key == (pthread_key_t)NVOS_INVALID_TLS_INDEX)
            if (pthread_key_create(&g_terminator_key, run_terminator_list))
                g_terminator_key = (pthread_key_t)NVOS_INVALID_TLS_INDEX;
        pthread_mutex_unlock(&g_timemutex);
    }
    if (g_terminator_key == (pthread_key_t)NVOS_INVALID_TLS_INDEX)
        return NvError_InsufficientMemory;

    term_list = (NvOsTlsTerminator *)pthread_getspecific(g_terminator_key);

    term = NvOsAlloc(sizeof(*term));
    if (!term)
        return NvError_InsufficientMemory;

    term->next = term_list;
    term->terminator = func;
    term->context = context;
    if (pthread_setspecific(g_terminator_key, term))
    {
        NvOsFree(term);
        return NvError_InsufficientMemory;
    }
    return NvSuccess;
}

NvError NvOsThreadCreateInternal(
    NvOsThreadFunction function,
    void *args,
    NvOsThreadHandle *thread,
    NvOsThreadPriorityType priority)
{
    NvError ret;
    int err;
    NvOsThread *t = 0;
    NvOsThreadArgs *a = 0;

    /* this stalls the new thread until the out-params are set properly to
       prevent race conditions.
     */

    if (!function || !thread)
        return NvError_BadParameter;

    /* create the thread struct */
    t = NvOsAlloc( sizeof(NvOsThread) );
    if( t == 0 )
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(t,0,sizeof(NvOsThread));

    /* setup the thread args */
    a = NvOsAlloc( sizeof(NvOsThreadArgs) );
    if( a == 0 )
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }
    NvOsMemset(a,0,sizeof(NvOsThreadArgs));

    a->function = function;
    a->thread = t;
    a->thread_args = args;
    (void)pthread_mutex_init(&a->barrier, 0);

    /* create the pthread - use atomic ops to ensure proper
     * SMP memory barriers */
    NvOsAtomicExchange32(&a->init, 0);
    /* The thread is created with the mutex lock held, to prevent
     * race conditions between return parameter (*thread) assignment and
     * thread function execution */
    pthread_mutex_lock(&a->barrier);

    err = pthread_create( &t->thread, 0, thread_wrapper, a );
    if( err != 0 )
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }

    /* wait for the thread to start - use atomic ops here to ensure proper
     * memory barriers are in place for SMP systems */
    while(!NvOsAtomicExchangeAdd32(&a->init, 0))
    {
        NvOsThreadYield();
    }

    /* set the thread id, allow the thread function to execute */
    *thread = t;
    pthread_mutex_unlock(&a->barrier);

    return NvSuccess;

fail:
    if (a)
   	{
        // http://www.opengroup.org/onlinepubs/000095399/functions/pthread_mutex_init.html
        // "Attempting to destroy a locked mutex results in undefined behavior."
        pthread_mutex_unlock(&a->barrier);
        pthread_mutex_destroy(&a->barrier);
    }
    NvOsFree( a );
    NvOsFree( t );
    *thread = 0;

    return ret;
}

void NvOsThreadJoinInternal(NvOsThreadHandle t)
{
    int e;

    if( !t )
        return;

    e = pthread_join( t->thread, 0 );
    if( e != 0 )
    {
        return;
    }

    /* free the thread */
    NvOsFree( t );
}

void NvOsThreadYieldInternal( void )
{
    (void)sched_yield();
}

NvBool NvOsLinuxErrnoToNvError(NvError *e)
{
    switch (errno)
    {
    case EROFS: *e = NvError_ReadOnlyAttribute; break;
    case ENOENT: *e = NvError_FileNotFound; break;
    case EACCES: *e = NvError_AccessDenied; break;
    case ENOTDIR: *e = NvError_DirOperationFailed; break;
    case EEXIST: *e = NvError_PathAlreadyExists; break;
    case EFAULT: *e = NvError_InvalidAddress; break;
    case ENFILE: *e = NvError_NotSupported; break;
    case EMFILE: *e = NvError_NotSupported; break;
    case EIO: *e = NvError_ResourceError; break;
    case EADDRINUSE: *e = NvError_AlreadyAllocated; break;
    case EBADF: *e = NvError_FileOperationFailed; break;
    case EAGAIN: *e = NvError_Busy; break;
    case EADDRNOTAVAIL: *e = NvError_InvalidAddress; break;
    case EFBIG: *e = NvError_InvalidSize; break;
    case EINTR: *e = NvError_Timeout; break;
    case EALREADY: *e = NvError_AlreadyAllocated; break;
    case ENOTSUP: *e = NvError_NotSupported; break;
    case ENOSPC: *e = NvError_InvalidSize; break;
    case EPERM: *e = NvError_AccessDenied; break;
    case ETIME: *e = NvError_Timeout; break;
    case ETIMEDOUT: *e = NvError_Timeout; break;
    case ELOOP: *e = NvError_InvalidState; break;
    case ENXIO: *e = NvError_ModuleNotPresent; break;
    default:
        return NV_FALSE;
    }
    errno = 0;
    return NV_TRUE;
}

#if NVCPU_IS_ARM
#define _ARM_DATA_BARRIER "dmb\n"
#else
/* Current L4T toolchain doesn't understand the instructions */
#if defined(__THUMBEL__) || defined(__THUMBEB__)
#define _ARM_DATA_BARRIER ".hword 0xF3BF,0x8F5F\n" /* dmb */
#else
#define _ARM_DATA_BARRIER  ".word  0xF57FF05F\n"   /* dmb */
#endif
#endif /* NVCPU_IS_ARM */

// Atomic operations
NvS32
NvOsAtomicCompareExchange32(NvS32 *pTarget, NvS32 OldValue, NvS32 NewValue)
{
    // XXX Would be nice to use gcc builtins on x86 and ARM also.
#if defined(__x86_64__)
    return __sync_val_compare_and_swap(pTarget, OldValue, NewValue);
#elif NVCPU_IS_X86
    unsigned long old;
    __asm__ __volatile__("lock; cmpxchgl %1,%2"
                         : "=a" (old)
                         : "q"((unsigned long)NewValue),
                           "m"(*((volatile unsigned long*)pTarget)),
                           "0"((unsigned long)OldValue)
                         : "memory");
    return old;
#elif  NVCPU_IS_ARM
    unsigned long res;
    NvS32 old;
    NvU32 zero = 0;

    do {

    __asm__ __volatile(
        "ldrex %1, [%2]\n"
        "mov %0, #0\n"
        "teq %1, %3\n"
        "strexeq %0, %4, [%2]\n"
        _ARM_DATA_BARRIER
            : "=&r" (res), "=&r" (old)
            : "r" (pTarget), "Ir" (OldValue), "r" (NewValue), "r" (zero)
            : "cc");
    } while (res);

    return old;
#else
#error NvOsAtomicCompareExchange32 not implemented for this CPU
#endif
}

NvS32 NvOsAtomicExchange32(NvS32 *pTarget, NvS32 Value)
{
    // XXX Would be nice to use gcc builtins on x86 and ARM also.
#if defined(__x86_64__)
    return __sync_lock_test_and_set(pTarget, Value);
#elif NVCPU_IS_X86
    __asm__ __volatile__("lock; xchgl %0,%1"
                         : "=r"((unsigned long)Value)
                         : "m"(*((volatile unsigned long*)pTarget)),
                           "0"((unsigned long)Value)
                         : "memory");
    return Value;
#elif NVCPU_IS_ARM
    unsigned long temp;
    NvS32 result;
    NvU32 zero = 0;

    __asm__ __volatile(
        "1: ldrex   %0, [%2]\n\t"
        "   strex   %1, %3, [%2]\n\t"
        "   teq     %1, #0\n\t"
        "   bne     1b\n"
        _ARM_DATA_BARRIER
            :   "=&r" (result), "=&r" (temp)
            :   "r" (pTarget), "r" (Value), "r" (zero)
            :   "cc");

    return result;
#else
#error NvOsAtomicExchange32 not implemented for this CPU
#endif
}

NvS32 NvOsAtomicExchangeAdd32(NvS32 *pTarget, NvS32 Value)
{
    // XXX Would be nice to use gcc builtins on x86 and ARM also.
#if defined(__x86_64__)
   return __sync_fetch_and_add(pTarget, Value);
#elif NVCPU_IS_X86
    __asm__ __volatile__("lock; xaddl %0,%1"
                         : "=r" ((unsigned long)Value)
                         : "m"(*((volatile unsigned long*)pTarget)),
                           "0"((unsigned long)Value)
                         : "memory");
    return Value;
#elif NVCPU_IS_ARM
    unsigned long temp, sum;
    NvS32 result;
    NvU32 zero = 0;

    __asm__ __volatile(
        "1: ldrex %0, [%3]\n\t"
        " add %2, %0, %4\n\t"
        " strex %1, %2, [%3]\n\t"
        " teq %1, #0\n\t"
        " bne 1b\n"
        _ARM_DATA_BARRIER
        : "=&r" (result), "=&r" (temp), "=&r" (sum)
        : "r" (pTarget), "Ir" (Value), "r" (zero)
        : "cc");

    return result;
#else
#error NvOsAtomicExchangeAdd32 not implemented for this CPU
#endif
}

NvError
NvOsGetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    struct timeval tp;
    if(gettimeofday(&tp, NULL) != 0)
    {
        return NvError_NotImplemented;
    }

    hNvOsSystemtime->Seconds = tp.tv_sec;
    hNvOsSystemtime->Milliseconds = tp.tv_usec;
    return NvSuccess;
}

NvError
NvOsSetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    return NvError_NotImplemented;
}

NvU32 NvOsGetTimeMS( void )
{
    NvU64 now;

    now = NvOs_gettimeofday();
    return (NvU32)( now / 1000 );
}

NvU64 NvOsGetTimeUS( void )
{
    return NvOs_gettimeofday();
}

void NvOsDataCacheWriteback( void )
{
    NV_ASSERT(0); // not currently supported by the traps available.
}

void NvOsDataCacheWritebackInvalidate( void )
{
    NV_ASSERT(0); // not currently supported by the traps available.
}

#if NVCPU_IS_ARM
static inline void NvOsCacheFlush( void *start, NvU32 length )
{
    unsigned long _beg = (unsigned long)(start);
    unsigned long _end = (unsigned long)((char*)start+length);

#ifdef __ARM_EABI__
    unsigned long _call = (unsigned long)0xf0002;

    __asm__ __volatile__ (
        "push {r7}\n\t"
        "mov r0, %0\n\t"
        "mov r1, %1\n\t"
        "mov r2, #0\n\t"
        "mov r7, %2\n\t"
        "swi 0 @ sys_cacheflush"
        "pop {r7}\n\t"

        : /* no outputs */
        : "r" (_beg), "r" (_end), "r" (_call)
        : "r0","r1","r2");
#else
    unsigned long _flg = (unsigned long)0;

    __asm__ __volatile__ (
        "mov r0, %0\n\t"
        "mov r1, %1\n\t"
        "mov r2, %2\n\t"
        "swi 0x9f0002 @ sys_cacheflush"

        : /* no outputs */
        : "r" (_beg), "r" (_end), "r" (_flg)
        : "r0","r1","r2");
#endif
}
#endif

void NvOsDataCacheWritebackRange( void *start, NvU32 length )
{
#if NVCPU_IS_ARM
    NvOsCacheFlush(start, length);
#endif
}

void NvOsDataCacheWritebackInvalidateRange( void *start, NvU32 length )
{
#if NVCPU_IS_ARM
    NvOsCacheFlush(start, length);
#endif
}

void NvOsInstrCacheInvalidate( void )
{
    NV_ASSERT(0);  // no trap available to do this
}

void NvOsInstrCacheInvalidateRange( void *start, NvU32 length )
{
#if NVCPU_IS_ARM
    // this trap does both I & D cache, which isn't exactly what you want,
    // but it works.
    NvOsCacheFlush(start, length);
#endif
}

void NvOsFlushWriteCombineBuffer( void )
{
}


NvError NvOsInterruptRegister(
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    if (!IrqListSize)
        return NvError_InvalidSize;

    if (!pIrqList || !pIrqHandlerList || !handle)
        return NvError_BadParameter;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsInterruptRegister)
        return g_NvOsKernel->nvOsInterruptRegister(IrqListSize,
                   pIrqList, pIrqHandlerList, context, handle, InterruptEnable);

    return NvError_NotImplemented;
}

void NvOsInterruptUnregister(NvOsInterruptHandle handle)
{
    if(!handle)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsInterruptUnregister)
        g_NvOsKernel->nvOsInterruptUnregister(handle);
}

NvError NvOsInterruptEnable(NvOsInterruptHandle handle)
{
    if (!handle)
        return NvError_BadParameter;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsInterruptEnable)
        return g_NvOsKernel->nvOsInterruptEnable(handle);

    return NvError_NotSupported;
}

void NvOsInterruptDone(NvOsInterruptHandle handle)
{
    if (!handle)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsInterruptDone)
        g_NvOsKernel->nvOsInterruptDone(handle);
}

void NvOsInterruptMask(NvOsInterruptHandle handle, NvBool mask)
{
    if (!handle)
        return;

    NvOs_StaticInit();
    if (g_NvOsKernel && g_NvOsKernel->nvOsInterruptMask)
        g_NvOsKernel->nvOsInterruptMask(handle, mask);
}

NvError NvOsPhysicalMemMapIntoCaller(
    void *pCallerPtr,
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags)
{
    NV_ASSERT(!"Should never get here");
    return NvError_NotImplemented;
}

void NvOsProfileApertureSizes( NvU32 *apertures, NvU32 *sizes )
{
    *apertures = 0;
}

void NvOsProfileStart( void **apertures )
{
}

void NvOsProfileStop( void **apertures )
{
}

NvError NvOsProfileWrite( NvOsFileHandle file, NvU32 index, void *apertures )
{
    return NvError_NotSupported;
}

NvError NvOsGetOsInformation(NvOsOsInfo *pOsInfo)
{
    if (pOsInfo)
    {
        NvOsMemset(pOsInfo, 0, sizeof(NvOsOsInfo));
        pOsInfo->OsType = NvOsOs_Linux;
#if defined(ANDROID)
        pOsInfo->Sku = NvOsSku_Android;
#else
        pOsInfo->Sku = NvOsSku_Unknown;
#endif
    }
    else
        return NvError_BadParameter;

    return NvError_Success;
}

#ifdef ANDROID
typedef struct
{
    int count;
    int ignore;
    void** frame;
} UnwindState;

static
_Unwind_Reason_Code UnwindCallback(_Unwind_Context* context, void* arg)
{
    UnwindState* state = arg;
    if (state->count)
    {
        _Unwind_Ptr ip = _Unwind_GetIP(context);
        if (ip)
        {
            if (state->ignore)
                --state->ignore;
            else
            {
                *state->frame++ = (void*)ip;
                --state->count;
            }
        }
    }
    return _URC_NO_REASON;
}

static
int AndroidBacktrace(void** frames, int ignore, int size)
{
    UnwindState state;
    state.count = size;
    state.ignore = ignore;
    state.frame = frames;
    _Unwind_Backtrace(UnwindCallback, &state);
    return size - state.count;
}

static
void AndroidDumpProcMaps(int pid)
{
    char buf[256];
    FILE* fp;

    sprintf(buf, "/proc/%d/maps", pid);
    fp = fopen(buf, "r");
    if (fp)
    {
        NvOsDebugPrintf("BeginProcMap\n");
        while (fgets(buf, sizeof(buf), fp))
            NvOsDebugPrintf("%s", buf);
        fclose(fp);
        NvOsDebugPrintf("EndProcMap\n");
    }
}

#endif

NvCallstack* NvOsCallstackCreate(NvOsCallstackType stackType)
{
    NvCallstack* stack = NULL;

    NV_ASSERT(stackType > 0 && stackType < NvOsCallstackType_Last);

    if (stackType == NvOsCallstackType_HexStack ||
        stackType == NvOsCallstackType_SymbolStack)
    {
        stack = NvOsAllocInternal(sizeof(NvCallstack));

        if (stack)
        {
            stack->type = stackType;
#ifdef ANDROID
            stack->numFrames = AndroidBacktrace(stack->frames,
                                                2,
                                                MAX_CALLSTACK_FRAMES);
            stack->frameSymbols = NULL;
#else
            stack->numFrames = backtrace(stack->frames, MAX_CALLSTACK_FRAMES);

            if (stackType == NvOsCallstackType_SymbolStack)
            {
                stack->frameSymbols = backtrace_symbols(stack->frames,
                    stack->numFrames);
            }
            else
            {
                stack->frameSymbols = NULL;
            }
#endif
        }
    }

    return stack;
}

void NvOsCallstackDestroy(NvCallstack* stack)
{
    if (stack)
    {
        if (stack->frameSymbols)
            free(stack->frameSymbols);

        NvOsFreeInternal(stack);
    }
}

NvU32 NvOsCallstackGetHeight(NvCallstack* stack)
{
    if (stack)
        return stack->numFrames;
    return 0;
}

void NvOsCallstackGetFrame(char* buf, NvU32 len, NvCallstack* stack, NvU32 level)
{
    if (stack && level < stack->numFrames)
    {
#ifdef ANDROID
        NvOsSnprintf(buf, len, "%p", stack->frames[level]);
#else
        if (stack->type == NvOsCallstackType_HexStack)
        {
            NvOsSnprintf(buf, len, "%p N/A",
                         stack->frames[level]);
        }
        else
        {
            if (stack->frameSymbols)
                NvOsSnprintf(buf, len, "%s",
                             stack->frameSymbols[level]);
            else
                NvOsSnprintf(buf, len, "%p <out of memory>",
                             stack->frames[level]);
        }
#endif
    }
    else
    {
        NvOsSnprintf(buf, len, "");
    }
}

void NvOsCallstackDump(
    NvCallstack* stack,
    NvU32 skip,
    NvOsDumpCallback callBack,
    void* context)
{
    if (stack)
    {
        char buf[256];
        NvU32 i;

        pthread_mutex_lock(&g_callstackmutex);
        callBack(context, "Callstack:");
#ifdef ANDROID
        AndroidDumpProcMaps(getpid());
#endif
        for (i = skip; i < stack->numFrames; ++i)
        {
            NvOsCallstackGetFrame(buf, sizeof(buf), stack, i);
            callBack(context, buf);
        }
        callBack(context, "");
        pthread_mutex_unlock(&g_callstackmutex);
    }
}

NvBool NvOsCallstackContainsPid(NvCallstack* stack, NvU32 pid)
{
    /* This function is a stub, pid filtering may not have any meaning on
     * Linux
     */
    (void)stack;
    (void)pid;
    return NV_TRUE;
}

NvU32 NvOsCallstackHash(NvCallstack* stack)
{
    if (stack)
        return NvOsHashJenkins(stack->frames, sizeof(void*) *
            stack->numFrames);

    return 0L;
}

void NvOsGetProcessInfo(char* buf, NvU32 len)
{
    char    buffer[128] = { 0 };
    FILE*   cmdline;

    cmdline = fopen("/proc/self/cmdline", "r");
    if (cmdline)
    {
        // add if (blah) {} to get around compiler warning
        if (fread(buffer, 1, 128, cmdline)) {}
        fclose(cmdline);
    }

    NvOsSnprintf(buf, len, "%s (pid %d)(tid %d)",
            buffer[0] ? buffer : "-",
#if defined(NVOS_IS_DARWIN)
            // this is probably as close as we can get
            // xxxnsubtil: can we just do this on linux?
            pthread_self(),
#else
            syscall(__NR_gettid),
#endif
            getpid());
}

#if defined(ANDROID)
void NvOsAndroidDebugString(const char* str)
{
    ALOG(LOG_DEBUG, "NvOsDebugPrintf", "%s", str);
}
#endif // ANDROID

#if __GNUC__ > 3
void __attribute__ ((destructor)) nvos_fini(void);
void __attribute__ ((destructor)) nvos_fini(void)
{
    if (g_terminator_key != (pthread_key_t)NVOS_INVALID_TLS_INDEX)
        (void)pthread_key_delete(g_terminator_key);
}

#endif

int NvOsSetFpsTarget(int target)
{
#if defined(ANDROID)
    if (tegra_throughput_fd == -1)
        tegra_throughput_fd = open(THROUGHPUT_DEVICE, O_RDWR, 0);

    if (tegra_throughput_fd == -1)
        NV_DEBUG_PRINTF(( "can\'t open " THROUGHPUT_DEVICE ": %s\n", strerror(errno) ));
    else
    {
        if (NvOsModifyFpsTarget(tegra_throughput_fd, target) < 0)
        {
            close(tegra_throughput_fd);
            tegra_throughput_fd = -1;
        }
    }

#if UNIFIED_SCALING
    NvOsSendThroughputHint("graphics", NvOsTPHintType_FramerateTarget, target, 2000);
#endif
#endif /* defined (ANDROID) */

    return 0;
}

int NvOsModifyFpsTarget(int fd, int target)
{
#if defined(ANDROID)
    if (target != lastFpsTarget) {
        int err;

        lastFpsTarget = target;
        err = ioctl(tegra_throughput_fd, TEGRA_THROUGHPUT_IOCTL_TARGET_FPS, target);

        if (err == -1) {
            NV_DEBUG_PRINTF(("ioctl TEGRA_THROUGHPUT_IOCTL_TARGET_FPS(%d) "
                             "failed: %s\n", target, strerror(errno)));
        }
    }

#if UNIFIED_SCALING
    NvOsSendThroughputHint("graphics", NvOsTPHintType_FramerateTarget, target, 2000);
#endif
#endif /* defined (ANDROID) */

    return 0;
}

void NvOsCancelFpsTarget(int fd)
{
#if defined(ANDROID)
#if UNIFIED_SCALING
    NvOsCancelThroughputHint("graphics");
#endif

    if (tegra_throughput_fd > -1) {
        close(tegra_throughput_fd);
        tegra_throughput_fd = -1;
    }
    lastFpsTarget = -1;
#endif /* defined (ANDROID) */
}
