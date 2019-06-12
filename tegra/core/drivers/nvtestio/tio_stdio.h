/*
 * Copyright (c) 2012-2013 NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TIO_STDIO_H
#define INCLUDED_TIO_STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

//###########################################################################
//############################### INCLUDES ##################################
//###########################################################################

#include "tio_local.h"


//###########################################################################
//############################### DEFINES ###################################
//###########################################################################
#define NV_TIO_STDIO_MAX_PKT NV_TIO_TARGET_CONNECTION_BUFFER_SIZE

// turn on verbose debug output of tio_stdio
#define NVTIO_STDIO_DEBUG 0

// target sends back whatever it receives
#define NVTIO_STDIO_SENDBACK 0

// host prints out each byte it received
// used to debug packet error
#define NVTIO_STDIO_HOST_VERBOSE 0

// Stdio stream type in the view of a target
// must match NvTioTargetStream defined in tio_local_host.h
typedef enum {
    NvTioStdioStream_stdin = 0,
    NvTioStdioStream_stdout,
    NvTioStdioStream_stderr,

    // cmd stream is not really used.
    // it is kept to be compatible with
    // the NvTioTargetStream of
    // tio_local_host.h
    NvTioStdioStream_cmd,

    NvTioStdioStream_reply,

    // all files share the same fd
    NvTioStdioStream_file,

    NvTioStdioStream_Count,

    NvTioStdioStream_Force32       =   0x7fffffff
} NvTioStdioStreamFd;

typedef enum NvTioStdioCmdTypeEnum
{
    NvTioStdioCmd_fread = 0,
    NvTioStdioCmd_fwrite,

    // reply a read request
    // sent by host to target
    NvTioStdioCmd_fread_reply,

    // open a file
    NvTioStdioCmd_fopen,

    // reply a fopen request
    NvTioStdioCmd_fopen_reply,

    // close a file
    NvTioStdioCmd_fclose,

    // acknowledges that a request succeeded
    NvTioStdioCmd_ack,

    // a request failed
    NvTioStdioCmd_nack,

    NvTioStdioCmd_exit,

    NvTioStdioCmd_Count,

    NvTioStdioCmd_Force32       =   0x7fffffff
} NvTioStdioCmdType;

typedef struct  NvTioStdioPktHeaderRec
{
    NvTioStdioStreamFd      fd;             // File descriptor
    NvTioStdioCmdType       cmd;            // Commond
    NvU32                   payload;        // Current payload
                                            // the size of the whole packet minus the size of the header
    NvU32                   checksum;       // checksum
} NvTioStdioPktHeader;

typedef struct NvTioStdioPktRec
{
    NvTioStdioPktHeader     *pPktHeader;
    char                    *pData;
} NvTioStdioPkt;

typedef struct NvTioStdioPktRec  *NvTioStdioPktHandle;

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

//===========================================================================
// NvTioStdioInitPkt() - Initialize a packet
//===========================================================================
void NvTioStdioInitPkt(NvTioStdioPktHandle pkt,  const void *buffer);

//===========================================================================
// NvTioStdioPktAddChar() - add character to a stio pkt
//===========================================================================
void NvTioStdioPktAddChar(NvTioStdioPktHandle pkt, char c);

//===========================================================================
// NvTioStdioPktAddData() - add a set of arbitrary data bytes to a pkt
//===========================================================================
void NvTioStdioPktAddData(NvTioStdioPktHandle pkt, const void *data, size_t cnt);

//===========================================================================
// NvTioStdioPktCheckSum() - Compute the checksum of a packet
//===========================================================================
NvU32 NvTioStdioPktChecksum(NvTioStdioPktHandle pkt);

//===========================================================================
// NvTioStdioGetChars() - get more characters in input buffer
//===========================================================================
NvError NvTioStdioGetChars(NvTioRemote *remote, NvU32 bytes,  NvU32 timeout_msec);

//===========================================================================
// NvTioStdioEraseCommand() - erase command from buffer
//===========================================================================
void NvTioStdioEraseCommand(NvTioRemote *remote);

//===========================================================================
// NvTioStdioPktSend() - send a stdio packet
//===========================================================================
NvError NvTioStdioPktSend(NvTioRemote *remote, NvTioStdioPktHandle pkt);

//===========================================================================
// NvTioStdioPktCheck() - Check for a complete command/reply pkt in the buffer
//===========================================================================
NvError NvTioStdioPktCheck(NvTioRemote *remote);

//===========================================================================
// NvTioStdioPktWait() - wait until a valid pkt is in the buffer
//===========================================================================
NvError NvTioStdioPktWait(
                NvTioRemote *remote,
                NvU32 timeout_msec);

#ifdef TIO_TARGET_SUPPORTED
void NvTioStdioTargetDebugf(const char *format, ... );
#endif

#ifdef __cplusplus
}
#endif

#endif // INCLUDED_TIO_STDIO_H
