/*
 * Copyright (c) 2007 NVIDIA Corporation.  All Rights Reserved.
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

#include "tio_local_host.h"
#include "nvassert.h"
#include "nvos.h"

//###########################################################################
//############################### CODE ######################################
//###########################################################################

//===========================================================================
// NvTioListen() - listen for connections
//===========================================================================
NvError NvTioListen(const char *addr, NvTioStreamHandle *pListen)
{
    return NvTioStreamCreate(addr, 0, pListen, NV_TIO_LISTEN_MAGIC);
}

//===========================================================================
// NvTioAcceptTimeout() - accept connections
//===========================================================================
NvError NvTioAcceptTimeout(
                        NvTioStreamHandle listen,
                        NvTioStreamHandle *pStream,
                        NvU32 timeoutMS)
{
    NvError      err = NvSuccess;
    NvTioStream    *stream;

    NV_ASSERT(listen && listen->magic == NV_TIO_LISTEN_MAGIC);
    NV_ASSERT(listen->ops->sopAccept);
    NV_ASSERT(pStream);

    stream = NvTioOsAllocStream();
    if (!stream)
        return DBERR(NvError_InsufficientMemory);

    stream->ops = listen->ops;
    stream->magic = NV_TIO_STREAM_MAGIC;
    err = listen->ops->sopAccept(listen, stream, timeoutMS);
    if (err) {
        NvTioOsFreeStream(stream);
        return DBERR(err);
    }
    *pStream = stream;
    return NvSuccess;
}

//===========================================================================
// NvTioAccept() - accept connections
//===========================================================================
NvError NvTioAccept(NvTioStreamHandle listen, NvTioStreamHandle *pStream)
{
    return NvTioAcceptTimeout(listen, pStream, NV_WAIT_INFINITE);
}

