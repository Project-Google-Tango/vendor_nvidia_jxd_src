/*
 * POSIX header for AOS
 *
 * Copyright 2012 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef _NVPOSIX_SEMAPHORE_H_
#define _NVPOSIX_SEMAPHORE_H_

#include <time.h>

typedef union {
    long int value;
} sem_t;

extern int sem_init(sem_t *sem, int pshared, unsigned int value);
extern int sem_destroy(sem_t *sem);
extern int sem_post(sem_t *sem);
extern int sem_wait(sem_t *sem);
extern int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);

#endif // #ifndef _NVPOSIX_SEMAPHORE_H_
