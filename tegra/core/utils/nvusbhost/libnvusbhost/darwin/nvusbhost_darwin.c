/*
 * Copyright (c) 2009 - 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include <stdio.h>
#include "nvusbhost.h"
#include "usb.h"

#define DUMP_IN_PACKETS 0
#define DUMP_OUT_PACKETS 0

typedef struct NvUsbDeviceRec
{
    NvU8 buffer[64];
    NvU32 remaining;
    NvU32 read;
    usb_handle *usb;
    usb_ifc_info *info;
} NvUsbDevice;

static NvUsbDevice s_UsbDevice;

int is_tegra(usb_ifc_info *info)
{
    if (info->dev_vendor != 0x0955) return -1;

    printf("Found the Nvidia device\n");

    s_UsbDevice.info = info;
    return 0;
}

// dummy implementation to avoid build error
void NvUsbDeviceSetBusNum(const char* usbBusNum)
{
    return;
}

NvError NvUsbDeviceOpen(NvU32 instance, NvUsbDeviceHandle *hDev)
{
    NvUsbDevice *dev;

    if (instance > MAX_SUPPORTED_DEVICES)
        return NvError_NotImplemented;

    dev = &s_UsbDevice;
    NvOsMemset(dev, 0, sizeof(NvUsbDevice));

    dev->usb = usb_open(is_tegra);
    if (!dev->usb)
    {
        return NvError_AccessDenied;
    }

    *hDev = dev;
    return NvSuccess;
}

NvError
NvUsbDeviceQueryConnectionStatus(NvUsbDeviceHandle hDev, NvBool *IsConnected)
{
    return NvError_NotImplemented;
}

NvError
NvUsbDeviceWrite(NvUsbDeviceHandle hDev, const void *data, NvU32 length )
{
    int ret;
    NvError nverr = NvSuccess;

    if (DUMP_OUT_PACKETS)
    {
        int i;
        printf("Write: %d\n", length);
        for (i=0; i<length; i++)
           printf("%d ", *((NvU8 *)data + i));
        printf("\n");
    }

    if (!hDev || !hDev->usb)
    {
        return NvError_BadParameter;
    }

    ret = usb_write(hDev->usb, data, length);
    if (ret == -1) nverr = NvError_BadParameter;
    if (ret != (int)length) nverr = NvError_BadParameter;

    return nverr;
}

NvError
NvUsbDeviceRead(NvUsbDeviceHandle hDev, void *data, NvU32 length,
    NvU32 *bytesRead)
{
    int bytes;
    int count;
    NvBool bShort;
    NvU8 *ptr;
    NvU8 *orig_ptr = data;

    if (DUMP_IN_PACKETS)
        printf("read: req %d leftover %d\n", length, hDev->remaining);

    if (!hDev || !hDev->usb)
    {
        return NvError_BadParameter;
    }

    count = 0;
    if( hDev->remaining )
    {
        bytes = (hDev->remaining >= length) ? length : hDev->remaining;
        NvOsMemcpy(data, hDev->buffer + hDev->read, bytes);
        hDev->read += bytes;
        hDev->remaining -= bytes;
        count += bytes;

        if (hDev->remaining)
        {
            goto exit;
        }

        length -= bytes;
        data = ((NvU8*)data) + bytes;
        hDev->read = 0;
    }

    while (length)
    {
        NvU32 len = (length > 4096) ? 4096 : length;
        if( len < 64 )
        {
            ptr = hDev->buffer;
            bShort = NV_TRUE;
            len = 64;
        }
        else
        {
            bShort = NV_FALSE;
            ptr = data;
        }

        bytes = usb_read(hDev->usb, ptr, len);
        if (bytes == -1) {
            printf("failed to read req %d\n", len);
            return NvError_InvalidState;
        }
        if( bShort )
        {
            /* copy at most 'length' bytes to 'data',
               handle the remainder. */
            NvOsMemcpy(data, ptr, NV_MIN( bytes, (int)length));
            if (bytes > (int)length)
            {
                /* remainder */
                hDev->remaining = bytes - length;
                bytes = length;
                hDev->read = bytes;
            }
        }

        data = ((NvU8*)data) + bytes;
        length -= bytes;
        count += bytes;
    }

exit:
    if( bytesRead )
    {
        *bytesRead = count;
    }

    if (DUMP_IN_PACKETS)
    {
        int i;
        printf(" got %d remaining %d\n", count, hDev->remaining);
        for (i=0; i<count; i++)
            printf("%d ", *(orig_ptr + i));
        printf("\n");
    }
    return NvSuccess;
}

void NvUsbDeviceClose(NvUsbDeviceHandle hDev)
{
    usb_close(hDev->usb);
    NvOsMemset(hDev, 0, sizeof(NvUsbDevice));
}

void NvUsbDeviceAbort(NvUsbDeviceHandle hDev)
{
    // not implemented
}

NvError
NvUsbDeviceSetReadPolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy,
    NvBool enable)
{
    return NvError_NotImplemented;
}

NvError
NvUsbDeviceSetWritePolicy(NvUsbDeviceHandle hDev, NvUsbPipePolicy policy,
    NvBool enable)
{
    return NvError_NotImplemented;
}

void
NvUsbDeviceReadMode(NvUsbDeviceHandle hDev, void* ovl)
{
        // not implemented
}

NvError
NvUsbDeviceReadDevId(NvUsbDeviceHandle hDev, NvU32 *productId)
{
    if (!hDev || !productId)
    {
        return NvError_BadParameter;
    }

    *productId = hDev->info->dev_product;
    *productId &= 0xFF;
    return NvSuccess;
}

