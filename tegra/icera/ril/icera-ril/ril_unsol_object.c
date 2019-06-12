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
* @file ril_unsol_object.c implements the RIL Unsolicited Event data which is transferred between
* different thread contexts, e.g. for decoupling the AT Channel reader thread and the handling
* of this (maybe unsolicited) events, which might send AT commands that have to be send in AT
* Writer thread context.
*/
/*************************************************************************************************
* Project header files. The first in this list should always be the public interface for this
* file. This ensures that the public interface header file compiles standalone.
************************************************************************************************/
#include "ril_unsol_object.h" /* this */

#define LOG_NDEBUG 0        /**< enable (==0)/disable(==1) log output for this file */
#define LOG_TAG unsoLogTag  /**< log tag for this file */
#include <utils/Log.h>      /* ALOGD, LOGE, LOGI */

/*************************************************************************************************
* Standard header files
************************************************************************************************/
#include <malloc.h>
#include <memory.h>

/*************************************************************************************************
* Public function definitions
************************************************************************************************/
struct _ril_unsol_object_t* ril_unsol_object_create( const char* unsolString, const char* SmsPdu )
{
    ril_unsol_object_t* ril_unsol_obj = (ril_unsol_object_t*) malloc (sizeof(ril_unsol_object_t));
    if ( NULL != ril_unsol_obj )
    {
        ril_unsol_obj->string = NULL;
        ril_unsol_obj->SmsPdu = NULL;

        if ( NULL != unsolString && strlen(unsolString) )
        {
          ril_unsol_obj->string = strdup(unsolString);
        }
        if ( NULL != SmsPdu && strlen(SmsPdu) )
        {
          ril_unsol_obj->SmsPdu = strdup(SmsPdu);
        }
    }
    return ril_unsol_obj;
}

void ril_unsol_object_destroy( struct _ril_unsol_object_t* ril_unsol_obj )
{
    if ( NULL != ril_unsol_obj )
    {
        if ( NULL != ril_unsol_obj->string )
        {
          free(ril_unsol_obj->string);
        }
        if ( NULL != ril_unsol_obj->SmsPdu )
        {
          free(ril_unsol_obj->SmsPdu);
        }
        free(ril_unsol_obj);
    }
}
