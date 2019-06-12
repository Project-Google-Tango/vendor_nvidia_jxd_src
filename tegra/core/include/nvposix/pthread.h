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

#ifndef _NVPOSIX_PTHREAD_H_
#define _NVPOSIX_PTHREAD_H_

typedef struct {
    int volatile value;
} pthread_mutex_t;

typedef struct {
    unsigned int flags;
    void *stack_base;
    size_t stack_size;
    size_t guard_size;
    int sched_policy;
    int sched_priority;
} pthread_attr_t;

typedef long pthread_mutexattr_t;
typedef int pthread_key_t;
typedef long pthread_t;

extern int pthread_create(pthread_t *thread,
                          const pthread_attr_t *attr,
                          void *(*start_routine)(void *), void *arg);
extern int pthread_join(pthread_t thread, void **value_ptr);

extern int pthread_mutex_init(pthread_mutex_t *mutex,
                              const pthread_mutexattr_t *attr);
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);
extern int pthread_mutex_lock(pthread_mutex_t *mutex);
extern int pthread_mutex_unlock(pthread_mutex_t *mutex);

extern int pthread_key_create(pthread_key_t *key,
                              void (*destructor_function)(void *));
extern int pthread_key_delete(pthread_key_t key);
extern int pthread_setspecific(pthread_key_t key, const void *value);
extern void *pthread_getspecific(pthread_key_t key);

#endif // #ifndef _NVPOSIX_PTHREAD_H_
