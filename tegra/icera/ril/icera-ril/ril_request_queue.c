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
* @file ril_request_queue.c implementation of the RIL Request queue
*
* This file implements functions for sending RIL requests via a FIFO mechanism over thread borders.
*/

/*************************************************************************************************
* Project header files. The first in this list should always be the public interface for this
* file. This ensures that the public interface header file compiles standalone.
************************************************************************************************/
#include "ril_request_queue.h" /* this */

#include "icera-ril.h"

#define LOG_NDEBUG 0        /**< enable (==0)/disable(==1) log output for this file */
#define LOG_TAG reqqLogTag  /**< log tag for this file */
#include <utils/Log.h>      /* ALOGD, LOGE, LOGI */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <pthread.h>  /* pthread_mutex_t, pthread_cond_t, ... */
#include <malloc.h>  /* malloc, free */

/*************************************************************************************************
* Private type definitions
************************************************************************************************/

/** RIL Request container which is defined to keep the RIL Request data independent on the
     storage information and mechanism */
typedef struct _rilq_req_container_t
{
    struct _rilq_req_container_t* next; /**< next RIL request container */
    void*  request;                     /**< RIL Request or Unsolicited Response data */
    rilq_object_type_t type;            /**< Ril Object tape,
                                             e.g. Request or Unsolicited Response */
}
rilq_container_t;

/** RIL Request queue handle, which stores all queue-specific parameters */
typedef struct _rilq_handle_t
{
    pthread_mutex_t   qlock;            /**< mutext for protecting the queue r/w-access from
                                               different thread contexts */
    pthread_cond_t    qcond;            /**< signal which is set when an object is put into
                                              the queue; the get function blocks if the queue is
                                              empty until the signal is set */
    rilq_container_t* req_cont_first;   /**< first container element of RIL request queue */
    rilq_container_t* req_cont_last;    /**< last container element of RIL request queue */
    unsigned long     req_count;        /**< counter of requests currently in the queue */
    rilq_container_t* req_lowp_cont_first;   /**< first container element of RIL request low priority queue */
    rilq_container_t* req_lowp_cont_last;    /**< last container element of RIL request low priority queue */
    unsigned long     req_lowp_count;        /**< counter of low priority requests currently in the queue */
    rilq_container_t* unsol_cont_first; /**< first container element of unsolicited event queue */
    rilq_container_t* unsol_cont_last;  /**< last container element of unsolicited event queue */
    unsigned long     unsol_count;      /**< counter of unsolicited responses currently in the queue */
    int               closing;          /**< flag which indicates we are about to destroy the queue */
}
rilq_handle_t;

/*************************************************************************************************
* Public function definitions
************************************************************************************************/

unsigned long rilq_get_req_count(const struct _rilq_handle_t* handle )
{
  unsigned long ret = 0;
  if ( NULL != handle )
  {
    ret = handle->req_count + handle->req_lowp_count;
  }
  return ret;
}

unsigned long rilq_get_unsol_count(const struct _rilq_handle_t* handle)
{
  unsigned long ret = 0;
  if ( NULL != handle )
  {
    ret = handle->unsol_count;
  }
  return ret;
}

struct _rilq_handle_t* rilq_create( void )
{
    rilq_handle_t* handle = (rilq_handle_t*)malloc(sizeof(rilq_handle_t));

    if ( NULL != handle )
    {
        pthread_mutex_init(&handle->qlock, NULL);
        pthread_cond_init (&handle->qcond, NULL);

        pthread_mutex_lock(&handle->qlock);

        handle->req_cont_first = NULL;
        handle->req_count = 0;
        handle->req_lowp_cont_first = NULL;
        handle->req_lowp_count = 0;
        handle->unsol_cont_first = NULL;
        handle->unsol_count = 0;
        handle->closing = 0;

        pthread_mutex_unlock(&handle->qlock);
    }

    return handle;
}

void rilq_destroy( struct _rilq_handle_t* handle )
{
    if ( NULL != handle )
    {
        handle->closing = 1; /* do not accept any ril requests/unsol events anymore */

        if ( handle->req_cont_first || handle->req_lowp_cont_first || handle->unsol_cont_first )
        {
            rilq_clear(handle);
        }

        pthread_cond_destroy (&handle->qcond);
        pthread_mutex_destroy(&handle->qlock);

        free(handle);
    }
}

int rilq_put_object(struct _rilq_handle_t* handle, void* data,  rilq_object_type_t type)
{
    rilq_container_t* cont;

    if ( NULL == handle || 0 != handle->closing )
    {
       ALOGD("rilq_put() handle: %p closing: %d\n", handle, (handle)?handle->closing:0);
       return -1; /* queue no longer accepts new objects... destruction in progress */
    }

    cont = (rilq_container_t*) malloc(sizeof(rilq_container_t));

    if ( NULL == cont )
    {
      return -1;
    }

    cont->request = data;
    cont->next = NULL;
    cont->type = type;

    pthread_mutex_lock(&handle->qlock);

    switch( type )
    {
    case RILQ_OBJECT_TYPE_REQUEST:
        if (handle->req_cont_first == NULL)
        {
            handle->req_cont_first = cont;
        }
        else
        {
            handle->req_cont_last->next = cont;
        }
        handle->req_cont_last = cont;
        handle->req_count++;
        break;
    case RILQ_OBJECT_TYPE_REQUEST_LOWP:
        if (handle->req_lowp_cont_first == NULL)
        {
            handle->req_lowp_cont_first = cont;
        }
        else
        {
            handle->req_lowp_cont_last->next = cont;
        }
        handle->req_lowp_cont_last = cont;
        handle->req_lowp_count++;
        break;
    case RILQ_OBJECT_TYPE_UNSOL:
        if (handle->unsol_cont_first == NULL)
        {
            handle->unsol_cont_first = cont;
        }
        else
        {
            handle->unsol_cont_last->next = cont;
        }
        handle->unsol_cont_last = cont;
        handle->unsol_count++;
        break;
    default:
        ALOGE("rilq_put() Error: unsupported object type %d\n", type);
        free(cont);
        pthread_mutex_unlock(&handle->qlock);
        return -1;
    }

    pthread_cond_signal(&handle->qcond);
    pthread_mutex_unlock(&handle->qlock);

    return 0;
}

void* rilq_get_object( struct _rilq_handle_t* handle,  rilq_object_type_t* type)
{
    void* obj = NULL;

    if ( NULL != handle )
    {
        pthread_mutex_lock(&handle->qlock);

        *type = RILQ_OBJECT_TYPE_UNDEFINED;

        if ((NULL == handle->req_cont_first)
         && (NULL == handle->req_lowp_cont_first)
         && (NULL == handle->unsol_cont_first))
        {
            /* only block and wait for new requests/events if all queues are empty */
            pthread_cond_wait(&handle->qcond, &handle->qlock);
        }

        if ( 1 == handle->closing )
        {
            pthread_mutex_unlock(&handle->qlock);
            return NULL;
        }

        if ( NULL != handle->unsol_cont_first )
        {
            rilq_container_t * cont = handle->unsol_cont_first;

            if ( cont )
            {
                handle->unsol_cont_first = cont->next;
                obj = cont->request;
                *type = cont->type;
                free(cont);
                handle->unsol_count--;
            }
        }
        else if ( NULL != handle->req_cont_first )
        {
            rilq_container_t * cont = handle->req_cont_first;

            if ( cont )
            {
                handle->req_cont_first = cont->next;
                obj = cont->request;
                *type = cont->type;
                free(cont);
                handle->req_count--;
            }
        }
        else if ( NULL != handle->req_lowp_cont_first )
        {
            rilq_container_t * cont = handle->req_lowp_cont_first;

            if ( cont )
            {
                handle->req_lowp_cont_first = cont->next;
                obj = cont->request;
                *type = cont->type;
                free(cont);
                handle->req_lowp_count--;
            }
        }

        pthread_mutex_unlock(&handle->qlock);
    }

    return obj;
}

void rilq_unblock( struct _rilq_handle_t* handle )
{
    handle->closing = 1;
    pthread_cond_signal(&handle->qcond);
}

void rilq_clear( struct _rilq_handle_t* handle )
{
    if ( NULL != handle )
    {
        pthread_mutex_lock(&handle->qlock);

        rilq_container_t* obj = handle->req_cont_first;
        rilq_container_t* nxt;
        while ( NULL != obj )
        {
            nxt = obj->next;
            ril_request_object_destroy(obj->request); /* free request object */
            free(obj);   /* free container */
            obj = nxt;
            handle->req_count--;

        }
        handle->req_cont_first = NULL;

        obj = handle->req_lowp_cont_first;
        while ( NULL != obj )
        {
            nxt = obj->next;
            ril_request_object_destroy(obj->request); /* free request object */
            free(obj);   /* free container */
            obj = nxt;
            handle->req_lowp_count--;

        }
        handle->req_lowp_cont_first = NULL;

        obj = handle->unsol_cont_first;
        while ( NULL != obj )
        {
            nxt = obj->next;
            ril_unsol_object_destroy(obj->request); /* free unsol resp object */
            free(obj);   /* free container */
            obj = nxt;
            handle->unsol_count--;
        }
        handle->unsol_cont_first = NULL;

        pthread_mutex_unlock(&handle->qlock);
    }
}

