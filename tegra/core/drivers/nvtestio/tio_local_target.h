#ifndef INCLUDED_TIO_LOCAL_TARGET_H
#define INCLUDED_TIO_LOCAL_TARGET_H
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
//############################### TYPEDEFS ##################################
//###########################################################################

//===========================================================================
// NvTioMultiStream - a stream connected to the host
//===========================================================================
typedef struct NvTioMultiStreamRec {
    struct NvTioHostRec     *host;

    NvU32                     streamCnt;
    NvTioStreamHandle         streams[3];
} NvTioMultiStream;

//===========================================================================
// NvTioHostStream - enum of host streams (from target's point of view)
//===========================================================================
typedef enum {
    NvTioHostStream_stdin,
    NvTioHostStream_stdout,
    NvTioHostStream_stderr,

    NvTioHostStream_Count,
    NvTioHostStream_Force32     =   0x7fffffff
} NvTioHostStream;

//===========================================================================
// NvTioHost - host info (from target's point of view)
//===========================================================================
typedef struct NvTioHostRec {
    NvTioRemote         remote;     // MUST BE FIRST
    NvTioStreamHandle   ownTransport;
    NvTioTransport      transportToUse;
    NvTioMultiStream    stream    [NvTioHostStream_Count];
    NvTioStream         gdbtStream[NvTioHostStream_Count];
} NvTioHost;

//###########################################################################
//############################### PROTOTYPES ################################
//###########################################################################

#if NV_TIO_GDBT_SUPPORTED == 1
//===========================================================================
// GdbTarget protocol
//===========================================================================
NvError NvTioGdbtInitHost(NvTioHost *host);
#else
//===========================================================================
// stdio protocol
//===========================================================================
NvError NvTioStdioInitHost(NvTioHost *host);
#endif //NV_TIO_GDBT_SUPPORTED

NvError NvTioGetHost(NvTioHostHandle *pHost);

#endif // INCLUDED_TIO_LOCAL_TARGET_H
