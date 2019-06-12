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
* @{
*/

/**
* @file at_channel_writer.c The AT channel writer implementation
*
*/

/*************************************************************************************************
* Project header files. The first in this list should always be the public interface for this
* file. This ensures that the public interface header file compiles standalone.
************************************************************************************************/
#include "at_channel_writer.h"
#include "at_channel_handle.h"
#include "atchannel.h"

#include "icera-ril.h"

#include "icera-util.h"

#define LOG_NDEBUG 0        /**< enable (==0)/disable(==1) log output for this file */
#define LOG_TAG atcwLogTag  /**< log tag for this file */
#include <utils/Log.h>      /* ALOGD, LOGE, LOGI */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <pthread.h>
#include <string.h>
#include <unistd.h> /* close() */

#include <cutils/properties.h>
/*************************************************************************************************
* Private type definitions
************************************************************************************************/
/**
  * handle of an AT Channel writer. It contains references to it's thread ID, the
  * belonging AT Channel Handle and the belonging RIL Object Queue
  */
typedef struct _at_channel_writer_handle_t
{
    pthread_t              thread_id;    /**< AT Channel specific thread ID */
    struct _rilq_handle_t* rilq_handle;  /**< AT Channel specific RIL Request Queue */
    unsigned char          terminate;    /**< flag indicating if the AT Channel should shut down */
    struct _at_channel_handle_t* at_channel_handle; /**< handle of AT channel, including the AT
                                            channel writer and all synchronization objects */
}
at_channel_writer_handle_t;

/**
 * struct which is used for handover of a set of parameters to the AT Channel worker thread
 */
struct at_channel_startup_params
{
    char*                                dev_name;        /**< device name, e.g. /dev/ttyS0,
                                                                127.0.0.1:12345 */
    at_channel_device_type_t             dev_type;        /**< device type, e.g. loopback, socket
                                                                or TTY as defined in
                                                                at_channel_device_type_t */
    at_channel_dev_open_callback_t       cb_dev_open;     /**< callback function which opens
                                                                the device */
    at_channel_dev_close_callback_t      cb_dev_close;    /**< callback function is called when the
                                                                 device has been closed */
    at_channel_init_callback_t           cb_init;         /**< init callback function which is
                                                                called after successfull at_open()*/
    at_channel_unsolicited_callback_t    cb_unsol;        /**< handler for unsolicited modem
                                                                events */
    at_channel_handle_unsolicited_callback_t cb_handle_unsol; /**< handler for unsolicited modem
                                                                events */
    at_channel_timeout_callback_t        cb_timeout;      /**< callback function which is called in
                                                                case of an AT Channel timeout while
                                                                waiting for a final response */
    at_channel_closed_callback_t         cb_close;        /**< callback function which indicates
                                                                that the AT channel reader thread
                                                                has ended */
    at_channel_handle_request_callback_t cb_handle_req;   /**< handle of channels RIL Request Queue */
    at_channel_writer_handle_t*                 handle;          /**< handle of the AT channel itself */
    int                                  default_channel; /**< flag indication if it's the default
                                                                AT channel */
    long long                            timeout;         /**< AT channel timeout which detects
                                                                missing final responses */
};

/*************************************************************************************************
* Private function definitions
************************************************************************************************/
/**
 * AT Channel Writer main rountine.
 *
 * This function opens and configures the TTY device, and handles the RIL request
 *  handling for this AT Channel.
 *
 * @param param [IN] pointer to at_channel_startup_params* structure
 *
 * @return NULL in any case, however we need to meet the pthread interface definition
 */
static void* at_channel_writer_main( void* param )
{
    int fd;

    struct at_channel_startup_params* p = (struct at_channel_startup_params*) param;

    ALOGD("at_channel_writer_main(%x) for device %s\n", (unsigned int)pthread_self(), p->dev_name);

    int testMode = isRunningTestMode();

    fd = p->cb_dev_open(p->dev_name, p->dev_type, p->default_channel);
    if (fd >= 0)
    {
        int ret;

        ret = at_open(fd, p->cb_unsol, p->default_channel, p->timeout);
        if (ret < 0) {
            ALOGE("at_channel_writer_main(%x) error %d on at_open\n", (unsigned int)pthread_self(), ret);
            at_close();
            return NULL;
        }

        /*
         * if in test mode set the property set property to say all is fine
         */
#ifndef ALWAYS_SET_MODEM_STATE_PROPERTY
        if(testMode)
#endif
        {

            property_set("gsm.modem.ril.init","ok");
        }

        /* Wait until the AT Channel Reader is up and running */
        do
        {
            p->handle->at_channel_handle = at_channel_handle_get();
        }
        while( NULL == p->handle->at_channel_handle );

        /* Register callback functions for error conditions on AT Channel reader */
        at_set_on_reader_closed(p->cb_close);
        at_set_on_timeout(p->cb_timeout);

        /* call channel specific initialization routine */
        p->cb_init();

        while( (unsigned char)0 == p->handle->terminate )
        {
            rilq_object_type_t type = RILQ_OBJECT_TYPE_UNDEFINED;

            void* data_object = rilq_get_object( p->handle->rilq_handle, &type );

            if ( NULL == data_object || (unsigned char)0 != p->handle->terminate)
            {
               /*
                * we are shutting down the AT channel...
                * don't need to take care about cleaning up the queue. This
                * will be handled by upper layers which triggered the shutdown
                */
               ALOGD("at_channel_writer_main(%x) received terminate event for AT channel writer %s\n",
                   (unsigned int)pthread_self(), p->dev_name);
               break;
            }

            if (( RILQ_OBJECT_TYPE_REQUEST == type )
              ||(RILQ_OBJECT_TYPE_REQUEST_LOWP == type))
            {
                struct _ril_request_object_t* req_obj = (struct _ril_request_object_t*)data_object;
                if ( NULL != p->cb_handle_req )
                {
                    p->cb_handle_req(req_obj->request, req_obj->data, req_obj->datalen, req_obj->token );
                }
                ril_request_object_destroy(req_obj);
            }
            else if ( RILQ_OBJECT_TYPE_UNSOL == type )
            {
                struct _ril_unsol_object_t* unsol_obj = (struct _ril_unsol_object_t*)data_object;
                if ( NULL != p->cb_handle_unsol )
                {
                    p->cb_handle_unsol(unsol_obj->string, unsol_obj->SmsPdu);
                }
                ril_unsol_object_destroy(unsol_obj);
            }
            else
            {
                ALOGE("at_channel_writer_main(%x) error: unknown object type in queue!!\n",
                    (unsigned int)pthread_self() );
            }
        }
        ALOGD("at_channel_writer_main(%x) closing AT channel writer for device %s\n",
            (unsigned int)pthread_self(), p->dev_name);
    }
    else
    {
        ALOGE("at_channel_writer_main(%x) failed to open device %s\n",
            (unsigned int)pthread_self(), p->dev_name);

        /*
         * if in test mode set the property so tester can
         * attempt to remediate
         */
#ifndef ALWAYS_SET_MODEM_STATE_PROPERTY
        if(testMode)
#endif
        {
            property_set("gsm.modem.ril.init","fail");
        }

        /*
         * Looks like no modem, init is aborted
         * release the wakelock to allow lP0
         */
        IceraWakeLockRelease();
    }

    at_close();
    p->cb_dev_close(p->dev_name);  /* just for information which device is closed */
    ALOGD("at_channel_writer_main(%x) device %s closed\n",  (unsigned int)pthread_self(), p->dev_name);
    free(p->dev_name);
    free(p);
    pthread_exit(NULL);
    return NULL;
}

/*************************************************************************************************
* Public function definitions
************************************************************************************************/
struct _at_channel_writer_handle_t* at_channel_writer_create( const char* device_name,
                                                       at_channel_device_type_t device_type,
                                                       const at_channel_callback_funcs_t*  callback_funcs,
                                                       struct _rilq_handle_t* rilq_handle,
                                                       int default_channel,
                                                       long long timeout )
{
    /* create new AT channel handle */
    int err;
    at_channel_writer_handle_t* handle = (at_channel_writer_handle_t*) malloc (sizeof(at_channel_writer_handle_t));
    struct at_channel_startup_params* startup_params = malloc(sizeof(struct at_channel_startup_params));

    if ( NULL != handle && NULL != startup_params )
    {
        handle->rilq_handle       = rilq_handle;
        handle->terminate         = (unsigned char)0;
        handle->at_channel_handle = NULL;

        /* initialize startup params */
        startup_params->dev_name        = strdup(device_name);
        startup_params->dev_type        = device_type;
        startup_params->cb_dev_open     = callback_funcs->callback_dev_open;
        startup_params->cb_dev_close    = callback_funcs->callback_dev_close;
        startup_params->cb_init         = callback_funcs->callback_init;
        startup_params->cb_unsol        = callback_funcs->callback_unsolicited;
        startup_params->cb_handle_unsol = callback_funcs->callback_handle_unsolicited;
        startup_params->cb_timeout      = callback_funcs->callback_timeout;
        startup_params->cb_close        = callback_funcs->callback_closed;
        startup_params->cb_handle_req   = callback_funcs->callback_handle_request;
        startup_params->handle          = handle;
        startup_params->default_channel = default_channel;
        startup_params->timeout         = timeout;

        /* start at channel handler thread */
        err = pthread_create(&handle->thread_id, NULL, at_channel_writer_main, (void*)startup_params );
        if ( err != 0 )
        {
            ALOGE("at_channel_writer_create() error creating AT channel writer thread: %d\n", err);
            free(handle);
            free(startup_params);
            handle = NULL;
        }
    }
    else
    {
        ALOGE("at_channel_writer_create() error: unable to allocate memory\n");
        free(handle);
        free(startup_params);
        handle = NULL;
    }

    return handle;
}

void at_channel_writer_destroy(struct _at_channel_writer_handle_t* handle)
{
    if ( NULL != handle )
    {
        handle->terminate = (unsigned char)1;     /* terminate at channel handler thread */

        if ( NULL != handle->at_channel_handle     &&
             0    <= handle->at_channel_handle->fd )
        {
            /*
             * unblock the reader thread by closing it's device and
             * make the device' file descriptor invalid
             */
            (void)close(handle->at_channel_handle->fd);
            handle->at_channel_handle->fd = -1;
        }

        rilq_unblock(handle->rilq_handle);      /* unblock the writer thread */

        (void)pthread_join(handle->thread_id, NULL);  /* wait until the writer thread has ternminated */

        /* free resources (at channel startup params, at_channel_handle) */
        free(handle);
    }
}

void at_channel_writer_set_at_cmd_timeout(struct _at_channel_writer_handle_t* at_channel_writer_handle,
                                          long long timeout)
{
    if ( at_channel_writer_handle && at_channel_writer_handle->at_channel_handle )
    {
        at_channel_writer_handle->at_channel_handle->ATCmdTimeout = timeout;
        ALOGD("at_channel_writer_set_timeout() reconfigured timeout for channel %d to %lld\n",
            at_channel_writer_handle->at_channel_handle->fd,
            at_channel_writer_handle->at_channel_handle->ATCmdTimeout);
    }
}

int at_channel_is_ready(struct _at_channel_writer_handle_t* at_channel_writer_handle)
{
    return (at_channel_writer_handle != NULL) && (at_channel_writer_handle->at_channel_handle != NULL);
}

/** @} END OF FILE */
