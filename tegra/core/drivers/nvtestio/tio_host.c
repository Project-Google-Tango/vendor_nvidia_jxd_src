/*
 * Copyright (c) 2007-2012 NVIDIA Corporation.  All Rights Reserved.
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
// NvTioCalcTimeout()
//===========================================================================
NvU32 NvTioCalcTimeout(NvU32 timeout, NvU32 startTime, NvU32 max)
{
    NvU32 deltaTime;

    if (timeout == NV_WAIT_INFINITE)
        return max ? max : timeout;

    deltaTime = NvTioOsGetTimeMS() - startTime;
    if (deltaTime >= timeout)
        return 0;
    deltaTime = timeout - deltaTime;
    if (max && deltaTime > max)
        deltaTime = max;
    return deltaTime;
}

//===========================================================================
// NvTioDisconnectFromTarget() - connect to target machine
//===========================================================================
void NvTioDisconnectFromTarget(NvTioTargetHandle target)
{
    if (target) {
        NvOsFree(target->remote.buf);
        target->remote.buf = 0;
        NvTioRemoteDisconnect(&target->remote);
        NvOsFree(target);
    }
}

//===========================================================================
// NvTioConnectToTarget() - connect to target machine
//===========================================================================
NvError NvTioConnectToTarget(
                    NvTioStreamHandle connection, 
                    const char *protocol,
                    NvTioTargetHandle *pTarget)
{
    NvTioTarget *target;
    static char *buf;
    NvError err;

    NvTioGlob.isHost = 1;

    //
    // protocol may be used later to specify protocols other than GdbTarget
    //
    if (protocol)
        return DBERR(NvError_BadParameter);

    if (!connection || connection->magic != NV_TIO_STREAM_MAGIC)
        return DBERR(NvError_BadParameter);

    target = NvOsAlloc(sizeof(NvTioTarget));
    if (!target)
        return DBERR(NvError_InsufficientMemory);

    NvOsMemset(target, 0, sizeof(NvTioTarget));

    buf = NvOsAlloc(NV_TIO_HOST_CONNECTION_BUFFER_SIZE);
    if (!buf) {
        err = NvError_InsufficientMemory;
        goto fail;
    }
    err = NvTioRemoteConnect(
                    &target->remote, 
                    connection, 
                    buf, 
                    NV_TIO_HOST_CONNECTION_BUFFER_SIZE);
    if (err)
        goto fail;

#if NV_TIO_GDBT_SUPPORTED == 1
    err = NvTioGdbtInitTarget(target);
#else
    err = NvTioStdioInitTarget(target);
#endif //NV_TIO_GDBT_SUPPORTED

    if (err)
        goto fail;

    *pTarget  = target;
    return NvSuccess;

fail:
    NvTioDisconnectFromTarget(target);
    return DBERR(err);
}

//===========================================================================
// NvTioTargetFopen() - open a file or stream on the target
//===========================================================================
NvError NvTioTargetFopen(
            NvTioTargetHandle target,
            const char *path, 
            NvTioStreamHandle *pStream)
{
    int i;
    NvTioStream    *stream;

    NV_ASSERT(path);
    NV_ASSERT(pStream);
    NV_ASSERT(target);

    for (i=0; i<NvTioTargetStream_Count; i++) {
        NvTioStreamBuffer *buf = &target->buf[i];
        if (!NvOsStrcmp(buf->name, path)) {

            if (buf->stream)
                return NvError_AlreadyAllocated;

            stream = NvTioOsAllocStream();
            if (!stream)
                return NvError_InsufficientMemory;

            stream->magic = NV_TIO_STREAM_MAGIC;
            stream->ops   = buf->ops;
            stream->f.buf = buf;
            buf->stream   = stream;

            *pStream = stream;
            return NvSuccess;
        }
    }

    // path did not match any of the target->buf[].name strings
    return DBERR(NvError_BadParameter);
}

//===========================================================================
// NvTioBufAlloc() - request space in an output buffer
//===========================================================================
NvError NvTioBufAlloc(NvTioStreamSubBuf *sub, NvU32 cnt, NvU8 **start)
{
    NV_ASSERT(sub->buf && sub->bufLen);

    // This is needed because callers of this function are not properly
    // handling buffer overflows (i.e. not indicating to user when buffers are
    // being overflowed).
    NV_ASSERT(sub->bufCnt + cnt <= sub->bufLen);

    if (sub->bufCnt + cnt > sub->bufLen)
        return DBERR(NvError_InsufficientMemory);
    *start = sub->buf + sub->bufCnt;
    return NvSuccess;
}

//===========================================================================
// NvTioBufCommit() - commit characters in an input buffer
//===========================================================================
void NvTioBufCommit(NvTioStreamSubBuf *sub, NvU32 cnt, NvU8 *start)
{
    NV_ASSERT(sub->buf && sub->bufLen);
    NV_ASSERT(sub->buf + sub->bufCnt == start);
    NV_ASSERT(sub->bufCnt + cnt <= sub->bufLen);
    sub->bufCnt += cnt;
}

//===========================================================================
// NvTioBufShrink() - remove chars from head of buffer
//===========================================================================
void NvTioBufShrink(NvTioStreamSubBuf *sub, NvU32 cnt)
{
    NV_ASSERT(sub->buf && sub->bufLen);
    NV_ASSERT(cnt <= sub->bufCnt);
    sub->bufCnt -= cnt;
    if (cnt != sub->bufCnt) {
        NvU8 *d = sub->buf;
        NvU8 *s = d + cnt;
        int  c = sub->bufCnt;
        while(c--)
            *(d++) = *(s++);
    }
}

