/*
 * Copyright 2007 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TESTIO_OPS_H
#define INCLUDED_TESTIO_OPS_H

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "nvtestio.h"

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioStreamOps - methods to call on a file or stream
//===========================================================================
//
// Any of these functions may be set to NULL if not supported.
//
typedef struct NvTioStreamOpsRec {

    // name of the filesystem (for debugging)
    const char *sopName;

    // check path and return NvSuccess if this is the right NvTioStreamOps
    // to open the file or stream.  Otherwise NvError_BadParameter.
    // Examples:
    //     "stdin:"     = standard input stream
    //     "stdout:"    = standard output stream
    //     "stderr:"    = standard error stream
    //     "command:"   = command stream
    //     "uart:<p>"   = serial port <p>
    //     "xio:<s>"    = XIO stream <s>
    //     "usb:<s>"    = USB stream <s>
    //     "tcp:<a>:<p> = TCP/IP IP address <a> port <p>
    //     "flash:<f>   = file <f> in flash filesystem
    //     "sd:<f>      = file <f> in sd filesystem
    //     "host:<f>    = file <f> on host
    //     "<f>"        = file <f> on host
    NvError (*sopCheckPath)(
                    const char *path);

    // open a file or stream.
    //    path - pathname of file or addr to connect to
    //    flags - one or more of 
    //                  NVOS_OPEN_READ
    //                  NVOS_OPEN_WRITE
    //                  NVOS_OPEN_CREATE
    //    stream - Allocated by caller.  sopFopen() fills this in. If sopFopen()
    //             returns an error then caller will free it.
    NvError (*sopFopen)( 
                    const char *path, 
                    NvU32 flags,
                    NvTioStreamHandle stream);

    // Listen for connections
    //    addr - address to listen to
    //    stream - Allocated by caller.  sopListen() fills this in.  If
    //             sopFopen() returns an error then caller will free it.
    NvError (*sopListen)( 
                    const char *path, 
                    NvTioStreamHandle stream);

    // Accept a connection.
    //    listener   - listening file handle
    //    connection - Allocated by caller.  sopAccept() fills this in.  If
    //             sopFopen() returns an error then caller will free it.
    //    timeoutMS  - timeout for accept
    NvError (*sopAccept)( 
                    NvTioStreamHandle listener,
                    NvTioStreamHandle connection,
                    NvU32             timeoutMS);

    // Close a file, stream, listener, or directory
    // sopClose() does NOT free the stream structure (caller frees it after 
    // sopClose() returns).
    void    (*sopClose)( 
                    NvTioStreamHandle stream);

    NvError (*sopFwrite)( 
                    NvTioStreamHandle stream, 
                    const void *ptr,
                    size_t size);
    NvError (*sopFread)( 
                    NvTioStreamHandle stream, 
                    void *ptr, 
                    size_t size,
                    size_t *bytes,
                    NvU32 timeout_msec);
    NvError (*sopFseek)( 
                    NvTioStreamHandle stream, 
                    NvS64 offset,
                    NvU32 whence);      // NvOsSeekEnum
    NvError (*sopFtell)( 
                    NvTioStreamHandle stream, 
                    NvU64 *position);
    NvError (*sopFflush)( 
                    NvTioStreamHandle stream);
    NvError (*sopIoctl)( 
                    NvTioStreamHandle stream,
                    NvU32 IoctlCode,
                    void *pBuffer,
                    NvU32 InBufferSize,
                    NvU32 InOutBufferSize,
                    NvU32 OutBufferSize);

    NvError (*sopOpendir)( 
                    const char *path, 
                    NvTioStreamHandle dir);
    NvError (*sopReaddir)( 
                    NvTioStreamHandle dir, 
                    char *name, 
                    size_t size);

    NvError (*sopVfprintf)(
                    NvTioStreamHandle stream,
                    const char *format,
                    va_list ap);

    //
    // if this is not implemented, poll will not work on the stream.
    // This should fill in the osDesc which is OS dependant.
    // On linux it is a pointer to a struct pollfd.
    //
    NvError (*sopPollPrep)(
                    NvTioPollDesc     *desc,
                    void              *osDesc);

    //
    // if this is not implemented, poll will not work on the stream.
    // This is called after poll.  It should update desc based on osDesc.
    //
    NvError (*sopPollCheck)(
                    NvTioPollDesc     *desc,
                    void              *osDesc);

    // internal use only
    struct NvTioStreamOpsRec   *sopNext;
} NvTioStreamOps;

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

/*-------------------------------------------------------------------------*/
/* NvTioRegisterOps() - register operations for a filesystem or transport.
 *
 * @param ops - operations for the filesystem or transport
 */
void NvTioRegisterOps(NvTioStreamOps *ops);

#endif // INCLUDED_TESTIO_OPS_H
