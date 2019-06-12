/*************************************************************************************************
 *
 * Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @file icera-ril-logging.c RIL modem logging utilities
 *
 */

/*************************************************************************************************
 * Project header files. The first in this list should always be the public interface for this
 * file. This ensures that the public interface header file compiles standalone.
 ************************************************************************************************/
#include "icera-ril.h"
#include "icera-util.h"
#include "icera-ril-logging.h"

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdio.h>
#include <pthread.h> /* pthread,... */
#include <stdbool.h> /* bool */
#include <dirent.h>  /* DIR *,...*/
#include <cutils/properties.h> /* property_get,... */
#define LOG_TAG rilLogTag
#include <utils/Log.h> /* ALOGI,...*/

/**************************************************************************************************************/
/* Private Macros  */
/**************************************************************************************************************/

/** Tool used to get BBC logging data
 *
 * To get a oneshot RAM logging for a given amount of time:
 *  icera_log_serial_arm -b -d /dev/log_modem -m <time_ms> <iom_filepath>
 *
 * To get BBC logging database (aka dictionary):
 *  icera_log_serial_arm -d /dev/log_modem -e <ix_filepath>
 */
#define LOGGING_TOOL "icera_log_serial_arm"

/** Device used to get BBC logging */
#define LOGGING_CHANNEL "/dev/log_modem"

/** Prefix given to dir used to store BBC logging data
 * and to retreived .iom file */
#define LOGGING_NAME_PREFIX "logging"
#define LOGGING_LOG_EXTENSION ".iom"

/** Prefix given to BBC logging database */
#define LOGGING_DB_PREFIX "tracegen"
#define LOGGING_DB_EXTENSION ".ix"

/**
 * Max number of "space separated " args that can be put in ril.modem.logging.args
 *
 */
#define MAX_LOGGING_ARGS 32

/** Default max of logging session to store */
#define LOGGING_DEFAULT_MAX_NUM_OF_FILES 10

/**
 * In case of deferred logging, default max amount of data (in MB) that can be stored
 * by icera_log_serial_arm before erasing older data.
 */
#define LOGGING_DEFAULT_DEFERRED_MAX_SIZE 100

/**
 * In case of deferred logging, default max size (in MB) for each separate
 * logging files created by icera_log_serial_arm
 */
#define LOGGING_DEFAULT_DEFERRED_MAX_FILESIZE 10

/**
 * In case of RAM logging, default duration of logging session.
 * In case of deferred logging, target timeout before sending
 * accumulated logging data
 */
#define LOGGING_DEFAULT_DURATION 30

/**
 * In case of deferred logging, default threshold of
 * accumulated logging data amount before sending
 * it.*/
#define LOGGING_DEFAULT_DEFERRED_BUFFER_PERCENTAGE 80

#define LOGGING_MAX_EVENT_MESSAGE_SIZE 8192

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/
/** Thread handler for logging events */
static pthread_t log_event_thread;

/** Mutex used to protect logging event signalling */
static pthread_mutex_t log_event_mutex;

/** Contidition set when signalling a logging event */
static pthread_cond_t log_event_cond;

/** To indicate if logging event handling is on going */
static bool log_running = false;

/** Logging storage data */
static LoggingData log_data;

/** Logging type (BBC logging or coredump retrieval...) */
static LoggingType log_type;

/** Set default logging settings.
 *  Can be bypassed with ril.modem.logging.args */
static int modem_logging_duration_s     = LOGGING_DEFAULT_DURATION;
static int modem_logging_type           = RAM_LOGGING;
static int modem_logging_max_size       = LOGGING_DEFAULT_DEFERRED_MAX_SIZE;
static int modem_logging_max_filesize   = LOGGING_DEFAULT_DEFERRED_MAX_FILESIZE;
static int modem_logging_buf_percentage = LOGGING_DEFAULT_DEFERRED_BUFFER_PERCENTAGE;

/**
 * Logging state
 */
static bool logging_initialised = false;

static char event_message_buffer[LOGGING_MAX_EVENT_MESSAGE_SIZE];

static int event_message_len;

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/
/**
 * Start LOGGING_TOOL to flush modem logging buffers for modem_logging_duration_s
 * amount of time.
 *
 * @param dir where to store logging data
 * @param time_str timestamp appended to logging file name.
 */
static void doRamLogging(char *dir, char *time_str)
{
    char *cmd;
    asprintf(&cmd,
            "%s -b -d %s -m %d %s/%s_%s%s",
            LOGGING_TOOL,
            LOGGING_CHANNEL,
            modem_logging_duration_s*1000,
            dir,
            LOGGING_NAME_PREFIX,
            time_str,
            LOGGING_LOG_EXTENSION);
    ALOGD("%s: %s", __FUNCTION__, cmd);
    int err = system(cmd);
    free(cmd);
    if (err)
    {
        ALOGE("%s: Fail to get logging data.", __FUNCTION__);
    }
    else
    {
        /* Get signal database */
        asprintf(&cmd,
                 "%s -d %s -e %s/%s_%s%s",
                 LOGGING_TOOL,
                 LOGGING_CHANNEL,
                 dir,
                 LOGGING_DB_PREFIX,
                 time_str,
                 LOGGING_DB_EXTENSION);
        ALOGD("%s: %s", __FUNCTION__, cmd);
        err = system(cmd);
        free(cmd);
        if (err)
        {
            ALOGE("%s: Fail to extract signal database.", __FUNCTION__);
        }
    }
}

/**
 * "Move" existing logging data in a dated folder
 *
 * @param dir where to store logging data
 */
static void doDeferredLogging(char *dir)
{
    ALOGI("%s", __FUNCTION__);
    IceraLogStoreModemLoggingData(log_data.dir, dir);
}

static void StoreEventMessage(const char *dirname)
{
    FILE *fp;
    char filename[PATH_MAX];
    snprintf(filename, PATH_MAX, "%s/event.txt", dirname);
    fp = fopen(filename, "w");
    if (fp)
    {
        fwrite(event_message_buffer, event_message_len, 1, fp);
        fclose(fp);
    }
}

/*
 * Task routine handling logging event
 */
static void *getModemLoggingEvent(void *arg)
{
    char time_str[MAX_TIME_LENGTH+1];
    char dir[PATH_MAX];
    int err = 0;
    int create_dir = 1;

    while(1)
    {
        /* Wait for logging event: lock mutex & wait on cond */
        pthread_mutex_lock(&log_event_mutex);
        while (!log_running)
        {
            pthread_cond_wait(&log_event_cond, &log_event_mutex);
        }

        /* Prepare logging storage env:
         * modem logging or coredump will be stored in a dedicated
         * dated folder. */
        switch (log_type)
        {
            case MODEM_CRASH_DETECTED_SINGLE:
                create_dir = 0;
            case COREDUMP_LOADER:
                strncpy(log_data.prefix, COREDUMP_NAME_PREFIX, PREFIX_MAX);
                break;
            default:
                strncpy(log_data.prefix, LOGGING_NAME_PREFIX, PREFIX_MAX);
                break;
        }

        if (create_dir)
        {
            /* Create dated directory to store logs if needed */
            IceraLogGetTimeStr(time_str, sizeof(time_str));
            err = IceraLogNewDatedDir(
                    &log_data,
                    time_str,
                    dir);
        }
        else
        {
            /** Use the one already created. Useful for
             *  MODEM_CRASH_DETECTED_SINGLE where FIL has previously created a
             *  dir to store coredump and that RIL is now updating it with
             *  latest AP logs and additional modem logging data. */
            property_get("gsm.modem.crashlogs.directory", dir, "");
        }

        if (!err)
        {
            /* Store event message in logging directory... */
            StoreEventMessage(dir);

            if (log_type == MODEM_LOGGING)
            {
                switch (modem_logging_type)
                {
                case RAM_LOGGING:
                    doRamLogging(dir, time_str);
                    break;
                case DEFERRED_LOGGING:
                    doDeferredLogging(dir);
                    break;
                }
            }
            else
            {
                /** A modem crash : try to store existing logging data with
                 * coredump already retreived. */
                IceraLogStoreModemLoggingData(log_data.dir, dir);
            }

            /** Store additional AP logs in dir, whatever the results of the logging operations
             *
             * In case of COREDUMP_LOADER log_type, this will trigger a stop of ril-daemon,
             * time for icera-crashlogs to get modem full coredump...
             */
            if (create_dir)
            {
                IceraLogStore(
                        log_type,
                        dir,
                        time_str);
            }
            else
            {
                IceraLogStore(
                        log_type,
                        NULL,
                        NULL);

            }
        }

        /* Logging terminated, update log_running */
        create_dir = 1;
        log_running = false;
        pthread_mutex_unlock(&log_event_mutex);
    };
    return NULL;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/
int IsLoggingInitialised(void)
{
    return logging_initialised;
}

int IsDeferredLogging(void)
{
    return (modem_logging_type == DEFERRED_LOGGING);
}

void StartDeferredLogging(void)
{
    ALOGI("%s", __FUNCTION__);

    /* Start logging daemon:
     * icera_log_serial_arm -d /dev/log_modem -e /data/rfs/data/debug/daemon_tracegen.ix
     * icera_log_serial_arm -b -p -x -t -d /dev/log_modem -a /data/rfs/data/debug,100,10 -r1,80,30 */
    char *cmd;
    asprintf(&cmd,
            "%s -d %s -e %s/%s_%s%s",
            LOGGING_TOOL,
            LOGGING_CHANNEL,
            log_data.dir,
            LOGGING_DEFERRED_PREFIX,
            LOGGING_DB_PREFIX,
            LOGGING_DB_EXTENSION);
    ALOGD("%s: %s", __FUNCTION__, cmd);
    int err = system(cmd);
    free(cmd);
    if (err)
    {
        ALOGE("%s: Fail to extract signal database.", __FUNCTION__);
    }

    asprintf(&cmd,
             "%s -b -p -x -t -d %s -a %s,%d,%d -r1,%d,%d &",
             LOGGING_TOOL,
             LOGGING_CHANNEL,
             log_data.dir,
             modem_logging_max_size,
             modem_logging_max_filesize,
             modem_logging_buf_percentage,
             modem_logging_duration_s);
    ALOGD("%s: %s", __FUNCTION__, cmd);
    err = system(cmd);
    free(cmd);
    if (err)
    {
        ALOGE("%s: Fail.", __FUNCTION__);
        logging_initialised = false;
    }
}

void SetModemLoggingEvent(LoggingType type, const char *msg, int len)
{
    if (logging_initialised)
    {
        if (!log_running)
        {
            if (modem_logging_type != DISABLED_LOGGING)
            {
               /* No logging on going, post logging event */
               pthread_mutex_lock(&log_event_mutex);
               log_type = type;
               log_running = true;
               if (msg != NULL)
               {
                   event_message_len = (len <= LOGGING_MAX_EVENT_MESSAGE_SIZE) ? len:LOGGING_MAX_EVENT_MESSAGE_SIZE;
                   memcpy(event_message_buffer,
                          msg,
                          event_message_len);
               }
               else
               {
                   ALOGW("%s: reporting event without message.", __FUNCTION__);
               }
               pthread_cond_signal(&log_event_cond);
               pthread_mutex_unlock(&log_event_mutex);
            }
            else
            {
                ALOGI("Modem logging disabled");
            }
        }
        else
        {
            /* Ignore logging event */
            ALOGI("%s: on going logging.", __FUNCTION__);
        }
    }
    else
    {
        ALOGW("%s: no modem logging done.", __FUNCTION__);
    }
}

void InitModemLogging(void)
{
    char prop[PROPERTY_VALUE_MAX];

    /* Init logging mutexes */
    pthread_mutex_init(&log_event_mutex, NULL);
    pthread_cond_init(&log_event_cond, NULL);

    /* Some default values */
    char *dir        = DEFAULT_LOGGING_DIR;
    int max          = LOGGING_DEFAULT_MAX_NUM_OF_FILES;

    /* Get logging options from property:
     * -d <dirname> : to indicate where to store logging data
     *                default: /data/rfs/data/debug
     *
     * -l <type>    : 0 - RAM logging, 1 - Deferred logging, 2 disabled
     *                default: RAM logging.
     *
     * -m <num>     : max num of logging session stored in fs
     *                default: 10
     *
     * -t <time_s>  : [RAM logging] - duration for one logging session
     *                [Deferred logging] - max time the target will not send log data for.
     *                default: 30s
     *
     * -f <size_MB> : [Deferred logging] - max size per logging file
     *                default: 10
     *
     * -s <size_MB> : [Deferred logging] - max size per all logging files when running as daemon.
     *                default: 100
     *
     * -b <percentage> : [Deferred logging] - log buffer percentage full needed to trigger sending
     *                   of log data from the target.
     *                   default: 80%
     */
    int ok = property_get("ril.modem.logging.args", prop, "");
    if (ok)
    {
        /* Build argv from args to parse in getopt... */
        int opt;
        int argc = 1; /* argv[0] never parsed by below getopt... */
        char *argv[MAX_LOGGING_ARGS];
        char *tok, *s;

        s = prop;
        while ((tok = strtok(s, " \0")))
        {
            if (argc > (MAX_LOGGING_ARGS))
            {
                ALOGE("%s: too much logging args", __FUNCTION__);
                return;
            }
            argv[argc] = tok;
            s = NULL;
            argc++;
        }

        int i;
        for (i=1; i<argc; i++)
        {
            ALOGI("%s: %d: %s", __FUNCTION__, i, argv[i]);
        }

        opterr = 0;
        optind = 1;
        while ( -1 != (opt = getopt(argc, argv, "d:l:m:t:f:s:")))
        {
            switch (opt)
            {
            case 'd':
                dir = optarg;
                break;

            case 'l':
                modem_logging_type = atoi(optarg);
                break;

            case 'm':
                max =  atoi(optarg);
                break;

            case 't':
                modem_logging_duration_s = atoi(optarg);
                break;

            case 's':
                modem_logging_max_size = atoi(optarg);
                break;

            case 'f':
                modem_logging_max_filesize = atoi(optarg);
                break;

            case 'b':
                modem_logging_buf_percentage = atoi(optarg);
                break;

            default:
                ALOGW("(%c) option will not be handled.", optopt);
                break;
            }
        }
    }

    modem_logging_duration_s = (modem_logging_duration_s <= 0) ? LOGGING_DEFAULT_DURATION : modem_logging_duration_s;
    modem_logging_buf_percentage = ((modem_logging_buf_percentage < 0) || (modem_logging_buf_percentage > 100)) ?
        LOGGING_DEFAULT_DEFERRED_BUFFER_PERCENTAGE : modem_logging_buf_percentage;

    /* Check some persistent props used for modem logging */
    if(modem_logging_type != DISABLED_LOGGING)
    {
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("persist.modem.log", prop, "enabled");
    }
    else
    {
        /* Either there's a system property, or logging is disabled */
        property_get("persist.modem.log", prop, "disabled");
    }
    if(strcmp(prop, "disabled") == 0)
    {
        modem_logging_type = DISABLED_LOGGING;
    }

    if(strcmp(dir, DEFAULT_LOGGING_DIR) != 0)
    {
        /* Either there's a system property either we do not overwrite command line arg */
        property_get("persist.modem.logdir", prop, dir);
    }
    else
    {
        /* Either there's a system property, or we use default log dir value */
        property_get("persist.modem.logdir", prop, DEFAULT_LOGGING_DIR);
    }

    /* Init generic logging data */
    strncpy(log_data.dir, prop, PATH_MAX);
    log_data.max = max;

    /* Init logging thread */
    if (pthread_create(&log_event_thread, NULL, getModemLoggingEvent, NULL))
    {
        ALOGE("%s: fail to start logging thread.", __FUNCTION__);
        return;
    }

    logging_initialised = true;
}
