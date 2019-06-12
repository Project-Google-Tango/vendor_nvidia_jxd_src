/*************************************************************************************************
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 ************************************************************************************************/

/**
 * @addtogroup fild
 * @{
 */

/**
 * @file fild_port.h FILD OS abs layer header.
 *
 */

#ifndef FILD_PORT_H
#define FILD_PORT_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <sys/stat.h>   /** S_IREAD, S_IRUSR,... */
#include <pthread.h>    /** pthread_t... */
#include <semaphore.h>  /** sem_t... */
#include <fcntl.h>      /** O_RDONLY,... */
#include <ctype.h>      /** isspace */

#ifdef ANDROID
#include <cutils/properties.h> /* system properties */
#include <icera-util.h>
#else
#include <string.h> /* strcat, strlen,... */
#include <stdlib.h> /* malloc, free,... */
#include <unistd.h> /* SEEK_SET... */
#include <stddef.h> /* offsetof,... */
#include <stdio.h>  /* snprintf */
#endif

#ifdef ANDROID
//#define FILD_EXTERNAL_LOG    /** Activate external logging mechanism */
#else
#define FILD_EXTERNAL_LOG      /** Activate external logging mechanism */
#define PROPERTY_VALUE_MAX 512
#endif

#ifndef FILD_EXTERNAL_LOG
#define LOG_TAG "FILD"
#include <utils/Log.h>
#endif

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
/* Max len in bytes for a full directory path */
#define MAX_DIRNAME_LEN 128

/* Max len in bytes for a full filename path */
#define MAX_FILENAME_LEN 256

#ifndef S_IREAD
#define S_IREAD		0000400
#endif
#ifndef S_IWRITE
#define	S_IWRITE	0000200
#endif

/**
 * Default flags used when creating a file:
 * rw-rw----
 */
#define DEFAULT_CREAT_FILE_MODE (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)

/** SHM interfaces */
#define DEFAULT_SHM_PRIV_IF  "/dev/tegra_bb_priv0"
#define DEFAULT_SHM_IPC_IF   "/dev/tegra_bb_ipc0"
#define DEFAULT_SHM_SYSFS_IF "/sys/devices/platform/tegra_bb.0"

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/
/* Verbosity levels used only for debug purpose...: corresponds to logcat levels as
    shown with 'adb logcat --help' cmd */
enum
{
    FILD_VERBOSE_SILENT
    ,FILD_VERBOSE_FATAL
    ,FILD_VERBOSE_ERROR
    ,FILD_VERBOSE_WARNING
    ,FILD_VERBOSE_INFO
    ,FILD_VERBOSE_DEBUG
    ,FILD_VERBOSE_VERBOSE
};

typedef pthread_t fildThread;             /** FILD thread type */
typedef sem_t fildSem;                    /** FILD semaphore type */
typedef timer_t fildTimer;                /** FILD timer type */
typedef void (*fildTimerHandler)(void *); /** FILD timer handler */
typedef char * fildModemPowerControl;     /** FILD modem power control */

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/
/** FILD current verbosity level: for debug or external logging only */
extern int fild_verbosity;

/** Name of sysfs folder in file system */
extern char fild_shm_sysfs_name[MAX_DIRNAME_LEN];

/** Name of private memory device in file system */
extern char fild_shm_priv_dev_name[MAX_DIRNAME_LEN];

/** Name of ipc memory device in file system */
extern char fild_shm_ipc_dev_name[MAX_DIRNAME_LEN];

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/
extern void Out(int level, const char *fmt, ...);

/**************************************************
 * Logging (if not default Android's one)
 *************************************************/
#ifdef FILD_EXTERNAL_LOG
#include <stdio.h>
#define OUT(verbosity, fmt...) Out(verbosity, fmt)
#define LOGF(...) OUT(FILD_VERBOSE_FATAL, "F/FILD: "__VA_ARGS__)
#define ALOGE(...) OUT(FILD_VERBOSE_ERROR, "E/FILD: "__VA_ARGS__)
#define ALOGW(...) OUT(FILD_VERBOSE_WARNING, "W/FILD: "__VA_ARGS__)
#define ALOGI(...) OUT(FILD_VERBOSE_INFO, "I/FILD: "__VA_ARGS__)
#define ALOGD(...) OUT(FILD_VERBOSE_DEBUG, "D/FILD: "__VA_ARGS__)
#define ALOGV(...) OUT(FILD_VERBOSE_VERBOSE, "V/FILD: "__VA_ARGS__)
#endif

#endif /* FILD_H */
