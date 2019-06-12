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

#ifndef ICERA_UTIL_H
#define ICERA_UTIL_H

/*************************************************************************************************
 * Project header files
 ************************************************************************************************/

/*************************************************************************************************
 * Standard header files (e.g. <string.h>, <stdlib.h> etc.
 ************************************************************************************************/
#include <stdlib.h>
#include <stdbool.h>
#include <linux/limits.h>

/*************************************************************************************************
 * Macros
 ************************************************************************************************/
#define MAX_TIME_LENGTH    15           /** yyyymmdd_hhmmss */
#define PREFIX_MAX 32

/**
 * Prefix given to created files when running icera_log_serial_arm as a daemon:
 * this cannot be changed and comes directly from icera_log_serial_arm...
 */
#define LOGGING_DEFERRED_PREFIX "daemon"

/* Coredump files */
#define COREDUMP_NAME_PREFIX "coredump"
#define COREDUMP_NAME_EXTENSION ".bin"

/* Default root dir for any Icera data */
#define DEFAULT_ICERA_ROOT_DIR "/data/rfs"

/* Default dir for modem logging  storage */
#define DEFAULT_LOGGING_DIR DEFAULT_ICERA_ROOT_DIR"/data/debug"

/*************************************************************************************************
 * Public Types
 ************************************************************************************************/

typedef enum
{
    MODEM_POWER_ON,
    MODEM_POWER_OFF,
    MODEM_POWER_CYCLE
}power_control_command_t;

typedef enum
{
    UNCONFIGURED =  -2,
    OVERRIDDEN =    -1,
    NORMAL =         0,
}power_control_status_t;

typedef enum
{
    MODEM_LOGGING,                /* modem logging */
    COREDUMP_MODEM,               /* modem coredump after crash */
    COREDUMP_LOADER,              /* modem coredump in loader after crash */
    MODEM_CRASH_DETECTED_SINGLE,  /* modem crash detected through AT cmd, in modem mode, on a single flash platform */
    MODEM_CRASH_DETECTED_DUAL,    /* modem crash detected through AT cmd, in modem mode, on a dual flash platform */
}LoggingType;

typedef struct
{
    char dir[PATH_MAX];    /* root dir to store logging data */
    char prefix[PREFIX_MAX]; /* prefix for dirname where data is stored */
    int max;    /* max num of logging session to store */
}LoggingData;

/*************************************************************************************************
 * Public variable declarations
 ************************************************************************************************/

/*************************************************************************************************
 * Public function declarations
 ************************************************************************************************/

/**
 *  Configure sysfs device that will be used for power control
 *  utilities.
 *
 *  Requires "gsm.modem.power.device" set as system property with
 *  following format:
 *  <power_sysfs_device_path>,<power_off_string>,<power_on_string>
 *
 *  where <power_off_string>,<power_on_string> are values to
 *  write in <power_sysfs_device_path> to handle poff or pon.
 *
 */
void IceraModemPowerControlConfig(void);

/**
 * Power control utilities in power_control_command_t.
 *
 * @param cmd    power cmd in power_control_command_t
 * @param force  bypass "gsm.modem.powercontrol" setting
 *
 * @return power_control_status_t NORMAL if command is successful, OVERRIDDEN if overridden UNCONFIGURED if unconfigured
 */
power_control_status_t IceraModemPowerControl(power_control_command_t cmd, int force);

/**
 * Gt power control status.
 *
 * @param None
 *
 * @return power_control_status_t NORMAL if command is successful, OVERRIDDEN if overridden UNCONFIGURED if unconfigured
 */
power_control_status_t IceraModemPowerStatus(void);

/**
 *  Register for wake lock framework
 *
 *  To enable/disable platform LP0
 *
 *
 * @param name      name of lock
 * @param use_count if true, all call to IceraWakeLockAcquire
 *                  increments a counter and each call to
 *                  IceraWakeLockRelease decrements it. Unlock
 *                  done only if counter is zero.
 *                  if false, unlock each time
 *                  IceraWakeLockRelease is called regardless
 *                  num of previous calls to
 *                  IceraWakeLockAcquire
 */
void IceraWakeLockRegister(char *name, bool use_count);

/**
 * Prevent LP0
 */
void IceraWakeLockAcquire(void);

/**
 * Allow LP0 (always if registered with use_count=false, only
 * when all unlock done if registered with use_count==true)
 */
void IceraWakeLockRelease(void);

/**
 *  Configure sysfs device that will be used for modem usb stack
 *  utilities.
 *
 *  Requires "gsm.modem.power.usbdevice" set as system property with
 *  following format:
 *  <mdm_usb_sysfs_device_path>,<unload_string>,<reload_string>
 *
 *  where <unload_string>,<reload_string> are values to
 *  write in <mdm_usb_sysfs_device_path> to handle unload/reload
 *  of modem USB stack.
 *
 *  Requires "modem.power.usb_host_always_on" set as system property
 *  with following format:
 *  <usb_host_always_on_control_path>,<always_on_active_string>,
 *  <always_on_inactive_string>
 *
 *  where <always_on_active_string>,<always_on_inactive_string> are
 *  values to write in <usb_host_always_on_control_path> to handle
 *  activation/deactivation of the always-on mode (no autosuspend) on
 *  modem root hub.
 *
 *  @return 1 if config was OK, 0 if not.
 *
 */
int IceraUsbStackConfig(void);

/**
 * Unload modem USB stack
 */
void IceraUnloadUsbStack(void);

/**
 * Reload modem USB stack
 */
void IceraReloadUsbStack(void);

/**
 *  Enable USB host controller always-on mode (no autosuspend)
 */
void IceraUsbHostAlwaysOnActive(void);

/**
 *  Disable USB host controller always-on mode (autosuspend)
 */
void IceraUsbHostAlwaysOnInactive(void);

/**
 * Store AP logs in dated files using icera-crashlogs executable
 *
 * @param type
 * @param dir  directory to store logs
 * @param time_str time stamp to append to log files.
 */
void IceraLogStore(
        LoggingType type,
        char *dir,
        char *time_str);

/**
 * Update a string with current date value.
 *
 * String updated with following format: yyyymmdd_hhmm
 *
 * @param str buffer to update.
 * @param len buffer max length.
 */
void IceraLogGetTimeStr(char *str, size_t len);

/**
 * Create a dated folder to store log data
 *
 * @param data
 * @param time_str
 * @param dir
 *
 * @return int 0, or -1 on failure.
 */
int IceraLogNewDatedDir(LoggingData *data, char *time_str, char *dir);

/**
 * Store any exiting modem logging data file from src_dir to dst_dir
 */
void IceraLogStoreModemLoggingData(char *src_dir, char *dst_dir);

/**
 * Dumps a kernel timereference into the current log
 */
#define LogKernelTimeStamp(A) {\
                struct timespec time_stamp;\
                clock_gettime(CLOCK_MONOTONIC, &time_stamp);\
                ALOGD("Kernel time: [%d.%6d]",(int)time_stamp.tv_sec,(int)time_stamp.tv_nsec/1000);\
                }

/**
 * Initialize EDP device
 * @return int 0, or strictly negative value on failure
 */
int IceraEDPInit(void);

/**
 * Configure EDP states
 * @param pThresh Array of power threshold values
 * @param pMax Array of max power values
 * @param n Number of values in above arrays
 * @return int 0, or strictly negative value on failure
 */
int IceraEDPConfigureStates(int *pThresh, int *pMax, int n);

/**
 * Set EDP state
 * @param n ID of current state
 * @return int 0, or strictly negative value on failure
 */
int IceraEDPSetState(int state);

#endif /* ICERA_UTIL_H */
