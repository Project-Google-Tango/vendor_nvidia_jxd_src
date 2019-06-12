/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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


#if NVOS_TRACE || NV_DEBUG
#undef NvOsSharedMemAlloc
#endif

NvError NvOsSharedMemAlloc(
    const char *key,
    size_t size,
    NvOsSharedMemHandle *descriptor)
{
    int fd;
    void *ptr;
    int err;
    int clearSharedMemory = 1;
#if !defined(ANDROID)
    char path[NVOS_PATH_MAX];
    NvError ret;
#endif

    // Create a file descriptor for a new, or an existing, shared memory object.
#if !defined(ANDROID)
    ret = NvOsSharedMemPath(key, path, sizeof(path));
    if (ret != NvSuccess)
        return ret;

    mode_t mode = 0666; // Use read-write permissions for everyone.
    // Linux we use the POSIX Shared Memory API.
    fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, mode);
    if (fd < 0) {
        // If a shared memory object already exists for that key
        // the call will fail and the EEXIST error will be set.
        if (errno == EEXIST) {
            // Re-try if the memory object already exists.
            fd = shm_open(path, O_RDWR, mode);
            if (fd < 0) {
                return NvError_InsufficientMemory;
            }
            // An existing shared memory object must not be cleared.
            clearSharedMemory = 0;
        } else {
            return NvError_InsufficientMemory;
        }
    }
    // A new shared memory object initially has a size of zero.
    // Truncate the file descriptor to the specified size.
    err = ftruncate(fd, size);
    if (err != 0) {
        close(fd);
        return NvError_InsufficientMemory;
    }
#else
    // On Android we use the Anonymous Shared Memory API (/dev/ashmem).
    fd = ashmem_create_region(key, size);
    if (fd < 0) {
        return NvError_InsufficientMemory;
    }
#endif

    if (clearSharedMemory) {
        // Map the shared memory in order to clear.
        ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
            close(fd);
            return NvError_InsufficientMemory;
        }

        // Clear the shared memory.
        NvOsMemset(ptr, 0, size);

        // Unmap after clearing.
        // Since the link is still there, the content is preserved after un-mapping.
        err = munmap(ptr, size);
        NV_ASSERT(err == 0);
    }

    return NvOsSharedMemHandleFromFd(fd, descriptor);
}

