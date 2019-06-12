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

#include "tio_local.h"
#include "tio_winnt.h"
#include "nvos.h"
#include "nvassert.h"

#include <string.h>
#include <io.h>
#include <winsock2.h>

#include <stdlib.h>
#include <errno.h>
#include "stdarg.h"

// for stdio
#include <stdio.h>

// for serial port
#include <fcntl.h>
//   TODO: Fix this.  For now, temporarily disable a warning 
// that makes the Visual Studio 2005 and/or VS2008 compiler unhappy.  
#ifdef _WIN32
//#pragma warning( disable : 4996 )
#endif

// Debug of serial code
#if 0
#define DBSU(x)     NvTioDebugf x
#else
#define DBSU(x)
#endif
#if 0
#define DBS(x)      NvTioDebugf x
#else
#define DBS(x)
#endif

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#define CONSOLE_BUFFER_SIZE     1024

#define EVENTLIST_MAX           32
#define EXTFD_MAX               32

typedef NvU32 socklen_t;
typedef NvU32 nfds_t;

typedef struct consoleThreadInfoRec
{
    char* buffer;
    int   bufferSize;
    int*  bufferRead;
    HANDLE stdinput;
    HANDLE consoleFreeEvent;
    HANDLE consoleReadyEvent;
} consoleThreadInfo;

//###########################################################################
//############################### GLOBALS ###################################
//###########################################################################

static HANDLE consoleThread = NULL;
static char   consoleBuffer[CONSOLE_BUFFER_SIZE];
static int    consoleBufferPos = 0;

NvTioExtFd fdmap[EXTFD_MAX] = {{ 0 }};

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioPollAdd() - Create a new extended file descriptor
//===========================================================================
int NvTioPollAdd(HANDLE fd,
                        NvTioFdType type,
                        HANDLE readEvent,
                        HANDLE signalEvent)
{
    int i;
    for(i=0; i<EXTFD_MAX; i++)
    {
        if (fdmap[i].fd == NULL)
        {
            fdmap[i].fd = fd;
            fdmap[i].type = type;
            fdmap[i].waitReadEvent = readEvent;
            fdmap[i].signalReadEvent = signalEvent;
            NvOsMemset(&fdmap[i].ovlr, 0, sizeof(OVERLAPPED));
            fdmap[i].ovlr.hEvent = readEvent;
            return i;
        }
    }
    return -1;
}

//===========================================================================
// NvTioPollRemove() - destroy extended file descriptor
//===========================================================================
NvError NvTioPollRemove(HANDLE fd)
{
    int i;
    for(i=0; i<EXTFD_MAX; i++)
    {
        if (fdmap[i].fd == fd)
        {
            fdmap[i].fd = NULL;
            return NvSuccess;
        }
    }
    return NvError_Force32;
}

//===========================================================================
// NvTioPollGetDescriptor() - look up file descriptor
//===========================================================================
NvTioExtFd* NvTioPollGetDescriptor(int fd)
{
    return ((fd>=0)&&(fd<EXTFD_MAX)) ? &fdmap[fd] : NULL;
}

//===========================================================================
// NvTioNtSynWriteFile() - emulation of synchronous WriteFile for handles
//                         with overlapped attribute set
//===========================================================================
static NvError NvTioNtSynWriteFile(
                                   HANDLE fd,
                                   const void* buffer,
                                   NvU32 toWrite,
                                   NvU32* written)
{
    NvU32 foo;
    int result;
    OVERLAPPED ovl;

    memset(&ovl,0,sizeof(ovl));
    ovl.hEvent = CreateEvent(0,TRUE,0,0);
    if (!ovl.hEvent)
        return DBERR(NvError_InsufficientMemory);

    DBSU(("NvTioNtSynWriteFile: request to write %d bytes\n", toWrite));
    if (!WriteFile(fd, buffer, toWrite, NULL, &ovl))
    {
        result = GetLastError();
        if (result!=ERROR_IO_PENDING)
        {
            CloseHandle(ovl.hEvent);
            return DBERR(NvError_FileWriteFailed);
        }
    }

    if (!written)
        written = &foo;

    // Block execution to emulate synchronous write
    DBS(("NvTioNtSynWriteFile: calling GetOverlappedResult\n"));
    result = GetOverlappedResult(fd, &ovl, written, TRUE);
    if (!result)
    {
        CloseHandle(ovl.hEvent);
        return DBERR(NvError_FileWriteFailed);
    }

    DBS(("NvTioNtSynWriteFile: written %d bytes\n", *written));
    CloseHandle(ovl.hEvent);
    return NvSuccess;
}

//===========================================================================
// NvTioTcpInit() - initialize Winsock stack
//===========================================================================
static void NvTioTcpInit(void)
{
    int err;
    WSADATA wsaData;

    err = WSAStartup( MAKEWORD( 2, 1 ), &wsaData );
    if (err!=0)
    {
        DB(("Could not find a usable WinSock DLL!"));
        abort();
    }
}

//===========================================================================
// NvTioHandleConsole() - blocking cooked read from console, runs in
//                        a thread on its own
//===========================================================================
static NvU32 WINAPI NvTioHandleConsole(consoleThreadInfo* infoIn)
{
    consoleThreadInfo info = *infoIn;
    int err;
    int result;

    // signal that infoIn has been copied and the caller's copy can be destroyed
    result = SetEvent(info.consoleReadyEvent);
    if (result == 0)
    {
        err = GetLastError();
        return err;
    }

    result = SetConsoleMode(info.stdinput,
                            ENABLE_ECHO_INPUT |            // echo
                            ENABLE_LINE_INPUT |            // wait for EOL
                            ENABLE_PROCESSED_INPUT);       // cooked mode
    if (result == 0)
    {
        err = GetLastError();
        return err;
    }

    FlushConsoleInputBuffer(info.stdinput);

    while(1)
    {
        result = WaitForSingleObject(info.consoleFreeEvent, INFINITE);
        if (result == WAIT_FAILED)
        {
            err = GetLastError();
            return err;
        }

        result = ReadFile(info.stdinput,
                          info.buffer,
                          info.bufferSize-1,
                          info.bufferRead,
                          NULL);
        if (result == 0)
        {
            err = GetLastError();
            return err;
        }

        info.buffer[*info.bufferRead] = 0;
        result = SetEvent(info.consoleReadyEvent);
        if (result == 0)
        {
            err = GetLastError();
            return err;
        }
    }
    return 0;
}

//===========================================================================
// NvTioNtStdioOpen() - open a stdio port
//===========================================================================
static NvError NvTioNtStdioOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    int fd;
    HANDLE readEvent = CreateEvent(NULL,   // default security attributes 
                                   FALSE,  // manual-reset event?
                                   FALSE,  // signaled?
                                   NULL);             

    if (!readEvent)
        return DBERR(NvError_InsufficientMemory);

    if (!strcmp(path, "stdin:")) {
        int result;
        HANDLE stdinput = GetStdHandle(STD_INPUT_HANDLE);

        // Is stdin console or a redirection from file?
        if (GetFileType(stdinput)!=FILE_TYPE_CHAR)
        {
            fd = NvTioPollAdd(stdinput, NvTioFdType_File, readEvent, NULL);
        }
        else
        {
            consoleThreadInfo info;
            HANDLE consoleFree = CreateEvent(
                                        NULL,   // default security attributes 
                                        FALSE,  // manual-reset event?
                                        TRUE,   // signaled?
                                        TEXT("ConsoleFreeEvent"));
            if (!consoleFree)
            {
                CloseHandle(readEvent);
                return DBERR(NvError_InsufficientMemory);
            }

            info.buffer = consoleBuffer;
            info.bufferSize = CONSOLE_BUFFER_SIZE;
            info.bufferRead = &consoleBufferPos;
            info.stdinput = stdinput;
            info.consoleReadyEvent = readEvent;
            info.consoleFreeEvent = consoleFree;

            consoleThread = CreateThread(NULL,
                                     0,
                                     (LPTHREAD_START_ROUTINE)NvTioHandleConsole,
                                     &info,
                                     0,
                                     NULL);

            if (consoleThread == NULL)
            {
                CloseHandle(readEvent);
                CloseHandle(consoleFree);
                return DBERR(NvError_FileOperationFailed);
            }

            result = WaitForSingleObject(readEvent, INFINITE);
            if (result == WAIT_FAILED)
            {
                CloseHandle(readEvent);
                CloseHandle(consoleFree);
                return DBERR(NvError_FileOperationFailed);
            }

            fd = NvTioPollAdd(stdinput,
                              NvTioFdType_Console,
                              readEvent,
                              consoleFree);
            if (fd<0)
                CloseHandle(consoleFree);
        }
        if (fd>=0)
        {
            NV_TIO_DBO(fd,"stdin:");
            stream->f.fd = fd;
            return NvSuccess;
        }
    }
    if (!strcmp(path, "stdout:")) {
        HANDLE stdoutput = GetStdHandle(STD_OUTPUT_HANDLE);
        fd = NvTioPollAdd(stdoutput, NvTioFdType_Console, NULL, NULL);
        if (fd>=0)
        {
            NV_TIO_DBO(fd,"stdout:");
            stream->f.fd = fd;
            return NvSuccess;
        }
    }
    if (!strcmp(path, "stderr:")) {
        HANDLE stderror = GetStdHandle(STD_ERROR_HANDLE);
        fd = NvTioPollAdd(stderror, NvTioFdType_Console, NULL, NULL);
        if (fd>=0)
        {
            NV_TIO_DBO(fd,"stderr:");
            stream->f.fd = fd;
            return NvSuccess;
        }
    }

    CloseHandle(readEvent);
    return DBERR(NvError_FileOperationFailed);
}

//===========================================================================
// NvTioNtUartCheckPath() - check filename to see if it is a uart port
//===========================================================================
static NvError NvTioNtUartCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a uart port
    //
    if (!strncmp(path, "uart:", 5))
        return NvSuccess;
    if (!strncmp(path, "debug:", 6))
        return NvSuccess;
    return NvError_BadValue;
}

//===========================================================================
// NvTioNtUartGetBaud() - get baudrate
//===========================================================================
static NvU32 NvTioNtUartGetBaud(void)
{
    NvU32 rate = NvTioGlob.uartBaud ? NvTioGlob.uartBaud : NV_TIO_DEFAULT_BAUD;
    int i;
    for (i=0; i<2; i++) {
        switch(rate) {
            case 115200:    return CBR_115200;
            case 57600:     return CBR_57600;
            case 9600:      return CBR_9600;
            default:
                rate = NV_TIO_DEFAULT_BAUD;
                break;
        }
    }
    //NV_ASSERT(!"Could not determine proper baud rate");
    return CBR_57600;
}

//===========================================================================
// NvTioNtUartOpen() - open a uart port
//===========================================================================
static NvError NvTioNtUartOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    NvError err = NvSuccess;
    int result;
    HANDLE comm;
    HANDLE readEvent;
    NvTioExtFd* efd;

    DCB dcb;
    COMMTIMEOUTS timeouts;

    // TODO support other devices
    const char *dev = NvTioGlob.uartPort ? NvTioGlob.uartPort : "\\\\.\\COM1";
    NvU32 baud = NvTioNtUartGetBaud();

    readEvent = CreateEvent(NULL,   // default security attributes 
                            TRUE,   // manual-reset event 
                            FALSE,  // signaled?
                            NULL);

    if (!readEvent)
        return DBERR(NvError_InsufficientMemory);

    comm = CreateFile( TEXT(dev),
                       GENERIC_READ | GENERIC_WRITE, 
                       0, 
                       0, 
                       OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED,
                       0);

    if (comm == INVALID_HANDLE_VALUE)
    {
        CloseHandle(readEvent);
        return DBERR(NvError_FileOperationFailed);
    }

    NvOsDebugPrintf("\nOpen Serial port %s with baud = %s\n\n",
                    dev,
                    baud==CBR_115200?"B115200":
                    baud==CBR_57600?"B57600":"????");

    stream->f.fd = NvTioPollAdd(comm,
                                NvTioFdType_Uart,
                                readEvent,
                                NULL);
    if (stream->f.fd < 0)
    {
        return DBERR(NvError_FileOperationFailed);
    }
    NV_TIO_DBO(stream->f.fd,"uart:");

    // flush the port
    result = PurgeComm(comm,
                       PURGE_RXABORT|PURGE_RXCLEAR|PURGE_TXABORT|PURGE_TXCLEAR);

    // set port params
    memset(&dcb, 0, sizeof(dcb));
    (void)GetCommState(comm, &dcb);
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;               // 8N1
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDsrSensitivity = FALSE;    // not a modem
    dcb.fAbortOnError = FALSE;
#if NV_TIO_UART_HW_FLOWCTRL
    dcb.fOutxCtsFlow = TRUE;
#endif
    dcb.EofChar = 0;                // EOF
    dcb.XonLim = 1;                 // wait for at least 1 char
    result = SetCommState(comm, &dcb);
    if (!result)
        return DBERR(NvError_FileOperationFailed);

    timeouts.ReadIntervalTimeout = 20;      // irrelevant with single char reads
    timeouts.ReadTotalTimeoutMultiplier = 0;// timeout does not depend on
                                            // the count of requested bytes!
    timeouts.ReadTotalTimeoutConstant = MAXDWORD;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    result = SetCommTimeouts(comm, &timeouts);
    if (!result)
        return DBERR(NvError_FileOperationFailed);

    // send a carriage return
    result = NvTioNtSynWriteFile(comm, "\r\n", 2, NULL);
    if (result)
    {
        err = NvError_FileOperationFailed;
        DB(("write() returned %d\n",result));
    }

    efd = &fdmap[stream->f.fd];

    efd->inReadBuffer = 0;
    efd->inReadBufferStart = efd->readBuffer;

    // start overlapped read
    result = ReadFile(efd->fd,
                      efd->readBuffer,
                      1,
                      NULL,
                      &efd->ovlr);
    DBS(("NvTioNtUartOpen: ReadFile returned\n"));
    if (!result)
    {
        if (GetLastError() != ERROR_IO_PENDING)
        {
            CancelIo(efd->fd);
            return DBERR(NvError_FileReadFailed);
        }
        DBS(("NvTioNtUartOpen: ReadFile pending...\n"));
    }

    if (err)
        return DBERR(err);
    else
        return NvSuccess;
}

//===========================================================================
// NvTioNtUartAccept() - accept uart connection (accept if any chars received)
//===========================================================================
static NvError NvTioNtUartAccept(
                    NvTioStreamHandle stream_listen,
                    NvTioStreamHandle stream_connect,
                    NvU32 timeoutMS)
{
    NvTioExtFd* efd = NvTioPollGetDescriptor(stream_listen->f.fd);

    DBS(("NvTioNtUartAccept called\n"));

    if (!efd)
        return DBERR(NvError_NotInitialized);

    DBS(("NvTioNtUartAccept: waiting for poll event %x\n", efd->waitReadEvent));
#if NV_TIO_UART_ACCEPT_WAIT_CHAR
    //
    // wait, sending carriage return every 2 seconds, until some chars are
    // received.
    //
    {
        NvU32 foo;
        while(1)
        {
            NvU32 wait = 2000;
            int result;

            wait = wait < timeoutMS ? wait : timeoutMS;
            result = WaitForSingleObject(efd->waitReadEvent, wait);
            if (result==WAIT_OBJECT_0)
                break;
            if (wait == timeoutMS)
                return NvError_Timeout;
            timeoutMS -= wait;
            NvTioNtSynWriteFile(efd->fd, "\r\n", 2, NULL);
        }
        if (!GetOverlappedResult(efd->fd, &efd->ovlr, &foo, FALSE))
            return DBERR(NvError_FileReadFailed);
    }
#endif

    stream_connect->f.fd = stream_listen->f.fd;
    stream_connect->ops  = stream_listen->ops;
    stream_listen->f.fd = -1;
    stream_listen->ops  = NvTioGetNullOps();
    NV_TIO_DBO(stream_connect->f.fd,"uart-accept:");
    return NvSuccess;
}

//===========================================================================
// NvTioNtUartListen() - listen for uart connection
//===========================================================================
static NvError NvTioNtUartListen(
                    const char *addr,
                    NvTioStreamHandle stream)
{
    return NvTioNtUartOpen(addr, NVOS_OPEN_READ|NVOS_OPEN_WRITE, stream);
}

//===========================================================================
// NvTioNtUartRead() - read uart file descriptor
//===========================================================================
static NvError NvTioNtUartRead(
                               NvTioStreamHandle stream,
                               void *ptr,
                               size_t size,
                               size_t *bytes,
                               NvU32 timeout_msec)
{
    NvU8* dst = (NvU8*)ptr;
    NvU32 totalBytesRead = 0;
    int result;
    int timeout;
    int read;
    int quit = 0;

    NvTioExtFd* efd = NvTioPollGetDescriptor(stream->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    timeout = (timeout_msec==NV_WAIT_INFINITE) ? INFINITE : (int)timeout_msec;

    DBSU(("NvTioNtUartRead: request to read %d bytes, timeout=%dms, "
         "time now=%dms\n",
          size, timeout, GetTickCount()));

    // if the buffer is empty, wait for char:
    //  - if NvTioNtUartRead was called 'directly', wait may timeout
    //  - if NvTioNtUartRead was called after successful poll, wait will
    //    return immediately
    if (!efd->inReadBuffer)
    {
        result = WaitForSingleObject(efd->ovlr.hEvent, timeout);
        DBS(("NvTioNtUartRead: WaitForSingleObject returned\n"));
        if (result == WAIT_TIMEOUT)
        {   
            DBSU(("   ... with timeout\n"));
            return DBERR(NvError_Timeout);
        }
        if (result == WAIT_FAILED)
        {
            DBS(("   ... and failed\n"));
            return DBERR(NvError_FileReadFailed);
        }
        DB(("NvTioNtUartRead: WaitForSingleObject OK\n"));

        result = GetOverlappedResult(efd->fd, &efd->ovlr, &read, TRUE);
        if (!result)
        {
            result = GetLastError();
            if (result == ERROR_IO_INCOMPLETE)
            {
                NV_ASSERT(!"ERROR_IO_INCOMPLETE should never happen here!");
            }
            return DBERR(NvError_FileReadFailed);
        }
        DBS(("NvTioNtUartRead: GetOverlappedResult OK, len=%d, %02x %c\n",
                read,
                *(efd->inReadBufferStart+efd->inReadBuffer),
                *(efd->inReadBufferStart+efd->inReadBuffer)));
        efd->inReadBuffer+=read;
        DB(("NvTioNtUartRead: efd->inReadBuffer=%d, size=%d\n", efd->inReadBuffer, size));
    }

    for (;;)
    {
        // If buffer is not empty, copy data to destination
        if (efd->inReadBuffer && size)
        {
            NvU32 bytesToCopy;

            bytesToCopy = NV_MIN(size, efd->inReadBuffer);
            NvOsMemcpy(dst, efd->inReadBufferStart, bytesToCopy);

            DBS(("NvTioNtUartRead: bytesToCopy = %d\n", bytesToCopy));

            dst += bytesToCopy;
            size -= bytesToCopy;

            efd->inReadBufferStart += bytesToCopy;
            efd->inReadBuffer -= bytesToCopy;

            totalBytesRead += bytesToCopy;
        }

        DB(("NvTioNtUartRead: totalBytesRead = %d\n", totalBytesRead));
        DB(("NvTioNtUartRead: efd->inReadBuffer = %d\n", efd->inReadBuffer));

        if (quit)
            break;

        if (size || !efd->inReadBuffer)
        {
            // read buffer was emptied, need to issue new read command
            // (very first read command is in NvTioNtUartOpen)
            efd->inReadBuffer = 0;
            efd->inReadBufferStart = efd->readBuffer;

            // read as much as we can
            for (;;)
            {
                result = ReadFile(efd->fd,
                                  efd->readBuffer+efd->inReadBuffer,
                                  1,
                                  NULL,
                                  &efd->ovlr);
                DB(("NvTioNtUartRead: ReadFile returned\n"));
                if (!result)
                {
                    if (GetLastError() != ERROR_IO_PENDING)
                    {
                        CancelIo(efd->fd);
                        return DBERR(NvError_FileReadFailed);
                    }
                    DBS(("NvTioNtUartRead: ReadFile pending...\n"));
                }

                result = GetOverlappedResult(efd->fd, &efd->ovlr, &read, FALSE);
                if (!result)
                {
                    result = GetLastError();
                    if (result != ERROR_IO_INCOMPLETE)
                        return DBERR(NvError_FileReadFailed);
                    // no more immediately available chars, return what we got
                    // the completion of pending read will be waited for either
                    // in NvTioPoll or by WaitForSingleObject at the beginning
                    // of NvTioNtUartRead
                    DBS(("NvTioNtUartRead: No more chars\n"));
                    break;
                }
                DBS(("NvTioNtUartRead: GetOverlappedResult OK, l=%d, %02x %c\n",
                        read,
                        *(efd->readBuffer+efd->inReadBuffer),
                        *(efd->readBuffer+efd->inReadBuffer)));
                efd->inReadBuffer+=read;
                DB(("NvTioNtUartRead: efd->inReadBuffer=%d, size=%d\n",
                        efd->inReadBuffer,size));

                // we got enough + one more char
                if (efd->inReadBuffer>size)
                    break;
            }
            DB(("While in read done\n"));
        }
        quit = 1;
    }
    NV_TIO_DBR(stream->f.fd,ptr,totalBytesRead);
    *bytes = totalBytesRead;
    DBSU(("NvTioNtUartRead: %d bytes returned\n", totalBytesRead));
    return NvSuccess;
}

//===========================================================================
// NvTioNtStreamClose() - close extended file descriptor
//===========================================================================
static void NvTioNtStreamClose(NvTioStreamHandle stream)
{
    NvTioExtFd* efd = NvTioPollGetDescriptor(stream->f.fd);
    HANDLE      savedFd = 0;
    if (efd)
    {
        // we can't call CloseHandle because Coverity will complain using
        // an already closed handle, but NvTioPollRemove will clear the
        // efd->fd to 0, so CloseHandle will close descriptor 0, which will
        // be wrong, so we save a copy of efd->fd for CloseHandle to use.
        savedFd = efd->fd;
        NvTioPollRemove(efd->fd);
        CloseHandle(savedFd);
        CloseHandle(efd->signalReadEvent);
        CloseHandle(efd->waitReadEvent);
    }
}

//===========================================================================
// NvTioNtStreamPollPrep() - prepare to poll
//===========================================================================
NvError NvTioNtStreamPollPrep(
              NvTioPollDesc   *tioDesc,
              void            *osDesc)
{
    NvTioFdEvent* ntDesc = osDesc;
    if (tioDesc->requestedEvents & NvTrnPollEvent_In)
    {
        NvTioExtFd* efd;
        ntDesc->fd = tioDesc->stream->f.fd;
        efd = NvTioPollGetDescriptor(ntDesc->fd);
        if (!efd)
            return DBERR(NvError_NotInitialized);
        ntDesc->events[0] = efd->waitReadEvent;
    }
    return NvSuccess;
}

//===========================================================================
// NvTioNtStreamPollCheck() - Check result of poll
//===========================================================================
NvError NvTioNtStreamPollCheck(
                               NvTioPollDesc   *tioDesc,
                               void            *osDesc)
{
    NvTioFdEvent* ntDesc = osDesc;
    if (ntDesc->fd != -1 && (ntDesc->revents & POLLIN)) {
        tioDesc->returnedEvents = NvTrnPollEvent_In;
    }
    NV_TIO_DBP(ntDesc->fd,tioDesc->returnedEvents);
    return NvSuccess;
}

//===========================================================================
// NvTioNtStreamWrite() - write winnt file descriptor
//===========================================================================
static NvError NvTioNtStreamWrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    const NvU8 *p   = ptr;
    NvTioExtFd *efd = NvTioPollGetDescriptor(stream->f.fd);
    NvU32       cnt;

    if (!efd)
        return DBERR(NvError_NotInitialized);

    NV_TIO_DBW((int)(efd->fd), ptr, size);
 
    switch (efd->type)
    {
        case NvTioFdType_Console:
            // Console does not support overlapped I/O
            if (!WriteFile(efd->fd, p, size, &cnt, NULL))
                return DBERR(NvError_FileWriteFailed);
            break;

        default:
            while(size)
            {
                int err = NvTioNtSynWriteFile(efd->fd, p, size, &cnt);
                if (err)
                    return DBERR(err);
                p    += cnt;
                size -= cnt;
            }
            break;
    }
    return NvSuccess;
}

//===========================================================================
// NvTioNtStreamRead() - read winnt file descriptor
//===========================================================================
static NvError NvTioNtStreamRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    int result;
    int cnt = 0;
    int timeout;

    NvTioExtFd* efd = NvTioPollGetDescriptor(stream->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    timeout = (timeout_msec==NV_WAIT_INFINITE) ? INFINITE : (int)timeout_msec;

    DB(("NvTioNtStreamRead: request for %d bytes, timeout=%dms, time now=%dms\n",
        size, timeout, GetTickCount()));

    if (efd->type == NvTioFdType_Console)
    {
        // Handle a special case - cooked console input
        NvOsMemcpy(ptr, consoleBuffer, consoleBufferPos);
        *bytes = consoleBufferPos;

        SetEvent(efd->signalReadEvent);
    }
    else
    {
        OVERLAPPED ovl;
        DWORD foo;

        if ((efd->type==NvTioFdType_Uart) && (efd->eventMask&EV_BREAK))
            return DBERR(NvError_EndOfFile);

        // check if the POOL_IN event was signaled
        result = WaitForSingleObject(efd->ovlr.hEvent, timeout);
        if (result == WAIT_TIMEOUT)
            return DBERR(NvError_Timeout);
        if (result == WAIT_FAILED)
            return DBERR(NvError_FileReadFailed);
        ResetEvent(efd->waitReadEvent);

        result = GetOverlappedResult(efd->fd, &efd->ovlr, &foo, FALSE);
        if (!result)
        {
            result = GetLastError();
            return DBERR(NvError_FileReadFailed);
        }

        memset(&ovl,0,sizeof(ovl));
        ovl.hEvent = CreateEvent(0,TRUE,0,0);
        if (!ovl.hEvent)
            return DBERR(NvError_InsufficientMemory);

        result = ReadFile(efd->fd, (char*)ptr, size, NULL, &ovl);
        if (!result)
        {
            if (GetLastError() != ERROR_IO_PENDING)
            {
                CancelIo(efd->fd);
                CloseHandle(ovl.hEvent);
                return DBERR(NvError_FileReadFailed);
            }

            // May need to block for the user specified timeout
            result = WaitForSingleObject(ovl.hEvent, timeout);
            if (result==WAIT_TIMEOUT)
            {
                CancelIo(efd->fd);
                CloseHandle(ovl.hEvent);
                return DBERR(NvError_Timeout);
            }
        }

        result = GetOverlappedResult(efd->fd, &ovl, &cnt, FALSE);
        if (!result)
        {
            if (GetLastError() != ERROR_IO_INCOMPLETE)
            {
                result = GetLastError();
                CloseHandle(ovl.hEvent);
                return DBERR(NvError_FileReadFailed);
            }
        }

        CloseHandle(ovl.hEvent);

        NV_TIO_DBR(stream->f.fd,ptr,cnt);

        if (efd->type==NvTioFdType_Uart)
        {
            // event can be waited again
            if (!WaitCommEvent(efd->fd, &efd->eventMask, &efd->ovlr))
            {
                result = GetLastError();
                if (result!=ERROR_IO_PENDING)
                    return DBERR(NvError_FileOperationFailed);
            }
        }
        else
        {
            if (cnt == 0)
                return DBERR(NvError_EndOfFile);
        }
        *bytes = cnt;
    }
    return NvSuccess;
}

//===========================================================================
// NvTioNtTcpCheckPath() - check filename to see if it is a tcp port
//===========================================================================
static NvError NvTioNtTcpCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a tcp port
    //
    if (!strncmp(path, "tcp:", 4))
        return NvSuccess;
    return DBERR(NvError_BadValue);
}

//===========================================================================
// NvTioNtTcpParseAddress() - fill in sockAddr
//===========================================================================
//
// any==1  - use for listening on all interfaces
// any==0  - use for connecting to a specific address
//
static NvError NvTioNtTcpParseAddress(
                    struct sockaddr_in *sockAddr,
                    const char *addr,
                    int any)
{
    NvError err;
    NvTioTcpAddrInfo info[1];

    err = NvTioParseIpAddr(addr, any, info);
    if (err)
        return err;

    NvOsMemset(sockAddr,0,sizeof(struct sockaddr_in));
    sockAddr->sin_family = AF_INET;
    sockAddr->sin_port = htons(info->portVal);
    sockAddr->sin_addr.S_un.S_un_b.s_b1 = info->addrVal[0];
    sockAddr->sin_addr.S_un.S_un_b.s_b2 = info->addrVal[1];
    sockAddr->sin_addr.S_un.S_un_b.s_b3 = info->addrVal[2];
    sockAddr->sin_addr.S_un.S_un_b.s_b4 = info->addrVal[3];

    if (any && info->defaultAddr) {
        sockAddr->sin_addr.s_addr = INADDR_ANY;
    }

    return NvSuccess;
}

//===========================================================================
// NvTioNtTcpConnect() - connect to a listening tcp port
//===========================================================================
static NvError NvTioNtTcpConnect(
                    const char *path,
                    NvU32 flags,
                    NvTioStreamHandle stream )
{
    struct sockaddr_in  sockAddr;
    NvError err;
    SOCKET fd;
    HANDLE readEvent;

    readEvent = CreateEvent(NULL,   // default security attributes 
                            TRUE,   // manual-reset event?
                            FALSE,  // signaled?
                            NULL);  // anonymous            

    if (!readEvent)
        return DBERR(NvError_InsufficientMemory);

    err = NvTioNtTcpParseAddress(&sockAddr, path, 0);
    if (err)
        return DBERR(err);

    fd = socket(PF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET)
    {
        CloseHandle(readEvent);
        return DBERR(NvError_InsufficientMemory);
    }

    if (connect(fd,(struct sockaddr*)&sockAddr, sizeof(sockAddr))==SOCKET_ERROR)
    {
        CloseHandle((HANDLE)fd);
        CloseHandle(readEvent);
        return DBERR(NvError_Timeout);
    }

    if (WSAEventSelect(fd, readEvent, FD_READ|FD_CLOSE))
    {
        //int err = WSAGetLastError();
        CloseHandle((HANDLE)fd);
        CloseHandle(readEvent);
        return DBERR(NvError_InvalidState);
    }

    stream->f.fd = 
        NvTioPollAdd((HANDLE)fd, NvTioFdType_Socket, readEvent, NULL);

    NV_TIO_DBO(stream->f.fd,"tcp-connect:");
    return NvSuccess;
}

//===========================================================================
// NvTioNtTcpAccept() - accept tcp connection 
//===========================================================================
static NvError NvTioNtTcpAccept(
                    NvTioStreamHandle stream_listen,
                    NvTioStreamHandle stream_connect,
                    NvU32 timeoutMS)
{
    int result;
    struct sockaddr sockAddr;
    socklen_t sockAddrLen = sizeof(sockAddr);
    SOCKET fd;
    HANDLE readEvent;
    NvTioExtFd* efd = NvTioPollGetDescriptor(stream_listen->f.fd);

    if (!efd)
        return DBERR(NvError_NotInitialized);

    result = WaitForSingleObject(efd->waitReadEvent, timeoutMS);
    if (result == WAIT_TIMEOUT)
        return DBERR(NvError_Timeout);
    if (result == WAIT_FAILED)
        return DBERR(NvError_FileOperationFailed);
    ResetEvent(efd->waitReadEvent);

    readEvent = CreateEvent(NULL,   // default security attributes 
                            TRUE,   // manual-reset event?
                            FALSE,  // signaled?
                            NULL);  // anonymous            

    if (!readEvent)
        return DBERR(NvError_InsufficientMemory);

    fd = accept((SOCKET)efd->fd, &sockAddr, &sockAddrLen);
    if (fd == INVALID_SOCKET)
    {
        //int err = WSAGetLastError();
        CloseHandle(readEvent);
        return DBERR(NvError_InvalidState);
    }

    if (WSAEventSelect(fd, readEvent, FD_READ|FD_CLOSE))
    {
        //int err = WSAGetLastError();
        CloseHandle((HANDLE)fd);
        CloseHandle(readEvent);
        return DBERR(NvError_InvalidState);
    }

    stream_connect->f.fd =
        NvTioPollAdd((HANDLE)fd, NvTioFdType_Socket, readEvent, NULL);
    if (stream_connect->f.fd < 0)
        return DBERR(NvError_InvalidState);
    NV_TIO_DBO(stream_connect->f.fd,"tcp-accept:");
    return NvSuccess;
}

//===========================================================================
// NvTioNtTcpListen() - listen for tcp connection
//===========================================================================
static NvError NvTioNtTcpListen(
                    const char *addr,
                    NvTioStreamHandle stream)
{
    struct sockaddr_in sockAddr;
    NvError err;
    SOCKET soc;
    HANDLE socketEvent;
    NvTioExtFd* efd;

    socketEvent = CreateEvent(NULL,   // default security attributes 
                              TRUE,   // manual-reset event?
                              FALSE,  // signaled?
                              NULL);  // anonymous

    if (!socketEvent)
        return DBERR(NvError_InsufficientMemory);

    DB(("Socket\n"));
    soc = socket(PF_INET, SOCK_STREAM, 0);
    if (soc == INVALID_SOCKET)
        return DBERR(NvError_InsufficientMemory);

    stream->f.fd =
        NvTioPollAdd((HANDLE)soc, NvTioFdType_Socket, socketEvent, NULL);

    if (stream->f.fd<0)
    {
        close(soc);
        return DBERR(NvError_FileOperationFailed);
    }

    // efd will be valid, descriptor has been recently added with success
    efd = NvTioPollGetDescriptor(stream->f.fd);

    DB(("NvTioNtTcpParseAddress\n"));
    err = NvTioNtTcpParseAddress(&sockAddr, addr, 1);
    if (err)
    {
        NvTioPollRemove(efd->fd);     
        close(soc);
        return DBERR(err);
    }

    efd->port = ntohs(sockAddr.sin_port);

    DB(("bind\n"));
    if (bind(soc, (struct sockaddr *)&sockAddr, sizeof(sockAddr))==SOCKET_ERROR)
    {
        NvTioPollRemove(efd->fd);     
        close(soc);
        return DBERR(NvError_AlreadyAllocated);
    }

    DB(("listen\n"));
    if (listen(soc, NV_TIO_TCP_LISTEN_BACKLOG)==SOCKET_ERROR)
    {
        NvTioPollRemove(efd->fd);     
        close(soc);
        return DBERR(NvError_InsufficientMemory);
    }

    if (WSAEventSelect(soc, socketEvent, FD_ACCEPT))
    {
        //int err = WSAGetLastError();
        NvTioPollRemove(efd->fd);     
        close(soc);
        return DBERR(NvError_FileOperationFailed);
    }

    return NvSuccess;
}

//===========================================================================
// NvTioRegisterStdio()
//===========================================================================
void NvTioRegisterStdio(void) 
{
    static NvTioStreamOps stdio_ops = {0};

    stdio_ops.sopName      = "winnt_stdio_ops",
    stdio_ops.sopFopen     = NvTioNtStdioOpen,
    stdio_ops.sopFwrite    = NvTioNtStreamWrite,
    stdio_ops.sopFread     = NvTioNtStreamRead,
    stdio_ops.sopPollPrep  = NvTioNtStreamPollPrep,
    stdio_ops.sopPollCheck = NvTioNtStreamPollCheck,

    NvTioRegisterOps(&stdio_ops);
}

//===========================================================================
// NvTioRegisterUart()
//===========================================================================
void NvTioRegisterUart(void) 
{
    static NvTioStreamOps uart_ops = {0};

    uart_ops.sopName        = "winnt_uart_ops",
    uart_ops.sopCheckPath   = NvTioNtUartCheckPath,
    uart_ops.sopFopen       = NvTioNtUartOpen,
    uart_ops.sopListen      = NvTioNtUartListen,
    uart_ops.sopAccept      = NvTioNtUartAccept,
    uart_ops.sopClose       = NvTioNtStreamClose,
    uart_ops.sopFwrite      = NvTioNtStreamWrite,
    uart_ops.sopFread       = NvTioNtUartRead,
    uart_ops.sopPollPrep    = NvTioNtStreamPollPrep,
    uart_ops.sopPollCheck   = NvTioNtStreamPollCheck,

    NvTioRegisterOps(&uart_ops);
}

//===========================================================================
// NvTioRegisterTcp()
//===========================================================================
void NvTioRegisterTcp(void) 
{
    static NvTioStreamOps tcp_ops = { 0 };

    if (!tcp_ops.sopName)
        NvTioTcpInit();

    tcp_ops.sopName        = "winnt_tcp_ops",
    tcp_ops.sopCheckPath   = NvTioNtTcpCheckPath,
    tcp_ops.sopFopen       = NvTioNtTcpConnect,
    tcp_ops.sopListen      = NvTioNtTcpListen,
    tcp_ops.sopAccept      = NvTioNtTcpAccept,
    tcp_ops.sopClose       = NvTioNtStreamClose,
    tcp_ops.sopFwrite      = NvTioNtStreamWrite,
    tcp_ops.sopFread       = NvTioNtStreamRead,
    tcp_ops.sopPollPrep    = NvTioNtStreamPollPrep,
    tcp_ops.sopPollCheck   = NvTioNtStreamPollCheck,

    NvTioRegisterOps(&tcp_ops);
}
