/*
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef NVSYNC_H
#define NVSYNC_H

#include "sync/sync.h"
#include "cutils/log.h"
#include "nvassert.h"
#include "nvrm_channel.h"
#include "nvgr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enables trace logging of nvsync functions.
 */
#ifndef NVSYNC_TRACE
#define NVSYNC_TRACE 0
#endif

/**
 * @brief Asserts whenever an error occurs.
 * If one of the nvsync functions encounters an error then it will assert.
 */
#ifndef NVSYNC_BREAK_ON_ERROR
#define NVSYNC_BREAK_ON_ERROR 0
#endif

/**
 * @brief Enables dumping of sync fds whenever an error occurs.
 * If one of the nvsync function encounters an error, then information about all
 * the current processes sync fds are dumped to the log.
 */
#ifndef NVSYNC_DUMP_ON_ERROR
#define NVSYNC_DUMP_ON_ERROR 0
#endif

/**
 * @brief Value that can represent an invalid sync fd.
 * This can be used to initialize a variable to hold an invalid sync fd. It must
 * not be used to test whether an existing sync fd is valid, as any value below
 * 0 can represent an invalid sync fd.
 */
#define NVSYNC_INIT_INVALID -1

#if NVSYNC_TRACE
#define NVSYNC_LOG(...) ALOGD("Sync: "__VA_ARGS__)
#else
#define NVSYNC_LOG(...)
#endif

#define NVSYNC_ERROR(...) \
    ALOGE("Sync: ERROR "__VA_ARGS__); \
    if (NVSYNC_DUMP_ON_ERROR) nvgr_sync_dump(); \
    NV_ASSERT(!NVSYNC_BREAK_ON_ERROR)

/**
 * @brief Tests whether a sync fd is valid.
 *
 * This function provides a safe way to test if the specified sync fd is valid.
 *
 * @param fd Sync fd to test.
 * @retval 0 if the sync fd is invalid, otherwise a non-zero value.
 */
static inline int
nvsync_is_valid(int fd)
{
    return fd >= 0;
}

/**
 * @brief Returns name of sync fd.
 *
 * This functions queries the specified sync fd for its name that was given at
 * creation time, and stores it in the specified string. A pointer to this
 * string is returned.
 *
 * @param fd        Sync fd to query.
 * @param str       Memory buffer to store name.
 * @param num_bytes Size of memory buffer.
 * @retval Name of the sync fd.
 */
static inline const char *
nvsync_name(int fd,
            char *str,
            int num_bytes)
{
    struct sync_fence_info_data *data;

    data = sync_fence_info(fd);

    if (data != NULL) {
        snprintf(str, num_bytes, "<%d:%s>", fd, data->name);
        sync_fence_info_free(data);
    } else {
        snprintf(str, num_bytes, "<%d:UNKNOWN>", fd);
    }

    return str;
}

/**
 * @brief Waits for a sync fd to be signalled.
 *
 * This function will wait on the specified sync fd to be signalled. If the sync
 * fd has not been signalled within the specified timeout, the function will
 * return early and indicate an error.
 *
 * @param fd      Sync fd to wait for.
 * @param timeout Maximum number of milliseconds to wait.
 * @param func    Name of calling function.
 * @retval 0 if successful, otherwise a non-zero value.
 */
static inline int
nvsync_wait(int fd,
            int timeout,
            const char *func)
{
    int result;
    char str[32];

    if (!nvsync_is_valid(fd)) {
        return 0;
    }

    result = sync_wait(fd, timeout);

    if (result != 0) {
        NVSYNC_ERROR("sync_wait(%s, %d) returned %d [%s]",
                     nvsync_name(fd, str, sizeof(str)),
                     timeout,
                     result,
                     func);
    } else {
        NVSYNC_LOG("sync_wait(%s, %d) [%s]",
                   nvsync_name(fd, str, sizeof(str)),
                   timeout,
                   func);
    }

    return result;
}

/**
 * @brief Closes a sync fd.
 *
 * @param fd   Sync fd to close.
 * @param func Name of calling function.
 * @retval 0 if successful, otherwise a non-zero value.
 */
static inline int
nvsync_close(int fd,
             const char *func)
{
    int result;
    char str[32];

    if (!nvsync_is_valid(fd)) {
        return 0;
    }

    #if NVSYNC_TRACE
    nvsync_name(fd, str, sizeof(str));
    #endif

    result = close(fd);

    if (result != 0) {
        NVSYNC_ERROR("close(%s) returned %d [%s]",
                     #if NVSYNC_TRACE
                     str,
                     #else
                     nvsync_name(fd, str, sizeof(str)),
                     #endif
                     result,
                     func);
    } else {
        NVSYNC_LOG("close(%s) [%s]", str, func);
    }

    return result;
}

/**
 * @brief Creates a new sync fd by duplicating an existing fd.
 *
 * This function creates a new sync fd, by duplicating the information from an
 * existing sync fd. The existing sync fd is untouched by this operation.
 *
 * @param name Name to give to new sync fd.
 * @param fd   Sync fd to duplicate.
 * @param func Name of calling function.
 * @retval New sync fd.
 */
static inline int
nvsync_dup(const char *name,
           int fd,
           const char *func)
{
    int result;
    char str1[32];
    char str2[32];

    if (!nvsync_is_valid(fd)) {
        return NVSYNC_INIT_INVALID;
    }

    #if NVSYNC_TRACE
    result = sync_merge(name, fd, fd);
    #else
    result = dup(fd);
    #endif

    if (!nvsync_is_valid(result)) {
        NVSYNC_ERROR("dup(%s) returned %d [%s]",
                     nvsync_name(fd, str1, sizeof(str1)),
                     result,
                     func);
    } else {
        NVSYNC_LOG("dup(%s) => %s [%s]",
                   nvsync_name(fd, str1, sizeof(str1)),
                   nvsync_name(result, str2, sizeof(str2)),
                   func);
    }

    return result;
}

/**
 * @brief Creates a new sync fd by combining two existing fds.
 *
 * This function creates a new sync fd, by combining the information from two
 * existing sync fds. The existing sync fds are untouched by this operation.
 *
 * @param name Name to give to new sync fd.
 * @param fd1  First sync fd to combine.
 * @param fd2  Second sync fd to combine.
 * @param func Name of calling function.
 * @retval New sync fd.
 */
static inline int
nvsync_merge(const char *name,
             int fd1,
             int fd2,
             const char *func)
{
    int result;
    char str1[32];
    char str2[32];
    char str3[32];

    if (fd1 == fd2) {
        return nvsync_dup(name, fd1, func);
    }

    if (!nvsync_is_valid(fd1)) {
        return nvsync_dup(name, fd2, func);
    }

    if (!nvsync_is_valid(fd2)) {
        return nvsync_dup(name, fd1, func);
    }

    result = sync_merge(name, fd1, fd2);

    if (!nvsync_is_valid(result)) {
        NVSYNC_ERROR("sync_merge(%s, %s, %s) returned %d [%s]",
                     name,
                     nvsync_name(fd1, str1, sizeof(str1)),
                     nvsync_name(fd2, str2, sizeof(str2)),
                     result,
                     func);
    } else {
        NVSYNC_LOG("sync_merge(%s, %s, %s) => %s [%s]",
                   name,
                   nvsync_name(fd1, str1, sizeof(str1)),
                   nvsync_name(fd2, str2, sizeof(str2)),
                   nvsync_name(result, str3, sizeof(str3)),
                   func);
    }

    return result;
}

/**
 * @brief Extracts the fences from a sync fd.
 *
 * This function extracts the information from a sync fd and converts it to an
 * array of NvRmFence objects. The sync fd is untouched by this operation.
 *
 * The caller is responsible for passing an array of fences that is large enough
 * to store all the required NvRmFence objects. The size of the array must be
 * specified by the caller as input to this function in the numFences parameter,
 * and the function itself will return the actual number of converted fences in
 * the same parameter.
 *
 * This function can also be used to just query the number of required fences,
 * by passing NULL in the fences parameter; the function will then return the
 * number of fences in the numFences parameter without actually writing to the
 * fences array.
 *
 * @param fd        Sync fd to extract from.
 * @param fences    Pointer to array of fences.
 * @param numFences Number of fences in fences array.
 * @param func      Name of calling function.
 * @retval 0 if successful, otherwise a non-zero value.
 */
static inline int
nvsync_to_fence(int fd,
                NvRmFence *fences,
                NvU32 *numFences,
                const char *func)
{
    NvError err;
    char str[32];

    NV_ASSERT(numFences);

    if (!nvsync_is_valid(fd)) {
        *numFences = 0;
        return 0;
    }

    err = NvRmFenceGetFromFile(fd, fences, numFences);

    if (err != NvSuccess) {
        *numFences = 0;
        NVSYNC_ERROR("sync_to_fence(%s) returned %x [%s]",
                     nvsync_name(fd, str, sizeof(str)),
                     err,
                     func);
    } else {
        NVSYNC_LOG("sync_to_fence(%s) [%s]",
                   nvsync_name(fd, str, sizeof(str)),
                   func);
    }

    return err == NvSuccess ? 0 : -1;
}

/**
 * @brief Creates a new sync fd from an existing list of fences.
 *
 * This function creates a new sync fd, using the information from the specified
 * array of NvRmFence objects.
 *
 * @param name      Name to give to new sync fd.
 * @param fences    Pointer to array of fences.
 * @param numFences Number of fences in fences array.
 * @param func      Name of calling function.
 * @retval New sync fd.
 */
static inline int
nvsync_from_fence(const char *name,
                  const NvRmFence *fences,
                  NvU32 numFences,
                  const char *func)
{
    int fd;
    NvError err;
    char str[32];

    NV_ASSERT(fences);

    if (numFences == 0) {
        return NVSYNC_INIT_INVALID;
    }

    err = NvRmFencePutToFile(name, fences, numFences, &fd);

    if (err != NvSuccess) {
        NVSYNC_ERROR("sync_from_fence() returned %x [%s]", err, func);
        fd = NVSYNC_INIT_INVALID;
    } else {
        NVSYNC_LOG("sync_from_fence() => %s [%s]",
                   nvsync_name(fd, str, sizeof(str)),
                   func);
    }

    return fd;
}

#define nvsync_wait(fd, timeout)      nvsync_wait(fd, timeout, __func__)
#define nvsync_close(fd)              nvsync_close(fd, __func__)
#define nvsync_dup(name, fd)          nvsync_dup(name, fd, __func__)
#define nvsync_merge(name, fd1, fd2)  nvsync_merge(name, fd1, fd2, __func__)
#define nvsync_to_fence(fd, f, n)     nvsync_to_fence(fd, f, n, __func__)
#define nvsync_from_fence(name, f, n) nvsync_from_fence(name, f, n, __func__)

#ifdef __cplusplus
}
#endif

#endif /* NVSYNC_H */
