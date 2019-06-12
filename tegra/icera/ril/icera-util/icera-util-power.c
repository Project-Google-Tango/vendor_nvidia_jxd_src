/* Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved. */
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

/** EDP macros */
#define EDP_PROPERTY                    "gsm.modem.edp.device"
#define EDP_SYSFS_REGISTER_SUFFIX       "consumer_register"
#define EDP_SYSFS_UNREGISTER_SUFFIX     "consumer_unregister"
#define EDP_SYSFS_CONSUMERS_SUBDIR      "consumers"
#define EDP_SYSFS_MDM_CONSUMER_NAME     "icemdm"
#define EDP_SYSFS_STATE_REQUEST_PATH    "gsm.modem.edp.state"
#define EDP_SYSFS_STRING_LEN         100
#define EDP_STATE_ID_MAX_STRLEN      3

/**************************************************************************************************************/
/* Private Functions Declarations (if needed) */
/**************************************************************************************************************/

/**************************************************************************************************************/
/* Private Type Declarations */
/**************************************************************************************************************/

typedef struct
{
    char power_device_config[PROPERTY_VALUE_MAX];
    char *power_device_path;
    char *power_on_string;
    char *power_off_string;
    char usb_host_always_on_config[PROPERTY_VALUE_MAX];
    char *usb_host_always_on_control_path;
    char *usb_host_always_on_active;
    char *usb_host_always_on_inactive;
}PowerDevice;

typedef struct
{
    char config[PROPERTY_VALUE_MAX];
    char state_path[PROPERTY_VALUE_MAX];
    char *root_path;
}EDPDevice;

/**************************************************************************************************************/
/* Private Variable Definitions  */
/**************************************************************************************************************/

static PowerDevice power_control_device =
{
    .power_device_path = NULL,
    .power_on_string   = NULL,
    .power_off_string  = NULL
};

static PowerDevice usb_modem_power_device =
{
    .power_device_path = NULL,
    .power_on_string   = NULL,
    .power_off_string  = NULL
};

static EDPDevice edp_device =
{
   .root_path = NULL,
};

/*************************************************************************************************
 * Private function definitions
 ************************************************************************************************/

/**
 * Effective write to a sysfs device.
 *
 * @param device_path path to the device.
 * @param data string to be written to the device.
 *
 * @return static void
 */
static void SysFsDeviceWrite(char *device_path,
                             char *data)
{
    int fd;

    fd = open(device_path, O_WRONLY);
    if(fd < 0)
    {
        ALOGE("%s: Failed to open %s. %s", __FUNCTION__, device_path, strerror(errno));
        return;
    }

    ALOGD("%s: writing '%s' to '%s'", __FUNCTION__, data, device_path);
    if(write(fd, data, strlen(data)) != (int)strlen(data))
    {
        ALOGE("%s: Failed to write to %s. %s\n",__FUNCTION__, device_path, strerror(errno));
    }

    close(fd);

    return;
}

/**
 * Check whether EDP consumer is registered
 *
 * Pre-condition: EDP initialized
 *
 * @return 1 if registered
 */
static int EDPIsRegistered(void)
{
    char *filename = NULL;
    int ret = 0;
    int len;
    struct stat sb;

    do
    {
        len = asprintf(&filename,
                       "%s/%s/%s",
                       edp_device.root_path,
                       EDP_SYSFS_CONSUMERS_SUBDIR,
                       EDP_SYSFS_MDM_CONSUMER_NAME);
        if (len<0)
        {
            ALOGE("%s: Failed to allocate memory\n", __FUNCTION__);
            break;
        }

        /* check to see if consumer directory exists */
        if (stat(filename,&sb)==0)
        {
            ret = 1;
        }
    } while(0);

    free(filename);

    return ret;
}

/**
 * Register modem as new EDP consumer
 *
 * Pre-condition: EDP initialized
 *
 * @param pThresh Array of power threshold values
 * @param pMax Array of max power values
 * @param n Number of values in above arrays
 * @return int 0, or strictly negative value on failure
 */
static int EDPRegister(int *pThresh, int *pMax, int n)
{
    char *filename;
    char *regcmd;
    int ret = -1;
    int i;
    int bytes_written;

    /* the following do {} while(0) is used to avoid nested if and return
       statements within body of function */
    do
    {
        filename = calloc(EDP_SYSFS_STRING_LEN, sizeof(*filename));
        regcmd = calloc(EDP_SYSFS_STRING_LEN, sizeof(*filename));
        if ( (!filename) || (!regcmd) )
        {
            ALOGE("Failed to allocate memory\n");
            break;
        }

        /* construct path to sysfs registration entry */
        bytes_written = snprintf(filename,
                                 EDP_SYSFS_STRING_LEN,
                                 "%s/%s",
                                 edp_device.root_path,
                                 EDP_SYSFS_REGISTER_SUFFIX);
        if (bytes_written>=EDP_SYSFS_STRING_LEN)
        {
            ALOGE("Filename too long\n");
            break;
        }

        /* construct registration command */
        bytes_written = snprintf(regcmd,
                                  EDP_SYSFS_STRING_LEN,
                                  "%s ",
                                  EDP_SYSFS_MDM_CONSUMER_NAME);

        /* first provide power thresholds */
        for (i=0; (i<n) && (bytes_written<EDP_SYSFS_STRING_LEN-1); i++)
        {
            bytes_written += snprintf(regcmd+bytes_written,
                                      EDP_SYSFS_STRING_LEN - bytes_written,
                                      "%d%c",
                                      pThresh[i],
                                      i<(n-1)? ',' : ';');
        }

        /* now provide max power values */
        for (i=0; (i<n) && (bytes_written<EDP_SYSFS_STRING_LEN-1); i++)
        {
            bytes_written += snprintf(regcmd+bytes_written,
                                      EDP_SYSFS_STRING_LEN - bytes_written,
                                      "%d%s",
                                      pMax[i],
                                      i<(n-1)? "," : "");
        }

        if (bytes_written >= EDP_SYSFS_STRING_LEN)
        {
            ALOGE("Registration command too long\n");
            break;
        }

        /* register */
        SysFsDeviceWrite(filename, regcmd);

        /* verify registration was successful */
        if (!EDPIsRegistered())
        {
            ALOGE("Failed to register EDP consumer\n");
            break;
        }

        ret = 0;
    } while (0);

    free(filename);
    free(regcmd);

    return ret;
}

/**
 * Unregister modem from EDP
 *
 * Pre-condition: EDP initialized
 *
 * @return int 0, or strictly negative value on failure
 */
static int EDPUnregister(void)
{
    char *filename = NULL;
    int len;
    int ret = -1;

    /* the following do {} while(0) is used to avoid nested if and return
       statements within body of function */
    do
    {
        /* construct path to sysfs unregistration entry */
        len = asprintf(&filename,
                       "%s/%s",
                       edp_device.root_path,
                       EDP_SYSFS_UNREGISTER_SUFFIX);
        if (len<0)
        {
            ALOGE("%s: Failed to allocate memory\n", __FUNCTION__);
            break;
        }

        /* unregister */
        SysFsDeviceWrite(filename, EDP_SYSFS_MDM_CONSUMER_NAME);

        /* verify unregistration was successful */
        if (EDPIsRegistered())
        {
            ALOGE("Failed to unregister EDP consumer\n");
            break;
        }

        ret = 0;
    } while(0);

    free(filename);

    return ret;
}

/*************************************************************************************************
 * Public function definitions (Not doxygen commented, as they should be exported in the header
 * file)
 ************************************************************************************************/

void IceraModemPowerControlConfig(void)
{
    int ok;

    memset(power_control_device.power_device_config, 0, PROPERTY_VALUE_MAX);
    ok = property_get("gsm.modem.power.device",
                      power_control_device.power_device_config,
                      "");
    ALOGD("%s: found property %d %s\n",
         __FUNCTION__,
         ok,
         power_control_device.power_device_config);
    if(ok)
    {
        /* Configure power control with property input */
        power_control_device.power_device_path = strtok (power_control_device.power_device_config,",");
        power_control_device.power_off_string  = strtok (NULL,",");
        power_control_device.power_on_string   = strtok (NULL,",");
    }
    else
    {
        ALOGI("Modem Power Control not configured with gsm.modem.power.device property.\n");
    }

    return;
}

power_control_status_t IceraModemPowerStatus(void)
{
    int ok;
    char power_control[PROPERTY_VALUE_MAX];

    if(power_control_device.power_device_path == NULL)
    {
        ALOGD("Power control device not configured - exiting power control.\n");
        return UNCONFIGURED;
    }

    /* Modem power control can be disabled at runtime (during a reboot for example...)
        Check this property prior to any modem power action */
    ok = property_get("gsm.modem.powercontrol", power_control ,"");
    if(ok)
    {
        if((strcmp(power_control, "fil_disabled") == 0) || (strcmp(power_control, "disabled") == 0))
        {
            return OVERRIDDEN;
        }
    }

    return NORMAL;
}

power_control_status_t IceraModemPowerControl(power_control_command_t cmd, int force)
{
    power_control_status_t powerStatus = IceraModemPowerStatus();

    if((powerStatus==UNCONFIGURED)||
      ((powerStatus!=NORMAL)&&(force==0)))
    {
        return powerStatus;
    }

    ALOGD("%s: %d %s %s\n",
         __FUNCTION__,
         cmd,
         power_control_device.power_device_path,
         power_control_device.power_on_string);

    switch(cmd)
    {
    case MODEM_POWER_ON:
        ALOGD("Modem Power ON \n");
        SysFsDeviceWrite(power_control_device.power_device_path,
                         power_control_device.power_on_string);
        break;
    case MODEM_POWER_OFF:
        ALOGD("Modem Power OFF \n");
        SysFsDeviceWrite(power_control_device.power_device_path,
                         power_control_device.power_off_string);
        break;
    case MODEM_POWER_CYCLE:
        ALOGD("Modem Power Cycle \n");
        SysFsDeviceWrite(power_control_device.power_device_path,
                         power_control_device.power_off_string);
        usleep(100);
        SysFsDeviceWrite(power_control_device.power_device_path,
                         power_control_device.power_on_string);
        break;
    default:
        ALOGD("Unknown Power Control command \n");
        break;
    }

    return NORMAL;
}

int IceraUsbStackConfig(void)
{
    int ok;

    memset(usb_modem_power_device.power_device_config, 0, PROPERTY_VALUE_MAX);
    ok = property_get("gsm.modem.power.usbdevice",
                      usb_modem_power_device.power_device_config,
                      "");
    ALOGD("%s: found property %d %s\n",
         __FUNCTION__,
         ok,
         usb_modem_power_device.power_device_config);
    if(ok)
    {
        /* Configure power control with property input */
        usb_modem_power_device.power_device_path = strtok (usb_modem_power_device.power_device_config,",");
        usb_modem_power_device.power_off_string  = strtok (NULL,",");
        usb_modem_power_device.power_on_string   = strtok (NULL,",");
    }
    else
    {
        ALOGI("Modem USB Power Control not configured with modem.power.usbdevice property.\n");
    }

    memset(usb_modem_power_device.usb_host_always_on_config, 0, PROPERTY_VALUE_MAX);
    ok = property_get("gsm.modem.power.usb_always_on",
                      usb_modem_power_device.usb_host_always_on_config,
                      "");
    ALOGD("%s: found property %d %s\n",
         __FUNCTION__,
         ok,
         usb_modem_power_device.usb_host_always_on_config);
    if(ok)
    {
        /* Configure host controller always on with property input */
        usb_modem_power_device.usb_host_always_on_control_path = strtok (usb_modem_power_device.usb_host_always_on_config,",");
        usb_modem_power_device.usb_host_always_on_active       = strtok (NULL,",");
        usb_modem_power_device.usb_host_always_on_inactive     = strtok (NULL,",");
    }
    else
    {
        ALOGI("Modem USB host always-on mode control not configured with gsm.modem.power.usb_always_on property.\n");
    }

    return ok;
}

void IceraUnloadUsbStack(void)
{
    if((usb_modem_power_device.power_device_path == NULL)||
      (usb_modem_power_device.power_off_string == NULL))
    {
        ALOGD("Modem USB Power control device not configured - exiting usb power control.\n");
        return;
    }

    ALOGI("%s: Unload modem USB stack", __FUNCTION__);
    SysFsDeviceWrite(usb_modem_power_device.power_device_path,
                     usb_modem_power_device.power_off_string);
}

void IceraReloadUsbStack(void)
{
    if((usb_modem_power_device.power_device_path == NULL)||
      (usb_modem_power_device.power_on_string == NULL))
    {
        ALOGD("Modem USB Power control device not configured - exiting usb power control.\n");
        return;
    }

    ALOGI("%s: Reload modem USB stack", __FUNCTION__);
    SysFsDeviceWrite(usb_modem_power_device.power_device_path,
                     usb_modem_power_device.power_on_string);
}


void IceraUsbHostAlwaysOnActive(void)
{
    if((usb_modem_power_device.usb_host_always_on_control_path == NULL)||
      (usb_modem_power_device.usb_host_always_on_active == NULL))
    {
        ALOGD("Modem USB host always on control not configured - exiting usb power control.\n");
        return;
    }

    ALOGI("%s: Disable USB root hub autosuspend", __FUNCTION__);
    SysFsDeviceWrite(usb_modem_power_device.usb_host_always_on_control_path,
                     usb_modem_power_device.usb_host_always_on_active);
}

void IceraUsbHostAlwaysOnInactive(void)
{
    if((usb_modem_power_device.usb_host_always_on_control_path == NULL)||
      (usb_modem_power_device.usb_host_always_on_inactive == NULL))
    {
        ALOGD("Modem USB host always on control not configured - exiting usb power control.\n");
        return;
    }

    ALOGI("%s: Enable USB root hub autosuspend", __FUNCTION__);
    SysFsDeviceWrite(usb_modem_power_device.usb_host_always_on_control_path,
                     usb_modem_power_device.usb_host_always_on_inactive);
}

int IceraEDPInit(void)
{
    int ok;
    int ret = -1;
    struct stat sb;
    int len;

    /* the following do {} while(0) is used to avoid nested if and return
       statements within body of function */
    do
    {
        /* expected format of property value is the path to the root of EDP
           sysfs entries, e.g. "/sys/power/sysedp" */
        memset(edp_device.config, 0, PROPERTY_VALUE_MAX);
        ok = property_get(EDP_PROPERTY,
                          edp_device.config,
                          "");
        if (!ok)
        {
            ALOGE("Modem EDP Control not configured with %s property.\n",
                  EDP_PROPERTY);
            break;
        }

        ALOGD("%s: found property: \"%s\"\n",
             __FUNCTION__,
             edp_device.config);

        /* verify sysfs root entry exists */
        if (stat(edp_device.config,&sb)!=0)
        {
            ALOGE("Failed to stat Modem EDP Device %s:%s\n",
                  edp_device.config,
                  strerror(errno));
            break;
        }

        /* Configure edp device path from property */
        edp_device.root_path = edp_device.config;

        /* expected format of property value is the path to the sysfs
	   entry through which the sysedp state request is made */
        memset(edp_device.state_path, 0, PROPERTY_VALUE_MAX);
        ok = property_get(EDP_SYSFS_STATE_REQUEST_PATH,
                          edp_device.state_path,
                          "");
        if (!ok)
        {
            ALOGE("Modem EDP State Control not configured with %s property.\n",
                  EDP_SYSFS_STATE_REQUEST_PATH);
            break;
        }

        ALOGD("%s: found property: \"%s\"\n",
             __FUNCTION__,
             edp_device.state_path);

        /* verify sysfs path exists */
        if (stat(edp_device.state_path,&sb)!=0)
        {
            ALOGE("Failed to stat Modem EDP State File%s:%s\n",
                  edp_device.state_path,
                  strerror(errno));
            break;
        }

        ret = 0;

    } while (0);

    return ret;
}

int IceraEDPConfigureStates(int *pThresh, int *pMax, int n)
{
    int ret = -1;

    /* the following do {} while(0) is used to avoid nested if and return
       statements within body of function */
    do
    {
        /* was EDP initialized? */
        if (edp_device.root_path == NULL)
        {
            ALOGE("Modem EDP Control not configured.\n");
            break;
        }

        /* is the modem already registered to EDP? */
        if (EDPIsRegistered())
        {
            ALOGE("EDP device already registered -> unregistering now\n");
            /* unregister */
            if (EDPUnregister()!=0)
            {
                ALOGE("Failed to unregister\n");
                break;
            }
        }

        /* register */
        if (EDPRegister(pThresh, pMax, n)!=0)
        {
            ALOGE("Failed to register\n");
            break;
        }

        ret = 0;

    } while (0);

    return ret;
}

int IceraEDPSetState(int state)
{
    int ret = -1;
    char tmp[EDP_STATE_ID_MAX_STRLEN];
    int len;

    /* the following do {} while(0) is used to avoid nested if and return
       statements within body of function */
    do
    {
        /* was EDP initialized? */
        if (edp_device.root_path == NULL)
        {
            ALOGE("Modem EDP Control not configured.\n");
            break;
        }

        /* build string from state ID */
        len = snprintf(tmp, EDP_STATE_ID_MAX_STRLEN, "%d", state);
        if (len >= EDP_STATE_ID_MAX_STRLEN)
        {
            ALOGE("Illegal state ID (%d)\n", state);
            break;
        }

        /* set state */
        SysFsDeviceWrite(edp_device.state_path, tmp);

        ret = 0;

    } while (0);

    return ret;
}
