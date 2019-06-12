/*
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/slog.h>
#include <sys/syspage.h>
#include <arm/mmu.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "nvos.h"
#include "nvos_internal.h"
#include "nvassert.h"
#include "qnx/nvos_devctls.h"

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

#define NVOS_SCHED_POLICY   SCHED_FIFO
#define NVOS_INTERRUPT_PRI  15

#define APBMISC_BASE    0x70000000
#define APBMISC_LEN     0x1000
#define APBMISC_REG_CHIP_ID 0x201

NvBool NvOsQnxErrnoToNvError(NvError *e);

static int nvos_fd = -1;
static pthread_mutex_t g_timemutex = PTHREAD_MUTEX_INITIALIZER;
static struct nvos_semaphore *semaphore_table = MAP_FAILED;
static void *apbmisc_base;

static void NvOsTableDestroy(void);

static int NvOsTableInit(void)
{
    int id;
    int flags = PROT_READ | PROT_WRITE;

    nvos_fd = open(DEV_NVOS, O_RDWR);
    if (nvos_fd < 0)
        return -1;

    if (NvOsPhysicalMemMap(APBMISC_BASE, APBMISC_LEN, NvOsMemAttribute_Uncached,
                NVOS_MEM_READ, &apbmisc_base) != NvSuccess) {
        NvOsTableDestroy();
        return -1;
    }

    id = shm_open(NVOS_SEMAPHORE_TABLE_SHMEM_PATH, O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
    if (id == -1) {
        NvOsTableDestroy();
        return -1;
    }

    semaphore_table = mmap((void *)0,
                           NVOS_SEMAPHORE_TABLE_NUM_ENTRIES * sizeof(struct nvos_semaphore),
                           flags, MAP_SHARED, id, 0);

    close(id);
    if (semaphore_table == MAP_FAILED) {
        NvOsTableDestroy();
        return -1;
    }

    return 0;
}

static void NvOsTableDestroy(void)
{
    if (semaphore_table != MAP_FAILED)
        munmap(semaphore_table, NVOS_SEMAPHORE_TABLE_NUM_ENTRIES * sizeof(struct nvos_semaphore));

    if (apbmisc_base != NULL) {
        NvOsPhysicalMemUnmap(apbmisc_base, APBMISC_LEN);
        apbmisc_base = NULL;
    }

    if (nvos_fd >= 0) {
        close(nvos_fd);
        nvos_fd = -1;
    }
}

void NvOsBreakPoint(const char* file, NvU32 line, const char* condition)
{
    if (file != NULL)
    {
        NvOsDebugPrintf("\n\nAssert on %s:%d: %s\n", file, line, (condition) ?
            condition : " " );
    }

#define NVOS_CRASH_ON_BREAK 1
    if (NVOS_CRASH_ON_BREAK)
    {
        static volatile int a = 1, b = 0, *c = (int*)0;
        *c += a;
        *c += a/b;
    }

#if NVCPU_IS_X86
    __asm__("int $3");
#elif NVCPU_IS_ARM
    __asm__ __volatile("BKPT #3"); /* immediate is ignored */
#else
#error "No asm implementation of NvOsBreakPoint yet for this CPU"
#endif
}

typedef struct NvOsFileRec
{
    FILE *file;
    int   fd;
    NvOsFileType type;
} NvOsFile;

NvError NvOsFprintf(NvOsFileHandle stream, const char *format, ...)
{
    int n;
    va_list ap;

    NV_ASSERT(stream != NULL);
    NV_ASSERT(format != NULL);

    va_start(ap, format);
    n = vfprintf(stream->file, format, ap);
    va_end(ap);

    if (n > 0)
        return NvSuccess;

    return NvError_FileWriteFailed;
}

NvS32 NvOsSnprintf(char *str, size_t size, const char *format, ...)
{
    int n;
    va_list ap;

    NV_ASSERT(str != NULL);
    NV_ASSERT(format != NULL);

    va_start(ap, format);
    n = vsnprintf(str, size, format, ap);
    va_end(ap);

    return n;
}

NvError NvOsVfprintf(NvOsFileHandle stream, const char *format, va_list ap)
{
    int n;

    NV_ASSERT(stream != NULL);
    NV_ASSERT(format != NULL);

    n = vfprintf(stream->file, format, ap);

    if (n > 0)
        return NvSuccess;

    return NvError_FileWriteFailed;
}

NvS32 NvOsVsnprintf(char *str, size_t size, const char *format, va_list ap)
{
    int n;

    NV_ASSERT(str != NULL);
    NV_ASSERT(format != NULL);

    n = vsnprintf(str, size, format, ap);
    return n;
}

#define NV_SLOGCODE 0xAAAA
void NvOsDebugString(const char *str)
{
    NV_ASSERT(str != NULL);

    slogf(_SLOG_SETCODE(NV_SLOGCODE, 0), _SLOG_ERROR, "%s", str);
}

void NvOsDebugPrintf(const char *format, ...)
{
    va_list ap;

    NV_ASSERT(format != NULL);

    va_start(ap, format);
    vslogf(_SLOG_SETCODE(NV_SLOGCODE, 0), _SLOG_ERROR, format, ap);
    va_end(ap);
}

void NvOsDebugVprintf(const char *format, va_list ap)
{
    NV_ASSERT(format != NULL);

    vslogf(_SLOG_SETCODE(NV_SLOGCODE, 0), _SLOG_ERROR, format, ap);
}

NvS32 NvOsDebugNprintf(const char *format, ...)
{
    int n;
    va_list ap;

    NV_ASSERT(format != NULL);

    va_start(ap, format);
    n = vslogf(_SLOG_SETCODE(NV_SLOGCODE, 0), _SLOG_ERROR, format, ap);
    va_end(ap);

    return n;
}

void NvOsStrcpy(char *dest, const char *src)
{
    NV_ASSERT(dest != NULL);
    NV_ASSERT(src != NULL);

    strcpy(dest, src);
}

void NvOsStrncpy(char *dest, const char *src, size_t size)
{
    NV_ASSERT(dest != NULL);
    NV_ASSERT(src != NULL);

    strncpy(dest, src, size);
}

size_t NvOsStrlen(const char *s)
{
    NV_ASSERT(s != NULL);

    return strlen(s);
}

NvOsCodePage NvOsStrGetSystemCodePage(void)
{
    return NvOsCodePage_Windows1252;
}

int NvOsStrcmp(const char *s1, const char *s2)
{
    NV_ASSERT(s1 != NULL);
    NV_ASSERT(s2 != NULL);

    return strcmp(s1, s2);
}

int NvOsStrncmp(const char *s1, const char *s2, size_t size)
{
    NV_ASSERT(s1 != NULL);
    NV_ASSERT(s2 != NULL);

    return strncmp(s1, s2, size);
}

void NvOsMemcpy(void *dest, const void *src, size_t size)
{
    /*
     * Check args, only if size is positive (no dereference expected).
     * Note: not all NvOsMemcpy callers respects ISO/IEC 9899:1999
     * (section 7.21.1 "String function conventions") and they provide
     * invalid pointers together with 0 (zero) size.
     */
    NV_ASSERT(size == 0 || dest != NULL);
    NV_ASSERT(size == 0 || src != NULL);

    memcpy(dest, src, size);
}

int NvOsMemcmp(const void *s1, const void *s2, size_t size)
{
    NV_ASSERT(s1 != NULL);
    NV_ASSERT(s2 != NULL);

    return memcmp(s1, s2, size);
}

void NvOsMemset(void *s, NvU8 c, size_t size)
{
    NV_ASSERT(s != NULL);

    memset(s, (int)c, size);
}

void NvOsMemmove(void *dest, const void *src, size_t size)
{
    NV_ASSERT(dest != NULL);
    NV_ASSERT(src != NULL);

    memmove(dest, src, size);
}

NvError NvOsCopyIn(void *pDst, const void *pSrc, size_t Bytes)
{
    NV_ASSERT(pDst != NULL);
    NV_ASSERT(pSrc != NULL);

    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError NvOsCopyOut(void *pDst, const void *pSrc, size_t Bytes)
{
    NV_ASSERT(pDst != NULL);
    NV_ASSERT(pSrc != NULL);

    NvOsMemcpy(pDst, pSrc, Bytes);
    return NvError_Success;
}

NvError NvOsFopenInternal(const char *path, NvU32 flags, NvOsFileHandle *file)
{
    char *mode;
    NvOsFile *f;
    NvOsStatType st;
    NvError e = NvSuccess;

    NV_ASSERT(path != NULL);
    NV_ASSERT(file != NULL);

    switch (flags)
    {
        case NVOS_OPEN_READ:
            mode = "r";
            break;
        case NVOS_OPEN_WRITE:
            mode = "w";
            break;
        case NVOS_OPEN_READ | NVOS_OPEN_WRITE:
            mode = "r+";
            break;
        case NVOS_OPEN_CREATE | NVOS_OPEN_WRITE:
            mode = "w";
            break;
        case NVOS_OPEN_CREATE | NVOS_OPEN_READ | NVOS_OPEN_WRITE:
            mode = "w+";
            break;
        default:
            return NvError_BadParameter;
    }

    f = NvOsAlloc(sizeof(NvOsFile));
    if (f == NULL)
        return NvError_InsufficientMemory;

    errno = 0;
    f->file = fopen(path, mode);
    if (f->file == NULL)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
        goto fail;
    }

    f->fd = fileno(f->file);
    if (f->fd == -1)
        goto fail;

    e = NvOsFstatInternal(f, &st);
    if (e != NvSuccess)
        goto fail;

    f->type = st.type;
    *file = f;
    return NvSuccess;

fail:
    NvOsFree(f);
    return e;
}

void NvOsFcloseInternal(NvOsFileHandle stream)
{
    if (stream == NULL)
        return;

    fclose(stream->file);
    NvOsFree(stream);
}

NvError NvOsFwriteInternal(NvOsFileHandle stream, const void *ptr, size_t size)
{
    char *p;
    size_t s;
    size_t len;
    NvError e = NvSuccess;

    NV_ASSERT(stream != NULL);
    NV_ASSERT(ptr != NULL);

    if (size == 0)
        return NvSuccess;

    s = size;
    p = (char *)ptr;
    for (;;)
    {
        errno = 0;
        len = fwrite(p, 1, s, stream->file);
        if (len == s)
            break;

        p += len;
        s -= len;

        if (errno != EINTR)
        {
            if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_FileReadFailed;
            clearerr(stream->file);
            errno = 0;
            return e;
        }
        clearerr(stream->file);
    }

    if (stream->type == NvOsFileType_Fifo)
        e = NvOsFflushInternal(stream);

    return e;
}

NvError NvOsFreadInternal(
    NvOsFileHandle stream,
    void *ptr,
    size_t size,
    size_t *bytes,
    NvU32 timeout_msec)
{
    char *p;
    size_t s;
    size_t len;
    NvError e = NvSuccess;

    NV_ASSERT(stream != NULL);
    NV_ASSERT(ptr != NULL);

    if (size == 0)
    {
        if (bytes != NULL)
            *bytes = 0;
        return e;
    }

    s = size;
    p = (char *)ptr;
    for (;;)
    {
        errno = 0;
        len = fread(p, 1, s, stream->file);
        if (len == s)
        {
            if (bytes != NULL)
                *bytes = size;
            return NvSuccess;
        }
        p += len;
        s -= len;

        if (errno != EINTR)
        {
            if (bytes != NULL)
                *bytes = size - s;

            if (errno == 0 && feof(stream->file))
                e = NvError_EndOfFile;
            else if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_FileReadFailed;
            clearerr(stream->file);
            return e;
        }
        clearerr(stream->file);
    }
    return e;
}

NvError NvOsFseekInternal(NvOsFileHandle hFile, NvS64 offset, NvOsSeekEnum whence)
{
    int w;
    int err;
    NvError e = NvSuccess;

    NV_ASSERT(hFile != NULL);

    switch (whence)
    {
        case NvOsSeek_Set:
            w = SEEK_SET;
            break;
        case NvOsSeek_Cur:
            w = SEEK_CUR;
            break;
        case NvOsSeek_End:
            w = SEEK_END;
            break;
        default:
            return NvError_BadParameter;
    }

    err = fseeko(hFile->file, (off_t)offset, w);
    if (err != 0)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    return e;
}

NvError NvOsFtellInternal(NvOsFileHandle hFile, NvU64 *position)
{
    off_t offset;
    NvError e = NvSuccess;

    NV_ASSERT(hFile != NULL);
    NV_ASSERT(position != NULL);

    offset = ftello(hFile->file);
    if (offset == -1)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }

    *position = (NvU64)offset;
    return NvSuccess;
}

NvError NvOsFstatInternal(NvOsFileHandle hFile, NvOsStatType *s)
{
    int fd;
    int err;
    struct stat fs;

    NV_ASSERT(hFile != NULL);
    NV_ASSERT(s != NULL);

    fd = hFile->fd;
    err = fstat(fd, &fs);
    if (err != 0)
        return NvError_FileOperationFailed;

    s->size = (NvU64)fs.st_size;
    if (S_ISREG(fs.st_mode))
        s->type = NvOsFileType_File;
    else if (S_ISDIR(fs.st_mode))
        s->type = NvOsFileType_Directory;
    else if (S_ISFIFO(fs.st_mode))
        s->type = NvOsFileType_Fifo;
    else if (S_ISCHR(fs.st_mode))
        s->type = NvOsFileType_CharacterDevice;
    else if (S_ISBLK(fs.st_mode))
        s->type = NvOsFileType_BlockDevice;
    else
        s->type = NvOsFileType_Unknown;

    return NvSuccess;
}

NvError NvOsFflushInternal(NvOsFileHandle stream)
{
    int err;
    NvError e = NvSuccess;

    NV_ASSERT(stream != NULL);

    err = fflush(stream->file);
    if (err != 0)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
        clearerr(stream->file);
    }
    return e;
}

NvError NvOsFsyncInternal(NvOsFileHandle stream)
{
    int err;
    NvError e = NvSuccess;

    NV_ASSERT(stream != NULL);

    err = fsync(stream->fd);
    if (err != 0)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
        clearerr(stream->file);
    }
    return e;
}

NvError NvOsFremoveInternal(const char *filename)
{
    int err;
    NvError e = NvSuccess;

    NV_ASSERT(filename != NULL);

    err = unlink(filename);
    if (err != 0)
    {
        if (!NvOsQnxErrnoToNvError(&e))
            e = NvError_FileOperationFailed;
    }
    return e;
}

NvError NvOsFlockInternal(NvOsFileHandle stream, NvOsFlockType type)
{
    return NvSuccess;
}

NvError NvOsFtruncateInternal(NvOsFileHandle stream, NvU64 length)
{
    return NvError_NotImplemented;
}

NvError NvOsIoctl(
    NvOsFileHandle hFile,
    NvU32 IoctlCode,
    void *pBuffer,
    NvU32 InBufferSize,
    NvU32 InOutBufferSize,
    NvU32 OutBufferSize)
{
    NV_ASSERT(!"NvOsIoctl not implemented");
    return NvError_NotImplemented;
}

NvError NvOsOpendirInternal(const char *path, NvOsDirHandle *dir)
{
    DIR *d;

    NV_ASSERT(path != NULL);
    NV_ASSERT(dir != NULL);

    d = opendir(path);
    if (d == NULL)
        return NvError_DirOperationFailed;

    *dir = (NvOsDirHandle)d;
    return NvSuccess;
}

NvError NvOsReaddirInternal(NvOsDirHandle dir, char *name, size_t size)
{
    struct dirent *d;

    NV_ASSERT(dir != NULL);
    NV_ASSERT(name != NULL);
    NV_ASSERT(size);

    d = readdir((DIR *)dir);
    if (d == NULL)
        return NvError_EndOfDirList;

    strncpy(name, d->d_name, size);
    name[size-1] = '\0';
    return NvSuccess;
}

void NvOsClosedirInternal(NvOsDirHandle dir)
{
    if (dir == NULL)
        return;

    closedir((DIR *)dir);
}

NvError NvOsMkdirInternal(char *dirname)
{
    return NvError_NotImplemented;
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
    if (ptr == NULL)
        return;

    free(ptr);
}

void *NvOsExecAlloc(size_t size)
{
    void *ptr;
    static int s_zerofd = -1;

    if (s_zerofd == -1)
    {
        s_zerofd = open("/dev/zero", O_RDWR);
        if (s_zerofd == -1)
            return NULL;
    }

    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE, s_zerofd, 0);
    if (ptr == MAP_FAILED)
        return NULL;

    return ptr;
}

void NvOsExecFree(void *ptr, size_t size)
{
    if (ptr == NULL)
        return;

   munmap(ptr, size);
}

typedef struct NvOsShmem
{
    char               path[NVOS_SHMEM_NAME_MAX_LENGTH];
    int                shmid;
    unsigned int       index;
} NvOsShmem;

NvError NvOsSharedMemAlloc(
    const char *key,
    size_t size,
    NvOsSharedMemHandle *descriptor)
{
    int ret;
    NvOsShmem *shmptr = NULL;
    struct nvos_shmem_op params;
    char path[NVOS_SHMEM_NAME_MAX_LENGTH];

    NV_ASSERT(key != NULL && descriptor != NULL);

    if (NvOsSharedMemPath(key, path, sizeof(path)) != NvSuccess)
        goto fail;

    shmptr = NvOsAlloc(sizeof(NvOsShmem));
    if (shmptr == NULL)
        goto fail;

    params.size = size;
    NvOsStrncpy(params.path, path, sizeof(params.path));
    ret = devctl(nvos_fd, NVOS_DEVCTL_SHMEM_ALLOC, &params, sizeof(params), NULL);
    if (ret != EOK)
        goto fail;

    shmptr->index = params.index;
    NvOsStrncpy(shmptr->path, path, NVOS_SHMEM_NAME_MAX_LENGTH);

    /* Open a shared memory object */
    shmptr->shmid = shm_open(path, O_RDWR, 0666);
    if (shmptr->shmid == -1)
    {
        ret = devctl(nvos_fd, NVOS_DEVCTL_SHMEM_FREE, &params, sizeof(params), NULL);
        goto fail;
    }

    *descriptor = (NvOsSharedMemHandle)shmptr;
    return NvSuccess;

fail:
    if (shmptr != NULL)
        NvOsFree(shmptr);
    return NvError_InsufficientMemory;
}

NvError NvOsSharedMemHandleFromFd(
    int fd,
    NvOsSharedMemHandle *descriptor)
{
    int ret;
    void *addr;
    int dupfd;
    off64_t phys;
    NvOsShmem *shmptr;
    struct nvos_shmem_op params;

    NV_ASSERT(descriptor != NULL);

    dupfd = dup(fd);
    if (dupfd == -1)
        return NvError_FileOperationFailed;

    shmptr = NvOsAlloc(sizeof(*shmptr));
    if (shmptr == NULL)
    {
        close(dupfd);
        return NvError_InsufficientMemory;
    }

    addr = mmap(NULL, 1, PROT_READ, MAP_SHARED, dupfd, 0);
    if (addr == MAP_FAILED)
        goto fail;

    ret = mem_offset64(addr, NOFD, 1, &phys, NULL);
    munmap(addr, 1);
    if (ret == -1)
        goto fail;

    params.phys = phys;
    ret = devctl(nvos_fd, NVOS_DEVCTL_SHMEM_DUP, &params, sizeof(params), NULL);
    if (ret != EOK)
        goto fail;

    shmptr->index = params.index;
    NvOsStrncpy(shmptr->path, params.path, NVOS_KEY_MAX);
    shmptr->shmid = dupfd;
    *descriptor = (NvOsSharedMemHandle)shmptr;
    return NvSuccess;

fail:
    NvOsFree(shmptr);
    close(dupfd);
    return NvError_InsufficientMemory;
}

NvError NvOsSharedMemGetFd(
    NvOsSharedMemHandle descriptor,
    int *fd)
{
    NvOsShmem *shmptr = (NvOsShmem *)descriptor;

    NV_ASSERT(fd != NULL);
    NV_ASSERT(shmptr != NULL);
    NV_ASSERT(shmptr->shmid != -1);

    if (fd == NULL || shmptr == NULL)
        return NvError_BadParameter;

    *fd = dup(shmptr->shmid);
    if (*fd == -1)
        return NvError_FileOperationFailed;
    return NvSuccess;
}

NvError NvOsSharedMemMap(
    NvOsSharedMemHandle desc,
    size_t offset,
    size_t size,
    void **ptr)
{
    NvOsShmem *shmptr = (NvOsShmem *)desc;
    void *addr;

    NV_ASSERT(shmptr != NULL && ptr != NULL);
    addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                shmptr->shmid, offset);
    if (addr == MAP_FAILED)
        return NvError_MemoryMapFailed;

    *ptr = addr;
    return NvSuccess;
}

void NvOsSharedMemUnmap(void *ptr, size_t size)
{
    if (ptr != NULL)
        (void)munmap(ptr, size);
}

void NvOsSharedMemFree(NvOsSharedMemHandle desc)
{
    int ret;
    struct nvos_shmem_op params;
    NvOsShmem *shmptr = (NvOsShmem *)desc;

    if (shmptr == NULL)
        return;

    params.index = shmptr->index;
    NvOsStrncpy(params.path, shmptr->path, sizeof(params.path));
    ret = devctl(nvos_fd, NVOS_DEVCTL_SHMEM_FREE, &params, sizeof(params), NULL);

    close(shmptr->shmid);

    NvOsFree(shmptr);
}

static NvError NvOsQnxPhysicalMemMap(
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr)
{
    NvError e = NvSuccess;
    void *addr;
    int id;
    int page_size;
    int page_offset;
    int prot = PROT_NONE;
    char path[NVOS_SHMEM_NAME_MAX_LENGTH];
    unsigned int shm_special = ARM_PTE_V6_S;

    NV_ASSERT(ptr != NULL);
    NV_ASSERT(size);

    if (flags & NVOS_MEM_READ)
        prot |= PROT_READ;
    if (flags & NVOS_MEM_WRITE)
        prot |= PROT_WRITE;
    if (flags & NVOS_MEM_EXECUTE)
        prot |= PROT_EXEC;
    if (attrib == NvOsMemAttribute_Uncached || attrib == NvOsMemAttribute_WriteCombined)
        prot |= PROT_NOCACHE;

    page_size = getpagesize();
    page_offset = phys & (page_size - 1);
    phys &= ~(page_size - 1);
    size += page_offset;
    size = (size + (page_size - 1)) & ~(page_size - 1);

    snprintf(path, NVOS_SHMEM_NAME_MAX_LENGTH, NVOS_SHMEM_FORMAT_STRING "-%d", ptr, getpid());
    id = shm_open(path, O_RDWR | O_CREAT | O_EXCL, S_IRWXU | S_IRWXG | S_IRWXO);
    if (id == -1)
        return NvError_MemoryMapFailed;

    switch (attrib)
    {
        case NvOsMemAttribute_Uncached:
            /* S/TEX/CB=1/000/00 */
            break;
        case NvOsMemAttribute_WriteCombined:
            /* S/TEX/CB=1/001/00 */
            shm_special |= ARM_PTE_V6_SP_TEX(1);
            break;
        default:
            /* S/TEX/CB=1/001/11 */
            shm_special |= ARM_PTE_V6_SP_TEX(1) | ARM_PTE_C | ARM_PTE_B;
            break;
    }

    if (shm_ctl_special(id, SHMCTL_PHYS, phys, size, shm_special) != 0)
    {
        e = NvError_MemoryMapFailed;
        goto fail;
    }

    addr = mmap(0, size, prot, MAP_SHARED | MAP_LAZY, id, 0);
    if (addr == MAP_FAILED)
    {
        e = NvError_MemoryMapFailed;
        goto fail;
    }

    NV_ASSERT(!(((NvUPtr)addr) & (page_size - 1)));
    *ptr = (char *)addr + page_offset;

fail:
    if (id != -1)
    {
        close(id);
        shm_unlink(path);
    }
    return e;
}

NvError NvOsPhysicalMemMap(
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags,
    void **ptr)
{
    NV_ASSERT(ptr != NULL);
    NV_ASSERT(size);

    if (flags == NVOS_MEM_NONE)
    {
        int fd;
        void *addr;

        fd = open("/dev/zero", O_RDWR);
        if (fd < 0)
            return NvError_MemoryMapFailed;

        addr = mmap(0, size, PROT_NONE, MAP_PRIVATE, fd, 0);
        close(fd);

        if (addr == MAP_FAILED)
            return NvError_MemoryMapFailed;

        *ptr = addr;
        return NvSuccess;
    }

    return NvOsQnxPhysicalMemMap(phys, size, attrib, flags, ptr);
}

void NvOsPhysicalMemUnmap(void *ptr, size_t size)
{
    int page_size;
    int page_offset;

    if (ptr == NULL)
        return;

    page_size = getpagesize();
    page_offset = (NvUPtr)ptr & (page_size - 1);

    ptr = (void *)((NvUPtr)ptr & ~(page_size - 1));
    size = size + page_offset;
    size = (size + page_size - 1) & ~(page_size - 1);

    munmap(ptr, size);
}

NvError NvOsPageAlloc(
    size_t size,
    NvOsMemAttribute attrib,
    NvOsPageFlags flags,
    NvU32 protect,
    NvOsPageAllocHandle *descriptor)
{
    NV_ASSERT(!"NvOsPageAlloc not implemented");
    return NvError_NotImplemented;
}

void NvOsPageFree(NvOsPageAllocHandle descriptor)
{
}

NvError NvOsPageLock(
    void *ptr,
    size_t size,
    NvU32 protect,
    NvOsPageAllocHandle *descriptor)
{
    NV_ASSERT(!"NvOsPageLock not implemented");
    return NvError_NotImplemented;
}

NvError NvOsPageMap(
    NvOsPageAllocHandle descriptor,
    size_t offset,
    size_t size,
    void **ptr)
{
    NV_ASSERT(!"NvOsPageMap not implemented");
    return NvError_NotImplemented;
}

NvError NvOsPageMapIntoPtr(
    NvOsPageAllocHandle descriptor,
    void *pCallerPtr,
    size_t offset,
    size_t size)
{
    NV_ASSERT(!"NvOsPageMapIntoPtr not implemented");
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
    NV_ASSERT(!"NvOsPageAddress not implemented");
    return 0;
}

NvError NvOsLibraryLoad(const char *name, NvOsLibraryHandle *library)
{
#ifndef BUILD_NO_DL_PROFILE
    char *n;
    void *lib;
    size_t len;
    char buff[NAME_MAX];

    NV_ASSERT(name != NULL);
    NV_ASSERT(library != NULL);

    len = strlen(name);
    if (len > (NAME_MAX - 4))
        return NvError_BadParameter;

    if (!strstr((const char *)name, ".so"))
    {
        memset(buff, 0, sizeof(buff));
        snprintf(buff, sizeof(buff), "%s.so", name);
        n = buff;
    }
    else
        n = (char *)name;

    lib = dlopen(n, RTLD_LAZY);
    if (lib == NULL)
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

void *NvOsLibraryGetSymbol(NvOsLibraryHandle library, const char *symbol)
{
#ifndef BUILD_NO_DL_PROFILE
    void *addr;

    NV_ASSERT(library != NULL);
    NV_ASSERT(symbol != NULL);

    addr = dlsym((void *)library, symbol);
    return addr;
#else
    return NULL;
#endif
}

void NvOsLibraryUnload(NvOsLibraryHandle library)
{
    if (library == NULL)
        return;
#ifndef BUILD_NO_DL_PROFILE
    dlclose((void *)library);
#endif
}

void NvOsSleepMSInternal(NvU32 msec)
{
    struct timespec ts_requested;
    struct timespec ts_remaining;
    NvU32 sec = msec / 1000;
    int rv;

    if (sec)
        msec -= (sec * 1000);

    ts_requested.tv_sec = sec;
    ts_requested.tv_nsec = msec * 1000 * 1000;
    do
    {
        rv = nanosleep(&ts_requested, &ts_remaining);
        ts_requested = ts_remaining;
    } while (rv < 0);
}

void NvOsWaitUS(NvU32 usec)
{
    if (g_UseCoopThread)
        return;

    NV_ASSERT(usec <= 1000);
    if (nanospin_ns(usec * 1000) != EOK)
    {
        NV_ASSERT(!"NvOsWaitUS: Failed in waiting");
    }
}

typedef struct NvOsMutexRec
{
    NvU32 count;
    pthread_mutex_t    mutex;
    volatile pthread_t owner;
} NvOsMutex;

NvError NvOsMutexCreateInternal(NvOsMutexHandle *mutex)
{
    NvOsMutex *m;
    NvError ret = NvSuccess;

    NV_ASSERT(mutex != NULL);

    m = NvOsAlloc(sizeof(NvOsMutex));
    if (m == NULL)
    {
        ret = NvError_InsufficientMemory;
        goto fail;
    }

    if (pthread_mutex_init(&m->mutex, 0) != EOK)
    {
        ret = NvError_ResourceError;
        goto fail;
    }

    m->count = 0;
    m->owner = (pthread_t)-1;
    *mutex = m;
    return NvSuccess;

fail:
    *mutex = 0;
    NvOsFree(m);
    return ret;
}

void NvOsMutexLockInternal(NvOsMutexHandle m)
{
    NvOsMutex *p = (NvOsMutex *)m;

    NV_ASSERT(p != NULL);

    if (p->owner == pthread_self())
        p->count++;
    else
    {
        pthread_mutex_lock(&p->mutex);
        NV_ASSERT((p->owner == (pthread_t)-1) && (p->count == 0));
        p->owner = pthread_self();
        p->count = 1;
    }
}

void NvOsMutexUnlockInternal(NvOsMutexHandle m)
{
    NvOsMutex *p = (NvOsMutex *)m;

    NV_ASSERT(p != NULL);

    if (p->owner == pthread_self())
    {
        p->count--;
        if (p->count == 0)
        {
            p->owner = (pthread_t)-1;
            pthread_mutex_unlock(&p->mutex);
        }
    }
    else
        NV_ASSERT(!"illegal thread id in unlock");
}

void NvOsMutexDestroyInternal(NvOsMutexHandle m)
{
    if (m == NULL)
        return;

    pthread_mutex_destroy( &m->mutex);
    NvOsFree(m);
}

typedef struct NvOsIntrMutexRec
{
    pthread_mutex_t mutex;
} NvOsIntrMutex;

NvError NvOsIntrMutexCreate(NvOsIntrMutexHandle *mutex)
{
    NvOsIntrMutex *m;

    NV_ASSERT(mutex != NULL);

    m = NvOsAlloc(sizeof(NvOsIntrMutex));
    if (m == NULL)
        return NvError_InsufficientMemory;

    if (pthread_mutex_init(&m->mutex, 0) != EOK)
    {
        NvOsFree(m);
        return NvError_ResourceError;
    }

    *mutex = m;
    return NvSuccess;
}

void NvOsIntrMutexLock(NvOsIntrMutexHandle mutex)
{
    NV_ASSERT(mutex != NULL);

    pthread_mutex_lock(&mutex->mutex);
}

void NvOsIntrMutexUnlock(NvOsIntrMutexHandle mutex)
{
    NV_ASSERT(mutex != NULL);

    pthread_mutex_unlock(&mutex->mutex);
}

void NvOsIntrMutexDestroy(NvOsIntrMutexHandle mutex)
{
    if (mutex == NULL)
        return;

    pthread_mutex_destroy(&mutex->mutex);
    NvOsFree(mutex);
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

    ret = pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts);
    switch (ret )
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

NvError NvOsSemaphoreCreateInternal(NvOsSemaphoreHandle *semaphore, NvU32 value)
{
    int ret;
    struct nvos_semaphore_op params;

    NV_ASSERT(semaphore != NULL);

    params.value = value;
    ret = devctl(nvos_fd, NVOS_DEVCTL_SEMAPHORE_CREATE, &params, sizeof(params), NULL);
    if (ret != EOK)
        return NvError_ResourceError;

    *semaphore = (NvOsSemaphoreHandle)params.index;
    return NvSuccess;
}

NvError NvOsSemaphoreCloneInternal(NvOsSemaphoreHandle s, NvOsSemaphoreHandle *semaphore)
{
    int ret;
    struct nvos_semaphore_op params;

    NV_ASSERT(semaphore != NULL);
    NV_ASSERT((uint32_t)s < NVOS_SEMAPHORE_TABLE_NUM_ENTRIES);

    params.index = (uint32_t)s;
    ret = devctl(nvos_fd, NVOS_DEVCTL_SEMAPHORE_CLONE, &params, sizeof(params), NULL);
    if (ret != EOK)
        return NvError_ResourceError;

    *semaphore = s;
    return NvError_Success;
}

NvError NvOsSemaphoreUnmarshal(
    NvOsSemaphoreHandle hClientSema,
    NvOsSemaphoreHandle *phDriverSema)
{
    int ret;
    struct nvos_semaphore_op params;

    NV_ASSERT(phDriverSema != NULL);
    NV_ASSERT((uint32_t)hClientSema < NVOS_SEMAPHORE_TABLE_NUM_ENTRIES);

    params.index = (uint32_t)hClientSema;
    ret = devctl(nvos_fd, NVOS_DEVCTL_SEMAPHORE_UNMARSHAL, &params, sizeof(params), NULL);
    if (ret != EOK)
        return NvError_ResourceError;

    *phDriverSema = hClientSema;
    return NvError_Success;
}

void NvOsSemaphoreWaitInternal(NvOsSemaphoreHandle s)
{
    int ret;
    struct nvos_semaphore *sema = semaphore_table + (uint32_t)s;

    NV_ASSERT((uint32_t)s < NVOS_SEMAPHORE_TABLE_NUM_ENTRIES);
    do
    {
        ret = sem_wait(&sema->sem);
    } while (ret == -1 && errno == EINTR);

    if (ret == -1)
        NvOsDebugPrintf("%s failed (%d)", __func__, errno);
}

NvError NvOsSemaphoreWaitTimeoutInternal(NvOsSemaphoreHandle s, NvU32 msec)
{
    int ret;
    struct timeval curr_time;
    struct timespec wait_time;
    struct nvos_semaphore *sema = semaphore_table + (uint32_t)s;

    NV_ASSERT((uint32_t)s < NVOS_SEMAPHORE_TABLE_NUM_ENTRIES);

    if (msec == NV_WAIT_INFINITE)
    {
        do
        {
            ret = sem_wait(&sema->sem);
        } while (ret == -1 && errno == EINTR);
    }
    else if (msec == 0)
    {
        ret = sem_trywait(&sema->sem);
    }
    else
    {
        gettimeofday(&curr_time, NULL);
        wait_time.tv_sec  = curr_time.tv_sec + (msec / 1000);
        wait_time.tv_nsec = ((msec % 1000) * 1000 + curr_time.tv_usec) * 1000;
        if (wait_time.tv_nsec >= 1000000000)
        {
            wait_time.tv_nsec -= 1000000000;
            wait_time.tv_sec  += 1;
        }

        do
        {
            ret = sem_timedwait(&sema->sem, &wait_time);
        } while (ret == -1 && errno == EINTR);
    }

    return ret ? NvError_Timeout : NvSuccess;
}

void NvOsSemaphoreSignalInternal(NvOsSemaphoreHandle s)
{
    struct nvos_semaphore *sema = semaphore_table + (uint32_t)s;

    NV_ASSERT((uint32_t)s < NVOS_SEMAPHORE_TABLE_NUM_ENTRIES);
    if (sem_post(&sema->sem) == -1)
        NvOsDebugPrintf("%s failed (%d)", __func__, errno);
}

void NvOsSemaphoreDestroyInternal(NvOsSemaphoreHandle s)
{
    int ret;
    struct nvos_semaphore_op params;

    params.index = (uint32_t)s;
    ret = devctl(nvos_fd, NVOS_DEVCTL_SEMAPHORE_DESTROY, &params, sizeof(params), NULL);
}

void NvOsThreadSetAffinity(NvU32 CpuHint)
{
    NV_ASSERT(!"NvOsThreadSetAffinity not implemented");
}

NvU32 NvOsThreadGetAffinity(void)
{
    NV_ASSERT(!"NvOsThreadGetAffinity not implemented");
    return NVOS_INVALID_CPU_AFFINITY;
}

NvU32 NvOsTlsAllocInternal(void)
{
    pthread_key_t key = 0;
    NV_ASSERT(sizeof(key) <= sizeof(NvU32));
    if (pthread_key_create(&key, NULL))
        return NVOS_INVALID_TLS_INDEX;
    NV_ASSERT(key != NVOS_INVALID_TLS_INDEX);
    return (NvU32)key;
}

void NvOsTlsFreeInternal(NvU32 TlsIndex)
{
    int fail;
    if (TlsIndex == NVOS_INVALID_TLS_INDEX)
        return;
    fail = pthread_key_delete((pthread_key_t)TlsIndex);
    NV_ASSERT(fail == EOK);
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
    NV_ASSERT(fail == EOK);
}

typedef struct NvOsThreadRec
{
    pthread_t thread;
} NvOsThread;

typedef struct
{
    NvS32 init;
    void *thread_args;
    NvOsThread     *thread;
    pthread_mutex_t barrier;
    NvOsThreadFunction function;
} NvOsThreadArgs;

static void *thread_wrapper(void *v)
{
    NvOsThreadArgs *args = v;

    NV_ASSERT(v != NULL);

    /*
     * indicate to the creator that this thread has started, use atomic
     * ops to ensure proper SMP memory barriers
     */
    (void)NvOsAtomicExchange32(&args->init, 1);
    pthread_mutex_lock(&args->barrier);
    pthread_mutex_unlock(&args->barrier);

    /* jump to user thread */
    args->function(args->thread_args);

    pthread_mutex_destroy(&args->barrier);
    NvOsFree(args);
    return 0;
}

/*
 * to ensure that terminators are run regardless of whether the thread
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

    while ((term = term_list) != NULL)
    {
        term_list = term->next;
        term->terminator(term->context);
        NvOsFree(term);
    }
}

NvError NvOsTlsAddTerminatorInternal(void (*func)(void*), void *context)
{
    NvOsTlsTerminator *term;
    NvOsTlsTerminator *term_list;

    if (g_terminator_key == NVOS_INVALID_TLS_INDEX)
    {
        /*
         * rather than creating yet another mutex for the rare case where
         * two threads each attempt to create a thread before the TLS key is
         * initialized, just reuse the time mutex
         */
        pthread_mutex_lock(&g_timemutex);
        if (g_terminator_key == NVOS_INVALID_TLS_INDEX)
            if (pthread_key_create(&g_terminator_key, run_terminator_list))
                g_terminator_key = NVOS_INVALID_TLS_INDEX;
        pthread_mutex_unlock(&g_timemutex);
    }
    if (g_terminator_key == NVOS_INVALID_TLS_INDEX)
        return NvError_InsufficientMemory;

    term_list = (NvOsTlsTerminator *)pthread_getspecific(g_terminator_key);

    term = NvOsAlloc(sizeof(*term));
    if (term == NULL)
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

NvError NvOsThreadCreateInternal(
    NvOsThreadFunction function,
    void *args,
    NvOsThreadHandle *thread,
    NvOsThreadPriorityType priority)
{
    int err;
    NvOsThread *t;
    NvOsThreadArgs *a;
    pthread_attr_t thr;
    struct sched_param param;

    NV_ASSERT(function != NULL);
    NV_ASSERT(thread != NULL);

    t = NvOsAlloc(sizeof(NvOsThread));
    if (t == NULL)
        return NvError_InsufficientMemory;

    NvOsMemset(t, 0, sizeof(NvOsThread));

    a = NvOsAlloc(sizeof(NvOsThreadArgs));
    if (a == NULL)
    {
        NvOsFree(t);
        return NvError_InsufficientMemory;
    }
    NvOsMemset(a, 0, sizeof(NvOsThreadArgs));

    a->function = function;
    a->thread = t;
    a->thread_args = args;
    pthread_mutex_init(&a->barrier, 0);

    if (priority == NvOsThreadPriorityType_NearInterrupt)
    {
        NvOsMemset(&thr, 0, sizeof(thr));
        pthread_attr_init(&thr);
        pthread_attr_setinheritsched(&thr, PTHREAD_EXPLICIT_SCHED);

        pthread_attr_setstackaddr(&thr, 0);
        pthread_attr_setschedpolicy(&thr, NVOS_SCHED_POLICY);

        param.sched_priority = NVOS_INTERRUPT_PRI;
        pthread_attr_setschedparam(&thr, &param);
    }

    /* create the pthread - use atomic ops to ensure proper SMP memory barriers */
    NvOsAtomicExchange32(&a->init, 0);
    /*
     * the thread is created with the mutex lock held, to prevent
     * race conditions between return parameter (*thread) assignment and
     * thread function execution
     */
    pthread_mutex_lock(&a->barrier);

    err = pthread_create(&t->thread,
                         (priority == NvOsThreadPriorityType_NearInterrupt) ? &thr : NULL,
                         thread_wrapper, a);
    if (err == EOK)
    {
        /*
         * wait for the thread to start - use atomic ops here to ensure proper
         * memory barriers are in place for SMP systems
         */
        while(!NvOsAtomicExchangeAdd32(&a->init, 0))
            NvOsSleepMS(1);

        /* set the thread id, allow the thread function to execute */
        *thread = t;
        pthread_mutex_unlock(&a->barrier);

        return NvSuccess;
    }

    pthread_mutex_unlock(&a->barrier);
    pthread_mutex_destroy(&a->barrier);
    NvOsFree(a);
    NvOsFree(t);
    *thread = 0;
    return NvError_InsufficientMemory;
}

void NvOsThreadJoinInternal(NvOsThreadHandle t)
{
    int e;

    if (t == NULL)
        return;

    e = pthread_join(t->thread, 0);
    if( e != EOK )
        return;

    NvOsFree(t);
}

void NvOsThreadYieldInternal(void)
{
    sched_yield();
}

NvBool NvOsQnxErrnoToNvError(NvError *e)
{
    switch (errno)
    {
        case EROFS:
            *e = NvError_ReadOnlyAttribute;
            break;
        case ENOENT:
            *e = NvError_FileNotFound;
            break;
        case EACCES:
            *e = NvError_AccessDenied;
            break;
        case ENOTDIR:
            *e = NvError_DirOperationFailed;
            break;
        case EFAULT:
            *e = NvError_InvalidAddress;
            break;
        case ENFILE:
            *e = NvError_NotSupported;
            break;
        case EMFILE:
            *e = NvError_NotSupported;
            break;
        case EIO:
            *e = NvError_ResourceError;
            break;
        case EADDRINUSE:
            *e = NvError_AlreadyAllocated;
            break;
        case EBADF:
            *e = NvError_FileOperationFailed;
            break;
        case EAGAIN:
            *e = NvError_Busy;
            break;
        case EADDRNOTAVAIL:
            *e = NvError_InvalidAddress;
            break;
        case EFBIG:
            *e = NvError_InvalidSize;
            break;
        case EINTR:
            *e = NvError_Timeout;
            break;
        case EALREADY:
            *e = NvError_AlreadyAllocated;
            break;
        case ENOTSUP:
            *e = NvError_NotSupported;
            break;
        case ENOSPC:
            *e = NvError_InvalidSize;
        case EPERM:
            *e = NvError_AccessDenied;
            break;
        case ETIME:
            *e = NvError_Timeout;
            break;
        case ETIMEDOUT:
            *e = NvError_Timeout;
            break;
        case ELOOP:
            *e = NvError_InvalidState;
            break;
        case ENXIO:
            *e = NvError_ModuleNotPresent;
            break;
        default:
            return NV_FALSE;
    }
    errno = 0;
    return NV_TRUE;
}

NvS32
NvOsAtomicCompareExchange32(NvS32 *pTarget, NvS32 OldValue, NvS32 NewValue)
{
#if NVCPU_IS_X86
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
        "mcr p15, 0, %5, c7, c10, 5\n"
        "ldrex %1, [%2]\n"
        "mov %0, #0\n"
        "teq %1, %3\n"
        "strexeq %0, %4, [%2]\n"
        "mcr p15, 0, %5, c7, c10, 5"
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
#if NVCPU_IS_X86
    __asm__ __volatile__("xchgl %0,%1"
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
        "   mcr p15, 0, %4, c7, c10, 5\n"
        "1: ldrex   %0, [%2]\n\t"
        "   strex   %1, %3, [%2]\n\t"
        "   teq     %1, #0\n\t"
        "   bne     1b\n"
        "   mcr p15, 0, %4, c7, c10, 5"
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
#if NVCPU_IS_X86
    __asm__ __volatile__("xaddl %0,%1"
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
        " mcr p15, 0, %5, c7, c10, 5\n"
        "1: ldrex %0, [%3]\n\t"
        " add %2, %0, %4\n\t"
        " strex %1, %2, [%3]\n\t"
        " teq %1, #0\n\t"
        " bne 1b\n"
        " mcr p15, 0, %5, c7, c10, 5"
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

    NV_ASSERT(hNvOsSystemtime != NULL);

    if (gettimeofday(&tp, NULL) != 0)
        return NvError_NotImplemented;

    hNvOsSystemtime->Seconds = tp.tv_sec;
    hNvOsSystemtime->Milliseconds = tp.tv_usec;
    return NvSuccess;
}

NvError
NvOsSetSystemTime(NvOsSystemTime *hNvOsSystemtime)
{
    return NvError_NotImplemented;
}

NvU32 NvOsGetTimeMS(void)
{
    NvU64 now;

    now = NvOsGetTimeUS();
    return (now / 1000);
}

NvU64 NvOsGetTimeUS(void)
{
    NvU64 now;

    now = ClockCycles();
    NV_ASSERT(SYSPAGE_ENTRY(qtime)->cycles_per_sec % 1000000 == 0);
    return now / (SYSPAGE_ENTRY(qtime)->cycles_per_sec / 1000000);
}

void NvOsDataCacheWriteback(void)
{
    NV_ASSERT(!"NvOsDataCacheWriteback not implemented");
}

void NvOsDataCacheWritebackInvalidate(void)
{
    NV_ASSERT(!"NvOsDataCacheWritebackInvalidate not implemented");
}

void NvOsDataCacheWritebackRange(void *start, NvU32 length)
{
    NV_ASSERT(!"NvOsDataCacheWritebackRange not implemented");
}

void NvOsDataCacheWritebackInvalidateRange(void *start, NvU32 length)
{
    NV_ASSERT(!"NvOsDataCacheWritebackInvalidateRange not implemented");
}

void NvOsInstrCacheInvalidate(void)
{
    NV_ASSERT(!"NvOsInstrCacheInvalidate not implemented");
}

void NvOsInstrCacheInvalidateRange(void *start, NvU32 length)
{
    NV_ASSERT(!"NvOsInstrCacheInvalidateRange not implemented");
}

void NvOsFlushWriteCombineBuffer(void)
{
    volatile int *dummy = (volatile int *)apbmisc_base + APBMISC_REG_CHIP_ID;

    /* drain CPU store buffers */
    __asm__ __volatile__("dsb");
    /* SO read of chip id reg to drain outer (L2) cache store buffers */
    (void)*dummy;
}

typedef struct NvOsInterruptThreadArgsRec
{
    void *context;
    NvOsInterruptHandler handler;
} NvOsInterruptThreadArgs;

typedef struct NvOsIntListRec
{
    NvU32  irq;
    NvS32  ch_id;
    NvS32  conn_id;
    NvS32  isr_id;
    NvBool valid;
    struct sigevent  sig_event;
    NvOsThreadHandle thread;
    NvOsInterruptThreadArgs arg;
} NvOsIntCtx;

typedef struct NvOsInterruptRec
{
    NvU32 num_irqs;
    NvOsIntCtx ctx[1];
} NvOsInterrupt;

static void QnxStubIntThreadWrapper(void *v)
{
    int rcvid;
    struct _pulse pulse;
    struct _msg_info msg;
    NvOsIntCtx *ctx = (NvOsIntCtx *)v;

    NV_ASSERT(ctx != NULL);

    for (;;)
    {
        rcvid = MsgReceivePulse(ctx->ch_id, (void *)&pulse, sizeof(pulse), &msg);
        if (rcvid < 0)
        {
            NV_ASSERT(!"ISR msg receive failure");
            break;
        }

        if ((NvU32)pulse.value.sival_ptr == 0xFFFFFFFF)
            break;

        ctx->arg.handler(ctx->arg.context);
    }
}

static void NvOsInterruptCleanup(NvOsInterruptHandle hIntr)
{
    int e;
    NvU32 i;

    if (hIntr == NULL)
        return;

    for (i = 0; i < hIntr->num_irqs; i++)
    {
        if (hIntr->ctx[i].valid)
        {
            e = MsgSendPulse(hIntr->ctx[i].conn_id,
                             sched_get_priority_max(NVOS_SCHED_POLICY),
                             _PULSE_CODE_MINAVAIL,
                             0xFFFFFFFF);
            NV_ASSERT(e >= 0);
            NvOsThreadJoin(hIntr->ctx[i].thread);
        }

        if (hIntr->ctx[i].isr_id)
        {
            e = InterruptDetach(hIntr->ctx[i].isr_id);
            NV_ASSERT(e >= 0);
        }

        if (hIntr->ctx[i].conn_id)
        {
            e = ConnectDetach(hIntr->ctx[i].conn_id);
            NV_ASSERT(e >= 0);
        }

        if (hIntr->ctx[i].ch_id)
        {
            e = ChannelDestroy(hIntr->ctx[i].ch_id);
            NV_ASSERT(e >= 0);
        }
    }
}

NvError NvOsInterruptRegister(
    NvU32 IrqListSize,
    const NvU32 *pIrqList,
    const NvOsInterruptHandler *pIrqHandlerList,
    void *context,
    NvOsInterruptHandle *handle,
    NvBool InterruptEnable)
{
    NvU32 i;
    NvU32 size;
    NvError e;
    NvOsInterrupt *intr;

    NV_ASSERT(IrqListSize);
    NV_ASSERT(pIrqList != NULL);
    NV_ASSERT(pIrqHandlerList != NULL);
    NV_ASSERT(handle != NULL);

    size = sizeof(NvOsInterrupt) + sizeof(NvOsIntCtx) * (IrqListSize - 1);
    intr = NvOsAlloc(size);
    if (intr == NULL)
        return NvError_InsufficientMemory;

    NvOsMemset(intr, 0, size);
    intr->num_irqs = IrqListSize;

    ThreadCtl_r(_NTO_TCTL_IO, 0);
    for (i = 0; i < IrqListSize; i++)
    {
        intr->ctx[i].arg.context = context;
        intr->ctx[i].arg.handler = pIrqHandlerList[i];
        intr->ctx[i].irq = pIrqList[i];

        intr->ctx[i].ch_id = ChannelCreate(_NTO_CHF_SENDER_LEN | _NTO_CHF_UNBLOCK);
        if (intr->ctx[i].ch_id < 0)
        {
            if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_ResourceError;
            goto fail;
        }

        intr->ctx[i].conn_id = ConnectAttach(ND_LOCAL_NODE, getpid(), intr->ctx[i].ch_id,
                                             _NTO_SIDE_CHANNEL, 0);
        if (intr->ctx[i].conn_id < 0)
        {
            if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_ResourceError;
            goto fail;
        }

        e = NvOsInterruptPriorityThreadCreate(QnxStubIntThreadWrapper, &intr->ctx[i],
                                              &intr->ctx[i].thread);
        if (e != NvError_Success)
            goto fail;

        intr->ctx[i].valid = NV_TRUE;

        SIGEV_PULSE_INIT(&intr->ctx[i].sig_event,
                         intr->ctx[i].conn_id,
                         NVOS_INTERRUPT_PRI,
                         _PULSE_CODE_MINAVAIL,
                         intr->ctx[i].irq);

        intr->ctx[i].isr_id = InterruptAttachEvent(intr->ctx[i].irq,
                                                    &intr->ctx[i].sig_event,
                                                    _NTO_INTR_FLAGS_PROCESS |
                                                    _NTO_INTR_FLAGS_TRK_MSK);
        if (intr->ctx[i].isr_id < 0)
        {
            if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_ResourceError;
            goto fail;
        }

        if (!InterruptEnable)
        {
            if (InterruptMask(intr->ctx[i].irq, intr->ctx[i].isr_id) < 0)
            {
                if (!NvOsQnxErrnoToNvError(&e))
                    e = NvError_ResourceError;
                goto fail;
            }
        }
    }

    *handle = intr;
    return e;

fail:
    NvOsInterruptCleanup(intr);
    NvOsFree(intr);
    return e;
}

void NvOsInterruptUnregister(NvOsInterruptHandle handle)
{
    if (handle == NULL)
        return;

    ThreadCtl_r(_NTO_TCTL_IO, 0);
    NvOsInterruptCleanup(handle);
    NvOsFree(handle);
}

NvError NvOsInterruptEnable(NvOsInterruptHandle handle)
{
    NvU32 i;
    NvError e = NvError_Success;

    NV_ASSERT(handle != NULL);

    ThreadCtl_r(_NTO_TCTL_IO, 0);
    for (i = 0; i < handle->num_irqs; i++)
    {
        if (InterruptUnmask(handle->ctx[i].irq, handle->ctx[i].isr_id) < 0)
        {
            if (!NvOsQnxErrnoToNvError(&e))
                e = NvError_ResourceError;
            break;
        }
    }

    return e;
}

void NvOsInterruptDone(NvOsInterruptHandle handle)
{
    NvU32 i;
    int e;

    if (handle == NULL)
        return;

    ThreadCtl_r(_NTO_TCTL_IO, 0);
    for (i = 0; i < handle->num_irqs; i++)
    {
        e = InterruptUnmask(handle->ctx[i].irq, handle->ctx[i].isr_id);
        NV_ASSERT(e >= 0);
    }
}

void NvOsInterruptMask(NvOsInterruptHandle handle, NvBool mask)
{
    NvU32 i;
    int status;

    if (!handle)
        return;

    ThreadCtl_r(_NTO_TCTL_IO, 0);
    for (i = 0; i < handle->num_irqs; i++)
    {
        if (mask)
            status = InterruptMask(handle->ctx[i].irq, handle->ctx[i].isr_id);
        else
            status = InterruptUnmask(handle->ctx[i].irq, handle->ctx[i].isr_id);

        NV_ASSERT(status >= 0);
    }
}

NvError NvOsPhysicalMemMapIntoCaller(
    void *pCallerPtr,
    NvOsPhysAddr phys,
    size_t size,
    NvOsMemAttribute attrib,
    NvU32 flags)
{
    NV_ASSERT(!"NvOsPhysicalMemMapIntoCaller not implemented");
    return NvError_NotImplemented;
}

void NvOsProfileApertureSizes(NvU32 *apertures, NvU32 *sizes)
{
    NV_ASSERT(apertures != NULL);
    *apertures = 0;
}

void NvOsProfileStart(void **apertures)
{
}

void NvOsProfileStop(void **apertures)
{
}

NvError NvOsProfileWrite(NvOsFileHandle file, NvU32 index, void *apertures)
{
    return NvError_NotSupported;
}

NvError NvOsGetOsInformation(NvOsOsInfo *pOsInfo)
{
    if (pOsInfo)
    {
        NvOsMemset(pOsInfo, 0, sizeof(NvOsOsInfo));
        pOsInfo->OsType = NvOsOs_Qnx;
        pOsInfo->Sku = NvOsSku_Unknown;
        return NvError_Success;
    }

    return NvError_BadParameter;
}

NvError NvOsSetConfigU32Internal(const char *name, NvU32 value)
{
    return NvError_NotImplemented;
}

NvError NvOsGetConfigU32Internal(const char *name, NvU32* value)
{
    return NvError_NotImplemented;
}

NvError NvOsSetConfigStringInternal(const char *name, const char *value)
{
    return NvError_NotSupported;
}

NvError NvOsGetConfigStringInternal(const char *name, char *value, NvU32 size)
{
    const char* str = NULL;

    NV_ASSERT(name != NULL);
    NV_ASSERT(value != NULL);
    NV_ASSERT(size);

    str = getenv(name);
    if (str == NULL)
        return NvError_ConfigVarNotFound;

    (void)strncpy(value, str, size);
    value[size-1] = '\0';
    return NvSuccess;
}

NvError NvOsGetSysConfigStringInternal(const char *name, char *value, NvU32 size)
{
    return NvOsGetConfigStringInternal(name, value, size);
}

NvError NvOsSetSysConfigStringInternal(const char *name, const char *value)
{
    return NvOsSetConfigStringInternal(name, value);
}

NvError NvOsConfigSetStateInternal(
    int stateId,
    const char *name,
    const char *value,
    int valueSize,
    int flags)
{
    return NvError_NotSupported;
}

NvError NvOsConfigGetStateInternal(
    int stateId,
    const char *name,
    char *value,
    int valueSize,
    int flags)
{
    return NvError_NotSupported;
}

NvError NvOsConfigQueryStateInternal(
    int stateId,
    const char **ppKeys,
    int *pNumKeys,
    int maxKeySize)
{
    return NvError_NotSupported;
}

static void __attribute__ ((constructor)) nvos_init(void)
{
    if (NvOsTableInit())
        NvOsDebugPrintf("%s: NvOsTableInit failed\n", __func__);
}

static void __attribute__ ((destructor)) nvos_fini(void)
{
    NvOsTableDestroy();
}

void NvOsGetProcessInfo (char* buf, NvU32 len)
{
    char    buffer[128] = { 0 };
    FILE*   cmdline;

    cmdline = fopen("/proc/self/cmdline", "r");
    if (cmdline)
    {
        fread(buffer, 1, 128, cmdline);
        fclose(cmdline);
    }

    NvOsSnprintf(buf, len, "%s (pid %d)", buffer[0] ? buffer : "-", getpid());
}

int NvOsSetFpsTarget(int target) { return -1; }
int NvOsModifyFpsTarget(int fd, int target) { return -1; }
void NvOsCancelFpsTarget(int fd) { }
int NvOsSendThroughputHint (const char *usecase, NvOsTPHintType type, NvU32 value, NvU32 timeout_ms) { return 0; }
int NvOsVaSendThroughputHints (NvU32 client_tag, ...) { return 0; }
void NvOsCancelThroughputHint (const char *usecase) { }
