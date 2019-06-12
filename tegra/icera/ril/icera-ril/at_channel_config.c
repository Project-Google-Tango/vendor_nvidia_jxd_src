/* Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved. */
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
* @file at_channel_config.c The implementation of the AT channel configuration module
*
* The AT Channel configuration implements functions which maintain the AT Channel
* configuration parameters.
*/

/*************************************************************************************************
* Project header files. The first in this list should always be the public interface for this
* file. This ensures that the public interface header file compiles standalone.
************************************************************************************************/
#include "at_channel_config.h"  /* this */
#include "at_channel_config_requests.h" /* AT channel RIL request mapping */
#include "icera-ril-extensions.h"
#include "icera-ril.h"

#define LOG_NDEBUG 0        /**< enable (==0)/disable(==1) log output for this file */

#define LOG_TAG atccLogTag  /**< log tag for this file */
#include <utils/Log.h>      /* ALOGD, LOGE, LOGI */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <stdio.h> /* FILE, fopen, fclose, ... */
#include <string.h> /* strlen, strdup, ... */

/*************************************************************************************************
* Macros
************************************************************************************************/
#define ATCC_EXTCHAN_DEFAULT_AT_CMD_TIMEOUT    0 /**< default timeout of an extended channel is 0[ms],
                                               means AT channel is blocking if nothing else is configured in the config file */
#define ATCC_DEFCHAN_DEFAULT_AT_CMD_TIMEOUT    180000 /**< default timeout of the default channel is 60[s] */
#define ATCC_DEFCHAN_AT_CMD_TIMEOUT    180000 /**< standard timeout for the default channel, if an extended
                                           channel exists is 10[s] */

/*************************************************************************************************
* Private type definitions
************************************************************************************************/
/**
 * Configurable parameters of an AT channel. THis structure is used for mapping
 * RIL requests to AT channels.
 * Note: The configuration mechanism handles only additional AT channels,
 *  but not the default channel. The default channel accepts every request
 *  which is not configured here
 */
typedef struct _at_channel_config_t
{
    at_channel_id_t              chan_id;           /**< Non-zero ID of the AT Channel */
    char*                        dev_name;          /**< device name (zero-terminated string) */
    int*                         ril_req_id_list;   /**< RIL request list for this channel */
    long long                    timeout;           /**< max time in [ms] to wait for a final response */
    struct _at_channel_config_t* next;              /**< pointer to another configuration */
}
at_channel_config_t;

/*************************************************************************************************
* Private variable definitions
************************************************************************************************/
static at_channel_config_t* s_config = NULL; /**< start of configuration parameter set list */

/*************************************************************************************************
* Private function definitions
************************************************************************************************/
/** attaches a RIL request list, defined in at_channel_config_requests.h, to an AT channel */
static void atcc_set_ril_request_list( at_channel_config_t* config, int last)
{

    int ** channel_list;
    int channel_list_size = 0;

    rilExtGetChannelRequestConf(&channel_list, & channel_list_size);

    /* No list specified by ext lib, use default */
    if((channel_list == NULL)||(channel_list_size == 0))
    {
        channel_list = s_chan_list;
        channel_list_size = (sizeof(s_chan_list) / sizeof(int*));
    }

    /* Only attempt collation if not all config have been used */
    if(last && (config->chan_id < channel_list_size-1))
    {
        ALOGD("Collating remaining requests on channel %d (%d->%d)",config->chan_id,config->chan_id,channel_list_size-1);
        int chan_id, index, request_count, i;
        int * new_request_list = NULL;

        /*
         * Loop twice, first to count the number of requests, then alloc
         * then fill the int array with the collated request ids
         */
        for(i=0;i<2;i++) {
            chan_id = config->chan_id;
            index = 0;
            request_count = 0;

            while (chan_id < channel_list_size) {
                while(channel_list[chan_id][index] != 0) {
                    if(new_request_list!=NULL) {
                        new_request_list[request_count] = channel_list[chan_id][index];
                    }

                    index++;
                    request_count++;
                }
                index = 0;
                chan_id++;
            }

            if(new_request_list == NULL) {
                new_request_list = malloc(sizeof(int)*(request_count+1));
            } else {
                /* Request list needs to be 0 termninated */
                new_request_list[request_count+1] = 0;

                /*
                 * We should free the previous list if applicable, but there is no easy way to know
                 * if it was dynamically allocated or not...
                 */
                config->ril_req_id_list = new_request_list;
                ALOGD("Collated request: %d",request_count);
                return;
            }
        }
    }

    if ( config->chan_id < channel_list_size )
    {
        config->ril_req_id_list = channel_list[config->chan_id];
    }
    else
    {
        ALOGE("no RIL request list defined for channel %d\n", config->chan_id );
    }
}

/** extracts the channel ID and the device name from a line of the config file */
static int atcc_set_channel_info( at_channel_config_t* config, const char* line )
{
    int ret = 0;
    if ( NULL != config && NULL != line )
    {
        char* p = strchr (line, '=') + 1; /* look for '=' and skip it */
        if ( NULL != p )
        {
            config->chan_id = atoi(line + 3);  /* skip 'DLC' prefix */
            config->dev_name = strdup(p); /* copy device name */
            ret = 1;
        }
    }
    return ret;
}

/** extracts the timeout from a line of the config file */
static int atcc_set_channel_timeout( at_channel_config_t* config, const char* line )
{
    int ret = 0;
    if ( NULL != config && NULL != line )
    {
        char* p = strchr (line, '=') + 1; /* look for '=' and skip it */
        if ( NULL != p )
        {
            config->timeout = atoi(p); /* store timeout in configuration */
            ret = 1;
        }
    }
    return ret;
}

/** registers a new configuration parameter set in the list of available configurations */
static void atcc_append_config( at_channel_config_t* config )
{
    if ( NULL == s_config )
    {
        s_config = config;
    }
    else
    {
        at_channel_config_t* p_config = s_config;
        while( NULL != p_config->next )
        {
              p_config = p_config->next;
        }
        p_config->next = config;
    }
}

/** read a line from the configuration file */
static int atcc_read_line( FILE* file, char* line, int len )
{
    int c;
    char* p = line;

    /* read until end of line or end of file is found */
    while ( (c = fgetc (file)) != EOF )
    {
        if ( c == '\r' || c == '\n' )
        {
            break; /* end of line found */
        }

        *p++ = (char)c; /* store charakter if not end of line */

        if ( p - line >= len  )
        {
            break; /* buffer is full -> return what is read so far */
        }
    }
    *p = '\0';
    return c;
}

/*************************************************************************************************
* Public function definitions
************************************************************************************************/

int at_channel_config_init( const char* config_file )
{
    int ret = 0;
    FILE *file;
    file = fopen( config_file, "r" );
    if ( file )
    {
        int read = EOF;
        char line[ATCC_MAX_LINE_LEN];
        do
        {
            *line = '\0';
            read = atcc_read_line(file, line, sizeof(line));
            /* TODO: add handling of different line information here */
            if ( strlen(line) )
            {
                if ( strstr( line, "DLC" ) )
                {
                    at_channel_config_t* config = (at_channel_config_t*) malloc( sizeof(at_channel_config_t) );
                    memset( config, 0, sizeof( at_channel_config_t )); /* clear new configuration memory */
                    if ( atcc_set_channel_info( config, line ) )  /* initialize ID and device name */
                    {
                        atcc_set_ril_request_list( config, 0);        /* initialize RIL Request list */
                        config->next = NULL;                        /* new config has no next at startup */
                        atcc_append_config( config );               /* append new configuration to existing list */
                        config->timeout = ATCC_EXTCHAN_DEFAULT_AT_CMD_TIMEOUT; /* set default timeout;
                                                                                  might be overwritten
                                                                                  if other value is defined in config file */

                        ALOGD("Found configuration AT Channel ID <%d>, Device <%s>\n", config->chan_id, config->dev_name);

                        ret = 1;
                    }
                    else
                    {
                        free(config);
                    }
                }
                else if ( strstr( line, "TMO" ) )
                {
                    at_channel_config_t* p_config = s_config;
                    while( NULL != p_config->next )
                    {
                        p_config = p_config->next;
                    }
                    atcc_set_channel_timeout( p_config, line );
                }
            }
        }
        while( read != EOF );
        fclose(file);

        /* Get the last channel, associate all of the remaining requests to it too */
        at_channel_config_t* p_config = s_config;
        while((NULL != p_config) && (NULL != p_config->next ))
        {
            p_config = p_config->next;
        }
        if(p_config) {
            atcc_set_ril_request_list( p_config, 1);        /* replace the configured request list so it contains all the non associated requests */
        }
    }
    else
    {
        ALOGE("config file %s not found, using default AT channel only\n", config_file);
    }
    return ret;
}

void at_channel_config_exit( void )
{
    at_channel_config_t* config;

    /* free all allocated configurations */
    for( config = s_config; NULL != config; config = s_config )
    {
        s_config = s_config->next;
        config->chan_id = 0;
        free(config->dev_name); /* allocated with strdup, so need to free it manually */
        config->ril_req_id_list = NULL;
        free(config);
    }
}

at_channel_id_t at_channel_config_get_num_channels( void )
{
    int channel_count = 0;
    if ( s_config )
    {
        at_channel_config_t* p_config = s_config;
        ++channel_count;
        while( NULL != p_config->next )
        {
            p_config = p_config->next;
            ++channel_count;
        }
    }
    return channel_count;
}

at_channel_id_t at_channel_config_get_channel_for_request( const int ril_request )
{
    at_channel_config_t* config;
    for( config = s_config; NULL != config; config = config->next )
    {
        int i;
        for ( i=0; config->ril_req_id_list && config->ril_req_id_list[i]; ++i )
        {
            if ( config->ril_req_id_list[i] == ril_request )
            {
                return config->chan_id;
            }
        }
    }
    return 0;
}

const char* at_channel_config_get_device_name( const at_channel_id_t channel_id )
{
    at_channel_config_t* config;
    for( config = s_config; NULL != config; config = config->next )
    {
        if ( config->chan_id == channel_id )
        {
            return config->dev_name;
        }
    }
    return NULL;
}

long long at_channel_config_get_at_cmd_timeout( const at_channel_id_t channel_id )
{
    long long timeout = 0;
    if ( 0 == channel_id ) /* default channel ? */
    {
        timeout = ATCC_DEFCHAN_AT_CMD_TIMEOUT;
    }
    else /* extended channel */
    {
        at_channel_config_t* config;
        for( config = s_config; NULL != config; config = config->next )
        {
            if ( config->chan_id == channel_id )
            {
                timeout = config->timeout;
            }
        }
        if ( 0 == timeout )
        {
            timeout = ATCC_EXTCHAN_DEFAULT_AT_CMD_TIMEOUT;
        }
    }
    ALOGD("at_channel_config_get_timeout() returning timeout of %lld for channel id %d\n",
         timeout, channel_id);
    return timeout;
}

long long at_channel_config_get_default_at_cmd_timeout( const at_channel_id_t channel_id )
{
    long long timeout = 0;
    if ( 0 == channel_id ) /* default channel ? */
    {
        timeout = ATCC_DEFCHAN_DEFAULT_AT_CMD_TIMEOUT;
    }
    else /* extended channel(s) */
    {
        timeout = ATCC_EXTCHAN_DEFAULT_AT_CMD_TIMEOUT;
    }
    ALOGD("at_channel_config_get_default_timeout() returning timeout of %lld for channel id %d\n",
         timeout, channel_id);
    return timeout;
}
