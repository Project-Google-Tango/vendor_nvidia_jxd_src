#ifndef INCLUDED_TIO_WINNT_H
#define INCLUDED_TIO_WINNT_H
/*
 * Copyright (c) 2005-2008 NVIDIA Corporation.  All rights reserved.
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
#include <winsock2.h>

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

#ifndef POLLIN
#define POLLIN      0x01
#define POLLPRI     0x02
#define POLLOUT     0x04

#define POLLERR     0x08
#define POLLHUP     0x10
#define POLLNVAL    0x20
#endif

#define NV_TIO_READ_BUFFER_THRESHOLD 512
#define NV_TIO_MAX_READ_BUFFER_LEN 4096
#define NV_TIO_OVERLAPPED_READ_BUF_SIZE 8192

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioFdType - type of extended file descriptor
//===========================================================================
typedef enum
{
    NvTioFdType_Invalid = 0,
    NvTioFdType_File,
    NvTioFdType_Console,      // console works only in a synchronous mode
    NvTioFdType_Socket,       // sockets need special event settings
    NvTioFdType_Uart,         // UART is a case on its own
    NvTioFdType_USB,          // USB allows simpler synchronous read/write

    NvTioFdType_Force32 = 0x7fffffff
} NvTioFdType;

typedef struct NvTioRBufRec 
{
    CRITICAL_SECTION criticalSection;
    NvU8  buffer[NV_TIO_MAX_READ_BUFFER_LEN];
    NvU32 dataStart;
    NvU32 volatile freeStart; // declared volatile to assure update NvTioNtUsbRead
                              // after the value was update by reading thread
} NvTioRBuf;

//===========================================================================
// NvTioExtFd - extended file descriptor
//===========================================================================
typedef struct NvTioExtFdRec
{
    HANDLE      fd;
    NvTioFdType type;
    HANDLE      waitReadEvent;
    HANDLE      signalReadEvent;// signaling for console descriptor
    HANDLE      readThread;     // thread for usb
    NvU32       isReadThreadExit;
    NvU32       eventMask;      // event mask for UART
    OVERLAPPED  ovlr;           // overlapped structure for overlapped wait
    NvTioRBuf   readBuf;
    NvU32       port;           // only for debugging
    NvU32       inReadBuffer;
    NvU8*       inReadBufferStart;
    NvU8        readBuffer[NV_TIO_OVERLAPPED_READ_BUF_SIZE];
} NvTioExtFd;

//===========================================================================
// NvTioFdEvent - file descriptor event
//===========================================================================
typedef struct NvTioFdEventRec
{
    int    fd;
    HANDLE events[1];
    short  revents;
} NvTioFdEvent;

//===========================================================================
// Functions for manipulating extended file descriptor
//===========================================================================

// Initialize descriptor table
void NvTioPollInit(void);

// Add a descriptor to the table
int NvTioPollAdd(
                 HANDLE fd,
                 NvTioFdType type,
                 HANDLE readEvent,
                 HANDLE signalEvent);

// Remove a descriptor from the table
NvError NvTioPollRemove(HANDLE fd);

// Retrieve descriptor structure
NvTioExtFd* NvTioPollGetDescriptor(int fd);

// Prepare to poll
NvError NvTioNtStreamPollPrep(NvTioPollDesc *tioDesc,
                              void *osDesc);

// Check result of poll
NvError NvTioNtStreamPollCheck(NvTioPollDesc *tioDesc,
                               void *osDesc);

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

//===========================================================================
// functions to manipulate extended file descriptor
//===========================================================================
extern NvTioExtFd* NvTioPollGetDescriptor(int fd);
                    
#endif // INCLUDED_TIO_WINNT_H
