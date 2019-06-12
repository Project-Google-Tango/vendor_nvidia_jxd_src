/*
 * Copyright 2007 - 2010 NVIDIA Corporation.  All Rights Reserved.
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
#include "nvos.h"
#include "nvassert.h"

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <errno.h>

#include "nvapputil.h"

// for stdio
#include <stdio.h>

// for serial port
#include <fcntl.h>
#include <termios.h>

// for TCP/IP
#include <sys/socket.h>
#include <arpa/inet.h>

//###########################################################################
//############################### CODE ######################################
//###########################################################################

NvError NvTioLinuxStreamPollPrep(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc);

NvError NvTioLinuxStreamPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc);

//===========================================================================
// NvTioLinuxStdioOpen() - open a uart port
//===========================================================================
static NvError NvTioLinuxStdioOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    if (!strcmp(path, "stdin:")) {
        NV_TIO_DBO(0,"stdin:");
        stream->f.fd = 0;
        return NvSuccess;
    }
    if (!strcmp(path, "stdout:")) {
        NV_TIO_DBO(1,"stdout:");
        stream->f.fd = 1;
        return NvSuccess;
    }
    if (!strcmp(path, "stderr:")) {
        NV_TIO_DBO(2,"stderr:");
        stream->f.fd = 2;
        return NvSuccess;
    }
    return DBERR(NvError_FileOperationFailed);
}

//===========================================================================
// NvTioLinuxUartCheckPath() - check filename to see if it is a uart port
//===========================================================================
static NvError NvTioLinuxUartCheckPath(const char *path)
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
// NvTioLinuxUartGetBaud() - get baudrate
//===========================================================================
static speed_t NvTioLinuxUartGetBaud(void)
{
    NvU32 rate = NvTioGlob.uartBaud ? NvTioGlob.uartBaud : NV_TIO_DEFAULT_BAUD;
    int i;

    /*
     * The for() loop ensures that if the default case is chosen on the first
     * pass, the default baud rate will be returned on the second pass.
     * Correct baud rates will return on the first pass.
     */
    for (i=0; i<2; i++) {
        switch(rate) {
            case 115200:    return B115200;
            case 57600:     return B57600;
            case 9600:      return B9600;
            case 4800:      return B4800;
            case 2400:      return B2400; // Added for QT
            default:
                rate = NV_TIO_DEFAULT_BAUD;
                break;
        }
    }
    NV_ASSERT(!"Could not determine proper baud rate");
    return B57600;
}

//===========================================================================
// NvTioLinuxUartOpen() - open a uart port
//===========================================================================
static NvError NvTioLinuxUartOpen(
            const char *path,
            NvU32 flags,
            NvTioStreamHandle stream )
{
    NvError err = NvSuccess;
    int rv;
    // TODO support other devices
    const char *dev = NvTioGlob.uartPort ? NvTioGlob.uartPort : "/dev/ttyS0";
    speed_t baud = NvTioLinuxUartGetBaud();

    struct termios newtio;

    stream->f.fd = open(dev, O_RDWR | O_NOCTTY);
    if (stream->f.fd < 0) {
        printf("\n\n\nFAILED TO OPEN SERIAL PORT %s\n\n\n\n",dev);
        return DBERR(NvError_FileOperationFailed);
    }

    NV_TIO_DBO(stream->f.fd,"uart:");

    // flush the port
    tcflush(stream->f.fd, TCIFLUSH);

    // set port params
    memset(&newtio, 0, sizeof(newtio));

    //tcgetattr(stream->f.fd, &newtio);
    
    newtio.c_cflag = 0
                    | CS8       // 8N1
                    | CLOCAL    // not a modem
                    | CREAD     // enable read
#if NV_TIO_TWO_STOP_BITS
                    | CSTOPB    // 2 stop bits
#endif
#if NV_TIO_UART_HW_FLOWCTRL
                    | CRTSCTS   // HW flow control
#endif
                    ;
    newtio.c_iflag = 0
                    | IGNPAR    // ignore parity
                    ;
    newtio.c_oflag = 0
                    ;
    newtio.c_lflag     = 0;     // non-canonical output
    newtio.c_cc[VEOF]  = 0;     // EOF
    newtio.c_cc[VMIN]  = 1;     // wait for at least 1 char
    newtio.c_cc[VTIME] = 1;     // wait 1/10 sec for more chars

#if 0
    printf("\nOpen Serial port %s  with baud = %s\n\n",
        dev,
        baud==B115200?"B115200":
        baud==B57600?"B57600":"????");
#endif
    rv = cfsetispeed(&newtio, B9600);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("cfsetispeed() returned %d\n",rv));
    }
    rv = cfsetospeed(&newtio, B9600);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("cfsetospeed() returned %d\n",rv));
    }
    rv = tcsetattr(stream->f.fd, TCSANOW, &newtio);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("tcsetattr() returned %d\n",rv));
    }
    // following 15 lines of code almost duplicate the 15 lines above.
    // this is a workaround for Keyspan USB/UART adaptor to work in
    // Karmic and Lucid. This is not needed if a native serial port is
    // used, like the one on the motherboard or PCI expansion card,
    // but keyspan has issues in these two Ubuntu releases, which
    // requires reset the baudrate each time when it's used.
    rv = cfsetispeed(&newtio, baud);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("cfsetispeed() returned %d\n",rv));
    }
    rv = cfsetospeed(&newtio, baud);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("cfsetospeed() returned %d\n",rv));
    }
    rv = tcsetattr(stream->f.fd, TCSANOW, &newtio);
    if (rv) {
        err = NvError_FileOperationFailed;
        DB(("tcsetattr() returned %d\n",rv));
    }

#if NV_TIO_UART_CONNECT_SEND_CR
    // send a carriage return
    rv = write(stream->f.fd, "\r", 1);
#if NV_TIO_CHAR_DELAY_US>0
        NvOsWaitUS(NV_TIO_CHAR_DELAY_US);
#endif
    if (rv < 0) {
        err = NvError_FileOperationFailed;
        DB(("write() returned %d\n",rv));
    }
    rv = write(stream->f.fd, "\n", 1);
#if NV_TIO_CHAR_DELAY_US>0
        NvOsWaitUS(NV_TIO_CHAR_DELAY_US);
#endif
    if (rv < 0) {
        err = NvError_FileOperationFailed;
        DB(("write() returned %d\n",rv));
    }
#endif

    return DBERR(err);
}

//===========================================================================
// NvTioLinuxUartAccept() - accept uart connection (accept if any chars rcvd)
//===========================================================================
static NvError NvTioLinuxUartAccept(
                    NvTioStreamHandle stream_listen,
                    NvTioStreamHandle stream_connect,
                    NvU32 timeoutMS)
{
#if NV_TIO_UART_ACCEPT_WAIT_CHAR
    //
    // wait, sending carriage return every 2 seconds, until some chars are
    // received.
    //
    while(1) {
        struct pollfd pfd[1];
        int rv;
        NvU32 wait = 2000;
        wait = wait < timeoutMS ? wait : timeoutMS;
        pfd[0].fd      = stream_listen->f.fd;
        pfd[0].revents = 0;
        pfd[0].events  = POLLIN;
        rv = poll(pfd, 1, wait);
        if (rv>0)
            break;
        if (rv<0)
            return NvError_FileOperationFailed;
        if (wait == timeoutMS)
            return NvError_Timeout;
        timeoutMS -= wait;
        write(stream_listen->f.fd, "\r\n", 2);
    }
#endif

    stream_connect->f.fd = stream_listen->f.fd;
    stream_connect->ops  = stream_listen->ops;
    stream_listen->f.fd = 0;
    stream_listen->ops  = NvTioGetNullOps();
    NV_TIO_DBO(stream_connect->f.fd,"uart-accept:");
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxUartListen() - listen for uart connection
//===========================================================================
static NvError NvTioLinuxUartListen(
                    const char *addr,
                    NvTioStreamHandle stream)
{
    NvError err;
    err = NvTioLinuxUartOpen(
                        addr, 
                        NVOS_OPEN_READ|NVOS_OPEN_WRITE, 
                        stream);
    return DBERR(err);
}

//===========================================================================
// NvTioLinuxStreamClose() - close linux file descriptor
//===========================================================================
static void NvTioLinuxStreamClose(NvTioStreamHandle stream)
{
    close(stream->f.fd);
}

//===========================================================================
// NvTioLinuxStreamPollPrep() - prepare to poll
//===========================================================================
NvError NvTioLinuxStreamPollPrep(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    struct pollfd *linuxDesc = osDesc;
    linuxDesc->fd = tioDesc->stream->f.fd;
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxStreamPollCheck() - Check result of poll
//===========================================================================
NvError NvTioLinuxStreamPollCheck(
                    NvTioPollDesc   *tioDesc,
                    void            *osDesc)
{
    struct pollfd *linuxDesc = osDesc;
    if (linuxDesc->fd != -1 && (linuxDesc->revents & POLLIN)) {
        tioDesc->returnedEvents = NvTrnPollEvent_In;
    }
    NV_TIO_DBP(linuxDesc->fd,tioDesc->returnedEvents);
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxStreamWrite() - write linux file descriptor
//===========================================================================
static NvError NvTioLinuxStreamWrite(
            NvTioStreamHandle stream,
            const void *ptr,
            size_t size )
{
    const NvU8 *p = ptr;

    NV_TIO_DBW(stream->f.fd,ptr,size);
    while(size)
    {
#if NV_TIO_CHAR_DELAY_US>0
        int cnt = write(stream->f.fd, p, 1);
        NvOsWaitUS(NV_TIO_CHAR_DELAY_US);
#else
        int cnt = write(stream->f.fd, p, size);
#endif
        if (cnt <= 0) {
            return DBERR(NvError_FileWriteFailed);
        }
        p    += cnt;
        size -= cnt;
    }
    
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxStreamRead() - read linux file descriptor
//===========================================================================
static NvError NvTioLinuxStreamRead(
            NvTioStreamHandle stream,
            void *ptr,
            size_t size,
            size_t *bytes,
            NvU32 timeout_msec)
{
    ssize_t cnt;

    if (timeout_msec != NV_WAIT_INFINITE) {
        int timeout = (int)timeout_msec;
        int n;
        struct pollfd desc;
    
        if (timeout < 0)
            timeout = 0x7fffffff;
        desc.fd = stream->f.fd;
        desc.revents = 0;
        desc.events = POLLIN;
        n = poll(&desc, 1, timeout);
        if (n <= 0)
            return n ? DBERR(NvError_FileReadFailed) : DBERR(NvError_Timeout);
    }

    // hopefully Coverity warning can be taken care of by following type cast
    cnt = (ssize_t)read(stream->f.fd, ptr, size);
    NV_TIO_DBR(stream->f.fd,ptr,cnt);
    if (cnt <= 0) {
        return cnt ? DBERR(NvError_FileReadFailed) : DBERR(NvError_EndOfFile);
    }

    *bytes = cnt;
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxTcpCheckPath() - check filename to see if it is a tcp port
//===========================================================================
static NvError NvTioLinuxTcpCheckPath(const char *path)
{
    //
    // return NvSuccess if path is a uart port
    //
    if (!strncmp(path, "tcp:", 4))
        return NvSuccess;
    return NvError_BadValue;
}

//===========================================================================
// NvTioLinuxTcpParseAddress() - fill in sockAddr
//===========================================================================
//
// any==1  - use for listening on all interfaces
// any==0  - use for connecting to a specific address
//
static NvError NvTioLinuxTcpParseAddress(
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

    // set the IP address
    if (any) {
        sockAddr->sin_addr.s_addr = INADDR_ANY;
    } else {
        if (!inet_aton(info->addrStr, &sockAddr->sin_addr))
            err = NvError_BadParameter;
    }

    // set the port
    ((NvU8*)&sockAddr->sin_port)[0] = (NvU8)((info->portVal >> 8) & 0xff);
    ((NvU8*)&sockAddr->sin_port)[1] = (NvU8)((info->portVal     ) & 0xff);

    // indicate that it is an IP address
    sockAddr->sin_family = AF_INET;

    return NvSuccess;
}

//===========================================================================
// NvTioLinuxTcpConnect() - connect to a listening tcp port
//===========================================================================
static NvError NvTioLinuxTcpConnect(
                    const char *path,
                    NvU32 flags,
                    NvTioStreamHandle stream )
{
    struct sockaddr_in  sockAddr;
    NvError err;

    err = NvTioLinuxTcpParseAddress(&sockAddr, path, 0);
    if (err)
        return DBERR(err);

    stream->f.fd = socket(PF_INET, SOCK_STREAM, 0);
    if (stream->f.fd < -1)
        return DBERR(NvError_InsufficientMemory);

    if (connect(stream->f.fd, (struct sockaddr*)&sockAddr, sizeof(sockAddr))<0)
    {
        close(stream->f.fd);
        return DBERR(NvError_Timeout);
    }
    NV_TIO_DBO(stream->f.fd,"tcp-connect:");
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxTcpAccept() - accept tcp connection 
//===========================================================================
static NvError NvTioLinuxTcpAccept(
                    NvTioStreamHandle stream_listen,
                    NvTioStreamHandle stream_connect,
                    NvU32 timeoutMS)
{
    struct sockaddr sockAddr;
    socklen_t sockAddrLen = sizeof(sockAddr);

    struct pollfd pfd[1];
    int wait = timeoutMS == NV_WAIT_INFINITE ? -1 :
               timeoutMS <= 0x7fffffff ? (int)timeoutMS : 0x7fffffff;
    int rv;

    pfd[0].fd      = stream_listen->f.fd;
    pfd[0].revents = 0;
    pfd[0].events  = POLLIN;
    rv = poll(pfd, 1, wait);
    if (rv<=0) {
        if (rv == 0)
            return NvError_Timeout;
        return DBERR(NvError_FileOperationFailed);
    }

    stream_connect->f.fd = accept(stream_listen->f.fd, &sockAddr, &sockAddrLen);
    if (stream_connect->f.fd < 0)
        return DBERR(NvError_InvalidState);

    NV_TIO_DBO(stream_connect->f.fd,"tcp-accept:");
    return NvSuccess;
}

//===========================================================================
// NvTioLinuxTcpListen() - listen for tcp connection
//===========================================================================
static NvError NvTioLinuxTcpListen(
                    const char *addr,
                    NvTioStreamHandle stream)
{
    struct sockaddr_in sockAddr;
    NvError err;

    stream->f.fd = socket(PF_INET, SOCK_STREAM, 0);
    if (stream->f.fd < -1)
        return DBERR(NvError_InsufficientMemory);

    err = NvTioLinuxTcpParseAddress(&sockAddr, addr, 1);
    if (err)
        return DBERR(err);


    if (bind(stream->f.fd, (struct sockaddr *)&sockAddr, sizeof(sockAddr))<0) {
        close(stream->f.fd);
        return DBERR(NvError_AlreadyAllocated);
    }

    if (listen(stream->f.fd, NV_TIO_TCP_LISTEN_BACKLOG) < 0) {
        close(stream->f.fd);
        return DBERR(NvError_InsufficientMemory);
    }

    return NvSuccess;
}

//===========================================================================
// NvTioRegisterStdio()
//===========================================================================
void NvTioRegisterStdio(void) 
{
    static NvTioStreamOps stdio_ops = {
        .sopName        = "linux_stdio_ops",
        .sopFopen       = NvTioLinuxStdioOpen,
        .sopFwrite      = NvTioLinuxStreamWrite,
        .sopFread       = NvTioLinuxStreamRead,
        .sopPollPrep    = NvTioLinuxStreamPollPrep,
        .sopPollCheck   = NvTioLinuxStreamPollCheck,
    };
    NvTioRegisterOps(&stdio_ops);
}

//===========================================================================
// NvTioRegisterUart()
//===========================================================================
void NvTioRegisterUart(void) 
{
    static NvTioStreamOps uart_ops = {
        .sopName        = "linux_uart_ops",
        .sopCheckPath   = NvTioLinuxUartCheckPath,
        .sopFopen       = NvTioLinuxUartOpen,
        .sopListen      = NvTioLinuxUartListen,
        .sopAccept      = NvTioLinuxUartAccept,
        .sopClose       = NvTioLinuxStreamClose,
        .sopFwrite      = NvTioLinuxStreamWrite,
        .sopFread       = NvTioLinuxStreamRead,
        .sopPollPrep    = NvTioLinuxStreamPollPrep,
        .sopPollCheck   = NvTioLinuxStreamPollCheck,
    };
    NvTioRegisterOps(&uart_ops);
}

//===========================================================================
// NvTioRegisterTcp()
//===========================================================================
void NvTioRegisterTcp(void) 
{
    static NvTioStreamOps tcp_ops = {
        .sopName        = "linux_tcp_ops",
        .sopCheckPath   = NvTioLinuxTcpCheckPath,
        .sopFopen       = NvTioLinuxTcpConnect,
        .sopListen      = NvTioLinuxTcpListen,
        .sopAccept      = NvTioLinuxTcpAccept,
        .sopClose       = NvTioLinuxStreamClose,
        .sopFwrite      = NvTioLinuxStreamWrite,
        .sopFread       = NvTioLinuxStreamRead,
        .sopPollPrep    = NvTioLinuxStreamPollPrep,
        .sopPollCheck   = NvTioLinuxStreamPollCheck,
    };
    NvTioRegisterOps(&tcp_ops);
}

