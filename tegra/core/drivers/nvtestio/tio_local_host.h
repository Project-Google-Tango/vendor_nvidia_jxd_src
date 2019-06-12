#ifndef INCLUDED_TIO_LOCAL_HOST_H
#define INCLUDED_TIO_LOCAL_HOST_H
/*
 * Copyright (c) 2005-2012 NVIDIA Corporation.  All rights reserved.
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

//###########################################################################
//############################### DEFINES ###################################
//###########################################################################

// max number of streams in remote file I/O is 5. this is a magic number. it
// can be increased if it's necessary.
#define NV_TIO_REMOTE_FILE_STREAM_CNT   5

//###########################################################################
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioStreamBufDir - a buffer for a stream
//===========================================================================
typedef struct NvTioStreamSubBufRec {
    NvU8        *buf;
    size_t       bufLen;            // size of buffer buf (chars)
    size_t       bufCnt;            // # characters currently in buffer
} NvTioStreamSubBuf;

//===========================================================================
// NvTioStreamBuffer - a buffer for a stream
//===========================================================================
typedef struct NvTioStreamBufferRec {
    struct NvTioRemoteRec   *remote;
    NvTioStreamOps          *ops;
    NvTioStream             *stream;        // Null if not opened

    const char  *name;

    //
    // chars received from target
    //
    NvTioStreamSubBuf   recv;   // chars received from target
    NvTioStreamSubBuf   send;   // chars to be sent to target

} NvTioStreamBuffer;

//===========================================================================
// NvTioTargetStream - enum of target streams (from host's point of view)
//===========================================================================
typedef enum {
    NvTioTargetStream_stdin,
    NvTioTargetStream_stdout,
    NvTioTargetStream_stderr,

    NvTioTargetStream_cmd,
    NvTioTargetStream_reply,

    NvTioTargetStream_Count = NvTioTargetStream_reply + NV_TIO_REMOTE_FILE_STREAM_CNT,
    NvTioTargetStream_Force32       =   0x7fffffff
} NvTioTargetStream;

//===========================================================================
// NvTioTarget - target info (from host's point of view)
//===========================================================================
typedef struct NvTioTargetRec {
    NvTioRemote         remote;     // MUST BE FIRST

    NvTioStreamBuffer   buf[NvTioTargetStream_Count];

    // If the target is currently waiting to read a file descriptor, then
    // readWait will be non-NULL, and the following info describes the
    // read.
    NvTioStreamBuffer   *readStream;    // stream that is being read.
    NvU32                readAddr;      // target address for read
    NvU32                readLen;       // len requested for read
} NvTioTarget;

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

#if NV_TIO_GDBT_SUPPORTED == 1
//===========================================================================
// GdbTarget protocol
//===========================================================================
NvError NvTioGdbtInitTarget(NvTioTarget *target);
#else
//===========================================================================
// Stdio protocol
//===========================================================================
NvError NvTioStdioInitTarget(NvTioTarget *target);
#endif //NV_TIO_GDBT_SUPPORTED

NvError NvTioBufAlloc(NvTioStreamSubBuf *sub, NvU32 cnt, NvU8 **start);
void    NvTioBufCommit(NvTioStreamSubBuf *sub, NvU32 cnt, NvU8 *start);
void    NvTioBufShrink(NvTioStreamSubBuf *sub, NvU32 cnt);


#endif // INCLUDED_TIO_LOCAL_HOST_H
