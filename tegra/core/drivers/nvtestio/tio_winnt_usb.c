/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_winnt.h"
#include "tio_local.h"
#include "nvos.h"
#include "nvassert.h"

#include <string.h>
#if NVOS_IS_WINDOWS
#include <io.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include "stdarg.h"

// for USB
#include <fcntl.h>
#include "nvusbhost.h"

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define USB_OPEN_MAX_RETRIES 8

// Delays to ensure proper USB operation.  Units are milliseconds.
#define NV_TIO_WAIT_BEFORE_READ_LOOP    500 // 2000
#define NV_TIO_WAIT_BEFORE_OPEN_RETRY   200
#define NV_TIO_WAIT_BEFORE_READ_RETRY   200
#define NV_TIO_WAIT_AFTER_CLOSE        1000 // 1000

#define DB_USB 0

#if DB_USB
#define NV_TIO_INITIALIZE_PRINT_GUARD InitializeCriticalSection(&printGuard);
#define NV_TIO_DELETE_PRINT_GUARD     DeleteCriticalSection(&printGuard);
static CRITICAL_SECTION printGuard;
#else
#define NV_TIO_INITIALIZE_PRINT_GUARD
#define NV_TIO_DELETE_PRINT_GUARD
#endif

#if DB_USB & 1
#define DBU1(x)    { EnterCriticalSection(&printGuard);\
                     NvTioDebugf x ;\
                     LeaveCriticalSection(&printGuard);\
}
#else
#define DBU1(x)
#endif

#if DB_USB & 2
#define DBU2(x)    { EnterCriticalSection(&printGuard);\
                     NvTioDebugf x ;\
                     LeaveCriticalSection(&printGuard);\
}
#else
#define DBU2(x)
#endif

typedef struct usbThreadInfoRec
{
    NvTioStreamHandle stream;
    NvBool listen;
} usbThreadInfo;

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

extern int gs_IsPollInit = 0;

static NvBool gs_IsUsbOpen = NV_FALSE;
static NvBool gs_IsHandleUsbRunning = NV_FALSE;

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// OpenUsb() - allocate events and return a new fd
//===========================================================================
static NvError OpenUsb(int* fd)
{
    HANDLE            readEvent;
    HANDLE            freeEvent;
    NvTioExtFd* efd;
    int lfd;

    readEvent = CreateEvent(NULL,   // default security attributes 
                            TRUE,   // manual-reset event?
                            FALSE,  // signaled?
                            NULL);

    freeEvent = CreateEvent(NULL,   // default security attributes 
                            FALSE,  // manual-reset event?
                            FALSE,  // signaled?
                            NULL);             

    if (!readEvent || !freeEvent)
    {
        CloseHandle(readEvent);
        CloseHandle(freeEvent);
        return DBERR(NvError_InsufficientMemory);
    }

    lfd = NvTioPollAdd((HANDLE)-1, 
                       NvTioFdType_USB,
                       readEvent,
                       freeEvent);

    if (lfd<0)
    {
        CloseHandle(readEvent);
        CloseHandle(freeEvent);
        return DBERR(NvError_InsufficientMemory);
    }

    efd = NvTioPollGetDescriptor(lfd);
    InitializeCriticalSection(&efd->readBuf.criticalSection);
    NV_TIO_INITIALIZE_PRINT_GUARD;

    *fd = lfd;

    return NvSuccess;
}

//===========================================================================
// CloseUsb() - deallocate events, close reading thread and free efd
//===========================================================================
static void CloseUsb(NvTioExtFd* efd)
{
    DBU1(("CloseUsb: closing usb\n"));
    if (efd)
    {
        if (efd->readThread)
        {
            int result;

            efd->isReadThreadExit = 1;

            // signal buffer free event
            SetEvent(efd->signalReadEvent);

            // Force any pending transaction to abort.
            if ((int)efd->fd >= 0)
                NvUsbDeviceAbort(efd->fd);

            DBU1(("NvTioNtUsbClose: waiting for thread...\n"));
            result = WaitForSingleObject(efd->readThread, INFINITE);
            if (result == WAIT_FAILED)
                return;
            DBU1(("NvTioNtUsbClose: wait for thread OK\n"));
            efd->readThread = NULL;
        }

        NvTioPollRemove(efd->fd);
        if ((int)efd->fd >= 0)
            NvUsbDeviceClose(efd->fd);

        NV_TIO_DELETE_PRINT_GUARD;
        DeleteCriticalSection(&efd->readBuf.criticalSection);
        CloseHandle(efd->signalReadEvent);
        CloseHandle(efd->waitReadEvent);
    }
}

//===========================================================================
// OpenUsbDevice() - make a single attempt to open usb device, loop over all
//                   instances
//===========================================================================
static NvError OpenUsbDevice(NvTioExtFd* efd)
{
    NvError e;
    NvU32 MAX_USB_INSTANCES = 0xFF;     // Maximum USB device to be queried
    NvU32 instance = 0;
    NvUsbDeviceHandle usbh;

    for (instance = 0; instance<MAX_USB_INSTANCES; instance++)
    {
        e = NvUsbDeviceOpen(instance, &usbh);
        if (e == NvError_KernelDriverNotFound)
            return DBERR(e);
        if (!e)
            break;
    }

    if (e)
        return DBERR(e);

    efd->fd = usbh;

    // set zero-length packet termination policy
    e = NvUsbDeviceSetReadPolicy(usbh,
                                 NvUsbPipePolicy_ShortPacketTerminate,
                                 TRUE);
    if (e)
        return DBERR(e);

    e = NvUsbDeviceSetWritePolicy(usbh,
                                  NvUsbPipePolicy_ShortPacketTerminate,
                                  TRUE);
    if (e)
        return DBERR(e);

    return NvSuccess;
}

//===========================================================================
// NvTioHandleUsb()
//===========================================================================
static NvU32 WINAPI HandleUsb(usbThreadInfo* infoIn)
{
    NvError e = NvSuccess;
    int result;

    usbThreadInfo info = *infoIn;
    NvTioExtFd* efd = NvTioPollGetDescriptor(info.stream->f.fd);

    DBU1(("NvTioHandleUsb: starting\n"));

    if (gs_IsHandleUsbRunning)
        NV_ASSERT(!"HandleUsb thread still running?");

    gs_IsHandleUsbRunning = NV_TRUE;

    if (!efd)
    {
        e = NvError_NotInitialized;
        goto exit;
    }

    // signal that infoIn has been copied and the caller's copy can be destroyed
    result = SetEvent(efd->waitReadEvent);
    if (result == 0)
    {
        e = NvError_FileOperationFailed;
        goto exit;
    }

    // wait till NvTioNtUsbListen resets efd->waitReadEvent
    DBU1(("NvTioHandleUsb: waiting for waitReadEvent reset\n"));
    result = WaitForSingleObject(efd->signalReadEvent, INFINITE);
    if (result == WAIT_FAILED)
    {
        e = NvError_FileOperationFailed;
        goto exit;
    }

    if (info.listen)
    {
        // Loop here w/a delay to try to get the USB to connect
        while (1)
        {
            DBU1(("NvTioHandleUsb: trying to open USB device\n"));
            e = OpenUsbDevice(efd);
            if (e == NvSuccess)
                break;
            if (efd->isReadThreadExit)
                goto exit;

            NvOsSleepMS(NV_TIO_WAIT_BEFORE_OPEN_RETRY);
        }

        // device opened, signal poll to allow accept
        DBU1(("NvTioHandleUsb: signaling poll event %x\n", efd->waitReadEvent));
        result = SetEvent(efd->waitReadEvent);
        if (result == 0)
        {
            e = NvError_FileOperationFailed;
            goto exit;
        }

        // wait for accept
        DBU1(("NvTioHandleUsb: waiting for accept\n"));
        result = WaitForSingleObject(efd->signalReadEvent, INFINITE);
        if (result == WAIT_FAILED)
        {
            e = NvError_FileOperationFailed;
            goto exit;
        }
    }

    // init buffer
    efd->readBuf.dataStart = 0;
    efd->readBuf.freeStart = 0;

    // Wait for 2s to give Windows time to complete device configuration.
    NvOsSleepMS(NV_TIO_WAIT_BEFORE_READ_LOOP);

    // read loop
    DBU1(("NvTioHandleUsb: entering main loop\n"));
    while(!efd->isReadThreadExit)
    {
        NvU8 msgBuff[NV_TIO_USB_MAX_SEGMENT_SIZE];
        NvTioRBuf* readBuf = &efd->readBuf;

        NvU8* src;
        NvU32 curDataStart;
        NvU32 curFreeStart;
        NvU32 freeSpace;
        NvU32 payload = 0;
        NvU32 bytesToCopy;
        NvU16 bytes2Receive;

        bytes2Receive = NV_TIO_USB_MAX_SEGMENT_SIZE;
        src = msgBuff;

        NvOsMemset(&msgBuff, 0xde, sizeof(msgBuff));
        DBU1(("NvTioHandleUsb: read message...\n"));
        e = NvUsbDeviceRead(efd->fd,
                            &msgBuff,
                            bytes2Receive,
                            &payload);

        if (efd->isReadThreadExit)
            break;

        if (e)
        {
            DBU1(("NvTioHandleUsb: ... read message failed, err=%d\n", e));
            NvOsSleepMS(NV_TIO_WAIT_BEFORE_READ_RETRY);
            // TODO: set error state?
            continue;
        }

        DBU1(("NvTioHandleUsb: ... requested %d, received %d\n",
              bytes2Receive,
              payload));

        // copy message payload
        curFreeStart = readBuf->freeStart;
        for (;;)
        {
            curDataStart = readBuf->dataStart;
            freeSpace = curDataStart<=curFreeStart
                ? curDataStart+NV_TIO_MAX_READ_BUFFER_LEN-curFreeStart
                : curDataStart-curFreeStart;

            if (payload <= freeSpace)
                break;

            // wait till there is space in buffer
            result = WaitForSingleObject(efd->signalReadEvent, INFINITE);
            if (result == WAIT_FAILED)
            {
                e = NvError_FileOperationFailed;
                goto exit;
            }
            if (efd->isReadThreadExit)
                goto exit;
        }

        bytesToCopy = payload;
        if (curFreeStart+bytesToCopy>NV_TIO_MAX_READ_BUFFER_LEN)
            bytesToCopy = NV_TIO_MAX_READ_BUFFER_LEN-curFreeStart;
        NvOsMemcpy(&readBuf->buffer[curFreeStart], src, bytesToCopy);

        src += bytesToCopy;
        freeSpace -= bytesToCopy;
        payload -= bytesToCopy;
        curFreeStart += bytesToCopy;
        if (curFreeStart==NV_TIO_MAX_READ_BUFFER_LEN)
            curFreeStart = 0;
        
        if (freeSpace && payload)
        {
            // the free space wraps, copy the rest
            bytesToCopy = NV_MIN(freeSpace, payload);
            NvOsMemcpy(&readBuf->buffer[curFreeStart], src, bytesToCopy);

            src += bytesToCopy;
            freeSpace -= bytesToCopy;
            payload -= bytesToCopy;
            curFreeStart += bytesToCopy;
        }

        // update of freeStart and signaling must be coupled together
        EnterCriticalSection(&efd->readBuf.criticalSection);
        readBuf->freeStart = curFreeStart;
        result = SetEvent(efd->waitReadEvent);
        LeaveCriticalSection(&efd->readBuf.criticalSection);
        if (result == 0)
        {
            e = NvError_FileOperationFailed;
            goto exit;
        }
    }


exit:
    DBU1(("NvTioHandleUsb: exiting\n"));
    gs_IsHandleUsbRunning = NV_FALSE;
    return (e==NvSuccess) ? e : DBERR(e);
}

//===========================================================================
// NvTioNtUsbCheckPath() - check filename to see if it is a USB port
//===========================================================================
static NvError NvTioNtUsbCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a USB port
    //
    if (!strncmp(path, "usb:", 5))
        return NvSuccess;
    if (!strncmp(path, "debug:", 6))
        return NvSuccess;
    return NvError_BadValue;
}

//===========================================================================
// NvTioNtUsbOpen() - open a USB port
//===========================================================================
static NvError NvTioNtUsbOpen(
                               const char *path,
                               NvU32 flags,
                               NvTioStreamHandle stream )
{
    int result;
    usbThreadInfo info;
    NvError       e;
    NvU32         RetryCount;
    NvTioExtFd*   efd;

    if (gs_IsUsbOpen)
        return DBERR(NvError_AlreadyAllocated);

    gs_IsUsbOpen = NV_TRUE;

    // create extended file descriptor
    e = OpenUsb(&stream->f.fd);
    if (e)
    {
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(e);
    }

    efd = NvTioPollGetDescriptor(stream->f.fd);
    NV_ASSERT(efd);

    // Loop here a few times w/a delay to try to get the USB to connect
    for (RetryCount = 0; RetryCount < USB_OPEN_MAX_RETRIES; RetryCount++)
    {
        e = OpenUsbDevice(efd);
        if (e == NvSuccess)
            break;

        NvOsSleepMS(NV_TIO_WAIT_BEFORE_OPEN_RETRY);
    }

    if (e)
    {
        CloseUsb(efd);
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(e);
    }

    info.stream = stream;
    info.listen = NV_FALSE;     // thread used only for asynchronous read
    efd->isReadThreadExit = 0;
    efd->readThread = CreateThread(NULL,
                                   0,
                                   (LPTHREAD_START_ROUTINE)HandleUsb,
                                   &info,
                                   0,
                                   NULL);

    if (efd->readThread == NULL)
    {
        CloseUsb(efd);
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(NvError_FileOperationFailed);
    }

    // wait till created thread makes his own local copy of "info" data
    result = WaitForSingleObject(efd->waitReadEvent, INFINITE);
    ResetEvent(efd->waitReadEvent);
    if (result == WAIT_FAILED)
    {
        CloseUsb(efd);
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(NvError_FileOperationFailed);
    }

    // copy is made, release waiting thread
    SetEvent(efd->signalReadEvent);

    return NvSuccess;
}

//===========================================================================
// NvTioNtUsbAccept() - accept USB connection (accept if any chars rcvd)
//===========================================================================
static NvError NvTioNtUsbAccept(
                                 NvTioStreamHandle stream_listen,
                                 NvTioStreamHandle stream_connect,
                                 NvU32 timeoutMS)
{
    int result;
    NvTioExtFd* efd = NvTioPollGetDescriptor(stream_listen->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    DBU1(("NvTioNtUsbAccept: waiting for poll event %x\n", efd->waitReadEvent));
    result = WaitForSingleObject(efd->waitReadEvent, timeoutMS);

    if (result == WAIT_TIMEOUT)
        return DBERR(NvError_Timeout);
    if (result == WAIT_FAILED)
        return DBERR(NvError_FileOperationFailed);
    ResetEvent(efd->waitReadEvent);

    stream_connect->f.fd = stream_listen->f.fd;
    stream_connect->ops  = stream_listen->ops;
    stream_listen->f.fd  = -1;
    stream_listen->ops   = NvTioGetNullOps();

    /* Signal NvTioHandleUsb that it can resume */
    SetEvent(efd->signalReadEvent);

    NV_TIO_DBO(stream_connect->f.fd,"USB-accept:");
    DBU1(("NvTioNtUsbAccept: exiting\n"));
    return NvSuccess;
}

//===========================================================================
// NvTioNtUsbListen() - listen for USB connection
//===========================================================================
static NvError NvTioNtUsbListen(
                                 const char *addr,
                                 NvTioStreamHandle stream)
{
    int result;
    usbThreadInfo info;
    NvTioExtFd* efd;
    NvError e = NvSuccess;

    if (gs_IsUsbOpen)
        return DBERR(NvError_AlreadyAllocated);

    gs_IsUsbOpen = NV_TRUE;

    // create extended file descriptor
    e = OpenUsb(&stream->f.fd);
    if (e)
    {
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(e);
    }

    efd = NvTioPollGetDescriptor(stream->f.fd);
    NV_ASSERT(efd);

    info.stream = stream;
    info.listen = NV_TRUE;  // thread used both for listen and asynchronous read
    efd->isReadThreadExit = 0;
    efd->readThread = CreateThread(NULL,
                                   0,
                                   (LPTHREAD_START_ROUTINE)HandleUsb,
                                   &info,
                                   0,
                                   NULL);

    if (efd->readThread == NULL)
    {
        CloseUsb(efd);
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(NvError_FileOperationFailed);
    }

    // wait till created thread makes his own local copy of "info" data
    result = WaitForSingleObject(efd->waitReadEvent, INFINITE);
    ResetEvent(efd->waitReadEvent);
    if (result == WAIT_FAILED)
    {
        CloseUsb(efd);
        gs_IsUsbOpen = NV_FALSE;
        return DBERR(NvError_FileOperationFailed);
    }

    // copy is made, release waiting thread
    SetEvent(efd->signalReadEvent);

    return NvSuccess;
}

//===========================================================================
// NvTioNtUsbRead() - read USB on winnt
//===========================================================================
static NvError NvTioNtUsbRead(NvTioStreamHandle stream,
                              void *ptr,
                              size_t size,
                              size_t *bytes,
                              NvU32 timeout_msec)
{
    int result;
    int timeout;

    NvU8* dst = (NvU8*)ptr;
    NvU32 curDataStart;
    NvU32 curFreeStart;
    NvU32 inReadBuffer;
    NvTioRBuf* readBuf;

    NvTioExtFd* efd = NvTioPollGetDescriptor(stream->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    if (!size)
    {
        *bytes = 0;
        return NvSuccess;
    }

    timeout = (timeout_msec==NV_WAIT_INFINITE) ? INFINITE : (int)timeout_msec;

    DBU1(("NvTioNtUsbRead: request to read %d bytes, timeout=%dms, "
         "time now=%dms\n",
         size, timeout, GetTickCount()));

    readBuf = &efd->readBuf;
    curDataStart = readBuf->dataStart;
    curFreeStart = readBuf->freeStart;
    inReadBuffer = curDataStart<=curFreeStart
        ? curFreeStart-curDataStart
        : curFreeStart+NV_TIO_MAX_READ_BUFFER_LEN-curDataStart;

    // if the buffer is empty, wait:
    //  - if NvTioNtUsbRead was called 'directly', wait may timeout
    //  - if NvTioNtUsbRead was called after successful poll, wait will
    //    return immediately
    if (!inReadBuffer)
    {
        result = WaitForSingleObject(efd->waitReadEvent, timeout);
        DBU1(("NvTioNtUsbRead: WaitForSingleObject returned\n"));
        if (result == WAIT_TIMEOUT)
        {   
            DBU1(("   ... with timeout\n"));
            return DBERR(NvError_Timeout);
        }
        if (result == WAIT_FAILED)
        {
            DBU1(("   ... and failed\n"));
            return DBERR(NvError_FileReadFailed);
        }
        DBU1(("NvTioNtUsbRead: WaitForSingleObject OK\n"));

        curFreeStart = readBuf->freeStart;
        inReadBuffer = curDataStart<=curFreeStart
            ? curFreeStart-curDataStart
            : curFreeStart+NV_TIO_MAX_READ_BUFFER_LEN-curDataStart;
    }

    // copy data to destination
    {
        NvU32 bytesToCopy = NV_MIN(size, inReadBuffer);
        NvU32 n = NV_MIN(bytesToCopy,
                         NV_TIO_MAX_READ_BUFFER_LEN-curDataStart);

        *bytes = bytesToCopy;

        NvOsMemcpy(dst, &readBuf->buffer[curDataStart], n);
        dst += n;
        bytesToCopy -= n;
        curDataStart += n;
        if (curDataStart==NV_TIO_MAX_READ_BUFFER_LEN)
            curDataStart = 0;

        if (bytesToCopy)
        {
            // the data wrap, copy the rest
            NvOsMemcpy(dst, &readBuf->buffer[0], bytesToCopy);
            curDataStart += bytesToCopy;
        }

        // update of dataStart and signaling must be coupled together
        EnterCriticalSection(&readBuf->criticalSection);
        curFreeStart = readBuf->freeStart;
        readBuf->dataStart = curDataStart;
        if (curDataStart==curFreeStart)
            ResetEvent(efd->waitReadEvent);     // buffer is empty
        SetEvent(efd->signalReadEvent);
        LeaveCriticalSection(&readBuf->criticalSection);
    }

    NV_TIO_DBR(stream->f.fd, ptr, *bytes);
    DBU1(("NvTioNtUsbRead: %d bytes returned\n", *bytes));
    return NvSuccess;
}

//===========================================================================
// NvTioNtUsbWrite() - write to USB on winnt
//===========================================================================
static NvError NvTioNtUsbWrite(
                                NvTioStreamHandle stream,
                                const void *ptr,
                                size_t size )
{
    NvError err;
    NvTioExtFd* efd;
    NvU8* src = (NvU8*)ptr;
    NvU8 msgBuff[NV_TIO_USB_MAX_SEGMENT_SIZE];
    NvU8 isSmallSegment = NV_TRUE;

    DBU1(("NvTioNtUsbWrite: request to write %d bytes\n", size));
  
    efd = NvTioPollGetDescriptor(stream->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    NV_TIO_DBW((int)(efd->fd),ptr,size);

    while (size)
    {
        NvU16 payload;
        NvU32 bytes2Send = NV_MIN(size, NV_TIO_USB_MAX_SEGMENT_SIZE);
        (void)isSmallSegment;
        payload = bytes2Send;

        NvOsMemcpy(msgBuff, src, bytes2Send);

        DBU1(("NvTioNtUsbWrite: writing payload\n"));

        err = NvUsbDeviceWrite(efd->fd, &msgBuff, bytes2Send);
        if (err)
        {
            return DBERR(err);
        }

        src += payload;
        size -= payload;
    }

    return NvSuccess;
}

//===========================================================================
// NvTioNtStreamClose() - close extended file descriptor
//===========================================================================
static void NvTioNtUsbClose(NvTioStreamHandle stream)
{
    NvTioExtFd* efd = NvTioPollGetDescriptor(stream->f.fd);
    DBU1(("NvTioNtUsbClose: closing usb\n"));

    if (efd)
    {
        CloseUsb(efd);

        // Wait for 1s to provide time for the target to drop its connection.
        NvOsSleepMS(NV_TIO_WAIT_AFTER_CLOSE);
    }
    gs_IsUsbOpen = NV_FALSE;
}

//===========================================================================
// NvTioRegisterUsb()
//===========================================================================
void NvTioRegisterUsb(void) 
{
    static NvTioStreamOps USB_ops = {0};

    if (!USB_ops.sopName) {
        USB_ops.sopName        = "winnt_usb_ops",
        USB_ops.sopCheckPath   = NvTioNtUsbCheckPath,
        USB_ops.sopFopen       = NvTioNtUsbOpen,
        USB_ops.sopListen      = NvTioNtUsbListen,
        USB_ops.sopAccept      = NvTioNtUsbAccept,
        USB_ops.sopClose       = NvTioNtUsbClose,
        USB_ops.sopFwrite      = NvTioNtUsbWrite,
        USB_ops.sopFread       = NvTioNtUsbRead,
        USB_ops.sopPollPrep    = NvTioNtStreamPollPrep,
        USB_ops.sopPollCheck   = NvTioNtStreamPollCheck,

        NvTioRegisterOps(&USB_ops);
    }
}
