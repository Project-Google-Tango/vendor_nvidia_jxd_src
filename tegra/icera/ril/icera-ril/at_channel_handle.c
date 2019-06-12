/* Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved. */
/*
** Copyright 2010, Icera Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** You may not use this file except in compliance with the License.
** You may obtain a copy of the License at:
**
** http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** imitations under the License.
*/
/*
**
** Copyright 2006, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/**
* @file at_channel_handle.c implementation of the AT Channel handle type
*
* The AT Channel handle implements functions which maintain the AT Channel
* specific parameters.
*/

/*************************************************************************************************
* Project header files. The first in this list should always be the public interface for this
* file. This ensures that the public interface header file compiles standalone.
************************************************************************************************/
#include "at_channel_handle.h"  /* this */
#include "icera-ril.h"
#define LOG_NDEBUG 0        /**< enable (==0)/disable(==1) log output for this file */
#define LOG_TAG atchLogTag  /**< log tag for this file */
#include <utils/Log.h>      /* ALOGD, LOGE, LOGI */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <malloc.h>     /* malloc/free */
#include <memory.h>     /* memset */

/*************************************************************************************************
* Private variable definitions
************************************************************************************************/
static pthread_key_t  key; /**< thread-specific data key visible to all AT Channel threads */
static pthread_once_t key_once = PTHREAD_ONCE_INIT; /**< thread initialization flag */

/*************************************************************************************************
* Private function definitions
************************************************************************************************/
/**
 * This function creates a thread-specific data key visible to all threads in the process
 */
static void at_channel_handle_make_key( void )
{
    int err = 0;

    err = pthread_key_create(&key, NULL);
    if ( 0 != err )
    {
        ALOGE("at_channel_handle_set() pthread_key_create() failed with err %d\n", err);
        return;
    }

    ALOGD("at_channel_handle_set() pthread_key_create() success\n");
}

/*************************************************************************************************
* Public function definitions
************************************************************************************************/

at_channel_handle_t* at_channel_handle_create( void )
{
    int                  err        = 0;    /* 0 == no error */
    at_channel_handle_t *new_handle = NULL; /* NULL == invalid handle */

    err = pthread_once(&key_once, at_channel_handle_make_key);
    if ( 0 != err )
    {
        ALOGE("at_channel_handle_create() pthread_once() failed with error %d for thread %x\n",
              err, (unsigned int)pthread_self());
        return NULL;
    }

    /* try to get an already existing handle. If there is already a handle for this AT Channel (thread)
       just skip allocation of a new object and return the existing one */
    new_handle = pthread_getspecific( key );
    if ( NULL == new_handle )
    {
        /* allocate memory for new AT Channel handle */
        new_handle = malloc(sizeof(at_channel_handle_t));
        if ( NULL != new_handle )
        {
            /* initialize new AT Channel handle */
            memset(new_handle, 0, sizeof(at_channel_handle_t));

            new_handle->fd = -1;
            new_handle->ATBufferCur = new_handle->ATBuffer;
            memset( new_handle->at_cmd, 0, sizeof( new_handle->at_cmd ) );
            (void)pthread_mutex_init(&new_handle->commandmutex, NULL);
            (void)pthread_cond_init(&new_handle->commandcond, NULL);

            /* store AT Channel handle for current thread */
            err = pthread_setspecific(key, new_handle);
            if ( 0 != err )
            {
                free(new_handle);
                new_handle = NULL;
                ALOGE("at_channel_handle_create() pthread_setspecific() failed with err %d\n", err);
            }
        }
        else
        {
            ALOGE("at_channel_handle_create() memory allocation for new AT channel handle failed: %d\n", err);
        }
    }

    return new_handle;
}

void at_channel_handle_destroy( at_channel_handle_t* handle )
{
    ALOGD("at_channel_handle_destroy() destroy AT channel handle %p\n", handle);

    /* unallocate/uninitialize AT Channel specific synchronization objects */
    (void)pthread_mutex_destroy(&handle->commandmutex);
    (void)pthread_cond_destroy(&handle->commandcond);

    /* unallocate handle memory */
    free(handle);
}

int at_channel_handle_set( const at_channel_handle_t* handle )
{
    int err = 0; /* 0 == no error */

    err = pthread_once(&key_once, at_channel_handle_make_key);
    if ( 0 != err )
    {
        ALOGE("at_channel_handle_set() pthread_once() failed with err %d for thread %x\n",
              err, (unsigned int)pthread_self());
        return err;
    }

    /* register AT Channel handle for this specific thread */
    err = pthread_setspecific(key, handle);
    if ( 0 != err )
    {
      ALOGE("at_channel_handle_set() pthread_setspecific() failed with err %d for thread %x\n",
        err, (unsigned int)pthread_self());
      /* no return, use final function return */
    }

    return err;
}

at_channel_handle_t* at_channel_handle_get( void )
{
    int err = 0;                       /* 0 == no error */
    at_channel_handle_t *handle = NULL;  /* NULL == invalid handle */

    err = pthread_once(&key_once, at_channel_handle_make_key);
    if ( 0 != err )
    {
      ALOGE("at_channel_handle_get() pthread_once() failed with err %d for thread %x\n",
            err, (unsigned int)pthread_self());
      return NULL;
    }

    handle = pthread_getspecific(key);
    if ( NULL == handle )
    {
      ALOGE("at_channel_handle_get() pthread_getspecific() failed with err %d for thread %x\n",
            err, (unsigned int)pthread_self());
    }

    return handle;
}
