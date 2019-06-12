/* Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved. */
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

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "icera-util.h"
#include "icera-util-local.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>
#include <utils/Log.h>

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/
#define WAKE_LOCK_PATH   "/sys/power/wake_lock"
#define WAKE_UNLOCK_PATH "/sys/power/wake_unlock"

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/
enum
{
    WAKELOCK_NOT_INITIALISED,
    WAKELOCK_INITIALISED
};

typedef struct
{
    int state;              /** initialised or not... */
    bool use_count;         /** unlock each time or only when all wake locks are released */
    int counter;            /** num of active wake lock(s) */
    char *name;             /** name of wake lock to write in wake locj sys files */
    pthread_mutex_t mutex;  /** mutual exclusion of wake lock resources */
}WakeLock;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

static WakeLock wake_lock =
{
    .state       = WAKELOCK_NOT_INITIALISED,
    .counter     = 0,
    .name        = NULL,
    .mutex       = PTHREAD_MUTEX_INITIALIZER
};

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Actuate wake lock or unlock path...
 *
 * @param path
 */
static void WakelockActuate(char *path)
{
    int fd;
    fd = open(path, O_RDWR, 0);
    if (fd>=0)
    {
        ALOGI("%s: writing %s to %s", __FUNCTION__, wake_lock.name, path);
        write(fd, wake_lock.name, strlen(wake_lock.name));
        close(fd);
    }
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
void IceraWakeLockRegister(char *name, bool use_count)
{
    struct stat wstat;

    if(wake_lock.state == WAKELOCK_NOT_INITIALISED)
    {
        wake_lock.name      = name;
        wake_lock.use_count = use_count;
        wake_lock.state     = WAKELOCK_INITIALISED;
    }
    else
    {
        ALOGE("%s: already done.", __FUNCTION__);
    }
}

void IceraWakeLockAcquire(void)
{
    if(wake_lock.state == WAKELOCK_INITIALISED)
    {
        pthread_mutex_lock(&wake_lock.mutex);
        if(wake_lock.counter==0)
        {
            WakelockActuate(WAKE_LOCK_PATH);
        }
        if(wake_lock.use_count)
        {
            wake_lock.counter++;
        }
        pthread_mutex_unlock(&wake_lock.mutex);
    }
    else
    {
        ALOGW("%s: wake lock not initialised.", __FUNCTION__);
    }
}

void IceraWakeLockRelease(void)
{
    if(wake_lock.state == WAKELOCK_INITIALISED)
    {
        pthread_mutex_lock(&wake_lock.mutex);
        if(wake_lock.use_count)
        {
            wake_lock.counter--;
        }
        if(wake_lock.counter<0)
        {
            ALOGW("wakelock_release: Count < 0");
        }

        if(wake_lock.counter==0)
        {
            WakelockActuate(WAKE_UNLOCK_PATH);
        }
        pthread_mutex_unlock(&wake_lock.mutex);
    }
}
