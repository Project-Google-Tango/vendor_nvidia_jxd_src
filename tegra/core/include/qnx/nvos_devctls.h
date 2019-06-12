/*
 * Copyright (c) 2011-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef __NVOS_DEVCTLS_H_INCLUDED
#define __NVOS_DEVCTLS_H_INCLUDED

#include <stdint.h>
#include <semaphore.h>
#include <devctl.h>

#define DEV_NVOS "/dev/nvos"

#define NVOS_SHMEM_NAME_MAX_LENGTH          128
#define NVOS_SHMEM_NAME_PREFIX              "/NVshm-"
#define NVOS_SHMEM_FORMAT_STRING            NVOS_SHMEM_NAME_PREFIX "%p"

#define NVOS_SEMAPHORE_TABLE_SHMEM_PATH     NVOS_SHMEM_NAME_PREFIX "sem"
#define NVOS_SEMAPHORE_TABLE_NUM_ENTRIES    256

struct nvos_semaphore_op {
    uint32_t index;
    uint32_t value;
};

struct nvos_pid_node {
    pid_t pid;
    uint32_t refcount;
    struct nvos_pid_node *next;
};

struct nvos_semaphore {
    sem_t sem;
    struct nvos_pid_node *pid_list;
};

struct nvos_shmem_op {
    char path[NVOS_SHMEM_NAME_MAX_LENGTH];
    size_t size;
    uint32_t index;
    off64_t phys;
};

#define NVOS_DEVCTL_SEMAPHORE_CREATE       __DIOTF(_DCMD_MISC, 0x20, struct nvos_semaphore_op)
#define NVOS_DEVCTL_SEMAPHORE_DESTROY      __DIOTF(_DCMD_MISC, 0x21, struct nvos_semaphore_op)
#define NVOS_DEVCTL_SEMAPHORE_CLONE        __DIOTF(_DCMD_MISC, 0x22, struct nvos_semaphore_op)
#define NVOS_DEVCTL_SEMAPHORE_UNMARSHAL    __DIOTF(_DCMD_MISC, 0x23, struct nvos_semaphore_op)
#define NVOS_DEVCTL_SHMEM_ALLOC            __DIOTF(_DCMD_MISC, 0x24, struct nvos_shmem_op)
#define NVOS_DEVCTL_SHMEM_FREE             __DIOTF(_DCMD_MISC, 0x25, struct nvos_shmem_op)
#define NVOS_DEVCTL_SHMEM_DUP              __DIOTF(_DCMD_MISC, 0x26, struct nvos_shmem_op)

#endif
