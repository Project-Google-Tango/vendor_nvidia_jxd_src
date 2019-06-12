/*
 * Copyright (c) 2007-2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvcommon.h"
#include "nvodm_services.h"
#include "nvos.h"
#include "nvassert.h"

void
NvOdmOsPrintf( const char *format, ...)
{
    va_list ap;
    va_start(ap, format);

    // On WinCE/AOS, printing goes to the OS debug console (there is no stdout).
    NvOsDebugVprintf(format, ap);

    va_end(ap);
}

void
NvOdmOsDebugPrintf( const char *format, ... )
{
#if NV_DEBUG
    va_list ap;
    va_start( ap, format );
    NvOsDebugVprintf( format, ap );
    va_end( ap );
#endif
}

void *
NvOdmOsAlloc(size_t size)
{
    return NvOsAlloc( size );
}

void
NvOdmOsFree(void *ptr)
{
    NvOsFree( ptr );
}

void
NvOdmOsMemcpy(void *dest, const void *src, size_t size)
{
    NvOsMemcpy(dest, src, size);
}

int
NvOdmOsMemcmp(const void *s1, const void *s2, size_t size)
{
    return NvOsMemcmp(s1, s2, size);
}

void
NvOdmOsMemset(void *s, NvU8 c, size_t size)
{
    NvOsMemset(s, c, size);
}

int NvOdmOsStrcmp( const char *s1, const char *s2)
{
    return NvOsStrcmp(s1, s2);
}

int NvOdmOsStrncmp( const char *s1, const char *s2, size_t size )
{
    return NvOsStrncmp(s1, s2, size);
}

NvOdmOsMutexHandle
NvOdmOsMutexCreate( void )
{
    NvError err;
    NvOsMutexHandle m;

    err = NvOsMutexCreate(&m);
    if( err == NvSuccess )
    {
        return (NvOdmOsMutexHandle)m;
    }

    return NULL;
}

void
NvOdmOsMutexLock( NvOdmOsMutexHandle mutex )
{
    NvOsMutexLock( (NvOsMutexHandle)mutex );
}

void
NvOdmOsMutexUnlock( NvOdmOsMutexHandle mutex )
{
    NvOsMutexUnlock( (NvOsMutexHandle)mutex );
}

void
NvOdmOsMutexDestroy( NvOdmOsMutexHandle mutex )
{
    NvOsMutexDestroy( (NvOsMutexHandle)mutex );
}

NvOdmOsSemaphoreHandle
NvOdmOsSemaphoreCreate( NvU32 value )
{
    NvError err;
    NvOsSemaphoreHandle s;

    err = NvOsSemaphoreCreate(&s, value);
    if( err == NvSuccess )
    {
        return (NvOdmOsSemaphoreHandle)s;
    }

    return NULL;
}

void
NvOdmOsSemaphoreWait( NvOdmOsSemaphoreHandle semaphore )
{
    NvOsSemaphoreWait( (NvOsSemaphoreHandle)semaphore );
}

NvBool
NvOdmOsSemaphoreWaitTimeout( NvOdmOsSemaphoreHandle semaphore, NvU32 msec )
{
    NvError err;

    err = NvOsSemaphoreWaitTimeout( (NvOsSemaphoreHandle)semaphore, msec );
    if( err == NvError_Timeout )
    {
        return NV_FALSE;
    }

    return NV_TRUE;
}

void
NvOdmOsSemaphoreSignal( NvOdmOsSemaphoreHandle semaphore )
{
    NvOsSemaphoreSignal( (NvOsSemaphoreHandle)semaphore );
}

void
NvOdmOsSemaphoreDestroy( NvOdmOsSemaphoreHandle semaphore )
{
    NvOsSemaphoreDestroy( (NvOsSemaphoreHandle)semaphore );
}

NvOdmOsThreadHandle
NvOdmOsThreadCreate(
    NvOdmOsThreadFunction function,
    void *args)
{
    NvError err;
    NvOsThreadHandle t;

    err = NvOsThreadCreate( (NvOsThreadFunction)function, args, &t );
    if( err == NvSuccess )
    {
        return (NvOdmOsThreadHandle)t;
    }

    return NULL;
}

void
NvOdmOsThreadJoin(NvOdmOsThreadHandle thread)
{
    NvOsThreadJoin((NvOsThreadHandle)thread);
}

void
NvOdmOsWaitUS(NvU32 usec)
{
    NvOsWaitUS(usec);
}

void
NvOdmOsSleepMS(NvU32 msec)
{
    NvOsSleepMS(msec);
}

NvU32
NvOdmOsGetTimeMS(void)
{
    return NvOsGetTimeMS();
}

// Assert that the types defined in nvodm_services.h map correctly to their
// corresponding nvos types.
NV_CT_ASSERT(NVOS_OPEN_READ == NVODMOS_OPEN_READ);
NV_CT_ASSERT(NVOS_OPEN_WRITE == NVODMOS_OPEN_WRITE);
NV_CT_ASSERT(NVOS_OPEN_CREATE == NVODMOS_OPEN_CREATE);

NV_CT_ASSERT((int)NvOsFileType_File == (int)NvOdmOsFileType_File);
NV_CT_ASSERT((int)NvOsFileType_Directory == (int)NvOdmOsFileType_Directory);
NV_CT_ASSERT(sizeof(NvOsStatType) == sizeof(NvOdmOsStatType));

NvBool
NvOdmOsFopen(const char *path, NvU32 flags, NvOdmOsFileHandle *file)
{
    return (NvOsFopen(path, flags, (NvOsFileHandle*)file) == NvSuccess);
}

void NvOdmOsFclose(NvOdmOsFileHandle stream)
{
    NvOsFclose((NvOsFileHandle)stream);
}

NvBool
NvOdmOsFwrite(NvOdmOsFileHandle stream, const void *ptr, size_t size)
{
    return (NvOsFwrite((NvOsFileHandle)stream, ptr, size) == NvSuccess);
}

NvBool
NvOdmOsFread(NvOdmOsFileHandle stream, void *ptr, size_t size, size_t *bytes)
{
    return (NvOsFread((NvOsFileHandle)stream, ptr, size, bytes) == NvSuccess);
}

NvBool
NvOdmOsStat(const char *filename, NvOdmOsStatType *stat)
{
    return (NvOsStat(filename, (NvOsStatType*)stat) == NvSuccess);
}

