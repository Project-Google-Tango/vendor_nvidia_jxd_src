/*
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#include "nvusbhost.h"
#include "nvassert.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <linux/usbdevice_fs.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,20)
#include <linux/usb/ch9.h>
#else
#include <linux/usb_ch9.h>
#endif

#define NVUSB_BUFFER_SIZE 96

typedef struct NvUsbDeviceRec
{
    int ep_in;
    int ep_out;
    int iface_number;
    int fd;

    NvU8 buffer[NVUSB_BUFFER_SIZE];
    NvU32 remaining;
    NvU32 read;
    NvU32 ProductId;
    NvU32 timeout;
} NvUsbDevice;

static NvUsbDevice s_UsbDevice;

/** s_UsbBusNum - specifiy the usb bus number to find a DUT
 * All devices appear the same. No serial number to
 * identify them right now. So the usb bus number is used
 * to support multiple DUTs on a single host.
 */
static const char* s_UsbBusNum = NULL;

static void
nvusb_decode_usbpath(NvU32 devinstance, char *decodedpath);

static NvBool
nvusb_parse_usb( NvU8 *buff, int bytes, int *ep_in, int *ep_out,
    int *iface_number, NvU32 *ProductId)
{
    struct usb_device_descriptor *dev;
    struct usb_config_descriptor *cfg;
    struct usb_interface_descriptor *iface;
    struct usb_endpoint_descriptor *ept;
    NvU8 *ptr;
    NvU32 i, j;

    #define PTR_INCR( length ) \
        ptr += (length); \
        bytes -= (length); \
        if( bytes < 0 ) \
        { \
            return NV_FALSE; \
        }

    ptr = buff;

    dev = (struct usb_device_descriptor *)ptr;
    PTR_INCR( dev->bLength );

    /* nvidia's vendor id for nvap is 0x955 */
    // FIXME: unhardcode this
    if( dev->idVendor != 0x955 )
    {
        return NV_FALSE;
    }

    // Store product ID
    *ProductId = dev->idProduct;

    cfg = (struct usb_config_descriptor *)ptr;
    PTR_INCR( cfg->bLength );

    for( i = 0; i < cfg->bNumInterfaces; i++ )
    {
        iface = (struct usb_interface_descriptor *)ptr;
        PTR_INCR( iface->bLength );

        //for the device in recovery mode vendor specific class,
        //subclass and protocol values are 0xff.
        //return false if it is not the case.(adb mode)
        if ((iface->bInterfaceClass != 0xff) ||
            (iface->bInterfaceSubClass != 0xff) ||
            (iface->bInterfaceProtocol != 0xff))
        {
            return NV_FALSE;
        }

        for( j = 0; j < iface->bNumEndpoints; j++ )
        {
            ept = (struct usb_endpoint_descriptor *)ptr;
            PTR_INCR( ept->bLength );

            if( (ept->bmAttributes & 0x3) != 0x2 )
            {
                continue;
            }

            if( (ept->bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN )
            {
                *ep_in = ept->bEndpointAddress;
            }
            else
            {
                *ep_out = ept->bEndpointAddress;
            }
        }

        *iface_number = iface->bInterfaceNumber;
    }

    #undef PTR_INCR
    return NV_TRUE;
}

static NvBool
check_name( const char *name )
{
    /* usb device names are all digits */
    while( *name )
    {
        if( !isdigit(*name++) )
        {
            return NV_FALSE;
        }
    }

    return NV_TRUE;
}

static void
nvusb_decode_usbpath(NvU32 devinstance, char *decodedpath)
{
    NvU32 rem, index, size = 9;
    // The encoding of the instance path encodes in the format
    // 1xxxyyy which is read backwards and decoded.
    decodedpath[size - 1] = '\0';
    decodedpath[0] = '/';
    decodedpath[4] = '/';
    index = size - 2;

    while (index)
    {
        if (index != 4)
        {
            rem = devinstance % 10;
            devinstance = devinstance / 10;
            decodedpath[index] = '0' + rem;
        }
        index--;
    }
}

static NvBool
nvusb_open_usb( NvUsbDevice *hUsb, NvU32 devinstance )
{
    NvBool b;
    NvBool bFound;
    DIR *usbdir = 0;
    DIR *devdir = 0;
    struct dirent *d;
    char dirname[64];
    char devname[64];
    unsigned char desc[1024];
    int fd = 0;
    int bytes;
    int e;
    struct stat sb;
    const char *usb = NULL;

    // Usually in /dev/bus (udev), but might be in /proc/usb (CentOS).
    usb = "/dev/bus/usb";
    if (stat(usb, &sb) == -1)
    {
        usb = "/proc/bus/usb";
        if (stat(usb, &sb) == -1)
        {
            goto fail;
        }
    }

    NvOsMemset( hUsb, 0, sizeof(hUsb) );

    // When instance argument is the usb device path string
    if (devinstance > MAX_SUPPORTED_DEVICES)
    {
        nvusb_decode_usbpath(devinstance, devname);
        strcpy(dirname, usb);
        strcat(dirname, devname);

        fd = open(dirname, O_RDWR);
        bytes = read(fd, desc, sizeof(desc));
        if(bytes < 0)
            goto fail;
        bFound = nvusb_parse_usb(desc, bytes, &hUsb->ep_in,
                 &hUsb->ep_out, &hUsb->iface_number, &hUsb->ProductId);
        if(bFound)
            hUsb->fd = fd;
        else
        {
            NvOsDebugPrintf("Failure to recognize USB device located at %s\n", dirname);
            goto fail;
        }
    }
    // When instance argument is the instance number
    /* enumerate the usb device:
     *
     * - walk the /dev/bus/usb directory tree
     * - for each device, read the device descriptor, find the nvidia device
     * - parse device descriptor, get the end points
     */
    else
    {
        usbdir = opendir( usb );
        if( !usbdir )
        {
            goto fail;
        }

        bFound = NV_FALSE;
        while( !bFound )
        {
            d = readdir( usbdir );
            if( !d )
            {
                break;
            }

            if( !check_name( d->d_name ) )
            {
                continue;
            }

            if( s_UsbBusNum &&
                NvOsStrcmp(s_UsbBusNum, d->d_name))
            {
                continue;
            }

            NvOsSnprintf( dirname, sizeof(dirname), "%s/%s", usb, d->d_name );

            devdir = opendir( dirname );
            if( !devdir )
            {
                continue;
            }

            for( ;; )
            {
                d = readdir( devdir );
                if( !d )
                {
                    break;
                }

                if( !check_name( d->d_name ) )
                {
                    continue;
                }

                NvOsSnprintf( devname, sizeof(devname), "%s/%s", dirname,
                    d->d_name );

                /* open/read the usb descriptor */
                fd = open( devname, O_RDWR );

                if( fd < 0 )
                {
                    continue;
                }

                bytes = read( fd, desc, sizeof(desc) );
                if( bytes < 0 )
                {
                    goto fail;
                }

                bFound = nvusb_parse_usb( desc, bytes, &hUsb->ep_in,
                    &hUsb->ep_out, &hUsb->iface_number, &hUsb->ProductId );

                if( bFound )
                {
                    if( devinstance == 0)
                    {
                        hUsb->fd = fd;
                        break;
                    }
                    devinstance--;
                }
                close( fd );
                fd = 0;
            }

            closedir( devdir );
            devdir = 0;
        }

        closedir( usbdir );
        usbdir = 0;
    }

    if( !bFound )
    {
        goto fail;
    }

    /* claim the device */
    e = ioctl( hUsb->fd, USBDEVFS_CLAIMINTERFACE, &hUsb->iface_number );
    if( e != 0 )
    {
        goto fail;
    }

    b = NV_TRUE;
    goto clean;

fail:
    if( fd > 0 ) close( fd );
    b = NV_FALSE;

clean:
    if( usbdir != 0 ) closedir( usbdir );
    if( devdir != 0 ) closedir( devdir );
    return b;
}

void NvUsbDeviceSetBusNum(const char* usbBusNum)
{
    s_UsbBusNum = usbBusNum;
}

NvError NvUsbDeviceOpen(NvU32 instance, NvUsbDeviceHandle *hDev)
{
    NvBool b;
    NvUsbDevice *dev;

    dev = &s_UsbDevice;

    b = nvusb_open_usb( dev, instance );
    if( !b )
    {
        if( errno == EACCES )
        {
            return NvError_AccessDenied;
        }
        else
        {
            return NvError_InvalidState;
        }
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
    struct usbdevfs_bulktransfer bulk;
    NvU8 *ptr;
    int bytes;

    NV_ASSERT( hDev && hDev->ep_out );

    if( length == 0 )
    {
        bulk.ep = hDev->ep_out;
        bulk.len = 0;
        bulk.data = (void *)data;
        bulk.timeout = 0;
        bytes = ioctl( hDev->fd, USBDEVFS_BULK, &bulk );
        if( bytes != 0 )
        {
            return NvError_InvalidState;
        }

        return NvSuccess;
    }

    ptr = (NvU8 *)data;
    while( length )
    {
        NvU32 len = (length > 4096) ? 4096 : length;

        bulk.ep = hDev->ep_out;
        bulk.len = len;
        bulk.data = ptr;
        bulk.timeout = 0;

        bytes = ioctl( hDev->fd, USBDEVFS_BULK, &bulk );
        if( bytes < 0 )
        {
            // FIXME: find a better error code
            return NvError_InvalidState;
        }

        ptr += bytes;
        length -= bytes;
    }

    return NvSuccess;
}

NvError
NvUsbDeviceRead(NvUsbDeviceHandle hDev, void *data, NvU32 length,
    NvU32 *bytesRead)
{
    struct usbdevfs_bulktransfer bulk;
    NvU8 *ptr;
    int bytes;
    int count;
    NvBool bShort;

    NV_ASSERT( hDev && hDev->ep_in );

    count = 0;

    if( bytesRead )
    {
        *bytesRead = 0;
    }

    if( hDev->remaining )
    {
        bytes = (hDev->remaining >= length) ? length : hDev->remaining;
        NvOsMemcpy( data, hDev->buffer + hDev->read, bytes );
        data = ((NvU8*)data) + bytes;
        hDev->read += bytes;
        hDev->remaining -= bytes;

        if( bytesRead )
        {
            *bytesRead += bytes;
        }

        if( hDev->remaining )
        {
            return NvSuccess;
        }

        length -= bytes;
        hDev->read = 0;
    }

    while( length )
    {
        NvU32 len = (length > 4096) ? 4096 : length;
        if(len < NVUSB_BUFFER_SIZE)
        {
            ptr = hDev->buffer;
            bShort = NV_TRUE;
            len = NVUSB_BUFFER_SIZE;
        }
        else
        {
            bShort = NV_FALSE;
            ptr = data;
        }

        bulk.ep = hDev->ep_in;
        bulk.len = len;
        bulk.data = ptr;
        bulk.timeout = hDev->timeout;

        bytes = ioctl( hDev->fd, USBDEVFS_BULK, &bulk );
        if( bytes < 0 )
        {
            NvOsDebugPrintf( "usb read error (%d): %s\n", errno,
                strerror(errno) );
            // FIXME: find a better error code
            return NvError_InvalidState;
        }

        if( bShort )
        {
            /* copy at most 'length' bytes to 'data', handle the remainder. */
            NvOsMemcpy( data, ptr, NV_MIN( bytes, (int)length ) );
            if( bytes > (int)length )
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

    if( bytesRead )
    {
        *bytesRead += count;
    }

    return NvSuccess;
}

void NvUsbDeviceClose(NvUsbDeviceHandle hDev)
{
    if( !hDev )
    {
        return;
    }

    close( hDev->fd );
}

void NvUsbDeviceAbort(NvUsbDeviceHandle hDev)
{
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
}

NvError 
NvUsbDeviceReadDevId(NvUsbDeviceHandle hDev, NvU32 *productId)
{
    if (!hDev)
    {
        return NvError_BadParameter;
    }
    else
    {
        *productId = hDev->ProductId;
        // Take the LS byte
        *productId &= 0xFF;
        return NvSuccess;
    }
}
