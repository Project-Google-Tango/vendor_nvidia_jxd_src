/*
 * Copyright (c) 2007 -- 2013 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"
#include "nvos.h"
#include "nvassert.h"
#include "nverror.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdio.h>
#include "nvusbhost.h"

void *NvUsbHandler(void *);

//###########################################################################
//############################### TYPE DEFINES ##############################
//###########################################################################
typedef struct NvUsbDeviceRec
{
    int ep_in;
    int ep_out;
    int iface_number;
    int fd;

    NvU8 buffer[64];
    NvU32 remaining;
    NvU32 read;
    NvU32 ProductId;
    NvU32 timeout;
} NvUsbDevice;

#define USBDEVFS_URB_SHORT_NOT_OK   0x01
#define USBDEVFS_URB_ISO_ASAP       0x02
#define USBDEVFS_URB_BULK_CONTINUATION  0x04
#define USBDEVFS_URB_NO_FSBR        0x20
#define USBDEVFS_URB_ZERO_PACKET    0x40
#define USBDEVFS_URB_NO_INTERRUPT   0x80

#define USBDEVFS_URB_TYPE_ISO          0
#define USBDEVFS_URB_TYPE_INTERRUPT    1
#define USBDEVFS_URB_TYPE_CONTROL      2
#define USBDEVFS_URB_TYPE_BULK         3

#define USBDEVFS_SUBMITURB         _IOR('U', 10, struct usbdevfs_urb)
#define USBDEVFS_REAPURB           _IOW('U', 12, void *)
#define USBDEVFS_REAPURBNDELAY     _IOW('U', 13, void *)

//###########################################################################
//############################### STRUCTS ###################################
//###########################################################################
typedef struct NvUsbHandlerArgsRec
{
    NvUsbDeviceHandle hDev;
    int               pipeFd;
} NvUsbHandlerArgs;

struct usbdevfs_iso_packet_desc {
    unsigned int length;
    unsigned int actual_length;
    unsigned int status;
};

struct usbdevfs_urb {
    unsigned char type;
    unsigned char endpoint;
    int status;
    unsigned int flags;
    void *buffer;
    int buffer_length;
    int actual_length;
    int start_frame;
    int number_of_packets;
    int error_count;
    unsigned int signr; /* signal to be sent on completion,
                  or 0 if none should be sent. */
    void *usercontext;
    struct usbdevfs_iso_packet_desc iso_frame_desc[0];
};

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static NvBool gs_IsUsbOpen = NV_FALSE;
static int    pipeFd[2];
static NvUsbDeviceHandle gs_hUsbDev = 0;
static NvOsThreadHandle gs_NvUsbThread;

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioLinuxUsbCheckPath() - check filename to see if it is a USB port
//===========================================================================
static NvError NvTioLinuxUsbCheckPath(const char *path)
{
    if (!NvOsStrncmp(path, "usb:", 5))
        return NvSuccess;
    if (!NvOsStrncmp(path, "debug:", 6))
        return NvSuccess;
    return NvError_BadValue;
}

//===========================================================================
// OpenUsb() - open Nvidia USB device
//===========================================================================
static NvError OpenUsb(int *fd)
{
    NvUsbDeviceHandle usbh;
    NvU32             instance = 0;
    NvU32             iteration = 0;
    NvU32             MAX_OPEN_USB_ITERATIONS = 0xFF;
    NvError           e = NvSuccess;

    // NvUsbDeviceOpen will return Success immediately
    // if the first instance is found.
    // Otherwise, it will be unable to find other instances
    // and the loop is useless.
    // So change the loop to open the first device found.
    // Use the usb bus number to denote a specific DUT if there
    // are multiple DUTs.
    for (iteration = 0;  iteration < MAX_OPEN_USB_ITERATIONS; iteration++)
    {
        e = NvUsbDeviceOpen(instance, &usbh);
        if (e == NvError_KernelDriverNotFound)
            return DBERR(e);
        if (!e)
            break;
    }

    if (e)
        return DBERR(e);

    usbh->timeout = 500;
    *fd = (int)usbh;
    gs_hUsbDev = usbh;
    return e;
}

//===========================================================================
// NvTioLinuxUsbOpen() - open a USB port
//===========================================================================
static NvError NvTioLinuxUsbOpen(
                                  const char        *path,
                                  NvU32             flags,
                                  NvTioStreamHandle stream )
{
    NvError e = NvSuccess;
    NvU32   cnt;

    if (gs_IsUsbOpen)
    {
        return DBERR(NvError_AlreadyAllocated);
    }
    gs_IsUsbOpen = NV_TRUE;
    pipeFd[0] = 0;
    pipeFd[1] = 0;
    // create extended file descriptor
    for (cnt=0; cnt < 10; cnt++)
    {
        e = OpenUsb(&stream->f.fd);
        if (e)
        {
            NvOsSleepMS(500);
        }
        else
        {
            break;
        }
    }
    if (e)
    {
        gs_IsUsbOpen = NV_FALSE;
        NvOsDebugPrintf("not able to open usb\n");
        return DBERR(e);
    }

    return e;
}
//===========================================================================
// NvTioLinuxUsbClose() - close a USB connection
//===========================================================================
static void NvTioLinuxUsbClose(NvTioStreamHandle stream)
{
    if (stream->f.fd != 0)
    {
        gs_IsUsbOpen = NV_FALSE;
        NvOsThreadJoin(gs_NvUsbThread);
        if (pipeFd[0] != 0 || pipeFd[1] != 0)
        {
            close(pipeFd[0]);
            close(pipeFd[1]);
            pipeFd[0] = 0;
            pipeFd[1] = 0;
        }
        NvUsbDeviceClose(gs_hUsbDev);
        // It's needed for centos 5.
        NvOsSleepMS(1000);
    }
}

//===========================================================================
// NvTioLinuxUsbSubmitReadUrb() - submit a read URB
//===========================================================================
static NvError NvTioLinuxUsbSubmitReadUrb(NvUsbDeviceHandle hDev, void *data, NvU32 length)
{
    int bytes;
    struct usbdevfs_urb *bulk_urb;

    NV_ASSERT( hDev && hDev->ep_in );

    bulk_urb = NvOsAlloc(sizeof (struct usbdevfs_urb));
    if (bulk_urb == NULL)
        return NvError_InsufficientMemory;
    NvOsMemset(bulk_urb, 0, sizeof(struct usbdevfs_urb));

    bulk_urb->type = USBDEVFS_URB_TYPE_BULK;
    bulk_urb->endpoint = hDev->ep_in;
    /* bulk_urb->status will be updated when transfer is done */
    if (length == 0)
    {
        bulk_urb->flags = USBDEVFS_URB_ZERO_PACKET;
    }
    bulk_urb->buffer = data;
    bulk_urb->buffer_length = length;
    /* bulk_urb->actual_length will be updated when transfer is done */
    /* bulk_urb->start_frame is only relevant for iso/irq transfer */
    /* bulk_urb->number_of_packages is iso only */
    /* bulk_urb->error_count is updated when transfer is done, iso only */
    /* bulk_urb->signr : no signal will be sent, so set to 0 */
    bulk_urb->usercontext = hDev;
    /* bulk_urb->iso_frame_desc: irrelevant since this is bulk transfer */
    bytes = ioctl(hDev->fd, USBDEVFS_SUBMITURB, bulk_urb);

    if (bytes < 0)
    {
        NvOsFree(bulk_urb);
        if (errno == ENODEV)
        {
            return NvError_DeviceNotFound;
        }
        else
        {
            return NvError_IoctlFailed;
        }
    }

    return NvSuccess;
}

//===========================================================================
// NvTioLinuxUsbReapUrb() - blocking on a URB until it returns
//===========================================================================
static NvError NvTioLinuxUsbReapUrb(NvUsbDeviceHandle hDev, NvU32 *length)
{
    int e;
    struct usbdevfs_urb *urb;

    e = ioctl(hDev->fd, USBDEVFS_REAPURB, &urb);
    if (e < 0 && errno == ENODEV)
    {
        return DBERR(NvError_DeviceNotFound);
    }
    else if (e < 0)
    {
        return DBERR(NvError_IoctlFailed);
    }

    if (urb->type != USBDEVFS_URB_TYPE_BULK)
    {
        return DBERR(NvError_UsbInvalidEndpoint);
    }
    if (urb->status != 0)
    {
        return DBERR(NvError_UsbfTxfrFail);
    }
    if (urb->actual_length)
    {
        *length = urb->actual_length;
    }

    NvOsFree(urb);
    return NvSuccess;
}

//===========================================================================
// NvUsbHandler() - thread function reading data from USB device
//===========================================================================
void *NvUsbHandler(void *arg)
{
    NvUsbHandlerArgs *hArgs = (NvUsbHandlerArgs *)arg;
    NvUsbDeviceHandle hDev;
    int               fileId;
    NvU32             bytesRead;
    static NvU8       data[NV_TIO_USB_MAX_SEGMENT_SIZE];
    NvError           e;

    fileId = hArgs->pipeFd;
    hDev = hArgs->hDev;
    while (gs_IsUsbOpen)
    {
        e = NvTioLinuxUsbSubmitReadUrb(hDev, data, NV_TIO_USB_MAX_SEGMENT_SIZE);
        if (e == NvError_DeviceNotFound)
        {
            break;
        }
        else if (e != NvSuccess)
        {
            continue;
        }
        bytesRead = 0;
        e = NvTioLinuxUsbReapUrb(hDev, &bytesRead);
        if (e == NvSuccess)
        {
            if (write(fileId, data, bytesRead) == -1)
            {
                NvOsDebugPrintf("write failure, error code: %x\n", errno);
                break;
            }
        }

        if (e != NvSuccess)
        {
            break;
        }
    }
    return NULL;
}

//===========================================================================
// NvTioLinuxUsbListen() - listen on the usb file descriptor
//===========================================================================
static NvError NvTioLinuxUsbListen(
                                    const char *addr,
                                    NvTioStreamHandle stream )
{
    NvError e = NvSuccess;
    static NvUsbHandlerArgs args;

    e = NvTioLinuxUsbOpen(addr, 0, stream);
    if (e)
    {
        NvOsDebugPrintf("error when opening up usb\n");
        goto fail;
    }
    if (pipe(pipeFd) != 0)
    {
        NvTioLinuxUsbClose(stream);
        return NvError_PipeNotConnected;
    }
    args.pipeFd = pipeFd[1];
    args.hDev = (NvUsbDeviceHandle)stream->f.fd;
    e = NvOsThreadCreate((NvOsThreadFunction)NvUsbHandler, (void *)&args,  &gs_NvUsbThread);
    if (e != NvSuccess)
    {
        NvTioLinuxUsbClose(stream);
        return NvError_FileOperationFailed;
    }
fail:
    return e;
}

//===========================================================================
// NvTioLinuxUsbAccept() - accept a USB connection from target
//===========================================================================
static NvError NvTioLinuxUsbAccept(
                                    NvTioStreamHandle stream_listen,
                                    NvTioStreamHandle stream_connect,
                                    NvU32             timeoutMS)
{
    NvError e = NvSuccess;
    stream_connect->f.fd = stream_listen->f.fd;
    stream_connect->ops = stream_listen->ops;
    stream_listen->f.fd = 0;
    stream_listen->ops = NvTioGetNullOps();
    return e;
}

//===========================================================================
// NvTioLinuxUsbWrite() - write to a USB stream
//===========================================================================
static NvError NvTioLinuxUsbWrite(
                                   NvTioStreamHandle stream,
                                   const void        *ptr,
                                   size_t            size)
{
    NvError e =  NvSuccess;
    NvUsbDeviceHandle hDev = (NvUsbDeviceHandle)stream->f.fd;
    e = NvUsbDeviceWrite(hDev, ptr, size);
    return e;
}

//===========================================================================
// NvTioLinuxUsbRead() - read from a USB stream
//===========================================================================
static NvError NvTioLinuxUsbRead(
                                  NvTioStreamHandle stream,
                                  void              *ptr,
                                  size_t            size,
                                  size_t            *bytes,
                                  NvU32             timeout_msec)
{
    NvError e =  NvSuccess;
    struct pollfd fds[1] = { {pipeFd[0], POLLIN, 0} };

    if (timeout_msec == 0)
    {
        int ret;
        ret = poll(fds, 1, 0);
        if (ret == 0)
        {
            return NvError_Timeout;
        }
    }
    *bytes = read(pipeFd[0], ptr, size);
    if (*bytes == -1)
    {
        e = NvError_FileOperationFailed;
    }
    return e;
}

//===========================================================================
// NvTioLinuxUsbPollPrep() -
//===========================================================================
static NvError NvTioLinuxUsbPollPrep(
                                         NvTioPollDesc *tioDesc,
                                         void          *osDesc )
{
    NvError e = NvSuccess;
    struct pollfd *linuxDesc = osDesc;
    linuxDesc->fd = pipeFd[0];
    return e;
}

//===========================================================================
// NvTioLinuxUsbPollCheck() -
//===========================================================================
static NvError NvTioLinuxUsbPollCheck(
                                         NvTioPollDesc *tioDesc,
                                         void          *osDesc )
{
    struct pollfd *linuxDesc = osDesc;
    if (linuxDesc->fd != -1 && (linuxDesc->revents & POLLIN))
    {
        tioDesc->returnedEvents = NvTrnPollEvent_In;
    }
    return NvSuccess;
}

//===========================================================================
// NvTioRegisterUsb() -
//===========================================================================
void NvTioRegisterUsb(void)
{
    static NvTioStreamOps USB_ops = {0};

    if (!USB_ops.sopName) {
        USB_ops.sopName         = "linux_usb_ops";
        USB_ops.sopCheckPath    = NvTioLinuxUsbCheckPath;
        USB_ops.sopFopen        = NvTioLinuxUsbOpen;
        USB_ops.sopListen       = NvTioLinuxUsbListen;
        USB_ops.sopAccept       = NvTioLinuxUsbAccept;
        USB_ops.sopClose        = NvTioLinuxUsbClose;
        USB_ops.sopFwrite       = NvTioLinuxUsbWrite;
        USB_ops.sopFread        = NvTioLinuxUsbRead;
        USB_ops.sopPollPrep     = NvTioLinuxUsbPollPrep;
        USB_ops.sopPollCheck    = NvTioLinuxUsbPollCheck;

        NvTioRegisterOps(&USB_ops);
    }
}
