/*
 * Copyright (c) 2013-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file icera-crashlogs.c .
 *
 */


/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#define LOG_TAG "ICECRASH"
#include <utils/Log.h>  /* ALOG... */
#include <dirent.h>     /** readdir... */
#include <stdlib.h>     /* system,.. */
#include <errno.h>      /** errno... */
#include <stdbool.h>    /** bool,... */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cutils/properties.h> /* system properties */
#include <private/android_filesystem_config.h>

#include "icera-util.h"

/*************************************************************************************************
 * Private macros definitions
 ************************************************************************************************/
/** Crash log cmds:
 *   - logcat
 *   - dmesg */
#define LOGCAT_CMD "/system/bin/logcat -d -v time -b main -b radio -b system"
#define DMESG_CMD  "/system/bin/dmesg"
#define CRASHCHECK_CMD "crash-check-arm"
#define FEEDBACK_CMD "am broadcast -a com.nvidia.feedback.NVIDIAFEEDBACK"

/* Logcat files */
#define LOGCAT_NAME_PREFIX "logcat_"
#define LOGCAT_NAME_EXTENSION ".txt"

/* Kernel logs files */
#define DMESG_NAME_PREFIX "dmesg_"
#define DMESG_NAME_EXTENSION ".txt"

/* Some default property values */
#define DEFAULT_CRASH_LOG_TIMESTAMP "0000_000000"
#define DEFAULT_LOG_TYPE ""
#define DEFAULT_LOG_PORT "/dev/ttyACM0"
#define DEFAULT_FEEDBACK "1"

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Change permission of file(s) created during a coredump in loader mode.
 * This to allow external tool without root access to read such file(s).
 */
static void set_log_permission(const char* root_path)
{
    char path[PATH_MAX];
    DIR *dir;
    struct dirent *entry;

    dir = opendir(root_path);
    while((entry = readdir(dir)) != NULL) {
            struct stat st;
        snprintf(path, PATH_MAX, "%s/%s", root_path, entry->d_name);
            stat(path, &st);
        if (((st.st_uid != AID_RADIO ||
            st.st_gid != AID_SYSTEM)) && (strcmp(entry->d_name, "..") != 0))
            {
                /* Change created coredump ownership/permissions */
            chown(path, AID_RADIO, AID_SYSTEM);
                chmod(path, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
            }
        }
    closedir(dir);

    /* Change created folder ownership/permissions also */
    chown(root_path, AID_RADIO, AID_SYSTEM);
    chmod(root_path, S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IWGRP|S_IXGRP);
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

int main(void)
{
    char *cmd;
    int ret;
    char time_stamp[PROPERTY_VALUE_MAX];
    char directory[PROPERTY_VALUE_MAX];
    char type[PROPERTY_VALUE_MAX];
    char feedback[PROPERTY_VALUE_MAX];
    char action[PROPERTY_VALUE_MAX];

    /* Get infos to store/extract logs */
    property_get("gsm.modem.crashlogs.directory", directory, DEFAULT_LOGGING_DIR);
    property_get("gsm.modem.crashlogs.timestamp", time_stamp, DEFAULT_CRASH_LOG_TIMESTAMP);
    property_get("gsm.modem.crashlogs.type", type, DEFAULT_LOG_TYPE);
    property_get("gsm.modem.crashlogs.feedback", feedback, DEFAULT_FEEDBACK);
    property_get("gsm.modem.crashlogs.action", action, "");

    if (strcmp(type, "COREDUMP_LOADER") == 0)
    {
        char devicename[PROPERTY_VALUE_MAX];

        /* 1st stop RIL: it will be re-started by icera-switcher detecting re-enumeration... */
        ALOGI("Stopping RIL");
        property_set("gsm.modem.ril.enabled", "0");
        property_set("gsm.modem.riltest.enabled", "0");

        /* Start crash_check to get full coredump */
        property_get("ro.ril.devicename", devicename, DEFAULT_LOG_PORT);
        asprintf(&cmd,
                 "%s --dir %s --ram-only --timestamp-fn %s",
                 CRASHCHECK_CMD,
                 directory,
                 devicename);
        ALOGD("Executing cmd: %s", cmd);
        ret = system(cmd);
        if (ret < 0)
            ALOGE("%s returns %d", CRASHCHECK_CMD, ret);
        free(cmd);
    }

    /* store logcat */
    asprintf(&cmd,
             "%s>%s/%s%s%s",
             LOGCAT_CMD,
             directory,
             LOGCAT_NAME_PREFIX,
             time_stamp,
             LOGCAT_NAME_EXTENSION);
    ALOGD("Executing cmd: %s", cmd);
    system(cmd);
    free(cmd);

    /* store dmesg */
    asprintf(&cmd,
             "%s>%s/%s%s%s",
             DMESG_CMD,
             directory,
             DMESG_NAME_PREFIX,
             time_stamp,
             DMESG_NAME_EXTENSION);
    ALOGD("Executing cmd: %s", cmd);
    system(cmd);
    free(cmd);

    set_log_permission(directory);

    if (strcmp(type, "COREDUMP_MODEM") == 0)
    {
        /*
        * Modem reset detected by FILD: exit without broadcasting any feedback.
        *
        * Feedback will be broadcasted once modem will have fully re-started  and crash
        * "detected" in RIL through AT cmd: above AP logs may be replaced after this detection,
        * but kept here in case detection mechanism is KO...
        *
        * Do not either reset crash logs infos so that next time this process will be called,
        * AP logs will be stored in relevant dated folder.
        *
        */
        return 0;
    }
    else
    {
        /* call feedback command if enabled */
        if (strcmp(feedback, "1") == 0)
        {
            if (strcmp(action, ""))
            {
                asprintf(&cmd,
                         "%s",
                         action);
            }
            else
            {
                asprintf(&cmd,
                         "%s",
                         FEEDBACK_CMD);
            }
            ALOGD("Executing cmd: %s", cmd);
            system(cmd);
            free(cmd);
        }

        /* Set back logs infos to default values... */
        property_set("gsm.modem.crashlogs.directory", DEFAULT_LOGGING_DIR);
        property_set("gsm.modem.crashlogs.timestamp", DEFAULT_CRASH_LOG_TIMESTAMP);
    }

    return 0;
}
