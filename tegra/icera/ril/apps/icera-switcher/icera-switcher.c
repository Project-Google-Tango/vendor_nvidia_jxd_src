/*
 * Copyright (c) 2012-2014 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

/**
 * @file icera-switcher.c Icera Usb Mode Switch Daemon.
 *
 */

#define LOG_TAG "Icera-Switcher"
#include <utils/Log.h>

#include <usbhost/usbhost.h>
#include <stdlib.h>
#include "errno.h"

#include <cutils/properties.h> /* system properties */
#include <private/android_filesystem_config.h>

int verbose = 0;
int reducedProfile = 0;

#define ICERA_MODEM_DESCRIPTOR_STRING "Nvidia Icera Modem"
#define ICERA_MS_DESCRIPTOR_STRING    "Nvidia Icera MS"

static int usb_device_removed(const char *dev_name, void *client_data);
static int usb_device_added(const char *dev_name, void *client_data);
static void SendSwichCommand(struct usb_device * dev);
static void CheckDevice(struct usb_device *device);

/*
 * Assumption is that only one interface and that 1st OUT EP is the
 * good one
 */
static void SendSwichCommand(struct usb_device * dev)
{
    int ret = 0,len = 0;
    char SwitchCommand[]= { 0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
                            0xFF, 0x01, 0x01, 0x00, 0x00, 0x00, 0x7A, 0x00, 0x21, 0x03, 0x83, 0x19, 0x00, 0x00, 0x00, 0x00};

    if(reducedProfile)
    {
        /* While debugging on 8040, not enough EP with full profile */
        SwitchCommand[21] = 0x12; /* Downgrade from IAD_5AE to IAD_3AE*/
    }

    sleep(1);

    if(verbose)
    {
        /*Only show this in the log, in case there is a problem */
        ret = usb_device_is_writeable(dev);
        ALOGD("writeable: %d",ret);
    }

    ret = usb_device_connect_kernel_driver(dev,0, 0);
    if(verbose)
    {
        ALOGD("detach: %d",ret);
    }

    ret = usb_device_claim_interface(dev, 0);
    if(verbose)
    {
        ALOGD("claim: %d",ret);
    }

    ret = usb_device_bulk_transfer(dev,
                            1,
                            (void *)SwitchCommand,
                            sizeof(SwitchCommand),
                            0);
    if(verbose)
    {
        ALOGD("bulk transfer ret: %d/%d",ret,sizeof(SwitchCommand));
    }

    usb_device_release_interface(dev, 0);
}

int main(int argc, char *argv[])
{
    int opt;
    ALOGD("Monitoring starting...");
    while ( -1 != (opt = getopt(argc, argv, "vp")))
    {
        switch (opt)
        {
            case 'v':
            ALOGD("Verbose enabled");
            verbose = 1;
            break;
            case 'p':
            ALOGD("Reduced profile enabled");
            reducedProfile = 1;
            break;
            default:
                ALOGE("Option not supported, ignoring");
                break;
        }
    }

    struct usb_host_context* context = usb_host_init();
    if (!context) {
        ALOGE("usb_host_init failed");
        /*
         * Wait before exiting so we don't lock the machine
         * if we're continuously restarted
         */
        sleep(10);
        return -1;
    }

    /* Doesn't normally return */
    usb_host_run(context, usb_device_added, usb_device_removed, NULL, NULL);
    ALOGE("usb_host_run returned");
    return 0;
}

static void RestartRil(void)
{
    int mode;
    char prop[PROPERTY_VALUE_MAX];

    property_get("ril.testmode", prop, "0");
    mode = strtol(prop, NULL, 10);
    if (mode == 0)
    {
        ALOGI("Start RIL");
        property_set("gsm.modem.ril.enabled", "1");
    }
    else if (mode == 1)
    {
        ALOGI("Start RIL TEST");
        property_set("gsm.modem.riltest.enabled", "1");
    }
}

static void CheckDevice(struct usb_device *device)
{
    /*
     * "A la" identity morphing
     * If this string returns something then we know device is supported
     */
    char* ModemString = usb_device_get_string(device, 0xE1);

    if(ModemString != NULL)
    {
        if(verbose)
        {
            ALOGD("String: %s",ModemString);
        }

        if(strstr(ModemString,ICERA_MODEM_DESCRIPTOR_STRING)==ModemString)
        {
            /* Just need to restart rild in that case */
            ALOGD("Modem found, restarting RILD...");
            RestartRil();
        }

        if(strstr(ModemString,ICERA_MS_DESCRIPTOR_STRING)==ModemString)
        {
            /* Just need to restart rild in that case */
            ALOGD("Compatible mass storage found, flipping...");
            SendSwichCommand(device);
        }
        free(ModemString);
    }
    else
    {
        if(verbose)
        {
            ALOGD("Not a supported device, ignoring...");
        }
    }
}

static int usb_device_added(const char *dev_name, void *client_data)
{
    (void) client_data;
    struct usb_device * device = NULL;
    if(verbose)
    {
        ALOGI("dev added: %s",dev_name);
    }

    device = usb_device_open(dev_name);
    if(device!=NULL)
    {
        if(verbose)
        {
            ALOGD("Checking device: %x:%x [%s/%s] (ser#:%s)",
                usb_device_get_vendor_id(device),
                usb_device_get_product_id(device),
                usb_device_get_manufacturer_name(device),
                usb_device_get_product_name(device),
                usb_device_get_serial(device));
        }
        else
        {
            ALOGD("Checking device: [%s/%s]",
                usb_device_get_manufacturer_name(device),
                usb_device_get_product_name(device));
        }

        switch(usb_device_get_vendor_id(device))
        {
            /* Potentially supported devices */
            case 0x19D2:
            case 0x1983:
            case 0x0489:
                /* Check for the homemade descriptors */
                CheckDevice(device);
                break;
            default:
                ALOGD("Device not supported...");
        }
        usb_device_close(device);
    }
    else
    {
        ALOGE("Problem opening device %d",errno);
    }

    return 0;
}

static int usb_device_removed(const char *dev_name, void *client_data)
{
    (void) client_data;
    /*
     * Nothing needed, Ril will try to recover
     * and go dormant naturally if device is absent
     */
    ALOGI("dev removed: %s",dev_name);

    return 0;
}

