/*
 * Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include "nvddk_blockdevmgr.h"


typedef struct DeviceInfoRec
{
    NvDdkBlockDevMgrDeviceId DeviceId;
    pfNvDdkXxxBlockDevInit pfDevInit;
    pfNvDdkXxxBlockDevDeinit pfDevDeinit;
    pfNvDdkXxxBlockDevOpen pfDevOpen;
} DeviceInfo;

NvError
NvDdkNandBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);
NvError
NvDdkSdBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);
NvError
NvDdkAesBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);
NvError
NvDdkSpifBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);
NvError
NvDdkSnorBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);
NvError
NvDdkUsbhBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError NvDdkNandBlockDevInit(NvRmDeviceHandle hDevice);
NvError NvDdkSdBlockDevInit(NvRmDeviceHandle hDevice);
NvError NvDdkAesBlockDevInit(NvRmDeviceHandle hDevice);
NvError NvDdkSpifBlockDevInit(NvRmDeviceHandle hDevice);
NvError NvDdkSnorBlockDevInit(NvRmDeviceHandle hDevice);
NvError NvDdkUsbhBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkNandBlockDevDeinit(void);
void NvDdkSdBlockDevDeinit(void);
void NvDdkAesBlockDevDeinit(void);
void NvDdkSpifBlockDevDeinit(void);
void NvDdkSnorBlockDevDeinit(void);

NvError
NvDdkSeBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError NvDdkSeBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkSeBlockDevDeinit(void);

#if SATA_EXISTS
NvError
NvDdkSataBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError NvDdkSataBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkSataBlockDevDeinit(void);
#endif

#if XUSB_EXISTS
NvError
NvDdkXusbhBlockDevOpen(
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev);

NvError NvDdkXusbhBlockDevInit(NvRmDeviceHandle hDevice);

void NvDdkXusbhBlockDevDeinit(void);
#endif
void NvDdkUsbhBlockDevDeinit(void);

static DeviceInfo
s_DeviceInfo[] =
{
#ifndef BOOT_MINIMAL_BL
#if 0 // disabling un-tested NAND path
    {
        NvDdkBlockDevMgrDeviceId_Nand,
        NvDdkNandBlockDevInit,
        NvDdkNandBlockDevDeinit,
        NvDdkNandBlockDevOpen
    },
#endif
    {
        NvDdkBlockDevMgrDeviceId_Nor,
        NvDdkSnorBlockDevInit,
        NvDdkSnorBlockDevDeinit,
        NvDdkSnorBlockDevOpen
    },
#endif
    {
        NvDdkBlockDevMgrDeviceId_SDMMC,
        NvDdkSdBlockDevInit,
        NvDdkSdBlockDevDeinit,
        NvDdkSdBlockDevOpen
    },
#ifndef BOOT_MINIMAL_BL
#if defined(BSEAV_USED) && !defined(BUILD_FOR_COSIM)
    {
        NvDdkBlockDevMgrDeviceId_Aes,
        NvDdkAesBlockDevInit,
        NvDdkAesBlockDevDeinit,
        NvDdkAesBlockDevOpen
    },
#endif
    {
        NvDdkBlockDevMgrDeviceId_Spi,
        NvDdkSpifBlockDevInit,
        NvDdkSpifBlockDevDeinit,
        NvDdkSpifBlockDevOpen
    },
    {
        NvDdkBlockDevMgrDeviceId_Se,
        NvDdkSeBlockDevInit,
        NvDdkSeBlockDevDeinit,
        NvDdkSeBlockDevOpen
    },
#endif
#if XUSB_EXISTS
    {
        NvDdkBlockDevMgrDeviceId_Usb3,
        NvDdkXusbhBlockDevInit,
        NvDdkXusbhBlockDevDeinit,
        NvDdkXusbhBlockDevOpen
    },
#endif
#if SATA_EXISTS
    {
        NvDdkBlockDevMgrDeviceId_Sata,
        NvDdkSataBlockDevInit,
        NvDdkSataBlockDevDeinit,
        NvDdkSataBlockDevOpen
    },
#endif
#if USBH_EXISTS
    {
        NvDdkBlockDevMgrDeviceId_Usb2Otg,
        NvDdkUsbhBlockDevInit,
        NvDdkUsbhBlockDevDeinit,
        NvDdkUsbhBlockDevOpen
    }
#endif
};

static NvU32 s_NumDevices = NV_ARRAY_SIZE(s_DeviceInfo);
static NvBool s_IsInitialized = NV_FALSE;
static NvRmDeviceHandle s_RmDevice = NULL;
// Device manager reference count
static NvU32 s_DevMgrRefCount = 0;

NvError
NvDdkBlockDevMgrInit(void)
{
    // create global table of all devices
    // * index by DeviceType
    // * data = function table, driver init flag, and number of open instances
    // Note: initialize number of open instance to zero for each device

    // mark initialization as complete
    NvU32 i = 0;
    NvU32 k = 0;
    NvError e;

    if (!s_IsInitialized)
    {
        NV_CHECK_ERROR(NvRmOpen(&s_RmDevice, 0));

        for (i=0; i<s_NumDevices; i++)
        {
            NV_CHECK_ERROR_CLEANUP(
                (s_DeviceInfo[i].pfDevInit)(s_RmDevice));
        }

        s_IsInitialized = NV_TRUE;
    }
    // increment device manager reference count
    s_DevMgrRefCount++;

    return NvSuccess;

fail:
    // Make sure that only devices init successfully are deinit
    for (k=0; k<i; k++)
    {
            (s_DeviceInfo[k].pfDevDeinit)();
    }
    NvRmClose(s_RmDevice);
    s_RmDevice = NULL;

    return e;
}

void
NvDdkBlockDevMgrDeinit(void)
{
    NvU32 i = 0;
    if (!s_IsInitialized)
        return;

    if (s_DevMgrRefCount == 1)
    {
        // free global table of devices
        for (i=0; i<s_NumDevices; i++)
        {
                (s_DeviceInfo[i].pfDevDeinit)();
        }

        NvRmClose(s_RmDevice);
        s_RmDevice = NULL;

        // mark initialization as complete
        s_IsInitialized = NV_FALSE;
    }

    // decrement device manager ref count
    s_DevMgrRefCount--;
}

NvError
NvDdkBlockDevMgrDeviceOpen(
    NvDdkBlockDevMgrDeviceId DeviceId,
    NvU32 Instance,
    NvU32 MinorInstance,
    NvDdkBlockDevHandle *phBlockDev)
{
    NvU32 i = 0;
    NvError e;

    // look up DeviceType in global table
    for (i=0; i<s_NumDevices; i++)
    {
        if (s_DeviceInfo[i].DeviceId == DeviceId)
        {
            NV_CHECK_ERROR(
                (s_DeviceInfo[i].pfDevOpen) (
                    Instance, MinorInstance, phBlockDev));
            return NvSuccess;
        }
    }

    return NvError_ModuleNotPresent;
}

