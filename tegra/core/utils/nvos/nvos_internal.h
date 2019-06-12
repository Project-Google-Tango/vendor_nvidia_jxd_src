/*
 * Copyright (c) 2008-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_NVOS_INTERNAL_H
#define INCLUDED_NVOS_INTERNAL_H

#ifndef INCLUDED_NVOS_H
#include "nvos.h"
#endif


typedef enum
{
    NvOsResourceType_NvOsAlloc = 1,
    NvOsResourceType_NvOsLibrary,
    NvOsResourceType_NvOsMutex,
    NvOsResourceType_NvOsSemaphore,
    NvOsResourceType_NvOsSharedMemMap,
    NvOsResourceType_NvOsSharedMemAlloc,
    NvOsResourceType_NvOsExecAlloc,
    NvOsResourceType_NvOsVirtualAlloc,
    NvOsResourceType_NvOsFile,
    NvOsResourceType_NvOsThread,
    NvOsResourceType_NvOsEvent,
    NvOsResourceType_NvOsPageAlloc,
    NvOsResourceType_NvOsPhysicalMemMap,
    
    NvOsResourceType_Last,
    NvOsResourceType_Force32 = 0x7FFFFFFF
} NvOsResourceType;

typedef enum
{
    NvOsThreadPriorityType_Normal = 1,
    NvOsThreadPriorityType_NearInterrupt,

    NvOsThreadPriorityType_Last,
    NvOsThreadPriorityType_Force32 = 0x7FFFFFFF
} NvOsThreadPriorityType;

// Set this variable to non-zero to enable support of memory allocation
// tracking on supported operating systems.  The size is the maximum number of
// allocations that will be tracked.
#define NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE        0 // recommended 2048-4096
#if NV_TRACK_MEMORY_ALLOCATION_LIST_SIZE
void AssertCheckAllocationTrackingList(void);
#endif

// "Internal" versions of functions don't include the debug support.
void *NvOsAllocInternal(size_t size);
void *NvOsReallocInternal(void *ptr, size_t size);
void NvOsFreeInternal(void *ptr);


// The internal thread packages are preemptive
NvError NvOsMutexCreateInternal( NvOsMutexHandle *mutex );
void NvOsMutexLockInternal( NvOsMutexHandle mutex );
void NvOsMutexUnlockInternal( NvOsMutexHandle mutex );
void NvOsMutexDestroyInternal( NvOsMutexHandle mutex );

NvError NvOsSemaphoreCreateInternal(
    NvOsSemaphoreHandle *semaphore, 
    NvU32 value);
void NvOsSemaphoreWaitInternal( NvOsSemaphoreHandle semaphore );
NvError NvOsSemaphoreWaitTimeoutInternal( NvOsSemaphoreHandle semaphore,
    NvU32 msec );
void NvOsSemaphoreSignalInternal( NvOsSemaphoreHandle semaphore );
void NvOsSemaphoreDestroyInternal( NvOsSemaphoreHandle semaphore );
NvError NvOsSemaphoreCloneInternal( NvOsSemaphoreHandle orig,
    NvOsSemaphoreHandle *semaphore );

NvU32 NvOsTlsAllocInternal(void);
void NvOsTlsFreeInternal(NvU32 TlsIndex);
void *NvOsTlsGetInternal(NvU32 TlsIndex);
void NvOsTlsSetInternal(NvU32 TlsIndex, void *Value);
NvBool NvOsTlsRemoveTerminatorInternal(void (*func)(void *), void *context);
NvError NvOsTlsAddTerminatorInternal(void (*func)(void*), void*);

NvError NvOsThreadCreateInternal(
    NvOsThreadFunction function,
    void *args,
    NvOsThreadHandle *thread,
    NvOsThreadPriorityType priority);

void NvOsThreadJoinInternal( NvOsThreadHandle thread );

void NvOsThreadYieldInternal( void );
void NvOsSleepMSInternal( NvU32 msec );

// Internal OS file handling functions
NvError NvOsFopenInternal(
    const char *path, 
    NvU32 flags, 
    NvOsFileHandle *file );
void    NvOsFcloseInternal(NvOsFileHandle stream);
NvError NvOsFwriteInternal(
    NvOsFileHandle stream, 
    const void *ptr, 
    size_t size);
NvError NvOsFreadInternal(
    NvOsFileHandle stream, 
    void *ptr, 
    size_t size, 
    size_t *bytes,
    NvU32 timeout_msec);
NvError NvOsFseekInternal(
    NvOsFileHandle file, 
    NvS64 offset, 
    NvOsSeekEnum whence);
NvError NvOsFtellInternal(NvOsFileHandle file, NvU64 *position);
NvError NvOsFstatInternal(NvOsFileHandle file, NvOsStatType *stat);
NvError NvOsStatInternal(const char *filename, NvOsStatType *stat);
NvError NvOsFflushInternal(NvOsFileHandle stream);
NvError NvOsFsyncInternal(NvOsFileHandle stream);
NvError NvOsFremoveInternal(const char *filename);
NvError NvOsFlockInternal(NvOsFileHandle stream, NvOsFlockType type);
NvError NvOsFtruncateInternal(NvOsFileHandle stream, NvU64 length);
NvError NvOsOpendirInternal(const char *path, NvOsDirHandle *dir);
NvError NvOsReaddirInternal(NvOsDirHandle dir, char *name, size_t size);
void    NvOsClosedirInternal(NvOsDirHandle dir);
NvError NvOsMkdirInternal(char *dirname);

// Platform implementations for config set/get. If not implemented then
// the platform independent config file mechanism is used instead.
// If the platform doesn't support setting properties, return
// NvError_NotSupported instead of NvError_NotImplemented for the setter.
// If the platform only supports string properties, return
// NvError_NotImplemented for the integer variants.
// If the platform does not support setting and getting generic state,
// return NvError_NotSupported
NvError NvOsSetConfigStringInternal (const char *name, const char *value);
NvError NvOsSetConfigU32Internal (const char *name, NvU32 value);
NvError NvOsGetConfigStringInternal (const char *name, char *value, NvU32 size);
NvError NvOsGetConfigU32Internal (const char *name, NvU32 *value);
NvError NvOsSetSysConfigStringInternal (const char *name, const char *value);
NvError NvOsGetSysConfigStringInternal (const char *name, char *value, NvU32 size);
NvError NvOsConfigSetStateInternal(int stateId, const char *name, const char *value,
        int valueSize, int flags);
NvError NvOsConfigGetStateInternal(int stateId, const char *name, char *value,
        int valueSize, int flags);
NvError NvOsConfigQueryStateInternal(int stateId, const char **ppKeys, int *pNumKeys,
        int maxKeySize);

// Windows only -- nowhere else to put this at present
void NvOsProcessAttachCommonInternal( void );
void NvOsCloseHandle(NvOsResourceType type, void *h);
void NvOsVirtualFree(void *p);

// resourceTag struct for memory leaks tracking
typedef struct resourceTag
{
  char *FileName;
  int LineNum;
  NvU8 *ptr;
} resourceTag;

/* Generic hash function for opaque data. */
NvU32 NvOsHashJenkins(const void *data, NvU32 length);

/* Generic mapping from key to safe, portable shm filename */
NvError NvOsSharedMemPath(const char *key, char *output, NvU32 length);


#endif // INCLUDED_NVOS_INTERNAL_H
